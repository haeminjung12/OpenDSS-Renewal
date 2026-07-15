#include "workspace_validator.h"

#include <QSettings>
#include <QtWidgets>

#include "image_validation_dialog.h"
#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

struct ValidatorMetricWidget {
    QFrame* frame = nullptr;
    QLabel* value = nullptr;
    QLabel* label = nullptr;
    QLabel* sub = nullptr;
};

struct CsvTable {
    QStringList headers;
    QList<QStringList> rows;
};

ValidatorMetricWidget makeValidatorMetric(const QString& label) {
    ValidatorMetricWidget widget;
    widget.frame = new QFrame;
    widget.frame->setProperty("panel", true);
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);
    widget.value = new QLabel("--");
    widget.value->setProperty("metricValue", true);
    widget.label = new QLabel(label);
    widget.label->setProperty("metricLabel", true);
    widget.sub = new QLabel;
    widget.sub->setProperty("mutedText", true);
    widget.sub->hide();
    layout->addWidget(widget.value);
    layout->addWidget(widget.label);
    layout->addWidget(widget.sub);
    widget.frame->setLayout(layout);
    return widget;
}

QJsonObject loadJsonObjectFile(const QString& path) {
    QFile file(path.trimmed());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
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

QString titleCaseLabel(QString value) {
    QString normalized = value.trimmed();
    if (normalized.compare("hit", Qt::CaseInsensitive) == 0)
        return "Hit";
    if (normalized.compare("waste", Qt::CaseInsensitive) == 0)
        return "Waste";
    normalized.replace('_', ' ');
    normalized.replace('-', ' ');
    QStringList words = normalized.simplified().split(' ', Qt::SkipEmptyParts);
    for (QString& word : words) {
        word = word.toLower();
        word[0] = word.at(0).toUpper();
    }
    return words.join(' ');
}

QStringList parseCsvRow(const QString& line) {
    QStringList cells;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == '"') {
                current += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            cells << current;
            current.clear();
            continue;
        }
        current += ch;
    }
    cells << current;
    return cells;
}

CsvTable readCsvTable(const QString& path) {
    CsvTable table;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return table;
    QTextStream stream(&file);
    bool first = true;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (first) {
            table.headers = parseCsvRow(line);
            first = false;
            continue;
        }
        if (!line.trimmed().isEmpty())
            table.rows.append(parseCsvRow(line));
    }
    return table;
}

QString csvValue(const CsvTable& table, const QStringList& row, const QString& header) {
    const int index = table.headers.indexOf(header);
    if (index < 0 || index >= row.size())
        return {};
    return row.at(index);
}

QString reviewSummaryText(int samplesEvaluated, int samplesIncorrect, int samplesFailed) {
    if (samplesEvaluated <= 0)
        return "--";
    if (samplesFailed > 0)
        return "Fail";
    if (samplesIncorrect > 0)
        return "Needs review";
    return "Pass";
}

QString defaultDisplayLabelForClassId(const QStringList& classIds, const QString& classId) {
    if (classIds == QStringList{"0", "1"}) {
        if (classId == "0")
            return "Non-target";
        if (classId == "1")
            return "Target";
    }
    if (classIds == QStringList{"0", "1", "2"}) {
        if (classId == "0")
            return "Non-target A";
        if (classId == "1")
            return "Target";
        if (classId == "2")
            return "Non-target B";
    }
    return titleCaseLabel(classId);
}

QMap<QString, QString> resolvedDisplayLabels(const QJsonObject& summary, const QStringList& classIds) {
    QMap<QString, QString> labels;
    for (const QString& classId : classIds)
        labels.insert(classId, defaultDisplayLabelForClassId(classIds, classId));

    auto mergeLabels = [&](const QJsonObject& values) {
        for (const QString& classId : classIds) {
            const QString displayLabel = values.value(classId).toString().trimmed();
            if (!displayLabel.isEmpty())
                labels[classId] = displayLabel;
        }
    };

    const QString metadataPath = summary.value("model").toObject().value("metadata_path").toString().trimmed();
    if (!metadataPath.isEmpty())
        mergeLabels(loadJsonObjectFile(metadataPath).value("display_labels").toObject());
    mergeLabels(summary.value("display_labels").toObject());
    return labels;
}

