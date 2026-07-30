#include "../v2/sequence/sequence_manifest_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <limits>

namespace {

using desktop_app::v2::sequence::SequenceManifestData;
using desktop_app::v2::sequence::SequenceManifestV2;

int fail(int code, const QString& message) {
    std::cerr << message.toStdString() << '\n';
    return code;
}

SequenceManifestData validData() {
    SequenceManifestData data{
        "sequence-001",
        "Test sequence",
        "Characterization",
        "Round-trip fixture",
        "completed",
        "2026-07-24T10:15:00-05:00",
        "2026-07-24T10:15:01-05:00",
        "2026-07-24T10:15:11-05:00",
        10.0,
        "duration",
        "2.0.0",
        250,
        QJsonObject{{"exposure_us", 200}},
        2048,
        1024,
        16,
        25.0,
    };
    data.integrity.sourceFrameGaps = {3, {{10, 11}, {15, 15}}};
    data.integrity.queueRejections = {1, {{21, 21}}};
    return data;
}

bool writeObject(const QString& path, const QJsonObject& object) {
    QFile file(path);
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

bool expectRejected(const QString& path, const QJsonObject& object,
                    const QString& errorFragment) {
    if (!writeObject(path, object))
        return false;
    QString error;
    return !SequenceManifestV2::load(path, &error) &&
           error.contains(errorFragment, Qt::CaseInsensitive);
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc > 1) {
        for (int index = 1; index < argc; ++index) {
            QString error;
            const QString path = QString::fromLocal8Bit(argv[index]);
            const auto manifest = SequenceManifestV2::load(path, &error);
            if (!manifest)
                return fail(32, path + ": " + error);
            std::cout << path.toStdString()
                      << ": frames=" << manifest->data().frameCount
                      << " pattern="
                      << manifest->data().frameFilenamePattern.toStdString()
                      << " image=" << manifest->data().imageWidth << 'x'
                      << manifest->data().imageHeight << 'x'
                      << manifest->data().bitDepth << '\n';
        }
        return 0;
    }
    QTemporaryDir temp;
    if (!temp.isValid())
        return fail(1, "Could not create temporary Sequence folder.");

    const QString path = QDir(temp.path()).filePath("sequence.json");
    QString error;
    const SequenceManifestData input = validData();
    if (!SequenceManifestV2::save(path, input, &error))
        return fail(2, "Sequence save failed: " + error);

    auto loaded = SequenceManifestV2::load(path, &error);
    if (!loaded)
        return fail(3, "Sequence load failed: " + error);
    const SequenceManifestData& output = loaded->data();
    if (output.sequenceId != input.sequenceId || output.name != input.name ||
        output.experimentType != input.experimentType || output.notes != input.notes ||
        output.status != input.status || output.createdAt != input.createdAt ||
        output.startedAt != input.startedAt || output.endedAt != input.endedAt ||
        output.requestedDurationSeconds != input.requestedDurationSeconds ||
        output.stopReason != input.stopReason || output.opendssVersion != input.opendssVersion ||
        output.frameCount != input.frameCount || output.cameraSettings != input.cameraSettings ||
        output.imageWidth != input.imageWidth || output.imageHeight != input.imageHeight ||
        output.bitDepth != input.bitDepth || output.nominalFps != input.nominalFps ||
        !output.provenanceMode.isEmpty() ||
        output.frameFilenamePattern != QStringLiteral("frames/frame_%08d.tif") ||
        output.integrity.sourceFrameGaps.count != 3 ||
        output.integrity.sourceFrameGaps.ranges.size() != 2 ||
        output.integrity.sourceFrameGaps.ranges.at(1).first != 15 ||
        output.integrity.queueRejections.count != 1 ||
        output.integrity.consumerFailures.count != 0) {
        return fail(4, "Sequence save/load round trip changed data.");
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(5, "Could not read saved Sequence fixture.");
    const QByteArray validBytes = file.readAll();
    const QJsonObject root = QJsonDocument::fromJson(validBytes).object();
    file.close();
    if (root.value("schema_version").toString() != SequenceManifestV2::SchemaVersion ||
        root.value("frame_format").toString() != "tiff" ||
        root.value("frame_filename_pattern").toString() != "frames/frame_%08d.tif" ||
        !root.value("timing").toObject().value("timestamps_file").isNull()) {
        return fail(6, "Sequence save emitted the wrong fixed contract fields.");
    }

    SequenceManifestData invalid = input;
    invalid.nominalFps = 0.0;
    if (SequenceManifestV2::save(path, invalid, &error))
        return fail(7, "Invalid Sequence was saved.");
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != validBytes)
        return fail(8, "Failed Sequence save replaced the valid manifest.");
    file.close();

    SequenceManifestData unsupportedMode = input;
    unsupportedMode.provenanceMode = QStringLiteral("unsupported");
    if (SequenceManifestV2::save(path, unsupportedMode, &error))
        return fail(35, "Unsupported in-memory provenance mode was saved.");
    if (!error.contains("provenance_mode", Qt::CaseInsensitive))
        return fail(36, "Unsupported provenance mode did not report its field.");
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != validBytes)
        return fail(37, "Unsupported provenance mode replaced the valid manifest.");
    file.close();

