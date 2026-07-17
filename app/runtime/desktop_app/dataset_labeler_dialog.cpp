#include "dataset_labeler_dialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QColorDialog>
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
#include <QSignalBlocker>
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
#include "model_registry_service.h"
#include "object_names.h"
#include "theme.h"

namespace {

QString canonicalLegacyLabel(const QString& label) {
    const QString lower = label.trimmed().toLower();
    if (lower == "hits" || lower == "hit" || lower == "target" || lower == "1")
        return "1";
    if (lower == "waste" || lower == "empty" || lower == "non-target" || lower == "non_target" || lower == "0")
        return "0";
    if (lower == "2")
        return "2";
    if (lower == "exclude" || lower == "excluded" || lower == "reject" || lower == "rejected")
        return "exclude";
    if (lower == "unreviewed")
        return "unreviewed";
    return lower;
}

QString defaultClassDisplayName(int mode, const QString& id) {
    if (id == "1")
        return "Target";
    if (id == "2")
        return "Non-target B";
    return mode >= 3 ? "Non-target A" : "Non-target";
}

QString defaultClassFolder(const QString& id) {
    return QString("reviewed/class_%1").arg(id);
}

QString classSchemaColorValue(const QJsonObject& cls) {
    const QString displayColor = cls.value("display_color").toString().trimmed();
    if (!displayColor.isEmpty())
        return displayColor;
    return cls.value("color").toString().trimmed();
}

QString filterValueForClassId(const QString& classId) {
    return "class:" + classId;
}

QString preferredDatasetMetadataPath(const QString& path) {
    const QFileInfo info(path.trimmed());
    if (!info.exists()) {
        return path.trimmed();
    }
    if (info.isFile()) {
        return info.absoluteFilePath();
    }
    const QDir dir(info.absoluteFilePath());
    const QString metadataManifest = dir.filePath("metadata/dataset_manifest.json");
    if (QFileInfo::exists(metadataManifest)) {
        return metadataManifest;
    }
    const QString rootManifest = dir.filePath("manifest.json");
    if (QFileInfo::exists(rootManifest)) {
        return rootManifest;
    }
    for (const QString& starterName : {QString("droplet_target_nontarget_binary_starter"),
                                       QString("droplet_target_nontarget_3class_starter")}) {
        const QString starterManifest = dir.filePath(starterName + "/metadata/dataset_manifest.json");
        if (QFileInfo::exists(starterManifest)) {
            return starterManifest;
        }
    }
    return info.absoluteFilePath();
}

} // namespace

