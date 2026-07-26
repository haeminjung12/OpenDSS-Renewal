#include "../desktop_app/model_registry_service.h"
#include "../desktop_app/pipeline_runner.h"
#include "../v2/model/model_load_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr auto kInitialId = "fixture_initial";
constexpr auto kCandidateId = "fixture_candidate";

int fail(int code, const QString& message) {
    std::cerr << message.toStdString() << '\n';
    return code;
}

QByteArray readBytes(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QJsonObject readJsonObject(const QString& path) {
    const QJsonDocument document = QJsonDocument::fromJson(readBytes(path));
    return document.isObject() ? document.object() : QJsonObject{};
}

bool writeJsonObject(const QString& path, const QJsonObject& object) {
    return writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QString sha256File(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

bool preparePackageFixture(const QString& packagePath, QString* error) {
    const QDir source(QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR))
                          .filePath("models/templates/pretrained/mobilenet_v3_small"));
    const QByteArray sourceMetadata = readBytes(source.filePath("metadata.json"));
    const QByteArray sourceModel = readBytes(source.filePath("model.onnx"));
    if (sourceMetadata.isEmpty() || sourceModel.isEmpty()) {
        if (error)
            *error = "Bundled MobileNetV3-Small package is incomplete.";
        return false;
    }

    if (!QDir().mkpath(packagePath)) {
        if (error)
            *error = "Could not create the temporary package directory.";
        return false;
    }
    const QDir destination(packagePath);
    if (!QFile::copy(source.filePath("metadata.json"), destination.filePath("metadata.json")) ||
        !QFile::copy(source.filePath("model.onnx"), destination.filePath("model.onnx"))) {
        if (error)
            *error = "Could not copy the bundled package.";
        return false;
    }
    if (readBytes(destination.filePath("metadata.json")) != sourceMetadata ||
        readBytes(destination.filePath("model.onnx")) != sourceModel) {
        if (error)
            *error = "The copied package does not match the bundled package.";
        return false;
    }
    return true;
}

QJsonObject registryEntry(const QString& id, const QString& packagePath, bool active) {
    return QJsonObject{{"registry_entry_id", id},
                       {"display_name", id},
                       {"package_path", QDir::cleanPath(packagePath)},
                       {"active", active}};
}

QJsonObject registryObject(const QString& packagePath) {
    return QJsonObject{{"schema_version", "model-registry-v3-simple"},
                       {"entries", QJsonArray{registryEntry(kInitialId, packagePath, true),
                                              registryEntry(kCandidateId, packagePath, false)}}};
}

bool hasSingleActiveEntry(const QString& path, const QString& expectedId) {
    int activeCount = 0;
    QString activeId;
    for (const QJsonValue& value : readJsonObject(path).value("entries").toArray()) {
        const QJsonObject entry = value.toObject();
        if (entry.value("active").toBool(false)) {
            ++activeCount;
            activeId = registryString(entry, "registry_entry_id");
        }
    }
    return activeCount == 1 && activeId == expectedId;
}

bool expectPrepareFailure(const desktop_app::v2::ModelLoadService& service,
                          const QString& entryId,
                          const QString& registryPath,
                          const PipelineRunner& pipeline,
                          QString* failure) {
    const QByteArray registryBefore = readBytes(registryPath);
    const std::string modelBefore = pipeline.loadedModelId();
    const bool readyBefore = pipeline.isReady();
    QString error;
    if (service.prepare(entryId, "cpu", nullptr, &error)) {
        if (failure)
            *failure = "Invalid candidate unexpectedly prepared successfully.";
        return false;
    }
    if (error.isEmpty()) {
        if (failure)
            *failure = "Invalid candidate did not report an error.";
        return false;
    }
    if (readBytes(registryPath) != registryBefore ||
        pipeline.loadedModelId() != modelBefore ||
        pipeline.isReady() != readyBefore) {
        if (failure)
            *failure = "Candidate preparation failure changed registry bytes or runtime state.";
        return false;
    }
    return true;
}

#ifdef Q_OS_WIN
class RegistryCommitBlocker {
  public:
    explicit RegistryCommitBlocker(const QString& path)
        : handle_(CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
                              GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr)) {}

    ~RegistryCommitBlocker() {
        if (valid())
            CloseHandle(handle_);
    }

    bool valid() const {
        return handle_ != INVALID_HANDLE_VALUE;
    }

  private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};