    QJsonObject unknown = root;
    unknown.insert("model_id", "not-allowed");
    if (!expectRejected(path, unknown, "Unknown field"))
        return fail(9, "Unknown Sequence field was accepted.");

    QJsonObject badTimestamp = root;
    badTimestamp.insert("ended_at", "not-a-timestamp");
    if (!expectRejected(path, badTimestamp, "ISO-8601"))
        return fail(10, "Invalid timestamp was accepted.");

    QJsonObject badDuration = root;
    badDuration.insert("requested_duration_seconds", 0);
    if (!expectRejected(path, badDuration, "positive"))
        return fail(11, "Nonpositive requested duration was accepted.");
    QJsonObject missingDuration = root;
    missingDuration.remove("requested_duration_seconds");
    if (!expectRejected(path, missingDuration, "missing"))
        return fail(38, "Native Sequence without requested_duration_seconds was accepted.");

    QJsonObject badCount = root;
    badCount.insert("frame_count", -1);
    if (!expectRejected(path, badCount, "nonnegative"))
        return fail(12, "Negative frame count was accepted.");

    QJsonObject badImage = root;
    QJsonObject image = badImage.value("image").toObject();
    image.insert("width", 0);
    badImage.insert("image", image);
    if (!expectRejected(path, badImage, "positive integers"))
        return fail(13, "Invalid image dimensions were accepted.");

    QJsonObject badTiming = root;
    QJsonObject timing = badTiming.value("timing").toObject();
    timing.insert("timestamps_file", "timestamps.csv");
    badTiming.insert("timing", timing);
    if (!expectRejected(path, badTiming, "must be null"))
        return fail(14, "Timestamp sidecar was accepted.");

    QJsonObject badFormat = root;
    badFormat.insert("frame_format", "png");
    if (!expectRejected(path, badFormat, "tiff"))
        return fail(15, "Non-TIFF frame format was accepted.");

    QJsonObject badIntegrity = root;
    QJsonObject integrity = badIntegrity.value("integrity").toObject();
    QJsonObject gaps = integrity.value("source_frame_gaps").toObject();
    gaps.insert("count", 2);
    integrity.insert("source_frame_gaps", gaps);
    badIntegrity.insert("integrity", integrity);
    if (!expectRejected(path, badIntegrity, "count must equal"))
        return fail(16, "Mismatched integrity count was accepted.");

    QJsonObject overlappingIntegrity = root;
    integrity = overlappingIntegrity.value("integrity").toObject();
    gaps = integrity.value("source_frame_gaps").toObject();
    gaps.insert("count", 4);
    gaps.insert("ranges", QJsonArray{
                              QJsonObject{{"first", 10}, {"last", 11}},
                              QJsonObject{{"first", 11}, {"last", 12}},
                          });
    integrity.insert("source_frame_gaps", gaps);
    overlappingIntegrity.insert("integrity", integrity);
    if (!expectRejected(path, overlappingIntegrity, "ordered and non-overlapping"))
        return fail(17, "Overlapping integrity ranges were accepted.");

    QJsonObject missingCategory = root;
    integrity = missingCategory.value("integrity").toObject();
    integrity.remove("consumer_failures");
    missingCategory.insert("integrity", integrity);
    if (!expectRejected(path, missingCategory, "consumer_failures"))
        return fail(18, "Missing integrity category was accepted.");

