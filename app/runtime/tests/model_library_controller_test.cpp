#include "../v2/model/model_library_controller.h"
#include "../v2/operation/operation_coordinator.h"
#include "../desktop_app/model_registry_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using desktop_app::v2::ModelLibraryController;
using desktop_app::v2::ModelAccess;
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
    require(copyFile(source.filePath(QStringLiteral("checkpoint.pth")),
                     QDir(package).filePath(QStringLiteral("checkpoint.pth"))),
            "copy checkpoint");
    return package;
}

QString packageEntryId(const QString &package)
{
    QFile metadataFile(QDir(package).filePath(QStringLiteral("metadata.json")));
    require(metadataFile.open(QIODevice::ReadOnly), "read package metadata ID");
    QString id = QJsonDocument::fromJson(metadataFile.readAll())
                     .object()
                     .value(QStringLiteral("model_id"))
                     .toString()
                     .trimmed()
                     .toLower();
    return QStringLiteral("trained_") + id;
}

#ifdef Q_OS_WIN
bool createJunction(const QString &junctionPath, const QString &targetPath)
{
    return QProcess::execute(
               QStringLiteral("cmd.exe"),
               {QStringLiteral("/d"), QStringLiteral("/c"), QStringLiteral("mklink"),
                QStringLiteral("/J"), QDir::toNativeSeparators(junctionPath),
                QDir::toNativeSeparators(targetPath)}) == 0;
}
#endif

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
    const QString trainedLocalPackage =
        createPackage(QDir(root).filePath(QStringLiteral("trained-local")),
                      QStringLiteral("mobilenet_v3_small"));
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
                   QStringLiteral("Trained Local Model"), trainedLocalPackage, false),
             entry(QStringLiteral("trained-unknown-model"),
                   QStringLiteral("Trained Unknown — Custom"), unknownPackage,
                   false)}}};
    require(file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) > 0,
            "write registry");
    return registry;
}

QString createSimpleRegistry(const QString &modelsRoot, const QJsonArray &entries = {})
{
    require(QDir().mkpath(modelsRoot), "create models root");
    const QString registry =
        QDir(modelsRoot).filePath(QStringLiteral("model_registry.json"));
    QFile file(registry);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open simple registry");
    const QJsonObject document{
        {QStringLiteral("schema_version"), QStringLiteral("model-registry-v3-simple")},
        {QStringLiteral("entries"), entries}};
    require(file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) > 0,
            "write simple registry");
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
    auto modelTest = operations.acquireWithModel(
        OperationKind::ModelTest, {},
        detail.value(QStringLiteral("packageLocation")).toString(),
        ModelAccess::Read);
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

    const QString missingPackage =
        QDir(temporary.path()).filePath(QStringLiteral("missing-metadata"));
    const QString malformedPackage =
        QDir(temporary.path()).filePath(QStringLiteral("malformed-metadata"));
    require(QDir().mkpath(missingPackage) && QDir().mkpath(malformedPackage),
            "create invalid metadata packages");
    QFile malformedFile(
        QDir(malformedPackage).filePath(QStringLiteral("metadata.json")));
    require(malformedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && malformedFile.write("{not-json") > 0,
            "write malformed metadata");
    malformedFile.close();
    const QString invalidRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("invalid-models")),
        QJsonArray{
            entry(QStringLiteral("missing-metadata"), QStringLiteral("Missing"),
                  missingPackage, false),
            entry(QStringLiteral("malformed-metadata"), QStringLiteral("Malformed"),
                  malformedPackage, false),
        });
    ModelLibraryController invalid(invalidRegistry, operations);
    require(invalid.refresh(), qPrintable(invalid.errorMessage()));
    const QVariantList invalidTrainingRows = invalid.trainingModelRows();
    const QVariantList invalidLibraryRows = invalid.modelRows();
    for (const QString &id : {QStringLiteral("missing-metadata"),
                              QStringLiteral("malformed-metadata")}) {
        const QVariantMap trainingRow = rowById(invalidTrainingRows, id);
        require(trainingRow.value(QStringLiteral("weightPath")).toString().isEmpty()
                    && !trainingRow.contains(QStringLiteral("compatibilityReason"))
                    && !rowById(invalidLibraryRows, id)
                            .value(QStringLiteral("message")).toString().isEmpty(),
                "Training omits compatibility presentation while preserving package facts");
    }
}

