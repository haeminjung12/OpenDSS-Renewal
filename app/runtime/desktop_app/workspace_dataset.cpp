#include "workspace_dataset.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <algorithm>

#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

constexpr int kSourceIndexRole = Qt::UserRole + 1;
constexpr int kIconSize = 86;

QString normalizedLabel(const QString& label) {
    const QString lower = label.trimmed().toLower();
    if (lower == "hits" || lower == "hit" || lower == "1")
        return "hit";
    if (lower == "waste" || lower == "empty" || lower == "0")
        return "waste";
    if (lower == "exclude" || lower == "excluded" || lower == "reject" || lower == "rejected")
        return "exclude";
    if (lower == "unknown")
        return "unknown";
    return lower;
}

QString displayLabel(const QString& label) {
    const QString normalized = normalizedLabel(label);
    if (normalized == "hit")
        return "Hit";
    if (normalized == "waste")
        return "Waste";
    if (normalized == "exclude")
        return "Excluded";
    if (normalized == "unknown")
        return "Unknown";
    if (normalized.isEmpty())
        return "Unreviewed";
    QString text = normalized;
    text[0] = text[0].toUpper();
    return text;
}

QColor labelColor(const QString& label) {
    const QString normalized = normalizedLabel(label);
    if (normalized == "hit")
        return QColor("#22c55e");
    if (normalized == "waste")
        return QColor("#ef4444");
    if (normalized == "exclude")
        return QColor("#9ca3af");
    return QColor("#3b82f6");
}

