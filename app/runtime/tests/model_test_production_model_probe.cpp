#include "../desktop_app/model_registry_service.h"
#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/model/model_load_service.h"
#include "../v2/model_test/model_test_service.h"
#include "../v2/model_test/model_test_summary_v2.h"
#include "../v2/operation/operation_coordinator.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

#include <array>
#include <optional>

namespace {

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;
using namespace desktop_app::v2::model_test;

constexpr auto ExpectedAuditManifestSha256 =
    "1583ed3ddd9c76c4fbb8badf34932ff79835ac7faaa639e64fb4ee84f3361adf";
constexpr auto ExpectedDatasetJsonSha256 =
    "e6bcea0f2f7e192008381329e4d8c355ba2ae2f0baf867feada0f60f255f1c72";
constexpr auto ExpectedDatasetId =
    "droplet_target_nontarget_3class_starter";
constexpr auto RepresentativeDatasetId =
    "droplet_target_nontarget_3class_starter_representative_6";
constexpr auto ExpectedRegistryEntryId =
    "trained_saved_pre-trained_mobilenetv3-small_-_faster_copy2";
constexpr auto ExpectedMetadataModelId =
    "saved_pre-trained_mobilenetv3-small_-_faster_copy2";
constexpr auto ExpectedModelName =
    "Pre-trained MobileNetV3-Small - Faster copy2";
constexpr auto ExpectedPackagePath =
    "C:/Users/goals/OneDrive/Documents/OpenDropletSortingSuite/models/packages/"
    "trained/Pre-trained MobileNetV3-Small - Faster copy2";
constexpr auto ExpectedOnnxSha256 =
    "18c2fb4ff61a82316bf46aa9d76ef91e8b134b63e21a9bc0ca8b11c367487615";
constexpr qint64 ExpectedRepresentativeCount = 6;
constexpr int ExpectedPerClass = 2;

const std::array<QString, 3> ExpectedClassIds{
    QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2")};
const std::array<QString, 3> ExpectedClassNames{
    QStringLiteral("Empty"), QStringLiteral("Single"),
    QStringLiteral("MoreThanOne")};

struct RepresentativeItem {
    QString classId;
    QString sourcePath;
    QString sourceSha256;
    QString fixtureCropPath;
    QString recordId;
    qint64 manifestIndex = -1;
};

QString normalizedAbsolutePath(const QString& path) {
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

QString canonicalPath(const QString& path) {
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).canonicalFilePath()));
}

Qt::CaseSensitivity pathCaseSensitivity() {
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool samePath(const QString& left, const QString& right) {
    const QString leftCanonical = canonicalPath(left);
    const QString rightCanonical = canonicalPath(right);
    return !leftCanonical.isEmpty() && !rightCanonical.isEmpty() &&
           leftCanonical.compare(rightCanonical, pathCaseSensitivity()) == 0;
}

bool containedPath(const QString& root, const QString& path) {
    const QString rootCanonical = canonicalPath(root);
    const QString pathCanonical = canonicalPath(path);
    if (rootCanonical.isEmpty() || pathCanonical.isEmpty())
        return false;
    const QString prefix = rootCanonical.endsWith('/')
                               ? rootCanonical
                               : rootCanonical + '/';
    return pathCanonical.startsWith(prefix, pathCaseSensitivity());
}

bool localAbsolutePath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path);
    return QFileInfo(path).isAbsolute() && !normalized.startsWith("//") &&
           !normalized.contains("://");
}

bool validSha256(const QString& value) {
    return QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
        .match(value)
        .hasMatch();
}

QString fileSha256(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

std::optional<QJsonObject> readObject(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Could not read '%1': %2")
                     .arg(path, file.errorString());
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        *error = QStringLiteral("Invalid JSON in '%1': %2")
                     .arg(path, parseError.errorString());
        return std::nullopt;
    }
    return document.object();
}

int expectedClassIndex(const QString& classId) {
    for (int index = 0; index < static_cast<int>(ExpectedClassIds.size());
         ++index) {
        if (classId == ExpectedClassIds[index])
            return index;
    }
    return -1;
}

bool expectedClasses(const QJsonArray& classes, const QString& nameKey) {
    if (classes.size() != static_cast<int>(ExpectedClassIds.size()))
        return false;
    for (int index = 0; index < classes.size(); ++index) {
        const QJsonObject item = classes.at(index).toObject();
        if (item.value("id").toString() != ExpectedClassIds[index] ||
            item.value(nameKey).toString() != ExpectedClassNames[index]) {
            return false;
        }
    }
    return true;
}