void testPackagePathSafetyAndRegistryIntegrity()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "path safety temporary directory");
    const QString sourcePackage = createPackage(
        QDir(temporary.path()).filePath(QStringLiteral("source")),
        QStringLiteral("mobilenet_v3_small"));
    QString exportedPath;
    QString error;

    require(!exportCompleteModelPackage(
                sourcePackage, QFileInfo(sourcePackage).absolutePath(),
                &exportedPath, &error)
                && error.contains(QStringLiteral("overlap")),
            "self export path rejected before staging");

    const QString descendantRoot =
        QDir(sourcePackage).filePath(QStringLiteral("nested-destination"));
    error.clear();
    require(!exportCompleteModelPackage(sourcePackage, descendantRoot,
                                        &exportedPath, &error)
                && error.contains(QStringLiteral("overlap"))
                && !QFileInfo::exists(descendantRoot),
            "descendant export path rejected before destination creation");

    const QString reverseRoot =
        QDir(temporary.path()).filePath(QStringLiteral("reverse"));
    const QString reverseSource = createPackage(
        QDir(reverseRoot)
            .filePath(QStringLiteral("mobilenet_v3_small/subdirectory")),
        QStringLiteral("mobilenet_v3_small"));
    error.clear();
    require(!exportCompleteModelPackage(reverseSource, reverseRoot,
                                        &exportedPath, &error)
                && error.contains(QStringLiteral("overlap")),
            "destination ancestor of source rejected before staging");

#ifdef Q_OS_WIN
    const QString sourceAlias =
        QDir(temporary.path()).filePath(QStringLiteral("source-alias"));
    require(createJunction(sourceAlias, sourcePackage),
            "create source package junction");
    error.clear();
    require(!exportCompleteModelPackage(
                sourceAlias, QFileInfo(sourceAlias).absolutePath(),
                &exportedPath, &error)
                && error.contains(QStringLiteral("overlap")),
            "canonical source alias overlap rejected");
    require(QDir().rmdir(sourceAlias), "remove source package junction");

    const QString destinationAlias =
        QDir(temporary.path()).filePath(QStringLiteral("destination-alias"));
    require(createJunction(destinationAlias, sourcePackage),
            "create destination junction");
    error.clear();
    require(!exportCompleteModelPackage(sourcePackage, destinationAlias,
                                        &exportedPath, &error)
                && error.contains(QStringLiteral("junction")),
            "destination reparse root rejected before staging");
    require(QDir().rmdir(destinationAlias), "remove destination junction");

    const QString publicationTarget =
        QDir(temporary.path()).filePath(QStringLiteral("publication-target"));
    const QString publicationAlias =
        QDir(temporary.path()).filePath(QStringLiteral("publication-alias"));
    require(QDir().mkpath(publicationTarget)
                && createJunction(publicationAlias, publicationTarget),
            "create publication root junction");
    const QString publicationRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("publication-registry")));
    QString savedPackage;
    QString registeredEntry;
    error.clear();
    require(!saveTrainedModelArtifacts(
                publicationRegistry, sourcePackage,
                QDir(sourcePackage).filePath(QStringLiteral("model.onnx")),
                QDir(sourcePackage).filePath(QStringLiteral("metadata.json")),
                {}, {}, {}, {}, {}, QStringLiteral("Blocked Publication"),
                publicationAlias, &savedPackage, &registeredEntry, &error)
                && error.contains(QStringLiteral("junction"))
                && QDir(publicationTarget).isEmpty(),
            "publication reparse root rejected before package mutation");
    require(QDir().rmdir(publicationAlias), "remove publication root junction");

    const QString identityModels =
        QDir(temporary.path()).filePath(QStringLiteral("identity-models"));
    qputenv("OVDS_MODELS_ROOT_PATH",
            QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR))
                .filePath(QStringLiteral("models")).toUtf8());
    const QString identityRegistry = createSimpleRegistry(identityModels);
    const QString identityTarget =
        QDir(temporary.path()).filePath(QStringLiteral("identity-target"));
    require(QDir().mkpath(identityTarget)
                && createJunction(QDir(identityModels).filePath(
                                      QStringLiteral(".opendss-model-identities")),
                                  identityTarget),
            "create identity root junction");
    OperationCoordinator identityOperations;
    ModelLibraryController identityController(identityRegistry, identityOperations);
    require(!identityController.addModel(QStringLiteral("Blocked Identity"), 0, 0,
                                         QUrl{})
                && identityController.errorMessage().contains(QStringLiteral("junction"))
                && QDir(identityTarget).isEmpty(),
            "identity reparse root rejected before mutation");
    require(QDir().rmdir(QDir(identityModels).filePath(
                QStringLiteral(".opendss-model-identities"))),
            "remove identity root junction");

    const QString recoveryModels =
        QDir(temporary.path()).filePath(QStringLiteral("recovery-models"));
    const QString recoveryPackage =
        createPackage(recoveryModels, QStringLiteral("mobilenet_v3_small"));
    const QString recoveryRegistry = createSimpleRegistry(
        recoveryModels,
        QJsonArray{entry(QStringLiteral("recovery-model"),
                         QStringLiteral("Recovery Model"), recoveryPackage, false)});
    const QString recoveryTarget =
        QDir(temporary.path()).filePath(QStringLiteral("recovery-target"));
    require(QDir().mkpath(recoveryTarget)
                && createJunction(QDir(recoveryModels).filePath(
                                      QStringLiteral(".opendss-model-recovery")),
                                  recoveryTarget),
            "create recovery root junction");
    bool recoveryRegistryCommitted = false;
    QString reparseRecoveryPath;
    error.clear();
    require(!deleteRegisteredModelPackage(
                recoveryRegistry, QStringLiteral("recovery-model"),
                &recoveryRegistryCommitted, nullptr, &reparseRecoveryPath, &error)
                && error.contains(QStringLiteral("junction"))
                && !recoveryRegistryCommitted
                && QFileInfo(recoveryPackage).isDir()
                && QDir(recoveryTarget).isEmpty(),
            "deletion recovery reparse root rejected before mutation");
    require(QDir().rmdir(QDir(recoveryModels).filePath(
                QStringLiteral(".opendss-model-recovery"))),
            "remove recovery root junction");
