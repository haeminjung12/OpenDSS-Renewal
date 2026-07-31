#include "dataset_label_controller.h"

#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSet>

#include <algorithm>

namespace desktop_app::v2::dataset {
namespace {

QString stateText(DatasetLabelState state)
{
    switch (state) {
    case DatasetLabelState::Unlabeled:
        return QStringLiteral("unreviewed");
    case DatasetLabelState::Class0:
        return QStringLiteral("class0");
    case DatasetLabelState::Class1:
        return QStringLiteral("class1");
    case DatasetLabelState::Class2:
        return QStringLiteral("class2");
    case DatasetLabelState::Excluded:
        return QStringLiteral("excluded");
    }
    return {};
}

bool isSupportedFilter(const QString &filter)
{
    return filter == QStringLiteral("all") || filter == QStringLiteral("class0") ||
           filter == QStringLiteral("class1") || filter == QStringLiteral("class2") ||
           filter == QStringLiteral("excluded") || filter == QStringLiteral("unreviewed");
}

QString localPath(const QUrl &url, const QString &kind, QString *error)
{
    if (!url.isValid() || !url.isLocalFile()) {
        *error = kind + QStringLiteral(" must be a local file URL.");
        return {};
    }
    const QString path = url.toLocalFile();
    if (path.isEmpty())
        *error = kind + QStringLiteral(" URL must contain a local path.");
    return path;
}

} // namespace

DatasetLabelController::DatasetLabelController(OperationCoordinator &operations,
                                               ApplicationStateStore &stateStore,
                                               QObject *parent)
    : QAbstractListModel(parent)
    , service_(operations)
    , stateStore_(stateStore)
{
}

DatasetLabelController::~DatasetLabelController()
{
    if (openThread_) {
        openThread_->wait();
        delete openThread_;
    }
}

int DatasetLabelController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : filteredRows_.size();
}

QVariant DatasetLabelController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= filteredRows_.size())
        return {};

    const DatasetLabelRecordState &record = snapshot_.records.at(filteredRows_.at(index.row()));
    switch (role) {
    case RecordIdRole:
        return record.recordId;
    case CropUrlRole:
        return cropUrl(record);
    case StateRole:
        return stateText(record.state);
    case SelectedRole:
        return selectedRecordIds_.contains(record.recordId);
    default:
        return {};
    }
}

QHash<int, QByteArray> DatasetLabelController::roleNames() const
{
    static const QHash<int, QByteArray> roles{
        {RecordIdRole, QByteArrayLiteral("recordId")},
        {CropUrlRole, QByteArrayLiteral("cropUrl")},
        {StateRole, QByteArrayLiteral("state")},
        {SelectedRole, QByteArrayLiteral("selected")},
    };
    return roles;
}

QString DatasetLabelController::presentation() const
{
    if (snapshot_.manifestPath.isEmpty())
        return QStringLiteral("empty");
    return snapshot_.classes.isEmpty() ? QStringLiteral("classDefinition")
                                       : QStringLiteral("ready");
}

QUrl DatasetLabelController::manifestUrl() const
{
    return snapshot_.manifestPath.isEmpty() ? QUrl{} : QUrl::fromLocalFile(snapshot_.manifestPath);
}
QString DatasetLabelController::datasetId() const { return snapshot_.datasetId; }
int DatasetLabelController::totalCount() const { return snapshot_.records.size(); }
int DatasetLabelController::unreviewedCount() const { return snapshot_.counts.unreviewed; }
int DatasetLabelController::excludedCount() const { return snapshot_.counts.excluded; }
int DatasetLabelController::classCount() const { return snapshot_.classes.size(); }
int DatasetLabelController::class0Count() const { return snapshot_.counts.classCounts.value(0); }
int DatasetLabelController::class1Count() const { return snapshot_.counts.classCounts.value(1); }
int DatasetLabelController::class2Count() const { return snapshot_.counts.classCounts.value(2); }
QVariantList DatasetLabelController::classNames() const
{
    QVariantList names;
    names.reserve(snapshot_.classes.size());
    for (const DatasetClass &datasetClass : snapshot_.classes)
        names.append(datasetClass.name);
    return names;
}
bool DatasetLabelController::class2Enabled() const { return classCount() == 3; }
bool DatasetLabelController::canUndo() const { return snapshot_.canUndo; }

int DatasetLabelController::labeledCount() const
{
    return totalCount() - unreviewedCount() - excludedCount();
}

