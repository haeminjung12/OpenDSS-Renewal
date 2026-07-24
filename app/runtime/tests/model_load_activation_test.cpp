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

    auto persisted = service.preparePersistedActive("cpu", nullptr, &error);
    if (!persisted)
        return fail(7, "Persisted active model preparation failed: " + error);
    service.installPersisted(std::move(persisted), pipeline);
    if (pipeline.loadedModelId() != kInitialId || pipeline.executionProvider().empty() ||
        pipeline.classLabels().size() != 3 || pipeline.isReady()) {
        return fail(8, "Persisted startup did not produce Model Ready without Pipeline Ready.");
    }

    QString failure;
    if (!expectPrepareFailure(service, "unknown", registryPath, pipeline, &failure))
        return fail(9, "Unknown-ID case: " + failure);

    QJsonObject metadata = validMetadata;
    QJsonObject artifact = metadata.value("artifact").toObject();
    artifact["onnx_sha256"] = QString(64, '0');
    metadata["artifact"] = artifact;
    if (!writeJsonObject(metadataPath, metadata) ||
        !expectPrepareFailure(service, kCandidateId, registryPath, pipeline, &failure)) {
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

    auto readyBlockedCandidate = service.prepare(kCandidateId, "cpu", nullptr, &error);
    if (!readyBlockedCandidate)
        return fail(26, "Ready-pipeline candidate preparation failed: " + error);
    PipelineConfig detectorOnlyConfig;
    detectorOnlyConfig.detectorOnly = true;
    std::string pipelineError;
    if (!pipeline.init(detectorOnlyConfig, pipelineError))
        return fail(27, "Could not make the pipeline Ready for activation guard coverage.");
    const QByteArray beforeReadyRejection = readBytes(registryPath);
    if (service.activateAndInstall(std::move(readyBlockedCandidate), pipeline, &error))
        return fail(28, "Ready pipeline unexpectedly accepted model activation.");
    if (readBytes(registryPath) != beforeReadyRejection ||
        pipeline.loadedModelId() != kInitialId || !pipeline.isReady()) {
        return fail(29, "Ready-pipeline rejection changed registry bytes or prior runtime.");
    }
    pipeline.clear();

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

    return 0;
}
