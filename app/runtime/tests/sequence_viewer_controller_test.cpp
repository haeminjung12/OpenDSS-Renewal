#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence/sequence_viewer_controller.h"
#include "../v2/sequence/sequence_viewer_image_provider.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QQmlEngine>
#include <QTemporaryDir>

#include <iostream>

namespace {

using namespace desktop_app::v2::sequence;

int fail(int code, const QString& message) {
    std::cerr << message.toStdString() << '\n';
    return code;
}

SequenceManifestData manifestData(qint64 frameCount) {
    return {
        "sequence-001", "Controller fixture", "Characterization", "", "completed",
        "2026-07-25T10:15:00-05:00", "2026-07-25T10:15:01-05:00",
        "2026-07-25T10:15:11-05:00", 10.0, "duration", "2.0.0", frameCount,
        {}, 2, 2, 8, 25.0,
    };
}

QString framePath(const QString& root, qint64 frame) {
    return QDir(root).filePath(
        QStringLiteral("frames/frame_%1.tif").arg(frame, 8, 10, QLatin1Char('0')));
}

bool writeFrame(const QString& root, qint64 frame, QRgb color) {
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(color);
    return image.save(framePath(root, frame), "TIFF");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid() || !QDir().mkpath(QDir(temp.path()).filePath("frames")))
        return fail(1, "Could not create controller fixture.");

    QString manifestError;
    const QString manifestPath = QDir(temp.path()).filePath("sequence.json");
    if (!SequenceManifestV2::save(manifestPath, manifestData(3), &manifestError) ||
        !writeFrame(temp.path(), 1, qRgb(1, 2, 3)) ||
        !writeFrame(temp.path(), 3, qRgb(3, 4, 5))) {
        return fail(2, "Could not write controller fixture: " + manifestError);
    }

    SequenceViewerController controller;
    QQmlEngine engine;
    engine.addImageProvider("sequence-frame", new SequenceViewerImageProvider(controller));
    int presentationChanges = 0;
    int currentFrameChanges = 0;
    int totalFramesChanges = 0;
    int errorChanges = 0;
    int imageUrlChanges = 0;
    QObject::connect(&controller, &SequenceViewerController::presentationChanged,
                     [&presentationChanges] { ++presentationChanges; });
    QObject::connect(&controller, &SequenceViewerController::currentFrameChanged,
                     [&currentFrameChanges] { ++currentFrameChanges; });
    QObject::connect(&controller, &SequenceViewerController::totalFramesChanged,
                     [&totalFramesChanges] { ++totalFramesChanges; });
    QObject::connect(&controller, &SequenceViewerController::errorChanged,
                     [&errorChanges] { ++errorChanges; });
    QObject::connect(&controller, &SequenceViewerController::currentFrameImageUrlChanged,
                     [&imageUrlChanges] { ++imageUrlChanges; });

    if (controller.presentation() != "empty" || controller.currentFrame() != 0 ||
        controller.totalFrames() != 0 || !controller.error().isEmpty() ||
        !controller.currentFrameImageUrl().isEmpty() || imageUrlChanges != 0) {
        return fail(3, "Initial controller properties are not empty.");
    }

    if (!controller.open(manifestPath) || controller.presentation() != "ready" ||
        controller.currentFrame() != 1 || controller.totalFrames() != 3 ||
        !controller.error().isEmpty() || presentationChanges != 1 ||
        currentFrameChanges != 1 || totalFramesChanges != 1 || errorChanges != 0 ||
        imageUrlChanges != 1) {
        return fail(4, "Open did not publish factual properties.");
    }

    const QUrl firstFrameUrl = controller.currentFrameImageUrl();
    auto* provider = dynamic_cast<SequenceViewerImageProvider*>(engine.imageProvider("sequence-frame"));
    QSize imageSize;
    if (!provider || firstFrameUrl.isEmpty() ||
        provider->requestImage("current/1", &imageSize, {}).pixel(0, 0) != qRgb(1, 2, 3) ||
        imageSize != QSize(2, 2)) {
        return fail(9, "Image provider did not return the current frame.");
    }

    if (!controller.next() || controller.currentFrame() != 3 || currentFrameChanges != 2) {
        return fail(5, "Navigation did not skip missing frames or respect bounds.");
    }

    const QUrl thirdFrameUrl = controller.currentFrameImageUrl();
    if (thirdFrameUrl == firstFrameUrl ||
        provider->requestImage("current/2", nullptr, {}).pixel(0, 0) != qRgb(3, 4, 5)) {
        return fail(10, "Image URL revision or provider pixels did not change after navigation.");
    }

    if (controller.next() || currentFrameChanges != 2 ||
        !controller.previous() || controller.currentFrame() != 1 ||
        controller.previous() || currentFrameChanges != 3) {
        return fail(5, "Navigation did not skip missing frames or respect bounds.");
    }

    if (!controller.seek(3) || controller.currentFrame() != 3 || currentFrameChanges != 4 ||
        controller.seek(0) || controller.error().isEmpty() || errorChanges != 1 ||
        controller.currentFrame() != 3 || controller.totalFrames() != 3) {
        return fail(6, "Seek bounds did not retain the factual frame state and error.");
    }

    controller.clear();
    if (controller.presentation() != "empty" || controller.currentFrame() != 0 ||
        controller.totalFrames() != 0 || !controller.error().isEmpty() ||
        presentationChanges != 2 || currentFrameChanges != 5 ||
        totalFramesChanges != 2 || errorChanges != 2 ||
        !controller.currentFrameImageUrl().isEmpty() || imageUrlChanges != 5) {
        return fail(7, "Clear did not restore empty presentation and notify changed properties.");
    }

    if (controller.open(QDir(temp.path()).filePath("missing.json")) ||
        controller.presentation() != "error" || controller.currentFrame() != 0 ||
        controller.totalFrames() != 0 || controller.error().isEmpty() ||
        presentationChanges != 3 || errorChanges != 3) {
        return fail(8, "Open failure did not expose the model error.");
    }

    if (!controller.currentFrameImageUrl().isEmpty())
        return fail(11, "Failure did not clear the image URL.");

    return 0;
}
