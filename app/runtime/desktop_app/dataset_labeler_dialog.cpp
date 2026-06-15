#include "dataset_labeler_dialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImageReader>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

#include "app_utils.h"
#include "object_names.h"

DatasetLabelerDialog::DatasetLabelerDialog(QWidget* parent, const QString& initialPath) : QDialog(parent) {
    setWindowTitle("Dataset Builder Review");
    resize(1280, 780);
    setMinimumSize(980, 620);
    nameWidget(this, "datasetLabelerWorkspace");

    auto* openFolderButton = new QPushButton("Open Dataset Folder");
    auto* openManifestButton = new QPushButton("Open Manifest");
    hitButton = new QPushButton("Hit");
    wasteButton = new QPushButton("Waste");
    excludeButton = new QPushButton("Exclude");
    acceptButton = new QPushButton("Accept Auto-label");
    undoButton = new QPushButton("Undo");
    saveButton = new QPushButton("Save Review");
    excludeReasonCombo = new QComboBox;
    excludeReasonCombo->addItems(
        {"edge_case", "artifact", "ambiguous", "partial_droplet", "bad_crop", "not_for_training", "other"});
    notesEdit = new QPlainTextEdit;
    notesEdit->setPlaceholderText("Review notes");
    notesEdit->setMaximumHeight(74);
    for (auto* button : {hitButton, wasteButton, excludeButton, acceptButton, undoButton, saveButton}) {
        button->setEnabled(false);
    }
    excludeReasonCombo->setEnabled(false);
    notesEdit->setEnabled(false);

    pathLabel = new QLabel("No dataset selected.");
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    bannerLabel = new QLabel("Open a Dataset Builder manifest to review crops. Auto-labels remain suggestions until a "
                             "reviewed label is saved.");
    bannerLabel->setWordWrap(true);
    bannerLabel->setStyleSheet("color:#6b4f00;");
    loadStatusLabel = new QLabel("Dataset Builder review load status: no manifest loaded");
    loadStatusLabel->setWordWrap(true);
    loadStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    loadStatusEdit = new QLineEdit("Dataset Builder review load status: no manifest loaded");
    loadStatusEdit->setReadOnly(true);
    loadStatusEdit->setFrame(false);
    loadStatusEdit->setFocusPolicy(Qt::NoFocus);
    filterCombo = new QComboBox;
    filterCombo->addItems(
        {"All crops", "Unreviewed", "Reviewed", "Excluded", "Auto hit", "Auto waste", "Low confidence", "Warnings"});
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Filter by image id, path, auto-label, reviewed label, state, or warning");
    prevButton = new QPushButton("Previous");
    nextButton = new QPushButton("Next");
    prevButton->setEnabled(false);
    nextButton->setEnabled(false);
    browserTable = new QTableWidget(0, 7);
    browserTable->setHorizontalHeaderLabels({"Image ID", "Crop", "Auto", "Reviewed", "State", "Eligible", "Warnings"});
    browserTable->horizontalHeader()->setStretchLastSection(true);
    browserTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    browserTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    browserTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    classBalanceTable = new QTableWidget(0, 6);
    classBalanceTable->setHorizontalHeaderLabels(
        {"Label", "Reviewed", "Trainer Eligible", "Auto", "Warning", "Policy"});
    classBalanceTable->horizontalHeader()->setStretchLastSection(true);
    classBalanceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewLabel = new QLabel("Select a dataset to inspect manifest entries and available summary artifacts.");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(320, 220);
    previewLabel->setStyleSheet("background:#111;color:#ddd;border:1px solid #555;");
    previewDetailsLabel = new QLabel("No crop selected.");
    previewDetailsLabel->setWordWrap(true);
    previewDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    outputText = new QPlainTextEdit;
    outputText->setReadOnly(true);
    outputText->setPlainText("Accepted inputs: Dataset Builder metadata/dataset_manifest.json, older dataset "
                             "manifests, metadata/labels.csv, or metadata/crops.csv.");

    nameWidget(openFolderButton, "datasetLabelerOpenDatasetAction");
    nameWidget(openManifestButton, "datasetLabelerOpenManifestButton");
    nameWidget(pathLabel, "datasetLabelerDatasetPathLabel");
    nameWidget(filterCombo, "datasetLabelerCropFilterCombo");
    nameWidget(searchEdit, "datasetLabelerCropSearchEdit");
    nameWidget(prevButton, "datasetLabelerPreviousCropButton");
    nameWidget(nextButton, "datasetLabelerNextCropButton");
    nameWidget(browserTable, "datasetLabelerCropBrowser");
    nameWidget(previewLabel, "datasetLabelerCropPreview");
    nameWidget(previewDetailsLabel, "datasetLabelerCropDetailsLabel");
    nameWidget(hitButton, "datasetBuilderReviewHitButton");
    nameWidget(wasteButton, "datasetBuilderReviewWasteButton");
    nameWidget(excludeButton, "datasetBuilderReviewExcludeButton");
    nameWidget(acceptButton, "datasetBuilderReviewAcceptAutoButton");
    nameWidget(undoButton, "datasetLabelerUndoButton");
    nameWidget(saveButton, "datasetBuilderSaveReviewButton");
    nameWidget(excludeReasonCombo, "datasetBuilderExcludeReasonCombo");
    nameWidget(notesEdit, "datasetBuilderReviewNotesEdit");
    nameWidget(classBalanceTable, "datasetLabelerClassBalanceTable");
    nameWidget(bannerLabel, "datasetLabelerReadinessBanner");
    nameWidget(loadStatusLabel, "DatasetBuilderReviewLoadStatusLabel");
    nameObject(loadStatusEdit, "DatasetBuilderReviewLoadStatusText");
    nameWidget(outputText, "datasetLabelerBackendOutputText");

    auto* topButtons = new QHBoxLayout;
    topButtons->addWidget(openFolderButton);
    topButtons->addWidget(openManifestButton);
    topButtons->addStretch(1);

    auto* actionsLayout = new QGridLayout;
    actionsLayout->addWidget(hitButton, 0, 0);
    actionsLayout->addWidget(wasteButton, 0, 1);
    actionsLayout->addWidget(excludeButton, 0, 2);
    actionsLayout->addWidget(acceptButton, 1, 0, 1, 3);
    actionsLayout->addWidget(new QLabel("Exclude reason"), 2, 0);
    actionsLayout->addWidget(excludeReasonCombo, 2, 1, 1, 2);
    actionsLayout->addWidget(notesEdit, 3, 0, 1, 3);
    actionsLayout->addWidget(undoButton, 4, 0);
    actionsLayout->addWidget(saveButton, 4, 1, 1, 2);
    auto* actionsGroup = new QGroupBox("Manual Review");
    actionsGroup->setLayout(actionsLayout);

    auto* leftLayout = new QVBoxLayout;
    leftLayout->addLayout(topButtons);
    leftLayout->addWidget(pathLabel);
    auto* filterLayout = new QGridLayout;
    filterLayout->addWidget(new QLabel("View"), 0, 0);
    filterLayout->addWidget(filterCombo, 0, 1);
    filterLayout->addWidget(new QLabel("Search"), 1, 0);
    filterLayout->addWidget(searchEdit, 1, 1);
    leftLayout->addLayout(filterLayout);
    leftLayout->addWidget(browserTable, 1);
    auto* navLayout = new QHBoxLayout;
    navLayout->addWidget(prevButton);
    navLayout->addWidget(nextButton);
    leftLayout->addLayout(navLayout);

    auto* rightLayout = new QVBoxLayout;
    rightLayout->addWidget(previewLabel, 1);
    rightLayout->addWidget(previewDetailsLabel);
    rightLayout->addWidget(actionsGroup);
    rightLayout->addWidget(new QLabel("Class Balance"));
    rightLayout->addWidget(classBalanceTable, 1);

    auto* splitter = new QSplitter;
    auto* leftWidget = new QWidget;
    leftWidget->setLayout(leftLayout);
    auto* rightWidget = new QWidget;
    rightWidget->setLayout(rightLayout);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    auto* layout = new QVBoxLayout;
    layout->addWidget(bannerLabel);
    layout->addWidget(loadStatusLabel);
    layout->addWidget(loadStatusEdit);
    layout->addWidget(splitter, 1);
    layout->addWidget(new QLabel("Inspection Output"));
    layout->addWidget(outputText, 1);
    setLayout(layout);

    QObject::connect(openFolderButton, &QPushButton::clicked, [this]() {
        const QString selected = QFileDialog::getExistingDirectory(this, "Select dataset folder", currentDatasetPath);
        if (!selected.isEmpty())
            loadDatasetPath(selected);
    });
    QObject::connect(openManifestButton, &QPushButton::clicked, [this]() {
        const QString selected = QFileDialog::getOpenFileName(this, "Select dataset manifest", currentDatasetPath,
                                                              "JSON manifest (*.json);;All files (*.*)");
        if (!selected.isEmpty())
            loadDatasetPath(selected);
    });
    QObject::connect(filterCombo, &QComboBox::currentTextChanged, [this]() { applyBrowserFilter(); });
    QObject::connect(searchEdit, &QLineEdit::textChanged, [this]() { applyBrowserFilter(); });
    QObject::connect(prevButton, &QPushButton::clicked, [this]() { selectRelativeRow(-1); });
    QObject::connect(nextButton, &QPushButton::clicked, [this]() { selectRelativeRow(1); });
    QObject::connect(browserTable, &QTableWidget::itemSelectionChanged, [this]() { updatePreviewFromSelection(); });
    QObject::connect(hitButton, &QPushButton::clicked, [this]() { applyReviewLabel("hit", true); });
    QObject::connect(wasteButton, &QPushButton::clicked, [this]() { applyReviewLabel("waste", true); });
    QObject::connect(excludeButton, &QPushButton::clicked, [this]() { applyReviewLabel("exclude", true); });
    QObject::connect(acceptButton, &QPushButton::clicked, [this]() { acceptAutoLabel(); });
    QObject::connect(saveButton, &QPushButton::clicked, [this]() { saveManifestAndLabels(); });
    QObject::connect(undoButton, &QPushButton::clicked, [this]() { undoLastReviewEdit(); });
    auto* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    auto* hitShortcut = new QShortcut(QKeySequence(Qt::Key_H), this);
    auto* wasteShortcut = new QShortcut(QKeySequence(Qt::Key_W), this);
    auto* excludeShortcut = new QShortcut(QKeySequence(Qt::Key_E), this);
    auto* acceptShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    auto* saveShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    QObject::connect(prevShortcut, &QShortcut::activated, [this]() { selectRelativeRow(-1); });
    QObject::connect(nextShortcut, &QShortcut::activated, [this]() { selectRelativeRow(1); });
    QObject::connect(hitShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabel("hit", true);
    });
    QObject::connect(wasteShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabel("waste", true);
    });
    QObject::connect(excludeShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabel("exclude", true);
    });
    QObject::connect(acceptShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            acceptAutoLabel();
    });
    QObject::connect(undoShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            undoLastReviewEdit();
    });
    QObject::connect(saveShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            saveManifestAndLabels();
    });

    if (!initialPath.isEmpty())
        loadDatasetPath(initialPath);
}

