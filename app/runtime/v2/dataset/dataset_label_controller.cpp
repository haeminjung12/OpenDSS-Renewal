#include "dataset_label_controller.h"

#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

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
        return record.recordId == selectedRecordId_;
    default:
        return {};
    }
}

QHash<int, QByteArray> DatasetLabelController::roleNames() const
{
    return {
        {RecordIdRole, QByteArrayLiteral("recordId")},
        {CropUrlRole, QByteArrayLiteral("cropUrl")},
        {StateRole, QByteArrayLiteral("state")},
        {SelectedRole, QByteArrayLiteral("selected")},
    };
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

    const QString previousSelectedRecordId = selectedRecordId_;
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
    if (previousSelectedRecordId != selectedRecordId_)
        emitSelectedRowsChanged(previousSelectedRecordId);
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

void DatasetLabelController::updateSelectedProjection()
{
    if (!isMatchingRecord(selectedRecordId_)) {
        selectedRecordId_ =
            filteredRows_.isEmpty() ? QString{} : snapshot_.records.at(filteredRows_.first()).recordId;
    }

    selectedIndex_ = -1;
    selectedCropUrl_.clear();
    const int row = modelRowForRecordId(selectedRecordId_);
    if (row >= 0) {
        selectedIndex_ = row;
        selectedCropUrl_ = cropUrl(snapshot_.records.at(filteredRows_.at(row)));
    }
}

void DatasetLabelController::setSelectedRecordId(const QString &recordId)
{
    const QString previousRecordId = selectedRecordId_;
    selectedRecordId_ = recordId;
    updateSelectedProjection();
    if (previousRecordId != selectedRecordId_)
        emitSelectedRowsChanged(previousRecordId);
}

void DatasetLabelController::emitSelectedRowsChanged(const QString &previousRecordId)
{
    const int previousRow = modelRowForRecordId(previousRecordId);
    const int currentRow = modelRowForRecordId(selectedRecordId_);
    if (previousRow >= 0)
        emit dataChanged(index(previousRow), index(previousRow), {SelectedRole});
    if (currentRow >= 0 && currentRow != previousRow)
        emit dataChanged(index(currentRow), index(currentRow), {SelectedRole});
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
    QString error;
    const QString manifestPath = localPath(manifestUrl, QStringLiteral("Dataset"), &error);
    if (manifestPath.isEmpty())
        return finish(false, error);
    const bool success = service_.open(manifestPath, &error);
    if (success) {
        filter_ = QStringLiteral("all");
        selectedRecordId_.clear();
        refreshSnapshot(true);
        publishDatasetState();
    }
    return finish(success, error);
}

bool DatasetLabelController::configureClassCount(int classCount)
{
    QString error;
    const bool success = service_.configureClassCount(classCount, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::renameClass(int index, const QString &name)
{
    if (index < 0 || index >= snapshot_.classes.size())
        return finish(false, QStringLiteral("Unknown Class index."));
    QString error;
    const bool success = service_.renameClass(snapshot_.classes.at(index).id, name, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::assignClass(const QString &classId)
{
    if (selectedRecordId_.isEmpty())
        return finish(false, QStringLiteral("No Droplet Crop is selected."));
    QString error;
    const bool success = service_.assignClass(selectedRecordId_, classId, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::exclude()
{
    if (selectedRecordId_.isEmpty())
        return finish(false, QStringLiteral("No Droplet Crop is selected."));
    QString error;
    const bool success = service_.exclude(selectedRecordId_, &error);
    return finish(success, error, true);
}

bool DatasetLabelController::undo()
{
    QString error;
    const bool success = service_.undo(&error);
    return finish(success, error, true);
}

bool DatasetLabelController::previous()
{
    const int index = selectedIndex();
    if (index <= 0)
        return finish(false, QStringLiteral("No previous Droplet Crop is available."));
    setSelectedRecordId(snapshot_.records.at(filteredRows_.at(index - 1)).recordId);
    return finish(true, {});
}

bool DatasetLabelController::next()
{
    const int index = selectedIndex();
    if (index < 0 || index + 1 >= filteredRows_.size())
        return finish(false, QStringLiteral("No next Droplet Crop is available."));
    setSelectedRecordId(snapshot_.records.at(filteredRows_.at(index + 1)).recordId);
    return finish(true, {});
}

bool DatasetLabelController::select(const QString &recordId)
{
    if (!isMatchingRecord(recordId))
        return finish(false, QStringLiteral("Selected Droplet Crop is unavailable in the current filter."));
    setSelectedRecordId(recordId);
    return finish(true, {});
}

bool DatasetLabelController::setFilter(const QString &filter)
{
    if (!isSupportedFilter(filter))
        return finish(false, QStringLiteral("Unknown Label filter."));
    if (filter == QStringLiteral("class2") && !class2Enabled())
        return finish(false, QStringLiteral("Class 2 is unavailable for a two-class Dataset."));
    if (filter_ == filter)
        return finish(true, {});

    beginResetModel();
    filter_ = filter;
    selectedRecordId_.clear();
    rebuildFilteredRows();
    updateSelectedProjection();
    endResetModel();
    return finish(true, {});
}

bool DatasetLabelController::saveAs(const QUrl &destinationFolderUrl)
{
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