bool expectedClasses(const QVector<DatasetClass>& classes) {
    if (classes.size() != static_cast<qsizetype>(ExpectedClassIds.size()))
        return false;
    for (qsizetype index = 0; index < classes.size(); ++index) {
        if (classes.at(index).id != ExpectedClassIds[index] ||
            classes.at(index).name != ExpectedClassNames[index]) {
            return false;
        }
    }
    return true;
}

bool expectedClasses(const QVector<PersistedActiveModelClass>& classes) {
    if (classes.size() != static_cast<qsizetype>(ExpectedClassIds.size()))
        return false;
    for (qsizetype index = 0; index < classes.size(); ++index) {
        if (classes.at(index).id != ExpectedClassIds[index] ||
            classes.at(index).displayLabel != ExpectedClassNames[index]) {
            return false;
        }
    }
    return true;
}

bool expectedClasses(const QVector<ModelTestClassSnapshot>& classes) {
    if (classes.size() != static_cast<qsizetype>(ExpectedClassIds.size()))
        return false;
    for (qsizetype index = 0; index < classes.size(); ++index) {
        if (classes.at(index).id != ExpectedClassIds[index] ||
            classes.at(index).name != ExpectedClassNames[index]) {
            return false;
        }
    }
    return true;
}

class CheckedTemporaryRoot {
  public:
    explicit CheckedTemporaryRoot(const QString& pattern)
        : temporary_(pattern) {
        temporary_.setAutoRemove(false);
        if (temporary_.isValid())
            trackedPaths_.push_back(temporary_.path());
    }

    ~CheckedTemporaryRoot() {
        if (!cleaned_) {
            QString ignored;
            cleanup(&ignored);
        }
    }

    bool isValid() const { return temporary_.isValid(); }
    QString path() const { return temporary_.path(); }

    void track(const QString& path) {
        if (!path.trimmed().isEmpty())
            trackedPaths_.push_back(path);
    }

    bool cleanup(QString* error) {
        if (cleaned_)
            return true;
        const QString root = temporary_.path();
        const bool removed =
            root.isEmpty() || !QFileInfo::exists(root) ||
            QDir(root).removeRecursively();
        QStringList residuals;
        for (const QString& path : trackedPaths_) {
            if (QFileInfo::exists(path))
                residuals.push_back(path);
        }
        if (!removed || !residuals.isEmpty()) {
            if (error) {
                *error = QStringLiteral(
                             "Disposable Model Test cleanup failed; residuals: %1")
                             .arg(residuals.join(QStringLiteral(", ")));
            }
            return false;
        }
        cleaned_ = true;
        return true;
    }

  private:
    QTemporaryDir temporary_;
    QStringList trackedPaths_;
    bool cleaned_ = false;
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream failure(stderr);
    const QString usage =
        QStringLiteral("Usage: model_test_production_model_probe "
                       "--dataset-manifest <path> --model-registry <path> "
                       "--python <path> --working-directory <path>");

    const QStringList arguments = application.arguments();
    if (arguments.size() != 9 ||
        arguments.at(1) != QStringLiteral("--dataset-manifest") ||
        arguments.at(3) != QStringLiteral("--model-registry") ||
        arguments.at(5) != QStringLiteral("--python") ||
        arguments.at(7) != QStringLiteral("--working-directory") ||
        arguments.at(2).trimmed().isEmpty() ||
        arguments.at(4).trimmed().isEmpty() ||
        arguments.at(6).trimmed().isEmpty() ||
        arguments.at(8).trimmed().isEmpty()) {
        failure << usage << '\n';
        return 2;
    }

    const QString auditManifestArgument = arguments.at(2).trimmed();
    const QString registryArgument = arguments.at(4).trimmed();
    const QString pythonArgument = arguments.at(6).trimmed();
    const QString workingDirectoryArgument = arguments.at(8).trimmed();
    if (!localAbsolutePath(auditManifestArgument) ||
        !localAbsolutePath(registryArgument) ||
        !localAbsolutePath(pythonArgument) ||
        !localAbsolutePath(workingDirectoryArgument)) {
        failure << "FAIL: All inputs must be absolute local paths.\n";
        return 3;
    }