void DatasetLabelerDialog::loadDatasetPath(const QString& selectedPath) {
    currentDatasetPath = QFileInfo(selectedPath).absoluteFilePath();
    browserRows.clear();
    undoStack.clear();
    manifestDoc = QJsonDocument();
    manifestPath.clear();
    isBuilderManifest = false;
    browserTable->setRowCount(0);
    classBalanceTable->setRowCount(0);
    previewLabel->setPixmap(QPixmap());
    previewLabel->setText("No crop selected.");
    previewDetailsLabel->setText("No crop selected.");
    setLoadStatusText("Dataset Builder review load status: loading " + QDir::toNativeSeparators(currentDatasetPath));

    QFileInfo info(currentDatasetPath);
    const bool isManifest = info.isFile();
    datasetRoot = isManifest ? info.dir().absolutePath() : info.absoluteFilePath();
    if (isManifest && info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
        QDir root(info.dir());
        root.cdUp();
        datasetRoot = root.absolutePath();
    }
    pathLabel->setText("Dataset: " + QDir::toNativeSeparators(datasetRoot));

    QStringList report;
    report << "Load: " + currentDatasetPath;
    bool loaded = false;
    if (isManifest) {
        loaded = loadManifest(info.absoluteFilePath(), report);
    } else {
        const QString manifest = QDir(datasetRoot).filePath("metadata/dataset_manifest.json");
        if (QFileInfo::exists(manifest))
            loaded = loadManifest(manifest, report);
        else
            report << "No dataset_manifest.json found.";
    }
    loadSummaryArtifacts(report);
    if (!loaded)
        loadCropsCsv(report);
    applyBrowserFilter();
    updateReviewControls();
    if (browserRows.isEmpty())
        report << "No crop/item rows were available for browsing.";
    else
        report << QString("Browser rows available: %1; visible after filter: %2")
                      .arg(browserRows.size())
                      .arg(browserTable->rowCount());
    updateLoadStatus();
    outputText->setPlainText(report.join("\n"));
}