#endif

    const QString unrelatedDirectory =
        QDir(temporary.path()).filePath(QStringLiteral("unrelated-directory"));
    require(QDir().mkpath(unrelatedDirectory), "create unrelated directory");
    QFile unrelatedSentinel(
        QDir(unrelatedDirectory).filePath(QStringLiteral("keep.txt")));
    require(unrelatedSentinel.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open unrelated sentinel");
    require(unrelatedSentinel.write("keep") == 4, "write unrelated sentinel");
    unrelatedSentinel.close();
    const QString damagedRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("damaged-registry")),
        QJsonArray{entry(QStringLiteral("wrong-model-id"),
                         QStringLiteral("Wrong Model"), unrelatedDirectory, false)});
    OperationCoordinator operations;
    ModelLibraryController damagedController(damagedRegistry, operations);
    require(damagedController.refresh() && damagedController.select(0),
            "select damaged registry entry");
    const QByteArray damagedRegistryBefore = bytes(damagedRegistry);
    require(!damagedController.deleteSelected()
                && !damagedController.errorMessage().isEmpty()
                && QFileInfo(unrelatedDirectory).isDir()
                && QFileInfo(unrelatedSentinel.fileName()).isFile()
                && bytes(damagedRegistry) == damagedRegistryBefore,
            "damaged registry path cannot delete an unrelated directory");

    const QString shippedPackage = createPackage(
        QDir(temporary.path()).filePath(QStringLiteral("shipped-pretrained")),
        QStringLiteral("mobilenet_v3_small"));
    const QString shippedEntryId =
        QStringLiteral("opendss_pretrained_mobilenet_v3_small");
    require(shippedEntryId != packageEntryId(shippedPackage),
            "shipped registry key remains independent from metadata Model ID");
    const QString shippedRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("shipped-registry")),
        QJsonArray{entry(
            shippedEntryId,
            QStringLiteral("Pre-trained MobileNetV3-Small — Faster"),
            shippedPackage, false)});
    ModelLibraryController shippedController(shippedRegistry, operations);
    require(shippedController.refresh() && shippedController.select(0),
            "select shipped pretrained registry entry");
    const bool shippedDeleted = shippedController.deleteSelected();
    require(shippedDeleted && shippedController.modelRows().isEmpty()
                && !QFileInfo::exists(shippedPackage),
            qPrintable(QStringLiteral("shipped pretrained delete: ")
                       + shippedController.errorMessage()));

#ifdef Q_OS_WIN
    const QString deleteAlias =
        QDir(temporary.path()).filePath(QStringLiteral("delete-alias"));
    require(createJunction(deleteAlias, sourcePackage),
            "create delete package junction");
    const QString aliasRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("alias-registry")),
        QJsonArray{entry(packageEntryId(sourcePackage),
                         QStringLiteral("Alias Model"), deleteAlias, false)});
    ModelLibraryController aliasController(aliasRegistry, operations);
    require(aliasController.refresh() && aliasController.select(0),
            "select alias registry entry");
    require(!aliasController.deleteSelected()
                && aliasController.errorMessage().contains(
                    QStringLiteral("alias or reparse"))
                && QFileInfo(sourcePackage).isDir(),
            "delete rejects a junction package root");
    require(QDir().rmdir(deleteAlias), "remove delete package junction");
#endif

    QString duplicatePackagePath = sourcePackage;
#ifdef Q_OS_WIN
    duplicatePackagePath =
        QDir(temporary.path()).filePath(QStringLiteral("duplicate-path-alias"));
    require(createJunction(duplicatePackagePath, sourcePackage),
            "create duplicate registry path junction");
