#include "workspace_validator.h"

#include <QtWidgets>
#include <QSettings>

#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

QWidget* makeValidatorField(const QString& label, const QString& value, const QString& objectName) {
    auto* wrapper = new QWidget;
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* fieldLabel = new QLabel(label);
    fieldLabel->setProperty("metricLabel", true);
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    auto* edit = new QLineEdit(value);
    edit->setReadOnly(true);
    edit->setMinimumWidth(0);
    nameWidget(edit, objectName.toLatin1().constData());
    auto* browse = new QPushButton("Browse");
    browse->setEnabled(false);
    browse->setToolTip("Use the Image Validation dialog to change validator paths.");
    row->addWidget(edit, 1);
    row->addWidget(browse, 0);
    layout->addWidget(fieldLabel);
    layout->addLayout(row);
    wrapper->setLayout(layout);
    return wrapper;
}

QFrame* makeValidatorMetric(const QString& label, const QString& value, const QString& sub = QString()) {
    auto* frame = new QFrame;
    frame->setProperty("panel", true);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);
    auto* valueLabel = new QLabel(value);
    valueLabel->setProperty("metricValue", true);
    auto* labelText = new QLabel(label);
    labelText->setProperty("metricLabel", true);
    layout->addWidget(valueLabel);
    layout->addWidget(labelText);
    if (!sub.isEmpty()) {
        auto* subLabel = new QLabel(sub);
        subLabel->setProperty("mutedText", true);
        layout->addWidget(subLabel);
    }
    frame->setLayout(layout);
    return frame;
}

QJsonObject loadSummaryArtifact(const QString& path, QString* parseDiagnostic) {
    if (parseDiagnostic)
        parseDiagnostic->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (parseDiagnostic) {
            if (parseError.error != QJsonParseError::NoError) {
                *parseDiagnostic = QString("Malformed JSON in %1 at offset %2: %3")
                                       .arg(QFileInfo(path).fileName())
                                       .arg(parseError.offset)
                                       .arg(parseError.errorString());
            } else {
                *parseDiagnostic = QString("Top-level JSON object expected in %1.").arg(QFileInfo(path).fileName());
            }
        }
        return {};
    }
    return doc.object();
}

QString validationSummaryPath() {
    QSettings settings;
    const QString outputFolder = settings.value("validator/outputFolder").toString().trimmed();
    if (outputFolder.isEmpty())
        return {};
    const QString summaryPath = QDir(outputFolder).filePath("image_validation/validation_summary.json");
    return QFileInfo::exists(summaryPath) ? QFileInfo(summaryPath).absoluteFilePath() : QString();
}

} // namespace

