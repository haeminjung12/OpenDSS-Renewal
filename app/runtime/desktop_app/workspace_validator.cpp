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
            item->setTextAlignment(Qt::AlignCenter);
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
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

QString percentageText(const QString& value) {
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok ? QString::number(number * 100.0, 'f', 1) + "%" : QString("--");
}

void populateClassMetricsTable(QTableWidget* table, const QStringList& classIds,
                               const QMap<QString, QString>& displayLabels, const QString& metricsPath) {
    table->clearContents();
    table->setRowCount(0);
    const CsvTable csv = readCsvTable(metricsPath);
    if (csv.headers.isEmpty())
        return;

    table->setRowCount(csv.rows.size());
    for (int row = 0; row < csv.rows.size(); ++row) {
        const QStringList values = csv.rows.at(row);
        const QString classId = csvValue(csv, values, "label");
        const int support = csvValue(csv, values, "support").toInt();
        const int correct = csvValue(csv, values, "true_positive").toInt();
        const QStringList cells = {
            displayLabelForClassId(classIds, displayLabels, classId),
            percentageText(csvValue(csv, values, "precision")),
            percentageText(csvValue(csv, values, "recall")),
            percentageText(csvValue(csv, values, "f1")),
            QString("%1 / %2").arg(correct).arg(support),
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells.at(column));
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, column, item);
        }
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void clearLayout(QLayout* layout) {
    while (auto* item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void relabelWorkspaceValidationWidget(ImageValidationWidget* widget) {
    if (!widget)
        return;

    if (auto* startButton = widget->findChild<QPushButton*>("ValidatorWorkspaceOpenImageValidationButton"))
        startButton->setText("Test Model");
    if (auto* openSummaryButton = widget->findChild<QPushButton*>("ValidatorWorkspaceOpenSummaryButton"))
        openSummaryButton->setText("Open Test Results");
    if (auto* openOutputButton = widget->findChild<QPushButton*>("ValidatorWorkspaceOpenOutputButton"))
        openOutputButton->setText("Open Results Folder");
    if (auto* artifactLabel = widget->findChild<QLabel*>("ValidatorWorkspaceArtifactLabel"))
        artifactLabel->setText("No test results yet.");

    auto* rootLayout = qobject_cast<QVBoxLayout*>(widget->layout());
    if (!rootLayout || rootLayout->count() == 0)
        return;

    auto* form = qobject_cast<QGridLayout*>(rootLayout->itemAt(0)->layout());
    if (!form)
        return;

    const auto setRowLabel = [form](int row, const QString& text) {
        if (auto* item = form->itemAtPosition(row, 0)) {
            if (auto* label = qobject_cast<QLabel*>(item->widget()))
                label->setText(text);
        }
    };

    setRowLabel(0, "Test model");
    setRowLabel(1, "Test dataset");
    setRowLabel(2, "Test results");
}

void populateFailureGrid(QWidget* grid, const QStringList& classIds, const QMap<QString, QString>& displayLabels,
                         const QString& failureCasesPath) {
    auto* layout = qobject_cast<QGridLayout*>(grid->layout());
    if (!layout)
        return;

    clearLayout(layout);
    const CsvTable csv = readCsvTable(failureCasesPath);
    if (csv.headers.isEmpty() || csv.rows.isEmpty()) {
        auto* emptyLabel = new QLabel("No misclassified samples in the latest model test.");
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

    auto validatorImagePanel = makePanel("Setup", "Choose a model and prepared test dataset");
    validatorImagePanel->setObjectName("ValidatorImageValidationPanel");
    auto validatorImageBody = makePanelBody(validatorImagePanel);
    auto* validatorImageWidget =
        new ImageValidationWidget(validatorImagePanel, controls.pythonExecutable, controls.modelPath,
                                  controls.metadataPath, controls.datasetPath, controls.outputPath,
                                  controls.trainerPythonPath, ImageValidationWidget::ObjectNameMode::Workspace);
    nameWidget(validatorImageWidget, "ValidatorWorkspaceImageValidationWidget");
    validatorImageWidget->refreshModelRegistry(controls.registryEntries);
    relabelWorkspaceValidationWidget(validatorImageWidget);
    validatorImageBody->addWidget(validatorImageWidget);

    auto validatorReportPanel = makePanel("Test results", "Model testing summary");
    validatorReportPanel->setObjectName("ValidatorLastReportPanel");
    auto validatorReportBody = makePanelBody(validatorReportPanel);
    auto validatorMetricsRow = new QHBoxLayout;
    validatorMetricsRow->setSpacing(8);
    const ValidatorMetricWidget accuracyMetric = makeValidatorMetric("Accuracy");
    const ValidatorMetricWidget macroF1Metric = makeValidatorMetric("Macro F1");
    const ValidatorMetricWidget correctMetric = makeValidatorMetric("Correct / Total");
    const ValidatorMetricWidget incorrectMetric = makeValidatorMetric("Incorrect");
    validatorMetricsRow->addWidget(accuracyMetric.frame);
    validatorMetricsRow->addWidget(macroF1Metric.frame);
    validatorMetricsRow->addWidget(correctMetric.frame);
    validatorMetricsRow->addWidget(incorrectMetric.frame);
    validatorReportBody->addLayout(validatorMetricsRow);

    auto validatorSummaryDiagnostic = new QLabel("Choose a model and test dataset, then run model testing.");
    validatorSummaryDiagnostic->setProperty("mutedText", true);
    validatorSummaryDiagnostic->setWordWrap(true);
    nameWidget(validatorSummaryDiagnostic, "ValidatorWorkspaceSummaryDiagnosticLabel");
    validatorReportBody->addWidget(validatorSummaryDiagnostic);

    auto validatorConfusionLabel = new QLabel("Confusion matrix");
    validatorConfusionLabel->setProperty("metricLabel", true);
    validatorReportBody->addWidget(validatorConfusionLabel);
    auto validatorConfusion = new QTableWidget(0, 0);
    nameWidget(validatorConfusion, "ValidatorWorkspaceConfusionTable");
    validatorConfusion->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validatorConfusion->setSelectionMode(QAbstractItemView::NoSelection);
    validatorConfusion->setShowGrid(false);
    validatorConfusion->setFont(QFont(validatorConfusion->font().family(), 11));
    validatorConfusion->verticalHeader()->setDefaultSectionSize(34);
    validatorConfusion->setMaximumHeight(150);
    validatorReportBody->addWidget(validatorConfusion);

    auto validatorClassMetricsLabel = new QLabel("Performance by class");
    validatorClassMetricsLabel->setProperty("metricLabel", true);
    validatorReportBody->addWidget(validatorClassMetricsLabel);
    auto validatorClassMetrics = new QTableWidget(0, 5);
    nameWidget(validatorClassMetrics, "ValidatorWorkspaceClassMetricsTable");
    validatorClassMetrics->setHorizontalHeaderLabels(
        {"Class", "Precision", "Recall", "F1", "Correct / Total"});
    validatorClassMetrics->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validatorClassMetrics->setSelectionMode(QAbstractItemView::NoSelection);
    validatorClassMetrics->setShowGrid(false);
    validatorClassMetrics->setFont(QFont(validatorClassMetrics->font().family(), 11));
    validatorClassMetrics->verticalHeader()->setDefaultSectionSize(34);
    validatorClassMetrics->setMaximumHeight(180);
    validatorReportBody->addWidget(validatorClassMetrics);

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

    auto* validatorWorkspaceSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(validatorWorkspaceSplitter, "ValidatorWorkspaceSplitter");
    validatorWorkspaceSplitter->addWidget(validatorLeftScroll);
    validatorWorkspaceSplitter->addWidget(validatorRightScroll);
    validatorWorkspaceSplitter->setStretchFactor(0, 1);
    validatorWorkspaceSplitter->setStretchFactor(1, 0);
    validatorWorkspaceSplitter->setMaximumWidth(1100);
    desktop_app::ui::configureWorkspaceSplitter(validatorWorkspaceSplitter, "workspace/validator/splitter",
                                                {760, 400}, {520, 390});
    validatorWorkspaceLayout->addWidget(validatorWorkspaceSplitter, 1);
    validatorWorkspaceLayout->addStretch(1);
    validatorWorkspacePage->setLayout(validatorWorkspaceLayout);

    auto refreshReport = [=](const QString& requestedSummaryPath) {
        const QString summaryPath = requestedSummaryPath.trimmed().isEmpty() ? validationSummaryPath() : requestedSummaryPath;
        QString parseDiagnostic;
        const QJsonObject summary = summaryPath.isEmpty() ? QJsonObject() : loadSummaryArtifact(summaryPath, &parseDiagnostic);
        if (!parseDiagnostic.isEmpty()) {
            validatorSummaryDiagnostic->setVisible(true);
            setMetricValue(accuracyMetric, "--");
            setMetricValue(macroF1Metric, "--");
            setMetricValue(correctMetric, "--");
            setMetricValue(incorrectMetric, "--");
            validatorSummaryDiagnostic->setText("The latest test results could not be read: " + parseDiagnostic);
            validatorConfusionLabel->setText("Confusion matrix");
            populateConfusionTable(validatorConfusion, QStringList(), QMap<QString, QString>(), QString());
            populateClassMetricsTable(validatorClassMetrics, QStringList(), QMap<QString, QString>(), QString());
            populateFailureGrid(validatorSampleGrid, QStringList(), QMap<QString, QString>(), QString());
            return;
        }

        if (summary.isEmpty()) {
            validatorSummaryDiagnostic->setVisible(true);
            setMetricValue(accuracyMetric, "--");
            setMetricValue(macroF1Metric, "--");
            setMetricValue(correctMetric, "--");
            setMetricValue(incorrectMetric, "--");
            validatorSummaryDiagnostic->setText("Choose a model and test dataset, then run model testing.");
            validatorConfusionLabel->setText("Confusion matrix");
            populateConfusionTable(validatorConfusion, QStringList(), QMap<QString, QString>(), QString());
            populateClassMetricsTable(validatorClassMetrics, QStringList(), QMap<QString, QString>(), QString());
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
        const QString accuracyText = metrics.contains("accuracy")
                                          ? QString::number(metrics.value("accuracy").toDouble() * 100.0, 'f', 1) + "%"
                                          : QString();
        const QString macroF1Text = metrics.contains("macro_f1")
                                        ? QString::number(metrics.value("macro_f1").toDouble(), 'f', 3)
                                        : QString();
        const int correct = metrics.value("samples_correct").toInt(qMax(0, samplesEvaluated - samplesIncorrect));
        setMetricValue(accuracyMetric, accuracyText);
        setMetricValue(macroF1Metric, macroF1Text);
        setMetricValue(correctMetric, QString("%1 / %2").arg(correct).arg(samplesEvaluated));
        setMetricValue(incorrectMetric, QString::number(samplesIncorrect));
        validatorSummaryDiagnostic->setText(samplesFailed > 0
                                                ? QString("%1 image%2 could not be evaluated.")
                                                      .arg(samplesFailed)
                                                      .arg(samplesFailed == 1 ? QString() : QString("s"))
                                                : QString());
        validatorSummaryDiagnostic->setVisible(samplesFailed > 0);

        validatorConfusionLabel->setText("Confusion matrix");
        populateConfusionTable(validatorConfusion, classIds, displayLabels,
                               artifacts.value("confusion_matrix_csv").toString());
        populateClassMetricsTable(validatorClassMetrics, classIds, displayLabels,
                                  artifacts.value("class_metrics_csv").toString());
        populateFailureGrid(validatorSampleGrid, classIds, displayLabels,
                            artifacts.value("failure_cases_csv").toString());
    };

    validatorImageWidget->setSummaryChangedCallback([refreshReport, callback = controls.imageSummaryChangedCallback](
                                                        const QString& summaryPath) {
        refreshReport(summaryPath);
        if (callback)
            callback(summaryPath);
    });
    refreshReport(validationSummaryPath());

    return validatorWorkspacePage;
}

} // namespace desktop_app::workspace