#endif
    const QString duplicateRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("duplicate-registry")),
        QJsonArray{
            entry(packageEntryId(sourcePackage), QStringLiteral("First"),
                  sourcePackage, false),
            entry(QStringLiteral("another-model-id"), QStringLiteral("Second"),
                  duplicatePackagePath, false)});
    ModelLibraryController duplicateController(duplicateRegistry, operations);
    require(duplicateController.refresh() && duplicateController.select(0),
            "select duplicate-path registry entry");
    const QByteArray duplicateRegistryBefore = bytes(duplicateRegistry);
    require(!duplicateController.deleteSelected()
                && duplicateController.errorMessage().contains(
                    QStringLiteral("another Model ID"))
                && QFileInfo(sourcePackage).isDir()
                && bytes(duplicateRegistry) == duplicateRegistryBefore,
            "delete rejects a package path registered to another ID");

    const QString importSource = createPackage(
        QDir(temporary.path()).filePath(QStringLiteral("import-source")),
        QStringLiteral("efficientnet_b0"));
    QString importedId;
    QString importedPackage;
    QString recoveryPath;
    error.clear();
    require(!importCompleteModelPackage(
                duplicateRegistry, importSource, &importedId, &importedPackage,
                &recoveryPath, &error)
                && error.contains(QStringLiteral("duplicate"))
                && bytes(duplicateRegistry) == duplicateRegistryBefore,
            "package mutation rejects an already duplicated canonical registry path");
#ifdef Q_OS_WIN
    require(QDir().rmdir(duplicatePackagePath),
            "remove duplicate registry path junction");
#endif
}