bool DatasetLabelerDialog::loadManifest(const QString& manifestPath, QStringList& report) {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report << "Manifest unreadable: " + manifestPath;
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        report << QString("Manifest parse failed at offset %1: %2").arg(err.offset).arg(err.errorString());
        return false;
    }
    QJsonObject root = doc.object();
    manifestDoc = doc;
    this->manifestPath = QFileInfo(manifestPath).absoluteFilePath();
    isBuilderManifest = isDatasetBuilderManifest(root);
    report << "Manifest: " + manifestPath;
    report << "Dataset id: " + root.value("dataset_id").toString("--");
    report << (isBuilderManifest ? "Mode: Dataset Builder review; reviewed_label is editable."
                                 : "Mode: read-only legacy dataset inspection.");

    if (isBuilderManifest) {
        loadBuilderManifest(root, report);
        return true;
    }

    QMap<QString, QString> displayByClass;
    QJsonArray classes = root.value("classes").toArray();
    if (classes.isEmpty())
        classes = root.value("schema").toObject().value("classes").toArray();
    for (const QJsonValue& value : classes) {
        QJsonObject cls = value.toObject();
        const QString id = cls.value("id").toVariant().toString();
        if (!id.isEmpty())
            displayByClass[id] = cls.value("display_name").toString(id);
    }
    QMap<QString, int> includedCounts;
    QMap<QString, int> statusCounts;
    int seedCount = 0;
    int demoCount = 0;
    int unknownRedistribution = 0;
    const QJsonArray items = root.value("items").toArray();
    const int maxRows = std::min(static_cast<int>(items.size()), 500);
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject item = items.at(i).toObject();
        const QString path = item.value("path").toString(item.value("source_relative_path").toString());
        const QString label = item.value("label").toVariant().toString();
        const QString status = item.value("status").toString("--");
        const QJsonObject provenance = item.value("provenance").toObject();
        const QString origin = provenance.value("origin").toString(provenance.value("source").toString("--"));
        const bool seed = provenance.value("seed_image").toBool(false);
        const bool demo = provenance.value("demo_image").toBool(false);
        const QString redistribution = provenance.value("redistribution_status").toString();
        statusCounts[status]++;
        if (status == "included" && !label.isEmpty())
            includedCounts[label]++;
        if (seed)
            seedCount++;
        if (demo)
            demoCount++;
        if (redistribution == "unknown")
            unknownRedistribution++;
        if (i < maxRows)
            addLegacyBrowserRow(path, label, status, origin, seed ? "yes" : (demo ? "demo" : "no"));
    }
    report << QString("Manifest items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
    if (!statusCounts.isEmpty()) {
        QStringList parts;
        for (auto it = statusCounts.begin(); it != statusCounts.end(); ++it)
            parts << QString("%1=%2").arg(it.key()).arg(it.value());
        report << "Statuses: " + parts.join(", ");
    }
    if (seedCount || demoCount || unknownRedistribution) {
        report << QString("Provenance: seed=%1, demo=%2, unknown redistribution=%3")
                      .arg(seedCount)
                      .arg(demoCount)
                      .arg(unknownRedistribution);
    }
    populateCountsFromMap(includedCounts, displayByClass);
    return true;
}

bool DatasetLabelerDialog::isDatasetBuilderManifest(const QJsonObject& root) const {
    if (root.value("schema_version").toString() == "dataset-builder-manifest-v1")
        return true;
    const QJsonArray items = root.value("items").toArray();
    if (items.isEmpty())
        return false;
    const QJsonObject first = items.first().toObject();
    return first.contains("crop_path") || first.contains("auto_label") || first.contains("reviewed_label") ||
           first.contains("trainer_eligible");
}

void DatasetLabelerDialog::loadBuilderManifest(const QJsonObject& root, QStringList& report) {
    QMap<QString, int> autoCounts;
    QMap<QString, int> reviewedCounts;
    QMap<QString, int> eligibleCounts;
    int lowConfidenceCount = 0;
    const QJsonArray items = root.value("items").toArray();
    const int maxRows = std::min(static_cast<int>(items.size()), 1000);
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject item = items.at(i).toObject();
        const QString autoLabel = normalizedLabel(item.value("auto_label").toString("unknown"));
        const QString reviewedLabel = normalizedLabel(item.value("reviewed_label").toString());
        const QString reviewState = item.value("review_state").toString("unreviewed");
        const bool eligible = item.value("trainer_eligible").toBool(false);
        const double confidence = item.value("auto_label_confidence").toDouble(-1.0);
        if (confidence >= 0.0 && confidence < 0.80) {
            lowConfidenceCount++;
        }
        autoCounts[autoLabel]++;
        if (reviewState == "unreviewed")
            reviewedCounts["unreviewed"]++;
        else
            reviewedCounts[reviewedLabel.isEmpty() ? "--" : reviewedLabel]++;
        if (eligible)
            eligibleCounts[reviewedLabel]++;
        if (i < maxRows) {
            browserRows.push_back(browserRowFromBuilderItem(i, item));
        }
    }
    report << QString("Builder manifest items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
    report << QString("Trainer eligible reviewed hit=%1 waste=%2; exclude=%3; unreviewed=%4")
                  .arg(eligibleCounts.value("hit"))
                  .arg(eligibleCounts.value("waste"))
                  .arg(reviewedCounts.value("exclude"))
                  .arg(reviewedCounts.value("unreviewed"));
    if (lowConfidenceCount)
        report << QString("Low-confidence auto-label suggestions: %1").arg(lowConfidenceCount);
    populateBuilderBalanceTable(autoCounts, reviewedCounts, eligibleCounts, items.size());
    updateBannerFromBuilderCounts(reviewedCounts, eligibleCounts, items.size());
}

