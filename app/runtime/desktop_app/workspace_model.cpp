#include "workspace_model.h"

#include <QtCore>
#include <QtWidgets>

#include "app_state.h"
#include "object_names.h"
#include "widget_helpers.h"

#include <QDesktopServices>
#include <QUrl>

#include <memory>
#include <utility>

namespace desktop_app::workspace {
namespace {

constexpr const char* kRuntimeTargetClassIdKey = "runtime/v1/model/targetClassId";

QString findProjectRootFromApp() {
    QStringList starts;
    starts << QCoreApplication::applicationDirPath() << QDir::currentPath();
    for (const auto& start : starts) {
        QDir dir(start);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists() &&
                (QFileInfo(dir.filePath("app/runtime/models")).isDir() ||
                 QFileInfo(dir.filePath("internal-release/app/runtime/models")).isDir())) {
                return dir.absolutePath();
            }
            if (!dir.cdUp())
                break;
        }
    }
    return QString();
}

QString registryString(const QJsonObject& entry, const QString& key) {
    return entry.value(key).toString();
}

QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key) {
    return entry.value(objectKey).toObject().value(key).toString();
}

QString runtimePathFromRegistryPath(const QString& path) {
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return trimmed;
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        QString absolute = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(absolute)) {
            return QDir(QCoreApplication::applicationDirPath()).relativeFilePath(absolute);
        }
    }
    return trimmed;
}

QString absoluteRegistryPath(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return trimmed;
    const QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString absolute = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(absolute))
            return absolute;
        const QString internalAbsolute = QDir(projectRoot).absoluteFilePath("internal-release/" + trimmed);
        if (QFileInfo::exists(internalAbsolute))
            return internalAbsolute;
    }
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(runtimePathFromRegistryPath(trimmed));
}

QString fileSizeSummary(const QString& path) {
    QFileInfo info(absoluteRegistryPath(path));
    if (!info.isFile())
        return QString();
    const qint64 bytes = info.size();
    if (bytes >= 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    if (bytes >= 1024) {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    return QString::number(bytes) + " B";
}

QLabel* makeModelFieldLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setProperty("mutedText", true);
    return label;
}

QLabel* makeModelValue(const QString& objectName, bool selectable = false) {
    auto* label = new QLabel("--");
    nameWidget(label, objectName.toUtf8().constData());
    label->setTextInteractionFlags(selectable ? (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)
                                              : Qt::NoTextInteraction);
    if (selectable)
        label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}

QString wrapTechnicalText(QString text) {
    if (text.isEmpty())
        return QString();
    QString escaped = text.toHtmlEscaped();
    escaped.replace("/", "/<br>");
    escaped.replace("\\", "\\<br>");
    escaped.replace("_", "_<br>");
    escaped.replace("-", "-<wbr>");
    QString wrapped;
    int runLength = 0;
    for (int i = 0; i < escaped.size(); ++i) {
        wrapped += escaped.at(i);
        const bool tag = escaped.at(i) == '<';
        if (tag) {
            const int tagEnd = escaped.indexOf('>', i);
            if (tagEnd >= i) {
                wrapped += escaped.mid(i + 1, tagEnd - i);
                i = tagEnd;
                runLength = 0;
                continue;
            }
        }
        if (escaped.at(i).isLetterOrNumber()) {
            ++runLength;
            if (runLength >= 24) {
                wrapped += "<br>";
                runLength = 0;
            }
        } else {
            runLength = 0;
        }
    }
    return wrapped;
}

void addField(QGridLayout* grid, int row, int column, const QString& label, QLabel* value, int columnSpan = 1) {
    grid->addWidget(makeModelFieldLabel(label), row, column * 2);
    grid->addWidget(value, row, column * 2 + 1, 1, columnSpan * 2 - 1);
}

QStringList jsonStringList(const QJsonArray& values) {
    QStringList result;
    for (const auto& value : values)
        result << value.toVariant().toString();
    return result;
}

QString jsonCompact(const QJsonValue& value) {
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return value.toVariant().toString();
}

QString jsonPathSummary(const QJsonValue& value) {
    if (value.isUndefined() || value.isNull())
        return "(none)";
    if (value.isObject()) {
        QStringList lines;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isArray()) {
                const QStringList values = jsonStringList(it.value().toArray());
                lines << QString("%1: %2").arg(it.key(), values.isEmpty() ? QString("(none)") : values.join("; "));
            } else {
                lines << QString("%1: %2").arg(it.key(), jsonCompact(it.value()));
            }
        }
        return lines.isEmpty() ? QString("(none)") : lines.join("\n");
    }
    return jsonCompact(value);
}

QString classesSummary(const QJsonObject& entry) {
    QStringList classLines;
    const QJsonObject displayLabels = entry.value("display_labels").toObject();
    for (const auto& value : entry.value("classes").toArray()) {
        const QString classId = value.toString();
        classLines << QString("%1 (%2)").arg(displayLabels.value(classId).toString(classId), classId);
    }
    return classLines.join(", ");
}