void testPackageOperations()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "package operations temporary directory");
    const QString sourceRoot = QDir(temporary.path()).filePath(QStringLiteral("source"));
    const QString sourcePackage =
        createPackage(sourceRoot, QStringLiteral("mobilenet_v3_small"));
    QString sourceValidationError;
    require(validateCompleteV2ModelPackage(sourcePackage, &sourceValidationError),
            qPrintable(QStringLiteral("source fixture validation: ") +
                       sourceValidationError));
    const QByteArray sourceMetadataBefore =
        bytes(QDir(sourcePackage).filePath(QStringLiteral("metadata.json")));
    const QString modelsRoot = QDir(temporary.path()).filePath(QStringLiteral("models"));
    const QString registryPath = createSimpleRegistry(modelsRoot);

    OperationCoordinator operations;
    ModelLibraryController controller(registryPath, operations);
    int activeClearCount = 0;
    controller.setActiveModelClearedCallback([&activeClearCount]() {
        ++activeClearCount;
    });
    require(controller.refresh(), qPrintable(controller.errorMessage()));

    const QString invalidRoot =
        QDir(temporary.path()).filePath(QStringLiteral("invalid"));
    const QString invalidPackage =
        createPackage(invalidRoot, QStringLiteral("efficientnet_b0"));
    require(QFile::remove(
                QDir(invalidPackage).filePath(QStringLiteral("checkpoint.pth"))),
            "remove invalid checkpoint");
    const QByteArray registryBeforeInvalid = bytes(registryPath);
    require(!controller.importModel(QUrl::fromLocalFile(
                QDir(invalidPackage).filePath(QStringLiteral("metadata.json"))))
                && controller.errorMessage().contains(QStringLiteral("checkpoint"))
                && bytes(registryPath) == registryBeforeInvalid,
            "invalid incomplete import rejected without registry mutation");

    const bool imported = controller.importModel(QUrl::fromLocalFile(
        QDir(sourcePackage).filePath(QStringLiteral("metadata.json"))));
    QString sourceAfterImportError;
    const bool sourceStillComplete =
        validateCompleteV2ModelPackage(sourcePackage, &sourceAfterImportError);
    require(imported,
            qPrintable(QStringLiteral("import success: %1; source after operation: %2 (%3)")
                           .arg(controller.errorMessage())
                           .arg(sourceStillComplete)
                           .arg(sourceAfterImportError)));
    require(controller.modelRows().size() == 1 && controller.selectedIndex() == 0,
            "import refreshes and selects only after success");
    const QString importedId = controller.selectedId();
    const QString importedPackage =
        controller.selectedDetail().value(QStringLiteral("packageLocation")).toString();
    require(QFileInfo(importedPackage).isDir()
                && QFileInfo(QDir(importedPackage).filePath(QStringLiteral("metadata.json"))).isFile()
                && QFileInfo(QDir(importedPackage).filePath(QStringLiteral("model.onnx"))).isFile()
                && QFileInfo(QDir(importedPackage).filePath(QStringLiteral("checkpoint.pth"))).isFile(),
            "import copies and registers complete package");
    const QByteArray registryAfterImport = bytes(registryPath);
    require(!controller.importModel(QUrl::fromLocalFile(
                QDir(sourcePackage).filePath(QStringLiteral("metadata.json"))))
                && bytes(registryPath) == registryAfterImport,
            "import collision rejected without registry mutation");
    require(bytes(QDir(sourcePackage).filePath(QStringLiteral("metadata.json")))
                == sourceMetadataBefore,
            "import does not mutate source package");

    const QString exportRoot =
        QDir(temporary.path()).filePath(QStringLiteral("exports"));
    require(controller.exportSelected(QUrl::fromLocalFile(exportRoot)),
            qPrintable(QStringLiteral("export success: ") +
                       controller.errorMessage()));
    const QString exportedPackage =
        QDir(exportRoot).filePath(QFileInfo(importedPackage).fileName());
    require(QFileInfo(QDir(exportedPackage).filePath(QStringLiteral("metadata.json"))).isFile()
                && bytes(registryPath) == registryAfterImport
                && bytes(QDir(importedPackage).filePath(QStringLiteral("metadata.json")))
                    == sourceMetadataBefore,
            "export copies complete package without source or registry mutation");
    require(!controller.exportSelected(QUrl::fromLocalFile(exportRoot))
                && bytes(registryPath) == registryAfterImport,
            "export collision rejected");

    const QString duplicateRoot =
        QDir(temporary.path()).filePath(QStringLiteral("duplicates"));
    require(controller.duplicateSelected(
            QStringLiteral("Independent Copy"),
            QUrl::fromLocalFile(duplicateRoot)),
            qPrintable(QStringLiteral("duplicate success: ") +
                       controller.errorMessage()));
    require(controller.modelRows().size() == 2
                && controller.selectedId() != importedId
                && controller.selectedDetail().value(QStringLiteral("name")).toString()
                    == QStringLiteral("Independent Copy"),
            "duplicate refreshes and selects independent package");
    const QString duplicatedPackage =
        controller.selectedDetail().value(QStringLiteral("packageLocation")).toString();
    QFile duplicateMetadataFile(
        QDir(duplicatedPackage).filePath(QStringLiteral("metadata.json")));
    require(duplicateMetadataFile.open(QIODevice::ReadOnly), "read duplicate metadata");
    const QJsonObject duplicateMetadata =
        QJsonDocument::fromJson(duplicateMetadataFile.readAll()).object();
    duplicateMetadataFile.close();
    require(duplicateMetadata.value(QStringLiteral("model_name")).toString()
                    == QStringLiteral("Independent Copy")
                && duplicateMetadata.value(QStringLiteral("model_id")).toString()
                    != QStringLiteral("opendss-pretrained-mobilenet_v3_small-3class")
                && bytes(QDir(importedPackage).filePath(QStringLiteral("metadata.json")))
                    == sourceMetadataBefore,
            "duplicate has new identity without source mutation");

    const QByteArray registryBeforeLock = bytes(registryPath);
    auto heldRead = operations.acquireModel(duplicatedPackage, ModelAccess::Read);
    require(heldRead.acquired(), "hold package read lock");
    require(!controller.deleteSelected()
                && controller.errorMessage().contains(QStringLiteral("in use"))
                && bytes(registryPath) == registryBeforeLock
                && QFileInfo(duplicatedPackage).isDir(),
            "per-package lock rejects delete without mutation");
    heldRead.lease.release();
    auto writeAfterRelease =
        operations.acquireModel(duplicatedPackage, ModelAccess::Write);
    require(writeAfterRelease.acquired(),
            "package write lock is available after held read release");
    writeAfterRelease.lease.release();
    require(controller.canDelete(),
            "controller projects delete available after package lock release");

    const bool duplicateDeleted = controller.deleteSelected();
    require(duplicateDeleted,
            qPrintable(QStringLiteral("delete duplicate success: ")
                       + controller.errorMessage()));
    require(controller.modelRows().size() == 1
                && !QFileInfo::exists(duplicatedPackage),
            "delete removes selected nonactive package");
    require(controller.select(0), qPrintable(controller.errorMessage()));
    const bool activeSet = controller.setActive();
    require(activeSet,
            qPrintable(QStringLiteral("set active success: ")
                       + controller.errorMessage()));
    require(controller.activeId() == importedId, "imported model becomes active");
    require(controller.canDelete() && controller.deleteSelected()
                && controller.activeId().isEmpty()
                && controller.modelRows().isEmpty()
                && activeClearCount == 1 && !QFileInfo::exists(importedPackage),
            "idle Active Model removal clears Active state without selecting a fallback");
}