void DatasetLabelerDialog::loadSummaryArtifacts(QStringList& report) {
    QDir metadataDir(QDir(datasetRoot).filePath("metadata"));
    const QString summaryPath = metadataDir.filePath("dataset_summary.json");
    if (QFileInfo::exists(summaryPath)) {
        QFile file(summaryPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject root = doc.object();
                report << "Summary artifact: " + summaryPath;
                report << QString("Samples included=%1 excluded=%2")
                              .arg(root.value("samples_included").toVariant().toString(),
                                   root.value("samples_excluded").toVariant().toString());
                QJsonArray warnings = root.value("warnings").toArray();
                for (const QJsonValue& warning : warnings)
                    report << "Warning: " + warning.toString();
            }
        }
    }
    const QString balancePath = metadataDir.filePath("class_balance.csv");
    if (QFileInfo::exists(balancePath)) {
        report << "Class balance artifact: " + balancePath;
        loadClassBalanceCsv(balancePath);
    }
}

void DatasetLabelerDialog::loadClassBalanceCsv(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream ts(&file);
    ts.readLine();
    classBalanceTable->setRowCount(0);
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (line.trimmed().isEmpty())
            continue;
        const QStringList cells = line.split(',');
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        for (int col = 0; col < 6; ++col) {
            classBalanceTable->setItem(row, col,
                                       new QTableWidgetItem(col < cells.size() ? cells.at(col).trimmed() : QString()));
        }
    }
}

bool DatasetLabelerDialog::loadCropsCsv(QStringList& report) {
    QString csvPath = QDir(datasetRoot).filePath("metadata/labels.csv");
    if (!QFileInfo::exists(csvPath))
        csvPath = QDir(datasetRoot).filePath("metadata/crops.csv");
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream ts(&file);
    const QStringList columns = ts.readLine().split(',');
    const int pathCol = std::max(columns.indexOf("path"), columns.indexOf("crop_path"));
    int labelCol = columns.indexOf("reviewed_label");
    if (labelCol < 0)
        labelCol = columns.indexOf("label");
    int statusCol = columns.indexOf("review_state");
    if (statusCol < 0)
        statusCol = columns.indexOf("status");
    const int autoCol = columns.indexOf("auto_label");
    int count = 0;
    while (!ts.atEnd() && count < 500) {
        const QStringList cells = ts.readLine().split(',');
        auto cell = [&](int index) {
            return (index >= 0 && index < cells.size()) ? cells.at(index).trimmed() : QString("--");
        };
        browserRows.push_back({-1, cell(columns.indexOf("image_id")), cell(pathCol), cell(autoCol), "--",
                               cell(labelCol), cell(statusCol), "no", "--", "--", "--", "--", "--"});
        count++;
    }
    report << QString("Loaded CSV rows for browsing: %1").arg(count);
    return count > 0;
}

void DatasetLabelerDialog::populateCountsFromMap(const QMap<QString, int>& counts,
                                                 const QMap<QString, QString>& displayByClass) {
    if (counts.isEmpty() || classBalanceTable->rowCount() > 0)
        return;
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        classBalanceTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(displayByClass.value(it.key(), it.key())));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(it.value())));
        for (int col = 3; col < 6; ++col)
            classBalanceTable->setItem(row, col, new QTableWidgetItem("--"));
    }
}

void DatasetLabelerDialog::populateBuilderBalanceTable(const QMap<QString, int>& autoCounts,
                                                       const QMap<QString, int>& reviewedCounts,
                                                       const QMap<QString, int>& eligibleCounts, int totalItems) {
    classBalanceTable->setRowCount(0);
    const QStringList labels = {"hit", "waste", "exclude", "unreviewed"};
    for (const QString& label : labels) {
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        const bool trainingLabel = label == "hit" || label == "waste";
        const int reviewed = reviewedCounts.value(label);
        const int eligible = eligibleCounts.value(label);
        QString warning;
        if (trainingLabel && eligible == 0)
            warning = "blocks handoff";
        if (label == "exclude" && totalItems > 0 && reviewed * 4 > totalItems)
            warning = ">25% excluded";
        classBalanceTable->setItem(row, 0, new QTableWidgetItem(label));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(reviewed)));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(eligible)));
        classBalanceTable->setItem(row, 3, new QTableWidgetItem(QString::number(autoCounts.value(label))));
        classBalanceTable->setItem(row, 4, new QTableWidgetItem(warning));
        classBalanceTable->setItem(row, 5,
                                   new QTableWidgetItem(trainingLabel ? "trainer class" : "not trainer-eligible"));
    }
}

void DatasetLabelerDialog::updateBannerFromBuilderCounts(const QMap<QString, int>& reviewedCounts,
                                                         const QMap<QString, int>& eligibleCounts, int totalItems) {
    const int hit = eligibleCounts.value("hit");
    const int waste = eligibleCounts.value("waste");
    const int exclude = reviewedCounts.value("exclude");
    const int unreviewed = reviewedCounts.value("unreviewed");
    QStringList warnings;
    if (unreviewed > 0)
        warnings << QString("%1 crops still need manual review").arg(unreviewed);
    if (hit == 0 || waste == 0)
        warnings << "trainer handoff blocked until reviewed hit and waste both exist";
    const int minority = std::min(hit, waste);
    const int majority = std::max(hit, waste);
    if (minority > 0 && majority > 3 * minority)
        warnings << QString("class imbalance %1:%2").arg(majority).arg(minority);
    if (totalItems > 0 && exclude * 4 > totalItems)
        warnings << "more than 25% excluded";
    if (warnings.isEmpty())
        bannerLabel->setText("Review status: reviewed hit/waste items are trainer-eligible; exclude and unreviewed "
                             "items are retained but not handed to training.");
    else
        bannerLabel->setText("Review warnings: " + warnings.join("; "));
}