#endif

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return fail(2, "Could not create a temporary test directory.");

    const QString packagePath = QDir(temp.path()).filePath("package");
    QString fixtureError;
    if (!preparePackageFixture(packagePath, &fixtureError))
        return fail(3, fixtureError);

    const QString metadataPath = QDir(packagePath).filePath("metadata.json");
    const QString modelPath = QDir(packagePath).filePath("model.onnx");
    const QByteArray validMetadataBytes = readBytes(metadataPath);
    const QByteArray validModelBytes = readBytes(modelPath);
    const QJsonObject validMetadata = QJsonDocument::fromJson(validMetadataBytes).object();
    const QJsonObject validArtifact = validMetadata.value("artifact").toObject();
    if (validMetadata.value("status").toString() != "trained" ||
        validArtifact.value("onnx_file").toString() != "model.onnx" ||
        validArtifact.value("onnx_sha256").toString().compare(sha256File(modelPath), Qt::CaseInsensitive) != 0 ||
        !validArtifact.value("external_data_files").toArray().isEmpty()) {
        return fail(4, "Bundled metadata does not describe the copied trained monolithic ONNX package.");
    }

    const QString registryDirectory = QDir(temp.path()).filePath("registry");
    if (!QDir().mkpath(registryDirectory))
        return fail(5, "Could not create the temporary registry directory.");
    const QString registryPath = QDir(registryDirectory).filePath("model_registry.json");
    if (!writeJsonObject(registryPath, registryObject(packagePath)))
        return fail(6, "Could not write the temporary model registry.");

    desktop_app::v2::ModelLoadService service(registryPath);
    PipelineRunner pipeline;
    QString error;

    qputenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE", "1");
    const auto activeInspection = service.inspectPersistedActive();
    if (!activeInspection.loadable || activeInspection.id != kInitialId ||
        activeInspection.displayName != kInitialId ||
        activeInspection.modelSha256 !=
            validArtifact.value("onnx_sha256").toString().toLower() ||
        activeInspection.classes.size() != 3 ||
        activeInspection.classes.at(0).id != "0" ||
        activeInspection.classes.at(0).displayLabel != "Empty" ||
        activeInspection.classes.at(1).id != "1" ||
        activeInspection.classes.at(1).displayLabel != "Single" ||
        activeInspection.classes.at(2).id != "2" ||
        activeInspection.classes.at(2).displayLabel != "MoreThanOne" ||
        activeInspection.classCount != 3 ||
        activeInspection.plannedDevice != "CPU" ||
        !activeInspection.error.isEmpty()) {
        return fail(
            73,
            "Compact-registry Active Model inspection did not publish validated snapshot facts.");
    }

    QJsonObject metadataWithoutDeclaredHash = validMetadata;
    QJsonObject artifactWithoutDeclaredHash =
        metadataWithoutDeclaredHash.value("artifact").toObject();
    artifactWithoutDeclaredHash.remove("onnx_sha256");
    metadataWithoutDeclaredHash["artifact"] = artifactWithoutDeclaredHash;
    if (!writeJsonObject(metadataPath, metadataWithoutDeclaredHash)) {
        return fail(74, "Could not create missing-hash Active Model fixture.");
    }
    const auto missingHashInspection = service.inspectPersistedActive();
    if (missingHashInspection.loadable ||
        !missingHashInspection.modelSha256.isEmpty() ||
        !missingHashInspection.error.contains(
            "no trusted declared ONNX SHA-256")) {
        return fail(
            75,
            "Active Model inspection accepted or obscured a missing trusted ONNX hash.");
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(76, "Could not restore metadata after missing-hash inspection.");

    const QString installedPackagePath =
        QDir(temp.path()).filePath("installed-pretrained");
    if (!QDir().mkpath(installedPackagePath) ||
        !QFile::copy(modelPath,
                     QDir(installedPackagePath).filePath("model.onnx")) ||
        !writeJsonObject(
            QDir(installedPackagePath).filePath("metadata.json"),
            metadataWithoutDeclaredHash)) {
        return fail(78, "Could not create installed pretrained repair fixture.");
    }
    QString repairError;
    if (!repairTrustedPretrainedMetadataHash(
            installedPackagePath, packagePath, &repairError)) {
        return fail(79, "Trusted pretrained metadata repair failed: " +
                            repairError);
    }
    const QJsonObject repairedMetadata = QJsonDocument::fromJson(
        readBytes(QDir(installedPackagePath).filePath("metadata.json")))
                                             .object();
    if (repairedMetadata.value("artifact")
            .toObject()
            .value("onnx_sha256")
            .toString()
            .compare(validArtifact.value("onnx_sha256").toString(),
                     Qt::CaseInsensitive) != 0) {
        return fail(
            80,
            "Trusted pretrained metadata repair wrote the wrong hash.");
    }

    const QString mismatchedInstalledPath =
        QDir(temp.path()).filePath("mismatched-installed-pretrained");
    if (!QDir().mkpath(mismatchedInstalledPath) ||
        !writeBytes(QDir(mismatchedInstalledPath).filePath("model.onnx"),
                    QByteArray("different model bytes")) ||
        !writeJsonObject(
            QDir(mismatchedInstalledPath).filePath("metadata.json"),
            metadataWithoutDeclaredHash)) {
        return fail(81, "Could not create mismatched pretrained repair fixture.");
    }
    if (repairTrustedPretrainedMetadataHash(
            mismatchedInstalledPath, packagePath, &repairError)) {
        return fail(82, "Mismatched pretrained bytes received a trusted hash.");
    }
    const QJsonObject unrepairedMetadata = QJsonDocument::fromJson(
        readBytes(QDir(mismatchedInstalledPath).filePath("metadata.json")))
                                               .object();
    if (!unrepairedMetadata.value("artifact")
             .toObject()
             .value("onnx_sha256")
             .toString()
             .isEmpty()) {
        return fail(83, "Rejected pretrained repair modified metadata.");
    }

    QJsonObject tamperedPreprocessing = metadataWithoutDeclaredHash;
    QJsonObject tamperedNormalization =
        tamperedPreprocessing.value("normalization").toObject();
    tamperedNormalization["mean"] = QJsonArray{0.0, 0.0, 0.0};
    tamperedPreprocessing["normalization"] = tamperedNormalization;

    QJsonObject tamperedClasses = metadataWithoutDeclaredHash;
    tamperedClasses["classes"] = QJsonArray{"1", "0"};

    QJsonObject tamperedRouting = metadataWithoutDeclaredHash;
    QJsonObject tamperedSortingPolicy =
        tamperedRouting.value("sorting_policy").toObject();
    tamperedSortingPolicy["trigger_rule"] =
        QStringLiteral("trigger_on_non_target_class");
    tamperedRouting["sorting_policy"] = tamperedSortingPolicy;

    const QList<QPair<QString, QJsonObject>> tamperedMetadataCases = {
        {QStringLiteral("preprocessing"), tamperedPreprocessing},
        {QStringLiteral("classes"), tamperedClasses},
        {QStringLiteral("routing"), tamperedRouting},
    };
    for (const auto& tamperedCase : tamperedMetadataCases) {
        const QString tamperedPackagePath =
            QDir(temp.path()).filePath(
                QStringLiteral("tampered-%1-installed-pretrained")
                    .arg(tamperedCase.first));
        const QString tamperedMetadataPath =
            QDir(tamperedPackagePath).filePath("metadata.json");
        if (!QDir().mkpath(tamperedPackagePath) ||
            !QFile::copy(modelPath,
                         QDir(tamperedPackagePath).filePath("model.onnx")) ||
            !writeJsonObject(tamperedMetadataPath, tamperedCase.second)) {
            return fail(
                84,
                "Could not create tampered pretrained metadata fixture: " +
                    tamperedCase.first);
        }
        const QByteArray metadataBytesBeforeRepair =
            readBytes(tamperedMetadataPath);
        if (repairTrustedPretrainedMetadataHash(
                tamperedPackagePath, packagePath, &repairError)) {
            return fail(
                85,
                "Tampered pretrained metadata received a trusted hash: " +
                    tamperedCase.first);
        }
        if (readBytes(tamperedMetadataPath) != metadataBytesBeforeRepair) {
            return fail(
                86,
                "Rejected pretrained metadata repair changed file bytes: " +
                    tamperedCase.first);
        }
    }
    qunsetenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE");

    QString activeDisplayName;
    auto persisted = service.preparePersistedActive(
        "cpu", nullptr, &error, &activeDisplayName);
    if (!persisted || activeDisplayName != kInitialId)
        return fail(7, "Persisted active model preparation failed: " + error);
    service.installPersisted(std::move(persisted), pipeline);
    if (pipeline.loadedModelId() != kInitialId || pipeline.executionProvider().empty() ||
        pipeline.classLabels().size() != 3 || pipeline.isReady()) {
        return fail(8, "Persisted startup did not produce Model Ready without Pipeline Ready.");
    }
    if (pipeline.loadedModelPath() != modelPath.toStdString() ||
        pipeline.loadedMetadataPath() != metadataPath.toStdString() ||
        pipeline.loadedModelSha256() != validArtifact.value("onnx_sha256").toString().toStdString() ||
        pipeline.loadedMetadataSha256() != sha256File(metadataPath).toStdString()) {
        return fail(52, "Installed-model provenance does not match the validated package identity.");
    }

    QString failure;
    if (!expectPrepareFailure(service, "unknown", registryPath, pipeline, &failure))
        return fail(9, "Unknown-ID case: " + failure);

    QJsonObject metadata = validMetadata;
    QJsonObject artifact = metadata.value("artifact").toObject();
    artifact["onnx_sha256"] = QString(64, '0');
    metadata["artifact"] = artifact;
    if (!writeJsonObject(metadataPath, metadata))
        return fail(10, "Could not create mismatched ONNX-hash fixture.");
    const auto mismatchedHashInspection = service.inspectPersistedActive();
    if (mismatchedHashInspection.loadable ||
        !mismatchedHashInspection.modelSha256.isEmpty() ||
        mismatchedHashInspection.error.isEmpty() ||
        !mismatchedHashInspection.error.contains("SHA-256") ||
        !mismatchedHashInspection.error.contains("does not match")) {
        return fail(
            77,
            "Persisted Active Model inspection accepted or obscured a declared ONNX-hash mismatch.");
    }
    if (!expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(10, "ONNX-hash case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(11, "Could not restore metadata after ONNX-hash case.");

    metadata = validMetadata;
    artifact = metadata.value("artifact").toObject();
    artifact["external_data_files"] =
        QJsonArray{QJsonObject{{"filename", "missing.onnx.data"},
                               {"sha256", QString(64, '0')},
                               {"required", true}}};
    metadata["artifact"] = artifact;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(12, "Missing-sidecar case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(13, "Could not restore metadata after missing-sidecar case.");

    const QString sidecarPath = QDir(packagePath).filePath("declared.onnx.data");
    if (!writeBytes(sidecarPath, "corrupt sidecar"))
        return fail(14, "Could not create corrupt sidecar fixture.");
    metadata = validMetadata;
    artifact = metadata.value("artifact").toObject();
    artifact["external_data_files"] =
        QJsonArray{QJsonObject{{"filename", "declared.onnx.data"},
                               {"sha256", QString(64, '0')},
                               {"required", true}}};
    metadata["artifact"] = artifact;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(15, "Corrupt-sidecar case: " + failure);
    }
    if (!QFile::remove(sidecarPath) || !writeBytes(metadataPath, validMetadataBytes))
        return fail(16, "Could not restore package after corrupt-sidecar case.");

    metadata = validMetadata;
    QJsonObject normalization = metadata.value("normalization").toObject();
    normalization["std"] = QJsonArray{0.0, 0.0, 0.0};
    metadata["normalization"] = normalization;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(17, "Invalid-normalization case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(18, "Could not restore metadata after normalization case.");

    metadata = validMetadata;
    metadata["classes"] = QJsonArray{"0", "0", "2"};
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(19, "Invalid-classes case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(20, "Could not restore metadata after invalid-classes case.");

    metadata = validMetadata;
    metadata["classes"] = QJsonArray{"0", "1"};
    QJsonObject architecture = metadata.value("architecture").toObject();
    architecture["num_classes"] = 2;
    metadata["architecture"] = architecture;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(21, "Smoke-output-count case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(22, "Could not restore metadata after smoke-output-count case.");

    if (!writeBytes(modelPath, "not an ONNX model"))
        return fail(23, "Could not create invalid-ONNX fixture.");
    metadata = validMetadata;
    artifact = metadata.value("artifact").toObject();
    artifact["onnx_sha256"] = sha256File(modelPath);
    metadata["artifact"] = artifact;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(24, "Invalid-ONNX-session case: " + failure);
    }
    if (!writeBytes(modelPath, validModelBytes) || !writeBytes(metadataPath, validMetadataBytes))
        return fail(25, "Could not restore package after invalid-ONNX case.");

    metadata = validMetadata;
    QJsonObject sortingPolicy = metadata.value("sorting_policy").toObject();
    sortingPolicy["target_class_id"] = "missing";
    metadata["sorting_policy"] = sortingPolicy;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(43, "Invalid sorting-policy target case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(44, "Could not restore metadata after invalid sorting-policy target case.");

    metadata = validMetadata;
    sortingPolicy = metadata.value("sorting_policy").toObject();
    sortingPolicy["target_display_label"] = "Wrong";
    metadata["sorting_policy"] = sortingPolicy;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(45, "Invalid sorting-policy display case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(46, "Could not restore metadata after invalid sorting-policy display case.");

    metadata = validMetadata;
    sortingPolicy = metadata.value("sorting_policy").toObject();
    sortingPolicy["trigger_rule"] = "unsupported_rule";
    metadata["sorting_policy"] = sortingPolicy;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
        return fail(47, "Invalid sorting-policy trigger case: " + failure);
    }
    if (!writeBytes(metadataPath, validMetadataBytes))
        return fail(48, "Could not restore metadata after invalid sorting-policy trigger case.");

    PipelineRunner emptyPipeline;
    PipelineConfig liveConfig;
    liveConfig.targetClassId = "2";
    liveConfig.targetLabel = "MoreThanOne";
    liveConfig.sortNonTarget = true;
    liveConfig.onnxPath = "selected-row-model.onnx";
    liveConfig.metadataPath = "selected-row-metadata.json";
    liveConfig.outputDir = temp.path().toStdString();
    liveConfig.daq = DaqConfig{};
    std::string pipelineError;
    if (emptyPipeline.configureInstalled(liveConfig, pipelineError) ||
        emptyPipeline.isReady() || !emptyPipeline.loadedModelId().empty()) {
        return fail(49, "configureInstalled accepted a runner without an installed model.");
    }

    auto readyBlockedCandidate = service.prepare(kCandidateId, "cpu", nullptr, &error);
    if (!readyBlockedCandidate)
        return fail(26, "Ready-pipeline candidate preparation failed: " + error);
    const std::string modelBeforeConfigure = pipeline.loadedModelId();
    const std::string modelPathBeforeConfigure = pipeline.loadedModelPath();
    const std::string metadataPathBeforeConfigure = pipeline.loadedMetadataPath();
    const std::string modelHashBeforeConfigure = pipeline.loadedModelSha256();
    const std::string metadataHashBeforeConfigure = pipeline.loadedMetadataSha256();
    const std::string providerBeforeConfigure = pipeline.executionProvider();
    const std::vector<std::string> classesBeforeConfigure = pipeline.classLabels();
    if (!pipeline.configureInstalled(liveConfig, pipelineError))
        return fail(27, "Installed-model pipeline configuration failed: " + QString::fromStdString(pipelineError));
    if (!pipeline.isReady() || pipeline.isTriggerReady() ||
        pipeline.targetClassId() != "2" || pipeline.targetDisplayLabel() != "MoreThanOne" ||
        pipeline.loadedModelId() != modelBeforeConfigure ||
        pipeline.loadedModelPath() != modelPathBeforeConfigure ||
        pipeline.loadedMetadataPath() != metadataPathBeforeConfigure ||
        pipeline.loadedModelSha256() != modelHashBeforeConfigure ||
        pipeline.loadedMetadataSha256() != metadataHashBeforeConfigure ||
        pipeline.executionProvider() != providerBeforeConfigure ||
        pipeline.classLabels() != classesBeforeConfigure) {
        return fail(50, "configureInstalled did not preserve the user target and installed-model identity.");
    }
    const QByteArray beforeReadyRejection = readBytes(registryPath);
    if (service.activateAndInstall(std::move(readyBlockedCandidate), pipeline, &error))
        return fail(28, "Ready pipeline unexpectedly accepted model activation.");
    if (readBytes(registryPath) != beforeReadyRejection ||
        pipeline.loadedModelId() != kInitialId || !pipeline.isReady()) {
        return fail(29, "Ready-pipeline rejection changed registry bytes or prior runtime.");
    }
    pipeline.clear();
    if (pipeline.isReady() || pipeline.isTriggerReady() ||
        pipeline.loadedModelId() != modelBeforeConfigure ||
        pipeline.loadedModelPath() != modelPathBeforeConfigure ||
        pipeline.loadedMetadataPath() != metadataPathBeforeConfigure ||
        pipeline.loadedModelSha256() != modelHashBeforeConfigure ||
        pipeline.loadedMetadataSha256() != metadataHashBeforeConfigure ||
        pipeline.executionProvider() != providerBeforeConfigure ||
        pipeline.classLabels() != classesBeforeConfigure) {
        return fail(51, "Pipeline clear did not release Pipeline Ready while preserving Model Ready.");
    }

    auto changedPathCandidate = service.prepare(kCandidateId, "cpu", nullptr, &error);
    if (!changedPathCandidate)
        return fail(30, "Changed-path candidate preparation failed: " + error);
    const QByteArray validRegistryBytes = readBytes(registryPath);
    QJsonObject changedRegistry = QJsonDocument::fromJson(validRegistryBytes).object();
    QJsonArray changedEntries = changedRegistry.value("entries").toArray();
    QJsonObject changedEntry = changedEntries.at(1).toObject();
    changedEntry["package_path"] = QDir(temp.path()).filePath("other-package");
    changedEntries[1] = changedEntry;
    changedRegistry["entries"] = changedEntries;
    if (!writeJsonObject(registryPath, changedRegistry))
        return fail(31, "Could not create changed-path registry fixture.");
    const QByteArray changedRegistryBytes = readBytes(registryPath);
    if (service.activateAndInstall(std::move(changedPathCandidate), pipeline, &error))
        return fail(32, "Commit-time package-path change unexpectedly activated.");
    if (readBytes(registryPath) != changedRegistryBytes || pipeline.loadedModelId() != kInitialId)
        return fail(33, "Commit-time path rejection changed registry bytes or prior runtime.");
    if (!writeBytes(registryPath, validRegistryBytes))
        return fail(34, "Could not restore registry after changed-path case.");

    auto blockedCandidate = service.prepare(kCandidateId, "cpu", nullptr, &error);
    if (!blockedCandidate)
        return fail(35, "Commit-failure candidate preparation failed: " + error);
#ifdef Q_OS_WIN
    const QByteArray beforeCommitFailure = readBytes(registryPath);
    {
        RegistryCommitBlocker blocker(registryPath);
        if (!blocker.valid())
            return fail(36, "Could not lock the registry for commit-failure coverage.");
        if (service.activateAndInstall(std::move(blockedCandidate), pipeline, &error))
            return fail(37, "Registry commit unexpectedly succeeded while replacement was blocked.");
        if (readBytes(registryPath) != beforeCommitFailure || pipeline.loadedModelId() != kInitialId)
            return fail(38, "Registry commit failure changed durable bytes or prior runtime.");
    }
#else
    Q_UNUSED(blockedCandidate);
#endif

    auto candidate = service.prepare(kCandidateId, "cpu", nullptr, &error);
    if (!candidate)
        return fail(39, "Valid candidate preparation failed: " + error);
    if (!service.activateAndInstall(std::move(candidate), pipeline, &error))
        return fail(40, "Valid candidate activation failed: " + error);
    if (pipeline.loadedModelId() != kCandidateId || pipeline.isReady())
        return fail(41, "Valid activation did not install the candidate as Model Ready only.");
    if (!hasSingleActiveEntry(registryPath, kCandidateId))
        return fail(42, "Valid activation did not durably select exactly the candidate entry.");

    const QString completionRegistryDirectory = QDir(temp.path()).filePath("completion-registry");
    if (!QDir().mkpath(completionRegistryDirectory))
        return fail(53, "Could not create completion registry directory.");
    const QString completionRegistryPath =
        QDir(completionRegistryDirectory).filePath("model_registry.json");
    if (!writeJsonObject(completionRegistryPath, registryObject(packagePath)))
        return fail(54, "Could not create completion registry fixture.");

    desktop_app::v2::ModelLoadService completionService(completionRegistryPath);
    PipelineRunner completionPipeline;
    auto completionPersisted =
        completionService.preparePersistedActive("cpu", nullptr, &error);
    if (!completionPersisted)
        return fail(55, "Could not prepare completion prior active model: " + error);
    completionService.installPersisted(std::move(completionPersisted), completionPipeline);

    const QString trainingRun = QDir(temp.path()).filePath("training-run");
    if (!preparePackageFixture(trainingRun, &fixtureError)
        || !writeBytes(QDir(trainingRun).filePath("checkpoint.pth"), "checkpoint")) {
        return fail(56, "Could not create completed Training run fixture: " + fixtureError);
    }
    const QString trainingModel = QDir(trainingRun).filePath("model.onnx");
    const QString trainingMetadata = QDir(trainingRun).filePath("metadata.json");
    const QString destinationRoot = QDir(temp.path()).filePath("saved-models");
    QString registeredEntryId;
    QString warning;
    if (!completionService.saveAndActivateTrainedModel(
            trainingRun, trainingModel, trainingMetadata, "Saved Success", destinationRoot,
            "cpu", completionPipeline, &registeredEntryId, &warning, &error)) {
        return fail(57, "Explicit-root save and activation failed: " + error);
    }
    const QString successPackage = QDir(destinationRoot).filePath("Saved Success");
    if (registeredEntryId.isEmpty() || !QFileInfo(successPackage).isDir()
        || !QFileInfo(QDir(successPackage).filePath("checkpoint.pth")).isFile()
        || completionPipeline.loadedModelId() != registeredEntryId.toStdString()
        || !hasSingleActiveEntry(completionRegistryPath, registeredEntryId)) {
        return fail(58, "Explicit-root completion did not install and activate the saved package.");
    }

    const QString badRun = QDir(temp.path()).filePath("bad-training-run");
    if (!preparePackageFixture(badRun, &fixtureError)
        || !writeBytes(QDir(badRun).filePath("checkpoint.pth"), "checkpoint")
        || !writeBytes(QDir(badRun).filePath("model.onnx"), "not an ONNX model")) {
        return fail(59, "Could not create prepare-failure Training run fixture.");
    }
    const QByteArray beforePrepareFailure = readBytes(completionRegistryPath);
    const std::string modelBeforePrepareFailure = completionPipeline.loadedModelId();
    if (completionService.saveAndActivateTrainedModel(
            badRun, QDir(badRun).filePath("model.onnx"),
            QDir(badRun).filePath("metadata.json"), "Retry Model", destinationRoot, "cpu",
            completionPipeline, &registeredEntryId, &warning, &error)) {
        return fail(60, "Invalid saved model unexpectedly prepared and activated.");
    }
    const QString retryPackage = QDir(destinationRoot).filePath("Retry Model");
    if (error.isEmpty() || readBytes(completionRegistryPath) != beforePrepareFailure
        || QFileInfo::exists(retryPackage)
        || completionPipeline.loadedModelId() != modelBeforePrepareFailure
        || !hasSingleActiveEntry(completionRegistryPath, QString::fromStdString(modelBeforePrepareFailure))) {
        return fail(61, "Prepare failure did not preserve registry, Active Model, and pipeline.");
    }
    if (!completionService.saveAndActivateTrainedModel(
            trainingRun, trainingModel, trainingMetadata, "Retry Model", destinationRoot, "cpu",
            completionPipeline, &registeredEntryId, &warning, &error)
        || !QFileInfo(retryPackage).isDir()
        || completionPipeline.loadedModelId() != registeredEntryId.toStdString()
        || !hasSingleActiveEntry(completionRegistryPath, registeredEntryId)) {
        return fail(62, "Prepare-failure rollback did not allow a same-name retry: " + error);
    }

    const QString missingCheckpointRun =
        QDir(temp.path()).filePath("missing-checkpoint-run");
    if (!preparePackageFixture(missingCheckpointRun, &fixtureError))
        return fail(63, "Could not create missing-checkpoint fixture.");
    const QByteArray beforeMissingCheckpoint = readBytes(completionRegistryPath);
    const std::string modelBeforeMissingCheckpoint = completionPipeline.loadedModelId();
    const QString missingCheckpointPackage =
        QDir(destinationRoot).filePath("Missing Checkpoint");
    if (completionService.saveAndActivateTrainedModel(
            missingCheckpointRun, QDir(missingCheckpointRun).filePath("model.onnx"),
            QDir(missingCheckpointRun).filePath("metadata.json"), "Missing Checkpoint",
            destinationRoot, "cpu", completionPipeline, &registeredEntryId, &warning, &error)
        || readBytes(completionRegistryPath) != beforeMissingCheckpoint
        || QFileInfo::exists(missingCheckpointPackage)
        || completionPipeline.loadedModelId() != modelBeforeMissingCheckpoint) {
        return fail(64, "Missing checkpoint changed package, registry, or pipeline state.");
    }

    if (!completionPipeline.configureInstalled(liveConfig, pipelineError))
        return fail(65, "Could not make completion pipeline Ready for rejection coverage.");
    const QByteArray beforeReadyCompletion = readBytes(completionRegistryPath);
    const std::string modelBeforeReadyCompletion = completionPipeline.loadedModelId();
    const QString readyPackage = QDir(destinationRoot).filePath("Ready Blocked");
    if (completionService.saveAndActivateTrainedModel(
            trainingRun, trainingModel, trainingMetadata, "Ready Blocked", destinationRoot,
            "cpu", completionPipeline, &registeredEntryId, &warning, &error)
        || !error.contains("Stop the active pipeline")
        || readBytes(completionRegistryPath) != beforeReadyCompletion
        || QFileInfo::exists(readyPackage) || !completionPipeline.isReady()
        || completionPipeline.loadedModelId() != modelBeforeReadyCompletion) {
        return fail(66, "Pipeline Ready rejection did not roll back the saved package and registry.");
    }
    completionPipeline.clear();

    const QString collisionPackage = QDir(destinationRoot).filePath("Collision Model");
    if (!QDir().mkpath(collisionPackage)
        || !writeBytes(QDir(collisionPackage).filePath("keep.txt"), "existing")) {
        return fail(67, "Could not create destination collision fixture.");
    }
    const QByteArray beforeCollision = readBytes(completionRegistryPath);
    const std::string modelBeforeCollision = completionPipeline.loadedModelId();
    if (completionService.saveAndActivateTrainedModel(
            trainingRun, trainingModel, trainingMetadata, "Collision Model", destinationRoot,
            "cpu", completionPipeline, &registeredEntryId, &warning, &error)
        || !error.contains("already exists") || readBytes(completionRegistryPath) != beforeCollision
        || readBytes(QDir(collisionPackage).filePath("keep.txt")) != QByteArray("existing")
        || completionPipeline.loadedModelId() != modelBeforeCollision) {
        return fail(68, "Destination collision did not remain a normal non-mutating save failure.");
    }

    const QString absentFailureRegistry =
        QDir(temp.path()).filePath("absent-failure-registry/model_registry.json");
    const QString absentFailureRoot = QDir(temp.path()).filePath("absent-failure-models");
    desktop_app::v2::ModelLoadService absentFailureService(absentFailureRegistry);
    PipelineRunner absentFailurePipeline;
    const QString priorCompletionId =
        QString::fromStdString(completionPipeline.loadedModelId());
    auto absentFailurePrior =
        completionService.prepare(priorCompletionId, "cpu", nullptr, &error);
    if (!absentFailurePrior)
        return fail(69, "Could not prepare prior pipeline state for absent-registry rollback.");
    completionService.installPersisted(std::move(absentFailurePrior), absentFailurePipeline);
    const std::string absentFailurePriorId = absentFailurePipeline.loadedModelId();
    const QString absentFailurePackage =
        QDir(absentFailureRoot).filePath("Absent Failure");
    if (absentFailureService.saveAndActivateTrainedModel(
            badRun, QDir(badRun).filePath("model.onnx"),
            QDir(badRun).filePath("metadata.json"), "Absent Failure", absentFailureRoot,
            "cpu", absentFailurePipeline, &registeredEntryId, &warning, &error)
        || QFileInfo::exists(absentFailureRegistry)
        || QFileInfo::exists(absentFailurePackage)
        || absentFailurePipeline.loadedModelId() != absentFailurePriorId) {
        return fail(70, "Absent-registry failure did not remove its new registry and package.");
    }

    const QString absentSuccessRegistry =
        QDir(temp.path()).filePath("absent-success-registry/model_registry.json");
    const QString absentSuccessRoot = QDir(temp.path()).filePath("absent-success-models");
    desktop_app::v2::ModelLoadService absentSuccessService(absentSuccessRegistry);
    PipelineRunner absentSuccessPipeline;
    if (!absentSuccessService.saveAndActivateTrainedModel(
            trainingRun, trainingModel, trainingMetadata, "Absent Success", absentSuccessRoot,
            "cpu", absentSuccessPipeline, &registeredEntryId, &warning, &error)) {
        return fail(71, "Absent-registry completion failed: " + error);
    }
    const QString absentSuccessPackage =
        QDir(absentSuccessRoot).filePath("Absent Success");
    if (!QFileInfo(absentSuccessRegistry).isFile()
        || !QFileInfo(absentSuccessPackage).isDir()
        || registeredEntryId.isEmpty()
        || absentSuccessPipeline.loadedModelId() != registeredEntryId.toStdString()
        || !hasSingleActiveEntry(absentSuccessRegistry, registeredEntryId)) {
        return fail(72, "Absent-registry completion did not create and activate its saved package.");
    }

    return 0;
}