QJsonObject loadMetadataDoc(const QJsonObject& entry) {
    const QString registryPath = registryString(entry, "metadata_path");
    QString absolutePath = registryPath;
    if (!registryPath.isEmpty() && !QFileInfo(registryPath).isAbsolute()) {
        const QString projectRootForMetadata = findProjectRootFromApp();
        QStringList candidates;
        if (!projectRootForMetadata.isEmpty()) {
            candidates << QDir(projectRootForMetadata).absoluteFilePath(registryPath);
            candidates << QDir(projectRootForMetadata).absoluteFilePath("internal-release/" + registryPath);
        }
        candidates
            << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(runtimePathFromRegistryPath(registryPath));
        QDir probe(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 8; ++i) {
            candidates << probe.absoluteFilePath(registryPath);
            candidates << probe.absoluteFilePath("internal-release/" + registryPath);
            if (!probe.cdUp())
                break;
        }
        for (const auto& candidate : candidates) {
            if (QFileInfo(candidate).isFile()) {
                absolutePath = candidate;
                break;
            }
        }
    }
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

QString inputSizeSummary(const QJsonObject& metadataDoc) {
    const QString value = jsonStringList(metadataDoc.value("input_size").toArray()).join(" x ");
    return value.isEmpty() ? "--" : value;
}

QString metadataArchitectureSummary(const QJsonObject& metadataDoc) {
    const QJsonObject architecture = metadataDoc.value("architecture").toObject();
    QStringList parts;
    if (!architecture.value("family").toString().isEmpty())
        parts << architecture.value("family").toString();
    if (!architecture.value("variant").toString().isEmpty())
        parts << architecture.value("variant").toString();
    const QString format = metadataDoc.value("export").toObject().value("format").toString();
    if (!format.isEmpty())
        parts << format.toUpper();
    return parts.isEmpty() ? QString("ONNX classifier") : parts.join(" / ");
}

bool entryIsActive(const QJsonObject& entry) {
    return entry.value("selectable_for_normal_live_sorting").toBool(false) ||
           registryString(entry, "state").contains("promoted", Qt::CaseInsensitive) ||
           registryString(entry, "promotion_status").contains("current", Qt::CaseInsensitive);
}

bool entryIsValidated(const QJsonObject& entry) {
    const QString status = registryString(entry, "validation_status").trimmed();
    if (status.isEmpty())
        return false;
    return !status.contains("no validation", Qt::CaseInsensitive) &&
           !status.contains("not validated", Qt::CaseInsensitive) && !status.contains("missing", Qt::CaseInsensitive);
}

QString displayNameForEntry(const QJsonObject& entry) {
    const QString displayName = registryString(entry, "display_name");
    return displayName.isEmpty() ? registryString(entry, "registry_entry_id") : displayName;
}

QString modelRegistryDirectory(const QString& registryFilePath) {
    if (!registryFilePath.trimmed().isEmpty()) {
        const QFileInfo info(registryFilePath);
        if (info.absoluteDir().exists())
            return info.absolutePath();
    }
    const QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString internalModels = QDir(projectRoot).absoluteFilePath("internal-release/app/runtime/models");
        if (QFileInfo(internalModels).isDir())
            return internalModels;
        const QString runtimeModels = QDir(projectRoot).absoluteFilePath("app/runtime/models");
        if (QFileInfo(runtimeModels).isDir())
            return runtimeModels;
    }
    return QCoreApplication::applicationDirPath();
}

QJsonArray loadRegistryEntriesFromPath(const QString& registryFilePath, QString* warning) {
    if (warning)
        warning->clear();
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warning)
            *warning = "Registry file not readable: " + registryFilePath;
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (warning)
            *warning = "Registry parse failed: " + parseError.errorString();
        return {};
    }
    return doc.object().value("entries").toArray();
}

bool saveRegistryEntriesToPath(const QString& registryFilePath, const QJsonArray& entries, QString* error) {
    if (error)
        error->clear();
    if (registryFilePath.trimmed().isEmpty()) {
        if (error)
            *error = "No registry file path is available.";
        return false;
    }
    QJsonObject registry;
    QFile existing(registryFilePath);
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll());
        if (doc.isObject())
            registry = doc.object();
    }
    if (registry.value("schema_version").toString().isEmpty())
        registry["schema_version"] = "model-registry-v1";
    registry["entries"] = entries;
    QFile file(registryFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error)
            *error = "Registry file not writable: " + registryFilePath;
        return false;
    }
    file.write(QJsonDocument(registry).toJson(QJsonDocument::Indented));
    return true;
}

QString firstExistingEvidencePath(const QJsonObject& entry) {
    const QJsonValue evidenceValue = entry.value("validation_evidence");
    QStringList candidates;
    if (evidenceValue.isObject()) {
        const QJsonObject evidence = evidenceValue.toObject();
        for (auto it = evidence.constBegin(); it != evidence.constEnd(); ++it) {
            if (it.value().isString())
                candidates << it.value().toString();
            if (it.value().isArray()) {
                for (const auto& value : it.value().toArray()) {
                    if (value.isString())
                        candidates << value.toString();
                }
            }
        }
    } else if (evidenceValue.isString()) {
        candidates << evidenceValue.toString();
    }
    const QString promotion = registryString(entry, "promotion_record_path");
    if (!promotion.isEmpty())
        candidates << promotion;
    for (const auto& candidate : candidates) {
        const QString absolute = absoluteRegistryPath(candidate);
        if (QFileInfo(absolute).exists())
            return absolute;
    }
    return {};
}

void openPathOrWarn(QWidget* parent, const QString& path, const QString& label) {
    const QString absolute = absoluteRegistryPath(path);
    if (!absolute.isEmpty() && QFileInfo(absolute).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
        return;
    }
    QMessageBox::information(parent, label, label + " is not available for the selected model.");
}

