#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSignalSpy>
#include <QThread>

#include <atomic>

using namespace desktop_app::v2;

namespace {

class FakeCameraDevice final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("fake-camera"); }
    bool open(QString *) override { ++openCalls; return true; }
    bool start(QString *error) override
    {
        ++startCalls;
        if (failStart.load()) {
            *error = QStringLiteral("Camera stream could not be armed.");
            return false;
        }
        return true;
    }
    bool stop(QString *) override { ++stopCalls; return true; }
    bool close(QString *) override { ++closeCalls; return true; }
    CameraFrameResult latestFrame(CameraFrame &frame, QString *error) override
    {
        if (failFrames.load()) {
            *error = QStringLiteral("Camera transport failed.");
            return CameraFrameResult::Error;
        }
        const quint64 id = delivery.fetch_add(1);
        frame.pixelFormat = CameraPixelFormat::Mono8;
        frame.width = 2;
        frame.height = 1;
        frame.rowBytes = 2;
        frame.bitDepth = 8;
        frame.deliveryId = id;
        frame.monotonicTimestampNs = static_cast<qint64>(id * 100);
        frame.bytes = QByteArray::fromHex("1122");
        return CameraFrameResult::Frame;
    }

    std::atomic_bool failStart = false;
    std::atomic_bool failFrames = false;
    std::atomic<quint64> delivery = 1;
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool waitForIdle(CameraController &controller, QSignalSpy &busySpy)
{
    while (controller.busy()) {
        if (!busySpy.wait(1000))
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ApplicationStateStore store;
    auto device = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *fake = device.get();
    auto *service = new CameraService(std::move(device), store);
    CameraPreviewImageProvider provider;
    CameraController controller(*service, provider);
    QThread worker;
    service->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished, service, &QObject::deleteLater);
    worker.start();

    QSignalSpy busySpy(&controller, &CameraController::busyChanged);
    QSignalSpy previewSpy(&controller, &CameraController::previewSourceChanged);

    ok &= check(controller.open() && !controller.open(),
                "Lifecycle bools must report request acceptance, not completion.");
    ok &= check(waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.deviceId() == QStringLiteral("fake-camera"),
                "Controller must cache the queued open projection.");

    ok &= check(controller.start() && waitForIdle(controller, busySpy)
                    && controller.streaming(),
                "Controller must queue start and project Streaming.");
    if (controller.previewSource().isEmpty())
        previewSpy.wait(1000);
    ok &= check(controller.previewSource().startsWith(
                    QStringLiteral("image://camera-preview/frame?r=")),
                "Controller must publish a revisioned camera-preview URL.");
    const QImage preview = provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(!preview.isNull() && preview.size() == QSize(2, 1),
                "Controller frames must immediately feed the preview provider.");

    ok &= check(controller.stop() && waitForIdle(controller, busySpy)
                    && !controller.streaming()
                    && controller.cameraStatus() == QStringLiteral("Connected"),
                "Stop must retain the Connected projection.");

    ok &= check(controller.start() && waitForIdle(controller, busySpy),
                "Controller must restart streaming before polling-fault coverage.");
    QSignalSpy stateSpy(&controller, &CameraController::stateChanged);
    fake->failFrames = true;
    if (controller.cameraStatus() != QStringLiteral("Unavailable"))
        stateSpy.wait(1000);
    ok &= check(controller.cameraStatus() == QStringLiteral("Unavailable")
                    && controller.error() == QStringLiteral("Camera transport failed.")
                    && fake->stopCalls == 2 && fake->closeCalls == 1,
                "A polling fault must project closed Unavailable truth through the facade.");
    fake->failFrames = false;
    ok &= check(controller.recover() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.error().isEmpty(),
                "Polling-fault recovery must remain an explicit controller action.");

    fake->failStart = true;
    ok &= check(controller.start() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Unavailable")
                    && controller.error()
                           == QStringLiteral("Camera stream could not be armed."),
                "Worker faults must project through the GUI facade.");
    fake->failStart = false;
    ok &= check(controller.recover() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.error().isEmpty(),
                "Recover must clear the factual fault without replacing the service.");

    controller.close();
    waitForIdle(controller, busySpy);
    worker.quit();
    worker.wait();
    return ok ? 0 : 1;
}