    const QString auditManifestPath =
        normalizedAbsolutePath(auditManifestArgument);
    const QString registryPath = normalizedAbsolutePath(registryArgument);
    const QString pythonPath = normalizedAbsolutePath(pythonArgument);
    const QString workingDirectory =
        normalizedAbsolutePath(workingDirectoryArgument);
    const auto fail = [&](int code, const QString& message) {
        failure << "FAIL: " << message << '\n';
        return code;
    };

    if (!QFileInfo(auditManifestPath).isFile() ||
        !QFileInfo(auditManifestPath).isReadable()) {
        return fail(4, QStringLiteral("Dataset audit manifest is not readable."));
    }
    if (!QFileInfo(registryPath).isFile() ||
        !QFileInfo(registryPath).isReadable()) {
        return fail(5, QStringLiteral("Model registry is not readable."));
    }
    if (!QFileInfo(pythonPath).isFile() ||
        !QFileInfo(workingDirectory).isDir()) {
        return fail(6, QStringLiteral("Installed Python runtime is unavailable."));
    }

    const QString auditManifestHash = fileSha256(auditManifestPath);
    if (auditManifestHash != QLatin1String(ExpectedAuditManifestSha256)) {
        return fail(7, QStringLiteral("Dataset audit manifest SHA-256 mismatch: %1")
                           .arg(auditManifestHash));
    }

    QDir sourceDatasetRoot(QFileInfo(auditManifestPath).absolutePath());
    if (!sourceDatasetRoot.cdUp()) {
        return fail(8, QStringLiteral("Could not derive the source Dataset root."));
    }
    const QString productionDatasetJsonPath =
        sourceDatasetRoot.filePath("dataset.json");
    if (!QFileInfo(productionDatasetJsonPath).isFile() ||
        QFileInfo(productionDatasetJsonPath).isSymLink()) {
        return fail(9, QStringLiteral("Production dataset.json is unavailable."));
    }
    const QString productionDatasetJsonHash =
        fileSha256(productionDatasetJsonPath);
    if (productionDatasetJsonHash !=
        QLatin1String(ExpectedDatasetJsonSha256)) {
        return fail(10, QStringLiteral("Production dataset.json SHA-256 mismatch: %1")
                            .arg(productionDatasetJsonHash));
    }

    QString error;
    const auto audit = readObject(auditManifestPath, &error);
    if (!audit) {
        return fail(11, error);
    }
    const QJsonArray auditItems = audit->value("items").toArray();
    if (audit->value("schema_version").toString() !=
            QStringLiteral("dataset-manifest-v1") ||
        audit->value("dataset_id").toString() !=
            QLatin1String(ExpectedDatasetId) ||
        auditItems.isEmpty() ||
        !expectedClasses(audit->value("classes").toArray(),
                         QStringLiteral("display_name"))) {
        return fail(12, QStringLiteral("Pinned Dataset audit facts mismatch."));
    }

    std::array<int, 3> selectedPerClass{};
    QVector<RepresentativeItem> selected;
    selected.reserve(ExpectedRepresentativeCount);
    for (qsizetype index = 0; index < auditItems.size(); ++index) {
        const QJsonObject item = auditItems.at(index).toObject();
        const int classIndex =
            expectedClassIndex(item.value("label").toString());
        if (classIndex < 0 ||
            selectedPerClass[classIndex] >= ExpectedPerClass ||
            item.value("status").toString() != QStringLiteral("included") ||
            !item.value("trainer_eligible").toBool()) {
            continue;
        }

        const QString relativePath = QDir::cleanPath(
            QDir::fromNativeSeparators(item.value("path").toString()));
        const QString declaredHash =
            item.value("hash_sha256").toString().trimmed().toLower();
        if (relativePath.isEmpty() || QDir::isAbsolutePath(relativePath) ||
            relativePath == QStringLiteral("..") ||
            relativePath.startsWith(QStringLiteral("../")) ||
            !validSha256(declaredHash)) {
            continue;
        }
        const QString sourcePath =
            normalizedAbsolutePath(sourceDatasetRoot.filePath(relativePath));
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.isFile() || !sourceInfo.isReadable() ||
            sourceInfo.isSymLink() ||
            !containedPath(sourceDatasetRoot.absolutePath(), sourcePath) ||
            fileSha256(sourcePath) != declaredHash) {
            continue;
        }