QString displayLabelForClassId(const QStringList& classIds, const QMap<QString, QString>& displayLabels,
                               const QString& classId) {
    const QString label = displayLabels.value(classId).trimmed();
    return label.isEmpty() ? defaultDisplayLabelForClassId(classIds, classId) : label;
}

QString targetClassIdFromSummary(const QJsonObject& summary, const QStringList& classIds) {
    QString targetClassId = summary.value("target_class_id").toString().trimmed();
    if (targetClassId.isEmpty())
        targetClassId = summary.value("model").toObject().value("target_class_id").toString().trimmed();
    if (!targetClassId.isEmpty())
        return targetClassId;
    if (classIds.contains("1"))
        return "1";
    return classIds.size() == 2 ? classIds.value(1) : QString();
}

void setMetricTitle(const ValidatorMetricWidget& widget, const QString& title) {
    if (widget.label)
        widget.label->setText(title);
}

void setMetricValue(const ValidatorMetricWidget& widget, const QString& value, const QString& sub = QString()) {
    widget.value->setText(value.isEmpty() ? "--" : value);
    if (sub.isEmpty()) {
        widget.sub->clear();
        widget.sub->hide();
    } else {
        widget.sub->setText(sub);
        widget.sub->show();
    }
}

void populateConfusionTable(QTableWidget* table, const QStringList& classIds,
                            const QMap<QString, QString>& displayLabels, const QString& confusionPath) {
    table->clear();
    if (classIds.isEmpty()) {
        table->setRowCount(0);
        table->setColumnCount(0);
        return;
    }

    table->setRowCount(classIds.size());
    table->setColumnCount(classIds.size());
    QStringList headers;
    for (const QString& classId : classIds)
        headers << displayLabelForClassId(classIds, displayLabels, classId);
    table->setHorizontalHeaderLabels(headers);
    table->setVerticalHeaderLabels(headers);
    for (int row = 0; row < classIds.size(); ++row) {
        for (int column = 0; column < classIds.size(); ++column) {
            auto* item = new QTableWidgetItem("--");
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(row, column, item);
        }
    }

    const CsvTable csv = readCsvTable(confusionPath);
    if (csv.headers.isEmpty())
        return;
    for (int row = 0; row < classIds.size() && row < csv.rows.size(); ++row) {
        const QStringList csvRow = csv.rows.at(row);
        for (int column = 0; column < classIds.size(); ++column) {
            const QString value = csvValue(csv, csvRow, "pred_" + classIds.at(column));
            if (auto* item = table->item(row, column))
                item->setText(value.isEmpty() ? "0" : value);
        }
    }
    table->horizontalHeader()->setStretchLastSection(true);
    table->resizeColumnsToContents();
}