void testLibraryOwnedIdentityCreation()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "Library identity temporary directory");
    const QString runtimeModels =
        QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR)).filePath(
            QStringLiteral("models"));
    qputenv("OVDS_MODELS_ROOT_PATH", runtimeModels.toUtf8());

    const QString registryPath = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("models")));
    const QString selectedRoot =
        QDir(temporary.path()).filePath(QStringLiteral("selected-root"));
    require(QDir().mkpath(selectedRoot), "create selected identity root");
    OperationCoordinator operations;
    ModelLibraryController controller(registryPath, operations);
    require(controller.refresh(), qPrintable(controller.errorMessage()));
    require(controller.startingWeightOptions(0)
                == QStringList{QStringLiteral("ImageNet")}
                && !controller.addModel(QString(), 0, 0,
                                        QUrl::fromLocalFile(selectedRoot)),
            "Add Model requires a nonblank Name");
    require(controller.addModel(QStringLiteral("New MobileNet Identity"), 0, 0,
                                QUrl::fromLocalFile(selectedRoot)),
            qPrintable(controller.errorMessage()));
    const QVariantList trainingRows = controller.trainingModelRows();
    require(trainingRows.size() == 1
                && trainingRows.front().toMap().value(
                       QStringLiteral("name")).toString()
                       == QStringLiteral("New MobileNet Identity")
                && trainingRows.front().toMap().value(
                       QStringLiteral("architecture")).toString()
                       == QStringLiteral("mobilenet_v3_small")
                && trainingRows.front().toMap().value(
                       QStringLiteral("startingWeights")).toString()
                       == QStringLiteral("ImageNet"),
            "Add Model creates one trainable Library identity");
    const QString identityPackage =
        controller.selectedDetail().value(
            QStringLiteral("packageLocation")).toString();
    require(QFileInfo(identityPackage).isDir()
                && QFileInfo(identityPackage).absolutePath()
                       == QFileInfo(selectedRoot).absoluteFilePath()
                && QFileInfo(identityPackage).absoluteFilePath()
                       .compare(QDir(runtimeModels).filePath(
                                    QStringLiteral(
                                        "templates/blank/mobilenet_v3_small")),
                                Qt::CaseInsensitive) != 0,
            "Library identity owns a distinct local package");
    const QString identityMetadata =
        QDir(identityPackage).filePath(QStringLiteral("metadata.json"));
    const QString identityOnnx =
        QDir(identityPackage).filePath(QStringLiteral("model.onnx"));
    const QString pretrainedOnnx =
        QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR))
            .filePath(QStringLiteral(
                "models/templates/pretrained/mobilenet_v3_small/model.onnx"));
    require(QFileInfo(identityOnnx).isFile()
                || QFile::copy(pretrainedOnnx, identityOnnx),
            "create trained Library ONNX fixture");
    const auto setIdentityStatus = [&identityMetadata](
                                       const QString &status,
                                       bool preserveArtifact = false) {
        QFile file(identityMetadata);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QJsonObject metadata = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        metadata.insert(QStringLiteral("status"), status);
        if (status == QStringLiteral("trained")) {
            const QJsonObject initialization =
                metadata.value(QStringLiteral("initialization")).toObject();
            metadata.insert(
                QStringLiteral("artifact"),
                QJsonObject{
                    {QStringLiteral("onnx_file"), QStringLiteral("model.onnx")},
                    {QStringLiteral("checkpoint_file"),
                     initialization.value(QStringLiteral("weight_file"))},
                    {QStringLiteral("checkpoint_sha256"),
                     initialization.value(QStringLiteral("weight_sha256"))},
                    {QStringLiteral("output_tensor"),
                     QJsonObject{
                         {QStringLiteral("shape"), QJsonArray{1, 3}},
                     }},
                });
        } else if (!preserveArtifact) {
            metadata.remove(QStringLiteral("artifact"));
        }
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented)) > 0;
    };
    require(setIdentityStatus(QStringLiteral("trained")),
            "create trained Library model fixture");
    const QVariantList trainedRows = controller.trainingModelRows();
    require(trainedRows.size() == 1
                && trainedRows.front().toMap()
                       .value(QStringLiteral("weightPath")).toString()
                       == trainingRows.front().toMap()
                              .value(QStringLiteral("weightPath")).toString()
                && trainedRows.front().toMap()
                       .value(QStringLiteral("initializationMode")).toString()
                       == QStringLiteral("checkpoint")
                && !trainedRows.front().toMap().contains(
                    QStringLiteral("compatible"))
                && !trainedRows.front().toMap().contains(
                    QStringLiteral("compatibilityReason")),
            "Training exposes the trained package checkpoint without compatibility state");
    require(setIdentityStatus(QStringLiteral("failed"), true),
            "create non-completed Library model fixture");
    const QVariantList failedRows = controller.trainingModelRows();
    require(failedRows.size() == 1
                && failedRows.front().toMap()
                       .value(QStringLiteral("weightPath")).toString().isEmpty(),
            "Training does not expose a checkpoint from a non-completed package");
    require(setIdentityStatus(QStringLiteral("library_identity")),
            "restore compatible Training identity fixture");
    const QString identityWeight =
        trainingRows.front().toMap().value(QStringLiteral("weightPath")).toString();
    QFile tamperedWeight(identityWeight);
    require(tamperedWeight.open(QIODevice::Append), "open identity weight for tamper");
    require(tamperedWeight.write("tamper") == 6, "tamper identity weight");
    tamperedWeight.close();
    const QVariantList tamperedRows = controller.trainingModelRows();
    require(tamperedRows.size() == 1
                && tamperedRows.front().toMap()
                       .value(QStringLiteral("weightPath")).toString().isEmpty()
                && !tamperedRows.front().toMap().contains(
                    QStringLiteral("compatibilityReason")),
            "Starting Weights hash mismatch leaves a factual unavailable local path");
    const QByteArray registryAfterCreate = bytes(registryPath);
    require(!controller.addModel(QStringLiteral(" new mobilenet identity "), 0, 0,
                                 QUrl::fromLocalFile(selectedRoot))
                && controller.errorMessage().contains(
                    QStringLiteral("already uses"))
                && bytes(registryPath) == registryAfterCreate,
            "Add Model rejects duplicate Names without mutation");

    const QString fallbackRegistry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("fallback-models")));
    OperationCoordinator fallbackOperations;
    ModelLibraryController fallbackController(fallbackRegistry, fallbackOperations);
    require(fallbackController.refresh()
                && fallbackController.addModel(
                    QStringLiteral("Fallback Identity"), 1, 0, QUrl{}),
            qPrintable(fallbackController.errorMessage()));
    const QString fallbackPackage =
        fallbackController.selectedDetail()
            .value(QStringLiteral("packageLocation")).toString();
    require(QFileInfo(fallbackPackage).absolutePath()
                == QDir(QFileInfo(fallbackRegistry).absolutePath())
                       .filePath(QStringLiteral(".opendss-model-identities")),
            "Blank Add Model destination retains the registry-adjacent default");
}