        RepresentativeItem representative;
        representative.classId = ExpectedClassIds[classIndex];
        representative.sourcePath = sourcePath;
        representative.sourceSha256 = declaredHash;
        representative.recordId =
            QStringLiteral("representative-%1").arg(index, 6, 10, QLatin1Char('0'));
        representative.manifestIndex = index;
        selected.push_back(representative);
        ++selectedPerClass[classIndex];
    }
    if (selected.size() != ExpectedRepresentativeCount ||
        selectedPerClass !=
            std::array<int, 3>{ExpectedPerClass, ExpectedPerClass,
                               ExpectedPerClass}) {
        return fail(
            13,
            QStringLiteral(
                "Could not select exactly two manifest-order, declared-hash-valid "
                "included trainer records per class."));
    }
    for (qsizetype index = 1; index < selected.size(); ++index) {
        if (selected.at(index - 1).manifestIndex >=
            selected.at(index).manifestIndex) {
            return fail(14, QStringLiteral("Representative selection order changed."));
        }
    }

    CheckedTemporaryRoot temporary(
        QDir::tempPath() +
        QStringLiteral("/opendss-model-test-representative-XXXXXX"));
    if (!temporary.isValid()) {
        return fail(15, QStringLiteral("Disposable fixture root is unavailable."));
    }
    const auto finish = [&](int code, const QString& message) {
        QString cleanupError;
        if (!temporary.cleanup(&cleanupError)) {
            failure << "FAIL: " << cleanupError << '\n';
            return 90;
        }
        if (code != 0)
            failure << "FAIL: " << message << '\n';
        return code;
    };
    const QString fixtureRoot =
        QDir(temporary.path()).filePath("representative-dataset");
    const QString cropsRoot = QDir(fixtureRoot).filePath("crops");
    const QString reportsRoot = QDir(temporary.path()).filePath("reports");
    temporary.track(fixtureRoot);
    temporary.track(reportsRoot);
    if (!QDir().mkpath(cropsRoot) || !QDir().mkpath(reportsRoot)) {
        return finish(
            16, QStringLiteral("Could not create disposable fixture folders."));
    }

    DatasetManifestData fixtureData;
    fixtureData.datasetId = QLatin1String(RepresentativeDatasetId);
    auto& provenance = fixtureData.provenance;
    provenance.name = QStringLiteral("Pinned representative Model Test Dataset");
    provenance.opendssVersion = QStringLiteral("2");
    provenance.createdAt = QStringLiteral("2026-07-27T00:00:00Z");
    provenance.updatedAt = provenance.createdAt;
    provenance.captureStartedAt = provenance.createdAt;
    provenance.captureEndedAt = provenance.createdAt;
    provenance.stopReason = QStringLiteral("representative_validation_fixture");
    provenance.status = QStringLiteral("completed");
    provenance.sequence.frameCount = ExpectedRepresentativeCount;
    provenance.sequence.imageWidth = 64;
    provenance.sequence.imageHeight = 64;
    provenance.sequence.bitDepth = 8;
    provenance.sequence.nominalFps = 1.0;
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(ExpectedClassIds.size()); ++index) {
        fixtureData.classes.push_back(
            {ExpectedClassIds[index], ExpectedClassNames[index]});
    }

    for (qsizetype index = 0; index < selected.size(); ++index) {
        auto& item = selected[index];
        const QString destination =
            QDir(cropsRoot)
                .filePath(QStringLiteral("%1.png")
                              .arg(index, 2, 10, QLatin1Char('0')));
        if (!QFile::copy(item.sourcePath, destination)) {
            return finish(
                17, QStringLiteral("Could not copy representative image %1.")
                        .arg(index));
        }
        item.fixtureCropPath = QDir::fromNativeSeparators(
            QDir(fixtureRoot).relativeFilePath(destination));
        fixtureData.records.push_back(
            {item.recordId,
             item.fixtureCropPath,
             item.sourceSha256,
             QStringLiteral("source-%1").arg(item.manifestIndex),
             QStringLiteral("representative"),
             provenance.createdAt,
             QRect(0, 0, 64, 64),
             item.manifestIndex});
        fixtureData.labels.push_back(
            {QStringLiteral("label-%1").arg(index), item.recordId,
             item.classId, false});
    }

    const QString fixtureDatasetJson =
        QDir(fixtureRoot).filePath("dataset.json");
    if (!DatasetManifestV2::save(fixtureDatasetJson, fixtureData, &error)) {
        return finish(
            18,
            QStringLiteral("Could not write representative dataset.json: %1")
                .arg(error));
    }
    const auto fixture =
        DatasetManifestV2::load(fixtureDatasetJson, &error);
    if (!fixture ||
        fixture->datasetId() != QLatin1String(RepresentativeDatasetId) ||
        fixture->records().size() != ExpectedRepresentativeCount ||
        fixture->trainingSamples(&error).size() !=
            ExpectedRepresentativeCount ||
        !expectedClasses(fixture->classes())) {
        return finish(
            19,
            fixture ? QStringLiteral("Representative Dataset facts mismatch.")
                    : error);
    }

    const QString registryHashBefore = fileSha256(registryPath);
    QString registryWarning;
    const QJsonArray registryEntries =
        readModelRegistryEntriesFromPath(registryPath, &registryWarning);
    int activeCount = 0;
    QJsonObject activeEntry;
    for (const QJsonValue& value : registryEntries) {
        const QJsonObject entry = value.toObject();
        if (entry.value("active").toBool()) {
            activeEntry = entry;
            ++activeCount;
        }
    }
    if (!registryWarning.isEmpty() || activeCount != 1) {
        return finish(
            20,
            registryWarning.isEmpty()
                ? QStringLiteral("Registry must have exactly one Active Model.")
                : registryWarning);
    }
    const QString configuredPackage =
        registryString(activeEntry, QStringLiteral("package_path"));
    if (registryString(activeEntry, QStringLiteral("registry_entry_id")) !=
            QLatin1String(ExpectedRegistryEntryId) ||
        registryString(activeEntry, QStringLiteral("display_name")) !=
            QLatin1String(ExpectedModelName) ||
        !localAbsolutePath(configuredPackage) ||
        !samePath(configuredPackage, QLatin1String(ExpectedPackagePath))) {
        return finish(
            21, QStringLiteral("Active Model registry identity/path mismatch."));
    }

    const ModelPackageInspection package = inspectModelPackage(activeEntry);
    const QString expectedOnnxPath =
        QDir(QLatin1String(ExpectedPackagePath)).filePath("model.onnx");
    const QString expectedMetadataPath =
        QDir(QLatin1String(ExpectedPackagePath)).filePath("metadata.json");
    if (!package.canActivate ||
        !samePath(package.packagePath, QLatin1String(ExpectedPackagePath)) ||
        !samePath(package.onnxPath, expectedOnnxPath) ||
        !samePath(package.metadataPath, expectedMetadataPath) ||
        !QFileInfo(package.checkpointPath).isFile() ||
        QFileInfo(package.onnxPath).isSymLink() ||
        QFileInfo(package.checkpointPath).isSymLink() ||
        QFileInfo(package.metadataPath).isSymLink()) {
        return finish(22, package.message.isEmpty()
                              ? QStringLiteral("Active Model package mismatch.")
                              : package.message);
    }
    const QString onnxHashBefore = fileSha256(package.onnxPath);
    const QString checkpointHashBefore = fileSha256(package.checkpointPath);
    const QString metadataHashBefore = fileSha256(package.metadataPath);
    if (onnxHashBefore != QLatin1String(ExpectedOnnxSha256) ||
        !validSha256(checkpointHashBefore) ||
        !validSha256(metadataHashBefore)) {
        return finish(23, QStringLiteral("Active Model artifact hash mismatch."));
    }
    const auto metadata = readObject(package.metadataPath, &error);
    const QJsonObject metadataArtifact =
        metadata ? metadata->value("artifact").toObject() : QJsonObject{};
    if (!metadata ||
        metadata->value("model_id").toString() !=
            QLatin1String(ExpectedMetadataModelId) ||
        metadata->value("model_name").toString() !=
            QLatin1String(ExpectedModelName) ||
        metadata->value("classes").toArray() !=
            QJsonArray{ExpectedClassIds[0], ExpectedClassIds[1],
                       ExpectedClassIds[2]} ||
        metadataArtifact.value("checkpoint_file").toString() !=
            QFileInfo(package.checkpointPath).fileName() ||
        metadataArtifact.value("onnx_file").toString() !=
            QFileInfo(package.onnxPath).fileName() ||
        (metadataArtifact.contains("checkpoint_sha256") &&
         metadataArtifact.value("checkpoint_sha256")
                 .toString()
                 .toLower() != checkpointHashBefore)) {
        return finish(24, metadata
                              ? QStringLiteral("Active Model metadata mismatch.")
                              : error);
    }

    ModelLoadService productionLoader(registryPath);
    const PersistedActiveModelInspection inspection =
        productionLoader.inspectPersistedActive();
    if (!inspection.loadable ||
        inspection.id != QLatin1String(ExpectedRegistryEntryId) ||
        inspection.displayName != QLatin1String(ExpectedModelName) ||
        inspection.modelSha256 != QLatin1String(ExpectedOnnxSha256) ||
        (inspection.plannedDevice != QStringLiteral("GPU") &&
         inspection.plannedDevice != QStringLiteral("CPU")) ||
        !expectedClasses(inspection.classes)) {
        return finish(
            25,
            inspection.error.isEmpty()
                ? QStringLiteral("Active Model loader inspection mismatch.")
                : inspection.error);
    }

    const QString temporaryPackage =
        QDir(temporary.path()).filePath("active-model-package");
    const QString temporaryRegistry =
        QDir(temporary.path()).filePath("model_registry.json");
    const QString temporaryOnnx =
        QDir(temporaryPackage).filePath(QFileInfo(package.onnxPath).fileName());
    const QString temporaryCheckpoint =
        QDir(temporaryPackage)
            .filePath(QFileInfo(package.checkpointPath).fileName());
    const QString temporaryMetadata =
        QDir(temporaryPackage)
            .filePath(QFileInfo(package.metadataPath).fileName());
    temporary.track(temporaryPackage);
    temporary.track(temporaryRegistry);
    temporary.track(temporaryOnnx);
    temporary.track(temporaryCheckpoint);
    temporary.track(temporaryMetadata);
    if (!QDir().mkpath(temporaryPackage) ||
        !QFile::copy(package.onnxPath, temporaryOnnx) ||
        !QFile::copy(package.checkpointPath, temporaryCheckpoint) ||
        !QFile::copy(package.metadataPath, temporaryMetadata)) {
        return finish(26, QStringLiteral("Could not copy Active Model into TEMP."));
    }

    auto registryObject = readObject(registryPath, &error);
    if (!registryObject)
        return finish(27, error);
    QJsonArray temporaryEntries = registryObject->value("entries").toArray();
    int rewrittenActiveCount = 0;
    for (qsizetype index = 0; index < temporaryEntries.size(); ++index) {
        QJsonObject entry = temporaryEntries.at(index).toObject();
        if (!entry.value("active").toBool())
            continue;
        entry.insert("package_path", temporaryPackage);
        temporaryEntries.replace(index, entry);
        ++rewrittenActiveCount;
    }
    registryObject->insert("entries", temporaryEntries);
    QFile temporaryRegistryFile(temporaryRegistry);
    const QByteArray temporaryRegistryBytes =
        QJsonDocument(*registryObject).toJson(QJsonDocument::Indented);
    bool temporaryRegistryWritten = false;
    if (rewrittenActiveCount == 1 &&
        temporaryRegistryFile.open(QIODevice::WriteOnly |
                                   QIODevice::NewOnly)) {
        temporaryRegistryWritten =
            temporaryRegistryFile.write(temporaryRegistryBytes) ==
                temporaryRegistryBytes.size() &&
            temporaryRegistryFile.flush();
        temporaryRegistryFile.close();
    }
    if (!temporaryRegistryWritten) {
        return finish(28, QStringLiteral("Could not write TEMP model registry."));
    }

    ModelLoadService loader(temporaryRegistry);
    const PersistedActiveCheckpointInspection checkpointInspection =
        loader.inspectAndMigratePersistedActiveCheckpoint();
    const QString temporaryMetadataHash = fileSha256(temporaryMetadata);
    if (!checkpointInspection.loadable ||
        checkpointInspection.checkpointSha256 != checkpointHashBefore ||
        checkpointInspection.metadataSha256 != temporaryMetadataHash ||
        fileSha256(temporaryOnnx) != onnxHashBefore ||
        fileSha256(temporaryCheckpoint) != checkpointHashBefore ||
        !validSha256(temporaryMetadataHash) ||
        fileSha256(package.metadataPath) != metadataHashBefore ||
        fileSha256(registryPath) != registryHashBefore) {
        return finish(
            29,
            checkpointInspection.error.isEmpty()
                ? QStringLiteral(
                      "TEMP Active Model checkpoint inspection failed.")
                : checkpointInspection.error);
    }

    qint64 progressProcessed = 0;
    qint64 progressEligible = 0;
    int completedBatches = 0;
    OperationCoordinator operations;
    ModelTestService service(
        operations, &loader, {},
        [&](qint64 processed, qint64 eligible) {
            if (processed > progressProcessed)
                ++completedBatches;
            progressProcessed = processed;
            progressEligible = eligible;
        },
        pythonPath, workingDirectory);
    const QString outputRoot =
        QDir(reportsRoot).filePath("model-test-1");
    temporary.track(outputRoot);
    QElapsedTimer elapsed;
    elapsed.start();
    const bool ran = service.run(
        {fixtureDatasetJson, outputRoot, QStringLiteral("2")}, &error);
    const qint64 elapsedMs = elapsed.elapsed();

    const auto sourceInputsUnchanged = [&]() {
        if (fileSha256(auditManifestPath) != auditManifestHash ||
            fileSha256(productionDatasetJsonPath) !=
                productionDatasetJsonHash ||
            fileSha256(registryPath) != registryHashBefore ||
            fileSha256(package.onnxPath) != onnxHashBefore ||
            fileSha256(package.checkpointPath) != checkpointHashBefore ||
            fileSha256(package.metadataPath) != metadataHashBefore) {
            return false;
        }
        for (const auto& item : selected) {
            if (fileSha256(item.sourcePath) != item.sourceSha256)
                return false;
        }
        return true;
    };
    if (!sourceInputsUnchanged()) {
        return finish(
            30,
            QStringLiteral("A pinned source input changed during the probe."));
    }
    if (!ran) {
        return finish(
            31, QStringLiteral("Production Model Test failed: %1").arg(error));
    }
    if (elapsedMs <= 0 || elapsedMs > 30000 ||
        completedBatches < 1 ||
        progressProcessed != ExpectedRepresentativeCount ||
        progressEligible != ExpectedRepresentativeCount) {
        return finish(
            32, QStringLiteral("Bounded durable-batch progress mismatch."));
    }

    const QString summaryPath =
        QDir(outputRoot).filePath("model_test_summary.json");
    const QString predictionsPath =
        QDir(outputRoot).filePath("predictions.csv");
    temporary.track(summaryPath);
    temporary.track(predictionsPath);
    const auto summaryObject = readObject(summaryPath, &error);
    auto summary = ModelTestSummaryV2::load(summaryPath, &error);
    if (!summaryObject || !summary) {
        return finish(
            33,
            QStringLiteral("Final summary/CSV coherence failed: %1").arg(error));
    }
    const auto& data = summary->data();
    const auto& predictions = summary->predictions();
    const auto& derived = summary->derivedResults();
    const QJsonObject rawActiveModel =
        summaryObject->value("active_model").toObject();
    const QJsonObject rawDevice = summaryObject->value("device").toObject();
    const QString expectedEffective =
        data.effectiveDevice == EffectiveDevice::Cuda
            ? QStringLiteral("cuda")
            : QStringLiteral("cpu");
    const bool truthfulFallback =
        (data.effectiveDevice == EffectiveDevice::Cuda &&
         !data.fallbackWarning && !rawDevice.contains("fallback_warning")) ||
        (data.effectiveDevice == EffectiveDevice::Cpu &&
         data.fallbackWarning &&
         *data.fallbackWarning ==
             QStringLiteral(
                 "CUDA was unavailable or unusable; automatic Model Test used CPU.") &&
         rawDevice.value("fallback_warning").toString() ==
             *data.fallbackWarning);
    if (rawDevice.value("effective").toString() != expectedEffective ||
        !truthfulFallback) {
        return finish(
            34, QStringLiteral("Actual/effective device facts mismatch."));
    }

    qint64 confusionSum = 0;
    for (const auto& row : derived.confusionMatrix) {
        for (qint64 value : row)
            confusionSum += value;
    }
    if (data.schemaVersion !=
            QLatin1String(ModelTestSummaryV2::SchemaVersion) ||
        data.status != ModelTestStatus::Completed ||
        data.stopReason != QStringLiteral("end_of_dataset") ||
        data.eligibleImages != ExpectedRepresentativeCount ||
        derived.processedImages != ExpectedRepresentativeCount ||
        predictions.size() != ExpectedRepresentativeCount ||
        confusionSum != ExpectedRepresentativeCount ||
        data.dataset.id != QLatin1String(RepresentativeDatasetId) ||
        !samePath(data.dataset.sourcePath, fixtureRoot) ||
        !expectedClasses(data.dataset.classes) ||
        data.activeModel.id != QLatin1String(ExpectedRegistryEntryId) ||
        data.activeModel.name != QLatin1String(ExpectedModelName) ||
        data.activeModel.checkpointSha256 != checkpointHashBefore ||
        data.activeModel.metadataSha256 != temporaryMetadataHash ||
        !data.activeModel.onnxSha256.isEmpty() ||
        rawActiveModel.contains("onnx_sha256") ||
        rawActiveModel.value("checkpoint_sha256").toString() !=
            checkpointHashBefore ||
        rawActiveModel.value("metadata_sha256").toString() !=
            temporaryMetadataHash ||
        !expectedClasses(data.activeModel.classes)) {
        return finish(
            35, QStringLiteral("Final Model Test summary facts mismatch."));
    }
    if (derived.perClass.size() !=
        static_cast<qsizetype>(ExpectedClassIds.size())) {
        return finish(
            36, QStringLiteral("Final per-class metric count mismatch."));
    }
    for (qsizetype index = 0; index < derived.perClass.size(); ++index) {
        if (derived.perClass.at(index).classId != ExpectedClassIds[index] ||
            derived.perClass.at(index).support != ExpectedPerClass) {
            return finish(
                37, QStringLiteral("Final per-class support mismatch."));
        }
    }
    for (qsizetype index = 0; index < predictions.size(); ++index) {
        if (predictions.at(index).imagePath !=
                selected.at(index).fixtureCropPath ||
            predictions.at(index).trueClassId !=
                selected.at(index).classId) {
            return finish(
                38, QStringLiteral("Ordered prediction facts mismatch."));
        }
    }

    QFile predictionsFile(predictionsPath);
    if (!predictionsFile.open(QIODevice::ReadOnly)) {
        return finish(39, QStringLiteral("Final predictions.csv is unreadable."));
    }
    const QByteArray predictionsBytes = predictionsFile.readAll();
    predictionsFile.close();
    if (predictionsBytes.count('\n') != ExpectedRepresentativeCount + 1) {
        return finish(
            40, QStringLiteral("predictions.csv line count mismatch."));
    }
    if (!sourceInputsUnchanged()) {
        return finish(
            41,
            QStringLiteral("A pinned source input changed after validation."));
    }
    QStringList residualOutputNames;
    QDirIterator residualOutput(
        outputRoot,
        {QStringLiteral("*.partial*"),
         QStringLiteral("model_test_checkpoint.json")},
        QDir::Files, QDirIterator::Subdirectories);
    const QDir outputDirectory(outputRoot);
    while (residualOutput.hasNext()) {
        residualOutputNames.push_back(QDir::fromNativeSeparators(
            outputDirectory.relativeFilePath(residualOutput.next())));
    }
    residualOutputNames.sort(Qt::CaseInsensitive);
    if (!residualOutputNames.isEmpty()) {
        return finish(
            42,
            QStringLiteral("Residual Model Test output artifacts remain: %1")
                .arg(residualOutputNames.join(QStringLiteral(", "))));
    }

    QString cleanupError;
    if (!temporary.cleanup(&cleanupError)) {
        failure << "FAIL: " << cleanupError << '\n';
        return 90;
    }
    output << "PASS source_dataset_id=" << ExpectedDatasetId
           << " representative_dataset_id=" << data.dataset.id
           << " audit_sha256=" << auditManifestHash.toUpper()
           << " selected=6 supports=2,2,2"
           << " planned_device=" << inspection.plannedDevice
           << " effective_device=" << expectedEffective
           << " fallback="
           << (data.fallbackWarning ? *data.fallbackWarning
                                    : QStringLiteral("none"))
           << " completed_batches=" << completedBatches
           << " elapsed_ms=" << elapsedMs
           << " checkpoint_sha256="
           << data.activeModel.checkpointSha256.toUpper()
           << " metadata_sha256="
           << data.activeModel.metadataSha256.toUpper()
           << " predictions_lines=" << predictionsBytes.count('\n')
           << Qt::endl;
    return 0;
}