    SequenceManifestData legacy = input;
    legacy.provenanceMode = QStringLiteral("legacy_tiff_sequence");
    legacy.frameFilenamePattern = QStringLiteral("%06d.tiff");
    legacy.createdAt.clear();
    legacy.startedAt.clear();
    legacy.endedAt.clear();
    legacy.requestedDurationSeconds.reset();
    legacy.stopReason.clear();
    legacy.opendssVersion.clear();
    legacy.cameraSettings = {};
    legacy.nominalFps = 0.0;
    legacy.integrity = {};
    if (!SequenceManifestV2::save(path, legacy, &error))
        return fail(27, "Legacy TIFF Sequence save failed: " + error);
    loaded = SequenceManifestV2::load(path, &error);
    if (!loaded ||
        loaded->data().provenanceMode != QStringLiteral("legacy_tiff_sequence") ||
        loaded->data().frameFilenamePattern != QStringLiteral("%06d.tiff") ||
        !loaded->data().createdAt.isEmpty() ||
        !loaded->data().cameraSettings.isEmpty() ||
        loaded->data().nominalFps != 0.0) {
        return fail(28, "Legacy TIFF Sequence facts did not round trip.");
    }
    QFile legacyFile(path);
    if (!legacyFile.open(QIODevice::ReadOnly))
        return fail(29, "Could not read saved legacy TIFF Sequence.");
    const QJsonObject legacyRoot =
        QJsonDocument::fromJson(legacyFile.readAll()).object();
    legacyFile.close();
    if (!legacyRoot.value("created_at").isNull() ||
        !legacyRoot.value("camera_settings").isNull() ||
        !legacyRoot.value("timing").toObject().value("nominal_fps").isNull() ||
        !legacyRoot.value("integrity").isNull()) {
        return fail(33, "Legacy TIFF Sequence emitted invented acquisition provenance.");
    }
    QJsonObject unsafeLegacy = legacyRoot;
    unsafeLegacy["frame_filename_pattern"] = QStringLiteral("../%06d.tiff");
    if (!expectRejected(path, unsafeLegacy, "frame_filename_pattern"))
        return fail(30, "Unsafe legacy TIFF frame pattern was accepted.");
    QJsonObject claimedLegacyIntegrity = legacyRoot;
    claimedLegacyIntegrity["integrity"] = root.value("integrity");
    if (!expectRejected(path, claimedLegacyIntegrity, "integrity must be null"))
        return fail(34, "Claimed legacy TIFF integrity was accepted.");
    QJsonObject nativeWithLegacyPattern = root;
    nativeWithLegacyPattern["frame_filename_pattern"] = QStringLiteral("%06d.tiff");
    if (!expectRejected(path, nativeWithLegacyPattern, "frame_filename_pattern"))
        return fail(31, "Legacy TIFF pattern was accepted without its provenance mode.");

    SequenceManifestData unlimited = input;
    unlimited.requestedDurationSeconds.reset();
    if (!SequenceManifestV2::save(path, unlimited, &error))
        return fail(19, "Null requested duration was rejected: " + error);
    loaded = SequenceManifestV2::load(path, &error);
    if (!loaded || loaded->data().requestedDurationSeconds)
        return fail(20, "Null requested duration did not round trip.");

    SequenceManifestData largeInteger = input;
    largeInteger.frameCount = 9007199254740993LL;
    largeInteger.integrity.sourceFrameGaps = {
        1, {{(std::numeric_limits<qint64>::max)(), (std::numeric_limits<qint64>::max)()}}};
    largeInteger.integrity.queueRejections = {};
    if (!SequenceManifestV2::save(path, largeInteger, &error))
        return fail(21, "Exact 64-bit integer save failed: " + error);
    loaded = SequenceManifestV2::load(path, &error);
    if (!loaded || loaded->data().frameCount != 9007199254740993LL ||
        loaded->data().integrity.sourceFrameGaps.ranges.front().first !=
            (std::numeric_limits<qint64>::max)()) {
        return fail(22, "64-bit integers above 2^53 did not round trip exactly.");
    }

    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(23, "Could not read 64-bit Sequence fixture.");
    QByteArray outOfRangeBytes = file.readAll();
    file.close();
    const QByteArray exactFrameCount = "\"frame_count\": 9007199254740993";
    if (!outOfRangeBytes.contains(exactFrameCount))
        return fail(24, "Saved Sequence did not retain the exact frame_count literal.");
    outOfRangeBytes.replace(exactFrameCount, "\"frame_count\": 9223372036854775808");
    if (!writeBytes(path, outOfRangeBytes))
        return fail(25, "Could not write out-of-range integer fixture.");
    loaded = SequenceManifestV2::load(path, &error);
    if (loaded || !error.contains("nonnegative integer", Qt::CaseInsensitive))
        return fail(26, "JSON integer 2^63 was accepted.");

    return 0;
}