DatasetLabelerDialog::DatasetLabelerDialog(QWidget* parent, const QString& initialPath, const QString& defaultPath)
    : QDialog(parent), defaultDatasetRoot(defaultPath) {
    setWindowTitle("Dataset Review");
    resize(1280, 780);
    setMinimumSize(980, 620);
    nameWidget(this, "datasetLabelerWorkspace");
    resetClassSchema(2);

    auto* openManifestButton = new QPushButton("Open Dataset File");
    classModeCombo = new QComboBox;
    classModeCombo->addItem("2 labels", 2);
    classModeCombo->addItem("3 labels", 3);
    classZeroEdit = new QLineEdit;
    classOneEdit = new QLineEdit;
    classTwoEdit = new QLineEdit;
    classTwoLabel = new QLabel("Second non-target label");
    classZeroColorButton = new QPushButton;
    classOneColorButton = new QPushButton;
    classTwoColorButton = new QPushButton;
    classZeroButton = new QPushButton;
    classOneButton = new QPushButton;
    classTwoButton = new QPushButton;
    excludeButton = new QPushButton("Exclude");
    undoButton = new QPushButton("Undo");
    saveButton = new QPushButton("Save Labels");
    notesEdit = new QPlainTextEdit;
    notesEdit->setPlaceholderText("Review notes");
    notesEdit->setMaximumHeight(74);
    for (auto* button : {classZeroButton, classOneButton, classTwoButton, excludeButton, undoButton, saveButton}) {
        button->setEnabled(false);
        button->setMinimumHeight(30);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    }
    notesEdit->setEnabled(false);

    pathLabel = new QLabel("No dataset selected.");
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    bannerLabel = new QLabel("Open a dataset file to review and label images.");
    bannerLabel->setWordWrap(true);
    bannerLabel->setStyleSheet("color:#6b4f00;");
    loadStatusLabel = new QLabel("Dataset review load status: no dataset file loaded");
    loadStatusLabel->setWordWrap(true);
    loadStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    loadStatusEdit = new QLineEdit("Dataset review load status: no dataset file loaded");
    loadStatusEdit->setReadOnly(true);
    loadStatusEdit->setFrame(false);
    loadStatusEdit->setFocusPolicy(Qt::NoFocus);
    filterCombo = new QComboBox;
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Filter by image id, path, label, state, or warning");
    prevButton = new QPushButton("Previous");
    nextButton = new QPushButton("Next");
    prevButton->setEnabled(false);
    nextButton->setEnabled(false);
    for (auto* button : {openManifestButton, prevButton, nextButton}) {
        button->setMinimumHeight(30);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    }
    browserTable = new QTableWidget(0, 6);
    browserTable->setHorizontalHeaderLabels({"Image ID", "Image", "Label", "State", "Ready", "Warnings"});
    browserTable->horizontalHeader()->setStretchLastSection(true);
    browserTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    browserTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    browserTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    classBalanceTable = new QTableWidget(0, 5);
    classBalanceTable->setHorizontalHeaderLabels({"Label", "Reviewed", "Ready", "Warning", "Policy"});
    classBalanceTable->horizontalHeader()->setStretchLastSection(true);
    classBalanceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewLabel = new QLabel("Select an image to review its label, state, and saved details.");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(320, 220);
    previewLabel->setStyleSheet("background:#111;color:#ddd;border:1px solid #555;");
    previewDetailsLabel = new QLabel("No image selected.");
    previewDetailsLabel->setWordWrap(true);
    previewDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    outputText = new QPlainTextEdit;
    outputText->setReadOnly(true);
    outputText->setPlainText("Accepted inputs: a dataset file (.json) or older labels/crops CSV files.");

    nameWidget(openManifestButton, "datasetLabelerOpenManifestButton");
    nameWidget(pathLabel, "datasetLabelerDatasetPathLabel");
    nameWidget(filterCombo, "datasetLabelerCropFilterCombo");
    nameWidget(searchEdit, "datasetLabelerCropSearchEdit");
    nameWidget(prevButton, "datasetLabelerPreviousCropButton");
    nameWidget(nextButton, "datasetLabelerNextCropButton");
    nameWidget(browserTable, "datasetLabelerCropBrowser");
    nameWidget(previewLabel, "datasetLabelerCropPreview");
    nameWidget(previewDetailsLabel, "datasetLabelerCropDetailsLabel");
    nameWidget(classModeCombo, "datasetLabelerClassModeCombo");
    nameWidget(classZeroEdit, "datasetLabelerClassZeroEdit");
    nameWidget(classOneEdit, "datasetLabelerClassOneEdit");
    nameWidget(classTwoEdit, "datasetLabelerClassTwoEdit");
    nameWidget(classZeroColorButton, "datasetLabelerClassZeroColorButton");
    nameWidget(classOneColorButton, "datasetLabelerClassOneColorButton");
    nameWidget(classTwoColorButton, "datasetLabelerClassTwoColorButton");
    nameWidget(classZeroButton, "datasetBuilderReviewClassZeroButton");
    nameWidget(classOneButton, "datasetBuilderReviewClassOneButton");
    nameWidget(classTwoButton, "datasetBuilderReviewClassTwoButton");
    nameWidget(excludeButton, "datasetBuilderReviewExcludeButton");
    nameWidget(undoButton, "datasetLabelerUndoButton");
    nameWidget(saveButton, "datasetBuilderSaveReviewButton");
    nameWidget(notesEdit, "datasetBuilderReviewNotesEdit");
    nameWidget(classBalanceTable, "datasetLabelerClassBalanceTable");
    nameWidget(bannerLabel, "datasetLabelerReadinessBanner");
    nameWidget(loadStatusLabel, "DatasetBuilderReviewLoadStatusLabel");
    nameObject(loadStatusEdit, "DatasetBuilderReviewLoadStatusText");
    nameWidget(outputText, "datasetLabelerBackendOutputText");

    auto* topButtons = new QHBoxLayout;
    topButtons->addWidget(openManifestButton);
    topButtons->addStretch(1);

    for (auto* button : {classZeroColorButton, classOneColorButton, classTwoColorButton}) {
        button->setFixedWidth(34);
        button->setMinimumHeight(28);
        button->setText(QString());
        button->setToolTip("Choose label color");
    }

    auto* classSchemaLayout = new QGridLayout;
    classSchemaLayout->addWidget(new QLabel("Class setup"), 0, 0);
    classSchemaLayout->addWidget(classModeCombo, 0, 1, 1, 2);
    classSchemaLayout->addWidget(new QLabel("Non-target label"), 1, 0);
    classSchemaLayout->addWidget(classZeroEdit, 1, 1);
    classSchemaLayout->addWidget(classZeroColorButton, 1, 2);
    classSchemaLayout->addWidget(new QLabel("Target label"), 2, 0);
    classSchemaLayout->addWidget(classOneEdit, 2, 1);
    classSchemaLayout->addWidget(classOneColorButton, 2, 2);
    classSchemaLayout->addWidget(classTwoLabel, 3, 0);
    classSchemaLayout->addWidget(classTwoEdit, 3, 1);
    classSchemaLayout->addWidget(classTwoColorButton, 3, 2);

    auto* actionsLayout = new QGridLayout;
    actionsLayout->setHorizontalSpacing(8);
    actionsLayout->setVerticalSpacing(8);
    actionsLayout->setColumnStretch(0, 1);
    actionsLayout->setColumnStretch(1, 1);
    actionsLayout->setColumnStretch(2, 1);
    actionsLayout->addWidget(classZeroButton, 0, 0);
    actionsLayout->addWidget(classOneButton, 0, 1);
    actionsLayout->addWidget(classTwoButton, 0, 2);
    actionsLayout->addWidget(excludeButton, 1, 0, 1, 3);
    actionsLayout->addWidget(notesEdit, 2, 0, 1, 3);
    actionsLayout->addWidget(undoButton, 3, 0);
    actionsLayout->addWidget(saveButton, 3, 1, 1, 2);
    auto* actionsGroup = new QGroupBox("Manual Review");
    actionsGroup->setLayout(actionsLayout);

    auto* leftLayout = new QVBoxLayout;
    leftLayout->addLayout(topButtons);
    leftLayout->addWidget(pathLabel);
    leftLayout->addLayout(classSchemaLayout);
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
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    leftWidget->setMinimumWidth(430);
    rightWidget->setMinimumWidth(420);

    auto* layout = new QVBoxLayout;
    layout->addWidget(bannerLabel);
    layout->addWidget(loadStatusLabel);
    layout->addWidget(loadStatusEdit);
    layout->addWidget(splitter, 1);
    layout->addWidget(new QLabel("Inspection Output"));
    layout->addWidget(outputText, 1);
    setLayout(layout);

    QObject::connect(openManifestButton, &QPushButton::clicked, [this]() {
        const QString startPath =
            chooseOpenFileDialogPath(preferredDatasetMetadataPath(currentDatasetPath),
                                     preferredDatasetMetadataPath(defaultDatasetRoot),
                                     findPackagedAppPath("datasets/prepared"));
        const QString selected = QFileDialog::getOpenFileName(
            this, "Select dataset file", startPath, "Dataset files (*.json);;All files (*.*)");
        if (!selected.isEmpty())
            loadDatasetPath(selected);
    });
    QObject::connect(filterCombo, &QComboBox::currentTextChanged, [this]() { applyBrowserFilter(); });
    QObject::connect(searchEdit, &QLineEdit::textChanged, [this]() { applyBrowserFilter(); });
    QObject::connect(prevButton, &QPushButton::clicked, [this]() { selectRelativeRow(-1); });
    QObject::connect(nextButton, &QPushButton::clicked, [this]() { selectRelativeRow(1); });
    QObject::connect(browserTable, &QTableWidget::itemSelectionChanged, [this]() { updatePreviewFromSelection(); });
    QObject::connect(classModeCombo, &QComboBox::currentIndexChanged, [this]() { handleClassModeChanged(); });
    QObject::connect(classZeroEdit, &QLineEdit::editingFinished, [this]() { handleClassLabelEdited(0); });
    QObject::connect(classOneEdit, &QLineEdit::editingFinished, [this]() { handleClassLabelEdited(1); });
    QObject::connect(classTwoEdit, &QLineEdit::editingFinished, [this]() { handleClassLabelEdited(2); });
    QObject::connect(classZeroColorButton, &QPushButton::clicked, [this]() { chooseClassColor(0); });
    QObject::connect(classOneColorButton, &QPushButton::clicked, [this]() { chooseClassColor(1); });
    QObject::connect(classTwoColorButton, &QPushButton::clicked, [this]() { chooseClassColor(2); });
    QObject::connect(classZeroButton, &QPushButton::clicked, [this]() { applyReviewLabelByIndex(0, true); });
    QObject::connect(classOneButton, &QPushButton::clicked, [this]() { applyReviewLabelByIndex(1, true); });
    QObject::connect(classTwoButton, &QPushButton::clicked, [this]() { applyReviewLabelByIndex(2, true); });
    QObject::connect(excludeButton, &QPushButton::clicked, [this]() { applyReviewLabel("exclude", true); });
    QObject::connect(saveButton, &QPushButton::clicked, [this]() { saveManifestAndLabels(); });
    QObject::connect(undoButton, &QPushButton::clicked, [this]() { undoLastReviewEdit(); });
    auto* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    auto* classZeroShortcut = new QShortcut(QKeySequence(Qt::Key_1), this);
    auto* classOneShortcut = new QShortcut(QKeySequence(Qt::Key_2), this);
    auto* classTwoShortcut = new QShortcut(QKeySequence(Qt::Key_3), this);
    auto* excludeShortcut = new QShortcut(QKeySequence(Qt::Key_E), this);
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    auto* saveShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    QObject::connect(prevShortcut, &QShortcut::activated, [this]() { selectRelativeRow(-1); });
    QObject::connect(nextShortcut, &QShortcut::activated, [this]() { selectRelativeRow(1); });
    QObject::connect(classZeroShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabelByIndex(0, true);
    });
    QObject::connect(classOneShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabelByIndex(1, true);
    });
    QObject::connect(classTwoShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabelByIndex(2, true);
    });
    QObject::connect(excludeShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            applyReviewLabel("exclude", true);
    });
    QObject::connect(undoShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            undoLastReviewEdit();
    });
    QObject::connect(saveShortcut, &QShortcut::activated, [this]() {
        if (reviewShortcutAllowed())
            saveManifestAndLabels();
    });
    updateClassModeUi();
    updateFilterChoices();
    updateReviewControls();

    if (!initialPath.isEmpty())
        loadDatasetPath(initialPath);
}