QString DatasetLabelController::selectedRecordId() const { return selectedRecordId_; }
QString DatasetLabelController::filter() const { return filter_; }
QString DatasetLabelController::errorMessage() const { return errorMessage_; }
bool DatasetLabelController::loading() const { return loading_; }
QUrl DatasetLabelController::selectedCropUrl() const { return selectedCropUrl_; }
int DatasetLabelController::selectedIndex() const { return selectedIndex_; }

bool DatasetLabelController::finish(bool success, const QString &error, bool serviceChanged)
{
    errorMessage_ = success ? QString{} : error;
    if (success && serviceChanged)
        refreshSnapshot();
    emit changed();
    return success;
}

void DatasetLabelController::publishDatasetState()
{
    stateStore_.publishDataset({datasetId(), snapshot_.manifestPath, true, {}});
}

void DatasetLabelController::refreshSnapshot(bool resetModel)
{
    applySnapshot(service_.snapshot(), resetModel);
}

bool DatasetLabelController::rejectWhileLoading()
{
    if (!loading_)
        return false;
    finish(false, QStringLiteral("Dataset is still loading."));
    return true;
}

void DatasetLabelController::finishOpen()
{
    QThread *completedThread = openThread_;
    openThread_ = nullptr;
    if (completedThread)
        completedThread->deleteLater();

    bool succeeded = false;
    QString error;
    {
        const QMutexLocker lock(&openResultMutex_);
        succeeded = openSucceeded_;
        error = openError_;
    }

    loading_ = false;
    errorMessage_ = succeeded ? QString{} : error;
    if (succeeded) {
        filter_ = QStringLiteral("all");
        selectedRecordIds_.clear();
        selectedRecordId_.clear();
        selectionAnchorRecordId_.clear();
        retainHiddenSelection_ = false;
        refreshSnapshot(true);
        publishDatasetState();
    }
    emit changed();
}

void DatasetLabelController::applySnapshot(DatasetLabelSnapshot nextSnapshot, bool resetModel)
{
    bool sameRecords = snapshot_.records.size() == nextSnapshot.records.size();
    for (qsizetype index = 0; sameRecords && index < snapshot_.records.size(); ++index) {
        sameRecords =
            snapshot_.records.at(index).recordId == nextSnapshot.records.at(index).recordId;
    }

    const bool normalizeClass2Filter =
        filter_ == QStringLiteral("class2") && nextSnapshot.classes.size() != 3;
    if (resetModel || !sameRecords || normalizeClass2Filter) {
        beginResetModel();
        snapshot_ = std::move(nextSnapshot);
        datasetRoot_ = QFileInfo(snapshot_.manifestPath).absolutePath();
        if (normalizeClass2Filter)
            filter_ = QStringLiteral("all");
        rebuildFilteredRows();
        updateSelectedProjection();
        endResetModel();
        return;
    }

    const QSet<QString> previousSelectedRecordIds = selectedRecordIds_;
    const bool cropRootChanged = snapshot_.manifestPath != nextSnapshot.manifestPath;
    QVector<int> changedSourceRows;
    for (qsizetype index = 0; index < snapshot_.records.size(); ++index) {
        if (snapshot_.records.at(index).state != nextSnapshot.records.at(index).state)
            changedSourceRows.append(static_cast<int>(index));
    }

    QVector<int> nextFilteredRows;
    nextFilteredRows.reserve(nextSnapshot.records.size());
    for (qsizetype index = 0; index < nextSnapshot.records.size(); ++index) {
        if (matchesFilter(nextSnapshot.records.at(index).state))
            nextFilteredRows.append(static_cast<int>(index));
    }

    QSet<int> nextRows;
    nextRows.reserve(nextFilteredRows.size());
    for (int sourceRow : nextFilteredRows)
        nextRows.insert(sourceRow);

    for (int row = filteredRows_.size() - 1; row >= 0; --row) {
        if (!nextRows.contains(filteredRows_.at(row))) {
            beginRemoveRows({}, row, row);
            filteredRows_.removeAt(row);
            endRemoveRows();
        }
    }

    snapshot_ = std::move(nextSnapshot);
    datasetRoot_ = QFileInfo(snapshot_.manifestPath).absolutePath();

    for (int row = 0; row < nextFilteredRows.size(); ++row) {
        if (row >= filteredRows_.size() || filteredRows_.at(row) != nextFilteredRows.at(row)) {
            beginInsertRows({}, row, row);
            filteredRows_.insert(row, nextFilteredRows.at(row));
            endInsertRows();
        }
    }

    updateSelectedProjection();

    for (int sourceRow : changedSourceRows) {
        const int row = filteredRows_.indexOf(sourceRow);
        if (row >= 0)
            emit dataChanged(index(row), index(row), {StateRole});
    }
    if (cropRootChanged && !filteredRows_.isEmpty()) {
        emit dataChanged(index(0), index(filteredRows_.size() - 1), {CropUrlRole});
    }
    if (previousSelectedRecordIds != selectedRecordIds_)
        emitSelectedRowsChanged(previousSelectedRecordIds);
}