void DatasetLabelerDialog::addLegacyBrowserRow(const QString& path, const QString& label, const QString& status,
                                               const QString& origin, const QString& seed) {
    browserRows.push_back(
        {-1, QFileInfo(path).fileName(), path, "--", origin, label, status, "no", seed, "--", "--", "--", "--"});
}

bool DatasetLabelerDialog::rowMatchesFilter(const BrowserRow& row) const {
    const QString mode = filterCombo ? filterCombo->currentText() : QString("All crops");
    const QString stateLower = row.reviewState.toLower();
    if (mode == "Unreviewed" && stateLower != "unreviewed")
        return false;
    if (mode == "Reviewed" && stateLower == "unreviewed")
        return false;
    if (mode == "Excluded" && row.reviewedLabel != "exclude" && stateLower != "excluded")
        return false;
    if (mode == "Auto hit" && row.autoLabel != "hit")
        return false;
    if (mode == "Auto waste" && row.autoLabel != "waste")
        return false;
    if (mode == "Low confidence" && !row.warnings.contains("low confidence", Qt::CaseInsensitive))
        return false;
    if (mode == "Warnings" && row.warnings.trimmed().isEmpty())
        return false;

    const QString needle = searchEdit ? searchEdit->text().trimmed().toLower() : QString();
    if (needle.isEmpty())
        return true;
    const QString haystack = QStringList{row.imageId,     row.cropPath, row.autoLabel, row.reviewedLabel,
                                         row.reviewState, row.eligible, row.warnings}
                                 .join(" ")
                                 .toLower();
    return haystack.contains(needle);
}

void DatasetLabelerDialog::applyBrowserFilter() {
    const QString previousPath = selectedPath();
    browserTable->setRowCount(0);
    int restoreRow = -1;
    for (const BrowserRow& rowData : browserRows) {
        if (!rowMatchesFilter(rowData))
            continue;
        const int row = browserTable->rowCount();
        browserTable->insertRow(row);
        const QStringList values = {rowData.imageId,     rowData.cropPath, rowData.autoLabel, rowData.reviewedLabel,
                                    rowData.reviewState, rowData.eligible, rowData.warnings};
        for (int col = 0; col < values.size(); ++col)
            browserTable->setItem(row, col, new QTableWidgetItem(values.at(col)));
        browserTable->item(row, 0)->setData(Qt::UserRole, rowData.manifestIndex);
        if (!previousPath.isEmpty() && rowData.cropPath == previousPath)
            restoreRow = row;
    }
    browserTable->resizeColumnsToContents();
    if (restoreRow >= 0) {
        browserTable->selectRow(restoreRow);
    } else if (browserTable->rowCount() > 0) {
        browserTable->selectRow(0);
    } else {
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText("No crop matches the current filter.");
        previewDetailsLabel->setText("No crop selected.");
        updateNavigationButtons();
    }
    updateLoadStatus();
}

void DatasetLabelerDialog::updateLoadStatus() {
    if (manifestPath.isEmpty()) {
        setLoadStatusText("Dataset Builder review load status: no manifest loaded");
        return;
    }
    QString datasetId = "--";
    if (manifestDoc.isObject()) {
        datasetId = manifestDoc.object().value("dataset_id").toString("--");
    }
    setLoadStatusText(QString("Dataset Builder manifest loaded: dataset_id=%1; items=%2; visible=%3; manifest=%4")
                          .arg(datasetId)
                          .arg(browserRows.size())
                          .arg(browserTable ? browserTable->rowCount() : 0)
                          .arg(QDir::toNativeSeparators(manifestPath)));
}

void DatasetLabelerDialog::setLoadStatusText(const QString& text) {
    if (loadStatusLabel)
        loadStatusLabel->setText(text);
    if (loadStatusEdit)
        loadStatusEdit->setText(text);
}

void DatasetLabelerDialog::updatePreviewFromSelection() {
    const QList<QTableWidgetItem*> selected = browserTable->selectedItems();
    if (selected.isEmpty()) {
        updateNavigationButtons();
        return;
    }
    const int row = selected.first()->row();
    const QString relPath = browserTable->item(row, 1) ? browserTable->item(row, 1)->text() : QString();
    const QString imageId = browserTable->item(row, 0) ? browserTable->item(row, 0)->text() : QString("--");
    const QString autoLabel = browserTable->item(row, 2) ? browserTable->item(row, 2)->text() : QString("--");
    const QString reviewedLabel = browserTable->item(row, 3) ? browserTable->item(row, 3)->text() : QString("--");
    const QString state = browserTable->item(row, 4) ? browserTable->item(row, 4)->text() : QString("--");
    const QString eligible = browserTable->item(row, 5) ? browserTable->item(row, 5)->text() : QString("--");
    const QString warnings = browserTable->item(row, 6) ? browserTable->item(row, 6)->text() : QString("--");
    const QString imagePath = QFileInfo(relPath).isAbsolute() ? relPath : QDir(datasetRoot).filePath(relPath);
    const BrowserRow data = rowDataForVisibleRow(row);
    previewDetailsLabel->setText(QString("Image ID: %1\nCrop: %2\nAuto-label: %3 (%4, confidence %5)\nReviewed label: "
                                         "%6\nReview state: %7\nTrainer eligible: %8\nWarnings: %9")
                                     .arg(imageId, QDir::toNativeSeparators(relPath), autoLabel, data.autoSource,
                                          data.confidence, reviewedLabel.isEmpty() ? "--" : reviewedLabel, state,
                                          eligible, warnings.isEmpty() ? "--" : warnings));
    if (notesEdit && !notesEdit->hasFocus())
        notesEdit->setPlainText(data.notes);
    if (excludeReasonCombo && !data.excludeReason.isEmpty())
        setComboTextIfPresent(excludeReasonCombo, data.excludeReason);
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText("Preview unavailable\n" + relPath);
        updateNavigationButtons();
        return;
    }
    previewLabel->setText(QString());
    previewLabel->setPixmap(
        QPixmap::fromImage(img).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    updateNavigationButtons();
    updateReviewControls();
}