void DatasetLabelerDialog::resetClassSchema(int mode) {
    classEntries.clear();
    classEntries.push_back({"0", defaultClassDisplayName(mode, "0"), defaultClassFolder("0"), defaultClassColorHex("0")});
    classEntries.push_back({"1", defaultClassDisplayName(mode, "1"), defaultClassFolder("1"), defaultClassColorHex("1")});
    if (mode >= 3)
        classEntries.push_back({"2", defaultClassDisplayName(mode, "2"), defaultClassFolder("2"), defaultClassColorHex("2")});
    excludedLabelDisplay = "Exclude";
}

void DatasetLabelerDialog::loadClassSchema(const QJsonObject& root) {
    QJsonObject schema = root.value("class_schema").toObject();
    QJsonArray classes = schema.value("classes").toArray();
    const bool schemaUsesObjects = !classes.isEmpty() && classes.first().isObject();
    if (classes.isEmpty() || !schemaUsesObjects)
        classes = root.value("classes").toArray();
    const int mode = classes.size() >= 3 ? 3 : 2;
    resetClassSchema(mode);
    for (int index = 0; index < classEntries.size() && index < classes.size(); ++index) {
        const QJsonObject cls = classes.at(index).toObject();
        const QString displayName = cls.value("display_name").toString().trimmed();
        if (!displayName.isEmpty())
            classEntries[index].displayName = displayName;
        const QString folder = cls.value("folder").toString().trimmed();
        if (!folder.isEmpty())
            classEntries[index].folder = folder;
        classEntries[index].colorHex =
            normalizedClassColorHex(classSchemaColorValue(cls), classEntries.at(index).colorHex);
    }
    const QJsonObject excluded = schema.value("excluded_label").toObject();
    const QString excludedDisplay = excluded.value("display_name").toString().trimmed();
    if (!excludedDisplay.isEmpty())
        excludedLabelDisplay = excludedDisplay;
}