void DatasetLabelController::rebuildFilteredRows()
{
    filteredRows_.clear();
    filteredRows_.reserve(snapshot_.records.size());
    for (qsizetype index = 0; index < snapshot_.records.size(); ++index) {
        if (matchesFilter(snapshot_.records.at(index).state))
            filteredRows_.append(static_cast<int>(index));
    }
}

void DatasetLabelController::updateSelectedProjection(bool selectFirstIfEmpty)
{
    QSet<QString> existingRecordIds;
    existingRecordIds.reserve(snapshot_.records.size());
    for (const DatasetLabelRecordState &record : snapshot_.records)
        existingRecordIds.insert(record.recordId);

    for (auto it = selectedRecordIds_.begin(); it != selectedRecordIds_.end();) {
        if (!existingRecordIds.contains(*it) ||
            (!retainHiddenSelection_ && !isMatchingRecord(*it))) {
            it = selectedRecordIds_.erase(it);
        } else {
            ++it;
        }
    }
    if (!selectedRecordId_.isEmpty() && !selectedRecordIds_.contains(selectedRecordId_))
        selectedRecordId_.clear();
    if (selectedRecordIds_.isEmpty() && selectFirstIfEmpty && !filteredRows_.isEmpty()) {
        selectedRecordId_ = snapshot_.records.at(filteredRows_.first()).recordId;
        selectedRecordIds_.insert(selectedRecordId_);
    } else if (selectedRecordId_.isEmpty() && !selectedRecordIds_.isEmpty()) {
        for (int sourceRow : filteredRows_) {
            const QString &recordId = snapshot_.records.at(sourceRow).recordId;
            if (selectedRecordIds_.contains(recordId)) {
                selectedRecordId_ = recordId;
                break;
            }
        }
        if (selectedRecordId_.isEmpty())
            selectedRecordId_ = *selectedRecordIds_.cbegin();
    }

    selectedIndex_ = -1;
    selectedCropUrl_.clear();
    const int row = modelRowForRecordId(selectedRecordId_);
    if (row >= 0) {
        selectedIndex_ = row;
        selectedCropUrl_ = cropUrl(snapshot_.records.at(filteredRows_.at(row)));
    } else if (!selectedRecordId_.isEmpty()) {
        const auto record = std::find_if(snapshot_.records.cbegin(), snapshot_.records.cend(),
                                         [this](const DatasetLabelRecordState &value) {
                                             return value.recordId == selectedRecordId_;
                                         });
        if (record != snapshot_.records.cend())
            selectedCropUrl_ = cropUrl(*record);
    }
}

void DatasetLabelController::setSelectedRecordId(const QString &recordId)
{
    setSelectedRecords(recordId.isEmpty() ? QSet<QString>{} : QSet<QString>{recordId}, recordId);
}

void DatasetLabelController::setSelectedRecords(const QSet<QString> &recordIds,
                                                const QString &currentRecordId,
                                                bool selectFirstIfEmpty)
{
    const QSet<QString> previousRecordIds = selectedRecordIds_;
    selectedRecordIds_ = recordIds;
    selectedRecordId_ = currentRecordId;
    retainHiddenSelection_ = false;
    updateSelectedProjection(selectFirstIfEmpty);
    if (previousRecordIds != selectedRecordIds_)
        emitSelectedRowsChanged(previousRecordIds);
}

void DatasetLabelController::emitSelectedRowsChanged(const QSet<QString> &previousRecordIds)
{
    QVector<int> changedRows;
    changedRows.reserve(previousRecordIds.size() + selectedRecordIds_.size());
    for (int row = 0; row < filteredRows_.size(); ++row) {
        const QString &recordId = snapshot_.records.at(filteredRows_.at(row)).recordId;
        if (previousRecordIds.contains(recordId) != selectedRecordIds_.contains(recordId))
            changedRows.append(row);
    }
    if (changedRows.size() <= 2) {
        for (int row : changedRows)
            emit dataChanged(index(row), index(row), {SelectedRole});
        return;
    }

    int firstChangedRow = changedRows.first();
    int previousChangedRow = firstChangedRow;
    for (qsizetype index = 1; index < changedRows.size(); ++index) {
        const int row = changedRows.at(index);
        if (row != previousChangedRow + 1) {
            emit dataChanged(this->index(firstChangedRow), this->index(previousChangedRow),
                             {SelectedRole});
            firstChangedRow = row;
        }
        previousChangedRow = row;
    }
    emit dataChanged(index(firstChangedRow), index(previousChangedRow), {SelectedRole});
}