QString DatasetLabelerDialog::selectedPath() const {
    const QList<QTableWidgetItem*> selected = browserTable ? browserTable->selectedItems() : QList<QTableWidgetItem*>();
    if (selected.isEmpty())
        return QString();
    const int row = selected.first()->row();
    return browserTable->item(row, 1) ? browserTable->item(row, 1)->text() : QString();
}

void DatasetLabelerDialog::selectRelativeRow(int delta) {
    const int rows = browserTable->rowCount();
    if (rows <= 0)
        return;
    int row = browserTable->currentRow();
    if (row < 0)
        row = 0;
    row = std::clamp(row + delta, 0, rows - 1);
    browserTable->selectRow(row);
    browserTable->scrollToItem(browserTable->item(row, 0), QAbstractItemView::PositionAtCenter);
}

void DatasetLabelerDialog::updateNavigationButtons() {
    const int rows = browserTable ? browserTable->rowCount() : 0;
    const int row = browserTable ? browserTable->currentRow() : -1;
    if (prevButton)
        prevButton->setEnabled(rows > 0 && row > 0);
    if (nextButton)
        nextButton->setEnabled(rows > 0 && row >= 0 && row < rows - 1);
}

bool DatasetLabelerDialog::reviewShortcutAllowed() const {
    QWidget* focus = QApplication::focusWidget();
    if (!focus)
        return true;
    if (qobject_cast<QLineEdit*>(focus))
        return false;
    if (qobject_cast<QPlainTextEdit*>(focus))
        return false;
    if (qobject_cast<QTextEdit*>(focus))
        return false;
    if (qobject_cast<QComboBox*>(focus))
        return false;
    return true;
}

int DatasetLabelerDialog::selectedManifestIndex() const {
    const int row = browserTable ? browserTable->currentRow() : -1;
    if (row < 0 || !browserTable->item(row, 0))
        return -1;
    return browserTable->item(row, 0)->data(Qt::UserRole).toInt();
}

QVector<int> DatasetLabelerDialog::selectedManifestIndexes() const {
    QVector<int> indexes;
    if (!browserTable)
        return indexes;

    QSet<int> seen;
    QList<int> selectedRows;
    if (browserTable->selectionModel()) {
        const QModelIndexList rows = browserTable->selectionModel()->selectedRows(0);
        for (const QModelIndex& rowIndex : rows)
            selectedRows.append(rowIndex.row());
    }
    if (selectedRows.isEmpty() && browserTable->currentRow() >= 0)
        selectedRows.append(browserTable->currentRow());
    std::sort(selectedRows.begin(), selectedRows.end());

    for (const int row : selectedRows) {
        if (row < 0 || !browserTable->item(row, 0))
            continue;
        const int manifestIndex = browserTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (manifestIndex >= 0 && !seen.contains(manifestIndex)) {
            seen.insert(manifestIndex);
            indexes.push_back(manifestIndex);
        }
    }
    return indexes;
}

DatasetLabelerDialog::BrowserRow DatasetLabelerDialog::rowDataForVisibleRow(int visibleRow) const {
    if (visibleRow < 0 || !browserTable || !browserTable->item(visibleRow, 0))
        return {};
    const int manifestIndex = browserTable->item(visibleRow, 0)->data(Qt::UserRole).toInt();
    for (const BrowserRow& row : browserRows) {
        if (row.manifestIndex == manifestIndex && manifestIndex >= 0)
            return row;
    }
    return {};
}

DatasetLabelerDialog::BrowserRow DatasetLabelerDialog::browserRowFromBuilderItem(int manifestIndex,
                                                                                 const QJsonObject& item) const {
    const QString autoLabel = normalizedLabel(item.value("auto_label").toString("unknown"));
    const QString reviewedLabel = normalizedLabel(item.value("reviewed_label").toString());
    const QString reviewState = item.value("review_state").toString("unreviewed");
    const bool eligible = item.value("trainer_eligible").toBool(false);
    const double confidence = item.value("auto_label_confidence").toDouble(-1.0);
    QStringList warnings;
    if (reviewState == "unreviewed")
        warnings << "needs review";
    if (reviewedLabel == "exclude" && item.value("exclude_reason").toString().isEmpty())
        warnings << "missing exclude reason";
    if (eligible && reviewedLabel != "hit" && reviewedLabel != "waste")
        warnings << "eligible label invalid";
    if ((reviewState == "confirmed" || reviewState == "relabeled") && !eligible)
        warnings << "reviewed class not trainer-eligible";
    if (confidence >= 0.0 && confidence < 0.80)
        warnings << "low confidence";

    return {manifestIndex,
            item.value("image_id").toString(QString("item_%1").arg(manifestIndex + 1)),
            item.value("crop_path").toString(item.value("path").toString()),
            autoLabel,
            item.value("auto_label_source").toString("--"),
            reviewedLabel,
            reviewState,
            eligible ? "yes" : "no",
            warnings.join("; "),
            confidence >= 0.0 ? QString::number(confidence, 'f', 3) : QString("--"),
            item.value("source_frame_path").toString(),
            item.value("notes").toString(),
            item.value("exclude_reason").toString()};
}

int DatasetLabelerDialog::visibleRowForManifestIndex(int manifestIndex) const {
    if (!browserTable || manifestIndex < 0)
        return -1;
    for (int row = 0; row < browserTable->rowCount(); ++row) {
        if (browserTable->item(row, 0) && browserTable->item(row, 0)->data(Qt::UserRole).toInt() == manifestIndex)
            return row;
    }
    return -1;
}