void DatasetLabelerDialog::storeClassSchema(QJsonObject& root) const {
    QJsonObject schema;
    schema["kind"] = classEntries.size() >= 3 ? "target-nontarget-ternary" : "target-nontarget-binary";
    schema["mode"] = classEntries.size();
    schema["target_class_id"] = "1";
    QJsonArray classes;
    for (int index = 0; index < classEntries.size(); ++index) {
        const ClassEntry& entry = classEntries.at(index);
        QJsonObject cls;
        cls["id"] = entry.id;
        cls["index"] = index;
        cls["display_name"] = entry.displayName;
        cls["folder"] = entry.folder;
        cls["display_color"] = entry.colorHex;
        classes.append(cls);
    }
    QJsonObject excluded;
    excluded["id"] = "exclude";
    excluded["display_name"] = excludedLabelDisplay;
    excluded["folder"] = "reviewed/exclude";
    schema["classes"] = classes;
    schema["excluded_label"] = excluded;
    root["class_schema"] = schema;
    root["classes"] = classes;
}

void DatasetLabelerDialog::updateClassModeUi() {
    if (!classModeCombo || !classZeroEdit || !classOneEdit || !classTwoEdit || !classZeroButton || !classOneButton ||
        !classTwoButton) {
        return;
    }

    const int mode = classEntries.size() >= 3 ? 3 : 2;
    {
        QSignalBlocker blocker(classModeCombo);
        const int index = classModeCombo->findData(mode);
        if (index >= 0)
            classModeCombo->setCurrentIndex(index);
    }
    {
        QSignalBlocker blocker(classZeroEdit);
        classZeroEdit->setText(classEntries.value(0).displayName);
    }
    {
        QSignalBlocker blocker(classOneEdit);
        classOneEdit->setText(classEntries.value(1).displayName);
    }
    {
        QSignalBlocker blocker(classTwoEdit);
        classTwoEdit->setText(mode >= 3 ? classEntries.value(2).displayName : QString());
    }
    classTwoEdit->setVisible(mode >= 3);
    if (classTwoLabel)
        classTwoLabel->setVisible(mode >= 3);
    if (classTwoColorButton)
        classTwoColorButton->setVisible(mode >= 3);
    classZeroButton->setText(classEntries.value(0).displayName);
    classOneButton->setText(classEntries.value(1).displayName);
    classTwoButton->setText(mode >= 3 ? classEntries.value(2).displayName : "Second non-target");
    classTwoButton->setVisible(mode >= 3);
    applyReviewButtonStyles();
    updateColorSelectorButton(classZeroColorButton, 0);
    updateColorSelectorButton(classOneColorButton, 1);
    updateColorSelectorButton(classTwoColorButton, 2);
}

void DatasetLabelerDialog::updateFilterChoices() {
    if (!filterCombo)
        return;
    const QString previous = filterCombo->currentData().toString();
    QSignalBlocker blocker(filterCombo);
    filterCombo->clear();
    filterCombo->addItem("All images", "all");
    filterCombo->addItem("Unreviewed", "unreviewed");
    filterCombo->addItem("Reviewed", "reviewed");
    for (const ClassEntry& entry : classEntries)
        filterCombo->addItem(entry.displayName, filterValueForClassId(entry.id));
    filterCombo->addItem(excludedLabelDisplay, "exclude");
    filterCombo->addItem("Warnings", "warnings");
    const int restoreIndex = filterCombo->findData(previous);
    filterCombo->setCurrentIndex(restoreIndex >= 0 ? restoreIndex : 0);
}

void DatasetLabelerDialog::handleClassModeChanged() {
    if (!classModeCombo)
        return;
    const int mode = classModeCombo->currentData().toInt();
    if (mode == classEntries.size())
        return;
    resetClassSchema(mode);
    updateClassModeUi();
    updateFilterChoices();
    if (manifestDoc.isObject()) {
        QJsonObject root = manifestDoc.object();
        storeClassSchema(root);
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifestDoc = QJsonDocument(root);
        rebuildRowsFromCurrentManifest();
        applyBrowserFilter();
        saveManifestAndLabels(false);
    }
}

void DatasetLabelerDialog::handleClassLabelEdited(int index) {
    if (index < 0 || index >= classEntries.size())
        return;
    QLineEdit* edit = index == 0 ? classZeroEdit : (index == 1 ? classOneEdit : classTwoEdit);
    if (!edit)
        return;
    QString updated = edit->text().trimmed();
    if (updated.isEmpty())
        updated = defaultClassDisplayName(classEntries.size() >= 3 ? 3 : 2, classEntries.at(index).id);
    if (updated == classEntries.at(index).displayName)
        return;
    classEntries[index].displayName = updated;
    updateClassModeUi();
    updateFilterChoices();
    rebuildRowsFromCurrentManifest();
    applyBrowserFilter();
    if (manifestDoc.isObject()) {
        QJsonObject root = manifestDoc.object();
        storeClassSchema(root);
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifestDoc = QJsonDocument(root);
        saveManifestAndLabels(false);
    }
}