QVector<QString> DatasetLabelController::selectedRecordIdsInVisibleOrder() const
{
    QVector<QString> recordIds;
    recordIds.reserve(selectedRecordIds_.size());
    for (int sourceRow : filteredRows_) {
        const QString &recordId = snapshot_.records.at(sourceRow).recordId;
        if (selectedRecordIds_.contains(recordId))
            recordIds.append(recordId);
    }
    return recordIds;
}

int DatasetLabelController::modelRowForRecordId(const QString &recordId) const
{
    if (recordId.isEmpty())
        return -1;
    for (qsizetype row = 0; row < filteredRows_.size(); ++row) {
        if (snapshot_.records.at(filteredRows_.at(row)).recordId == recordId)
            return static_cast<int>(row);
    }
    return -1;
}

bool DatasetLabelController::matchesFilter(DatasetLabelState state) const
{
    return filter_ == QStringLiteral("all") || stateText(state) == filter_;
}

bool DatasetLabelController::isMatchingRecord(const QString &recordId) const
{
    return modelRowForRecordId(recordId) >= 0;
}

QUrl DatasetLabelController::cropUrl(const DatasetLabelRecordState &record) const
{
    return QUrl::fromLocalFile(QDir(datasetRoot_).absoluteFilePath(record.cropPath));
}

bool DatasetLabelController::open(const QUrl &manifestUrl)
{
    if (rejectWhileLoading())
        return false;
    QString error;
    const QString manifestPath = localPath(manifestUrl, QStringLiteral("Dataset"), &error);
    if (manifestPath.isEmpty())
        return finish(false, error);
    loading_ = true;
    errorMessage_ = QStringLiteral("Loading Dataset...");
    emit changed();

    openThread_ = QThread::create([this, manifestPath] {
        QString error;
        const bool success = service_.open(manifestPath, &error);
        const QMutexLocker lock(&openResultMutex_);
        openSucceeded_ = success;
        openError_ = std::move(error);
    });
    connect(openThread_, &QThread::finished, this, &DatasetLabelController::finishOpen);
    openThread_->start();
    return true;
}

