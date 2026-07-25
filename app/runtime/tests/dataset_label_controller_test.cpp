#include "../v2/dataset/dataset_label_controller.h"
#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMetaProperty>
#include <QTemporaryDir>
#include <QUrl>

#include <iostream>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;

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
        "class2Count", "classNames", "class2Enabled", "canUndo", "records", "filteredRecords",
        "selectedRecordId", "selectedCropUrl", "selectedIndex", "filter", "errorMessage",
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
        !check(!controller.open(QUrl::fromLocalFile(
                   QDir(temporary.path()).filePath(QStringLiteral("missing.json")))),
               QStringLiteral("missing Dataset URL was accepted")) ||
        !check(!controller.errorMessage().isEmpty(), QStringLiteral("invalid path error missing")) ||
        !check(controller.presentation() == QStringLiteral("empty") && changedCount == 2,
               QStringLiteral("missing Dataset changed the presentation or signal count")) ||
        !check(controller.open(QUrl::fromLocalFile(manifestPath)), controller.errorMessage()) ||
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
        !check(changedCount == 3, QStringLiteral("successful open missed changed signal")))
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
        !check(controller.filteredRecords().size() == 1 &&
                   controller.selectedRecordId() == QStringLiteral("r2") &&
                   controller.class2Count() == 1,
               QStringLiteral("filter projection is incorrect")) ||
        !check(!controller.configureClassCount(2),
               QStringLiteral("three-to-two change with Class 2 label succeeded")) ||
        !check(controller.classCount() == 3 && controller.filter() == QStringLiteral("class2") &&
                   controller.presentation() == QStringLiteral("ready"),
               QStringLiteral("rejected schema change corrupted projection")) ||
        !check(controller.exclude(), controller.errorMessage()) ||
        !check(controller.filteredRecords().isEmpty() && controller.selectedRecordId().isEmpty(),
               QStringLiteral("filter selection did not normalize after exclusion")) ||
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
        !check(controller.assignClass(QStringLiteral("0")), controller.errorMessage()) ||
        !check(controller.class0Count() == 1 && controller.canUndo(),
               QStringLiteral("Class 0 projection is incorrect")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.class0Count() == 0 && !controller.canUndo(),
               QStringLiteral("label undo projection is incorrect")) ||
        !check(controller.exclude(), controller.errorMessage()) ||
        !check(controller.excludedCount() == 2 && controller.canUndo(),
               QStringLiteral("exclude projection is incorrect")) ||
        !check(controller.undo(), controller.errorMessage()) ||
        !check(controller.excludedCount() == 1 && !controller.canUndo(),
               QStringLiteral("exclude undo projection is incorrect")) ||
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
    return 0;
}