void testCommittedDeleteCleanupWarning()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "cleanup warning temporary directory");
    const QString package = createPackage(
        QDir(temporary.path()).filePath(QStringLiteral("package-source")),
        QStringLiteral("mobilenet_v3_small"));
    const QString registry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("models")),
        QJsonArray{entry(packageEntryId(package), QStringLiteral("Retained Model"),
                         package, false)});
    OperationCoordinator operations;
    ModelLibraryController controller(registry, operations);
    int activeClearCount = 0;
    controller.setActiveModelClearedCallback(
        [&activeClearCount]() { ++activeClearCount; });
    require(controller.refresh() && controller.select(0),
            "select active cleanup-warning model");

    const QString checkpoint =
        QDir(package).filePath(QStringLiteral("checkpoint.pth"));
#ifdef Q_OS_WIN
    const QString userName = qEnvironmentVariable("USERNAME");
    require(!userName.isEmpty(), "resolve current user for cleanup denial");
    require(QProcess::execute(
                QStringLiteral("icacls.exe"),
                {checkpoint, QStringLiteral("/deny"),
                 userName + QStringLiteral(":(D)")}) == 0,
            "deny staged checkpoint deletion");
    require(QProcess::execute(
                QStringLiteral("icacls.exe"),
                {package, QStringLiteral("/deny"),
                 userName + QStringLiteral(":(DC)")}) == 0,
            "deny staged package child deletion");
#else
    const QFileDevice::Permissions originalPermissions =
        QFile::permissions(checkpoint);
    require(QFile::setPermissions(checkpoint, QFileDevice::ReadOwner),
            "make staged checkpoint read only");
#endif
    const bool deleted = controller.deleteSelected();
    const QString recoveryRoot =
        QDir(QFileInfo(package).absolutePath())
            .filePath(QStringLiteral(".opendss-model-recovery"));
    const QStringList retained =
        QDir(recoveryRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    require(!deleted && controller.modelRows().isEmpty()
                && controller.activeId().isEmpty() && activeClearCount == 0
                && !QFileInfo::exists(package)
                && controller.errorMessage().contains(QStringLiteral("committed"))
                && controller.errorMessage().contains(
                    QStringLiteral("retained recovery"))
                && retained.size() == 1,
            "Recycle Bin failure reports retained recovery without false success");

    const QString retainedPackage = QDir(recoveryRoot).filePath(retained.first());
#ifdef Q_OS_WIN
    require(QProcess::execute(
                QStringLiteral("icacls.exe"),
                {retainedPackage, QStringLiteral("/remove:d"), userName,
                 QStringLiteral("/T"), QStringLiteral("/C"),
                 QStringLiteral("/Q")}) == 0,
            "restore retained recovery permissions");
#else
    QFile::setPermissions(
        QDir(retainedPackage).filePath(QStringLiteral("checkpoint.pth")),
        originalPermissions);
#endif
    require(QDir(retainedPackage).removeRecursively(),
            "remove retained recovery fixture");
    QDir().rmdir(recoveryRoot);
}

