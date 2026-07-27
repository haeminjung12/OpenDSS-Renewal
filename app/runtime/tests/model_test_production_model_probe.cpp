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
constexpr qint64 ExpectedTotal = 3625;
constexpr qint64 ExpectedEligible = 3620;

const std::array<QString, 3> ExpectedClassIds{
    QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2")};
const std::array<QString, 3> ExpectedClassNames{
    QStringLiteral("Empty"), QStringLiteral("Single"),
    QStringLiteral("MoreThanOne")};
constexpr std::array<qint64, 3> ExpectedAuditSupports{3142, 387, 91};
constexpr std::array<qint64, 3> ExpectedProductionSupports{3142, 386, 92};

QString normalizedAbsolutePath(const QString& path) {
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

QString canonicalPath(const QString& path) {
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).canonicalFilePath()));
}

bool samePath(const QString& left, const QString& right) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    const QString leftCanonical = canonicalPath(left);
    const QString rightCanonical = canonicalPath(right);
    return !leftCanonical.isEmpty() && !rightCanonical.isEmpty() &&
           leftCanonical.compare(rightCanonical, sensitivity) == 0;
}

bool localAbsolutePath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path);
    return QFileInfo(path).isAbsolute() && !normalized.startsWith("//") &&
           !normalized.contains("://");
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

