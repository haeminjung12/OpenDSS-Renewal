#include "../v2/model/model_library_controller.h"
#include "../v2/operation/operation_coordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using desktop_app::v2::ModelLibraryController;
using desktop_app::v2::OperationCoordinator;
using desktop_app::v2::OperationKind;
using desktop_app::v2::ResourceLock;

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool copyFile(const QString &source, const QString &destination)
{
    QFile::remove(destination);
    return QFile::copy(source, destination);
}

QByteArray bytes(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read registry bytes");
    return file.readAll();
}

QString createPackage(const QString &root, const QString &architecture)
{
    const QDir source(QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR))
                          .filePath(QStringLiteral("models/templates/pretrained/%1")
                                        .arg(architecture)));
    const QString package = QDir(root).filePath(architecture);
    require(QDir().mkpath(package), "create package");
    require(copyFile(source.filePath(QStringLiteral("metadata.json")),
                     QDir(package).filePath(QStringLiteral("metadata.json"))),
            "copy metadata");
    require(copyFile(source.filePath(QStringLiteral("model.onnx")),
                     QDir(package).filePath(QStringLiteral("model.onnx"))),
            "copy model");
    return package;
}

QJsonObject entry(const QString &id, const QString &name, const QString &package,
                  bool active)
{
    return {{QStringLiteral("registry_entry_id"), id},
            {QStringLiteral("display_name"), name},
            {QStringLiteral("package_path"), package},
            {QStringLiteral("active"), active}};
}

QString createRegistry(const QString &root)
{
    const QString mobileNetPackage =
        createPackage(root, QStringLiteral("mobilenet_v3_small"));
    const QString efficientNetPackage =
        createPackage(root, QStringLiteral("efficientnet_b0"));
    const QString unknownPackage = QDir(root).filePath(QStringLiteral("unknown_architecture"));
    require(QDir().mkpath(unknownPackage), "create unknown package");
    require(copyFile(QDir(mobileNetPackage).filePath(QStringLiteral("model.onnx")),
                     QDir(unknownPackage).filePath(QStringLiteral("model.onnx"))),
            "copy unknown model");
    QFile sourceMetadata(QDir(mobileNetPackage).filePath(QStringLiteral("metadata.json")));
    require(sourceMetadata.open(QIODevice::ReadOnly), "open source metadata");
    QJsonObject unknownMetadata =
        QJsonDocument::fromJson(sourceMetadata.readAll()).object();
    sourceMetadata.close();
    QJsonObject unknownArchitecture =
        unknownMetadata.value(QStringLiteral("architecture")).toObject();
    unknownArchitecture.insert(QStringLiteral("id"),
                               QStringLiteral("unknown_architecture"));
    unknownMetadata.insert(QStringLiteral("architecture"), unknownArchitecture);
    QFile unknownMetadataFile(
        QDir(unknownPackage).filePath(QStringLiteral("metadata.json")));
    require(unknownMetadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open unknown metadata");
    require(unknownMetadataFile.write(QJsonDocument(unknownMetadata).toJson())
                > 0,
            "write unknown metadata");
    const QString registry = QDir(root).filePath(QStringLiteral("model_registry.json"));
    QFile file(registry);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open registry");
    const QJsonObject document{
        {QStringLiteral("schema_version"), QStringLiteral("model-registry-v3-simple")},
        {QStringLiteral("entries"),
         QJsonArray{
             entry(QStringLiteral("opendss_pretrained_mobilenet_v3_small"),
                   QStringLiteral("Pre-trained MobileNetV3-Small — Faster"),
                   mobileNetPackage, true),
             entry(QStringLiteral("opendss_pretrained_efficientnet_b0"),
                   QStringLiteral("Pre-trained EfficientNet-B0 — More Accurate"),
                   efficientNetPackage, false),
             entry(QStringLiteral("trained-local-model"),
                   QStringLiteral("Trained Local Model"), mobileNetPackage, false),
             entry(QStringLiteral("trained-unknown-model"),
                   QStringLiteral("Trained Unknown — Custom"), unknownPackage,
                   false)}}};
    require(file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) > 0,
            "write registry");
    return registry;
}

QVariantMap rowById(const QVariantList &rows, const QString &id)
{
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("id")).toString() == id)
            return row;
    }
    return {};
}

