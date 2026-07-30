#include "../v2/model/model_load_service.h"
#include "../desktop_app/model_registry_service.h"
#include "../detection/droplet_detector.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence_test/sequence_test_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <opencv2/core.hpp>

namespace {

class ProbeDetector final : public IDropletDetector {
public:
    void reset() override { frameIndex_ = 0; }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        DropletDetectionFrame result;
        ++frameIndex_;
        if (frameIndex_ == 101) {
            result.detected = true;
            result.eventEntered = true;
            result.bbox = {8, 8, 48, 48};
            result.centroid = {32.0f, 24.0f};
        }
        return result;
    }

private:
    int frameIndex_ = 0;
};

QByteArray fileBytes(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream failure(stderr);

    if (application.arguments().size() < 2 ||
        application.arguments().size() > 3) {
        failure << "Usage: sequence_test_production_model_probe "
                   "<model_registry.json> [trusted_pretrained_templates_root]\n";
        return 2;
    }

    const QString registryPath = application.arguments().at(1);
    if (application.arguments().size() == 3) {
        const QString installedRoot =
            QDir(QFileInfo(registryPath).absolutePath())
                .filePath("packages/pretrained");
        const QString trustedRoot = application.arguments().at(2);
        for (const QString& architecture :
             {QStringLiteral("mobilenet_v3_small"),
              QStringLiteral("efficientnet_b0")}) {
            QString repairError;
            if (!repairTrustedPretrainedMetadataHash(
                    QDir(installedRoot).filePath(architecture),
                    QDir(trustedRoot).filePath(architecture),
                    &repairError)) {
                failure << "Repair failed for " << architecture << ": "
                        << repairError << '\n';
                return 7;
            }
        }
    }

    desktop_app::v2::ModelLoadService loader(registryPath);
    const auto inspection = loader.inspectPersistedActive();
    if (!inspection.loadable) {
        failure << "Inspection failed: " << inspection.error << '\n';
        return 3;
    }

    QString warning;
    QString error;
    auto model = loader.preparePersistedActive(QStringLiteral("auto"), &warning, &error);
    if (!model) {
        failure << "Preparation failed: " << error << '\n';
        return 4;
    }

    cv::Mat crop(64, 64, CV_8UC1);
    for (int y = 0; y < crop.rows; ++y) {
        for (int x = 0; x < crop.cols; ++x)
            crop.at<uchar>(y, x) = static_cast<uchar>((x + y) % 256);
    }

    try {
        const ClassificationResult result = model->classify(crop);
        if (result.scores.size() != inspection.classes.size()) {
            failure << "Inference returned " << result.scores.size()
                    << " scores for " << inspection.classes.size() << " classes.\n";
            return 5;
        }
        output << "Prepared " << inspection.displayName << " on "
               << QString::fromStdString(model->executionProvider()) << "; scores:";
        for (const float score : result.scores)
            output << ' ' << score;
        output << '\n';
        if (!warning.isEmpty())
            output << "Warning: " << warning << '\n';
    } catch (const std::exception& exception) {
        failure << "Inference failed: " << exception.what() << '\n';
        return 6;
    }

    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString outputRoot = QDir(temporary.path()).filePath("runs");
    if (!temporary.isValid() || !QDir().mkpath(sequenceRoot) ||
        !QDir().mkpath(outputRoot)) {
        failure << "Could not create Sequence Test probe folders.\n";
        return 8;
    }

    constexpr qint64 FrameCount = 2103;
    desktop_app::v2::sequence::SequenceManifestData manifestData{
        QStringLiteral("production-model-probe"),
        QStringLiteral("Production Model Probe"),
        QString(),
        QString(),
        QStringLiteral("completed"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
        std::nullopt,
        QStringLiteral("end_of_sequence"),
        QStringLiteral("2"),
        FrameCount,
        QJsonObject{{QStringLiteral("probe"), true}},
        64,
        64,
        8,
        10000.0,
    };
    const QString sequenceJson = QDir(sequenceRoot).filePath("sequence.json");
    if (!desktop_app::v2::sequence::SequenceManifestV2::save(
            sequenceJson, manifestData, &error)) {
        failure << "Could not create Sequence Test probe manifest: " << error
                << '\n';
        return 9;
    }

    auto loaded =
        std::make_shared<desktop_app::v2::sequence_test::LoadedSequence>();
    loaded->sourceSequenceJson = QFileInfo(sequenceJson).canonicalFilePath();
    loaded->sequenceId = manifestData.sequenceId;
    loaded->frames.reserve(FrameCount);
    QImage frame(64, 64, QImage::Format_Grayscale8);
    frame.fill(96);
    for (qint64 index = 1; index <= FrameCount; ++index)
        loaded->frames.push_back({index, frame});

    ProbeDetector detector;
    desktop_app::v2::OperationCoordinator operations;
    desktop_app::v2::sequence_test::SequenceTestService service(
        operations, detector, &loader);
    desktop_app::v2::sequence_test::SequenceTestRequest request;
    request.sequenceJson = sequenceJson;
    request.frozenManifestBytes = fileBytes(sequenceJson);
    request.loadedSequence = std::move(loaded);
    request.outputRoot = outputRoot;
    request.runName = QStringLiteral("Production Model Probe");
    request.triggerMode = desktop_app::v2::run::TriggerMode::ClassBased;
    request.hitClassId = inspection.classes.constFirst().id;
    request.hitBoundary = {
        32.0, desktop_app::v2::run::HitSide::PositiveY, 64, 64};
    request.requestedProcessingFps = 10000.0;
    request.useActiveModel = true;
    request.opendssVersion = QStringLiteral("2");
    if (!service.run(request, &error)) {
        failure << "Production Sequence Test failed: " << error << '\n';
        return 10;
    }
    output << "Sequence Test processed " << FrameCount
           << " loaded frames with the persisted Active Model.\n";

    return 0;
}
