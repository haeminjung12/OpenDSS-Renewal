#include "../v2/dataset/dataset_label_controller.h"
#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QMetaProperty>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <iostream>
#include <algorithm>
#include <utility>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;

namespace desktop_app::v2::dataset {

struct DatasetLabelControllerTestAccess {
    static void applySnapshot(DatasetLabelController &controller,
                              DatasetLabelSnapshot snapshot, bool resetModel)
    {
        controller.applySnapshot(std::move(snapshot), resetModel);
    }

    static DatasetLabelSnapshot snapshot(const DatasetLabelController &controller)
    {
        return controller.snapshot_;
    }
};

} // namespace desktop_app::v2::dataset

namespace {

bool check(bool value, const QString &message)
{
    if (!value)
        std::cerr << message.toStdString() << '\n';
    return value;
}

QString hashFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(
        QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
}

DatasetManifestData fixture(const QString &root)
{
    QDir().mkpath(QDir(root).filePath(QStringLiteral("crops")));
    QImage image(8, 8, QImage::Format_Grayscale8);
    image.fill(77);
    image.save(QDir(root).filePath(QStringLiteral("crops/one.png")), "PNG");
    image.fill(88);
    image.save(QDir(root).filePath(QStringLiteral("crops/two.png")), "PNG");

    DatasetManifestData data;
    data.datasetId = QStringLiteral("fixture-dataset");
    data.provenance.name = QStringLiteral("Fixture Dataset");
    data.provenance.experimentType = QStringLiteral("fixture");
    data.provenance.notes = QStringLiteral("controller test");
    data.provenance.opendssVersion = QStringLiteral("v2");
    data.provenance.createdAt = QStringLiteral("2026-07-24T10:00:00Z");
    data.provenance.status = QStringLiteral("completed");
    data.provenance.updatedAt = QStringLiteral("2026-07-24T10:00:00Z");
    data.provenance.captureStartedAt = QStringLiteral("2026-07-24T10:00:00Z");
    data.provenance.captureEndedAt = QStringLiteral("2026-07-24T10:01:00Z");
    data.provenance.stopReason = QStringLiteral("user");
    data.provenance.sequence.frameCount = 2;
    data.provenance.sequence.imageWidth = 8;
    data.provenance.sequence.imageHeight = 8;
    data.provenance.sequence.bitDepth = 8;
    data.provenance.sequence.nominalFps = 1.0;
    data.records = {{QStringLiteral("r1"), QStringLiteral("crops/one.png"),
                     hashFile(QDir(root).filePath(QStringLiteral("crops/one.png"))),
                     QStringLiteral("frame-1"), QStringLiteral("event-1"),
                     QStringLiteral("2026-07-24T10:00:00Z"), QRect(0, 0, 8, 8), 1},
                    {QStringLiteral("r2"), QStringLiteral("crops/two.png"),
                     hashFile(QDir(root).filePath(QStringLiteral("crops/two.png"))),
                     QStringLiteral("frame-2"), QStringLiteral("event-2"),
                     QStringLiteral("2026-07-24T10:00:01Z"), QRect(0, 0, 8, 8), 2}};
    return data;
}

bool waitForOpen(DatasetLabelController &controller, int timeoutMilliseconds = 15000)
{
    QElapsedTimer timer;
    timer.start();
    while (controller.loading() && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents();
    return !controller.loading();
}

DatasetLabelSnapshot largeFixture(const QString &root, int recordCount)
{
    DatasetLabelSnapshot snapshot;
    snapshot.manifestPath = QDir(root).filePath(QStringLiteral("dataset.json"));
    snapshot.datasetId = QStringLiteral("large-fixture-dataset");
    snapshot.classes = {{QStringLiteral("0"), QStringLiteral("Class 0")},
                        {QStringLiteral("1"), QStringLiteral("Class 1")},
                        {QStringLiteral("2"), QStringLiteral("Class 2")}};
    snapshot.counts.classCounts.fill(0, 3);
    snapshot.counts.unreviewed = recordCount;
    snapshot.records.reserve(recordCount);
    for (int index = 0; index < recordCount; ++index) {
        const QString suffix = QStringLiteral("%1").arg(index, 5, 10, QLatin1Char('0'));
        snapshot.records.append({QStringLiteral("large-") + suffix,
                                 QStringLiteral("crops/") + suffix + QStringLiteral(".png"),
                                 DatasetLabelState::Unlabeled});
    }
    return snapshot;
}

struct ModelSignalEvidence {
    int modelResetCount = 0;
    int rowsInsertedCount = 0;
    int rowsRemovedCount = 0;
    QVector<QPair<int, int>> dataChangedRows;
    QVector<QList<int>> dataChangedRoles;

    void clear()
    {
        modelResetCount = 0;
        rowsInsertedCount = 0;
        rowsRemovedCount = 0;
        dataChangedRows.clear();
        dataChangedRoles.clear();
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    const QString source = QDir(temporary.path()).filePath(QStringLiteral("source folder"));
    QDir().mkpath(source);
    const QString manifestPath = QDir(source).filePath(QStringLiteral("dataset.json"));
    QString error;
    if (!check(DatasetManifestV2::save(manifestPath, fixture(source), &error), error))
        return 1;

    OperationCoordinator operations;
    ApplicationStateStore stateStore;
    DatasetLabelController controller(operations, stateStore);
    int changedCount = 0;
    QObject::connect(&controller, &DatasetLabelController::changed,
                     [&changedCount] { ++changedCount; });

    const QMetaObject *metaObject = controller.metaObject();
    const int changedSignal = metaObject->indexOfSignal("changed()");
    const char *propertyNames[] = {
        "presentation", "manifestUrl", "datasetId", "totalCount", "labeledCount",
        "unreviewedCount", "excludedCount", "classCount", "class0Count", "class1Count",
        "class2Count", "classNames", "class2Enabled", "canUndo", "selectedRecordId",
        "selectedCropUrl", "selectedIndex", "filter", "errorMessage",
        "loading",
    };
    bool propertiesUseChangedSignal = changedSignal >= 0;
    for (const char *propertyName : propertyNames) {
        const int index = metaObject->indexOfProperty(propertyName);
        propertiesUseChangedSignal =
            propertiesUseChangedSignal && index >= 0 &&
            metaObject->property(index).notifySignalIndex() == changedSignal;
    }
    const int manifestUrlProperty = metaObject->indexOfProperty("manifestUrl");
    const int cropUrlProperty = metaObject->indexOfProperty("selectedCropUrl");
    if (!check(propertiesUseChangedSignal,
               QStringLiteral("controller meta-properties do not notify through changed")) ||
        !check(manifestUrlProperty >= 0 &&
                   metaObject->property(manifestUrlProperty).metaType().id() == QMetaType::QUrl &&
                   cropUrlProperty >= 0 &&
                   metaObject->property(cropUrlProperty).metaType().id() == QMetaType::QUrl,
               QStringLiteral("URL projections are not QUrl meta-properties")) ||
        !check(metaObject->indexOfMethod("open(QUrl)") >= 0,
               QStringLiteral("QUrl open invokable missing")) ||
        !check(metaObject->indexOfMethod("saveAs(QUrl)") >= 0,
               QStringLiteral("QUrl Save As invokable missing")) ||
        !check(metaObject->indexOfMethod("renameClass(int,QString)") >= 0,
               QStringLiteral("index-based class rename invokable missing")) ||
        !check(controller.presentation() == QStringLiteral("empty"),
               QStringLiteral("initial presentation is not empty")) ||
        !check(!controller.open(QUrl(QStringLiteral("https://example.invalid/dataset.json"))),
               QStringLiteral("non-local Dataset URL was accepted")) ||
        !check(controller.presentation() == QStringLiteral("empty") && changedCount == 1,
               QStringLiteral("failed open changed the presentation or missed changed signal")) ||
        !check(controller.open(QUrl::fromLocalFile(
                   QDir(temporary.path()).filePath(QStringLiteral("missing.json")))),
               QStringLiteral("missing Dataset load was not started")) ||
        !check(waitForOpen(controller), QStringLiteral("missing Dataset load did not finish")) ||
        !check(!controller.errorMessage().isEmpty(), QStringLiteral("invalid path error missing")) ||
        !check(controller.presentation() == QStringLiteral("empty") && changedCount == 3,
               QStringLiteral("missing Dataset changed the presentation or signal count")) ||
        !check(controller.open(QUrl::fromLocalFile(manifestPath)), controller.errorMessage()) ||
        !check(controller.loading() &&
                   controller.presentation() == QStringLiteral("empty") &&
                   controller.errorMessage() == QStringLiteral("Loading Dataset..."),
               QStringLiteral("accepted Dataset open did not publish loading")) ||
        !check(!controller.setFilter(QStringLiteral("all")) &&
                   controller.errorMessage() == QStringLiteral("Dataset is still loading."),
               QStringLiteral("action was not rejected while Dataset loaded")) ||
        !check(waitForOpen(controller), QStringLiteral("Dataset load did not finish")) ||
        !check(controller.errorMessage().isEmpty(), controller.errorMessage()) ||
        !check(controller.rowCount() == 2,
               QStringLiteral("controller model does not expose both records")) ||
        !check(controller.roleNames().value(DatasetLabelController::RecordIdRole) ==
                   QByteArrayLiteral("recordId") &&
                   controller.roleNames().value(DatasetLabelController::CropUrlRole) ==
                       QByteArrayLiteral("cropUrl") &&
                   controller.roleNames().value(DatasetLabelController::StateRole) ==
                       QByteArrayLiteral("state") &&
                   controller.roleNames().value(DatasetLabelController::SelectedRole) ==
                       QByteArrayLiteral("selected"),
               QStringLiteral("controller model roles are incorrect")) ||
        !check(controller.presentation() == QStringLiteral("classDefinition"),
               QStringLiteral("unconfigured Dataset is not in classDefinition")) ||
        !check(controller.totalCount() == 2 && controller.unreviewedCount() == 2,
               QStringLiteral("initial counts are incorrect")) ||
        !check(!controller.canUndo(), QStringLiteral("open Dataset can undo")) ||
        !check(controller.manifestUrl() == QUrl::fromLocalFile(manifestPath),
               QStringLiteral("manifest URL projection is incorrect")) ||
        !check(controller.selectedRecordId() == QStringLiteral("r1") &&
                   controller.selectedCropUrl() ==
                       QUrl::fromLocalFile(QDir(source).filePath(QStringLiteral("crops/one.png"))) &&
                   controller.selectedCropUrl().isLocalFile(),
               QStringLiteral("selected crop URL is not the resolved absolute file URL")) ||
        !check(stateStore.snapshot().dataset.ready &&
                   stateStore.snapshot().dataset.path == manifestPath,
               QStringLiteral("dataset state was not published")) ||
        !check(changedCount == 6, QStringLiteral("successful open missed changed signal")))
        return 2;

    if (!check(controller.configureClassCount(3), controller.errorMessage()) ||
        !check(controller.presentation() == QStringLiteral("ready") && controller.canUndo(),
               QStringLiteral("class configuration projection is incorrect")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.presentation() == QStringLiteral("classDefinition") &&
                   !controller.canUndo(),
               QStringLiteral("undo did not restore unconfigured state")) ||
        !check(controller.configureClassCount(3), controller.errorMessage()) ||
        !check(controller.select(QStringLiteral("r2")), controller.errorMessage()) ||
        !check(controller.assignClass(QStringLiteral("2")), controller.errorMessage()) ||
        !check(controller.setFilter(QStringLiteral("class2")), controller.errorMessage()) ||
        !check(controller.rowCount() == 1 &&
                   controller.data(controller.index(0), DatasetLabelController::RecordIdRole)
                           .toString() == QStringLiteral("r2") &&
                   controller.selectedRecordId() == QStringLiteral("r2") &&
                   controller.class2Count() == 1,
               QStringLiteral("filter projection is incorrect")) ||
        !check(!controller.configureClassCount(2),
               QStringLiteral("three-to-two change with Class 2 label succeeded")) ||
        !check(controller.classCount() == 3 && controller.filter() == QStringLiteral("class2") &&
                   controller.presentation() == QStringLiteral("ready"),
               QStringLiteral("rejected schema change corrupted projection")) ||
        !check(controller.exclude(), controller.errorMessage()) ||
        !check(controller.rowCount() == 0 &&
                   controller.selectedRecordId() == QStringLiteral("r2") &&
                   controller.selectedIndex() == -1,
               QStringLiteral("final filtered Crop did not stay selected after exclusion")) ||
        !check(controller.configureClassCount(2), controller.errorMessage()) ||
        !check(controller.classCount() == 2 && !controller.class2Enabled() &&
                   controller.filter() == QStringLiteral("all") && controller.canUndo(),
               QStringLiteral("successful three-to-two normalization is incorrect")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.classCount() == 3 && controller.filter() == QStringLiteral("all") &&
                   !controller.canUndo(),
               QStringLiteral("schema undo projection is incorrect")))
        return 3;

    if (!check(controller.open(QUrl::fromLocalFile(manifestPath)), controller.errorMessage()) ||
        !check(waitForOpen(controller), QStringLiteral("Dataset reopen did not finish")) ||
        !check(controller.errorMessage().isEmpty(), controller.errorMessage()) ||
        !check(!controller.canUndo(), QStringLiteral("reopen retained undo state")) ||
        !check(controller.classNames() == QVariantList{QStringLiteral("Class 0"),
                                                       QStringLiteral("Class 1"),
                                                       QStringLiteral("Class 2")},
               QStringLiteral("class-name projection is incorrect")) ||
        !check(!controller.renameClass(3, QStringLiteral("Unexpected")),
               QStringLiteral("out-of-range class rename was accepted")) ||
        !check(controller.renameClass(1, QStringLiteral("Single cell")), controller.errorMessage()) ||
        !check(controller.classNames().at(1).toString() == QStringLiteral("Single cell"),
               QStringLiteral("class-name rename projection is incorrect")) ||
        !check(controller.setFilter(QStringLiteral("all")), controller.errorMessage()) ||
        !check(controller.select(QStringLiteral("r1")), controller.errorMessage()) ||
        !check(controller.select(QStringLiteral("r2"), true, false), controller.errorMessage()) ||
        !check(!controller.assignClass(QStringLiteral("missing")),
               QStringLiteral("unknown Class assignment succeeded")) ||
        !check(controller.selectedRecordId() == QStringLiteral("r2") &&
                   controller.data(controller.index(0),
                                   DatasetLabelController::SelectedRole).toBool() &&
                   controller.data(controller.index(1),
                                   DatasetLabelController::SelectedRole).toBool() &&
                   controller.class0Count() == 0 && controller.excludedCount() == 1,
               QStringLiteral("failed atomic assignment changed selection, labels, or advance")) ||
        !check(controller.assignClass(QStringLiteral("0")), controller.errorMessage()) ||
        !check(controller.class0Count() == 2 &&
                   controller.selectedRecordId() == QStringLiteral("r2"),
               QStringLiteral("full selection assignment was not atomic or wrapped at final")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.class0Count() == 0 && controller.excludedCount() == 1,
               QStringLiteral("full selection assignment did not undo as one mutation")) ||
        !check(controller.select(QStringLiteral("r1")), controller.errorMessage()) ||
        !check(controller.assignClass(QStringLiteral("0")), controller.errorMessage()) ||
        !check(controller.class0Count() == 1 && controller.canUndo() &&
                   controller.selectedRecordId() == QStringLiteral("r2"),
               QStringLiteral("Class assignment did not advance after the highest selection")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.class0Count() == 0 && !controller.canUndo(),
               QStringLiteral("label undo projection is incorrect")) ||
        !check(controller.select(QStringLiteral("r1")), controller.errorMessage()) ||
        !check(controller.exclude(), controller.errorMessage()) ||
        !check(controller.excludedCount() == 2 && controller.canUndo(),
               QStringLiteral("exclude projection is incorrect")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.excludedCount() == 1 && !controller.canUndo(),
               QStringLiteral("exclude undo projection is incorrect")) ||
        !check(controller.select(QStringLiteral("r1")), controller.errorMessage()) ||
        !check(controller.next(), controller.errorMessage()) ||
        !check(controller.selectedRecordId() == QStringLiteral("r2"), QStringLiteral("next did not select r2")) ||
        !check(!controller.next(), QStringLiteral("next accepted past final record")) ||
        !check(controller.presentation() == QStringLiteral("ready"),
               QStringLiteral("bounds error changed ready presentation")) ||
        !check(controller.previous(), controller.errorMessage()) ||
        !check(controller.selectedRecordId() == QStringLiteral("r1"), QStringLiteral("previous did not select r1")) ||
        !check(!controller.setFilter(QStringLiteral("unknown")),
               QStringLiteral("unknown filter was accepted")) ||
        !check(controller.presentation() == QStringLiteral("ready"),
               QStringLiteral("filter error changed ready presentation")))
        return 4;

    const QString copy = QDir(temporary.path()).filePath(QStringLiteral("copy folder"));
    if (!check(controller.assignClass(QStringLiteral("1")), controller.errorMessage()) ||
        !check(controller.canUndo(), QStringLiteral("pre-Save As mutation cannot undo")) ||
        !check(!controller.saveAs(QUrl(QStringLiteral("https://example.invalid/copy"))),
               QStringLiteral("non-local Save As URL was accepted")) ||
        !check(controller.presentation() == QStringLiteral("ready"),
               QStringLiteral("Save As error changed ready presentation")) ||
        !check(controller.saveAs(QUrl::fromLocalFile(copy)), controller.errorMessage()) ||
        !check(QFile::exists(QDir(copy).filePath(QStringLiteral("dataset.json"))),
               QStringLiteral("Save As did not create dataset.json")) ||
        !check(controller.manifestUrl() ==
                   QUrl::fromLocalFile(QDir(copy).filePath(QStringLiteral("dataset.json"))) &&
                   stateStore.snapshot().dataset.path ==
                       QDir(copy).filePath(QStringLiteral("dataset.json")),
               QStringLiteral("Save As URL projection is incorrect")) ||
        !check(!controller.canUndo(), QStringLiteral("Save As retained undo state")) ||
        !check(changedCount > 3, QStringLiteral("controller actions emitted no changed signals")))
        return 5;

    constexpr int largeRecordCount = 18072;
    QTemporaryDir largeTemporary;
    const QString largeSource =
        QDir(largeTemporary.path()).filePath(QStringLiteral("large source"));

    OperationCoordinator largeOperations;
    ApplicationStateStore largeStateStore;
    DatasetLabelController largeController(largeOperations, largeStateStore);
    ModelSignalEvidence modelSignals;
    QObject::connect(&largeController, &QAbstractItemModel::modelReset,
                     [&modelSignals] { ++modelSignals.modelResetCount; });
    QObject::connect(
        &largeController, &QAbstractItemModel::rowsInserted,
        [&modelSignals](const QModelIndex &, int, int) { ++modelSignals.rowsInsertedCount; });
    QObject::connect(
        &largeController, &QAbstractItemModel::rowsRemoved,
        [&modelSignals](const QModelIndex &, int, int) { ++modelSignals.rowsRemovedCount; });
    QObject::connect(
        &largeController, &QAbstractItemModel::dataChanged,
        [&modelSignals](const QModelIndex &topLeft, const QModelIndex &bottomRight,
                        const QList<int> &roles) {
            modelSignals.dataChangedRows.append({topLeft.row(), bottomRight.row()});
            modelSignals.dataChangedRoles.append(roles);
        });

    QElapsedTimer timer;
    timer.start();
    DatasetLabelControllerTestAccess::applySnapshot(
        largeController, largeFixture(largeSource, largeRecordCount), true);
    const qint64 largeOpenMilliseconds = timer.elapsed();
    if (!check(largeController.rowCount() == largeRecordCount,
               QStringLiteral("large model row count is incorrect")) ||
        !check(modelSignals.modelResetCount == 1 && modelSignals.rowsInsertedCount == 0 &&
                   modelSignals.rowsRemovedCount == 0 &&
                   modelSignals.dataChangedRows.isEmpty(),
               QStringLiteral("large open did not publish one model reset")) ||
        !check(largeOpenMilliseconds < 2000,
               QStringLiteral("large snapshot open exceeded the 2-second CI bound")))
        return 7;

    modelSignals.clear();
    timer.restart();
    const bool filterSucceeded = largeController.setFilter(QStringLiteral("unreviewed"));
    const qint64 largeFilterMilliseconds = timer.elapsed();
    if (!check(filterSucceeded, largeController.errorMessage()) ||
        !check(largeController.rowCount() == largeRecordCount,
               QStringLiteral("large filter row count is incorrect")) ||
        !check(modelSignals.modelResetCount == 1 && modelSignals.dataChangedRows.isEmpty(),
               QStringLiteral("filter change did not use one model reset")) ||
        !check(largeFilterMilliseconds < 2000,
               QStringLiteral("large filter exceeded the 2-second CI bound")) ||
        !check(largeController.setFilter(QStringLiteral("all")),
               largeController.errorMessage()))
        return 8;

    const int selectedRole = DatasetLabelController::SelectedRole;
    const int stateRole = DatasetLabelController::StateRole;
    const QString firstRecordId = QStringLiteral("large-00000");
    const QString secondRecordId = QStringLiteral("large-00001");
    const QString lastRecordId = QStringLiteral("large-18071");

    modelSignals.clear();
    if (!check(largeController.select(lastRecordId), largeController.errorMessage()) ||
        !check(modelSignals.modelResetCount == 0 && modelSignals.rowsInsertedCount == 0 &&
                   modelSignals.rowsRemovedCount == 0 &&
                   modelSignals.dataChangedRows ==
                       QVector<QPair<int, int>>{{0, 0},
                                                {largeRecordCount - 1, largeRecordCount - 1}} &&
                   modelSignals.dataChangedRoles ==
                       QVector<QList<int>>{{selectedRole}, {selectedRole}},
               QStringLiteral("selection updated rows other than old and new")))
        return 9;

    if (!check(largeController.select(firstRecordId), largeController.errorMessage()))
        return 10;
    if (!check(largeController.select(secondRecordId, true, false),
               largeController.errorMessage()) ||
        !check(largeController.data(largeController.index(0),
                                    DatasetLabelController::SelectedRole).toBool() &&
                   largeController.data(largeController.index(1),
                                        DatasetLabelController::SelectedRole).toBool(),
               QStringLiteral("Ctrl selection did not toggle an additional Crop")) ||
        !check(largeController.select(lastRecordId, false, true),
               largeController.errorMessage()) ||
        !check(largeController.data(largeController.index(0),
                                    DatasetLabelController::SelectedRole).toBool() &&
                   largeController.data(largeController.index(largeRecordCount / 2),
                                        DatasetLabelController::SelectedRole).toBool() &&
                   largeController.data(largeController.index(largeRecordCount - 1),
                                        DatasetLabelController::SelectedRole).toBool(),
               QStringLiteral("Shift selection did not use the visible anchor range")) ||
        !check(largeController.setFilter(QStringLiteral("class0")),
               largeController.errorMessage()) ||
        !check(largeController.rowCount() == 0 &&
                   largeController.selectedRecordId().isEmpty(),
               QStringLiteral("filter change retained hidden selected Crops")) ||
        !check(largeController.setFilter(QStringLiteral("all")),
               largeController.errorMessage()) ||
        !check(largeController.select(secondRecordId, false, true),
               largeController.errorMessage()) ||
        !check(!largeController.data(largeController.index(0),
                                     DatasetLabelController::SelectedRole).toBool() &&
                   largeController.data(largeController.index(1),
                                        DatasetLabelController::SelectedRole).toBool(),
               QStringLiteral("filter change did not reset the Shift anchor")) ||
        !check(largeController.select(firstRecordId), largeController.errorMessage()))
        return 10;
    modelSignals.clear();
    if (!check(largeController.next(), largeController.errorMessage()) ||
        !check(largeController.selectedRecordId() == secondRecordId &&
                   modelSignals.modelResetCount == 0 &&
                   modelSignals.dataChangedRows ==
                       QVector<QPair<int, int>>{{0, 0}, {1, 1}} &&
                   modelSignals.dataChangedRoles ==
                       QVector<QList<int>>{{selectedRole}, {selectedRole}},
               QStringLiteral("next updated rows other than old and new")))
        return 11;

    modelSignals.clear();
    DatasetLabelSnapshot labeledSnapshot =
        DatasetLabelControllerTestAccess::snapshot(largeController);
    labeledSnapshot.records[1].state = DatasetLabelState::Class0;
    --labeledSnapshot.counts.unreviewed;
    ++labeledSnapshot.counts.classCounts[0];
    labeledSnapshot.canUndo = true;
    DatasetLabelControllerTestAccess::applySnapshot(
        largeController, std::move(labeledSnapshot), false);
    if (!check(modelSignals.modelResetCount == 0 && modelSignals.rowsInsertedCount == 0 &&
                   modelSignals.rowsRemovedCount == 0 &&
                   modelSignals.dataChangedRows == QVector<QPair<int, int>>{{1, 1}} &&
                   modelSignals.dataChangedRoles == QVector<QList<int>>{{stateRole}} &&
                   largeController.data(largeController.index(1), stateRole).toString() ==
                       QStringLiteral("class0"),
               QStringLiteral("label assignment was not one row-level state update")))
        return 12;

    modelSignals.clear();
    DatasetLabelSnapshot undoneSnapshot =
        DatasetLabelControllerTestAccess::snapshot(largeController);
    undoneSnapshot.records[1].state = DatasetLabelState::Unlabeled;
    ++undoneSnapshot.counts.unreviewed;
    --undoneSnapshot.counts.classCounts[0];
    undoneSnapshot.canUndo = false;
    DatasetLabelControllerTestAccess::applySnapshot(
        largeController, std::move(undoneSnapshot), false);
    if (!check(modelSignals.modelResetCount == 0 && modelSignals.rowsInsertedCount == 0 &&
                   modelSignals.rowsRemovedCount == 0 &&
                   modelSignals.dataChangedRows == QVector<QPair<int, int>>{{1, 1}} &&
                   modelSignals.dataChangedRoles == QVector<QList<int>>{{stateRole}} &&
                   largeController.data(largeController.index(1), stateRole).toString() ==
                       QStringLiteral("unreviewed"),
               QStringLiteral("label undo was not one row-level state update")))
        return 13;

    const QString realManifestPath =
        qEnvironmentVariable("OPENDSS_REAL_LABEL_DATASET");
    if (!realManifestPath.isEmpty()) {
        OperationCoordinator realOperations;
        ApplicationStateStore realStateStore;
        DatasetLabelController realController(realOperations, realStateStore);
        ModelSignalEvidence realSignals;
        QObject::connect(&realController, &QAbstractItemModel::modelReset,
                         [&realSignals] { ++realSignals.modelResetCount; });
        QObject::connect(
            &realController, &QAbstractItemModel::rowsInserted,
            [&realSignals](const QModelIndex &, int, int) {
                ++realSignals.rowsInsertedCount;
            });
        QObject::connect(
            &realController, &QAbstractItemModel::rowsRemoved,
            [&realSignals](const QModelIndex &, int, int) {
                ++realSignals.rowsRemovedCount;
            });
        QObject::connect(
            &realController, &QAbstractItemModel::dataChanged,
            [&realSignals](const QModelIndex &topLeft, const QModelIndex &bottomRight,
                           const QList<int> &roles) {
                realSignals.dataChangedRows.append({topLeft.row(), bottomRight.row()});
                realSignals.dataChangedRoles.append(roles);
            });

        QElapsedTimer eventTick;
        eventTick.start();
        qint64 maximumEventStallMilliseconds = 0;
        QTimer responsivenessTimer;
        responsivenessTimer.setTimerType(Qt::PreciseTimer);
        responsivenessTimer.setInterval(10);
        QObject::connect(&responsivenessTimer, &QTimer::timeout, [&] {
            maximumEventStallMilliseconds =
                std::max(maximumEventStallMilliseconds, eventTick.restart());
        });
        responsivenessTimer.start();

        timer.restart();
        if (!check(realController.open(QUrl::fromLocalFile(realManifestPath)),
                   realController.errorMessage()))
            return 14;
        const qint64 openCallMilliseconds = timer.elapsed();
        timer.restart();
        if (!check(waitForOpen(realController, 30000),
                   QStringLiteral("real Dataset load did not finish")) ||
            !check(realController.errorMessage().isEmpty(),
                   realController.errorMessage()))
            return 15;
        const qint64 readyMilliseconds = timer.elapsed();
        responsivenessTimer.stop();
        if (!check(openCallMilliseconds < 100,
                   QStringLiteral("real Dataset open blocked the GUI call")) ||
            !check(maximumEventStallMilliseconds < 100,
                   QStringLiteral("GUI event loop stalled during real Dataset validation")) ||
            !check(realController.rowCount() == 3625,
                   QStringLiteral("real Dataset row count is not 3625")))
            return 16;

        const QString firstId =
            realController.data(realController.index(0),
                                DatasetLabelController::RecordIdRole).toString();
        const QString lastId =
            realController.data(realController.index(realController.rowCount() - 1),
                                DatasetLabelController::RecordIdRole).toString();
        QVector<double> selectMilliseconds;
        selectMilliseconds.reserve(1000);
        realSignals.clear();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            QElapsedTimer selectTimer;
            selectTimer.start();
            if (!realController.select(iteration % 2 == 0 ? lastId : firstId))
                return 17;
            selectMilliseconds.append(selectTimer.nsecsElapsed() / 1000000.0);
        }
        std::sort(selectMilliseconds.begin(), selectMilliseconds.end());
        const double selectP95 = selectMilliseconds.at(949);
        const double selectMaximum = selectMilliseconds.last();
        if (!check(selectP95 <= 5.0 && selectMaximum <= 20.0,
                   QStringLiteral("real Dataset selection latency exceeded the bound")) ||
            !check(realSignals.modelResetCount == 0 &&
                       realSignals.rowsInsertedCount == 0 &&
                       realSignals.rowsRemovedCount == 0 &&
                       realSignals.dataChangedRows.size() == 2000,
                   QStringLiteral("real Dataset selection reset or updated other rows"))) {
            return 18;
        }
        for (const QList<int> &roles : std::as_const(realSignals.dataChangedRoles)) {
            if (!check(roles == QList<int>{DatasetLabelController::SelectedRole},
                       QStringLiteral("real Dataset selection changed a non-selected role")))
                return 19;
        }

        timer.restart();
        if (!check(realController.setFilter(QStringLiteral("unreviewed")),
                   realController.errorMessage()))
            return 20;
        const qint64 filterMilliseconds = timer.elapsed();
        if (!check(filterMilliseconds < 100,
                   QStringLiteral("real Dataset filter exceeded 100 ms")) ||
            !check(realController.setFilter(QStringLiteral("all")),
                   realController.errorMessage()))
            return 21;

        QTemporaryDir mutationTemporary;
        const QString copyFolder =
            QDir(mutationTemporary.path()).filePath(QStringLiteral("dataset-copy"));
        if (!check(realController.saveAs(QUrl::fromLocalFile(copyFolder)),
                   realController.errorMessage()))
            return 22;
        const QString mutationRecordId =
            realController.data(realController.index(0),
                                DatasetLabelController::RecordIdRole).toString();
        if (!check(realController.select(mutationRecordId),
                   realController.errorMessage()))
            return 23;
        const QString currentState =
            realController.data(realController.index(realController.selectedIndex()),
                                DatasetLabelController::StateRole).toString();
        const QString replacementClass =
            currentState == QStringLiteral("class0") ? QStringLiteral("1")
                                                     : QStringLiteral("0");
        realSignals.clear();
        timer.restart();
        if (!check(realController.assignClass(replacementClass),
                   realController.errorMessage()))
            return 24;
        const qint64 assignMilliseconds = timer.elapsed();
        if (!check(assignMilliseconds <= 500,
                   QStringLiteral("real Dataset assign/persist/refresh exceeded 500 ms")) ||
            !check(realSignals.modelResetCount == 0 &&
                       realSignals.rowsInsertedCount == 0 &&
                       realSignals.rowsRemovedCount == 0 &&
                       realSignals.dataChangedRows.size() == 1 &&
                       realSignals.dataChangedRoles ==
                           QVector<QList<int>>{{DatasetLabelController::StateRole}},
                   QStringLiteral("real Dataset assignment was not one StateRole update")))
            return 25;

        std::cout << " real_records=" << realController.rowCount()
                  << " open_call_ms=" << openCallMilliseconds
                  << " ready_ms=" << readyMilliseconds
                  << " gui_max_stall_ms=" << maximumEventStallMilliseconds
                  << " select_p95_ms=" << selectP95
                  << " select_max_ms=" << selectMaximum
                  << " filter_ms=" << filterMilliseconds
                  << " assign_persist_refresh_ms=" << assignMilliseconds;
    }

    std::cout << "large_records=" << largeRecordCount
              << " snapshot_open_ms=" << largeOpenMilliseconds
              << " filter_ms=" << largeFilterMilliseconds
              << " select_data_changed=2 next_data_changed=2 label_data_changed=1"
                 " undo_data_changed=1\n";
    return 0;
}