bool DatasetLabelController::configureClassCount(int classCount)
{
    if (rejectWhileLoading())
        return false;
    QString error;
    const bool success = service_.configureClassCount(classCount, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::renameClass(int index, const QString &name)
{
    if (rejectWhileLoading())
        return false;
    if (index < 0 || index >= snapshot_.classes.size())
        return finish(false, QStringLiteral("Unknown Class index."));
    QString error;
    const bool success = service_.renameClass(snapshot_.classes.at(index).id, name, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::assignClass(const QString &classId)
{
    if (rejectWhileLoading())
        return false;
    const QVector<QString> selectedRecordIds = selectedRecordIdsInVisibleOrder();
    if (selectedRecordIds.isEmpty())
        return finish(false, QStringLiteral("No Droplet Crop is selected."));
    const int highestSelectedRow = modelRowForRecordId(selectedRecordIds.last());
    const bool selectionIncludesFinalRow = highestSelectedRow == filteredRows_.size() - 1;
    const QString nextRecordId =
        selectionIncludesFinalRow
            ? selectedRecordIds.last()
            : snapshot_.records.at(filteredRows_.at(highestSelectedRow + 1)).recordId;
    QString error;
    const bool success = service_.assignClass(selectedRecordIds, classId, &error);
    if (!success)
        return finish(false, error);

    const QSet<QString> previousRecordIds = selectedRecordIds_;
    selectedRecordIds_ =
        selectionIncludesFinalRow ? QSet<QString>{selectedRecordIds.last()}
                                  : QSet<QString>{nextRecordId};
    selectedRecordId_ = nextRecordId;
    selectionAnchorRecordId_ = nextRecordId;
    retainHiddenSelection_ = selectionIncludesFinalRow;
    refreshSnapshot();
    if (previousRecordIds != selectedRecordIds_)
        emitSelectedRowsChanged(previousRecordIds);
    return finish(true, {});
}

bool DatasetLabelController::exclude()
{
    if (rejectWhileLoading())
        return false;
    const QVector<QString> selectedRecordIds = selectedRecordIdsInVisibleOrder();
    if (selectedRecordIds.isEmpty())
        return finish(false, QStringLiteral("No Droplet Crop is selected."));
    const int highestSelectedRow = modelRowForRecordId(selectedRecordIds.last());
    const bool selectionIncludesFinalRow = highestSelectedRow == filteredRows_.size() - 1;
    const QString nextRecordId =
        selectionIncludesFinalRow
            ? selectedRecordIds.last()
            : snapshot_.records.at(filteredRows_.at(highestSelectedRow + 1)).recordId;
    QString error;
    const bool success = service_.exclude(selectedRecordIds, &error);
    if (!success)
        return finish(false, error);

    const QSet<QString> previousRecordIds = selectedRecordIds_;
    selectedRecordIds_ =
        selectionIncludesFinalRow ? QSet<QString>{selectedRecordIds.last()}
                                  : QSet<QString>{nextRecordId};
    selectedRecordId_ = nextRecordId;
    selectionAnchorRecordId_ = nextRecordId;
    retainHiddenSelection_ = selectionIncludesFinalRow;
    refreshSnapshot();
    if (previousRecordIds != selectedRecordIds_)
        emitSelectedRowsChanged(previousRecordIds);
    return finish(true, {});
}

bool DatasetLabelController::undo()
{
    if (rejectWhileLoading())
        return false;
    QString error;
    const bool success = service_.undo(&error);
    return finish(success, error, true);
}

bool DatasetLabelController::previous()
{
    if (rejectWhileLoading())
        return false;
    const int index = selectedIndex();
    if (index <= 0)
        return finish(false, QStringLiteral("No previous Droplet Crop is available."));
    setSelectedRecordId(snapshot_.records.at(filteredRows_.at(index - 1)).recordId);
    return finish(true, {});
}

bool DatasetLabelController::next()
{
    if (rejectWhileLoading())
        return false;
    const int index = selectedIndex();
    if (index < 0 || index + 1 >= filteredRows_.size())
        return finish(false, QStringLiteral("No next Droplet Crop is available."));
    setSelectedRecordId(snapshot_.records.at(filteredRows_.at(index + 1)).recordId);
    return finish(true, {});
}

bool DatasetLabelController::select(const QString &recordId, bool control, bool shift)
{
    if (rejectWhileLoading())
        return false;
    if (!isMatchingRecord(recordId))
        return finish(false, QStringLiteral("Selected Droplet Crop is unavailable in the current filter."));
    const int clickedRow = modelRowForRecordId(recordId);
    if (shift && isMatchingRecord(selectionAnchorRecordId_)) {
        const int anchorRow = modelRowForRecordId(selectionAnchorRecordId_);
        const int firstRow = std::min(anchorRow, clickedRow);
        const int lastRow = std::max(anchorRow, clickedRow);
        QSet<QString> range;
        range.reserve(lastRow - firstRow + 1);
        for (int row = firstRow; row <= lastRow; ++row)
            range.insert(snapshot_.records.at(filteredRows_.at(row)).recordId);
        setSelectedRecords(range, recordId);
    } else if (control) {
        QSet<QString> toggled = selectedRecordIds_;
        if (toggled.contains(recordId))
            toggled.remove(recordId);
        else
            toggled.insert(recordId);
        const QString current =
            toggled.contains(recordId) ? recordId
                                       : (toggled.isEmpty() ? QString{} : selectedRecordId_);
        setSelectedRecords(toggled, current);
    } else {
        setSelectedRecordId(recordId);
        selectionAnchorRecordId_ = recordId;
    }
    return finish(true, {});
}

bool DatasetLabelController::setFilter(const QString &filter)
{
    if (rejectWhileLoading())
        return false;
    if (!isSupportedFilter(filter))
        return finish(false, QStringLiteral("Unknown Label filter."));
    if (filter == QStringLiteral("class2") && !class2Enabled())
        return finish(false, QStringLiteral("Class 2 is unavailable for a two-class Dataset."));
    if (filter_ == filter)
        return finish(true, {});

    beginResetModel();
    filter_ = filter;
    selectionAnchorRecordId_.clear();
    retainHiddenSelection_ = false;
    rebuildFilteredRows();
    updateSelectedProjection();
    endResetModel();
    return finish(true, {});
}

bool DatasetLabelController::saveAs(const QUrl &destinationFolderUrl)
{
    if (rejectWhileLoading())
        return false;
    QString error;
    const QString destinationFolder =
        localPath(destinationFolderUrl, QStringLiteral("Save As destination"), &error);
    if (destinationFolder.isEmpty())
        return finish(false, error);
    const bool success = service_.saveAs(destinationFolder, &error);
    if (success) {
        refreshSnapshot();
        publishDatasetState();
    }
    return finish(success, error);
}

} // namespace desktop_app::v2::dataset