void DatasetLabelerDialog::chooseClassColor(int index) {
    if (index < 0 || index >= classEntries.size())
        return;
    const QColor current(classEntries.at(index).colorHex);
    const QColor chosen = QColorDialog::getColor(current, this, "Choose label color");
    if (chosen.isValid())
        setClassColor(index, chosen);
}

void DatasetLabelerDialog::setClassColor(int index, const QColor& color, bool persist) {
    if (index < 0 || index >= classEntries.size() || !color.isValid())
        return;
    const QString normalized = color.name(QColor::HexRgb);
    if (normalized == classEntries.at(index).colorHex)
        return;
    classEntries[index].colorHex = normalized;
    updateClassModeUi();
    if (manifestDoc.isObject()) {
        QJsonObject root = manifestDoc.object();
        storeClassSchema(root);
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifestDoc = QJsonDocument(root);
        if (persist)
            saveManifestAndLabels(false);
    }
    applyBrowserFilter();
    updatePreviewFromSelection();
}

QString DatasetLabelerDialog::defaultClassColorHex(const QString& classId) const {
    return desktop_app::theme::reviewClassColors(classId).fill.name(QColor::HexRgb);
}

QString DatasetLabelerDialog::normalizedClassColorHex(const QString& raw, const QString& fallback) const {
    const QColor color(raw.trimmed());
    if (color.isValid())
        return color.name(QColor::HexRgb);
    const QColor fallbackColor(fallback);
    return fallbackColor.isValid() ? fallbackColor.name(QColor::HexRgb) : defaultClassColorHex("0");
}

QColor DatasetLabelerDialog::classColorForId(const QString& id) const {
    const QString canonical = canonicalLabel(id);
    for (const ClassEntry& entry : classEntries) {
        if (entry.id == canonical) {
            const QColor color(entry.colorHex);
            if (color.isValid())
                return color;
        }
    }
    return desktop_app::theme::reviewClassColors(canonical).fill;
}

QString DatasetLabelerDialog::classColorHexForId(const QString& id) const {
    return classColorForId(id).name(QColor::HexRgb);
}

void DatasetLabelerDialog::applyReviewButtonStyles() {
    const auto mode = useDarkThemeStyles() ? desktop_app::theme::ThemeMode::Dark : desktop_app::theme::ThemeMode::Light;
    auto styleButton = [mode](QPushButton* button, const QColor& color, const QString& classId) {
        if (!button)
            return;
        button->setProperty("reviewClassId", classId);
        button->setProperty("reviewClassColorHex", color.name(QColor::HexRgb));
        button->setStyleSheet(desktop_app::theme::reviewClassButtonStyle(color, mode));
    };

    styleButton(classZeroButton, classColorForId("0"), "0");
    styleButton(classOneButton, classColorForId("1"), "1");
    styleButton(classTwoButton, classColorForId("2"), "2");
    styleButton(excludeButton, desktop_app::theme::reviewClassColors("exclude", mode).fill, "exclude");
}

void DatasetLabelerDialog::updateColorSelectorButton(QPushButton* button, int index) {
    if (!button)
        return;
    const bool visible = index >= 0 && index < classEntries.size();
    button->setVisible(visible);
    if (!visible)
        return;
    const QColor color(classEntries.at(index).colorHex);
    button->setProperty("selectedColorHex", color.name(QColor::HexRgb));
    button->setToolTip(QString("Choose %1 color").arg(classEntries.at(index).displayName));
    button->setStyleSheet(QStringLiteral("QPushButton { background:%1; border:1px solid %2; border-radius:5px; }"
                                         "QPushButton:disabled { background:%3; border:1px solid %4; }")
                              .arg(color.name(QColor::HexRgb),
                                   color.darker(150).name(QColor::HexRgb),
                                   color.name(QColor::HexRgb),
                                   color.darker(150).name(QColor::HexRgb)));
}

bool DatasetLabelerDialog::useDarkThemeStyles() const {
    return palette().color(QPalette::Window).lightness() < 128;
}

QString DatasetLabelerDialog::canonicalLabel(const QString& label) const {
    const QString canonical = canonicalLegacyLabel(label);
    if (canonical.isEmpty() || canonical == "0" || canonical == "1" || canonical == "2" || canonical == "exclude" ||
        canonical == "unreviewed") {
        return canonical;
    }
    for (const ClassEntry& entry : classEntries) {
        if (canonical == entry.id || canonical == entry.displayName.trimmed().toLower())
            return entry.id;
    }
    if (canonical == excludedLabelDisplay.trimmed().toLower())
        return "exclude";
    return canonical;
}

QString DatasetLabelerDialog::displayNameForLabel(const QString& label) const {
    const QString canonical = canonicalLabel(label);
    if (canonical.isEmpty() || canonical == "unreviewed")
        return "Unreviewed";
    if (canonical == "exclude")
        return excludedLabelDisplay;
    for (const ClassEntry& entry : classEntries) {
        if (entry.id == canonical)
            return entry.displayName;
    }
    return canonical;
}

bool DatasetLabelerDialog::isTrainerClassId(const QString& label) const {
    const QString canonical = canonicalLabel(label);
    for (const ClassEntry& entry : classEntries) {
        if (entry.id == canonical)
            return true;
    }
    return false;
}