void DatasetLabelerDialog::updateVisibleBrowserRow(int visibleRow, const BrowserRow& rowData) {
    if (!browserTable || visibleRow < 0 || visibleRow >= browserTable->rowCount())
        return;
    const QStringList values = {rowData.imageId,     rowData.cropPath, rowData.autoLabel, rowData.reviewedLabel,
                                rowData.reviewState, rowData.eligible, rowData.warnings};
    for (int col = 0; col < values.size(); ++col) {
        if (!browserTable->item(visibleRow, col))
            browserTable->setItem(visibleRow, col, new QTableWidgetItem);
        browserTable->item(visibleRow, col)->setText(values.at(col));
    }
    browserTable->item(visibleRow, 0)->setData(Qt::UserRole, rowData.manifestIndex);
}

void DatasetLabelerDialog::refreshBrowserRowFromManifestItem(int manifestIndex, const QJsonObject& item) {
    const BrowserRow rowData = browserRowFromBuilderItem(manifestIndex, item);
    for (BrowserRow& row : browserRows) {
        if (row.manifestIndex == manifestIndex) {
            row = rowData;
            break;
        }
    }
    updateVisibleBrowserRow(visibleRowForManifestIndex(manifestIndex), rowData);
}

void DatasetLabelerDialog::selectManifestIndexes(const QVector<int>& manifestIndexes) {
    if (!browserTable)
        return;
    browserTable->clearSelection();
    int firstVisibleRow = -1;
    for (const int manifestIndex : manifestIndexes) {
        const int row = visibleRowForManifestIndex(manifestIndex);
        if (row < 0)
            continue;
        if (firstVisibleRow < 0)
            firstVisibleRow = row;
        for (int col = 0; col < browserTable->columnCount(); ++col) {
            if (browserTable->item(row, col))
                browserTable->item(row, col)->setSelected(true);
        }
    }
    if (firstVisibleRow >= 0) {
        browserTable->setCurrentCell(firstVisibleRow, 0);
        browserTable->scrollToItem(browserTable->item(firstVisibleRow, 0), QAbstractItemView::PositionAtCenter);
    }
}

void DatasetLabelerDialog::refreshBuilderReviewSummary() {
    if (!isBuilderManifest || !manifestDoc.isObject())
        return;

    QMap<QString, int> autoCounts;
    QMap<QString, int> reviewedCounts;
    QMap<QString, int> eligibleCounts;
    const QJsonArray items = manifestDoc.object().value("items").toArray();
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QString autoLabel = normalizedLabel(item.value("auto_label").toString("unknown"));
        const QString reviewedLabel = normalizedLabel(item.value("reviewed_label").toString());
        const QString reviewState = item.value("review_state").toString("unreviewed");
        const bool eligible = item.value("trainer_eligible").toBool(false);
        autoCounts[autoLabel]++;
        if (reviewState == "unreviewed")
            reviewedCounts["unreviewed"]++;
        else
            reviewedCounts[reviewedLabel.isEmpty() ? "--" : reviewedLabel]++;
        if (eligible)
            eligibleCounts[reviewedLabel]++;
    }
    populateBuilderBalanceTable(autoCounts, reviewedCounts, eligibleCounts, items.size());
    updateBannerFromBuilderCounts(reviewedCounts, eligibleCounts, items.size());
}

void DatasetLabelerDialog::updateReviewControls() {
    const bool canReview = isBuilderManifest && !selectedManifestIndexes().isEmpty();
    for (auto* button : {hitButton, wasteButton, excludeButton, acceptButton, saveButton}) {
        if (button)
            button->setEnabled(canReview);
    }
    if (undoButton)
        undoButton->setEnabled(canReview && !undoStack.isEmpty());
    if (excludeReasonCombo)
        excludeReasonCombo->setEnabled(canReview);
    if (notesEdit)
        notesEdit->setEnabled(canReview);
    if (!canReview && isBuilderManifest) {
        bannerLabel->setText("Select a crop row to review. Auto-labels are suggestions; only reviewed hit/waste rows "
                             "are trainer-eligible.");
    } else if (!isBuilderManifest && !manifestDoc.isNull()) {
        bannerLabel->setText(
            "Read-only legacy manifest inspection. Open a Dataset Builder manifest to edit reviewed_label.");
    }
}

void DatasetLabelerDialog::acceptAutoLabel() {
    const QVector<int> indexes = selectedManifestIndexes();
    if (indexes.isEmpty())
        return;
    QJsonArray items = manifestDoc.object().value("items").toArray();
    QVector<int> hitWasteIndexes;
    for (const int index : indexes) {
        if (index < 0 || index >= items.size())
            continue;
        const QString autoLabel = normalizedLabel(items.at(index).toObject().value("auto_label").toString());
        if (autoLabel == "hit" || autoLabel == "waste")
            hitWasteIndexes.push_back(index);
    }
    if (hitWasteIndexes.isEmpty())
        return;

    QJsonObject root = manifestDoc.object();
    const QString reviewedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    bool needsFilterRefresh = false;
    for (const int index : hitWasteIndexes) {
        QJsonObject item = items.at(index).toObject();
        undoStack.push_back({index, item});
        const QString autoLabel = normalizedLabel(item.value("auto_label").toString());
        item["reviewed_label"] = autoLabel;
        item["reviewed_at"] = reviewedAt;
        item["review_state"] = "confirmed";
        item["trainer_eligible"] = true;
        item["notes"] = notesEdit ? notesEdit->toPlainText().trimmed() : QString();
        item.remove("exclude_reason");
        items.replace(index, item);
        const BrowserRow rowData = browserRowFromBuilderItem(index, item);
        needsFilterRefresh = needsFilterRefresh || !rowMatchesFilter(rowData);
    }
    root["items"] = items;
    root["updated_at"] = reviewedAt;
    manifestDoc = QJsonDocument(root);
    for (const int index : hitWasteIndexes)
        refreshBrowserRowFromManifestItem(index, items.at(index).toObject());
    refreshBuilderReviewSummary();
    if (needsFilterRefresh)
        applyBrowserFilter();
    else
        selectManifestIndexes(hitWasteIndexes);
    saveManifestAndLabels(false);
    if (hitWasteIndexes.size() == 1)
        selectRelativeRow(1);
}

