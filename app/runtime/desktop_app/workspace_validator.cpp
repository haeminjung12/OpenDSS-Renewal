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
    auto* labelText = new QLabel(label);
    labelText->setProperty("metricLabel", true);
    widget.sub = new QLabel;
    widget.sub->setProperty("mutedText", true);
    widget.sub->hide();
    layout->addWidget(widget.value);
    layout->addWidget(labelText);
    layout->addWidget(widget.sub);
    widget.frame->setLayout(layout);
    return widget;
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

void populateConfusionTable(QTableWidget* table, const QStringList& labels, const QString& confusionPath) {
    table->clear();
    if (labels.isEmpty()) {
        table->setRowCount(0);
        table->setColumnCount(0);
        return;
    }

    table->setRowCount(labels.size());
    table->setColumnCount(labels.size());
    QStringList displayLabels;
    for (const QString& label : labels)
        displayLabels << titleCaseLabel(label);
    table->setHorizontalHeaderLabels(displayLabels);
    table->setVerticalHeaderLabels(displayLabels);
    for (int row = 0; row < labels.size(); ++row) {
        for (int column = 0; column < labels.size(); ++column) {
            auto* item = new QTableWidgetItem("--");
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(row, column, item);
        }
    }

    const CsvTable csv = readCsvTable(confusionPath);
    if (csv.headers.isEmpty())
        return;
    for (int row = 0; row < labels.size() && row < csv.rows.size(); ++row) {
        const QStringList csvRow = csv.rows.at(row);
        for (int column = 0; column < labels.size(); ++column) {
            const QString value = csvValue(csv, csvRow, "pred_" + labels.at(column));
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

void populateFailureGrid(QWidget* grid, const QString& failureCasesPath) {
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
        const QString trueLabel = titleCaseLabel(csvValue(csv, row, "true_label"));
        const QString predLabel = titleCaseLabel(csvValue(csv, row, "pred_label"));

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

    auto validatorImagePanel = makePanel("Validation", "Run a trained model on saved images");
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
    const ValidatorMetricWidget hitMetric = makeValidatorMetric("Hit results");
    const ValidatorMetricWidget wasteMetric = makeValidatorMetric("Waste results");
    const ValidatorMetricWidget reviewMetric = makeValidatorMetric("Review summary");
    validatorMetricsRow->addWidget(checkedMetric.frame);
    validatorMetricsRow->addWidget(hitMetric.frame);
    validatorMetricsRow->addWidget(wasteMetric.frame);
    validatorMetricsRow->addWidget(reviewMetric.frame);
    validatorReportBody->addLayout(validatorMetricsRow);

    auto validatorSummaryDiagnostic = new QLabel("Choose a model and validation images, then run validation.");
    validatorSummaryDiagnostic->setProperty("mutedText", true);
    validatorSummaryDiagnostic->setWordWrap(true);
    nameWidget(validatorSummaryDiagnostic, "ValidatorWorkspaceSummaryDiagnosticLabel");
    validatorReportBody->addWidget(validatorSummaryDiagnostic);

    auto validatorConfusionLabel = new QLabel("Hit/Waste results");
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
    validatorRightScroll->setFixedWidth(360);
    validatorRightScroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
            setMetricValue(checkedMetric, "--");
            setMetricValue(hitMetric, "--");
            setMetricValue(wasteMetric, "--");
            setMetricValue(reviewMetric, "Needs review");
            validatorSummaryDiagnostic->setText("The latest validation report could not be read: " + parseDiagnostic);
            validatorConfusionLabel->setText("Hit/Waste results");
            populateConfusionTable(validatorConfusion, {}, QString());
            populateFailureGrid(validatorSampleGrid, QString());
            return;
        }

        if (summary.isEmpty()) {
            setMetricValue(checkedMetric, "--");
            setMetricValue(hitMetric, "--");
            setMetricValue(wasteMetric, "--");
            setMetricValue(reviewMetric, "--");
            validatorSummaryDiagnostic->setText("Choose a model and validation images, then run validation.");
            validatorConfusionLabel->setText("Hit/Waste results");
            populateConfusionTable(validatorConfusion, {}, QString());
            populateFailureGrid(validatorSampleGrid, QString());
            return;
        }

        const QJsonObject dataset = summary.value("dataset").toObject();
        const QJsonObject metrics = summary.value("metrics").toObject();
        const QJsonObject artifacts = summary.value("artifacts").toObject();
        QStringList labels;
        for (const QJsonValue& labelValue : summary.value("labels").toArray())
            labels << labelValue.toString();

        const int samplesTotal = dataset.value("samples_total").toInt();
        const int samplesEvaluated = dataset.value("samples_evaluated").toInt();
        const int samplesFailed = dataset.value("samples_failed").toInt();
        const int samplesIncorrect = metrics.value("samples_incorrect").toInt();
        const QJsonObject classCounts = dataset.value("class_counts").toObject();
        const bool hasHitCounts = classCounts.contains("hit");
        const bool hasWasteCounts = classCounts.contains("waste");
        const QString reviewSummary = reviewSummaryText(samplesEvaluated, samplesIncorrect, samplesFailed);
        const QString accuracyText = metrics.contains("accuracy")
                                         ? QString::number(metrics.value("accuracy").toDouble() * 100.0, 'f', 1) + "%"
                                         : QString();

        setMetricValue(checkedMetric, QString("%1 / %2").arg(samplesEvaluated).arg(samplesTotal),
                       samplesFailed > 0 ? QString("%1 failed").arg(samplesFailed) : QString());
        setMetricValue(hitMetric, hasHitCounts ? QString::number(classCounts.value("hit").toInt()) : QString(),
                       hasHitCounts ? "images in set" : QString());
        setMetricValue(wasteMetric, hasWasteCounts ? QString::number(classCounts.value("waste").toInt()) : QString(),
                       hasWasteCounts ? "images in set" : QString());
        setMetricValue(reviewMetric, reviewSummary, accuracyText.isEmpty() ? QString() : "accuracy " + accuracyText);

        QStringList summaryLines;
        summaryLines << QString("Latest run: %1.").arg(titleCaseLabel(summary.value("status").toString("unknown")));
        summaryLines << QString("Checked %1 of %2 images.").arg(samplesEvaluated).arg(samplesTotal);
        if (samplesIncorrect > 0 || samplesFailed > 0) {
            summaryLines << QString("%1 images need review and %2 failed to evaluate.")
                                .arg(samplesIncorrect)
                                .arg(samplesFailed);
        } else {
            summaryLines << "No misclassified samples were reported in the latest run.";
        }
        validatorSummaryDiagnostic->setText(summaryLines.join(" "));

        validatorConfusionLabel->setText(labels.contains("hit", Qt::CaseInsensitive) && labels.contains("waste", Qt::CaseInsensitive)
                                             ? "Hit/Waste results"
                                             : "Results by label");
        populateConfusionTable(validatorConfusion, labels, artifacts.value("confusion_matrix_csv").toString());
        populateFailureGrid(validatorSampleGrid, artifacts.value("failure_cases_csv").toString());
    };

    validatorImageWidget->setSummaryChangedCallback(refreshReport);
    refreshReport(validationSummaryPath());

    return validatorWorkspacePage;
}

} // namespace desktop_app::workspace
