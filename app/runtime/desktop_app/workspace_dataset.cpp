#include "workspace_dataset.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <algorithm>
#include <memory>

#include "model_registry_service.h"
#include "object_names.h" 
#include "theme.h"
#include "widget_helpers.h" 

namespace desktop_app::workspace {
namespace {

constexpr int kSourceIndexRole = Qt::UserRole + 1;
constexpr int kIconSize = 86;
constexpr int kDefaultPageSize = 100;
constexpr int kLoadBatchSize = 250;
constexpr int kVisiblePageButtonCount = 5;
constexpr int kManifestAutosaveDebounceMs = 750;
constexpr int kTileLabelMaxChars = 14;
constexpr int kTileDisplayTextRole = Qt::UserRole + 2;

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
    if (lower == "unknown")
        return "unknown";
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

QString defaultClassColorHex(const QString& classId) {
    return desktop_app::theme::reviewClassColors(classId).fill.name(QColor::HexRgb);
}

QString normalizedClassColorHex(const QString& raw, const QString& fallback) {
    const QColor color(raw.trimmed());
    if (color.isValid())
        return color.name(QColor::HexRgb);
    const QColor fallbackColor(fallback);
    return fallbackColor.isValid() ? fallbackColor.name(QColor::HexRgb) : defaultClassColorHex("0");
}

QString classSchemaColorValue(const QJsonObject& cls) {
    const QString displayColor = cls.value("display_color").toString().trimmed();
    if (!displayColor.isEmpty())
        return displayColor;
    return cls.value("color").toString().trimmed();
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

QStringList imageNameFilters() {
    return {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff", "*.webp"};
}

QString hiddenExcludeReason(const QString& existingReason) {
    const QString trimmed = existingReason.trimmed();
    return trimmed.isEmpty() ? QString("other") : trimmed;
}

QString firstNonEmptyString(const QJsonObject& item, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const QString value = item.value(QString::fromLatin1(key)).toVariant().toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

QFrame* makeDatasetMetric(const QString& label, const QString& value, const QString& sub = QString()) {
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

QPushButton* makeDatasetFilterRow(const QString& objectName) {
    auto* row = new QPushButton;
    row->setCheckable(true);
    row->setMinimumHeight(34);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setProperty("datasetFilterRow", true);
    nameWidget(row, objectName.toLatin1().constData());
    return row;
}

QWidget* makeDatasetKeyValue(const QString& key, QLabel* value) {
    auto* wrapper = new QWidget;
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    auto* keyLabel = new QLabel(key);
    keyLabel->setProperty("metricLabel", true);
    value->setWordWrap(true);
    value->setProperty("mutedText", true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(keyLabel);
    layout->addWidget(value);
    wrapper->setLayout(layout);
    return wrapper;
}

QSize scaledToFit(const QSize& sourceSize, const QSize& bounds) {
    if (!sourceSize.isValid() || sourceSize.isEmpty() || !bounds.isValid() || bounds.isEmpty())
        return {};
    QSize scaled = sourceSize;
    scaled.scale(bounds, Qt::KeepAspectRatio);
    return scaled;
}

class DatasetWorkspaceWidget final : public QWidget {
  public:
    explicit DatasetWorkspaceWidget(const DatasetWorkspaceControls& controls) : controls_(controls) {
        nameWidget(this, "DatasetWorkspace");
        thumbnailCache_.setMaxCost(64 * 1024);
        setupManifestAutosaveTimer();
        buildUi();
        wireUi();
        updateAll();
        maybeRunVerifier();
    }

    ~DatasetWorkspaceWidget() override { flushPendingManifestSave(); }

  protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        updatePreview();
    }

  private:
    struct CropItem {
        int manifestIndex = -1;
        QString imageId;
        QString cropPath;
        QString autoLabel;
        QString manualLabel;
        QString reviewState = "unreviewed";
        QString confidence;
        QString frameNumber;
        QString timestamp;
        QString excludeReason;
        QJsonObject json;
    };

    struct UndoEntry {
        int manifestIndex = -1;
        QJsonObject previousItem;
    };

    struct ClassEntry {
        QString id;
        QString displayName;
        QString folder;
        QString colorHex;
    };

    enum class FilterMode { All, Class0, Class1, Class2, Excluded, Unreviewed };

    struct PendingLoadState {
        enum class Mode { None, Manifest, FolderScan };

        Mode mode = Mode::None;
        QString manifestPath;
        QJsonDocument manifestDoc;
        QJsonObject manifestRoot;
        QJsonArray rows;
        bool legacyManifestFallback = false;
        int nextIndex = 0;
        QString folderPath;
        std::unique_ptr<QDirIterator> scanIterator;
        QJsonArray scannedManifestItems;
    };

    void resetClassSchema(int mode) {
        classEntries_.clear();
        classEntries_.push_back(
            {"0", defaultClassDisplayName(mode, "0"), defaultClassFolder("0"), defaultClassColorHex("0")});
        classEntries_.push_back(
            {"1", defaultClassDisplayName(mode, "1"), defaultClassFolder("1"), defaultClassColorHex("1")});
        if (mode >= 3)
            classEntries_.push_back(
                {"2", defaultClassDisplayName(mode, "2"), defaultClassFolder("2"), defaultClassColorHex("2")});
        excludedLabelDisplay_ = "Exclude";
    }

    QString canonicalLabel(const QString& label) const {
        const QString canonical = canonicalLegacyLabel(label);
        if (canonical.isEmpty() || canonical == "0" || canonical == "1" || canonical == "2" ||
            canonical == "exclude" || canonical == "unknown" || canonical == "unreviewed") {
            return canonical;
        }
        for (const ClassEntry& entry : classEntries_) {
            if (canonical == entry.id || canonical == entry.displayName.trimmed().toLower())
                return entry.id;
        }
        if (canonical == excludedLabelDisplay_.trimmed().toLower())
            return "exclude";
        return canonical;
    }

    QString displayLabel(const QString& label) const {
        const QString canonical = canonicalLabel(label);
        if (canonical.isEmpty() || canonical == "unreviewed")
            return "Unreviewed";
        if (canonical == "exclude")
            return excludedLabelDisplay_;
        if (canonical == "unknown")
            return "Unknown";
        for (const ClassEntry& entry : classEntries_) {
            if (entry.id == canonical)
                return entry.displayName;
        }
        return canonical;
    }

    bool isTrainerClassId(const QString& label) const {
        const QString canonical = canonicalLabel(label);
        for (const ClassEntry& entry : classEntries_) {
            if (entry.id == canonical)
                return true;
        }
        return false;
    }

    bool isDatasetBuilderManifest(const QJsonObject& root) const {
        const QString schemaVersion = root.value("schema_version").toString().trimmed();
        if (!schemaVersion.isEmpty())
            return schemaVersion == "dataset-builder-manifest-v1";
        const QJsonArray items = root.value("items").toArray();
        if (items.isEmpty())
            return false;
        const QJsonObject first = items.first().toObject();
        return first.contains("crop_path") || first.contains("auto_label") || first.contains("reviewed_label") ||
               first.contains("review_state") || first.contains("trainer_eligible");
    }

    bool usesLegacyManifestFallback(const QJsonObject& root) const {
        return root.value("schema_version").toString().trimmed() == "dataset-manifest-v1";
    }

    QString legacyReviewLabel(const QJsonObject& item) const {
        return canonicalLabel(firstNonEmptyString(item, {"label", "class_id"}));
    }

    QString legacyStatus(const QJsonObject& item) const { return item.value("status").toString().trimmed().toLower(); }

    QString derivedLegacyReviewState(const QJsonObject& item, const QString& manualLabel) const {
        const QString status = legacyStatus(item);
        if (status == "rejected" || status == "excluded")
            return "excluded";
        if (status == "included") {
            if (manualLabel == "exclude")
                return "excluded";
            if (isTrainerClassId(manualLabel))
                return "confirmed";
            return "unreviewed";
        }
        return {};
    }

    void loadClassSchema(const QJsonObject& root) {
        QJsonObject schema = root.value("class_schema").toObject();
        QJsonArray classes = schema.value("classes").toArray();
        const bool schemaUsesObjects = !classes.isEmpty() && classes.first().isObject();
        if (classes.isEmpty() || !schemaUsesObjects)
            classes = root.value("classes").toArray();
        const int mode = classes.size() >= 3 ? 3 : 2;
        resetClassSchema(mode);
        for (int index = 0; index < classEntries_.size() && index < classes.size(); ++index) {
            const QJsonObject cls = classes.at(index).toObject();
            const QString displayName = cls.value("display_name").toString().trimmed();
            if (!displayName.isEmpty())
                classEntries_[index].displayName = displayName;
            const QString folder = cls.value("folder").toString().trimmed();
            if (!folder.isEmpty())
                classEntries_[index].folder = folder;
            classEntries_[index].colorHex =
                normalizedClassColorHex(classSchemaColorValue(cls), classEntries_.at(index).colorHex);
        }
        const QJsonObject excluded = schema.value("excluded_label").toObject();
        const QString excludedDisplay = excluded.value("display_name").toString().trimmed();
        if (!excludedDisplay.isEmpty())
            excludedLabelDisplay_ = excludedDisplay;
    }

    void storeClassSchema(QJsonObject& root) const {
        QJsonObject schema;
        schema["kind"] = classEntries_.size() >= 3 ? "target-nontarget-ternary" : "target-nontarget-binary";
        schema["mode"] = classEntries_.size();
        schema["target_class_id"] = "1";
        QJsonArray classes;
        for (int index = 0; index < classEntries_.size(); ++index) {
            const ClassEntry& entry = classEntries_.at(index);
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
        excluded["display_name"] = excludedLabelDisplay_;
        excluded["folder"] = "reviewed/exclude";
        schema["classes"] = classes;
        schema["excluded_label"] = excluded;
        root["class_schema"] = schema;
        root["classes"] = classes;
    }

    void updateClassSchemaUi() {
        const int mode = classEntries_.size() >= 3 ? 3 : 2;
        if (classModeCombo_) {
            QSignalBlocker blocker(classModeCombo_);
            const int index = classModeCombo_->findData(mode);
            if (index >= 0)
                classModeCombo_->setCurrentIndex(index);
        }
        if (classZeroEdit_) {
            QSignalBlocker blocker(classZeroEdit_);
            classZeroEdit_->setText(classEntries_.value(0).displayName);
        }
        if (classOneEdit_) {
            QSignalBlocker blocker(classOneEdit_);
            classOneEdit_->setText(classEntries_.value(1).displayName);
        }
        if (classTwoEdit_) {
            QSignalBlocker blocker(classTwoEdit_);
            classTwoEdit_->setText(mode >= 3 ? classEntries_.value(2).displayName : QString());
            classTwoEdit_->setVisible(mode >= 3);
        }
        if (classTwoLabel_)
            classTwoLabel_->setVisible(mode >= 3);
        if (classTwoColorButton_)
            classTwoColorButton_->setVisible(mode >= 3);
        if (hitButton_)
            hitButton_->setText(classEntries_.value(0).displayName);
        if (wasteButton_)
            wasteButton_->setText(classEntries_.value(1).displayName);
        if (classThreeButton_) {
            classThreeButton_->setText(mode >= 3 ? classEntries_.value(2).displayName : "Second non-target");
            classThreeButton_->setVisible(mode >= 3);
        }
        applyClassButtonStyles();
        updateColorSelectorButton(classZeroColorButton_, 0);
        updateColorSelectorButton(classOneColorButton_, 1);
        updateColorSelectorButton(classTwoColorButton_, 2);
    }

    void handleClassModeChanged() {
        const int mode = classModeCombo_ ? classModeCombo_->currentData().toInt() : 2;
        if (mode == classEntries_.size())
            return;
        resetClassSchema(mode);
        updateClassSchemaUi();
        if (manifestDoc_.isObject()) {
            QJsonObject root = manifestDoc_.object();
            storeClassSchema(root);
            root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            manifestDoc_ = QJsonDocument(root);
            autosaveManifest();
        }
        applyFilters();
    }

    void handleClassLabelEdited(int index) {
        if (index < 0 || index >= classEntries_.size())
            return;
        QLineEdit* edit = index == 0 ? classZeroEdit_ : (index == 1 ? classOneEdit_ : classTwoEdit_);
        if (!edit)
            return;
        QString updated = edit->text().trimmed();
        if (updated.isEmpty())
            updated = defaultClassDisplayName(classEntries_.size() >= 3 ? 3 : 2, classEntries_.at(index).id);
        if (updated == classEntries_.at(index).displayName)
            return;
        classEntries_[index].displayName = updated;
        updateClassSchemaUi();
        if (manifestDoc_.isObject()) {
            QJsonObject root = manifestDoc_.object();
            storeClassSchema(root);
            root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            manifestDoc_ = QJsonDocument(root);
            autosaveManifest();
        }
        applyFilters();
    }

    desktop_app::theme::ThemeMode currentThemeMode() const {
        return palette().color(QPalette::Window).lightness() < 128 ? desktop_app::theme::ThemeMode::Dark
                                                                   : desktop_app::theme::ThemeMode::Light;
    }

    QString classColorHexForId(const QString& id) const {
        const QString canonical = canonicalLabel(id);
        for (const ClassEntry& entry : classEntries_) {
            if (entry.id == canonical)
                return normalizedClassColorHex(entry.colorHex, defaultClassColorHex(canonical));
        }
        return desktop_app::theme::reviewClassColors(canonical).fill.name(QColor::HexRgb);
    }

    QColor classColorForId(const QString& id) const { return QColor(classColorHexForId(id)); }

    QColor reviewBaseColor(const QString& label) const {
        const QString canonical = canonicalLabel(label);
        if (canonical == "0" || canonical == "1" || canonical == "2")
            return classColorForId(canonical);
        return desktop_app::theme::reviewClassColors(canonical, currentThemeMode()).fill;
    }

    void applyClassButtonStyles() {
        const auto mode = currentThemeMode();
        auto styleButton = [mode](QPushButton* button, const QColor& color, const QString& classId) {
            if (!button)
                return;
            button->setProperty("reviewClassId", classId);
            button->setProperty("reviewClassColorHex", color.name(QColor::HexRgb));
            button->setStyleSheet(desktop_app::theme::reviewClassButtonStyle(color, mode));
        };

        styleButton(hitButton_, classColorForId("0"), "0");
        styleButton(wasteButton_, classColorForId("1"), "1");
        styleButton(classThreeButton_, classColorForId("2"), "2");
        styleButton(excludeButton_, desktop_app::theme::reviewClassColors("exclude", mode).fill, "exclude");
    }

    void updateColorSelectorButton(QPushButton* button, int index) {
        if (!button)
            return;
        const bool visible = index >= 0 && index < classEntries_.size();
        button->setVisible(visible);
        if (!visible)
            return;
        const QColor color(classEntries_.at(index).colorHex);
        button->setProperty("selectedColorHex", color.name(QColor::HexRgb));
        button->setToolTip(QString("Choose %1 color").arg(classEntries_.at(index).displayName));
        button->setStyleSheet(QStringLiteral("QPushButton { background:%1; border:1px solid %2; border-radius:5px; }"
                                             "QPushButton:disabled { background:%3; border:1px solid %4; }")
                                  .arg(color.name(QColor::HexRgb),
                                       color.darker(150).name(QColor::HexRgb),
                                       color.name(QColor::HexRgb),
                                       color.darker(150).name(QColor::HexRgb)));
    }

    void setClassColor(int index, const QColor& color, bool persist = true) {
        if (index < 0 || index >= classEntries_.size() || !color.isValid())
            return;
        const QString normalized = color.name(QColor::HexRgb);
        if (normalized == classEntries_.at(index).colorHex)
            return;
        classEntries_[index].colorHex = normalized;
        updateClassSchemaUi();
        if (manifestDoc_.isObject()) {
            QJsonObject root = manifestDoc_.object();
            storeClassSchema(root);
            root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            manifestDoc_ = QJsonDocument(root);
            if (persist)
                autosaveManifest();
        }
        applyFilters();
        updatePreview();
    }

    void chooseClassColor(int index) {
        if (index < 0 || index >= classEntries_.size())
            return;
        const QColor current(classEntries_.at(index).colorHex);
        const QColor chosen = QColorDialog::getColor(current, this, "Choose label color");
        if (chosen.isValid())
            setClassColor(index, chosen);
    }

    void buildUi() {
        using desktop_app::ui::makePanel;
        using desktop_app::ui::makePanelBody;

        auto* root = new QHBoxLayout;
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(0);

        auto* leftPanel = makePanel("Image Set", "Image set file and class setup");
        leftPanel->setMinimumWidth(280);
        leftPanel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        auto* leftBody = makePanelBody(leftPanel);

        manifestPathEdit_ = new QLineEdit;
        manifestPathEdit_->setReadOnly(true);
        manifestPathEdit_->setPlaceholderText("Choose image set file (.json)...");
        manifestPathEdit_->setMinimumWidth(0); 
        nameWidget(manifestPathEdit_, "DatasetWorkspaceManifestPathEdit"); 
        auto* browseButton = new QPushButton("Browse"); 
        browseButton->setMaximumWidth(78); 
        nameWidget(browseButton, "DatasetWorkspaceManifestBrowseButton"); 
        auto* browseMenu = new QMenu(browseButton);
        browseJsonAction_ = browseMenu->addAction("Open Image Set File...");
        browseButton->setMenu(browseMenu);
        auto* manifestRow = new QHBoxLayout; 
        manifestRow->setSpacing(6); 
        manifestRow->addWidget(manifestPathEdit_, 1); 
        manifestRow->addWidget(browseButton); 
        auto* manifestLabel = new QLabel;
        nameWidget(manifestLabel, "DatasetWorkspaceManifestLabel");
        leftBody->addWidget(makeDatasetKeyValue("Image set file", manifestLabel));
        leftBody->addLayout(manifestRow);

        totalMetricValue_ = new QLabel("0");
        reviewedMetricValue_ = new QLabel("0");
        auto* metrics = new QHBoxLayout;
        metrics->setSpacing(8);
        metrics->addWidget(makeDatasetMetric("Total", "0"));
        metrics->addWidget(makeDatasetMetric("Reviewed", "0", "0%"));
        leftBody->addLayout(metrics);
        totalMetric_ =
            qobject_cast<QLabel*>(qobject_cast<QFrame*>(metrics->itemAt(0)->widget())->layout()->itemAt(0)->widget());
        reviewedMetric_ =
            qobject_cast<QLabel*>(qobject_cast<QFrame*>(metrics->itemAt(1)->widget())->layout()->itemAt(0)->widget());
        reviewedSubMetric_ =
            qobject_cast<QLabel*>(qobject_cast<QFrame*>(metrics->itemAt(1)->widget())->layout()->itemAt(2)->widget());

        classModeCombo_ = new QComboBox;
        classModeCombo_->addItem("2 labels", 2);
        classModeCombo_->addItem("3 labels", 3);
        classZeroEdit_ = new QLineEdit;
        classOneEdit_ = new QLineEdit;
        classTwoEdit_ = new QLineEdit;
        classTwoLabel_ = new QLabel("Second non-target label");
        classZeroColorButton_ = new QPushButton;
        classOneColorButton_ = new QPushButton;
        classTwoColorButton_ = new QPushButton;
        for (auto* button : {classZeroColorButton_, classOneColorButton_, classTwoColorButton_}) {
            button->setFixedWidth(34);
            button->setMinimumHeight(28);
            button->setText(QString());
            button->setToolTip("Choose label color");
        }
        nameWidget(classZeroColorButton_, "DatasetWorkspaceClassZeroColorButton");
        nameWidget(classOneColorButton_, "DatasetWorkspaceClassOneColorButton");
        nameWidget(classTwoColorButton_, "DatasetWorkspaceClassTwoColorButton");
        auto* classLayout = new QGridLayout;
        classLayout->addWidget(new QLabel("Class setup"), 0, 0);
        classLayout->addWidget(classModeCombo_, 0, 1, 1, 2);
        classLayout->addWidget(new QLabel("Non-target label"), 1, 0);
        classLayout->addWidget(classZeroEdit_, 1, 1);
        classLayout->addWidget(classZeroColorButton_, 1, 2);
        classLayout->addWidget(new QLabel("Target label"), 2, 0);
        classLayout->addWidget(classOneEdit_, 2, 1);
        classLayout->addWidget(classOneColorButton_, 2, 2);
        classLayout->addWidget(classTwoLabel_, 3, 0);
        classLayout->addWidget(classTwoEdit_, 3, 1);
        classLayout->addWidget(classTwoColorButton_, 3, 2);
        leftBody->addLayout(classLayout);

        filterGroup_ = new QButtonGroup(this);
        filterGroup_->setExclusive(true);
        addFilterButton(FilterMode::All, "All", "DatasetWorkspaceFilterAllButton", true, leftBody);
        addFilterButton(FilterMode::Class0, "Non-target", "DatasetWorkspaceFilterHitButton", false, leftBody);
        addFilterButton(FilterMode::Class1, "Target", "DatasetWorkspaceFilterWasteButton", false, leftBody);
        addFilterButton(FilterMode::Class2, "Second non-target", "DatasetWorkspaceFilterClassTwoButton", false, leftBody);
        addFilterButton(FilterMode::Excluded, "Excluded", "DatasetWorkspaceFilterExcludedButton", false, leftBody);
        addFilterButton(FilterMode::Unreviewed, "Unreviewed", "DatasetWorkspaceFilterUnreviewedButton", false,
                        leftBody);

        openFolderButton_ = new QPushButton("Open Image Set Folder");
        nameWidget(openFolderButton_, "DatasetWorkspaceOpenFolderButton");
        leftBody->addWidget(openFolderButton_);
        leftBody->addStretch(1);

        auto* centerPanel = makePanel("All images", "0 shown");
        centerPanel->setObjectName("DatasetCropBrowserPanel");
        centerPanel->setMinimumWidth(360);
        centerPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        centerPanelSubtitle_ = centerPanel->findChildren<QLabel*>().value(1);
        centerPanelTitle_ = centerPanel->findChildren<QLabel*>().value(0);
        auto* centerBody = makePanelBody(centerPanel);

        auto* toolbar = new QHBoxLayout;
        toolbar->setSpacing(8);
        searchEdit_ = new QLineEdit;
        searchEdit_->setPlaceholderText("Search images...");
        searchEdit_->setMaximumWidth(260);
        nameWidget(searchEdit_, "DatasetWorkspaceCropSearchEdit");
        gridButton_ = new QToolButton;
        listButton_ = new QToolButton;
        gridButton_->setText("Grid");
        listButton_->setText("List");
        gridButton_->setCheckable(true);
        listButton_->setCheckable(true);
        gridButton_->setChecked(true);
        nameWidget(gridButton_, "DatasetWorkspaceGridViewButton");
        nameWidget(listButton_, "DatasetWorkspaceListViewButton");
        viewGroup_ = new QButtonGroup(this);
        viewGroup_->setExclusive(true);
        viewGroup_->addButton(gridButton_, 0);
        viewGroup_->addButton(listButton_, 1);
        toolbar->addWidget(searchEdit_);
        toolbar->addStretch(1);
        toolbar->addWidget(gridButton_);
        toolbar->addWidget(listButton_);
        centerBody->addLayout(toolbar);

        browserStack_ = new QStackedWidget;
        nameWidget(browserStack_, "DatasetWorkspaceCropBrowserStack");
        gridList_ = new QListWidget;
        gridList_->setViewMode(QListView::IconMode);
        gridList_->setResizeMode(QListView::Adjust);
        gridList_->setMovement(QListView::Static);
        gridList_->setIconSize(QSize(kIconSize, kIconSize));
        gridList_->setGridSize(QSize(112, 126));
        gridList_->setTextElideMode(Qt::ElideNone);
        gridList_->setSelectionMode(QAbstractItemView::SingleSelection);
        gridList_->setUniformItemSizes(true);
        nameWidget(gridList_, "DatasetWorkspaceCropGrid");
        listTable_ = new QTableWidget(0, 4);
        listTable_->setHorizontalHeaderLabels({"Thumbnail", "Filename", "Assigned label", "State"});
        listTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        listTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        listTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        listTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        listTable_->verticalHeader()->setDefaultSectionSize(58);
        listTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        listTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        listTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        nameWidget(listTable_, "DatasetWorkspaceCropListTable");
        browserStack_->addWidget(gridList_);
        browserStack_->addWidget(listTable_);
        centerBody->addWidget(browserStack_, 1);

        auto* paginationRow = new QHBoxLayout;
        paginationRow->setSpacing(8);
        auto* pageSizeLabel = new QLabel("Page size");
        pageSizeLabel->setProperty("metricLabel", true);
        pageSizeCombo_ = new QComboBox;
        pageSizeCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        pageSizeCombo_->setMinimumContentsLength(4);
        pageSizeCombo_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        pageSizeCombo_->view()->setTextElideMode(Qt::ElideNone);
        pageSizeCombo_->addItem("100", 100);
        pageSizeCombo_->addItem("200", 200);
        pageSizeCombo_->addItem("300", 300);
        pageSizeCombo_->setCurrentIndex(0);
        nameWidget(pageSizeCombo_, "DatasetWorkspacePageSizeCombo");
        pageSummaryLabel_ = new QLabel("0 results");
        pageSummaryLabel_->setProperty("mutedText", true);
        nameWidget(pageSummaryLabel_, "DatasetWorkspacePageSummaryLabel");
        pagePrevButton_ = new QPushButton("<");
        pageNextButton_ = new QPushButton(">");
        pagePrevButton_->setMinimumWidth(34);
        pageNextButton_->setMinimumWidth(34);
        nameWidget(pagePrevButton_, "DatasetWorkspacePagePrevButton");
        nameWidget(pageNextButton_, "DatasetWorkspacePageNextButton");
        auto* pageButtonsHost = new QWidget;
        pageButtonsLayout_ = new QHBoxLayout;
        pageButtonsLayout_->setContentsMargins(0, 0, 0, 0);
        pageButtonsLayout_->setSpacing(4);
        pageButtonsHost->setLayout(pageButtonsLayout_);
        nameWidget(pageButtonsHost, "DatasetWorkspacePageButtons");
        paginationRow->addWidget(pageSizeLabel);
        paginationRow->addWidget(pageSizeCombo_);
        paginationRow->addWidget(pageSummaryLabel_);
        paginationRow->addStretch(1);
        paginationRow->addWidget(pagePrevButton_);
        paginationRow->addWidget(pageButtonsHost);
        paginationRow->addWidget(pageNextButton_);
        centerBody->addLayout(paginationRow);

        auto* rightPanel = makePanel("Review", "Selected image");
        rightPanel->setObjectName("DatasetReviewPanel");
        rightPanel->setMinimumWidth(340);
        rightPanel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        auto* rightBody = makePanelBody(rightPanel);

        previewLabel_ = new QLabel("No image selected");
        previewLabel_->setAlignment(Qt::AlignCenter);
        previewLabel_->setMinimumSize(220, 220);
        previewLabel_->setMaximumHeight(300);
        previewLabel_->setProperty("viewerCanvas", true);
        nameWidget(previewLabel_, "DatasetWorkspacePreviewFrame");
        rightBody->addWidget(previewLabel_);

        filenameLabel_ = new QLabel("--");
        autoLabel_ = new QLabel("--");
        frameLabel_ = new QLabel("--");
        timestampLabel_ = new QLabel("--");
        manualLabel_ = new QLabel("--");
        auto* metadata = new QGridLayout;
        addMetadataRow(metadata, 0, "Filename", filenameLabel_);
        addMetadataRow(metadata, 1, "Assigned label", manualLabel_);
        addMetadataRow(metadata, 2, "Review state", autoLabel_);
        addMetadataRow(metadata, 3, "Frame", frameLabel_);
        addMetadataRow(metadata, 4, "Timestamp", timestampLabel_);
        rightBody->addLayout(metadata);

        auto* manualReviewLabel = new QLabel("Manual Review");
        manualReviewLabel->setProperty("panelTitle", true);
        rightBody->addWidget(manualReviewLabel);
        auto* actionRow = new QHBoxLayout;
        actionRow->setSpacing(8);
        hitButton_ = new QPushButton;
        wasteButton_ = new QPushButton;
        classThreeButton_ = new QPushButton;
        excludeButton_ = new QPushButton("Exclude");
        hitButton_->setProperty("reviewClassId", "0");
        wasteButton_->setProperty("reviewClassId", "1");
        classThreeButton_->setProperty("reviewClassId", "2");
        excludeButton_->setProperty("reviewClassId", "exclude");
        for (auto* button :
             {hitButton_, wasteButton_, classThreeButton_, excludeButton_, openFolderButton_}) {
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        }
        nameWidget(hitButton_, "DatasetWorkspaceHitButton");
        nameWidget(wasteButton_, "DatasetWorkspaceWasteButton");
        nameWidget(classThreeButton_, "DatasetWorkspaceClassThreeButton");
        nameWidget(excludeButton_, "DatasetWorkspaceExcludeButton");
        actionRow->addWidget(hitButton_, 1);
        actionRow->addWidget(wasteButton_, 1);
        actionRow->addWidget(classThreeButton_, 1);
        rightBody->addLayout(actionRow);
        rightBody->addWidget(excludeButton_);
        auto* nav = new QHBoxLayout;
        undoButton_ = new QPushButton("Undo");
        previousButton_ = new QPushButton("Previous");
        nextButton_ = new QPushButton("Next");
        nameWidget(undoButton_, "DatasetWorkspaceUndoButton");
        nameWidget(previousButton_, "DatasetWorkspacePreviousButton");
        nameWidget(nextButton_, "DatasetWorkspaceNextButton");
        for (auto* button : {undoButton_, previousButton_, nextButton_}) {
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        }
        nav->addWidget(undoButton_);
        nav->addStretch(1);
        nav->addWidget(previousButton_);
        nav->addWidget(nextButton_);
        rightBody->addLayout(nav);
        statusLabel_ = new QLabel("Choose a manifest or folder to load crops.");
        statusLabel_->setWordWrap(true);
        statusLabel_->setProperty("mutedText", true);
        nameWidget(statusLabel_, "DatasetWorkspaceStatusLabel");
        rightBody->addWidget(statusLabel_);
        rightBody->addStretch(1);

        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->setObjectName("DatasetWorkspaceSplitter");
        splitter->setChildrenCollapsible(false);
        splitter->setOpaqueResize(true);
        splitter->addWidget(leftPanel);
        splitter->addWidget(centerPanel);
        splitter->addWidget(rightPanel);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);
        splitter->setStretchFactor(2, 1);
        splitter->setSizes({310, 620, 390});

        root->addWidget(splitter, 1);
        setMinimumWidth(1000);
        setLayout(root);

        browseButton_ = browseButton;
        browseMenu_ = browseMenu;
        resetClassSchema(2);
        updateClassSchemaUi();
    }

    void addMetadataRow(QGridLayout* layout, int row, const QString& key, QLabel* value) {
        auto* keyLabel = new QLabel(key);
        keyLabel->setProperty("metricLabel", true);
        value->setProperty("mutedText", true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        layout->addWidget(keyLabel, row, 0);
        layout->addWidget(value, row, 1);
    }

    void addFilterButton(FilterMode mode, const QString& label, const QString& objectName, bool checked,
                         QVBoxLayout* layout) {
        auto* button = makeDatasetFilterRow(objectName);
        button->setText(QString("%1 (0)").arg(label));
        button->setChecked(checked);
        filterGroup_->addButton(button, static_cast<int>(mode));
        filterButtons_.insert(mode, button);
        layout->addWidget(button);
    }

    bool shouldUseSynchronousLoad() const {
        return !qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed().isEmpty() ||
               qApp->property("ovdsDatasetWorkspaceForceSynchronousLoad").toBool();
    }

    void setupManifestAutosaveTimer() {
        manifestAutosaveTimer_ = new QTimer(this);
        manifestAutosaveTimer_->setSingleShot(true);
        manifestAutosaveTimer_->setInterval(kManifestAutosaveDebounceMs);
        connect(manifestAutosaveTimer_, &QTimer::timeout, this, [this]() { flushPendingManifestSave(); });
    }

    void ensureLoadingDialog() {
        if (loadingDialog_)
            return;
        loadingDialog_ = new QDialog(this, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        loadingDialog_->setModal(true);
        loadingDialog_->setMinimumWidth(360);
        auto* layout = new QVBoxLayout(loadingDialog_);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);
        loadingTitleLabel_ = new QLabel;
        loadingTitleLabel_->setProperty("panelTitle", true);
        loadingDetailLabel_ = new QLabel;
        loadingDetailLabel_->setWordWrap(true);
        loadingDetailLabel_->setProperty("mutedText", true);
        loadingProgressBar_ = new QProgressBar;
        loadingProgressBar_->setTextVisible(true);
        layout->addWidget(loadingTitleLabel_);
        layout->addWidget(loadingDetailLabel_);
        layout->addWidget(loadingProgressBar_);
    }

    void showLoadingPopup(const QString& title, const QString& detail, int maximum, bool busy) {
        ensureLoadingDialog();
        loadingDialog_->setWindowTitle(title);
        loadingTitleLabel_->setText(title);
        loadingDetailLabel_->setText(detail);
        if (busy) {
            loadingProgressBar_->setRange(0, 0);
        } else {
            loadingProgressBar_->setRange(0, std::max(maximum, 1));
            loadingProgressBar_->setValue(0);
        }
        loadingDialog_->show();
        loadingDialog_->raise();
        loadingDialog_->activateWindow();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    void updateLoadingPopup(const QString& detail, int value = -1, int maximum = -1) {
        if (!loadingDialog_)
            return;
        if (maximum >= 0) {
            loadingProgressBar_->setRange(0, std::max(maximum, 1));
            if (value >= 0)
                loadingProgressBar_->setValue(std::clamp(value, 0, loadingProgressBar_->maximum()));
        } else if (value >= 0 && loadingProgressBar_->maximum() > 0) {
            loadingProgressBar_->setValue(std::clamp(value, 0, loadingProgressBar_->maximum()));
        }
        loadingDetailLabel_->setText(detail);
    }

    void hideLoadingPopup() {
        if (loadingDialog_)
            loadingDialog_->hide();
    }

    int pageCount() const {
        if (filteredIndexes_.isEmpty())
            return 0;
        return (filteredIndexes_.size() + pageSize_ - 1) / pageSize_;
    }

    int filteredRowForSourceIndex(int sourceIndex) const {
        return filteredIndexes_.indexOf(sourceIndex);
    }

    int pageForFilteredRow(int filteredRow) const {
        if (filteredRow < 0 || pageSize_ <= 0)
            return 0;
        return filteredRow / pageSize_;
    }

    void clearPaginationButtons() {
        if (!pageButtonsLayout_)
            return;
        while (QLayoutItem* item = pageButtonsLayout_->takeAt(0)) {
            if (QWidget* widget = item->widget())
                delete widget;
            delete item;
        }
    }

    void rebuildVisiblePage() {
        visibleIndexes_.clear();
        if (filteredIndexes_.isEmpty())
            return;
        const int first = currentPage_ * pageSize_;
        const int last = std::min(first + pageSize_, static_cast<int>(filteredIndexes_.size()));
        for (int i = first; i < last; ++i)
            visibleIndexes_.push_back(filteredIndexes_.at(i));
    }

    void markBrowserPagesDirty() {
        gridPageDirty_ = true;
        tablePageDirty_ = true;
    }

    void rebuildActiveBrowserView() {
        const int activeIndex = browserStack_ ? browserStack_->currentIndex() : 0;
        if (activeIndex == 0) {
            if (gridPageDirty_) {
                rebuildGrid();
                gridPageDirty_ = false;
            }
        } else if (tablePageDirty_) {
            rebuildTable();
            tablePageDirty_ = false;
        }
    }

    void updatePageSummary() {
        if (!pageSummaryLabel_)
            return;
        if (filteredIndexes_.isEmpty()) {
            pageSummaryLabel_->setText("0 results");
            return;
        }
        pageSummaryLabel_->setText(QString("Page %1 of %2 (%3 filtered)")
                                       .arg(currentPage_ + 1)
                                       .arg(std::max(pageCount(), 1))
                                       .arg(filteredIndexes_.size()));
    }

    void rebuildPaginationControls() {
        clearPaginationButtons();
        updatePageSummary();
        const int totalPages = pageCount();
        if (pagePrevButton_)
            pagePrevButton_->setEnabled(currentPage_ > 0);
        if (pageNextButton_)
            pageNextButton_->setEnabled(totalPages > 0 && currentPage_ < totalPages - 1);

        auto addPageButton = [this](int page) {
            auto* button = new QPushButton(QString::number(page + 1));
            button->setCheckable(true);
            button->setChecked(page == currentPage_);
            button->setEnabled(page != currentPage_);
            button->setMinimumWidth(34);
            connect(button, &QPushButton::clicked, this, [this, page]() { goToPage(page); });
            pageButtonsLayout_->addWidget(button);
        };
        auto addEllipsis = [this]() {
            auto* label = new QLabel("...");
            label->setProperty("mutedText", true);
            pageButtonsLayout_->addWidget(label);
        };

        if (totalPages <= 0) {
            auto* button = new QPushButton("1");
            button->setEnabled(false);
            button->setMinimumWidth(34);
            pageButtonsLayout_->addWidget(button);
            return;
        }

        int start = std::max(0, currentPage_ - (kVisiblePageButtonCount / 2));
        int end = std::min(totalPages - 1, start + kVisiblePageButtonCount - 1);
        start = std::max(0, end - kVisiblePageButtonCount + 1);

        if (start > 0) {
            addPageButton(0);
            if (start > 1)
                addEllipsis();
        }
        for (int page = start; page <= end; ++page)
            addPageButton(page);
        if (end < totalPages - 1) {
            if (end < totalPages - 2)
                addEllipsis();
            addPageButton(totalPages - 1);
        }
    }

    void showCurrentPage(int preferredSourceIndex = -1, bool fallbackToFirst = true) {
        const int totalPages = pageCount();
        if (totalPages <= 0) {
            currentPage_ = 0;
        } else {
            currentPage_ = std::clamp(currentPage_, 0, totalPages - 1);
        }
        rebuildVisiblePage();
        markBrowserPagesDirty();
        rebuildActiveBrowserView();
        rebuildPaginationControls();

        int selectedVisibleRow = -1;
        const int preferred = preferredSourceIndex >= 0 ? preferredSourceIndex : selectedSourceIndex_;
        if (preferred >= 0)
            selectedVisibleRow = visibleIndexes_.indexOf(preferred);
        if (selectedVisibleRow < 0 && fallbackToFirst && !visibleIndexes_.isEmpty())
            selectedVisibleRow = 0;
        setSelectionByVisibleRow(selectedVisibleRow);
        updateCounts();
        updatePreview();
        updateReviewControls();
    }

    void goToPage(int page) {
        if (pageCount() <= 0)
            return;
        currentPage_ = std::clamp(page, 0, pageCount() - 1);
        showCurrentPage(selectedSourceIndex_, true);
    }

    void handleBrowserViewChanged(int index) {
        if (browserStack_)
            browserStack_->setCurrentIndex(index);
        rebuildActiveBrowserView();
        const int selectedVisibleRow = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (selectedVisibleRow >= 0)
            setSelectionByVisibleRow(selectedVisibleRow);
    }

    void handlePageSizeChanged() {
        const int requested = pageSizeCombo_ ? pageSizeCombo_->currentData().toInt() : kDefaultPageSize;
        if (requested <= 0 || requested == pageSize_)
            return;
        const int anchorSourceIndex = selectedSourceIndex_ >= 0
                                          ? selectedSourceIndex_
                                          : (visibleIndexes_.isEmpty() ? -1 : visibleIndexes_.front());
        pageSize_ = requested;
        if (anchorSourceIndex >= 0) {
            const int filteredRow = filteredRowForSourceIndex(anchorSourceIndex);
            currentPage_ = pageForFilteredRow(filteredRow);
        } else {
            currentPage_ = 0;
        }
        showCurrentPage(anchorSourceIndex, true);
    }

    void wireUi() {
        connect(browseJsonAction_, &QAction::triggered, this, [this]() { browseForDatasetJson(); });
        connect(openFolderButton_, &QPushButton::clicked, this, [this]() { openManifestFolder(); });
        connect(filterGroup_, &QButtonGroup::idClicked, this, [this](int id) {
            filterMode_ = static_cast<FilterMode>(id);
            applyFilters();
        });
        connect(searchEdit_, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
        connect(viewGroup_, &QButtonGroup::idClicked, this, [this](int id) { handleBrowserViewChanged(id); });
        connect(pageSizeCombo_, &QComboBox::currentIndexChanged, this, [this]() { handlePageSizeChanged(); });
        connect(pagePrevButton_, &QPushButton::clicked, this, [this]() { goToPage(currentPage_ - 1); });
        connect(pageNextButton_, &QPushButton::clicked, this, [this]() { goToPage(currentPage_ + 1); });
        connect(gridList_, &QListWidget::itemSelectionChanged, this, [this]() { selectFromGrid(); });
        connect(listTable_, &QTableWidget::itemSelectionChanged, this, [this]() { selectFromTable(); });
        connect(classModeCombo_, &QComboBox::currentIndexChanged, this, [this]() { handleClassModeChanged(); });
        connect(classZeroEdit_, &QLineEdit::editingFinished, this, [this]() { handleClassLabelEdited(0); });
        connect(classOneEdit_, &QLineEdit::editingFinished, this, [this]() { handleClassLabelEdited(1); });
        connect(classTwoEdit_, &QLineEdit::editingFinished, this, [this]() { handleClassLabelEdited(2); });
        connect(classZeroColorButton_, &QPushButton::clicked, this, [this]() { chooseClassColor(0); });
        connect(classOneColorButton_, &QPushButton::clicked, this, [this]() { chooseClassColor(1); });
        connect(classTwoColorButton_, &QPushButton::clicked, this, [this]() { chooseClassColor(2); });
        connect(hitButton_, &QPushButton::clicked, this,
                [this]() { applyReviewLabel(classEntries_.value(0).id); });
        connect(wasteButton_, &QPushButton::clicked, this,
                [this]() { applyReviewLabel(classEntries_.value(1).id); });
        connect(classThreeButton_, &QPushButton::clicked, this, [this]() {
            if (classEntries_.size() >= 3)
                applyReviewLabel(classEntries_.value(2).id);
        });
        connect(excludeButton_, &QPushButton::clicked, this, [this]() { applyReviewLabel("exclude"); });
        connect(undoButton_, &QPushButton::clicked, this, [this]() { undoLastLabelChange(); });
        connect(previousButton_, &QPushButton::clicked, this, [this]() { selectRelative(-1); });
        connect(nextButton_, &QPushButton::clicked, this, [this]() { selectRelative(1); });

        auto* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
        auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
        connect(prevShortcut, &QShortcut::activated, this, [this]() {
            if (shortcutAllowed())
                selectRelative(-1);
        });
        connect(nextShortcut, &QShortcut::activated, this, [this]() {
            if (shortcutAllowed())
                selectRelative(1);
        });
    }

    void browseForDatasetFolder() {
        const QString startDir = chooseExistingDirectoryDialogPath(
            datasetRoot_, defaultOpenDssPreparedDatasetsPath(), findPackagedAppPath("datasets/prepared"));
        const QString selected =
            QFileDialog::getExistingDirectory(this, "Select image set folder", startDir, QFileDialog::ShowDirsOnly);
        if (!selected.isEmpty())
            loadDatasetPath(selected);
    }

    void browseForDatasetJson() {
        const QString currentPath =
            preferredDatasetMetadataPath(manifestPath_.trimmed().isEmpty() ? datasetRoot_ : manifestPath_);
        const QString startPath = chooseOpenFileDialogPath(
            currentPath, preferredDatasetMetadataPath(defaultOpenDssPreparedDatasetsPath()),
            findPackagedAppPath("datasets/prepared"));
        const QString selected = QFileDialog::getOpenFileName(this, "Select image set file", startPath,
                                                              "Image set files (*.json);;All files (*.*)");
        if (!selected.isEmpty())
            loadDatasetPath(selected);
    }

    void openManifestFolder() {
        QString folder = datasetRoot_;
        if (!manifestPath_.isEmpty())
            folder = QFileInfo(manifestPath_).absolutePath();
        if (!folder.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }

    CropItem cropFromManifestItem(const QJsonObject& item, int manifestIndex, bool legacyManifestFallback) const {
        CropItem crop;
        crop.manifestIndex = manifestIndex;
        crop.imageId = item.value("image_id").toString(QString("crop_%1").arg(manifestIndex + 1, 4, 10, QChar('0')));
        crop.cropPath = firstNonEmptyString(item, {"crop_path", "path", "image_path", "relative_path"});
        crop.autoLabel = canonicalLabel(item.value("auto_label").toString("unknown"));
        crop.manualLabel = canonicalLabel(item.value("reviewed_label").toVariant().toString());
        if (legacyManifestFallback && crop.manualLabel.isEmpty())
            crop.manualLabel = legacyReviewLabel(item);
        crop.reviewState = item.value("review_state").toString().trimmed();
        if (legacyManifestFallback && crop.reviewState.isEmpty())
            crop.reviewState = derivedLegacyReviewState(item, crop.manualLabel);
        if (legacyManifestFallback && (legacyStatus(item) == "rejected" || legacyStatus(item) == "excluded") &&
            item.value("reviewed_label").toVariant().toString().trimmed().isEmpty()) {
            crop.manualLabel = "exclude";
        }
        if (crop.reviewState.isEmpty())
            crop.reviewState = crop.manualLabel.isEmpty() ? "unreviewed" : "confirmed";
        crop.confidence = item.value("auto_label_confidence").toVariant().toString();
        crop.frameNumber = item.value("source_frame_id").toVariant().toString();
        crop.timestamp = item.value("timestamp").toString();
        crop.excludeReason = item.value("exclude_reason").toString();
        crop.json = item;
        return crop;
    }

    CropItem cropFromAbsoluteImagePath(const QString& folder, const QString& absolutePath, int manifestIndex) const {
        const QFileInfo info(absolutePath);
        CropItem crop;
        crop.manifestIndex = manifestIndex;
        crop.imageId = info.completeBaseName();
        crop.cropPath = QDir(folder).relativeFilePath(absolutePath);
        crop.autoLabel = "unknown";
        crop.reviewState = "unreviewed";
        crop.timestamp = info.lastModified().toUTC().toString(Qt::ISODate);
        crop.json = cropToJson(crop);
        return crop;
    }

    void cancelPendingLoad() {
        ++loadGeneration_;
        isLoading_ = false;
        pendingLoad_ = PendingLoadState{};
        hideLoadingPopup();
    }

    bool beginManifestLoad(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            return false;

        pendingLoad_ = PendingLoadState{};
        pendingLoad_.mode = PendingLoadState::Mode::Manifest;
        pendingLoad_.manifestPath = QFileInfo(path).absoluteFilePath();
        pendingLoad_.manifestDoc = doc;
        pendingLoad_.manifestRoot = doc.object();
        pendingLoad_.rows = pendingLoad_.manifestRoot.value("items").toArray();
        pendingLoad_.legacyManifestFallback = usesLegacyManifestFallback(pendingLoad_.manifestRoot);
        pendingLoad_.nextIndex = 0;

        manifestDoc_ = pendingLoad_.manifestDoc;
        manifestPath_ = pendingLoad_.manifestPath;
        QFileInfo info(manifestPath_);
        datasetRoot_ = info.absolutePath();
        if (info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
            QDir root(info.dir());
            root.cdUp();
            datasetRoot_ = root.absolutePath();
        }
        loadClassSchema(pendingLoad_.manifestRoot);
        updateClassSchemaUi();

        isLoading_ = true;
        const int totalRows = pendingLoad_.rows.size();
        showLoadingPopup("Loading image set",
                         QString("Loading 0 of %1 items...").arg(totalRows),
                         std::max(totalRows, 1),
                         false);
        const quint64 generation = loadGeneration_;
        QTimer::singleShot(0, this, [this, generation]() { processPendingLoadBatch(generation); });
        return true;
    }

    bool beginFolderScanLoad(const QString& folder) {
        if (folder.trimmed().isEmpty() || !QDir(folder).exists())
            return false;

        pendingLoad_ = PendingLoadState{};
        pendingLoad_.mode = PendingLoadState::Mode::FolderScan;
        pendingLoad_.folderPath = folder;
        pendingLoad_.scanIterator = std::make_unique<QDirIterator>(folder, imageNameFilters(), QDir::Files,
                                                                   QDirIterator::Subdirectories);
        pendingLoad_.nextIndex = 0;

        isLoading_ = true;
        showLoadingPopup("Loading image set", "Scanning review images...", 0, true);
        const quint64 generation = loadGeneration_;
        QTimer::singleShot(0, this, [this, generation]() { processPendingLoadBatch(generation); });
        return true;
    }

    void processPendingLoadBatch(quint64 generation) {
        if (!isLoading_ || generation != loadGeneration_)
            return;

        if (pendingLoad_.mode == PendingLoadState::Mode::Manifest) {
            const int totalRows = pendingLoad_.rows.size();
            const int endRow = std::min(pendingLoad_.nextIndex + kLoadBatchSize, totalRows);
            for (int row = pendingLoad_.nextIndex; row < endRow; ++row)
                items_.push_back(cropFromManifestItem(pendingLoad_.rows.at(row).toObject(), row,
                                                      pendingLoad_.legacyManifestFallback));
            pendingLoad_.nextIndex = endRow;
            updateLoadingPopup(QString("Loading %1 of %2 items...").arg(endRow).arg(totalRows), endRow);
            if (pendingLoad_.nextIndex < totalRows) {
                QTimer::singleShot(0, this, [this, generation]() { processPendingLoadBatch(generation); });
                return;
            }
            finishPendingLoad("Image set file loaded. Label changes save automatically.");
            return;
        }

        if (pendingLoad_.mode == PendingLoadState::Mode::FolderScan) {
            int processed = 0;
            while (processed < kLoadBatchSize && pendingLoad_.scanIterator && pendingLoad_.scanIterator->hasNext()) {
                const QString absolutePath = pendingLoad_.scanIterator->next();
                CropItem crop =
                    cropFromAbsoluteImagePath(pendingLoad_.folderPath, absolutePath, pendingLoad_.nextIndex++);
                items_.push_back(crop);
                pendingLoad_.scannedManifestItems.append(crop.json);
                ++processed;
            }
            updateLoadingPopup(QString("Scanning review images... %1 found").arg(items_.size()));
            if (pendingLoad_.scanIterator && pendingLoad_.scanIterator->hasNext()) {
                QTimer::singleShot(0, this, [this, generation]() { processPendingLoadBatch(generation); });
                return;
            }

            QJsonObject root;
            root["schema_version"] = "dataset-builder-manifest-v1";
            root["dataset_id"] = QFileInfo(pendingLoad_.folderPath).fileName();
            root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            root["items"] = pendingLoad_.scannedManifestItems;
            resetClassSchema(2);
            storeClassSchema(root);
            updateClassSchemaUi();
            manifestDoc_ = QJsonDocument(root);
            manifestPath_ = QDir(pendingLoad_.folderPath).filePath("metadata/dataset_manifest.json");
            finishPendingLoad(items_.isEmpty()
                                  ? "No image set file or review images found. Use Browse to choose an image set file."
                                  : "No image set file found. Review images were scanned and a file will be saved after the first label change.");
        }
    }

    void finishPendingLoad(const QString& statusMessage) {
        if (loadingProgressBar_ && loadingProgressBar_->maximum() > 0)
            loadingProgressBar_->setValue(loadingProgressBar_->maximum());
        if (loadingDetailLabel_)
            loadingDetailLabel_->setText("Preparing filtered view...");
        manifestPathEdit_->setText(manifestPath_.isEmpty() ? QDir::toNativeSeparators(datasetRoot_)
                                                           : QDir::toNativeSeparators(manifestPath_));
        manifestPathEdit_->setToolTip(manifestPathEdit_->text());
        updateAll();
        statusLabel_->setText(statusMessage);
        pendingLoad_ = PendingLoadState{};
        isLoading_ = false;
        hideLoadingPopup();
    }

    void loadDatasetPath(const QString& path) {
        flushPendingManifestSave();
        cancelPendingLoad();
        clearDataset();
        const QFileInfo info(path);
        datasetRoot_ = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        QString manifest = info.isFile() ? info.absoluteFilePath() : QString();
        if (manifest.isEmpty()) {
            const QDir dir(datasetRoot_);
            const QString metadataManifest = dir.filePath("metadata/dataset_manifest.json");
            const QString rootManifest = dir.filePath("manifest.json");
            if (QFileInfo::exists(metadataManifest))
                manifest = metadataManifest;
            else if (QFileInfo::exists(rootManifest))
                manifest = rootManifest;
        }
        updateAll();
        manifestPathEdit_->setText(QDir::toNativeSeparators(!manifest.isEmpty() ? manifest : datasetRoot_));
        manifestPathEdit_->setToolTip(manifestPathEdit_->text());

        if (shouldUseSynchronousLoad()) {
            if (!manifest.isEmpty() && loadManifest(manifest)) {
                statusLabel_->setText("Image set file loaded. Label changes save automatically.");
            } else {
                scanFolderForImages(datasetRoot_);
                statusLabel_->setText(
                    items_.isEmpty()
                        ? "No image set file or review images found. Use Browse to choose an image set file."
                        : "No image set file found. Review images were scanned and a file will be saved after the first label change.");
            }
            manifestPathEdit_->setText(manifestPath_.isEmpty() ? QDir::toNativeSeparators(datasetRoot_)
                                                               : QDir::toNativeSeparators(manifestPath_));
            manifestPathEdit_->setToolTip(manifestPathEdit_->text());
            updateAll();
            return;
        }

        ++loadGeneration_;
        statusLabel_->setText("Loading image set...");
        showLoadingPopup("Loading image set", "Opening image set file...", 0, true);
        if (!manifest.isEmpty() && beginManifestLoad(manifest))
            return;
        if (beginFolderScanLoad(datasetRoot_))
            return;

        hideLoadingPopup();
        statusLabel_->setText("No image set file or review images found. Use Browse to choose an image set file.");
        manifestPathEdit_->setText(manifestPath_.isEmpty() ? QDir::toNativeSeparators(datasetRoot_)
                                                           : QDir::toNativeSeparators(manifestPath_));
        manifestPathEdit_->setToolTip(manifestPathEdit_->text());
        updateAll();
    }

    bool loadManifest(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            return false;
        const QJsonObject rootObject = doc.object();
        const bool legacyManifestFallback = usesLegacyManifestFallback(rootObject);
        manifestDoc_ = doc;
        manifestPath_ = QFileInfo(path).absoluteFilePath();
        QFileInfo info(manifestPath_);
        datasetRoot_ = info.absolutePath();
        if (info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
            QDir root(info.dir());
            root.cdUp();
            datasetRoot_ = root.absolutePath();
        }
        loadClassSchema(rootObject);
        updateClassSchemaUi();
        const QJsonArray rows = rootObject.value("items").toArray();
        for (int i = 0; i < rows.size(); ++i)
            items_.push_back(cropFromManifestItem(rows.at(i).toObject(), i, legacyManifestFallback));
        return true;
    }

    void scanFolderForImages(const QString& folder) {
        if (folder.isEmpty())
            return;
        QDirIterator it(folder, imageNameFilters(), QDir::Files, QDirIterator::Subdirectories);
        QJsonArray manifestItems;
        while (it.hasNext()) {
            CropItem crop = cropFromAbsoluteImagePath(folder, it.next(), items_.size());
            items_.push_back(crop);
            manifestItems.append(crop.json);
        }
        QJsonObject root;
        root["schema_version"] = "dataset-builder-manifest-v1";
        root["dataset_id"] = QFileInfo(folder).fileName();
        root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        root["items"] = manifestItems;
        resetClassSchema(2);
        storeClassSchema(root);
        updateClassSchemaUi();
        manifestDoc_ = QJsonDocument(root);
        manifestPath_ = QDir(folder).filePath("metadata/dataset_manifest.json");
    }

    QJsonObject cropToJson(const CropItem& crop) const {
        QJsonObject item = crop.json;
        item["image_id"] = crop.imageId;
        item["crop_path"] = crop.cropPath;
        item["timestamp"] =
            crop.timestamp.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : crop.timestamp;
        item["collection_mode"] = item.value("collection_mode").toString("mixed");
        item["batch_index"] = item.value("batch_index").toInt(1);
        item["batch_target"] = item.value("batch_target").toInt(1);
        item["auto_label"] = crop.autoLabel.isEmpty() ? "unknown" : crop.autoLabel;
        item["auto_label_source"] = item.value("auto_label_source").toString("none");
        item["review_state"] = crop.reviewState.isEmpty() ? "unreviewed" : crop.reviewState;
        if (crop.manualLabel.isEmpty())
            item["reviewed_label"] = QJsonValue::Null;
        else
            item["reviewed_label"] = crop.manualLabel;
        if (crop.excludeReason.isEmpty())
            item["exclude_reason"] = QJsonValue::Null;
        else
            item["exclude_reason"] = crop.excludeReason;
        item["trainer_eligible"] = isTrainerClassId(crop.manualLabel);
        return item;
    }

    void clearDataset() {
        if (manifestAutosaveTimer_)
            manifestAutosaveTimer_->stop();
        pendingManifestItemUpdates_.clear();
        items_.clear();
        filteredIndexes_.clear();
        visibleIndexes_.clear();
        undoStack_.clear();
        thumbnailCache_.clear();
        markBrowserPagesDirty();
        manifestDoc_ = QJsonDocument();
        manifestPath_.clear();
        selectedSourceIndex_ = -1;
        currentPage_ = 0;
    }

    QString effectiveLabel(const CropItem& crop) const {
        const QString manual = canonicalLabel(crop.manualLabel);
        if (isTrainerClassId(manual) || manual == "exclude")
            return manual;
        if (crop.reviewState.toLower() == "excluded")
            return "exclude";
        return "unreviewed";
    }

    bool cropMatchesFilter(const CropItem& crop) const {
        if (!hasDisplayableCropPath(crop))
            return false;
        const QString label = effectiveLabel(crop);
        if (filterMode_ == FilterMode::Class0 && label != "0")
            return false;
        if (filterMode_ == FilterMode::Class1 && label != "1")
            return false;
        if (filterMode_ == FilterMode::Class2 && (classEntries_.size() < 3 || label != "2"))
            return false;
        if (filterMode_ == FilterMode::Excluded && label != "exclude")
            return false;
        if (filterMode_ == FilterMode::Unreviewed && label != "unreviewed")
            return false;
        const QString needle = searchEdit_->text().trimmed().toLower();
        if (needle.isEmpty())
            return true;
        const QString haystack = QStringList{crop.imageId,
                                             crop.cropPath,
                                             displayLabel(crop.manualLabel),
                                             crop.manualLabel,
                                             crop.reviewState,
                                             crop.frameNumber,
                                             crop.timestamp}
                                     .join(' ')
                                     .toLower();
        return haystack.contains(needle);
    }

    QString compactTileLabelCandidate(const QString& raw) const {
        QString stem = raw.trimmed();
        if (stem.isEmpty())
            return {};

        stem.replace('\\', '/');
        const QString fileName = QFileInfo(stem).fileName();
        if (!fileName.isEmpty())
            stem = QFileInfo(fileName).completeBaseName();
        stem = stem.trimmed();
        if (stem.isEmpty())
            return {};

        const QRegularExpression leadingDigits(QStringLiteral("^(\\d{3,})"));
        const QRegularExpressionMatch leadingMatch = leadingDigits.match(stem);
        if (leadingMatch.hasMatch())
            return leadingMatch.captured(1).left(kTileLabelMaxChars);

        const QStringList parts = stem.split(QRegularExpression(QStringLiteral("[\\s_\\-.]+")), Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            const QString part = parts.at(i);
            if (!part.contains(QRegularExpression(QStringLiteral("\\d"))))
                continue;
            if (i > 0) {
                const QString prefix = parts.at(i - 1);
                const QString combined = prefix + "_" + part;
                if (combined.size() <= kTileLabelMaxChars)
                    return combined;
            }
            return part.left(kTileLabelMaxChars);
        }

        if (stem.size() <= kTileLabelMaxChars)
            return stem;
        return stem.left(kTileLabelMaxChars);
    }

    QString tileDisplayText(const CropItem& crop) const {
        const QStringList candidates = {
            crop.imageId,
            QFileInfo(crop.cropPath).completeBaseName(),
            QFileInfo(crop.cropPath).fileName(),
        };
        for (const QString& candidate : candidates) {
            const QString label = compactTileLabelCandidate(candidate);
            if (!label.isEmpty())
                return label;
        }
        if (crop.manifestIndex >= 0)
            return QString("#%1").arg(crop.manifestIndex + 1);
        return "image";
    }

    void applyFilters(bool refreshView = true) {
        const int previousSelection = selectedSourceIndex_;
        filteredIndexes_.clear();
        for (int i = 0; i < items_.size(); ++i) {
            if (cropMatchesFilter(items_.at(i)))
                filteredIndexes_.push_back(i);
        }
        const int selectedFilteredRow = filteredRowForSourceIndex(previousSelection);
        if (selectedFilteredRow >= 0)
            currentPage_ = pageForFilteredRow(selectedFilteredRow);
        if (refreshView)
            showCurrentPage(previousSelection, true);
    }

    void rebuildGrid() {
        QSignalBlocker blocker(gridList_);
        gridList_->clear();
        if (visibleIndexes_.isEmpty()) {
            auto* empty = new QListWidgetItem("No images match");
            empty->setFlags(Qt::NoItemFlags);
            gridList_->addItem(empty);
            return;
        }
        for (const int sourceIndex : visibleIndexes_) {
            const CropItem& crop = items_.at(sourceIndex);
            const QString tileText = tileDisplayText(crop);
            auto* item = new QListWidgetItem(makeThumbnailIcon(crop, kIconSize), tileText);
            item->setData(kSourceIndexRole, sourceIndex);
            item->setData(kTileDisplayTextRole, tileText);
            item->setToolTip(QString("%1\n%2").arg(crop.imageId, QFileInfo(crop.cropPath).fileName()));
            item->setTextAlignment(Qt::AlignCenter);
            item->setForeground(reviewBaseColor(effectiveLabel(crop)));
            gridList_->addItem(item);
        }
    }

    void rebuildTable() {
        QSignalBlocker blocker(listTable_);
        listTable_->setRowCount(0);
        for (const int sourceIndex : visibleIndexes_) {
            const CropItem& crop = items_.at(sourceIndex);
            const int row = listTable_->rowCount();
            listTable_->insertRow(row);
            auto* thumb = new QTableWidgetItem;
            thumb->setIcon(makeThumbnailIcon(crop, 44));
            thumb->setData(kSourceIndexRole, sourceIndex);
            listTable_->setItem(row, 0, thumb);
            const QStringList values = {
                QFileInfo(crop.cropPath).fileName(),
                displayLabel(effectiveLabel(crop)),
                crop.reviewState.isEmpty() ? "unreviewed" : crop.reviewState,
            };
            for (int col = 0; col < values.size(); ++col) {
                auto* tableItem = new QTableWidgetItem(values.at(col));
                tableItem->setData(kSourceIndexRole, sourceIndex);
                if (col == 1)
                    tableItem->setForeground(reviewBaseColor(effectiveLabel(crop)));
                listTable_->setItem(row, col + 1, tableItem);
            }
        }
    }

    void refreshVisibleRowForSourceIndex(int sourceIndex) {
        const int visibleRow = visibleIndexes_.indexOf(sourceIndex);
        if (visibleRow < 0 || sourceIndex < 0 || sourceIndex >= items_.size())
            return;

        const CropItem& crop = items_.at(sourceIndex);
        const int activeIndex = browserStack_ ? browserStack_->currentIndex() : 0;
        if (activeIndex == 0) {
            if (!gridPageDirty_) {
                if (QListWidgetItem* item = gridList_->item(visibleRow)) {
                    const QString tileText = tileDisplayText(crop);
                    item->setIcon(makeThumbnailIcon(crop, kIconSize));
                    item->setText(tileText);
                    item->setData(kTileDisplayTextRole, tileText);
                    item->setToolTip(QString("%1\n%2").arg(crop.imageId, QFileInfo(crop.cropPath).fileName()));
                    item->setForeground(reviewBaseColor(effectiveLabel(crop)));
                }
            }
            tablePageDirty_ = true;
            return;
        }

        if (!tablePageDirty_ && visibleRow < listTable_->rowCount()) {
            if (QTableWidgetItem* thumb = listTable_->item(visibleRow, 0)) {
                thumb->setIcon(makeThumbnailIcon(crop, 44));
                thumb->setData(kSourceIndexRole, sourceIndex);
            }
            const QStringList values = {
                QFileInfo(crop.cropPath).fileName(),
                displayLabel(effectiveLabel(crop)),
                crop.reviewState.isEmpty() ? "unreviewed" : crop.reviewState,
            };
            for (int col = 0; col < values.size(); ++col) {
                QTableWidgetItem* tableItem = listTable_->item(visibleRow, col + 1);
                if (!tableItem) {
                    tableItem = new QTableWidgetItem;
                    listTable_->setItem(visibleRow, col + 1, tableItem);
                }
                tableItem->setText(values.at(col));
                tableItem->setData(kSourceIndexRole, sourceIndex);
                if (col == 1)
                    tableItem->setForeground(reviewBaseColor(effectiveLabel(crop)));
                else
                    tableItem->setForeground(QBrush());
            }
        }
        gridPageDirty_ = true;
    }

    QString thumbnailCacheKey(const QString& path, int size) const {
        const QFileInfo info(path);
        return QString("%1|%2|%3|%4")
            .arg(info.absoluteFilePath())
            .arg(info.size())
            .arg(info.lastModified().toMSecsSinceEpoch())
            .arg(size);
    }

    QPixmap thumbnailBasePixmap(const QString& path, int size) const {
        const QString cacheKey = thumbnailCacheKey(path, size);
        if (QPixmap* cached = thumbnailCache_.object(cacheKey))
            return *cached;

        QPixmap pix(size, size);
        pix.fill(QColor("#111827"));
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize decodeSize = scaledToFit(reader.size(), QSize(size - 8, size - 8));
        if (decodeSize.isValid() && !decodeSize.isEmpty())
            reader.setScaledSize(decodeSize);
        const QImage img = reader.read();
        QPainter painter(&pix);
        if (!img.isNull()) {
            const QPixmap source = QPixmap::fromImage(img).scaled(QSize(size - 8, size - 8), Qt::KeepAspectRatio,
                                                                  Qt::SmoothTransformation);
            const QPoint topLeft((size - source.width()) / 2, (size - source.height()) / 2);
            painter.drawPixmap(topLeft, source);
        } else {
            painter.setPen(QColor("#9ca3af"));
            painter.drawText(pix.rect(), Qt::AlignCenter, "unreadable");
        }
        const int cacheCostKb = std::max(1, pix.width() * pix.height() * std::max(1, pix.depth()) / 8 / 1024);
        thumbnailCache_.insert(cacheKey, new QPixmap(pix), cacheCostKb);
        return pix;
    }

    QIcon makeThumbnailIcon(const CropItem& crop, int size) const {
        const QString path = absoluteCropPath(crop);
        QPixmap pix = thumbnailBasePixmap(path, size);
        QPainter painter(&pix);
        QPen pen(reviewBaseColor(effectiveLabel(crop)));
        pen.setWidth(4);
        painter.setPen(pen);
        painter.drawRect(pix.rect().adjusted(2, 2, -3, -3));
        return QIcon(pix);
    }

    QString absoluteCropPath(const CropItem& crop) const {
        if (crop.cropPath.trimmed().isEmpty())
            return {};
        if (QFileInfo(crop.cropPath).isAbsolute())
            return crop.cropPath;
        const QString datasetPath = QDir(datasetRoot_).filePath(crop.cropPath);
        if (QFileInfo::exists(datasetPath) || manifestPath_.isEmpty())
            return datasetPath;
        return QDir(QFileInfo(manifestPath_).absolutePath()).filePath(crop.cropPath);
    }

    bool hasDisplayableCropPath(const CropItem& crop) const {
        const QString path = absoluteCropPath(crop);
        return !path.isEmpty() && QFileInfo::exists(path);
    }

    void setSelectionByVisibleRow(int visibleRow) {
        QSignalBlocker gridBlocker(gridList_);
        QSignalBlocker tableBlocker(listTable_);
        if (!gridPageDirty_)
            gridList_->clearSelection();
        if (!tablePageDirty_)
            listTable_->clearSelection();
        if (visibleRow < 0 || visibleRow >= visibleIndexes_.size()) {
            selectedSourceIndex_ = -1;
            return;
        }
        selectedSourceIndex_ = visibleIndexes_.at(visibleRow);
        if (!gridPageDirty_ && gridList_->item(visibleRow))
            gridList_->item(visibleRow)->setSelected(true);
        if (!tablePageDirty_ && visibleRow < listTable_->rowCount())
            listTable_->selectRow(visibleRow);
    }

    void selectFromGrid() {
        const QList<QListWidgetItem*> selected = gridList_->selectedItems();
        if (selected.isEmpty() || !selected.first()->data(kSourceIndexRole).isValid())
            return;
        selectedSourceIndex_ = selected.first()->data(kSourceIndexRole).toInt();
        syncTableSelection();
        updatePreview();
        updateReviewControls();
    }

    void selectFromTable() {
        const int row = listTable_->currentRow();
        if (row < 0 || !listTable_->item(row, 0))
            return;
        selectedSourceIndex_ = listTable_->item(row, 0)->data(kSourceIndexRole).toInt();
        syncGridSelection();
        updatePreview();
        updateReviewControls();
    }

    void syncGridSelection() {
        if (gridPageDirty_)
            return;
        QSignalBlocker blocker(gridList_);
        gridList_->clearSelection();
        const int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (row >= 0 && gridList_->item(row)) {
            gridList_->item(row)->setSelected(true);
            gridList_->scrollToItem(gridList_->item(row), QAbstractItemView::PositionAtCenter);
        }
    }

    void syncTableSelection() {
        if (tablePageDirty_)
            return;
        QSignalBlocker blocker(listTable_);
        listTable_->clearSelection();
        const int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (row >= 0 && row < listTable_->rowCount()) {
            listTable_->selectRow(row);
            listTable_->scrollToItem(listTable_->item(row, 0), QAbstractItemView::PositionAtCenter);
        }
    }

    void selectRelative(int delta) {
        if (filteredIndexes_.isEmpty())
            return;
        int row = filteredRowForSourceIndex(selectedSourceIndex_);
        if (row < 0)
            row = 0;
        row = std::clamp(row + delta, 0, static_cast<int>(filteredIndexes_.size()) - 1);
        const int targetSourceIndex = filteredIndexes_.at(row);
        const int targetPage = pageForFilteredRow(row);
        if (targetPage == currentPage_) {
            const int visibleRow = visibleIndexes_.indexOf(targetSourceIndex);
            setSelectionByVisibleRow(visibleRow);
            updatePreview();
            updateReviewControls();
            return;
        }
        currentPage_ = targetPage;
        showCurrentPage(targetSourceIndex, true);
    }

    void applyReviewLabel(const QString& label) {
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size())
            return;
        const int previousFilteredRow = filteredRowForSourceIndex(selectedSourceIndex_);
        const int previousSourceIndex = selectedSourceIndex_;
        CropItem& crop = items_[selectedSourceIndex_];
        const QString previousEffectiveLabel = effectiveLabel(crop);
        const bool wasDisplayable = hasDisplayableCropPath(crop);
        undoStack_.push_back({crop.manifestIndex, crop.json});
        crop.manualLabel = canonicalLabel(label);
        crop.reviewState = crop.manualLabel == "exclude"
                               ? "excluded"
                               : (crop.manualLabel == canonicalLabel(crop.autoLabel) ? "confirmed" : "relabeled");
        crop.excludeReason = crop.manualLabel == "exclude" ? hiddenExcludeReason(crop.excludeReason) : QString();
        crop.json = cropToJson(crop);
        const bool remainsInCurrentFilter = cropMatchesFilter(crop);
        saveItemToManifest(crop);
        scheduleManifestAutosave();
        if (remainsInCurrentFilter) {
            adjustCountsForLabelChange(previousEffectiveLabel, effectiveLabel(crop), wasDisplayable);
        } else {
            applyFilters(false);
        }
        advanceAfterReview(previousSourceIndex, previousFilteredRow, !remainsInCurrentFilter);
    }

    void undoLastLabelChange() {
        if (undoStack_.isEmpty())
            return;
        const UndoEntry undo = undoStack_.takeLast();
        if (undo.manifestIndex < 0 || undo.manifestIndex >= items_.size())
            return;
        CropItem& crop = items_[undo.manifestIndex];
        crop.json = undo.previousItem;
        crop.manualLabel = canonicalLabel(undo.previousItem.value("reviewed_label").toString());
        crop.reviewState =
            undo.previousItem.value("review_state").toString(crop.manualLabel.isEmpty() ? "unreviewed" : "confirmed");
        crop.excludeReason = undo.previousItem.value("exclude_reason").toString();
        saveItemToManifest(crop);
        scheduleManifestAutosave();
        applyFilters();
    }

    void advanceAfterReview(int previousSourceIndex, int previousFilteredRow, bool rebuildPage) {
        if (filteredIndexes_.isEmpty()) {
            showCurrentPage(-1, true);
            return;
        }
        int targetRow = filteredRowForSourceIndex(previousSourceIndex);
        if (targetRow >= 0)
            ++targetRow;
        else
            targetRow = previousFilteredRow;
        targetRow = std::clamp(targetRow, 0, static_cast<int>(filteredIndexes_.size()) - 1);
        const int targetSourceIndex = filteredIndexes_.at(targetRow);
        const int targetPage = pageForFilteredRow(targetRow);
        if (rebuildPage || targetPage != currentPage_) {
            currentPage_ = targetPage;
            showCurrentPage(targetSourceIndex, true);
            return;
        }

        refreshVisibleRowForSourceIndex(previousSourceIndex);
        const int targetVisibleRow = visibleIndexes_.indexOf(targetSourceIndex);
        setSelectionByVisibleRow(targetVisibleRow);
        refreshCountsUi();
        updatePreview();
        updateReviewControls();
    }

    void saveItemToManifest(const CropItem& crop) {
        if (crop.manifestIndex < 0)
            return;
        pendingManifestItemUpdates_.insert(crop.manifestIndex, crop.json);
    }

    void applyPendingItemUpdates(QJsonObject& root) const {
        QJsonArray rows = root.value("items").toArray();
        for (auto it = pendingManifestItemUpdates_.cbegin(); it != pendingManifestItemUpdates_.cend(); ++it) {
            while (rows.size() <= it.key())
                rows.append(QJsonObject());
            rows.replace(it.key(), it.value());
        }
        root["items"] = rows;
    }

    void prepareManifestRootForSave(QJsonObject& root) const {
        applyPendingItemUpdates(root);
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!root.contains("schema_version"))
            root["schema_version"] = "dataset-builder-manifest-v1";
        if (!root.contains("dataset_id"))
            root["dataset_id"] = QFileInfo(datasetRoot_).fileName();
        storeClassSchema(root);
    }

    bool autosaveManifest() {
        if (manifestPath_.isEmpty() || !manifestDoc_.isObject())
            return false;
        QJsonObject root = manifestDoc_.object();
        prepareManifestRootForSave(root);
        const QJsonDocument savedDoc(root);
        QDir().mkpath(QFileInfo(manifestPath_).absolutePath());
        QFile file(manifestPath_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            if (statusLabel_)
                statusLabel_->setText("Could not save the image set file: " + QDir::toNativeSeparators(manifestPath_));
            return false;
        }
        file.write(savedDoc.toJson(QJsonDocument::Indented));
        manifestDoc_ = savedDoc;
        pendingManifestItemUpdates_.clear();
        if (statusLabel_)
            statusLabel_->setText("Saved image-set labels to " + QDir::toNativeSeparators(manifestPath_));
        if (manifestPathEdit_)
            manifestPathEdit_->setText(QDir::toNativeSeparators(manifestPath_));
        return true;
    }

    void scheduleManifestAutosave() {
        if (statusLabel_)
            statusLabel_->setText("Label applied. Saving image-set file shortly...");
        if (manifestAutosaveTimer_)
            manifestAutosaveTimer_->start();
        else
            autosaveManifest();
    }

    bool flushPendingManifestSave() {
        if (manifestAutosaveTimer_)
            manifestAutosaveTimer_->stop();
        if (pendingManifestItemUpdates_.isEmpty())
            return true;
        return autosaveManifest();
    }

    void updateAll() {
        applyFilters();
    }

    void adjustCountBucket(const QString& label, int delta) {
        if (label == "0")
            class0Count_ += delta;
        else if (label == "1")
            class1Count_ += delta;
        else if (label == "2")
            class2Count_ += delta;
        else if (label == "exclude")
            excludedCount_ += delta;
        else
            unreviewedCount_ += delta;
    }

    void adjustCountsForLabelChange(const QString& oldLabel, const QString& newLabel, bool displayable) {
        if (!displayable || oldLabel == newLabel)
            return;
        adjustCountBucket(oldLabel, -1);
        adjustCountBucket(newLabel, 1);
    }

    void updateCounts() {
        class0Count_ = 0;
        class1Count_ = 0;
        class2Count_ = 0;
        excludedCount_ = 0;
        unreviewedCount_ = 0;
        for (const CropItem& crop : items_) {
            if (!hasDisplayableCropPath(crop))
                continue;
            adjustCountBucket(effectiveLabel(crop), 1);
        }
        refreshCountsUi();
    }

    void refreshCountsUi() {
        const int reviewed = class0Count_ + class1Count_ + class2Count_ + excludedCount_;
        const int total = reviewed + unreviewedCount_;
        if (totalMetric_)
            totalMetric_->setText(QString::number(total));
        if (reviewedMetric_)
            reviewedMetric_->setText(QString::number(reviewed));
        if (reviewedSubMetric_) {
            reviewedSubMetric_->setText(total == 0 ? "0%" : QString("%1%").arg(reviewed * 100 / total));
        }
        setFilterText(FilterMode::All, "All", total);
        setFilterText(FilterMode::Class0, classEntries_.value(0).displayName, class0Count_);
        setFilterText(FilterMode::Class1, classEntries_.value(1).displayName, class1Count_);
        if (classEntries_.size() >= 3)
            setFilterText(FilterMode::Class2, classEntries_.value(2).displayName, class2Count_);
        setFilterText(FilterMode::Excluded, "Excluded", excludedCount_);
        setFilterText(FilterMode::Unreviewed, "Unreviewed", unreviewedCount_);
        if (centerPanelTitle_)
            centerPanelTitle_->setText(filterTitle());
        if (centerPanelSubtitle_) {
            if (filteredIndexes_.isEmpty()) {
                centerPanelSubtitle_->setText("0 shown");
            } else {
                const int first = currentPage_ * pageSize_ + 1;
                const int last = first + visibleIndexes_.size() - 1;
                centerPanelSubtitle_->setText(
                    QString("Showing %1-%2 of %3 filtered").arg(first).arg(last).arg(filteredIndexes_.size()));
            }
        }
    }

    void setFilterText(FilterMode mode, const QString& label, int count) {
        if (auto* button = filterButtons_.value(mode, nullptr))
            button->setText(QString("%1 (%2)").arg(label).arg(count));
    }

    QString filterTitle() const {
        switch (filterMode_) {
        case FilterMode::Class0:
            return classEntries_.value(0).displayName + " images";
        case FilterMode::Class1:
            return classEntries_.value(1).displayName + " images";
        case FilterMode::Class2:
            return classEntries_.size() >= 3 ? classEntries_.value(2).displayName + " images" : "Second non-target images";
        case FilterMode::Excluded:
            return excludedLabelDisplay_ + " images";
        case FilterMode::Unreviewed:
            return "Unreviewed images";
        case FilterMode::All:
            return "All images";
        }
        return "All images";
    }

    void updatePreview() {
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size()) {
            previewLabel_->setPixmap(QPixmap());
            previewLabel_->setText("No image selected");
            filenameLabel_->setText("--");
            autoLabel_->setText("--");
            manualLabel_->setText("--");
            manualLabel_->setStyleSheet(QString());
            frameLabel_->setText("--");
            timestampLabel_->setText("--");
            return;
        }
        const CropItem& crop = items_.at(selectedSourceIndex_);
        filenameLabel_->setText(QFileInfo(crop.cropPath).fileName());
        manualLabel_->setText(displayLabel(effectiveLabel(crop)));
        manualLabel_->setStyleSheet(QStringLiteral("color:%1; font-weight:650;")
                                        .arg(reviewBaseColor(effectiveLabel(crop)).name(QColor::HexRgb)));
        autoLabel_->setText(crop.reviewState.isEmpty() ? "unreviewed" : crop.reviewState);
        frameLabel_->setText(crop.frameNumber.isEmpty() ? "--" : crop.frameNumber);
        timestampLabel_->setText(crop.timestamp.isEmpty() ? "--" : crop.timestamp);
        QImageReader reader(absoluteCropPath(crop));
        reader.setAutoTransform(true);
        const QSize previewSize = previewLabel_->size();
        const QSize decodeSize = scaledToFit(reader.size(), previewSize);
        if (decodeSize.isValid() && !decodeSize.isEmpty())
            reader.setScaledSize(decodeSize);
        const QImage image = reader.read();
        if (image.isNull()) {
            previewLabel_->setPixmap(QPixmap());
            previewLabel_->setText("Preview unavailable\n" + QFileInfo(crop.cropPath).fileName());
        } else {
            previewLabel_->setText(QString());
            previewLabel_->setPixmap(
                QPixmap::fromImage(image).scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    void updateReviewControls() {
        const bool hasSelection = selectedSourceIndex_ >= 0 && selectedSourceIndex_ < items_.size();
        const bool canEditSchema = manifestDoc_.isObject() && isDatasetBuilderManifest(manifestDoc_.object());
        if (classModeCombo_)
            classModeCombo_->setEnabled(canEditSchema);
        if (classZeroEdit_)
            classZeroEdit_->setEnabled(canEditSchema);
        if (classOneEdit_)
            classOneEdit_->setEnabled(canEditSchema);
        if (classTwoEdit_)
            classTwoEdit_->setEnabled(canEditSchema && classEntries_.size() >= 3);
        if (classZeroColorButton_)
            classZeroColorButton_->setEnabled(canEditSchema);
        if (classOneColorButton_)
            classOneColorButton_->setEnabled(canEditSchema);
        if (classTwoColorButton_)
            classTwoColorButton_->setEnabled(canEditSchema && classEntries_.size() >= 3);
        hitButton_->setEnabled(hasSelection);
        wasteButton_->setEnabled(hasSelection);
        if (classThreeButton_)
            classThreeButton_->setEnabled(hasSelection && classEntries_.size() >= 3);
        excludeButton_->setEnabled(hasSelection);
        undoButton_->setEnabled(!undoStack_.isEmpty());
        const int row = filteredRowForSourceIndex(selectedSourceIndex_);
        previousButton_->setEnabled(row > 0);
        nextButton_->setEnabled(row >= 0 && row < filteredIndexes_.size() - 1);
    }

    void maybeRunVerifier() {
        const QString manifest = qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed();
        if (manifest.isEmpty())
            return;
        QTimer::singleShot(0, this, [this, manifest]() {
            const QString outputPath = qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_OUT").trimmed();
            QJsonObject result = runVerifier(manifest);
            if (!outputPath.isEmpty()) {
                QDir().mkpath(QFileInfo(outputPath).absolutePath());
                QFile out(outputPath);
                if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    out.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
                }
            }
            if (qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_QUIT") == "1") {
                const int exitCode = result.value("ok").toBool() ? 0 : 2;
                qApp->setProperty("ovdsDatasetWorkspaceVerifyExitCode", exitCode);
            }
        });
    }

    QJsonObject runVerifier(const QString& manifest) {
        QStringList failures;
        auto expect = [&failures](bool condition, const QString& message) {
            if (!condition)
                failures.push_back(message);
        };
        auto readFileBytes = [](const QString& path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return QByteArray();
            return file.readAll();
        };
        auto sourceRootForManifest = [](const QString& manifestPath) {
            QFileInfo info(manifestPath);
            QString root = info.absolutePath();
            if (info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
                QDir dir(info.dir());
                if (dir.cdUp())
                    root = dir.absolutePath();
            }
            return root;
        };
        qint64 firstLabelElapsedMs = -1;
        bool pendingSaveDebounceObserved = false;
        auto expectedDefaultColor = [](const QString& classId) {
            if (classId == "0")
                return QColor("#FF0000");
            if (classId == "1")
                return QColor("#00FF00");
            if (classId == "2")
                return QColor("#0000FF");
            return QColor();
        };
        auto expectDefaultPalette = [&expect, &expectedDefaultColor](desktop_app::theme::ThemeMode mode,
                                                                     const QString& modeName) {
            for (const QString& classId : {QString("0"), QString("1"), QString("2")}) {
                const QColor actual = desktop_app::theme::reviewClassColors(classId, mode).fill;
                const QColor expectedColor = expectedDefaultColor(classId);
                expect(actual == expectedColor,
                       QString("%1 mode default color for class %2 expected %3, got %4")
                           .arg(modeName)
                           .arg(classId)
                           .arg(expectedColor.name(QColor::HexRgb))
                           .arg(actual.name(QColor::HexRgb)));
            }
        };
        QTemporaryDir verifierInputRoot;
        expect(verifierInputRoot.isValid(), "dataset verifier temp input directory could not be created");
        auto normalizeClasses = [&expectedDefaultColor](QJsonArray classes) {
            for (int i = 0; i < classes.size(); ++i) {
                QJsonObject cls = classes.at(i).toObject();
                const QString id = cls.value("id").toVariant().toString().trimmed();
                const QColor color = expectedDefaultColor(id);
                if (color.isValid()) {
                    cls["display_color"] = color.name(QColor::HexRgb);
                    cls["color"] = color.name(QColor::HexRgb);
                    classes.replace(i, cls);
                }
            }
            return classes;
        };
        auto copyRelativeImage = [&expect](const QString& sourceRoot, const QString& destRoot, const QString& relativePath) {
            const QString trimmed = relativePath.trimmed();
            if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
                return;
            const QString sourcePath = QDir(sourceRoot).filePath(trimmed);
            if (!QFileInfo::exists(sourcePath))
                return;
            const QString destPath = QDir(destRoot).filePath(trimmed);
            QDir().mkpath(QFileInfo(destPath).absolutePath());
            if (QFileInfo::exists(destPath))
                QFile::remove(destPath);
            expect(QFile::copy(sourcePath, destPath),
                   QString("failed to copy verifier image %1 to %2").arg(sourcePath, destPath));
        };
        auto makeVerifierWorkingManifest = [&](const QString& sourceManifest) {
            if (!verifierInputRoot.isValid())
                return sourceManifest;
            QFile file(sourceManifest);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                expect(false, QString("failed to open verifier source manifest %1").arg(sourceManifest));
                return sourceManifest;
            }
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                expect(false, QString("failed to parse verifier source manifest %1").arg(sourceManifest));
                return sourceManifest;
            }

            QJsonObject root = doc.object();
            QJsonObject schema = root.value("class_schema").toObject();
            QJsonArray classes = schema.value("classes").toArray();
            if (!classes.isEmpty()) {
                classes = normalizeClasses(classes);
                schema["classes"] = classes;
                root["class_schema"] = schema;
                root["classes"] = classes;
            } else {
                QJsonArray rootClasses = root.value("classes").toArray();
                if (!rootClasses.isEmpty()) {
                    rootClasses = normalizeClasses(rootClasses);
                    root["classes"] = rootClasses;
                    schema["classes"] = rootClasses;
                    root["class_schema"] = schema;
                }
            }

            const QString sourceRoot = sourceRootForManifest(sourceManifest);
            const QString destRoot = verifierInputRoot.path();
            const QJsonArray rows = root.value("items").toArray();
            for (const QJsonValue& value : rows) {
                const QJsonObject item = value.toObject();
                copyRelativeImage(sourceRoot, destRoot,
                                  firstNonEmptyString(item, {"crop_path", "path", "image_path", "relative_path"}));
            }

            const QString workingManifest = QDir(destRoot).filePath("manifest.json");
            QFile out(workingManifest);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                expect(false, QString("failed to write verifier working manifest %1").arg(workingManifest));
                return sourceManifest;
            }
            out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            return workingManifest;
        };
        auto thumbnailBorderColor = [this](const CropItem& crop) {
            const QImage image = makeThumbnailIcon(crop, 44).pixmap(44, 44).toImage();
            if (image.isNull())
                return QColor();
            return image.pixelColor(2, image.height() / 2);
        };
        auto expectedSemanticColor = [this](const QString& classId) {
            const QString canonical = canonicalLabel(classId);
            if (canonical == "0" || canonical == "1" || canonical == "2")
                return desktop_app::theme::semanticClassColorForBase(classColorForId(canonical), currentThemeMode());
            return desktop_app::theme::semanticClassColor(canonical, currentThemeMode());
        };
        auto expectGridLabel = [this, &expect](int row, const QString& expectedText) {
            if (!gridList_ || row < 0 || row >= gridList_->count()) {
                expect(false, QString("grid row %1 is outside the visible grid").arg(row));
                return;
            }
            const QListWidgetItem* item = gridList_->item(row);
            const QString text = item ? item->text().trimmed() : QString();
            expect(!text.isEmpty(), QString("grid row %1 label is empty").arg(row));
            expect(text != "...", QString("grid row %1 label is still ellipsis-only").arg(row));
            expect(text == item->data(kTileDisplayTextRole).toString(),
                   QString("grid row %1 display role does not match visible text").arg(row));
            if (!expectedText.isEmpty()) {
                expect(text == expectedText,
                       QString("grid row %1 label expected %2, got %3").arg(row).arg(expectedText).arg(text));
            }
        };
        auto expectReviewColorMatch =
            [this, &expect, &thumbnailBorderColor, &expectedSemanticColor](QPushButton* button,
                                                                           const QString& expectedClassId,
                                                                           int sourceIndex, const QString& context) {
                expect(button != nullptr, context + " button was not created");
                if (!button)
                    return;
                const QString reviewClassId = button->property("reviewClassId").toString();
                expect(reviewClassId == expectedClassId,
                       QString("%1 button reviewClassId expected %2, got %3")
                           .arg(context)
                           .arg(expectedClassId)
                           .arg(reviewClassId));
                if (sourceIndex < 0 || sourceIndex >= items_.size()) {
                    expect(false,
                           QString("%1 source index %2 is outside the loaded item range")
                               .arg(context)
                               .arg(sourceIndex));
                    return;
                }
                const QColor expectedColor = reviewBaseColor(reviewClassId);
                const QColor semanticColor = expectedSemanticColor(reviewClassId);
                expect(semanticColor == expectedColor,
                       QString("%1 semantic class color expected %2, got %3")
                           .arg(context)
                           .arg(expectedColor.name(QColor::HexRgb))
                           .arg(semanticColor.name(QColor::HexRgb)));
                const QString buttonColorHex = button->property("reviewClassColorHex").toString();
                expect(buttonColorHex == expectedColor.name(QColor::HexRgb),
                       QString("%1 button color property expected %2, got %3")
                           .arg(context)
                           .arg(expectedColor.name(QColor::HexRgb))
                           .arg(buttonColorHex));
                const QColor actualColor = thumbnailBorderColor(items_.at(sourceIndex));
                expect(actualColor == expectedColor,
                       QString("%1 thumbnail border color expected %2, got %3")
                           .arg(context)
                           .arg(expectedColor.name(QColor::HexRgb))
                           .arg(actualColor.name(QColor::HexRgb)));
            };

        expectDefaultPalette(desktop_app::theme::ThemeMode::Light, "light");
        expectDefaultPalette(desktop_app::theme::ThemeMode::Dark, "dark");

        const QString verifierManifest = makeVerifierWorkingManifest(manifest);
        const QByteArray manifestBytesBeforeLoad = readFileBytes(verifierManifest);
        loadDatasetPath(verifierManifest);
        expect(readFileBytes(manifestPath_) == manifestBytesBeforeLoad,
               "Loading the image set unexpectedly mutated the manifest before any label changes");
        expect(items_.size() == 5, QString("expected 5 manifest rows, got %1").arg(items_.size()));
        expect(filteredIndexes_.size() == 4,
               QString("expected 4 displayable crops, got %1").arg(filteredIndexes_.size()));
        expect(gridList_->count() == 4, QString("expected 4 grid thumbnails, got %1").arg(gridList_->count()));
        expect(gridList_->textElideMode() == Qt::ElideNone, "grid labels still use Qt eliding");
        expectGridLabel(0, QString());
        expect(pageSizeCombo_ != nullptr, "page-size combo was not created");
        if (pageSizeCombo_) {
            const QStringList expectedPageSizes = {"100", "200", "300"};
            expect(pageSizeCombo_->count() == expectedPageSizes.size(),
                   QString("page-size combo expected %1 entries, got %2")
                       .arg(expectedPageSizes.size())
                       .arg(pageSizeCombo_->count()));
            expect(pageSizeCombo_->currentText() == "100",
                   QString("default page-size text expected 100, got %1").arg(pageSizeCombo_->currentText()));
            expect(pageSizeCombo_->sizeAdjustPolicy() == QComboBox::AdjustToContents,
                   "page-size combo does not adjust to contents");
            expect(pageSizeCombo_->minimumContentsLength() >= 3,
                   QString("page-size combo minimum contents length expected at least 3, got %1")
                       .arg(pageSizeCombo_->minimumContentsLength()));
            expect(pageSizeCombo_->width() >= pageSizeCombo_->minimumSizeHint().width(),
                   QString("page-size combo width %1 is narrower than minimum size hint %2")
                       .arg(pageSizeCombo_->width())
                       .arg(pageSizeCombo_->minimumSizeHint().width()));
            auto* pageSizeView = pageSizeCombo_->view();
            expect(pageSizeView != nullptr, "page-size combo popup view is missing");
            if (pageSizeView)
                expect(pageSizeView->textElideMode() == Qt::ElideNone, "page-size combo popup text is still elided");
            for (int i = 0; i < expectedPageSizes.size() && i < pageSizeCombo_->count(); ++i) {
                expect(pageSizeCombo_->itemText(i) == expectedPageSizes.at(i),
                       QString("page-size option %1 expected %2, got %3")
                           .arg(i)
                           .arg(expectedPageSizes.at(i), pageSizeCombo_->itemText(i)));
            }
        }
        expect(centerPanelSubtitle_->text() == "Showing 1-4 of 4 filtered",
               "center subtitle did not report the current filtered page range");
        auto* datasetSplitter = findChild<QSplitter*>("DatasetWorkspaceSplitter");
        expect(datasetSplitter != nullptr, "Dataset workspace splitter was not created");
        QList<int> splitterSizesBeforeFilter;
        if (datasetSplitter) {
            expect(datasetSplitter->count() == 3,
                   QString("Dataset splitter expected 3 sections, got %1").arg(datasetSplitter->count()));
            expect(!datasetSplitter->childrenCollapsible(), "Dataset splitter sections are collapsible");
            expect(datasetSplitter->widget(0)->maximumWidth() == QWIDGETSIZE_MAX,
                   "Image Set section still has a fixed maximum width");
            expect(datasetSplitter->widget(2)->maximumWidth() == QWIDGETSIZE_MAX,
                   "Review section still has a fixed maximum width");
            datasetSplitter->setSizes({300, 620, 380});
            QApplication::processEvents();
            splitterSizesBeforeFilter = datasetSplitter->sizes();
            applyFilters();
            QApplication::processEvents();
            expect(datasetSplitter->sizes() == splitterSizesBeforeFilter,
                   "Dataset splitter sizes changed after applying filters");
        }
        expect(!filteredIndexes_.contains(1), "manifest row with empty crop_path is still visible");
        expect(hitButton_->text() == classEntries_.value(0).displayName,
               QString("Class 0 button text expected %1, got %2")
                   .arg(classEntries_.value(0).displayName)
                   .arg(hitButton_->text()));
        expect(wasteButton_->text() == classEntries_.value(1).displayName,
               QString("Class 1 button text expected %1, got %2")
                   .arg(classEntries_.value(1).displayName)
                   .arg(wasteButton_->text()));
        expect(classZeroColorButton_ != nullptr, "Class 0 color selector was not created");
        expect(classOneColorButton_ != nullptr, "Class 1 color selector was not created");
        expect(classTwoColorButton_ != nullptr, "Class 2 color selector was not created");
        expect(classColorHexForId("0") == QString("#ff0000"),
               QString("Default class 0 color expected #ff0000, got %1").arg(classColorHexForId("0")));
        expect(classColorHexForId("1") == QString("#00ff00"),
               QString("Default class 1 color expected #00ff00, got %1").arg(classColorHexForId("1")));
        expect(findChild<QObject*>("DatasetWorkspaceExcludeReasonCombo") == nullptr, 
               "Legacy exclude selector is still present"); 
        const QMenu* browseMenu = browseButton_ ? browseButton_->menu() : nullptr;
        expect(browseMenu != nullptr, "Browse button does not own an app menu");
        if (browseMenu) {
            QStringList browseActions;
            for (const QAction* action : browseMenu->actions()) {
                if (action)
                    browseActions << action->text().trimmed();
            }
            expect(!browseActions.contains("Open Image Set Folder..."),
                   "Browse menu still includes the removed folder option.");
            expect(browseActions.contains("Open Image Set File..."),
                   "Browse menu is missing Open Image Set File...");
        }

        const QColor customClassZero("#D93636");
        const QColor customClassOne("#19C46B");
        setClassColor(0, customClassZero);
        setClassColor(1, customClassOne);
        expect(classColorHexForId("0") == customClassZero.name(QColor::HexRgb),
               "Class 0 color change did not update the color source");
        expect(classColorHexForId("1") == customClassOne.name(QColor::HexRgb),
               "Class 1 color change did not update the color source");
        const QByteArray manifestBytesBeforeLabels = readFileBytes(manifestPath_);

        setSelectionByVisibleRow(0); 
        QElapsedTimer firstLabelTimer;
        firstLabelTimer.start();
        applyReviewLabel("0"); 
        firstLabelElapsedMs = firstLabelTimer.elapsed();
        expect(selectedSourceIndex_ == 2, "Class 0 review did not advance to the next visible crop"); 
        pendingSaveDebounceObserved = pendingManifestItemUpdates_.contains(0);
        expect(pendingSaveDebounceObserved, "Class 0 review was not queued for deferred manifest save");
        expect(readFileBytes(manifestPath_) == manifestBytesBeforeLabels,
               "Class 0 review wrote the manifest synchronously instead of waiting for the debounce flush");
        expect(firstLabelElapsedMs < 250,
               QString("Class 0 review took %1 ms; expected an instant in-memory/UI update").arg(firstLabelElapsedMs));
        expectReviewColorMatch(hitButton_, "0", 0, "Class 0");

        setSelectionByVisibleRow(1);
        applyReviewLabel("1");
        expect(selectedSourceIndex_ == 3, "Class 1 review did not advance to the next visible crop");
        expectReviewColorMatch(wasteButton_, "1", 2, "Class 1");

        const int ternaryIndex = classModeCombo_->findData(3);
        expect(ternaryIndex >= 0, "3-class mode option was not found");
        if (ternaryIndex >= 0)
            classModeCombo_->setCurrentIndex(ternaryIndex);
        expect(classEntries_.size() == 3, "3-class mode did not enable the third class");
        expect(classThreeButton_ && !classThreeButton_->isHidden(),
               "Third class button is not available in 3-class mode");

        classTwoEdit_->setText("Non-target B");
        handleClassLabelEdited(2);
        expect(classThreeButton_->text() == "Non-target B", "Class 2 label edit did not update the review button");
        const QColor customClassTwo("#2E6BFF");
        setClassColor(2, customClassTwo);
        expect(classColorHexForId("2") == customClassTwo.name(QColor::HexRgb),
               "Class 2 color change did not update the color source");

        setSelectionByVisibleRow(0);
        if (selectedSourceIndex_ >= 0 && selectedSourceIndex_ < items_.size())
            items_[selectedSourceIndex_].excludeReason = "partial_droplet";
        applyReviewLabel("exclude");
        expect(selectedSourceIndex_ == 2, "Exclude did not advance to the next visible crop");
        expectReviewColorMatch(excludeButton_, "exclude", 0, "Exclude");

        setSelectionByVisibleRow(2);
        applyReviewLabel("2");
        expect(selectedSourceIndex_ == 4, "Class 2 review did not advance after applying the label");
        expectReviewColorMatch(classThreeButton_, "2", 3, "Class 2");

        expect(flushPendingManifestSave(), "Pending label changes did not flush to the manifest");
        QFile file(manifestPath_);
        QJsonArray rows;
        QJsonObject savedRoot;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument saved = QJsonDocument::fromJson(file.readAll());
            savedRoot = saved.object();
            rows = savedRoot.value("items").toArray();
        }
        expect(rows.size() == 5, QString("saved manifest row count changed to %1").arg(rows.size()));
        if (rows.size() >= 5) {
            expect(rows.at(0).toObject().value("reviewed_label").toString() == "exclude",
                   "Exclude did not persist reviewed_label");
            expect(rows.at(0).toObject().value("exclude_reason").toString() == "partial_droplet",
                   "Hidden legacy exclude metadata did not persist");
            expect(rows.at(2).toObject().value("reviewed_label").toString() == "1",
                   "Class 1 review did not persist reviewed_label");
            expect(rows.at(3).toObject().value("reviewed_label").toString() == "2",
                   "Class 2 review did not persist reviewed_label");
        }
        const QJsonObject schema = savedRoot.value("class_schema").toObject();
        const QJsonArray classes = schema.value("classes").toArray();
        expect(schema.value("target_class_id").toString() == "1", "Target class id did not persist as 1");
        expect(classes.size() == 3, QString("expected 3 classes in saved schema, got %1").arg(classes.size()));
        if (classes.size() >= 3) {
            expect(classes.at(0).toObject().value("display_name").toString() == "Non-target A",
                   "Class 0 display label did not persist");
            expect(classes.at(1).toObject().value("display_name").toString() == "Target",
                   "Class 1 display label did not persist");
            expect(classes.at(2).toObject().value("display_name").toString() == "Non-target B",
                   "Class 2 display label did not persist");
            expect(classes.at(0).toObject().value("display_color").toString() == customClassZero.name(QColor::HexRgb),
                   "Class 0 display color did not persist");
            expect(classes.at(1).toObject().value("display_color").toString() == customClassOne.name(QColor::HexRgb),
                   "Class 1 display color did not persist");
            expect(classes.at(2).toObject().value("display_color").toString() == customClassTwo.name(QColor::HexRgb),
                   "Class 2 display color did not persist");
        }

        const QString reloadedManifestPath = manifestPath_;
        loadDatasetPath(reloadedManifestPath);
        filterMode_ = FilterMode::All;
        if (auto* allButton = filterButtons_.value(FilterMode::All, nullptr))
            allButton->setChecked(true);
        applyFilters();
        expect(classEntries_.size() == 3, "Reloaded builder manifest did not preserve 3-class mode");
        expect(classColorHexForId("0") == customClassZero.name(QColor::HexRgb),
               "Reloaded builder manifest lost class 0 color");
        expect(classColorHexForId("1") == customClassOne.name(QColor::HexRgb),
               "Reloaded builder manifest lost class 1 color");
        expect(classColorHexForId("2") == customClassTwo.name(QColor::HexRgb),
               "Reloaded builder manifest lost class 2 color");
        expect(classZeroColorButton_ && classZeroColorButton_->property("selectedColorHex").toString() ==
                                           customClassZero.name(QColor::HexRgb),
               "Class 0 color selector did not reload the saved color");
        expect(classOneColorButton_ && classOneColorButton_->property("selectedColorHex").toString() ==
                                          customClassOne.name(QColor::HexRgb),
               "Class 1 color selector did not reload the saved color");
        expect(classTwoColorButton_ && classTwoColorButton_->property("selectedColorHex").toString() ==
                                          customClassTwo.name(QColor::HexRgb),
               "Class 2 color selector did not reload the saved color");
        setSelectionByVisibleRow(2);
        expectReviewColorMatch(classThreeButton_, "2", 3, "Class 2 reload");

        const int builderVisibleCount = filteredIndexes_.size();
        const int builderGridCount = gridList_->count();
        const int builderSelectedSourceIndex = selectedSourceIndex_;
        const int builderClassMode = classEntries_.size();
        const int builderPageSize = pageSize_;
        const int builderCurrentPage = currentPage_ + 1;
        const int builderTotalPages = std::max(pageCount(), 1);
        const QString builderManifestPath = reloadedManifestPath;

        QJsonObject largeSuite;
        QTemporaryDir largeFixtureRoot;
        expect(largeFixtureRoot.isValid(), "large paging verifier fixture temp directory could not be created");
        if (largeFixtureRoot.isValid()) {
            const QString largeRoot = largeFixtureRoot.path();
            const QString largeCropDir = QDir(largeRoot).filePath("crops");
            QDir().mkpath(largeCropDir);
            QJsonArray largeRows;
            constexpr int kLargeFixtureCount = 305;
            for (int i = 0; i < kLargeFixtureCount; ++i) {
                const QString fileName = QString("large_%1.png").arg(i, 4, 10, QChar('0'));
                const QString relativePath = "crops/" + fileName;
                QImage image(6, 6, QImage::Format_ARGB32);
                image.fill(QColor::fromHsv(i % 360, 180, 220));
                const QString imagePath = QDir(largeRoot).filePath(relativePath);
                expect(image.save(imagePath), QString("failed to write large paging image %1").arg(imagePath));

                QJsonObject item;
                item["image_id"] = QString("large_%1").arg(i);
                item["crop_path"] = relativePath;
                item["auto_label"] = "unknown";
                item["review_state"] = "unreviewed";
                largeRows.append(item);
            }

            QJsonObject largeRootObject;
            largeRootObject["schema_version"] = "dataset-builder-manifest-v1";
            largeRootObject["dataset_id"] = "large_paging_fixture";
            largeRootObject["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            largeRootObject["items"] = largeRows;
            resetClassSchema(3);
            storeClassSchema(largeRootObject);
            const QString largeManifestPath = QDir(largeRoot).filePath("manifest.json");
            QFile largeManifestFile(largeManifestPath);
            expect(largeManifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
                   QString("failed to open large paging manifest %1").arg(largeManifestPath));
            if (largeManifestFile.isOpen()) {
                largeManifestFile.write(QJsonDocument(largeRootObject).toJson(QJsonDocument::Indented));
                largeManifestFile.close();
            }

            loadDatasetPath(largeManifestPath);
            expect(items_.size() == kLargeFixtureCount,
                   QString("large fixture expected %1 rows, got %2").arg(kLargeFixtureCount).arg(items_.size()));
            expect(filteredIndexes_.size() == kLargeFixtureCount,
                   QString("large fixture expected %1 filtered rows, got %2")
                       .arg(kLargeFixtureCount)
                       .arg(filteredIndexes_.size()));
            expect(pageSize_ == 100, QString("large fixture default page size expected 100, got %1").arg(pageSize_));
            expect(pageCount() == 4, QString("large fixture expected 4 pages at size 100, got %1").arg(pageCount()));
            expect(gridList_->count() == 100,
                   QString("large fixture first page expected 100 grid items, got %1").arg(gridList_->count()));

            pageNextButton_->click();
            expect(currentPage_ == 1, QString("next-page button expected page index 1, got %1").arg(currentPage_));
            expect(selectedSourceIndex_ == 100,
                   QString("next-page button expected selected source 100, got %1").arg(selectedSourceIndex_));
            pagePrevButton_->click();
            expect(currentPage_ == 0, QString("previous-page button expected page index 0, got %1").arg(currentPage_));

            if (pageButtonsLayout_ && pageButtonsLayout_->count() > 1) {
                if (auto* pageTwo = qobject_cast<QPushButton*>(pageButtonsLayout_->itemAt(1)->widget()))
                    pageTwo->click();
            }
            expect(currentPage_ == 1, QString("page-number button expected page index 1, got %1").arg(currentPage_));

            const int pageSize200Index = pageSizeCombo_ ? pageSizeCombo_->findData(200) : -1;
            expect(pageSize200Index >= 0, "page-size 200 option was not found");
            if (pageSize200Index >= 0)
                pageSizeCombo_->setCurrentIndex(pageSize200Index);
            expect(pageSize_ == 200, QString("page-size switch expected 200, got %1").arg(pageSize_));
            expect(pageSizeCombo_ && pageSizeCombo_->currentText() == "200",
                   QString("page-size combo selected text expected 200, got %1")
                       .arg(pageSizeCombo_ ? pageSizeCombo_->currentText() : QString("<null>")));
            expect(pageCount() == 2, QString("large fixture expected 2 pages at size 200, got %1").arg(pageCount()));
            expect(gridList_->count() == 200,
                   QString("large fixture 200-size page expected 200 grid items, got %1").arg(gridList_->count()));
            expectGridLabel(0, "large_0");
            expectGridLabel(199, "large_199");
            if (datasetSplitter && !splitterSizesBeforeFilter.isEmpty()) {
                expect(datasetSplitter->sizes() == splitterSizesBeforeFilter,
                       "Dataset splitter sizes changed after paging or page-size updates");
            }

            selectRelative(1);
            expect(selectedSourceIndex_ >= 1,
                   QString("relative next selection did not move forward; selected source %1").arg(selectedSourceIndex_));

            largeSuite["manifest_path"] = largeManifestPath;
            largeSuite["visible_count"] = filteredIndexes_.size();
            largeSuite["page_size"] = pageSize_;
            largeSuite["total_pages"] = pageCount();
            largeSuite["grid_count"] = gridList_->count();
            largeSuite["selected_source_index"] = selectedSourceIndex_;
        }

        QJsonObject legacySuite;
        QTemporaryDir legacyFixtureRoot;
        expect(legacyFixtureRoot.isValid(), "legacy verifier fixture temp directory could not be created");
        if (legacyFixtureRoot.isValid()) {
            auto writeFixtureImage = [&expect](const QString& path, const QColor& color) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                QImage image(14, 14, QImage::Format_ARGB32);
                image.fill(color);
                expect(image.save(path), QString("failed to write verifier image %1").arg(path));
            };
            auto makeClassObject = [](const QString& id, const QString& displayName, int index) {
                QJsonObject cls;
                cls["id"] = id;
                cls["index"] = index;
                cls["display_name"] = displayName;
                cls["folder"] = QString("reviewed/class_%1").arg(id);
                return cls;
            };
            auto makeLegacyRoot = [&](const QString& datasetId, int mode) {
                QJsonObject root;
                root["schema_version"] = "dataset-manifest-v1";
                root["dataset_id"] = datasetId;
                QJsonArray classes;
                classes.append(makeClassObject("0", mode >= 3 ? "Non-target A" : "Non-target", 0));
                classes.append(makeClassObject("1", "Target", 1));
                if (mode >= 3)
                    classes.append(makeClassObject("2", "Non-target B", 2));
                QJsonObject schema;
                schema["kind"] = mode >= 3 ? "target-nontarget-ternary" : "target-nontarget-binary";
                schema["mode"] = mode;
                schema["target_class_id"] = "1";
                schema["classes"] = classes;
                QJsonObject excluded;
                excluded["id"] = "exclude";
                excluded["display_name"] = "Exclude";
                excluded["folder"] = "reviewed/exclude";
                schema["excluded_label"] = excluded;
                root["class_schema"] = schema;
                root["classes"] = classes;
                return root;
            };
            auto writeLegacyFixture = [&](const QString& fixtureName, const QJsonObject& root,
                                          const QList<QPair<QString, QColor>>& images) {
                const QString fixtureRoot = QDir(legacyFixtureRoot.path()).filePath(fixtureName);
                const QString manifestDir = QDir(fixtureRoot).filePath("metadata");
                QDir().mkpath(manifestDir);
                for (const auto& image : images) {
                    const QString imagePath = QDir(fixtureRoot).filePath(image.first);
                    writeFixtureImage(imagePath, image.second);
                }
                const QString manifestPath = QDir(manifestDir).filePath("dataset_manifest.json");
                QFile manifestFile(manifestPath);
                expect(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
                       QString("failed to open verifier manifest %1").arg(manifestPath));
                if (manifestFile.isOpen()) {
                    manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
                    manifestFile.close();
                }
                return manifestPath;
            };
            auto verifyLegacyFixture = [&](const QString& fixtureName, const QString& manifestPath,
                                           const QStringList& expectedVisibleLabels, int expectedClassMode) {
                loadDatasetPath(manifestPath);
                expect(classEntries_.size() == expectedClassMode,
                       QString("%1 class mode expected %2, got %3")
                           .arg(fixtureName)
                           .arg(expectedClassMode)
                           .arg(classEntries_.size()));
                expect(filteredIndexes_.size() == expectedVisibleLabels.size(),
                       QString("%1 visible count expected %2, got %3")
                           .arg(fixtureName)
                           .arg(expectedVisibleLabels.size())
                           .arg(filteredIndexes_.size()));
                QJsonArray visibleLabels;
                int visibleUnreviewed = 0;
                for (int row = 0; row < filteredIndexes_.size() && row < expectedVisibleLabels.size(); ++row) {
                    const QString label = effectiveLabel(items_.at(filteredIndexes_.at(row)));
                    visibleLabels.append(label);
                    if (label == "unreviewed")
                        ++visibleUnreviewed;
                    expect(label == expectedVisibleLabels.at(row),
                           QString("%1 visible row %2 expected %3, got %4")
                               .arg(fixtureName)
                               .arg(row)
                               .arg(expectedVisibleLabels.at(row))
                               .arg(label));
                }
                for (const CropItem& crop : items_) {
                    const QString status = legacyStatus(crop.json);
                    if ((status == "rejected" || status == "excluded") && effectiveLabel(crop) != "exclude") {
                        expect(false,
                               QString("%1 %2 with legacy status %3 did not load as excluded")
                                   .arg(fixtureName)
                                   .arg(crop.imageId)
                                   .arg(status));
                    }
                }
                legacySuite[fixtureName + "_manifest_path"] = manifestPath;
                legacySuite[fixtureName + "_visible_labels"] = visibleLabels;
                legacySuite[fixtureName + "_visible_unreviewed"] = visibleUnreviewed;
                legacySuite[fixtureName + "_class_mode"] = classEntries_.size();
            };

            QJsonObject binaryRoot = makeLegacyRoot("legacy_binary_fixture", 2);
            QJsonArray binaryItems;
            {
                QJsonObject item;
                item["image_id"] = "legacy_binary_0";
                item["path"] = "crops/legacy_binary_0.png";
                item["label"] = "0";
                item["status"] = "included";
                binaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_binary_1";
                item["path"] = "crops/legacy_binary_1.png";
                item["label"] = "1";
                item["status"] = "included";
                binaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_binary_unknown";
                item["path"] = "crops/legacy_binary_unknown.png";
                item["label"] = "mystery";
                item["status"] = "included";
                binaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_binary_excluded";
                item["path"] = "crops/legacy_binary_excluded.png";
                item["status"] = "excluded";
                binaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_binary_rejected";
                item["path"] = "";
                item["label"] = "0";
                item["status"] = "rejected";
                binaryItems.append(item);
            }
            binaryRoot["items"] = binaryItems;
            const QString binaryManifest = writeLegacyFixture(
                "legacy-binary", binaryRoot,
                {{"crops/legacy_binary_0.png", QColor("#2563EB")},
                 {"crops/legacy_binary_1.png", QColor("#14B8A6")},
                 {"crops/legacy_binary_unknown.png", QColor("#F59E0B")},
                 {"crops/legacy_binary_excluded.png", QColor("#EF4444")}});
            verifyLegacyFixture("legacy_binary", binaryManifest, {"0", "1", "unreviewed", "exclude"}, 2);

            QJsonObject ternaryRoot = makeLegacyRoot("legacy_ternary_fixture", 3);
            QJsonArray ternaryItems;
            {
                QJsonObject item;
                item["image_id"] = "legacy_ternary_0";
                item["path"] = "crops/legacy_ternary_0.png";
                item["label"] = "0";
                item["status"] = "included";
                ternaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_ternary_1";
                item["path"] = "crops/legacy_ternary_1.png";
                item["label"] = "1";
                item["status"] = "included";
                ternaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_ternary_2";
                item["path"] = "crops/legacy_ternary_2.png";
                item["class_id"] = 2;
                item["status"] = "included";
                ternaryItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_ternary_rejected";
                item["path"] = "";
                item["label"] = "2";
                item["status"] = "rejected";
                ternaryItems.append(item);
            }
            ternaryRoot["items"] = ternaryItems;
            const QString ternaryManifest = writeLegacyFixture(
                "legacy-ternary", ternaryRoot,
                {{"crops/legacy_ternary_0.png", QColor("#2563EB")},
                 {"crops/legacy_ternary_1.png", QColor("#14B8A6")},
                 {"crops/legacy_ternary_2.png", QColor("#8B5CF6")}});
            verifyLegacyFixture("legacy_ternary", ternaryManifest, {"0", "1", "2"}, 3);

            QJsonObject mixedRoot = makeLegacyRoot("legacy_mixed_fixture", 3);
            QJsonArray mixedItems;
            {
                QJsonObject item;
                item["image_id"] = "legacy_mixed_edited";
                item["crop_path"] = "crops/legacy_mixed_edited.png";
                item["path"] = "crops/legacy_mixed_edited.png";
                item["auto_label"] = "unknown";
                item["reviewed_label"] = "0";
                item["review_state"] = "relabeled";
                item["trainer_eligible"] = true;
                item["label"] = "0";
                item["status"] = "included";
                mixedItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_mixed_1";
                item["path"] = "crops/legacy_mixed_1.png";
                item["label"] = "1";
                item["status"] = "included";
                mixedItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_mixed_2";
                item["path"] = "crops/legacy_mixed_2.png";
                item["class_id"] = 2;
                item["status"] = "included";
                mixedItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_mixed_0";
                item["path"] = "crops/legacy_mixed_0.png";
                item["label"] = "0";
                item["status"] = "included";
                mixedItems.append(item);
            }
            {
                QJsonObject item;
                item["image_id"] = "legacy_mixed_rejected";
                item["path"] = "";
                item["label"] = "0";
                item["status"] = "rejected";
                mixedItems.append(item);
            }
            mixedRoot["items"] = mixedItems;
            const QString mixedManifest = writeLegacyFixture(
                "legacy-mixed", mixedRoot,
                {{"crops/legacy_mixed_edited.png", QColor("#2563EB")},
                 {"crops/legacy_mixed_1.png", QColor("#14B8A6")},
                 {"crops/legacy_mixed_2.png", QColor("#8B5CF6")},
                 {"crops/legacy_mixed_0.png", QColor("#0EA5E9")}});
            loadDatasetPath(mixedManifest);
            expect(classEntries_.size() == 3,
                   QString("legacy_mixed class mode expected 3, got %1").arg(classEntries_.size()));
            expect(filteredIndexes_.size() == 4,
                   QString("legacy_mixed visible count expected 4, got %1").arg(filteredIndexes_.size()));
            QJsonArray mixedVisibleLabels;
            int mixedVisibleUnreviewed = 0;
            for (int row = 0; row < filteredIndexes_.size() && row < 4; ++row) {
                const CropItem& crop = items_.at(filteredIndexes_.at(row));
                const QString label = effectiveLabel(crop);
                mixedVisibleLabels.append(label);
                if (label == "unreviewed")
                    ++mixedVisibleUnreviewed;
            }
            expect(mixedVisibleLabels == QJsonArray::fromStringList({"0", "1", "2", "0"}),
                   QString("legacy_mixed visible labels were %1")
                       .arg(QString::fromUtf8(QJsonDocument(mixedVisibleLabels).toJson(QJsonDocument::Compact))));
            expect(mixedVisibleUnreviewed == 0,
                   QString("legacy_mixed unreviewed count expected 0, got %1").arg(mixedVisibleUnreviewed));
            expect(!items_.isEmpty() && items_.at(0).manualLabel == "0",
                   "legacy_mixed edited row did not preserve reviewed_label");
            expect(!items_.isEmpty() && items_.at(0).reviewState == "relabeled",
                   "legacy_mixed edited row did not preserve review_state");
            legacySuite["legacy_mixed_manifest_path"] = mixedManifest;
            legacySuite["legacy_mixed_visible_labels"] = mixedVisibleLabels;
            legacySuite["legacy_mixed_visible_unreviewed"] = mixedVisibleUnreviewed;
            legacySuite["legacy_mixed_first_review_state"] = items_.isEmpty() ? QString() : items_.at(0).reviewState;
        }

        QJsonObject result;
        result["manifest_path"] = builderManifestPath;
        result["visible_count"] = builderVisibleCount;
        result["grid_count"] = builderGridCount;
        result["page_size"] = builderPageSize;
        result["current_page"] = builderCurrentPage;
        result["total_pages"] = builderTotalPages;
        result["selected_source_index"] = builderSelectedSourceIndex;
        result["class_mode"] = builderClassMode;
        result["first_label_elapsed_ms"] = static_cast<qint64>(firstLabelElapsedMs);
        result["pending_save_debounce_observed"] = pendingSaveDebounceObserved;
        if (rows.size() >= 5) {
            result["row0_reviewed_label"] = rows.at(0).toObject().value("reviewed_label").toString();
            result["row0_exclude_reason"] = rows.at(0).toObject().value("exclude_reason").toString();
            result["row2_reviewed_label"] = rows.at(2).toObject().value("reviewed_label").toString();
            result["row3_reviewed_label"] = rows.at(3).toObject().value("reviewed_label").toString();
        }
        if (classes.size() >= 3) {
            result["class2_display_name"] = classes.at(2).toObject().value("display_name").toString();
            result["class0_display_color"] = classes.at(0).toObject().value("display_color").toString();
            result["class1_display_color"] = classes.at(1).toObject().value("display_color").toString();
            result["class2_display_color"] = classes.at(2).toObject().value("display_color").toString();
        }
        result["large_suite"] = largeSuite;
        result["legacy_suite"] = legacySuite;
        result["ok"] = failures.isEmpty();
        result["failures"] = QJsonArray::fromStringList(failures);
        return result;
    }

    bool shortcutAllowed() const {
        QWidget* focus = QApplication::focusWidget();
        return !qobject_cast<QLineEdit*>(focus) && !qobject_cast<QComboBox*>(focus) && !qobject_cast<QTextEdit*>(focus);
    }

    DatasetWorkspaceControls controls_;
    QLineEdit* manifestPathEdit_ = nullptr; 
    QPushButton* browseButton_ = nullptr;
    QMenu* browseMenu_ = nullptr;
    QAction* browseFolderAction_ = nullptr;
    QAction* browseJsonAction_ = nullptr;
    QPushButton* openFolderButton_ = nullptr; 
    QLabel* totalMetric_ = nullptr;
    QLabel* reviewedMetric_ = nullptr;
    QLabel* reviewedSubMetric_ = nullptr;
    QLabel* totalMetricValue_ = nullptr;
    QLabel* reviewedMetricValue_ = nullptr;
    QButtonGroup* filterGroup_ = nullptr;
    QMap<FilterMode, QPushButton*> filterButtons_;
    QLineEdit* searchEdit_ = nullptr;
    QToolButton* gridButton_ = nullptr;
    QToolButton* listButton_ = nullptr;
    QComboBox* pageSizeCombo_ = nullptr;
    QPushButton* pagePrevButton_ = nullptr;
    QPushButton* pageNextButton_ = nullptr;
    QButtonGroup* viewGroup_ = nullptr;
    QStackedWidget* browserStack_ = nullptr;
    QListWidget* gridList_ = nullptr;
    QTableWidget* listTable_ = nullptr;
    QLabel* centerPanelTitle_ = nullptr;
    QLabel* centerPanelSubtitle_ = nullptr;
    QLabel* pageSummaryLabel_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QLabel* filenameLabel_ = nullptr;
    QLabel* autoLabel_ = nullptr;
    QLabel* manualLabel_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QLabel* timestampLabel_ = nullptr;
    QComboBox* classModeCombo_ = nullptr;
    QLineEdit* classZeroEdit_ = nullptr;
    QLineEdit* classOneEdit_ = nullptr;
    QLineEdit* classTwoEdit_ = nullptr;
    QLabel* classTwoLabel_ = nullptr;
    QPushButton* classZeroColorButton_ = nullptr;
    QPushButton* classOneColorButton_ = nullptr;
    QPushButton* classTwoColorButton_ = nullptr;
    QPushButton* hitButton_ = nullptr;
    QPushButton* wasteButton_ = nullptr;
    QPushButton* classThreeButton_ = nullptr;
    QPushButton* excludeButton_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QHBoxLayout* pageButtonsLayout_ = nullptr;
    QDialog* loadingDialog_ = nullptr;
    QLabel* loadingTitleLabel_ = nullptr;
    QLabel* loadingDetailLabel_ = nullptr;
    QProgressBar* loadingProgressBar_ = nullptr;
    QTimer* manifestAutosaveTimer_ = nullptr;

    QVector<CropItem> items_;
    QVector<int> filteredIndexes_;
    QVector<int> visibleIndexes_;
    QVector<UndoEntry> undoStack_;
    QVector<ClassEntry> classEntries_;
    mutable QCache<QString, QPixmap> thumbnailCache_;
    QMap<int, QJsonObject> pendingManifestItemUpdates_;
    QJsonDocument manifestDoc_;
    QString manifestPath_;
    QString datasetRoot_;
    QString excludedLabelDisplay_ = "Exclude";
    int class0Count_ = 0;
    int class1Count_ = 0;
    int class2Count_ = 0;
    int excludedCount_ = 0;
    int unreviewedCount_ = 0;
    int selectedSourceIndex_ = -1;
    int currentPage_ = 0;
    int pageSize_ = kDefaultPageSize;
    FilterMode filterMode_ = FilterMode::All;
    PendingLoadState pendingLoad_;
    bool isLoading_ = false;
    bool gridPageDirty_ = true;
    bool tablePageDirty_ = true;
    quint64 loadGeneration_ = 0;
};

} // namespace

QWidget* buildDatasetWorkspace(const DatasetWorkspaceControls& controls) {
    return new DatasetWorkspaceWidget(controls);
}

} // namespace desktop_app::workspace