void DatasetLabelerDialog::loadDatasetPath(const QString& selectedPath) {
    currentDatasetPath = QFileInfo(selectedPath).absoluteFilePath();
    browserRows.clear();
    undoStack.clear();
    manifestDoc = QJsonDocument();
    manifestPath.clear();
    isBuilderManifest = false;
    resetClassSchema(2);
    updateClassModeUi();
    updateFilterChoices();
    browserTable->setRowCount(0);
    classBalanceTable->setRowCount(0);
    previewLabel->setPixmap(QPixmap());
    previewLabel->setText("No image selected.");
    previewDetailsLabel->setText("No image selected.");
    setLoadStatusText("Dataset review load status: loading " + QDir::toNativeSeparators(currentDatasetPath));

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
            report << "No dataset file was found.";
    }
    loadSummaryArtifacts(report);
    if (!loaded)
        loadCropsCsv(report);
    applyBrowserFilter();
    updateReviewControls();
    if (browserRows.isEmpty())
        report << "No images were available for review.";
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
        report << "Dataset file could not be opened: " + manifestPath;
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        report << QString("Dataset file is not valid JSON (offset %1): %2").arg(err.offset).arg(err.errorString());
        return false;
    }
    QJsonObject root = doc.object();
    manifestDoc = doc;
    this->manifestPath = QFileInfo(manifestPath).absoluteFilePath();
    isBuilderManifest = isDatasetBuilderManifest(root);
    loadClassSchema(root);
    updateClassModeUi();
    updateFilterChoices();
    report << "Dataset file: " + manifestPath;
    report << "Dataset id: " + root.value("dataset_id").toString("--");
    report << (isBuilderManifest ? "Mode: image-set review; reviewed labels are editable."
                                 : "Mode: read-only legacy image-set inspection.");

    if (isBuilderManifest) {
        loadBuilderManifest(root, report);
        return true;
    }

    QMap<QString, QString> displayByClass;
    for (const ClassEntry& entry : classEntries)
        displayByClass[entry.id] = entry.displayName;
    displayByClass["exclude"] = excludedLabelDisplay;
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
        const QString label = canonicalLabel(item.value("label").toVariant().toString());
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
    report << QString("Image list items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
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
    const QJsonArray items = root.value("items").toArray();
    const int maxRows = std::min(static_cast<int>(items.size()), 1000);
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject item = items.at(i).toObject();
        const QString autoLabel = canonicalLabel(item.value("auto_label").toString("unknown"));
        const QString reviewedLabel = canonicalLabel(item.value("reviewed_label").toString());
        const QString reviewState = item.value("review_state").toString("unreviewed");
        const bool eligible = item.value("trainer_eligible").toBool(false);
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
    report << QString("Image list items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
    QStringList eligibleSummary;
    for (const ClassEntry& entry : classEntries)
        eligibleSummary << QString("%1=%2").arg(entry.displayName).arg(eligibleCounts.value(entry.id));
    eligibleSummary << QString("%1=%2").arg(excludedLabelDisplay).arg(reviewedCounts.value("exclude"))
                    << QString("Unreviewed=%1").arg(reviewedCounts.value("unreviewed"));
    report << "Ready counts: " + eligibleSummary.join(", ");
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
        for (int col = 0; col < 5; ++col) {
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
        browserRows.push_back({-1,
                               cell(columns.indexOf("image_id")),
                               cell(pathCol),
                               cell(autoCol),
                               "--",
                               canonicalLabel(cell(labelCol)),
                               cell(statusCol),
                               "no",
                               "--",
                               "--",
                               "--",
                               "--"});
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
        classBalanceTable->setItem(row, 0, new QTableWidgetItem(displayByClass.value(it.key(), it.key())));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(it.value())));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(it.value())));
        for (int col = 3; col < 5; ++col)
            classBalanceTable->setItem(row, col, new QTableWidgetItem("--"));
    }
}

void DatasetLabelerDialog::populateBuilderBalanceTable(const QMap<QString, int>& autoCounts,
                                                       const QMap<QString, int>& reviewedCounts,
                                                       const QMap<QString, int>& eligibleCounts, int totalItems) {
    classBalanceTable->setRowCount(0);
    for (const ClassEntry& entry : classEntries) {
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        const int reviewed = reviewedCounts.value(entry.id);
        const int eligible = eligibleCounts.value(entry.id);
        QString warning;
        if (eligible == 0)
            warning = "no labeled items";
        classBalanceTable->setItem(row, 0, new QTableWidgetItem(entry.displayName));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(reviewed)));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(eligible)));
        classBalanceTable->setItem(row, 3, new QTableWidgetItem(warning));
        classBalanceTable->setItem(row, 4, new QTableWidgetItem(QString("class %1").arg(entry.id)));
    }

    {
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        const int reviewed = reviewedCounts.value("exclude");
        QString warning;
        if (totalItems > 0 && reviewed * 4 > totalItems)
            warning = ">25% excluded";
        classBalanceTable->setItem(row, 0, new QTableWidgetItem(excludedLabelDisplay));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(reviewed)));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem("0"));
        classBalanceTable->setItem(row, 3, new QTableWidgetItem(warning));
        classBalanceTable->setItem(row, 4, new QTableWidgetItem("excluded"));
    }

    {
        const int row = classBalanceTable->rowCount();
        classBalanceTable->insertRow(row);
        classBalanceTable->setItem(row, 0, new QTableWidgetItem("Unreviewed"));
        classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(reviewedCounts.value("unreviewed"))));
        classBalanceTable->setItem(row, 2, new QTableWidgetItem("0"));
        classBalanceTable->setItem(row, 3, new QTableWidgetItem("needs review"));
        classBalanceTable->setItem(row, 4, new QTableWidgetItem("review queue"));
    }
}