QStringList imageNameFilters() {
    return {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff", "*.webp"};
}

QString firstNonEmptyString(const QJsonObject& item, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const QString value = item.value(QString::fromLatin1(key)).toString().trimmed();
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

class DatasetWorkspaceWidget final : public QWidget {
  public:
    explicit DatasetWorkspaceWidget(const DatasetWorkspaceControls& controls) : controls_(controls) {
        nameWidget(this, "DatasetWorkspace");
        buildUi();
        wireUi();
        updateAll();
        maybeRunVerifier();
    }

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

    enum class FilterMode { All, Hit, Waste, Excluded, Unreviewed };

    void buildUi() {
        using desktop_app::ui::makePanel;
        using desktop_app::ui::makePanelBody;

        auto* root = new QHBoxLayout;
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(12);

        auto* leftPanel = makePanel("Dataset", "Manifest and filters");
        leftPanel->setFixedWidth(260);
        leftPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        auto* leftBody = makePanelBody(leftPanel);

        manifestPathEdit_ = new QLineEdit;
        manifestPathEdit_->setReadOnly(true);
        manifestPathEdit_->setPlaceholderText("Choose manifest or dataset folder...");
        manifestPathEdit_->setMinimumWidth(0);
        nameWidget(manifestPathEdit_, "DatasetWorkspaceManifestPathEdit");
        auto* browseButton = new QPushButton("Browse");
        browseButton->setMaximumWidth(78);
        nameWidget(browseButton, "DatasetWorkspaceManifestBrowseButton");
        auto* manifestRow = new QHBoxLayout;
        manifestRow->setSpacing(6);
        manifestRow->addWidget(manifestPathEdit_, 1);
        manifestRow->addWidget(browseButton);
        auto* manifestLabel = new QLabel;
        nameWidget(manifestLabel, "DatasetWorkspaceManifestLabel");
        leftBody->addWidget(makeDatasetKeyValue("Manifest", manifestLabel));
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

        filterGroup_ = new QButtonGroup(this);
        filterGroup_->setExclusive(true);
        addFilterButton(FilterMode::All, "All", "DatasetWorkspaceFilterAllButton", true, leftBody);
        addFilterButton(FilterMode::Hit, "Hit", "DatasetWorkspaceFilterHitButton", false, leftBody);
        addFilterButton(FilterMode::Waste, "Waste", "DatasetWorkspaceFilterWasteButton", false, leftBody);
        addFilterButton(FilterMode::Excluded, "Excluded", "DatasetWorkspaceFilterExcludedButton", false, leftBody);
        addFilterButton(FilterMode::Unreviewed, "Unreviewed", "DatasetWorkspaceFilterUnreviewedButton", false,
                        leftBody);

        openFolderButton_ = new QPushButton("Open Folder");
        nameWidget(openFolderButton_, "DatasetWorkspaceOpenFolderButton");
        leftBody->addWidget(openFolderButton_);
        leftBody->addStretch(1);

        auto* centerPanel = makePanel("All crops", "0 shown");
        centerPanel->setObjectName("DatasetCropBrowserPanel");
        centerPanel->setMinimumWidth(420);
        centerPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        centerPanelSubtitle_ = centerPanel->findChildren<QLabel*>().value(1);
        centerPanelTitle_ = centerPanel->findChildren<QLabel*>().value(0);
        auto* centerBody = makePanelBody(centerPanel);

        auto* toolbar = new QHBoxLayout;
        toolbar->setSpacing(8);
        searchEdit_ = new QLineEdit;
        searchEdit_->setPlaceholderText("Search crops...");
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
        gridList_->setSelectionMode(QAbstractItemView::SingleSelection);
        gridList_->setUniformItemSizes(true);
        nameWidget(gridList_, "DatasetWorkspaceCropGrid");
        listTable_ = new QTableWidget(0, 5);
        listTable_->setHorizontalHeaderLabels({"Thumbnail", "Filename", "Auto-label", "Manual label", "Confidence"});
        listTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        listTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        listTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        listTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        listTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        listTable_->verticalHeader()->setDefaultSectionSize(58);
        listTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        listTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        listTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        nameWidget(listTable_, "DatasetWorkspaceCropListTable");
        browserStack_->addWidget(gridList_);
        browserStack_->addWidget(listTable_);
        centerBody->addWidget(browserStack_, 1);

        auto* rightPanel = makePanel("Review", "Selected crop");
        rightPanel->setObjectName("DatasetReviewPanel");
        rightPanel->setFixedWidth(320);
        rightPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        auto* rightBody = makePanelBody(rightPanel);

        previewLabel_ = new QLabel("No crop selected");
        previewLabel_->setAlignment(Qt::AlignCenter);
        previewLabel_->setMinimumSize(220, 220);
        previewLabel_->setMaximumHeight(300);
        previewLabel_->setProperty("viewerCanvas", true);
        nameWidget(previewLabel_, "DatasetWorkspacePreviewFrame");
        rightBody->addWidget(previewLabel_);

        filenameLabel_ = new QLabel("--");
        autoLabel_ = new QLabel("--");
        confidenceLabel_ = new QLabel("--");
        frameLabel_ = new QLabel("--");
        timestampLabel_ = new QLabel("--");
        manualLabel_ = new QLabel("--");
        auto* metadata = new QGridLayout;
        addMetadataRow(metadata, 0, "Filename", filenameLabel_);
        addMetadataRow(metadata, 1, "Auto-label", autoLabel_);
        addMetadataRow(metadata, 2, "Manual label", manualLabel_);
        addMetadataRow(metadata, 3, "Confidence", confidenceLabel_);
        addMetadataRow(metadata, 4, "Frame", frameLabel_);
        addMetadataRow(metadata, 5, "Timestamp", timestampLabel_);
        rightBody->addLayout(metadata);

        auto* manualReviewLabel = new QLabel("Manual Review");
        manualReviewLabel->setProperty("panelTitle", true);
        rightBody->addWidget(manualReviewLabel);
        auto* actionRow = new QHBoxLayout;
        actionRow->setSpacing(8);
        hitButton_ = new QPushButton("Hit");
        wasteButton_ = new QPushButton("Waste");
        excludeButton_ = new QPushButton("Exclude");
        hitButton_->setStyleSheet("background:#166534;color:white;");
        wasteButton_->setStyleSheet("background:#991b1b;color:white;");
        excludeButton_->setStyleSheet("background:#4b5563;color:white;");
        nameWidget(hitButton_, "DatasetWorkspaceHitButton");
        nameWidget(wasteButton_, "DatasetWorkspaceWasteButton");
        nameWidget(excludeButton_, "DatasetWorkspaceExcludeButton");
        actionRow->addWidget(hitButton_, 1);
        actionRow->addWidget(wasteButton_, 1);
        actionRow->addWidget(excludeButton_, 1);
        rightBody->addLayout(actionRow);
        acceptAutoButton_ = new QPushButton("Accept Auto-label");
        nameWidget(acceptAutoButton_, "DatasetWorkspaceAcceptAutoButton");
        rightBody->addWidget(acceptAutoButton_);
        excludeReasonCombo_ = new QComboBox;
        excludeReasonCombo_->addItem("Blurry", "bad_crop");
        excludeReasonCombo_->addItem("Partial", "partial_droplet");
        excludeReasonCombo_->addItem("Ambiguous", "ambiguous");
        excludeReasonCombo_->addItem("Other", "other");
        nameWidget(excludeReasonCombo_, "DatasetWorkspaceExcludeReasonCombo");
        rightBody->addWidget(new QLabel("Exclude reason"));
        rightBody->addWidget(excludeReasonCombo_);
        auto* nav = new QHBoxLayout;
        undoButton_ = new QPushButton("Undo");
        previousButton_ = new QPushButton("Previous");
        nextButton_ = new QPushButton("Next");
        nameWidget(undoButton_, "DatasetWorkspaceUndoButton");
        nameWidget(previousButton_, "DatasetWorkspacePreviousButton");
        nameWidget(nextButton_, "DatasetWorkspaceNextButton");
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

        root->addWidget(leftPanel, 0);
        root->addWidget(centerPanel, 1);
        root->addWidget(rightPanel, 0);
        setMinimumWidth(1000);
        setLayout(root);

        browseButton_ = browseButton;
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

    void wireUi() {
        connect(browseButton_, &QPushButton::clicked, this, [this]() { browseForDataset(); });
        connect(openFolderButton_, &QPushButton::clicked, this, [this]() { openManifestFolder(); });
        connect(filterGroup_, &QButtonGroup::idClicked, this, [this](int id) {
            filterMode_ = static_cast<FilterMode>(id);
            applyFilters();
        });
        connect(searchEdit_, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
        connect(viewGroup_, &QButtonGroup::idClicked, browserStack_, &QStackedWidget::setCurrentIndex);
        connect(gridList_, &QListWidget::itemSelectionChanged, this, [this]() { selectFromGrid(); });
        connect(listTable_, &QTableWidget::itemSelectionChanged, this, [this]() { selectFromTable(); });
        connect(hitButton_, &QPushButton::clicked, this, [this]() { applyReviewLabel("hit"); });
        connect(wasteButton_, &QPushButton::clicked, this, [this]() { applyReviewLabel("waste"); });
        connect(excludeButton_, &QPushButton::clicked, this, [this]() { applyReviewLabel("exclude"); });
        connect(acceptAutoButton_, &QPushButton::clicked, this, [this]() { acceptAutoLabel(); });
        connect(undoButton_, &QPushButton::clicked, this, [this]() { undoLastLabelChange(); });
        connect(previousButton_, &QPushButton::clicked, this, [this]() { selectRelative(-1); });
        connect(nextButton_, &QPushButton::clicked, this, [this]() { selectRelative(1); });
        connect(excludeReasonCombo_, &QComboBox::currentIndexChanged, this, [this]() { updateExcludeReason(); });

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

    void browseForDataset() {
        QFileDialog dialog(this, "Select dataset manifest or folder");
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilter("Dataset manifests (*.json);;All files (*.*)");
        if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
            loadDatasetPath(dialog.selectedFiles().first());
            return;
        }
        const QString folder = QFileDialog::getExistingDirectory(this, "Select dataset folder", datasetRoot_);
        if (!folder.isEmpty())
            loadDatasetPath(folder);
    }

    void openManifestFolder() {
        QString folder = datasetRoot_;
        if (!manifestPath_.isEmpty())
            folder = QFileInfo(manifestPath_).absolutePath();
        if (!folder.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }

    void loadDatasetPath(const QString& path) {
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
        if (!manifest.isEmpty() && loadManifest(manifest)) {
            statusLabel_->setText("Manifest loaded. Label changes autosave immediately.");
        } else {
            scanFolderForImages(datasetRoot_);
            statusLabel_->setText(items_.isEmpty() ? "No manifest or crop images found. Use Browse to choose a dataset."
                                                   : "No manifest found. Scanned images and will create "
                                                     "metadata/dataset_manifest.json on first label change.");
        }
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
        manifestDoc_ = doc;
        manifestPath_ = QFileInfo(path).absoluteFilePath();
        QFileInfo info(manifestPath_);
        datasetRoot_ = info.absolutePath();
        if (info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
            QDir root(info.dir());
            root.cdUp();
            datasetRoot_ = root.absolutePath();
        }
        const QJsonArray rows = doc.object().value("items").toArray();
        for (int i = 0; i < rows.size(); ++i) {
            const QJsonObject item = rows.at(i).toObject();
            CropItem crop;
            crop.manifestIndex = i;
            crop.imageId = item.value("image_id").toString(QString("crop_%1").arg(i + 1, 4, 10, QChar('0')));
            crop.cropPath = firstNonEmptyString(item, {"crop_path", "path", "image_path", "relative_path"});
            crop.autoLabel = normalizedLabel(item.value("auto_label").toString("unknown"));
            crop.manualLabel = normalizedLabel(item.value("reviewed_label").toString());
            crop.reviewState =
                item.value("review_state").toString(crop.manualLabel.isEmpty() ? "unreviewed" : "confirmed");
            crop.confidence = item.value("auto_label_confidence").toVariant().toString();
            crop.frameNumber = item.value("source_frame_id").toVariant().toString();
            crop.timestamp = item.value("timestamp").toString();
            crop.excludeReason = item.value("exclude_reason").toString();
            crop.json = item;
            items_.push_back(crop);
        }
        return true;
    }

    void scanFolderForImages(const QString& folder) {
        if (folder.isEmpty())
            return;
        QDirIterator it(folder, imageNameFilters(), QDir::Files, QDirIterator::Subdirectories);
        QJsonArray manifestItems;
        while (it.hasNext()) {
            const QString absolute = it.next();
            const QFileInfo info(absolute);
            CropItem crop;
            crop.manifestIndex = items_.size();
            crop.imageId = info.completeBaseName();
            crop.cropPath = QDir(folder).relativeFilePath(absolute);
            crop.autoLabel = "unknown";
            crop.reviewState = "unreviewed";
            crop.timestamp = info.lastModified().toUTC().toString(Qt::ISODate);
            crop.json = cropToJson(crop);
            items_.push_back(crop);
            manifestItems.append(crop.json);
        }
        QJsonObject root;
        root["schema_version"] = "dataset-builder-manifest-v1";
        root["dataset_id"] = QFileInfo(folder).fileName();
        root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        root["items"] = manifestItems;
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
        item["trainer_eligible"] = crop.manualLabel == "hit" || crop.manualLabel == "waste";
        return item;
    }

    void clearDataset() {
        items_.clear();
        visibleIndexes_.clear();
        undoStack_.clear();
        manifestDoc_ = QJsonDocument();
        manifestPath_.clear();
        selectedSourceIndex_ = -1;
    }

    QString effectiveLabel(const CropItem& crop) const {
        const QString manual = normalizedLabel(crop.manualLabel);
        if (manual == "hit" || manual == "waste" || manual == "exclude")
            return manual;
        if (crop.reviewState.toLower() == "excluded")
            return "exclude";
        return "unreviewed";
    }

    bool cropMatchesFilter(const CropItem& crop) const {
        if (!hasDisplayableCropPath(crop))
            return false;
        const QString label = effectiveLabel(crop);
        if (filterMode_ == FilterMode::Hit && label != "hit")
            return false;
        if (filterMode_ == FilterMode::Waste && label != "waste")
            return false;
        if (filterMode_ == FilterMode::Excluded && label != "exclude")
            return false;
        if (filterMode_ == FilterMode::Unreviewed && label != "unreviewed")
            return false;
        const QString needle = searchEdit_->text().trimmed().toLower();
        if (needle.isEmpty())
            return true;
        const QString haystack = QStringList{crop.imageId,     crop.cropPath,   crop.autoLabel,   crop.manualLabel,
                                             crop.reviewState, crop.confidence, crop.frameNumber, crop.timestamp}
                                     .join(' ')
                                     .toLower();
        return haystack.contains(needle);
    }

    void applyFilters() {
        const int previousSelection = selectedSourceIndex_;
        visibleIndexes_.clear();
        for (int i = 0; i < items_.size(); ++i) {
            if (cropMatchesFilter(items_.at(i)))
                visibleIndexes_.push_back(i);
        }
        rebuildGrid();
        rebuildTable();
        int selectedVisible = visibleIndexes_.indexOf(previousSelection);
        if (selectedVisible < 0 && !visibleIndexes_.isEmpty())
            selectedVisible = 0;
        setSelectionByVisibleRow(selectedVisible);
        updateCounts();
        updatePreview();
        updateReviewControls();
    }

    void rebuildGrid() {
        QSignalBlocker blocker(gridList_);
        gridList_->clear();
        if (visibleIndexes_.isEmpty()) {
            auto* empty = new QListWidgetItem("No crops match");
            empty->setFlags(Qt::NoItemFlags);
            gridList_->addItem(empty);
            return;
        }
        for (const int sourceIndex : visibleIndexes_) {
            const CropItem& crop = items_.at(sourceIndex);
            auto* item = new QListWidgetItem(makeThumbnailIcon(crop, kIconSize), QFileInfo(crop.cropPath).fileName());
            item->setData(kSourceIndexRole, sourceIndex);
            item->setTextAlignment(Qt::AlignCenter);
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
                displayLabel(crop.autoLabel),
                displayLabel(effectiveLabel(crop)),
                crop.confidence.isEmpty() ? "--" : crop.confidence,
            };
            for (int col = 0; col < values.size(); ++col) {
                auto* tableItem = new QTableWidgetItem(values.at(col));
                tableItem->setData(kSourceIndexRole, sourceIndex);
                listTable_->setItem(row, col + 1, tableItem);
            }
        }
    }

    QIcon makeThumbnailIcon(const CropItem& crop, int size) const {
        QPixmap pix(size, size);
        pix.fill(QColor("#111827"));
        const QString path = absoluteCropPath(crop);
        QImageReader reader(path);
        reader.setAutoTransform(true);
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
        QPen pen(labelColor(effectiveLabel(crop)));
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
        gridList_->clearSelection();
        listTable_->clearSelection();
        if (visibleRow < 0 || visibleRow >= visibleIndexes_.size()) {
            selectedSourceIndex_ = -1;
            return;
        }
        selectedSourceIndex_ = visibleIndexes_.at(visibleRow);
        if (gridList_->item(visibleRow))
            gridList_->item(visibleRow)->setSelected(true);
        if (visibleRow < listTable_->rowCount())
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
        QSignalBlocker blocker(gridList_);
        gridList_->clearSelection();
        const int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (row >= 0 && gridList_->item(row)) {
            gridList_->item(row)->setSelected(true);
            gridList_->scrollToItem(gridList_->item(row), QAbstractItemView::PositionAtCenter);
        }
    }

    void syncTableSelection() {
        QSignalBlocker blocker(listTable_);
        listTable_->clearSelection();
        const int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (row >= 0 && row < listTable_->rowCount()) {
            listTable_->selectRow(row);
            listTable_->scrollToItem(listTable_->item(row, 0), QAbstractItemView::PositionAtCenter);
        }
    }

    void selectRelative(int delta) {
        if (visibleIndexes_.isEmpty())
            return;
        int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        if (row < 0)
            row = 0;
        row = std::clamp(row + delta, 0, static_cast<int>(visibleIndexes_.size()) - 1);
        setSelectionByVisibleRow(row);
        updatePreview();
        updateReviewControls();
    }

    void applyReviewLabel(const QString& label) {
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size())
            return;
        const int previousVisibleRow = visibleIndexes_.indexOf(selectedSourceIndex_);
        const int previousSourceIndex = selectedSourceIndex_;
        CropItem& crop = items_[selectedSourceIndex_];
        undoStack_.push_back({crop.manifestIndex, crop.json});
        crop.manualLabel = normalizedLabel(label);
        crop.reviewState = crop.manualLabel == "exclude"
                               ? "excluded"
                               : (crop.manualLabel == normalizedLabel(crop.autoLabel) ? "confirmed" : "relabeled");
        crop.excludeReason = crop.manualLabel == "exclude" ? excludeReasonCombo_->currentData().toString() : QString();
        crop.json = cropToJson(crop);
        saveItemToManifest(crop);
        autosaveManifest();
        applyFilters();
        advanceAfterReview(previousSourceIndex, previousVisibleRow);
    }

    void acceptAutoLabel() {
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size())
            return;
        const QString label = normalizedLabel(items_.at(selectedSourceIndex_).autoLabel);
        if (label == "hit" || label == "waste")
            applyReviewLabel(label);
    }

    void undoLastLabelChange() {
        if (undoStack_.isEmpty())
            return;
        const UndoEntry undo = undoStack_.takeLast();
        if (undo.manifestIndex < 0 || undo.manifestIndex >= items_.size())
            return;
        CropItem& crop = items_[undo.manifestIndex];
        crop.json = undo.previousItem;
        crop.manualLabel = normalizedLabel(undo.previousItem.value("reviewed_label").toString());
        crop.reviewState =
            undo.previousItem.value("review_state").toString(crop.manualLabel.isEmpty() ? "unreviewed" : "confirmed");
        crop.excludeReason = undo.previousItem.value("exclude_reason").toString();
        saveItemToManifest(crop);
        autosaveManifest();
        applyFilters();
    }

    void updateExcludeReason() {
        if (suppressReasonAutosave_)
            return;
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size())
            return;
        CropItem& crop = items_[selectedSourceIndex_];
        if (normalizedLabel(crop.manualLabel) != "exclude")
            return;
        undoStack_.push_back({crop.manifestIndex, crop.json});
        crop.excludeReason = excludeReasonCombo_->currentData().toString();
        crop.json = cropToJson(crop);
        saveItemToManifest(crop);
        autosaveManifest();
        applyFilters();
    }

    void advanceAfterReview(int previousSourceIndex, int previousVisibleRow) {
        if (visibleIndexes_.isEmpty())
            return;
        int targetRow = visibleIndexes_.indexOf(previousSourceIndex);
        if (targetRow >= 0)
            ++targetRow;
        else
            targetRow = previousVisibleRow;
        targetRow = std::clamp(targetRow, 0, static_cast<int>(visibleIndexes_.size()) - 1);
        setSelectionByVisibleRow(targetRow);
        updatePreview();
        updateReviewControls();
    }

    void saveItemToManifest(const CropItem& crop) {
        QJsonObject root = manifestDoc_.isObject() ? manifestDoc_.object() : QJsonObject();
        QJsonArray rows = root.value("items").toArray();
        while (rows.size() <= crop.manifestIndex)
            rows.append(QJsonObject());
        rows.replace(crop.manifestIndex, crop.json);
        root["items"] = rows;
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!root.contains("schema_version"))
            root["schema_version"] = "dataset-builder-manifest-v1";
        if (!root.contains("dataset_id"))
            root["dataset_id"] = QFileInfo(datasetRoot_).fileName();
        manifestDoc_ = QJsonDocument(root);
    }

    bool autosaveManifest() {
        if (manifestPath_.isEmpty() || !manifestDoc_.isObject())
            return false;
        QDir().mkpath(QFileInfo(manifestPath_).absolutePath());
        QFile file(manifestPath_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            statusLabel_->setText("Autosave failed: " + QDir::toNativeSeparators(manifestPath_));
            return false;
        }
        file.write(manifestDoc_.toJson(QJsonDocument::Indented));
        statusLabel_->setText("Autosaved labels to " + QDir::toNativeSeparators(manifestPath_));
        manifestPathEdit_->setText(QDir::toNativeSeparators(manifestPath_));
        return true;
    }

    void updateAll() {
        applyFilters();
        updateCounts();
        updatePreview();
        updateReviewControls();
    }

    void updateCounts() {
        int hit = 0;
        int waste = 0;
        int excluded = 0;
        int unreviewed = 0;
        for (const CropItem& crop : items_) {
            if (!hasDisplayableCropPath(crop))
                continue;
            const QString label = effectiveLabel(crop);
            if (label == "hit")
                ++hit;
            else if (label == "waste")
                ++waste;
            else if (label == "exclude")
                ++excluded;
            else
                ++unreviewed;
        }
        const int reviewed = hit + waste + excluded;
        const int total = hit + waste + excluded + unreviewed;
        if (totalMetric_)
            totalMetric_->setText(QString::number(total));
        if (reviewedMetric_)
            reviewedMetric_->setText(QString::number(reviewed));
        if (reviewedSubMetric_) {
            reviewedSubMetric_->setText(total == 0 ? "0%" : QString("%1%").arg(reviewed * 100 / total));
        }
        setFilterText(FilterMode::All, "All", total);
        setFilterText(FilterMode::Hit, "Hit", hit);
        setFilterText(FilterMode::Waste, "Waste", waste);
        setFilterText(FilterMode::Excluded, "Excluded", excluded);
        setFilterText(FilterMode::Unreviewed, "Unreviewed", unreviewed);
        if (centerPanelTitle_)
            centerPanelTitle_->setText(filterTitle());
        if (centerPanelSubtitle_)
            centerPanelSubtitle_->setText(QString("%1 shown").arg(visibleIndexes_.size()));
    }

    void setFilterText(FilterMode mode, const QString& label, int count) {
        if (auto* button = filterButtons_.value(mode, nullptr))
            button->setText(QString("%1 (%2)").arg(label).arg(count));
    }

    QString filterTitle() const {
        switch (filterMode_) {
        case FilterMode::Hit:
            return "Hit crops";
        case FilterMode::Waste:
            return "Waste crops";
        case FilterMode::Excluded:
            return "Excluded crops";
        case FilterMode::Unreviewed:
            return "Unreviewed crops";
        case FilterMode::All:
            return "All crops";
        }
        return "All crops";
    }

    void updatePreview() {
        suppressReasonAutosave_ = true;
        if (selectedSourceIndex_ < 0 || selectedSourceIndex_ >= items_.size()) {
            previewLabel_->setPixmap(QPixmap());
            previewLabel_->setText("No crop selected");
            filenameLabel_->setText("--");
            autoLabel_->setText("--");
            manualLabel_->setText("--");
            confidenceLabel_->setText("--");
            frameLabel_->setText("--");
            timestampLabel_->setText("--");
            suppressReasonAutosave_ = false;
            return;
        }
        const CropItem& crop = items_.at(selectedSourceIndex_);
        filenameLabel_->setText(QFileInfo(crop.cropPath).fileName());
        autoLabel_->setText(displayLabel(crop.autoLabel));
        manualLabel_->setText(displayLabel(effectiveLabel(crop)));
        confidenceLabel_->setText(crop.confidence.isEmpty() ? "--" : crop.confidence);
        frameLabel_->setText(crop.frameNumber.isEmpty() ? "--" : crop.frameNumber);
        timestampLabel_->setText(crop.timestamp.isEmpty() ? "--" : crop.timestamp);
        const int reasonIndex = excludeReasonCombo_->findData(crop.excludeReason);
        excludeReasonCombo_->setCurrentIndex(reasonIndex >= 0 ? reasonIndex : 0);
        QImageReader reader(absoluteCropPath(crop));
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            previewLabel_->setPixmap(QPixmap());
            previewLabel_->setText("Preview unavailable\n" + QFileInfo(crop.cropPath).fileName());
        } else {
            previewLabel_->setText(QString());
            previewLabel_->setPixmap(
                QPixmap::fromImage(image).scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        suppressReasonAutosave_ = false;
    }

    void updateReviewControls() {
        const bool hasSelection = selectedSourceIndex_ >= 0 && selectedSourceIndex_ < items_.size();
        hitButton_->setEnabled(hasSelection);
        wasteButton_->setEnabled(hasSelection);
        excludeButton_->setEnabled(hasSelection);
        acceptAutoButton_->setEnabled(hasSelection && (items_.at(selectedSourceIndex_).autoLabel == "hit" ||
                                                       items_.at(selectedSourceIndex_).autoLabel == "waste"));
        undoButton_->setEnabled(!undoStack_.isEmpty());
        excludeReasonCombo_->setEnabled(hasSelection);
        const int row = visibleIndexes_.indexOf(selectedSourceIndex_);
        previousButton_->setEnabled(row > 0);
        nextButton_->setEnabled(row >= 0 && row < visibleIndexes_.size() - 1);
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
                QCoreApplication::quit();
            }
        });
    }

    QJsonObject runVerifier(const QString& manifest) {
        QStringList failures;
        auto expect = [&failures](bool condition, const QString& message) {
            if (!condition)
                failures.push_back(message);
        };

        loadDatasetPath(manifest);
        expect(items_.size() == 5, QString("expected 5 manifest rows, got %1").arg(items_.size()));
        expect(visibleIndexes_.size() == 4,
               QString("expected 4 displayable crops, got %1").arg(visibleIndexes_.size()));
        expect(gridList_->count() == 4, QString("expected 4 grid thumbnails, got %1").arg(gridList_->count()));
        expect(centerPanelSubtitle_->text() == "4 shown", "center subtitle did not report only displayable crops");
        expect(!visibleIndexes_.contains(1), "manifest row with empty crop_path is still visible");

        setSelectionByVisibleRow(0);
        applyReviewLabel("hit");
        expect(selectedSourceIndex_ == 2, "Hit did not advance to the next visible crop");

        setSelectionByVisibleRow(1);
        applyReviewLabel("waste");
        expect(selectedSourceIndex_ == 3, "Waste did not advance to the next visible crop");

        setSelectionByVisibleRow(0);
        const int partialIndex = excludeReasonCombo_->findData("partial_droplet");
        expect(partialIndex >= 0, "Partial exclude reason option was not found");
        if (partialIndex >= 0)
            excludeReasonCombo_->setCurrentIndex(partialIndex);
        applyReviewLabel("exclude");
        expect(selectedSourceIndex_ == 2, "Exclude did not advance to the next visible crop");

        setSelectionByVisibleRow(2);
        updateReviewControls();
        const bool acceptAutoEnabledOnAutoLabel = acceptAutoButton_->isEnabled();
        expect(acceptAutoEnabledOnAutoLabel, "Accept Auto-label was not enabled for a waste auto-label crop");
        acceptAutoLabel();
        expect(selectedSourceIndex_ == 4, "Accept Auto-label did not advance after applying the auto-label");

        QFile file(manifestPath_);
        QJsonArray rows;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument saved = QJsonDocument::fromJson(file.readAll());
            rows = saved.object().value("items").toArray();
        }
        expect(rows.size() == 5, QString("saved manifest row count changed to %1").arg(rows.size()));
        if (rows.size() >= 5) {
            expect(rows.at(0).toObject().value("reviewed_label").toString() == "exclude",
                   "Exclude did not persist reviewed_label");
            expect(rows.at(0).toObject().value("exclude_reason").toString() == "partial_droplet",
                   "Exclude reason did not persist");
            expect(rows.at(2).toObject().value("reviewed_label").toString() == "waste",
                   "Waste did not persist reviewed_label");
            expect(rows.at(3).toObject().value("reviewed_label").toString() == "waste",
                   "Auto-label did not persist reviewed_label");
        }

        QJsonObject result;
        result["ok"] = failures.isEmpty();
        result["failures"] = QJsonArray::fromStringList(failures);
        result["manifest_path"] = manifestPath_;
        result["visible_count"] = visibleIndexes_.size();
        result["grid_count"] = gridList_->count();
        result["selected_source_index"] = selectedSourceIndex_;
        result["accept_auto_enabled_on_auto_label"] = acceptAutoEnabledOnAutoLabel;
        if (rows.size() >= 5) {
            result["row0_reviewed_label"] = rows.at(0).toObject().value("reviewed_label").toString();
            result["row0_exclude_reason"] = rows.at(0).toObject().value("exclude_reason").toString();
            result["row2_reviewed_label"] = rows.at(2).toObject().value("reviewed_label").toString();
            result["row3_reviewed_label"] = rows.at(3).toObject().value("reviewed_label").toString();
        }
        return result;
    }

    bool shortcutAllowed() const {
        QWidget* focus = QApplication::focusWidget();
        return !qobject_cast<QLineEdit*>(focus) && !qobject_cast<QComboBox*>(focus) && !qobject_cast<QTextEdit*>(focus);
    }

    DatasetWorkspaceControls controls_;
    QLineEdit* manifestPathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
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
    QButtonGroup* viewGroup_ = nullptr;
    QStackedWidget* browserStack_ = nullptr;
    QListWidget* gridList_ = nullptr;
    QTableWidget* listTable_ = nullptr;
    QLabel* centerPanelTitle_ = nullptr;
    QLabel* centerPanelSubtitle_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QLabel* filenameLabel_ = nullptr;
    QLabel* autoLabel_ = nullptr;
    QLabel* manualLabel_ = nullptr;
    QLabel* confidenceLabel_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QLabel* timestampLabel_ = nullptr;
    QPushButton* hitButton_ = nullptr;
    QPushButton* wasteButton_ = nullptr;
    QPushButton* excludeButton_ = nullptr;
    QPushButton* acceptAutoButton_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QComboBox* excludeReasonCombo_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QVector<CropItem> items_;
    QVector<int> visibleIndexes_;
    QVector<UndoEntry> undoStack_;
    QJsonDocument manifestDoc_;
    QString manifestPath_;
    QString datasetRoot_;
    int selectedSourceIndex_ = -1;
    FilterMode filterMode_ = FilterMode::All;
    bool suppressReasonAutosave_ = false;
};

} // namespace

QWidget* buildDatasetWorkspace(const DatasetWorkspaceControls& controls) {
    return new DatasetWorkspaceWidget(controls);
}

} // namespace desktop_app::workspace