void DatasetLabelerDialog::applyReviewLabel(const QString& label, bool advance) {
    if (!isBuilderManifest)
        return;
    const QVector<int> indexes = selectedManifestIndexes();
    if (indexes.isEmpty())
        return;
    QJsonObject root = manifestDoc.object();
    QJsonArray items = root.value("items").toArray();
    const QString normalized = normalizedLabel(label);
    const QString reviewedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    bool needsFilterRefresh = false;
    QVector<int> changedIndexes;
    for (const int index : indexes) {
        if (index < 0 || index >= items.size())
            continue;
        QJsonObject item = items.at(index).toObject();
        undoStack.push_back({index, item});
        const QString autoLabel = normalizedLabel(item.value("auto_label").toString());
        item["reviewed_label"] = normalized;
        item["reviewed_at"] = reviewedAt;
        item["review_state"] =
            normalized == "exclude" ? "excluded" : (normalized == autoLabel ? "confirmed" : "relabeled");
        item["trainer_eligible"] = (normalized == "hit" || normalized == "waste");
        item["notes"] = notesEdit ? notesEdit->toPlainText().trimmed() : QString();
        if (normalized == "exclude") {
            item["exclude_reason"] = excludeReasonCombo ? excludeReasonCombo->currentText() : QString("other");
        } else {
            item.remove("exclude_reason");
        }
        items.replace(index, item);
        changedIndexes.push_back(index);
        const BrowserRow rowData = browserRowFromBuilderItem(index, item);
        needsFilterRefresh = needsFilterRefresh || !rowMatchesFilter(rowData);
    }
    if (changedIndexes.isEmpty())
        return;
    root["items"] = items;
    root["updated_at"] = reviewedAt;
    manifestDoc = QJsonDocument(root);
    for (const int index : changedIndexes)
        refreshBrowserRowFromManifestItem(index, items.at(index).toObject());
    refreshBuilderReviewSummary();
    if (needsFilterRefresh)
        applyBrowserFilter();
    else
        selectManifestIndexes(changedIndexes);
    saveManifestAndLabels(false);
    if (advance && changedIndexes.size() == 1)
        selectRelativeRow(1);
}

void DatasetLabelerDialog::undoLastReviewEdit() {
    if (undoStack.isEmpty())
        return;
    ReviewUndo undo = undoStack.takeLast();
    QJsonObject root = manifestDoc.object();
    QJsonArray items = root.value("items").toArray();
    if (undo.manifestIndex < 0 || undo.manifestIndex >= items.size())
        return;
    items.replace(undo.manifestIndex, undo.previousItem);
    root["items"] = items;
    root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifestDoc = QJsonDocument(root);
    rebuildRowsFromCurrentManifest();
    applyBrowserFilter();
    saveManifestAndLabels(false);
}

void DatasetLabelerDialog::rebuildRowsFromCurrentManifest() {
    QStringList report;
    browserRows.clear();
    classBalanceTable->setRowCount(0);
    loadBuilderManifest(manifestDoc.object(), report);
}

bool DatasetLabelerDialog::saveManifestAndLabels(bool showMessage) {
    if (!isBuilderManifest || manifestPath.isEmpty())
        return false;
    QFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (showMessage)
            QMessageBox::warning(this, "Save failed", "Could not write manifest:\n" + manifestPath);
        return false;
    }
    file.write(manifestDoc.toJson(QJsonDocument::Indented));
    file.close();
    writeLabelsCsv();
    if (showMessage)
        outputText->setPlainText("Saved Dataset Builder review manifest and labels.csv:\n" + manifestPath);
    return true;
}

void DatasetLabelerDialog::writeLabelsCsv() {
    QDir metadataDir(QDir(datasetRoot).filePath("metadata"));
    metadataDir.mkpath(".");
    QFile file(metadataDir.filePath("labels.csv"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream ts(&file);
    ts << "image_id,crop_path,source_frame_path,source_frame_id,timestamp,crop_x,crop_y,crop_w,crop_h,collection_mode,"
          "batch_index,auto_label,auto_label_source,auto_label_confidence,auto_label_model_id,review_state,reviewed_"
          "label,exclude_reason,trainer_eligible,hash_sha256\n";
    const QJsonArray items = manifestDoc.object().value("items").toArray();
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QJsonArray rect = item.value("crop_rect").toArray();
        QStringList cols;
        cols << item.value("image_id").toString() << item.value("crop_path").toString()
             << item.value("source_frame_path").toString() << item.value("source_frame_id").toVariant().toString()
             << item.value("timestamp").toString() << (rect.size() > 0 ? rect.at(0).toVariant().toString() : QString())
             << (rect.size() > 1 ? rect.at(1).toVariant().toString() : QString())
             << (rect.size() > 2 ? rect.at(2).toVariant().toString() : QString())
             << (rect.size() > 3 ? rect.at(3).toVariant().toString() : QString())
             << item.value("collection_mode").toString() << item.value("batch_index").toVariant().toString()
             << item.value("auto_label").toString() << item.value("auto_label_source").toString()
             << item.value("auto_label_confidence").toVariant().toString()
             << item.value("auto_label_model_id").toString() << item.value("review_state").toString("unreviewed")
             << item.value("reviewed_label").toString() << item.value("exclude_reason").toString()
             << (item.value("trainer_eligible").toBool(false) ? "true" : "false")
             << item.value("hash_sha256").toString();
        for (int i = 0; i < cols.size(); ++i)
            cols[i] = csvEscape(cols.at(i));
        ts << cols.join(',') << "\n";
    }
}

QString DatasetLabelerDialog::normalizedLabel(const QString& label) const {
    const QString lower = label.trimmed().toLower();
    if (lower == "hits" || lower == "hit" || lower == "1")
        return "hit";
    if (lower == "waste" || lower == "empty" || lower == "0")
        return "waste";
    if (lower == "exclude" || lower == "excluded" || lower == "reject" || lower == "rejected")
        return "exclude";
    if (lower.isEmpty())
        return QString();
    return lower;
}

QString DatasetLabelerDialog::csvEscape(QString text) const {
    const bool quote = text.contains(',') || text.contains('"') || text.contains('\n') || text.contains('\r');
    text.replace("\"", "\"\"");
    return quote ? "\"" + text + "\"" : text;
}