QPushButton* makeSmallButton(const QString& text, const char* objectName) {
    auto* button = new QPushButton(text);
    nameWidget(button, objectName);
    button->setMinimumHeight(26);
    return button;
}

QString canonicalTargetClassId(QString value) {
    value = value.trimmed();
    if (value.compare("Empty", Qt::CaseInsensitive) == 0)
        return "0";
    if (value.compare("Single", Qt::CaseInsensitive) == 0)
        return "1";
    if (value.compare("MoreThanTwo", Qt::CaseInsensitive) == 0 ||
        value.compare("More than two", Qt::CaseInsensitive) == 0) {
        return "2";
    }
    return value.isEmpty() ? QString("1") : value;
}

} // namespace

QWidget* buildModelWorkspace(const ModelWorkspaceControls& controls) {
    using desktop_app::ui::makeMetric;
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    auto registryEntries = std::make_shared<QJsonArray>(controls.registryEntries);
    auto modelWorkspacePage = new QWidget;
    nameWidget(modelWorkspacePage, "ModelWorkspace");
    auto modelWorkspaceLayout = new QHBoxLayout;
    modelWorkspaceLayout->setContentsMargins(10, 10, 10, 10);
    modelWorkspaceLayout->setSpacing(12);

    auto modelWorkspaceRegistryPanel = makePanel("Models");
    modelWorkspaceRegistryPanel->setObjectName("ModelRegistryPanel");
    modelWorkspaceRegistryPanel->setMinimumWidth(300);
    modelWorkspaceRegistryPanel->setMaximumWidth(360);
    auto modelRegistryBody = makePanelBody(modelWorkspaceRegistryPanel, 0, 0, 0, 0);
    auto* registryHeaderActions = new QWidget;
    auto* registryHeaderLayout = new QHBoxLayout;
    registryHeaderLayout->setContentsMargins(12, 10, 12, 8);
    registryHeaderLayout->setSpacing(8);
    auto* addModelButton = makeSmallButton("Add Model", "ModelWorkspaceAddModelButton");
    auto* refreshModelsButton = makeSmallButton("Refresh", "ModelWorkspaceRefreshButton");
    registryHeaderLayout->addWidget(addModelButton, 1);
    registryHeaderLayout->addWidget(refreshModelsButton, 0);
    registryHeaderActions->setLayout(registryHeaderLayout);
    modelRegistryBody->addWidget(registryHeaderActions);

    auto modelRegistryList = new QTableWidget(registryEntries->size(), 1);
    nameWidget(modelRegistryList, "ModelWorkspaceRegistryTable");
    modelRegistryList->setHorizontalHeaderLabels({"Model"});
    modelRegistryList->horizontalHeader()->setVisible(false);
    modelRegistryList->horizontalHeader()->setStretchLastSection(true);
    modelRegistryList->verticalHeader()->setVisible(false);
    modelRegistryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modelRegistryList->setSelectionBehavior(QAbstractItemView::SelectRows);
    modelRegistryList->setSelectionMode(QAbstractItemView::SingleSelection);
    modelRegistryList->setAlternatingRowColors(false);
    modelRegistryList->setShowGrid(false);
    modelRegistryList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto populateRegistryList = [=]() {
        modelRegistryList->setRowCount(registryEntries->size());
        for (int i = 0; i < registryEntries->size(); ++i) {
            const QJsonObject entry = registryEntries->at(i).toObject();
            const QJsonObject metadataDoc = loadMetadataDoc(entry);
            QStringList summary;
            const QString input = inputSizeSummary(metadataDoc);
            if (input != "--")
                summary << input;
            const QString size = fileSizeSummary(registryString(entry, "model_path"));
            if (!size.isEmpty())
                summary << size;
            const QString liveMode = registryString(entry, "live_use_mode");
            if (!liveMode.isEmpty())
                summary << liveMode;
            const QString metadataStatus = registryString(entry, "metadata_status");
            if (!metadataStatus.isEmpty())
                summary << metadataStatus;
            const QString active = entryIsActive(entry) ? "  [Active]" : QString();
            auto* item = new QTableWidgetItem(displayNameForEntry(entry) + active + "\n" + summary.join("  -  "));
            item->setToolTip(displayNameForEntry(entry));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            if (!entryIsActive(entry)) {
                item->setForeground(QBrush(QColor(Qt::gray)));
            }
            modelRegistryList->setItem(i, 0, item);
            modelRegistryList->setRowHeight(i, 72);
        }
    };
    populateRegistryList();
    modelRegistryBody->addWidget(modelRegistryList, 1);
    auto modelRegistryActions = new QHBoxLayout;
    modelRegistryActions->setContentsMargins(12, 0, 12, 12);
    auto modelValidateBtn = new QPushButton("Validate");
    nameWidget(modelValidateBtn, "ModelWorkspaceValidateButton");
    modelRegistryActions->addWidget(modelValidateBtn);
    modelRegistryBody->addLayout(modelRegistryActions);

    if (controls.imageValidationAction) {
        QObject::connect(modelValidateBtn, &QPushButton::clicked, controls.imageValidationAction, &QAction::trigger);
    }

    auto modelDetailScroll = new QScrollArea;
    nameWidget(modelDetailScroll, "ModelWorkspaceDetailScrollArea");
    modelDetailScroll->setWidgetResizable(true);
    modelDetailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto modelDetailStack = new QWidget;
    nameWidget(modelDetailStack, "ModelWorkspaceDetailStack");
    auto modelDetailLayout = new QVBoxLayout;
    modelDetailLayout->setContentsMargins(0, 0, 2, 0);
    modelDetailLayout->setSpacing(12);

    auto modelOverviewPanel = makePanel("Model detail", "ONNX classifier");
    modelOverviewPanel->setObjectName("ModelOverviewPanel");
    auto modelOverviewBody = makePanelBody(modelOverviewPanel);
    auto modelOverviewGrid = new QGridLayout;
    modelOverviewGrid->setContentsMargins(0, 0, 0, 0);
    modelOverviewGrid->setHorizontalSpacing(10);
    modelOverviewGrid->setVerticalSpacing(8);
    auto modelNameValue = makeModelValue("ModelWorkspaceNameValue");
    auto registryIdValue = makeModelValue("ModelWorkspaceRegistryIdValue", true);
    auto architectureValue = makeModelValue("ModelWorkspaceArchitectureValue");
    auto modelStateValue = makeModelValue("ModelWorkspaceStateValue");
    auto modelLiveModeValue = makeModelValue("ModelWorkspaceLiveModeValue");
    auto modelFileSizeValue = makeModelValue("ModelWorkspaceFileSizeValue");
    auto modelPathValue = makeModelValue("ModelWorkspacePathValue", true);
    auto modelShaValue = makeModelValue("ModelWorkspaceShaValue", true);
    auto metadataPathValue = makeModelValue("ModelWorkspaceMetadataPathValue", true);
    auto metadataShaValue = makeModelValue("ModelWorkspaceMetadataShaValue", true);
    auto modelClassesValue = makeModelValue("ModelWorkspaceClassesValue");
    addField(modelOverviewGrid, 0, 0, "Model", modelNameValue);
    addField(modelOverviewGrid, 1, 0, "Classifier", architectureValue);
    addField(modelOverviewGrid, 2, 0, "State", modelStateValue);
    addField(modelOverviewGrid, 3, 0, "Live use", modelLiveModeValue);
    addField(modelOverviewGrid, 4, 0, "File size", modelFileSizeValue);
    addField(modelOverviewGrid, 5, 0, "Registry ID", registryIdValue);
    addField(modelOverviewGrid, 6, 0, "Output classes", modelClassesValue);
    addField(modelOverviewGrid, 7, 0, "Path", modelPathValue);
    addField(modelOverviewGrid, 8, 0, "ONNX SHA-256", modelShaValue);
    addField(modelOverviewGrid, 9, 0, "Metadata", metadataPathValue);
    addField(modelOverviewGrid, 10, 0, "Metadata SHA-256", metadataShaValue);
    modelOverviewGrid->setColumnStretch(1, 1);
    modelOverviewGrid->setColumnStretch(3, 1);
    modelOverviewBody->addLayout(modelOverviewGrid);
    auto* overviewActions = new QHBoxLayout;
    overviewActions->setContentsMargins(0, 0, 0, 0);
    overviewActions->setSpacing(8);
    auto* setActiveButton = makeSmallButton("Set Active", "ModelWorkspaceSetActiveButton");
    auto* modelActionsButton = makeSmallButton("...", "ModelWorkspaceActionsButton");
    modelActionsButton->setToolTip("Model actions");
    overviewActions->addWidget(setActiveButton, 0);
    overviewActions->addWidget(modelActionsButton, 0);
    overviewActions->addStretch(1);
    modelOverviewBody->addLayout(overviewActions);

    auto modelMetadataPanel = makePanel("Metadata - normalization");
    modelMetadataPanel->setObjectName("ModelMetadataNormalizationPanel");
    auto modelMetadataBody = makePanelBody(modelMetadataPanel);
    auto modelMetadataGrid = new QGridLayout;
    modelMetadataGrid->setContentsMargins(0, 0, 0, 0);
    modelMetadataGrid->setHorizontalSpacing(10);
    modelMetadataGrid->setVerticalSpacing(8);
    auto metadataSchemaValue = makeModelValue("ModelWorkspaceMetadataSchemaValue");
    auto labelSchemaValue = makeModelValue("ModelWorkspaceLabelSchemaValue");
    auto inputSizeValue = makeModelValue("ModelWorkspaceInputSizeValue");
    auto normalizationMeanValue = makeModelValue("ModelWorkspaceNormalizationMeanValue");
    auto normalizationStdValue = makeModelValue("ModelWorkspaceNormalizationStdValue");
    addField(modelMetadataGrid, 0, 0, "Metadata status", metadataSchemaValue);
    addField(modelMetadataGrid, 1, 0, "Label schema", labelSchemaValue);
    addField(modelMetadataGrid, 2, 0, "Input size", inputSizeValue);
    addField(modelMetadataGrid, 3, 0, "Mean", normalizationMeanValue);
    addField(modelMetadataGrid, 4, 0, "Std", normalizationStdValue);
    modelMetadataGrid->setColumnStretch(1, 1);
    modelMetadataGrid->setColumnStretch(3, 1);
    modelMetadataBody->addLayout(modelMetadataGrid);
    auto* metadataActions = new QHBoxLayout;
    metadataActions->setContentsMargins(0, 0, 0, 0);
    metadataActions->setSpacing(8);
    auto* openMetadataButton = makeSmallButton("Open Metadata", "ModelWorkspaceOpenMetadataButton");
    auto* reloadMetadataButton = makeSmallButton("Reload Metadata", "ModelWorkspaceReloadMetadataButton");
    metadataActions->addWidget(openMetadataButton, 0);
    metadataActions->addWidget(reloadMetadataButton, 0);
    metadataActions->addStretch(1);
    modelMetadataBody->addLayout(metadataActions);

    auto modelTargetPanel = makePanel("Target class", "Drives DAQ trigger policy when prediction matches");
    modelTargetPanel->setObjectName("ModelTargetClassPanel");
    auto modelTargetBody = makePanelBody(modelTargetPanel);
    auto targetPolicyValue = makeModelValue("ModelWorkspaceTargetPolicyValue");
    auto targetRuntimeValue = makeModelValue("ModelWorkspaceRuntimeTargetValue");
    targetRuntimeValue->setProperty("statusPill", true);
    auto* targetSegmentRow = new QHBoxLayout;
    targetSegmentRow->setContentsMargins(0, 0, 0, 0);
    targetSegmentRow->setSpacing(6);
    auto* targetButtonGroup = new QButtonGroup(modelWorkspacePage);
    targetButtonGroup->setExclusive(true);
    const QVector<std::pair<QString, QString>> targetClasses = {
        {"0", "Empty"},
        {"1", "Single"},
        {"2", "MoreThanTwo"},
    };
    for (const auto& targetClass : targetClasses) {
        auto* button = new QPushButton(targetClass.second);
        button->setCheckable(true);
        button->setProperty("segmentedButton", true);
        nameWidget(button, QString("ModelWorkspaceTargetClass%1Button").arg(targetClass.first).toUtf8().constData());
        targetButtonGroup->addButton(button, targetClass.first.toInt());
        targetSegmentRow->addWidget(button);
    }
    targetSegmentRow->addStretch(1);
    modelTargetBody->addWidget(targetPolicyValue);
    modelTargetBody->addLayout(targetSegmentRow);
    modelTargetBody->addWidget(targetRuntimeValue);

    auto modelValidationPanel = makePanel("Validation result");
    modelValidationPanel->setObjectName("ModelValidationPanel");
    auto modelValidationBody = makePanelBody(modelValidationPanel);
    auto validationStatusValue = makeModelValue("ModelWorkspaceValidationStatusValue");
    modelValidationBody->addWidget(validationStatusValue);
    auto validationMetricsGrid = new QGridLayout;
    validationMetricsGrid->setContentsMargins(0, 0, 0, 0);
    validationMetricsGrid->setHorizontalSpacing(1);
    validationMetricsGrid->setVerticalSpacing(1);
    auto accuracyValue = new QLabel("--");
    auto macroF1Value = new QLabel("--");
    auto sequenceValue = new QLabel("--");
    auto lossValue = new QLabel("--");
    validationMetricsGrid->addWidget(makeMetric("Accuracy", accuracyValue), 0, 0);
    validationMetricsGrid->addWidget(makeMetric("Macro F1", macroF1Value), 0, 1);
    validationMetricsGrid->addWidget(makeMetric("Sequence", sequenceValue), 0, 2);
    validationMetricsGrid->addWidget(makeMetric("Loss", lossValue), 0, 3);
    modelValidationBody->addLayout(validationMetricsGrid);
    auto validationEvidenceValue = makeModelValue("ModelWorkspaceValidationEvidenceValue", true);
    validationEvidenceValue->setProperty("mutedText", true);
    validationEvidenceValue->setMaximumHeight(72);
    modelValidationBody->addWidget(validationEvidenceValue);
    auto* validationActions = new QHBoxLayout;
    validationActions->setContentsMargins(0, 0, 0, 0);
    validationActions->setSpacing(8);
    auto* revalidateButton = makeSmallButton("Re-validate", "ModelWorkspaceRevalidateButton");
    auto* openReportButton = makeSmallButton("Open Report", "ModelWorkspaceOpenReportButton");
    auto* runValidationButton = makeSmallButton("Run Validation", "ModelWorkspaceRunValidationButton");
    validationActions->addWidget(revalidateButton, 0);
    validationActions->addWidget(openReportButton, 0);
    validationActions->addWidget(runValidationButton, 0);
    validationActions->addStretch(1);
    modelValidationBody->addLayout(validationActions);

    auto modelPromotionPanel = makePanel("Promotion policy");
    modelPromotionPanel->setObjectName("ModelPromotionPolicyPanel");
    auto modelPromotionBody = makePanelBody(modelPromotionPanel);
    auto promotionStatusValue = makeModelValue("ModelWorkspacePromotionStatusValue");
    auto promotionRecordValue = makeModelValue("ModelWorkspacePromotionRecordValue", true);
    auto limitationsValue = makeModelValue("ModelWorkspaceLimitationsValue");
    auto blockersValue = makeModelValue("ModelWorkspaceBlockersValue", true);
    modelPromotionBody->addWidget(promotionStatusValue);
    modelPromotionBody->addWidget(promotionRecordValue);
    modelPromotionBody->addWidget(limitationsValue);
    modelPromotionBody->addWidget(blockersValue);

    auto* benchmarkBody = new QWidget;
    auto* benchmarkLayout = new QGridLayout;
    benchmarkLayout->setContentsMargins(0, 0, 0, 0);
    benchmarkLayout->setHorizontalSpacing(1);
    benchmarkLayout->setVerticalSpacing(1);
    auto* meanLatencyValue = new QLabel("--");
    auto* p99LatencyValue = new QLabel("--");
    auto* throughputValue = new QLabel("--");
    auto* benchmarkAccuracyValue = new QLabel("--");
    benchmarkLayout->addWidget(makeMetric("Mean latency", meanLatencyValue), 0, 0);
    benchmarkLayout->addWidget(makeMetric("P99", p99LatencyValue), 0, 1);
    benchmarkLayout->addWidget(makeMetric("Throughput", throughputValue), 0, 2);
    benchmarkLayout->addWidget(makeMetric("Top-1 accuracy", benchmarkAccuracyValue), 0, 3);
    benchmarkBody->setLayout(benchmarkLayout);
    auto* benchmarkDisclosure = desktop_app::ui::makeCollapsedGroup("Inference benchmark", benchmarkBody);
    benchmarkDisclosure->setObjectName("ModelBenchmarkDisclosure");

    modelDetailLayout->addWidget(modelOverviewPanel);
    modelDetailLayout->addWidget(modelMetadataPanel);
    modelDetailLayout->addWidget(modelTargetPanel);
    modelDetailLayout->addWidget(modelValidationPanel);
    modelDetailLayout->addWidget(modelPromotionPanel);
    modelDetailLayout->addWidget(benchmarkDisclosure);
    modelDetailLayout->addStretch(1);
    modelDetailStack->setLayout(modelDetailLayout);
    modelDetailScroll->setWidget(modelDetailStack);

    auto setRuntimeTargetClassId = [=](const QString& classId) {
        if (classId.trimmed().isEmpty())
            return;
        const QString canonicalId = canonicalTargetClassId(classId);
        QSettings().setValue(kRuntimeTargetClassIdKey, canonicalId);
        if (controls.appState)
            controls.appState->targetClassId = canonicalId;
        if (controls.targetClassCombo) {
            int match = -1;
            for (int i = 0; i < controls.targetClassCombo->count(); ++i) {
                const QString data = controls.targetClassCombo->itemData(i).toString();
                const QString text = controls.targetClassCombo->itemText(i);
                if (data == canonicalId || text.contains("(" + canonicalId + ")") ||
                    canonicalTargetClassId(text) == canonicalId) {
                    match = i;
                    break;
                }
            }
            if (match >= 0)
                controls.targetClassCombo->setCurrentIndex(match);
        }
    };

    auto selectedEntry = [=]() -> QJsonObject {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            row = 0;
        if (row < 0 || row >= registryEntries->size())
            return {};
        return registryEntries->at(row).toObject();
    };

    auto updateModelWorkspaceDetails = [=]() {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            row = 0;
        if (row < 0 || row >= registryEntries->size())
            return;
        const QJsonObject entry = registryEntries->at(row).toObject();
        const QJsonObject metadataDoc = loadMetadataDoc(entry);
        const QString modelName = registryString(entry, "display_name").isEmpty()
                                      ? registryString(entry, "registry_entry_id")
                                      : registryString(entry, "display_name");
        modelNameValue->setText(modelName);
        registryIdValue->setText(wrapTechnicalText(registryString(entry, "registry_entry_id")));
        architectureValue->setText(metadataArchitectureSummary(metadataDoc));
        modelStateValue->setText(registryString(entry, "state"));
        modelLiveModeValue->setText(registryString(entry, "live_use_mode"));
        const QString modelFileSize = fileSizeSummary(registryString(entry, "model_path"));
        modelFileSizeValue->setText(modelFileSize.isEmpty() ? "(unavailable)" : modelFileSize);
        modelPathValue->setText(wrapTechnicalText(registryString(entry, "model_path")));
        const QString modelSha =
            registryString(entry, "model_sha256").isEmpty() ? "(not recorded)" : registryString(entry, "model_sha256");
        modelShaValue->setText(wrapTechnicalText(modelSha));
        metadataPathValue->setText(wrapTechnicalText(registryString(entry, "metadata_path")));
        const QString metadataSha = registryString(entry, "metadata_sha256").isEmpty()
                                        ? "(not recorded)"
                                        : registryString(entry, "metadata_sha256");
        metadataShaValue->setText(wrapTechnicalText(metadataSha));
        modelClassesValue->setText(classesSummary(entry));
        const QString metadataStatus = registryString(entry, "metadata_status");
        metadataSchemaValue->setText(metadataStatus.isEmpty()
                                         ? "(unknown)"
                                         : metadataStatus + " / " + registryString(entry, "metadata_schema_version"));
        labelSchemaValue->setText(registryString(entry, "label_schema_version").isEmpty()
                                      ? "(unknown)"
                                      : registryString(entry, "label_schema_version"));
        inputSizeValue->setText(inputSizeSummary(metadataDoc));
        const QJsonObject normalization = metadataDoc.value("normalization").toObject();
        const QString mean = jsonStringList(normalization.value("mean").toArray()).join(", ");
        const QString std = jsonStringList(normalization.value("std").toArray()).join(", ");
        normalizationMeanValue->setText(mean.isEmpty() ? "--" : mean);
        normalizationStdValue->setText(std.isEmpty() ? "--" : std);
        const QString targetId = registryNestedString(entry, "target_policy", "target_class_id");
        const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label");
        targetPolicyValue->setText(
            QString("Policy target: %1 (%2)").arg(targetDisplay.isEmpty() ? targetId : targetDisplay, targetId));
        const QString runtimeTargetId = canonicalTargetClassId(
            controls.appState && !controls.appState->targetClassId.isEmpty()
                ? controls.appState->targetClassId
                : QSettings()
                      .value(kRuntimeTargetClassIdKey, controls.targetClassCombo
                                                           ? controls.targetClassCombo->currentData().toString()
                                                           : QString("1"))
                      .toString());
        if (auto* button = targetButtonGroup->button(runtimeTargetId.toInt())) {
            QSignalBlocker blocker(targetButtonGroup);
            button->setChecked(true);
        }
        const QString runtimeTarget =
            controls.targetClassCombo ? controls.targetClassCombo->currentText().trimmed() : runtimeTargetId;
        targetRuntimeValue->setText("Shared runtime target: " +
                                    (runtimeTarget.isEmpty() ? runtimeTargetId : runtimeTarget));
        setActiveButton->setEnabled(!entryIsActive(entry));
        setActiveButton->setToolTip(entryIsActive(entry) ? "Selected model is already active."
                                                         : "Mark the selected model as active.");
        validationStatusValue->setText("Status: " + registryString(entry, "validation_status"));
        const QJsonObject validationSummary = metadataDoc.value("validation_summary").toObject();
        const QJsonObject imageValidation = validationSummary.value("image_validation").toObject();
        const QJsonObject sequenceValidation = validationSummary.value("sequence_validation").toObject();
        const auto percent = [](const QJsonValue& value) {
            return value.isDouble() ? QString::number(value.toDouble() * 100.0, 'f', 1) + "%" : QString("--");
        };
        const auto decimal = [](const QJsonValue& value) {
            return value.isDouble() ? QString::number(value.toDouble(), 'f', 3) : QString("--");
        };
        accuracyValue->setText(percent(imageValidation.value("accuracy")));
        macroF1Value->setText(decimal(imageValidation.value("macro_f1")));
        sequenceValue->setText(sequenceValidation.value("status").toString("--"));
        lossValue->setText(decimal(imageValidation.value("loss")));
        const QJsonObject benchmark = metadataDoc.value("benchmark").toObject();
        const QJsonObject inferenceBenchmark = metadataDoc.value("inference_benchmark").toObject();
        meanLatencyValue->setText(decimal(benchmark.value("mean_latency_ms").isUndefined()
                                              ? inferenceBenchmark.value("mean_latency_ms")
                                              : benchmark.value("mean_latency_ms")));
        p99LatencyValue->setText(decimal(benchmark.value("p99_latency_ms").isUndefined()
                                             ? imageValidation.value("p99_latency_ms")
                                             : benchmark.value("p99_latency_ms")));
        const QJsonValue throughput = benchmark.value("throughput_fps").isUndefined()
                                          ? inferenceBenchmark.value("throughput_fps")
                                          : benchmark.value("throughput_fps");
        throughputValue->setText(throughput.isDouble() ? QString::number(throughput.toDouble(), 'f', 0) + " fps"
                                                       : "--");
        benchmarkAccuracyValue->setText(percent(imageValidation.value("accuracy")));
        validationEvidenceValue->setText(wrapTechnicalText(jsonPathSummary(entry.value("validation_evidence"))));
        promotionStatusValue->setText("Promotion: " + registryString(entry, "promotion_status"));
        promotionRecordValue->setText(
            wrapTechnicalText("Record: " + (registryString(entry, "promotion_record_path").isEmpty()
                                                ? "(none)"
                                                : registryString(entry, "promotion_record_path"))));
        limitationsValue->setText("Limitations: " + jsonStringList(entry.value("limitations").toArray()).join("; "));
        blockersValue->setText(wrapTechnicalText("Blockers: " + (entry.value("blockers").toArray().isEmpty()
                                                                     ? "(none)"
                                                                     : jsonPathSummary(entry.value("blockers")))));
    };
    QObject::connect(modelRegistryList, &QTableWidget::currentCellChanged,
                     [=](int, int, int, int) { updateModelWorkspaceDetails(); });
    if (controls.targetClassCombo) {
        QObject::connect(controls.targetClassCombo, qOverload<int>(&QComboBox::currentIndexChanged), [=]() {
            if (controls.appState) {
                const QString classId = controls.targetClassCombo->currentData().toString().trimmed();
                if (!classId.isEmpty())
                    controls.appState->targetClassId = canonicalTargetClassId(classId);
            }
            updateModelWorkspaceDetails();
        });
    }
    QObject::connect(targetButtonGroup, &QButtonGroup::idClicked, [=](int id) {
        setRuntimeTargetClassId(QString::number(id));
        updateModelWorkspaceDetails();
    });
    QObject::connect(openMetadataButton, &QPushButton::clicked, [=]() {
        openPathOrWarn(modelWorkspacePage, selectedEntry().value("metadata_path").toString(), "Open Metadata");
    });
    QObject::connect(reloadMetadataButton, &QPushButton::clicked, [=]() { updateModelWorkspaceDetails(); });
    QObject::connect(openReportButton, &QPushButton::clicked, [=]() {
        const QString reportPath = firstExistingEvidencePath(selectedEntry());
        openPathOrWarn(modelWorkspacePage, reportPath, "Open Report");
    });
    auto navigateToValidator = [=]() {
        if (controls.imageValidationAction)
            controls.imageValidationAction->trigger();
        if (controls.workspaceStack && controls.validatorWorkspace) {
            controls.workspaceStack->setCurrentWidget(controls.validatorWorkspace);
        }
    };
    QObject::connect(revalidateButton, &QPushButton::clicked, navigateToValidator);
    QObject::connect(runValidationButton, &QPushButton::clicked, navigateToValidator);
    QObject::connect(modelActionsButton, &QPushButton::clicked, [=]() {
        QMenu menu(modelActionsButton);
        menu.addAction("Open Metadata", [=]() {
            openPathOrWarn(modelWorkspacePage, selectedEntry().value("metadata_path").toString(), "Open Metadata");
        });
        menu.addAction("Open Report", [=]() {
            openPathOrWarn(modelWorkspacePage, firstExistingEvidencePath(selectedEntry()), "Open Report");
        });
        menu.addAction("Run Validation", navigateToValidator);
        menu.exec(modelActionsButton->mapToGlobal(QPoint(0, modelActionsButton->height())));
    });
    QObject::connect(setActiveButton, &QPushButton::clicked, [=]() {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            return;
        QJsonObject selected = registryEntries->at(row).toObject();
        if (!entryIsValidated(selected)) {
            QMessageBox::warning(modelWorkspacePage, "Unvalidated Model",
                                 "This model has no validation pass recorded. Activation is allowed, but run "
                                 "validation before relying on DAQ firing.");
        }
        QJsonArray updated;
        for (int i = 0; i < registryEntries->size(); ++i) {
            QJsonObject entry = registryEntries->at(i).toObject();
            const bool active = i == row;
            entry["selectable_for_normal_live_sorting"] = active;
            if (active) {
                entry["state"] = "promoted_current";
                entry["promotion_status"] = "Active in workspace";
                const QString targetId = registryNestedString(entry, "target_policy", "target_class_id");
                if (!targetId.isEmpty())
                    setRuntimeTargetClassId(targetId);
            } else if (registryString(entry, "state") == "promoted_current") {
                entry["state"] = "available";
                entry["promotion_status"] = "Available";
            }
            updated.append(entry);
        }
        *registryEntries = updated;
        QString error;
        if (!saveRegistryEntriesToPath(controls.registryFilePath, *registryEntries, &error)) {
            QMessageBox::warning(modelWorkspacePage, "Set Active", error);
        }
        populateRegistryList();
        modelRegistryList->selectRow(row);
        updateModelWorkspaceDetails();
    });
    QObject::connect(addModelButton, &QPushButton::clicked, [=]() {
        const QString startDir = modelRegistryDirectory(controls.registryFilePath);
        const QString path = QFileDialog::getOpenFileName(modelWorkspacePage, "Add ONNX Model", startDir,
                                                          "ONNX models (*.onnx);;All files (*.*)");
        if (path.isEmpty())
            return;
        QJsonObject entry;
        const QFileInfo info(path);
        entry["registry_entry_id"] = info.completeBaseName();
        entry["display_name"] = info.fileName();
        entry["state"] = "available";
        entry["live_use_mode"] = "normal";
        entry["selectable_for_normal_live_sorting"] = false;
        entry["model_path"] = path;
        entry["metadata_status"] = "Missing";
        entry["validation_status"] = "No validation on file for this model";
        entry["promotion_status"] = "Available";
        QJsonObject labels;
        labels["0"] = "Empty";
        labels["1"] = "Single";
        labels["2"] = "MoreThanTwo";
        entry["display_labels"] = labels;
        entry["classes"] = QJsonArray{"0", "1", "2"};
        QJsonObject targetPolicy;
        targetPolicy["target_class_id"] = QSettings().value(kRuntimeTargetClassIdKey, "1").toString();
        targetPolicy["target_display_label"] = "Single";
        entry["target_policy"] = targetPolicy;
        registryEntries->append(entry);
        QString error;
        if (!saveRegistryEntriesToPath(controls.registryFilePath, *registryEntries, &error)) {
            QMessageBox::warning(modelWorkspacePage, "Add Model", error);
        }
        populateRegistryList();
        modelRegistryList->selectRow(registryEntries->size() - 1);
        updateModelWorkspaceDetails();
    });
    QObject::connect(refreshModelsButton, &QPushButton::clicked, [=]() {
        QString warning;
        QJsonArray refreshed = loadRegistryEntriesFromPath(controls.registryFilePath, &warning);
        if (refreshed.isEmpty()) {
            QMessageBox::information(modelWorkspacePage, "Refresh Models",
                                     warning.isEmpty() ? "Registry refresh found no model entries." : warning);
            return;
        }
        *registryEntries = refreshed;
        populateRegistryList();
        modelRegistryList->selectRow(0);
        updateModelWorkspaceDetails();
    });
    modelRegistryList->selectRow(0);
    updateModelWorkspaceDetails();
    QTimer::singleShot(2000, modelWorkspacePage, updateModelWorkspaceDetails);

    modelWorkspaceLayout->addWidget(modelWorkspaceRegistryPanel, 0);
    modelWorkspaceLayout->addWidget(modelDetailScroll, 1);
    modelWorkspacePage->setLayout(modelWorkspaceLayout);
    return modelWorkspacePage;
}

} // namespace desktop_app::workspace