void DatasetLabelerDialog::updateBannerFromBuilderCounts(const QMap<QString, int>& reviewedCounts,
                                                         const QMap<QString, int>& eligibleCounts, int totalItems) {
    const int exclude = reviewedCounts.value("exclude");
    const int unreviewed = reviewedCounts.value("unreviewed");
    QStringList warnings;
    if (unreviewed > 0)
        warnings << QString("%1 images still need review").arg(unreviewed);
    for (const ClassEntry& entry : classEntries) {
        if (eligibleCounts.value(entry.id) == 0)
            warnings << QString("%1 has no labeled images").arg(entry.displayName);
    }
    if (totalItems > 0 && exclude * 4 > totalItems)
        warnings << "more than 25% excluded";
    if (warnings.isEmpty())
        bannerLabel->setText("Review status: labeled classes are ready; excluded and unreviewed images stay out of "
                             "training exports.");
    else
        bannerLabel->setText("Review warnings: " + warnings.join("; "));
}

void DatasetLabelerDialog::addLegacyBrowserRow(const QString& path, const QString& label, const QString& status,
                                               const QString& origin, const QString& seed) {
    browserRows.push_back({-1, QFileInfo(path).fileName(), path, "--", origin, label, status, "no", seed, "--", "--",
                           "--"});
}

bool DatasetLabelerDialog::rowMatchesFilter(const BrowserRow& row) const {
    const QString mode = filterCombo ? filterCombo->currentData().toString() : QString("all");
    const QString stateLower = row.reviewState.toLower();
    if (mode == "unreviewed" && stateLower != "unreviewed")
        return false;
    if (mode == "reviewed" && stateLower == "unreviewed")
        return false;
    if (mode == "exclude" && canonicalLabel(row.reviewedLabel) != "exclude" && stateLower != "excluded")
        return false;
    if (mode.startsWith("class:") && canonicalLabel(row.reviewedLabel) != mode.mid(QString("class:").size()))
        return false;
    if (mode == "warnings" && row.warnings.trimmed().isEmpty())
        return false;

    const QString needle = searchEdit ? searchEdit->text().trimmed().toLower() : QString();
    if (needle.isEmpty())
        return true;
    const QString haystack = QStringList{row.imageId,
                                         row.cropPath,
                                         displayNameForLabel(row.reviewedLabel),
                                         row.reviewedLabel,
                                         row.reviewState,
                                         row.eligible,
                                         row.warnings}
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
        const QStringList values = {rowData.imageId,
                                    rowData.cropPath,
                                    displayNameForLabel(rowData.reviewedLabel),
                                    rowData.reviewState,
                                    rowData.eligible,
                                    rowData.warnings};
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
        previewLabel->setText("No images match the current filter.");
        previewDetailsLabel->setText("No image selected.");
        updateNavigationButtons();
    }
    updateLoadStatus();
}