bool noPartialArtifacts(const QString& outputRoot, QString* offendingPath) {
    QDirIterator iterator(outputRoot, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).fileName().contains(".partial")) {
            *offendingPath = path;
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream failure(stderr);
    const QString usage =
        QStringLiteral("Usage: model_test_production_model_probe "
                       "--dataset-manifest <path> --model-registry <path> "
                       "--output-root <fresh-root>");

    const QStringList arguments = application.arguments();
    if (arguments.size() != 7 ||
        arguments.at(1) != QStringLiteral("--dataset-manifest") ||
        arguments.at(3) != QStringLiteral("--model-registry") ||
        arguments.at(5) != QStringLiteral("--output-root") ||
        arguments.at(2).trimmed().isEmpty() ||
        arguments.at(4).trimmed().isEmpty() ||
        arguments.at(6).trimmed().isEmpty()) {
        failure << usage << '\n';
        return 2;
    }

    const QString auditManifestArgument = arguments.at(2).trimmed();
    const QString registryArgument = arguments.at(4).trimmed();
    const QString outputArgument = arguments.at(6).trimmed();
    if (!localAbsolutePath(auditManifestArgument) ||
        !localAbsolutePath(registryArgument) ||
        !localAbsolutePath(outputArgument)) {
        failure << "FAIL: All inputs must be absolute local paths.\n";
        return 3;
    }
    const QString auditManifestPath =
        normalizedAbsolutePath(auditManifestArgument);
    const QString registryPath = normalizedAbsolutePath(registryArgument);
    const QString outputRoot = normalizedAbsolutePath(outputArgument);
    output << "output_root=" << outputRoot << Qt::endl;
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
    if (QFileInfo::exists(outputRoot)) {
        return fail(6, QStringLiteral("Output root already exists; refusing to replace it."));
    }
    const QFileInfo outputInfo(outputRoot);
    const QFileInfo outputParent(outputInfo.absolutePath());
    if (!outputParent.isDir() || !outputParent.isWritable() ||
        outputParent.fileName().compare(QStringLiteral("reports"),
                                        Qt::CaseInsensitive) != 0 ||
        !QRegularExpression(QStringLiteral("^model-test-[1-9][0-9]*$"))
             .match(outputInfo.fileName())
             .hasMatch()) {
        return fail(
            7,
            QStringLiteral(
                "Output root must be a fresh reports/model-test-N directory."));
    }

    const QString auditManifestHash = fileSha256(auditManifestPath);
    if (auditManifestHash != QLatin1String(ExpectedAuditManifestSha256)) {
        return fail(8, QStringLiteral("Dataset audit manifest SHA-256 mismatch: %1")
                           .arg(auditManifestHash));
    }

    QDir datasetRoot(QFileInfo(auditManifestPath).absolutePath());
    if (!datasetRoot.cdUp()) {
        return fail(9, QStringLiteral("Could not derive the Dataset root."));
    }
    const QString datasetJsonPath = datasetRoot.filePath("dataset.json");
    if (!QFileInfo(datasetJsonPath).isFile() ||
        QFileInfo(datasetJsonPath).isSymLink()) {
        return fail(10, QStringLiteral("Derived production dataset.json is unavailable."));
    }
    const QString datasetJsonHash = fileSha256(datasetJsonPath);
    if (datasetJsonHash != QLatin1String(ExpectedDatasetJsonSha256)) {
        return fail(11, QStringLiteral("Production dataset.json SHA-256 mismatch: %1")
                            .arg(datasetJsonHash));
    }

    QString error;
    const auto audit = readObject(auditManifestPath, &error);
    if (!audit) {
        return fail(12, error);
    }
    const QJsonArray auditItems = audit->value("items").toArray();
    if (audit->value("schema_version").toString() !=
            QStringLiteral("dataset-manifest-v1") ||
        audit->value("dataset_id").toString() !=
            QLatin1String(ExpectedDatasetId) ||
        audit->value("items_total").toInteger(-1) != ExpectedTotal ||
        audit->value("items_included").toInteger(-1) != ExpectedEligible ||
        audit->value("items_excluded").toInteger(-1) !=
            ExpectedTotal - ExpectedEligible ||
        auditItems.size() != ExpectedTotal ||
        !expectedClasses(audit->value("classes").toArray(),
                         QStringLiteral("display_name"))) {
        return fail(13, QStringLiteral("Dataset audit manifest facts mismatch."));
    }
    std::array<qint64, 3> auditSupports{};
    qint64 auditEligible = 0;
    for (const QJsonValue& value : auditItems) {
        const QJsonObject item = value.toObject();
        if (item.value("status").toString() != QStringLiteral("included") ||
            !item.value("trainer_eligible").toBool()) {
            continue;
        }
        const int classIndex =
            ExpectedClassIds[0] == item.value("label").toString()
                ? 0
                : ExpectedClassIds[1] == item.value("label").toString()
                      ? 1
                      : ExpectedClassIds[2] == item.value("label").toString()
                            ? 2
                            : -1;
        if (classIndex < 0) {
            return fail(14, QStringLiteral("Eligible audit item has an unknown class."));
        }
        ++auditEligible;
        ++auditSupports[classIndex];
    }
    if (auditEligible != ExpectedEligible ||
        auditSupports != ExpectedAuditSupports) {
        return fail(15, QStringLiteral("Dataset audit support counts mismatch."));
    }

    auto dataset = DatasetManifestV2::load(datasetJsonPath, &error);
    if (!dataset) {
        return fail(16, error);
    }
    std::array<qint64, 3> productionSupports{};
    qint64 productionEligible = 0;
    for (const auto& label : dataset->labels()) {
        if (label.excluded)
            continue;
        const int classIndex =
            ExpectedClassIds[0] == label.classId
                ? 0
                : ExpectedClassIds[1] == label.classId
                      ? 1
                      : ExpectedClassIds[2] == label.classId ? 2 : -1;
        if (classIndex < 0) {
            return fail(17, QStringLiteral("Production Dataset has an unknown class."));
        }
        ++productionEligible;
        ++productionSupports[classIndex];
    }
    if (dataset->datasetId() != QLatin1String(ExpectedDatasetId) ||
        dataset->records().size() != ExpectedTotal ||
        productionEligible != ExpectedEligible ||
        !expectedClasses(dataset->classes())) {
        return fail(18, QStringLiteral("Production Dataset facts mismatch."));
    }
    if (productionSupports != ExpectedProductionSupports) {
        return fail(19, QStringLiteral("Production Dataset support counts mismatch."));
    }

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
        return fail(20, registryWarning.isEmpty()
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
        return fail(21, QStringLiteral("Active Model registry identity/path mismatch."));
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
        QFileInfo(package.onnxPath).isSymLink() ||
        QFileInfo(package.metadataPath).isSymLink()) {
        return fail(22, package.message.isEmpty()
                            ? QStringLiteral("Active Model package mismatch.")
                            : package.message);
    }
    const QString onnxHash = fileSha256(package.onnxPath);
    const QString metadataHash = fileSha256(package.metadataPath);
    if (onnxHash != QLatin1String(ExpectedOnnxSha256)) {
        return fail(23, QStringLiteral("Active Model ONNX SHA-256 mismatch: %1")
                            .arg(onnxHash));
    }
    const auto metadata = readObject(package.metadataPath, &error);
    if (!metadata ||
        metadata->value("model_id").toString() !=
            QLatin1String(ExpectedMetadataModelId) ||
        metadata->value("model_name").toString() !=
            QLatin1String(ExpectedModelName) ||
        metadata->value("classes").toArray() !=
            QJsonArray{ExpectedClassIds[0], ExpectedClassIds[1],
                       ExpectedClassIds[2]}) {
        return fail(24, metadata ? QStringLiteral("Active Model metadata identity mismatch.")
                                 : error);
    }

    ModelLoadService loader(registryPath);
    const PersistedActiveModelInspection inspection =
        loader.inspectPersistedActive();
    if (!inspection.loadable ||
        inspection.id != QLatin1String(ExpectedRegistryEntryId) ||
        inspection.displayName != QLatin1String(ExpectedModelName) ||
        inspection.modelSha256 != QLatin1String(ExpectedOnnxSha256) ||
        !expectedClasses(inspection.classes)) {
        return fail(25, inspection.error.isEmpty()
                            ? QStringLiteral("Active Model loader inspection mismatch.")
                            : inspection.error);
    }

    const QString registryHashBefore = fileSha256(registryPath);
    qint64 progressProcessed = -1;
    qint64 progressEligible = -1;
    OperationCoordinator operations;
    ModelTestService service(
        operations, &loader, {},
        [&](qint64 processed, qint64 eligible) {
            progressProcessed = processed;
            progressEligible = eligible;
        });
    QElapsedTimer elapsed;
    elapsed.start();
    const bool ran =
        service.run({datasetJsonPath, outputRoot, QStringLiteral("2")}, &error);
    const qint64 elapsedMs = elapsed.elapsed();
    output << "elapsed_ms=" << elapsedMs
           << " progress=" << progressProcessed << '/' << progressEligible
           << Qt::endl;

    if (fileSha256(auditManifestPath) != auditManifestHash ||
        fileSha256(datasetJsonPath) != datasetJsonHash ||
        fileSha256(registryPath) != registryHashBefore ||
        fileSha256(package.onnxPath) != onnxHash ||
        fileSha256(package.metadataPath) != metadataHash) {
        return fail(26, QStringLiteral("A supplied input changed during the probe."));
    }
    if (!ran) {
        return fail(27, QStringLiteral("Production Model Test failed: %1").arg(error));
    }

    const QString summaryPath =
        QDir(outputRoot).filePath("model_test_summary.json");
    const QString predictionsPath =
        QDir(outputRoot).filePath("predictions.csv");
    auto summary = ModelTestSummaryV2::load(summaryPath, &error);
    if (!summary) {
        return fail(28, QStringLiteral("Final summary/CSV coherence failed: %1")
                            .arg(error));
    }
    const auto& data = summary->data();
    const auto& derived = summary->derivedResults();
    qint64 confusionSum = 0;
    for (const auto& row : derived.confusionMatrix) {
        for (qint64 value : row)
            confusionSum += value;
    }
    if (data.status != ModelTestStatus::Completed ||
        data.stopReason != QStringLiteral("end_of_dataset") ||
        data.eligibleImages != ExpectedEligible ||
        derived.processedImages != ExpectedEligible ||
        summary->predictions().size() != ExpectedEligible ||
        confusionSum != ExpectedEligible ||
        data.dataset.id != QLatin1String(ExpectedDatasetId) ||
        !samePath(data.dataset.sourcePath, datasetRoot.absolutePath()) ||
        !expectedClasses(data.dataset.classes) ||
        data.activeModel.id != QLatin1String(ExpectedRegistryEntryId) ||
        data.activeModel.name != QLatin1String(ExpectedModelName) ||
        data.activeModel.onnxSha256 != QLatin1String(ExpectedOnnxSha256) ||
        data.activeModel.metadataSha256 != metadataHash ||
        !expectedClasses(data.activeModel.classes)) {
        return fail(29, QStringLiteral("Final Model Test summary facts mismatch."));
    }
    if (derived.perClass.size() !=
        static_cast<qsizetype>(ExpectedProductionSupports.size())) {
        return fail(30, QStringLiteral("Final per-class metric count mismatch."));
    }
    for (qsizetype index = 0; index < derived.perClass.size(); ++index) {
        if (derived.perClass.at(index).support !=
            ExpectedProductionSupports[index]) {
            return fail(31, QStringLiteral("Final per-class support mismatch."));
        }
    }

    QFile predictions(predictionsPath);
    if (!predictions.open(QIODevice::ReadOnly)) {
        return fail(32, QStringLiteral("Final predictions.csv is unreadable."));
    }
    const QByteArray predictionsBytes = predictions.readAll();
    if (predictionsBytes.count('\n') != ExpectedEligible + 1) {
        return fail(33, QStringLiteral("predictions.csv line count mismatch."));
    }
    QString partialPath;
    if (!noPartialArtifacts(outputRoot, &partialPath)) {
        return fail(34, QStringLiteral("Partial artifact remains: %1")
                            .arg(partialPath));
    }

    output << "PASS dataset_id=" << data.dataset.id
           << " audit_sha256=" << auditManifestHash.toUpper()
           << " total=" << ExpectedTotal
           << " eligible_processed=" << derived.processedImages
           << " audit_supports=3142,387,91"
           << " production_supports=3142,386,92"
           << " model_id=" << data.activeModel.id
           << " model_name=\"" << data.activeModel.name << '"'
           << " onnx_sha256=" << data.activeModel.onnxSha256.toUpper()
           << " confusion_sum=" << confusionSum
           << " predictions_lines=" << predictionsBytes.count('\n')
           << Qt::endl;
    return 0;
}