void testRegistryFailureRollsBackDelete()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "rollback temporary directory");
    const QString modelsRoot = QDir(temporary.path()).filePath(QStringLiteral("models"));
    const QString package =
        createPackage(QDir(temporary.path()).filePath(QStringLiteral("package-source")),
                      QStringLiteral("mobilenet_v3_small"));
    const QString registry = createSimpleRegistry(
        modelsRoot,
        QJsonArray{entry(packageEntryId(package),
                         QStringLiteral("Rollback Model"), package, false)});
    OperationCoordinator operations;
    ModelLibraryController controller(registry, operations);
    require(controller.refresh() && controller.select(0), "select rollback model");
    const QByteArray registryBefore = bytes(registry);
    const QFileDevice::Permissions originalPermissions = QFile::permissions(registry);
    require(QFile::setPermissions(registry, QFileDevice::ReadOwner),
            "make registry read only");
    const bool deleted = controller.deleteSelected();
    QFile::setPermissions(registry, originalPermissions);
    require(!deleted && QFileInfo(package).isDir()
                && bytes(registry) == registryBefore,
            "registry write failure restores staged package and registry");
}

void testStagedPublicationFailureIsRetrySafe()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "staged publication temporary directory");
    const QString sourcePackage =
        createPackage(QDir(temporary.path()).filePath(QStringLiteral("source")),
                      QStringLiteral("mobilenet_v3_small"));
    const QJsonObject sourceMetadata = QJsonDocument::fromJson(
        bytes(QDir(sourcePackage).filePath(QStringLiteral("metadata.json")))).object();
    const QString modelName =
        sourceMetadata.value(QStringLiteral("model_name")).toString();
    const QString replacementId = packageEntryId(sourcePackage);
    const QString registry = createSimpleRegistry(
        QDir(temporary.path()).filePath(QStringLiteral("registry")),
        QJsonArray{entry(replacementId, modelName, sourcePackage, false)});
    const QByteArray validRegistry = bytes(registry);
    QFile invalidRegistry(registry);
    require(invalidRegistry.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && invalidRegistry.write("{invalid") == 8,
            "write deterministic late publication registry failure");
    invalidRegistry.close();

    const QString destination =
        QDir(temporary.path()).filePath(QStringLiteral("destination"));
    QString savedPackage;
    QString registeredId;
    QString error;
    require(!saveTrainedModelArtifacts(
                registry, sourcePackage,
                QDir(sourcePackage).filePath(QStringLiteral("model.onnx")),
                QDir(sourcePackage).filePath(QStringLiteral("metadata.json")),
                {}, {}, {}, {}, {}, modelName, destination, &savedPackage,
                &registeredId, &error, replacementId)
                && error.contains(QStringLiteral("parse"))
                && QFileInfo(destination).isDir()
                && QDir(destination).entryList(
                       QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty(),
            "late publication failure removes staging and exposes no incomplete final");

    QFile restoredRegistry(registry);
    require(restoredRegistry.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && restoredRegistry.write(validRegistry) == validRegistry.size(),
            "restore registry for publication retry");
    restoredRegistry.close();
    error.clear();
    require(saveTrainedModelArtifacts(
                registry, sourcePackage,
                QDir(sourcePackage).filePath(QStringLiteral("model.onnx")),
                QDir(sourcePackage).filePath(QStringLiteral("metadata.json")),
                {}, {}, {}, {}, {}, modelName, destination, &savedPackage,
                &registeredId, &error, replacementId)
                && QFileInfo(savedPackage).isDir()
                && validateCompleteV2ModelPackage(savedPackage, &error)
                && QDir(destination).entryList(
                       {QStringLiteral(".*.staging-*")},
                       QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty(),
            "publication retry atomically exposes one complete final package");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    testRefreshSelectionActivationAndRename();
    testErrorsAndEmptyPresentation();
    testPackagePathSafetyAndRegistryIntegrity();
    testPackageOperations();
    testLibraryOwnedIdentityCreation();
    testCommittedDeleteCleanupWarning();
    testRegistryFailureRollsBackDelete();
    testStagedPublicationFailureIsRetrySafe();
    return 0;
}
