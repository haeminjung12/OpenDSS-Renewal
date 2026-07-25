#include "dataset_label_controller.h"

#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <QDir>
#include <QFileInfo>

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
    : QObject(parent)
    , service_(operations)
    , stateStore_(stateStore)
{
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

QVariantList DatasetLabelController::records() const
{
    return records_;
}

QVariantList DatasetLabelController::filteredRecords() const { return filteredRecords_; }
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

void DatasetLabelController::refreshSnapshot()
{
    snapshot_ = service_.snapshot();
    if (filter_ == QStringLiteral("class2") && !class2Enabled())
        filter_ = QStringLiteral("all");

    records_.clear();
    records_.reserve(snapshot_.records.size());
    const QString datasetRoot = QFileInfo(snapshot_.manifestPath).absolutePath();
    for (const DatasetLabelRecordState &record : snapshot_.records) {
        records_.append(
            QVariantMap{{QStringLiteral("recordId"), record.recordId},
                        {QStringLiteral("cropUrl"),
                         QUrl::fromLocalFile(QDir(datasetRoot).absoluteFilePath(record.cropPath))},
                        {QStringLiteral("state"), stateText(record.state)}});
    }
    refreshFilteredProjection();
}

void DatasetLabelController::refreshFilteredProjection()
{
    filteredRecords_.clear();
    filteredRecords_.reserve(records_.size());
    for (const QVariant &value : records_) {
        const QVariantMap record = value.toMap();
        if (filter_ == QStringLiteral("all") ||
            record.value(QStringLiteral("state")).toString() == filter_) {
            filteredRecords_.append(record);
        }
    }

    if (!isMatchingRecord(selectedRecordId_)) {
        selectedRecordId_ = filteredRecords_.isEmpty()
            ? QString{}
            : filteredRecords_.first().toMap().value(QStringLiteral("recordId")).toString();
    }

    selectedIndex_ = -1;
    selectedCropUrl_.clear();
    for (qsizetype index = 0; index < filteredRecords_.size(); ++index) {
        const QVariantMap record = filteredRecords_.at(index).toMap();
        if (record.value(QStringLiteral("recordId")).toString() == selectedRecordId_) {
            selectedIndex_ = static_cast<int>(index);
            selectedCropUrl_ = record.value(QStringLiteral("cropUrl")).toUrl();
            break;
        }
    }
}

bool DatasetLabelController::isMatchingRecord(const QString &recordId) const
{
    for (const QVariant &value : filteredRecords_) {
        if (value.toMap().value(QStringLiteral("recordId")).toString() == recordId)
            return true;
    }
    return false;
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
        refreshSnapshot();
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
    selectedRecordId_ =
        filteredRecords_.at(index - 1).toMap().value(QStringLiteral("recordId")).toString();
    refreshFilteredProjection();
    return finish(true, {});
}

bool DatasetLabelController::next()
{
    const int index = selectedIndex();
    if (index < 0 || index + 1 >= filteredRecords_.size())
        return finish(false, QStringLiteral("No next Droplet Crop is available."));
    selectedRecordId_ =
        filteredRecords_.at(index + 1).toMap().value(QStringLiteral("recordId")).toString();
    refreshFilteredProjection();
    return finish(true, {});
}

bool DatasetLabelController::select(const QString &recordId)
{
    if (!isMatchingRecord(recordId))
        return finish(false, QStringLiteral("Selected Droplet Crop is unavailable in the current filter."));
    selectedRecordId_ = recordId;
    refreshFilteredProjection();
    return finish(true, {});
}

bool DatasetLabelController::setFilter(const QString &filter)
{
    if (!isSupportedFilter(filter))
        return finish(false, QStringLiteral("Unknown Label filter."));
    if (filter == QStringLiteral("class2") && !class2Enabled())
        return finish(false, QStringLiteral("Class 2 is unavailable for a two-class Dataset."));
    filter_ = filter;
    selectedRecordId_.clear();
    refreshFilteredProjection();
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
