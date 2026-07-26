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
    CameraConfigurationSupport configurationSupport(QString *error) const override
    {
        error->clear();
        return CameraConfigurationSupport::Supported;
    }
    bool readConfiguration(CameraAppliedSettings &settings, QString *error) override
    {
        settings = appliedSettings;
        error->clear();
        return true;
    }
    CameraConfigurationResult applyConfiguration(
        const CameraAppliedSettings &requested,
        CameraAppliedSettings &applied,
        QString *error) override
    {
        ++applyConfigurationCalls;
        if (rejectConfiguration.load()) {
            *error = QStringLiteral("Camera configuration was rejected.");
            return CameraConfigurationResult::Rejected;
        }
        appliedSettings = requested;
        applied = appliedSettings;
        error->clear();
        return CameraConfigurationResult::Applied;
    }
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
    std::atomic_bool rejectConfiguration = false;
    std::atomic<quint64> delivery = 1;
    CameraAppliedSettings appliedSettings = [] {
        CameraAppliedSettings settings;
        settings.width = 2304;
        settings.height = 2304;
        settings.bitDepth = 12;
        settings.pixelType = CameraPixelType::Mono16;
        settings.exposureMs = 10.0;
        settings.readoutMode = CameraReadoutMode::Fast;
        return settings;
    }();
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
    int applyConfigurationCalls = 0;
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
                    && controller.deviceId() == QStringLiteral("fake-camera")
                    && controller.configurationAvailable()
                    && controller.resolution() == QStringLiteral("2304 x 2304")
                    && controller.resolutionPresets().size() == 25
                    && controller.resolutionPresets().at(20)
                           == QStringLiteral("Custom")
                    && controller.resolutionPresets().at(21)
                           == QStringLiteral("512 x 128")
                    && controller.resolutionPresetIndex() == 0
                    && controller.bitDepth() == QStringLiteral("12-bit")
                    && controller.exposureMs() == QStringLiteral("10")
                    && controller.readoutMode() == QStringLiteral("Fast"),
                "Controller must cache the queued open projection and factual settings.");

    ok &= check(controller.applyExposureMs(4.5)
                    && !controller.applyBitDepth(8)
                    && waitForIdle(controller, busySpy)
                    && controller.exposureMs() == QStringLiteral("4.5")
                    && fake->applyConfigurationCalls == 1,
                "Controller must serialize configuration and publish factual readback.");

    ok &= check(controller.selectCustomResolution()
                    && controller.resolution() == QStringLiteral("Custom"),
                "Custom selection may reveal the existing draft fields.");
    fake->rejectConfiguration = true;
    ok &= check(controller.applyResolution(1536, 1024)
                    && waitForIdle(controller, busySpy)
                    && controller.error()
                           == QStringLiteral("Camera configuration was rejected.")
                    && controller.customWidth() == QStringLiteral("2304")
                    && controller.customHeight() == QStringLiteral("2304")
                    && controller.resolution() == QStringLiteral("Custom"),
                "Rejected custom edits must retain factual applied dimensions and draft mode.");
    fake->rejectConfiguration = false;
    ok &= check(controller.selectResolutionPreset(10)
                    && waitForIdle(controller, busySpy)
                    && controller.resolution() == QStringLiteral("1152 x 1152")
                    && controller.resolutionPresetIndex() == 10,
                "A successful preset apply must leave Custom draft mode.");

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
    const QString previewBeforeLut = controller.previewSource();
    controller.setPreviewLutRange(20, 30);
    const QImage lutPreview =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(controller.previewLutMinimum() == 20
                    && controller.previewLutMaximum() == 30
                    && controller.previewSource() != previewBeforeLut
                    && lutPreview.constScanLine(0)[0] == 0
                    && lutPreview.constScanLine(0)[1] == 255,
                "Preview LUT changes must republish only the rendered camera image.");

    ok &= check(controller.applyExposureMs(6.0)
                    && !controller.applyBitDepth(8)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.exposureMs() == QStringLiteral("6")
                    && controller.error().isEmpty()
                    && fake->stopCalls == 1 && fake->startCalls == 2,
                "Streaming configuration must reach the serialized service transaction.");

    fake->rejectConfiguration = true;
    ok &= check(controller.applyExposureMs(9.0)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.exposureMs() == QStringLiteral("6")
                    && controller.error()
                           == QStringLiteral("Camera configuration was rejected.")
                    && fake->stopCalls == 2 && fake->startCalls == 3,
                "Rejected streaming configuration must retain facts and project its error.");
    fake->rejectConfiguration = false;

    ok &= check(controller.selectCustomResolution()
                    && controller.resolution() == QStringLiteral("Custom")
                    && controller.applyResolution(1536, 1024)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.resolution() == QStringLiteral("Custom")
                    && controller.customWidth() == QStringLiteral("1536")
                    && controller.customHeight() == QStringLiteral("1024")
                    && fake->stopCalls == 3 && fake->startCalls == 4,
                "Custom resolution must be selectable and factually applied while streaming.");

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
                    && fake->stopCalls == 5 && fake->closeCalls == 1,
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