void clearLayout(QLayout* layout) {
    while (auto* item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void populateFailureGrid(QWidget* grid, const QStringList& classIds, const QMap<QString, QString>& displayLabels,
                         const QString& failureCasesPath) {
    auto* layout = qobject_cast<QGridLayout*>(grid->layout());
    if (!layout)
        return;

    clearLayout(layout);
    const CsvTable csv = readCsvTable(failureCasesPath);
    if (csv.headers.isEmpty() || csv.rows.isEmpty()) {
        auto* emptyLabel = new QLabel("No misclassified samples in the latest validation run.");
        emptyLabel->setProperty("mutedText", true);
        emptyLabel->setWordWrap(true);
        layout->addWidget(emptyLabel, 0, 0, 1, 3);
        return;
    }

    const int limit = qMin(9, csv.rows.size());
    for (int index = 0; index < limit; ++index) {
        const QStringList row = csv.rows.at(index);
        const QString sampleId = csvValue(csv, row, "sample_id");
        const QString imagePath = csvValue(csv, row, "image_path");
        const QString trueClassId = csvValue(csv, row, "true_label");
        const QString predClassId = csvValue(csv, row, "pred_label");
        const QString trueLabel = displayLabelForClassId(classIds, displayLabels, trueClassId);
        const QString predLabel = displayLabelForClassId(classIds, displayLabels, predClassId);

        auto* tile = new QFrame;
        tile->setProperty("datasetTile", true);
        tile->setMinimumSize(104, 116);
        auto* tileLayout = new QVBoxLayout;
        tileLayout->setContentsMargins(7, 7, 7, 6);
        tileLayout->setSpacing(4);

        auto* thumb = new QLabel;
        thumb->setProperty("datasetThumb", true);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setMinimumHeight(58);
        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            thumb->setPixmap(pixmap.scaled(QSize(88, 58), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumb->setText("sample");
        }

        auto* verdict = new QLabel(QString("%1 -> %2").arg(trueLabel, predLabel));
        verdict->setWordWrap(true);
        verdict->setAlignment(Qt::AlignCenter);
        auto* id = new QLabel(sampleId.isEmpty() ? QString("sample_%1").arg(index + 1) : sampleId);
        id->setProperty("mutedText", true);
        id->setAlignment(Qt::AlignCenter);
        id->setWordWrap(true);

        tileLayout->addWidget(thumb, 1);
        tileLayout->addWidget(verdict);
        tileLayout->addWidget(id);
        tile->setLayout(tileLayout);
        layout->addWidget(tile, index / 3, index % 3);
    }
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

    auto validatorImagePanel = makePanel("Validation", "Run the current model on training images");
    validatorImagePanel->setObjectName("ValidatorImageValidationPanel");
    auto validatorImageBody = makePanelBody(validatorImagePanel);
    auto* validatorImageWidget =
        new ImageValidationWidget(validatorImagePanel, controls.pythonExecutable, controls.modelPath,
                                  controls.metadataPath, controls.datasetPath, controls.outputPath,
                                  controls.trainerPythonPath, ImageValidationWidget::ObjectNameMode::Workspace);
    nameWidget(validatorImageWidget, "ValidatorWorkspaceImageValidationWidget");
    validatorImageBody->addWidget(validatorImageWidget);

    auto validatorReportPanel = makePanel("Latest results", "Validation summary");
    validatorReportPanel->setObjectName("ValidatorLastReportPanel");
    auto validatorReportBody = makePanelBody(validatorReportPanel);
    auto validatorMetricsRow = new QHBoxLayout;
    validatorMetricsRow->setSpacing(8);
    const ValidatorMetricWidget checkedMetric = makeValidatorMetric("Images checked");
    const ValidatorMetricWidget hitMetric = makeValidatorMetric("Target");
    const ValidatorMetricWidget wasteMetric = makeValidatorMetric("Non-target");
    const ValidatorMetricWidget reviewMetric = makeValidatorMetric("Review summary");
    validatorMetricsRow->addWidget(checkedMetric.frame);
    validatorMetricsRow->addWidget(hitMetric.frame);
    validatorMetricsRow->addWidget(wasteMetric.frame);
    validatorMetricsRow->addWidget(reviewMetric.frame);
    validatorReportBody->addLayout(validatorMetricsRow);

    auto validatorSummaryDiagnostic = new QLabel("Choose a model and training images, then run validation.");
    validatorSummaryDiagnostic->setProperty("mutedText", true);
    validatorSummaryDiagnostic->setWordWrap(true);
    nameWidget(validatorSummaryDiagnostic, "ValidatorWorkspaceSummaryDiagnosticLabel");
    validatorReportBody->addWidget(validatorSummaryDiagnostic);

    auto validatorConfusionLabel = new QLabel("Results by label");
    validatorConfusionLabel->setProperty("metricLabel", true);
    validatorReportBody->addWidget(validatorConfusionLabel);
    auto validatorConfusion = new QTableWidget(0, 0);
    nameWidget(validatorConfusion, "ValidatorWorkspaceConfusionTable");
    validatorConfusion->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validatorConfusion->setSelectionMode(QAbstractItemView::NoSelection);
    validatorConfusion->setShowGrid(false);
    validatorConfusion->setMaximumHeight(150);
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
    validatorRightScroll->setMinimumWidth(390);
    validatorRightScroll->setMaximumWidth(460);
    validatorRightScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto validatorRightStack = new QWidget;
    nameWidget(validatorRightStack, "ValidatorWorkspaceRightStack");
    auto validatorRightLayout = new QVBoxLayout;
    validatorRightLayout->setContentsMargins(0, 0, 2, 0);
    validatorRightLayout->setSpacing(12);

    auto validatorMisclassifiedPanel = makePanel("Misclassified samples", "Examples that need review");
    validatorMisclassifiedPanel->setObjectName("ValidatorMisclassifiedPanel");
    auto validatorMisclassifiedBody = makePanelBody(validatorMisclassifiedPanel);
    auto validatorSampleGrid = new QWidget;
    nameWidget(validatorSampleGrid, "ValidatorWorkspaceMisclassifiedGrid");
    auto validatorSampleLayout = new QGridLayout;
    validatorSampleLayout->setContentsMargins(0, 0, 0, 0);
    validatorSampleLayout->setSpacing(8);
    validatorSampleGrid->setLayout(validatorSampleLayout);
    validatorMisclassifiedBody->addWidget(validatorSampleGrid);

    validatorRightLayout->addWidget(validatorMisclassifiedPanel);
    validatorRightLayout->addStretch(1);
    validatorRightStack->setLayout(validatorRightLayout);
    validatorRightScroll->setWidget(validatorRightStack);

    validatorWorkspaceLayout->addWidget(validatorLeftScroll, 1);
    validatorWorkspaceLayout->addWidget(validatorRightScroll, 0);
    validatorWorkspacePage->setLayout(validatorWorkspaceLayout);

    auto refreshReport = [=](const QString& requestedSummaryPath) {
        const QString summaryPath = requestedSummaryPath.trimmed().isEmpty() ? validationSummaryPath() : requestedSummaryPath;
        QString parseDiagnostic;
        const QJsonObject summary = summaryPath.isEmpty() ? QJsonObject() : loadSummaryArtifact(summaryPath, &parseDiagnostic);
        if (!parseDiagnostic.isEmpty()) {
            setMetricTitle(hitMetric, "Target");
            setMetricTitle(wasteMetric, "Non-target");
            setMetricValue(checkedMetric, "--");
            setMetricValue(hitMetric, "--");
            setMetricValue(wasteMetric, "--");
            setMetricValue(reviewMetric, "Needs review");
            validatorSummaryDiagnostic->setText("The latest validation report could not be read: " + parseDiagnostic);
            validatorConfusionLabel->setText("Results by label");
            populateConfusionTable(validatorConfusion, QStringList(), QMap<QString, QString>(), QString());
            populateFailureGrid(validatorSampleGrid, QStringList(), QMap<QString, QString>(), QString());
            return;
        }

        if (summary.isEmpty()) {
            setMetricTitle(hitMetric, "Target");
            setMetricTitle(wasteMetric, "Non-target");
            setMetricValue(checkedMetric, "--");
            setMetricValue(hitMetric, "--");
            setMetricValue(wasteMetric, "--");
            setMetricValue(reviewMetric, "--");
            validatorSummaryDiagnostic->setText("Choose a model and training images, then run validation.");
            validatorConfusionLabel->setText("Results by label");
            populateConfusionTable(validatorConfusion, QStringList(), QMap<QString, QString>(), QString());
            populateFailureGrid(validatorSampleGrid, QStringList(), QMap<QString, QString>(), QString());
            return;
        }

        const QJsonObject dataset = summary.value("dataset").toObject();
        const QJsonObject metrics = summary.value("metrics").toObject();
        const QJsonObject artifacts = summary.value("artifacts").toObject();
        QStringList classIds;
        for (const QJsonValue& labelValue : summary.value("labels").toArray())
            classIds << labelValue.toString();
        const QMap<QString, QString> displayLabels = resolvedDisplayLabels(summary, classIds);

        const int samplesTotal = dataset.value("samples_total").toInt();
        const int samplesEvaluated = dataset.value("samples_evaluated").toInt();
        const int samplesFailed = dataset.value("samples_failed").toInt();
        const int samplesIncorrect = metrics.value("samples_incorrect").toInt();
        const QJsonObject classCounts = dataset.value("class_counts").toObject();
        const QString reviewSummary = reviewSummaryText(samplesEvaluated, samplesIncorrect, samplesFailed);
        const QString accuracyText = metrics.contains("accuracy")
                                         ? QString::number(metrics.value("accuracy").toDouble() * 100.0, 'f', 1) + "%"
                                         : QString();

        if (classIds.size() <= 2) {
            const QString firstClassId = classIds.value(0);
            const QString secondClassId = classIds.value(1);
            setMetricTitle(hitMetric, displayLabelForClassId(classIds, displayLabels, firstClassId));
            setMetricTitle(wasteMetric, displayLabelForClassId(classIds, displayLabels, secondClassId));
            setMetricValue(hitMetric, firstClassId.isEmpty() ? QString() : QString::number(classCounts.value(firstClassId).toInt()),
                           firstClassId.isEmpty() ? QString() : QString("class id %1").arg(firstClassId));
            setMetricValue(wasteMetric,
                           secondClassId.isEmpty() ? QString() : QString::number(classCounts.value(secondClassId).toInt()),
                           secondClassId.isEmpty() ? QString() : QString("class id %1").arg(secondClassId));
        } else {
            const QString targetClassId = targetClassIdFromSummary(summary, classIds);
            const QString targetLabel = displayLabelForClassId(classIds, displayLabels, targetClassId);
            QStringList nonTargetLabels;
            int nonTargetCount = 0;
            for (const QString& classId : classIds) {
                if (classId == targetClassId)
                    continue;
                nonTargetCount += classCounts.value(classId).toInt();
                nonTargetLabels << displayLabelForClassId(classIds, displayLabels, classId);
            }
            setMetricTitle(hitMetric, targetLabel.isEmpty() ? "Target class" : targetLabel);
            setMetricTitle(wasteMetric, "Other labels");
            setMetricValue(hitMetric, targetClassId.isEmpty() ? QString() : QString::number(classCounts.value(targetClassId).toInt()),
                           targetClassId.isEmpty() ? QString() : QString("class id %1").arg(targetClassId));
            setMetricValue(wasteMetric, QString::number(nonTargetCount),
                           nonTargetLabels.isEmpty() ? QString() : nonTargetLabels.join(", "));
        }

        setMetricValue(checkedMetric, QString("%1 / %2").arg(samplesEvaluated).arg(samplesTotal),
                       samplesFailed > 0 ? QString("%1 failed").arg(samplesFailed) : QString());
        setMetricValue(reviewMetric, reviewSummary, accuracyText.isEmpty() ? QString() : "accuracy " + accuracyText);

        QStringList summaryLines;
        summaryLines << QString("Latest run: %1.").arg(titleCaseLabel(summary.value("status").toString("unknown")));
        summaryLines << QString("Checked %1 of %2 images.").arg(samplesEvaluated).arg(samplesTotal);
        if (!classIds.isEmpty()) {
            QStringList labelSummary;
            for (const QString& classId : classIds)
                labelSummary << QString("%1 (%2)").arg(displayLabelForClassId(classIds, displayLabels, classId), classId);
            summaryLines << "Labels: " + labelSummary.join(", ") + ".";
        }
        if (samplesIncorrect > 0 || samplesFailed > 0) {
            summaryLines << QString("%1 images need review and %2 failed to evaluate.")
                                .arg(samplesIncorrect)
                                .arg(samplesFailed);
        } else {
            summaryLines << "No misclassified samples were reported in the latest run.";
        }
        validatorSummaryDiagnostic->setText(summaryLines.join(" "));

        validatorConfusionLabel->setText("Results by label");
        populateConfusionTable(validatorConfusion, classIds, displayLabels,
                               artifacts.value("confusion_matrix_csv").toString());
        populateFailureGrid(validatorSampleGrid, classIds, displayLabels,
                            artifacts.value("failure_cases_csv").toString());
    };

    validatorImageWidget->setSummaryChangedCallback(refreshReport);
    refreshReport(validationSummaryPath());

    return validatorWorkspacePage;
}

} // namespace desktop_app::workspace