void DatasetLabelerDialog::updateLoadStatus() {
    if (manifestPath.isEmpty()) {
        setLoadStatusText("Dataset review load status: no dataset file loaded");
        return;
    }
    QString datasetId = "--";
    if (manifestDoc.isObject()) {
        datasetId = manifestDoc.object().value("dataset_id").toString("--");
    }
    setLoadStatusText(QString("Dataset file loaded: dataset_id=%1; items=%2; visible=%3; file=%4")
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
    const QString reviewedLabel = browserTable->item(row, 2) ? browserTable->item(row, 2)->text() : QString("--");
    const QString state = browserTable->item(row, 3) ? browserTable->item(row, 3)->text() : QString("--");
    const QString eligible = browserTable->item(row, 4) ? browserTable->item(row, 4)->text() : QString("--");
    const QString warnings = browserTable->item(row, 5) ? browserTable->item(row, 5)->text() : QString("--");
    const QString imagePath = QFileInfo(relPath).isAbsolute() ? relPath : QDir(datasetRoot).filePath(relPath);
    const BrowserRow data = rowDataForVisibleRow(row);
    previewDetailsLabel->setText(QString("Image ID: %1\nImage: %2\nAssigned label: %3\nReview state: %4\nReady: %5\n"
                                         "Warnings: %6")
                                     .arg(imageId,
                                          QDir::toNativeSeparators(relPath),
                                          reviewedLabel.isEmpty() ? "Unreviewed" : reviewedLabel,
                                          state,
                                          eligible,
                                          warnings.isEmpty() ? "--" : warnings));
    if (notesEdit && !notesEdit->hasFocus())
        notesEdit->setPlainText(data.notes);
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
    const QString autoLabel = canonicalLabel(item.value("auto_label").toString("unknown"));
    const QString reviewedLabel = canonicalLabel(item.value("reviewed_label").toString());
    const QString reviewState = item.value("review_state").toString("unreviewed");
    const bool eligible = item.value("trainer_eligible").toBool(false) && isTrainerClassId(reviewedLabel);
    const QString displayReviewed = displayNameForLabel(reviewedLabel);
    QStringList warnings;
    if (reviewState == "unreviewed")
        warnings << "needs review";
    if (eligible && !isTrainerClassId(reviewedLabel))
        warnings << "eligible label invalid";
    if ((reviewState == "confirmed" || reviewState == "relabeled") && reviewedLabel != "exclude" && !eligible)
        warnings << "reviewed class not active in this mode";

    return {manifestIndex,
            item.value("image_id").toString(QString("item_%1").arg(manifestIndex + 1)),
            item.value("crop_path").toString(item.value("path").toString()),
            autoLabel,
            item.value("auto_label_source").toString("--"),
            displayReviewed == "Unreviewed" ? QString() : reviewedLabel,
            reviewState,
            eligible ? "yes" : "no",
            warnings.join("; "),
            QString("--"),
            item.value("source_frame_path").toString(),
            item.value("notes").toString()};
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
    const QStringList values = {rowData.imageId,
                                rowData.cropPath,
                                displayNameForLabel(rowData.reviewedLabel),
                                rowData.reviewState,
                                rowData.eligible,
                                rowData.warnings};
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
        const QString autoLabel = canonicalLabel(item.value("auto_label").toString("unknown"));
        const QString reviewedLabel = canonicalLabel(item.value("reviewed_label").toString());
        const QString reviewState = item.value("review_state").toString("unreviewed");
        const bool eligible = item.value("trainer_eligible").toBool(false) && isTrainerClassId(reviewedLabel);
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
    const bool canEditSchema = isBuilderManifest;
    if (classModeCombo)
        classModeCombo->setEnabled(canEditSchema);
    if (classZeroEdit)
        classZeroEdit->setEnabled(canEditSchema);
    if (classOneEdit)
        classOneEdit->setEnabled(canEditSchema);
    if (classTwoEdit)
        classTwoEdit->setEnabled(canEditSchema && classEntries.size() >= 3);
    if (classZeroColorButton)
        classZeroColorButton->setEnabled(canEditSchema);
    if (classOneColorButton)
        classOneColorButton->setEnabled(canEditSchema);
    if (classTwoColorButton)
        classTwoColorButton->setEnabled(canEditSchema && classEntries.size() >= 3);
    for (auto* button : {classZeroButton, classOneButton, classTwoButton, excludeButton, saveButton}) {
        if (button)
            button->setEnabled(canReview);
    }
    if (classTwoButton)
        classTwoButton->setEnabled(canReview && classEntries.size() >= 3);
    if (undoButton)
        undoButton->setEnabled(canReview && !undoStack.isEmpty());
    if (notesEdit)
        notesEdit->setEnabled(canReview);
    if (!canReview && isBuilderManifest) {
        bannerLabel->setText("Select an image row to assign a class label.");
    } else if (!isBuilderManifest && !manifestDoc.isNull()) {
        bannerLabel->setText("Read-only legacy dataset review. Open a dataset file to edit labels.");
    }
}

void DatasetLabelerDialog::applyReviewLabelByIndex(int index, bool advance) {
    if (index < 0 || index >= classEntries.size())
        return;
    applyReviewLabel(classEntries.at(index).id, advance);
}

void DatasetLabelerDialog::applyReviewLabel(const QString& label, bool advance) {
    if (!isBuilderManifest)
        return;
    const QVector<int> indexes = selectedManifestIndexes();
    if (indexes.isEmpty())
        return;
    QJsonObject root = manifestDoc.object();
    QJsonArray items = root.value("items").toArray();
    const QString normalized = canonicalLabel(label);
    const QString reviewedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    bool needsFilterRefresh = false;
    QVector<int> changedIndexes;
    for (const int index : indexes) {
        if (index < 0 || index >= items.size())
            continue;
        QJsonObject item = items.at(index).toObject();
        undoStack.push_back({index, item});
        const QString autoLabel = canonicalLabel(item.value("auto_label").toString());
        item["reviewed_label"] = normalized;
        item["reviewed_at"] = reviewedAt;
        item["review_state"] =
            normalized == "exclude" ? "excluded" : (normalized == autoLabel ? "confirmed" : "relabeled");
        item["trainer_eligible"] = isTrainerClassId(normalized);
        item["notes"] = notesEdit ? notesEdit->toPlainText().trimmed() : QString();
        if (normalized == "exclude") {
            const QString existingExcludeReason = item.value("exclude_reason").toString().trimmed();
            item["exclude_reason"] = existingExcludeReason.isEmpty() ? QString("other") : existingExcludeReason;
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
    storeClassSchema(root);
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
    storeClassSchema(root);
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
    if (manifestDoc.isObject()) {
        QJsonObject root = manifestDoc.object();
        storeClassSchema(root);
        manifestDoc = QJsonDocument(root);
    }
    QFile file(manifestPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (showMessage)
            QMessageBox::warning(this, "Save failed", "Could not save the dataset file:\n" + manifestPath);
        return false;
    }
    file.write(manifestDoc.toJson(QJsonDocument::Indented));
    file.close();
    writeLabelsCsv();
    if (showMessage)
        outputText->setPlainText("Saved dataset file and labels.csv:\n" + manifestPath);
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
             << (isTrainerClassId(item.value("reviewed_label").toString()) ? "true" : "false")
             << item.value("hash_sha256").toString();
        for (int i = 0; i < cols.size(); ++i)
            cols[i] = csvEscape(cols.at(i));
        ts << cols.join(',') << "\n";
    }
}

QString DatasetLabelerDialog::csvEscape(QString text) const {
    const bool quote = text.contains(',') || text.contains('"') || text.contains('\n') || text.contains('\r');
    text.replace("\"", "\"\"");
    return quote ? "\"" + text + "\"" : text;
}