QWidget* buildValidatorWorkspace(const ValidatorWorkspaceControls& controls) {
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    auto validatorWorkspacePage = new QWidget;
    nameWidget(validatorWorkspacePage, "ValidatorWorkspace");
    auto validatorWorkspaceLayout = new QHBoxLayout;
    validatorWorkspaceLayout->setContentsMargins(10, 10, 10, 10);
    validatorWorkspaceLayout->setSpacing(12);

    auto validatorLeftScroll = new QScrollArea;
    nameWidget(validatorLeftScroll, "ValidatorWorkspaceLeftScrollArea");
    validatorLeftScroll->setWidgetResizable(true);
    validatorLeftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto validatorLeftStack = new QWidget;
    nameWidget(validatorLeftStack, "ValidatorWorkspaceLeftStack");
    auto validatorLeftLayout = new QVBoxLayout;
    validatorLeftLayout->setContentsMargins(0, 0, 2, 0);
    validatorLeftLayout->setSpacing(12);

    auto validatorImagePanel = makePanel("Image Validation", "External Python validator");
    validatorImagePanel->setObjectName("ValidatorImageValidationPanel");
    auto validatorImageBody = makePanelBody(validatorImagePanel);
    validatorImageBody->addWidget(
        makeValidatorField("Model artifact", controls.modelPath, "ValidatorWorkspaceModelEdit"));
    validatorImageBody->addWidget(
        makeValidatorField("Model metadata", controls.metadataPath, "ValidatorWorkspaceMetadataEdit"));
    validatorImageBody->addWidget(
        makeValidatorField("Test set", "datasets/prepared/droplet_binary_2026-04-30", "ValidatorWorkspaceDatasetEdit"));
    validatorImageBody->addWidget(
        makeValidatorField("Output folder", "outputs/validation_gui_image", "ValidatorWorkspaceOutputEdit"));
    auto validatorActionRow = new QHBoxLayout;
    validatorActionRow->setSpacing(8);
    auto validatorOpenBtn = new QPushButton("Run Image Validation");
    auto validatorCancelBtn = new QPushButton("Cancel");
    auto validatorSummaryBtn = new QPushButton("Open Summary");
    auto validatorOutputBtn = new QPushButton("Open Output Folder");
    nameWidget(validatorOpenBtn, "ValidatorWorkspaceOpenImageValidationButton");
    nameWidget(validatorCancelBtn, "ValidatorWorkspaceCancelButton");
    nameWidget(validatorSummaryBtn, "ValidatorWorkspaceOpenSummaryButton");
    nameWidget(validatorOutputBtn, "ValidatorWorkspaceOpenOutputButton");
    validatorOpenBtn->setProperty("primaryAction", true);
    validatorCancelBtn->setEnabled(false);
    validatorSummaryBtn->setEnabled(false);
    validatorOutputBtn->setEnabled(false);
    validatorCancelBtn->setToolTip("Cancellation is available inside the running Image Validation dialog.");
    validatorSummaryBtn->setToolTip("Enabled by the Image Validation dialog after a successful run.");
    validatorOutputBtn->setToolTip("Enabled by the Image Validation dialog after a successful run.");
    validatorActionRow->addWidget(validatorOpenBtn);
    validatorActionRow->addWidget(validatorCancelBtn);
    validatorActionRow->addStretch(1);
    validatorActionRow->addWidget(validatorSummaryBtn);
    validatorActionRow->addWidget(validatorOutputBtn);
    validatorImageBody->addLayout(validatorActionRow);
    auto validatorProgress = new QProgressBar;
    nameWidget(validatorProgress, "ValidatorWorkspaceProgressBar");
    validatorProgress->setRange(0, 100);
    validatorProgress->setValue(0);
    validatorProgress->setTextVisible(false);
    validatorImageBody->addWidget(validatorProgress);
    auto validatorStatusRow = new QHBoxLayout;
    auto validatorStatusLabel = new QLabel("Idle - image validation opens in the existing dialog");
    auto validatorEtaLabel = new QLabel("No active subprocess");
    nameWidget(validatorStatusLabel, "ValidatorWorkspaceStatusLabel");
    nameWidget(validatorEtaLabel, "ValidatorWorkspaceEtaLabel");
    validatorStatusLabel->setProperty("mutedText", true);
    validatorEtaLabel->setProperty("mutedText", true);
    validatorStatusRow->addWidget(validatorStatusLabel);
    validatorStatusRow->addStretch(1);
    validatorStatusRow->addWidget(validatorEtaLabel);
    validatorImageBody->addLayout(validatorStatusRow);

    auto validatorSequenceNote =
        new QLabel("Sequence validation remains disabled: runner-wrapped replay is not available in this workspace, "
                   "and provisional artifact comparison is not promoted for public gates.");
    validatorSequenceNote->setWordWrap(true);
    validatorSequenceNote->setProperty("mutedText", true);
    nameWidget(validatorSequenceNote, "ValidatorWorkspaceSequenceValidationNote");
    validatorImageBody->addWidget(validatorSequenceNote);

    auto validatorReportPanel = makePanel("Last report", "Awaiting validation_summary.json");
    validatorReportPanel->setObjectName("ValidatorLastReportPanel");
    auto validatorReportBody = makePanelBody(validatorReportPanel);
    auto validatorMetricsRow = new QHBoxLayout;
    validatorMetricsRow->setSpacing(8);
    validatorMetricsRow->addWidget(makeValidatorMetric("Top-1 accuracy", "--", "held-out"));
    validatorMetricsRow->addWidget(makeValidatorMetric("Macro F1", "--"));
    validatorMetricsRow->addWidget(makeValidatorMetric("ROC AUC", "--"));
    validatorMetricsRow->addWidget(makeValidatorMetric("Latency P99", "--"));
    validatorReportBody->addLayout(validatorMetricsRow);
    auto validatorSummaryDiagnostic = new QLabel("Validation summary diagnostics: not checked");
    validatorSummaryDiagnostic->setProperty("mutedText", true);
    nameWidget(validatorSummaryDiagnostic, "ValidatorWorkspaceSummaryDiagnosticLabel");
    validatorReportBody->addWidget(validatorSummaryDiagnostic);
    auto validatorConfusionLabel = new QLabel("Confusion - no report loaded");
    validatorConfusionLabel->setProperty("metricLabel", true);
    validatorReportBody->addWidget(validatorConfusionLabel);
    auto validatorConfusion = new QTableWidget(3, 3);
    nameWidget(validatorConfusion, "ValidatorWorkspaceConfusionTable");
    validatorConfusion->setHorizontalHeaderLabels({"Empty", "Single", "More"});
    validatorConfusion->setVerticalHeaderLabels({"Empty", "Single", "More"});
    validatorConfusion->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validatorConfusion->setSelectionMode(QAbstractItemView::NoSelection);
    validatorConfusion->setShowGrid(false);
    validatorConfusion->setMaximumHeight(150);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            auto* item = new QTableWidgetItem("--");
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            validatorConfusion->setItem(row, column, item);
        }
    }
    validatorConfusion->horizontalHeader()->setStretchLastSection(true);
    validatorConfusion->resizeColumnsToContents();
    validatorReportBody->addWidget(validatorConfusion);
    validatorLeftLayout->addWidget(validatorImagePanel);
    validatorLeftLayout->addWidget(validatorReportPanel);
    validatorLeftLayout->addStretch(1);
    validatorLeftStack->setLayout(validatorLeftLayout);
    validatorLeftScroll->setWidget(validatorLeftStack);

    auto validatorRightScroll = new QScrollArea;
    nameWidget(validatorRightScroll, "ValidatorWorkspaceRightScrollArea");
    validatorRightScroll->setWidgetResizable(true);
    validatorRightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    validatorRightScroll->setFixedWidth(360);
    validatorRightScroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto validatorRightStack = new QWidget;
    nameWidget(validatorRightStack, "ValidatorWorkspaceRightStack");
    auto validatorRightLayout = new QVBoxLayout;
    validatorRightLayout->setContentsMargins(0, 0, 2, 0);
    validatorRightLayout->setSpacing(12);

    auto validatorMisclassifiedPanel = makePanel("Misclassified samples");
    validatorMisclassifiedPanel->setObjectName("ValidatorMisclassifiedPanel");
    auto validatorMisclassifiedBody = makePanelBody(validatorMisclassifiedPanel);
    auto validatorSampleGrid = new QWidget;
    nameWidget(validatorSampleGrid, "ValidatorWorkspaceMisclassifiedGrid");
    auto validatorSampleLayout = new QGridLayout;
    validatorSampleLayout->setContentsMargins(0, 0, 0, 0);
    validatorSampleLayout->setSpacing(8);
    for (int i = 0; i < 9; ++i) {
        auto* tile = new QFrame;
        tile->setProperty("datasetTile", true);
        tile->setFixedSize(96, 78);
        auto* tileLayout = new QVBoxLayout;
        tileLayout->setContentsMargins(7, 7, 7, 6);
        tileLayout->setSpacing(4);
        auto* thumb = new QLabel("sample");
        thumb->setProperty("datasetThumb", true);
        thumb->setAlignment(Qt::AlignCenter);
        auto* id = new QLabel(QString("err_%1").arg(i + 1, 3, 10, QChar('0')));
        id->setProperty("mutedText", true);
        id->setAlignment(Qt::AlignCenter);
        tileLayout->addWidget(thumb, 1);
        tileLayout->addWidget(id);
        tile->setLayout(tileLayout);
        validatorSampleLayout->addWidget(tile, i / 3, i % 3);
    }
    validatorSampleGrid->setLayout(validatorSampleLayout);
    validatorMisclassifiedBody->addWidget(validatorSampleGrid);

    auto validatorLogPanel = makePanel("Validator log");
    validatorLogPanel->setObjectName("ValidatorLogPanel");
    auto validatorLogBody = makePanelBody(validatorLogPanel);
    auto validatorLog = new QPlainTextEdit;
    nameWidget(validatorLog, "ValidatorWorkspaceLogTextEdit");
    validatorLog->setReadOnly(true);
    validatorLog->setMaximumHeight(170);
    validatorLog->setPlainText("Idle. Run Image Validation opens the existing validator dialog, where stdout/stderr "
                               "streaming and cancellation are preserved.");
    validatorLogBody->addWidget(validatorLog);
    auto validatorSequenceButton = new QPushButton("Sequence Validation");
    nameWidget(validatorSequenceButton, "ValidatorWorkspaceSequenceValidationButton");
    validatorSequenceButton->setEnabled(false);
    validatorSequenceButton->setToolTip("Runner-wrapped sequence validation remains unavailable.");
    validatorLogBody->addWidget(validatorSequenceButton);

    validatorRightLayout->addWidget(validatorMisclassifiedPanel);
    validatorRightLayout->addWidget(validatorLogPanel);
    validatorRightLayout->addStretch(1);
    validatorRightStack->setLayout(validatorRightLayout);
    validatorRightScroll->setWidget(validatorRightStack);

    validatorWorkspaceLayout->addWidget(validatorLeftScroll, 1);
    validatorWorkspaceLayout->addWidget(validatorRightScroll, 0);
    validatorWorkspacePage->setLayout(validatorWorkspaceLayout);

    const QString summaryPath = validationSummaryPath();
    QString parseDiagnostic;
    if (!summaryPath.isEmpty()) {
        const QJsonObject summary = loadSummaryArtifact(summaryPath, &parseDiagnostic);
        if (!parseDiagnostic.isEmpty()) {
            validatorSummaryDiagnostic->setText("Validation summary parse diagnostic: " + parseDiagnostic);
            validatorSummaryBtn->setEnabled(false);
        } else if (!summary.isEmpty()) {
            const QString status = summary.value("status").toString("unknown");
            const QJsonObject metrics = summary.value("metrics").toObject();
            const QString accuracy = metrics.value("accuracy").toString();
            validatorSummaryDiagnostic->setText(
                QString("Loaded validation summary from %1 (status=%2).")
                    .arg(QFileInfo(summaryPath).fileName(), status) +
                (accuracy.isEmpty() ? QString() : QString(" accuracy=%1").arg(accuracy)));
        }
    } else {
        validatorSummaryDiagnostic->setText("Validation summary diagnostics: summary path was not found in settings.");
    }
    if (!parseDiagnostic.isEmpty()) {
        validatorLog->appendPlainText("Validation summary parse diagnostic: " + parseDiagnostic);
    }

    if (controls.imageValidationAction) {
        QObject::connect(validatorOpenBtn, &QPushButton::clicked, controls.imageValidationAction, &QAction::trigger);
    }

    return validatorWorkspacePage;
}

} // namespace desktop_app::workspace