void testRefreshSelectionActivationAndRename()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory");
    const QString registryPath = createRegistry(temporary.path());
    OperationCoordinator operations;
    ModelLibraryController controller(registryPath, operations);

    require(controller.refresh(), qPrintable(controller.errorMessage()));
    require(controller.presentation() == QStringLiteral("ready"), "ready presentation");
    require(controller.modelRows().size() == 4, "four factual rows");
    const QVariantMap trainedRow =
        rowById(controller.modelRows(), QStringLiteral("trained-local-model"));
    require(trainedRow.value(QStringLiteral("userFacingLabel")).toString().isEmpty()
                && trainedRow.value(QStringLiteral("architecture")).toString()
                    == QStringLiteral("mobilenet_v3_small")
                && trainedRow.value(QStringLiteral("performanceLabel")).toString()
                    == QStringLiteral("Faster"),
            "trained row performance follows inspected supported architecture");
    const QVariantMap unknownRow =
        rowById(controller.modelRows(), QStringLiteral("trained-unknown-model"));
    require(unknownRow.value(QStringLiteral("architecture")).toString()
                    == QStringLiteral("unknown_architecture")
                && unknownRow.value(QStringLiteral("performanceLabel")).toString().isEmpty(),
            "unknown architecture has no inferred performance label");
    const QVariantMap row =
        rowById(controller.modelRows(),
                QStringLiteral("opendss_pretrained_efficientnet_b0"));
    require(row.value(QStringLiteral("name")).toString()
                == QStringLiteral("Pre-trained EfficientNet-B0 — More Accurate"),
            "row name remains factual");
    require(row.value(QStringLiteral("architecture")).toString()
                == QStringLiteral("efficientnet_b0"),
            "row architecture remains factual");
    require(row.value(QStringLiteral("userFacingLabel")).toString()
                == QStringLiteral("EfficientNet-B0 — More Accurate"),
            "row user-facing label remains factual");
    require(row.value(QStringLiteral("performanceLabel")).toString()
                == QStringLiteral("More Accurate"),
            "row performance label remains factual");
    require(row.value(QStringLiteral("classSummary")).toString()
                == QStringLiteral("Empty, Single, MoreThanOne"),
            "row class summary remains factual");
    require(!row.value(QStringLiteral("active")).toBool(),
            "row active flag remains factual");

    require(controller.select(1), qPrintable(controller.errorMessage()));
    require(controller.selectedIndex() == 1
                && controller.selectedId()
                    == QStringLiteral("opendss_pretrained_efficientnet_b0"),
            "selection projection");
    const QVariantMap detail = controller.selectedDetail();
    require(detail.value(QStringLiteral("userFacingLabel")).toString()
                    == QStringLiteral("EfficientNet-B0 — More Accurate")
                && detail.value(QStringLiteral("performanceLabel")).toString()
                    == QStringLiteral("More Accurate")
                && detail.value(QStringLiteral("classSummary")).toString()
                    == QStringLiteral("Empty, Single, MoreThanOne"),
            "selected factual detail");

    const QByteArray registryBeforeLock = bytes(registryPath);
    auto modelTest =
        operations.acquire(OperationKind::ModelTest, ResourceLock::Model);
    require(modelTest.acquired(), "hold Model Test Model lock");
    require(!controller.renameSelected(QStringLiteral("Blocked Rename")) &&
                controller.errorMessage().contains(
                    QStringLiteral("Model Test")) &&
                bytes(registryPath) == registryBeforeLock,
            "Model Test lock rejects rename without registry change");
    require(!controller.setActive() &&
                controller.errorMessage().contains(
                    QStringLiteral("Model Test")) &&
                bytes(registryPath) == registryBeforeLock,
            "Model Test lock rejects activation without registry change");
    modelTest.lease.release();

    require(controller.renameSelected(QStringLiteral("Renamed Model")),
            qPrintable(controller.errorMessage()));
    require(controller.selectedId()
                    == QStringLiteral("opendss_pretrained_efficientnet_b0")
                && controller.selectedDetail().value(QStringLiteral("name")).toString()
                    == QStringLiteral("Renamed Model"),
            "rename preserves selection");

    require(controller.setActive(), qPrintable(controller.errorMessage()));
    require(controller.activeId()
                    == QStringLiteral("opendss_pretrained_efficientnet_b0")
                && controller.selectedDetail().value(QStringLiteral("active")).toBool(),
            "activation refreshes authoritative state");
    require(!controller.setActive()
                && controller.errorMessage()
                    == QStringLiteral("Selected model is already Active."),
            "already-active reason");
}

void testErrorsAndEmptyPresentation()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary error directory");
    OperationCoordinator operations;
    ModelLibraryController missing(
        QDir(temporary.path()).filePath(QStringLiteral("missing-registry.json")),
        operations);
    require(!missing.refresh() && missing.presentation() == QStringLiteral("error")
                && !missing.errorMessage().isEmpty(),
            "missing registry error");

    const QString emptyRegistry =
        QDir(temporary.path()).filePath(QStringLiteral("empty-registry.json"));
    QFile file(emptyRegistry);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open empty registry");
    require(file.write("{\"schema_version\":\"model-registry-v3-simple\",\"entries\":[]}") > 0,
            "write empty registry");
    file.close();

    ModelLibraryController empty(emptyRegistry, operations);
    require(empty.refresh() && empty.presentation() == QStringLiteral("empty"),
            "empty presentation");
    require(!empty.select(0)
                && empty.errorMessage() == QStringLiteral("Selected model is unavailable."),
            "invalid selection reason");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    testRefreshSelectionActivationAndRename();
    testErrorsAndEmptyPresentation();
    return 0;
}
