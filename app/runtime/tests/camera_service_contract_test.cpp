#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QSignalSpy>
#include <QThread>

#include <atomic>

using namespace desktop_app::v2;

namespace {

class FakeCameraDevice final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("fake-camera"); }

    bool open(QString *error) override
    {
        ++openCalls;
        if (!openError.isEmpty()) {
            *error = openError;
            return false;
        }
        return true;
    }

    bool start(QString *error) override
    {
        ++startCalls;
        if (!startError.isEmpty()) {
            *error = startError;
            return false;
        }
        return true;
    }

    bool stop(QString *error) override
    {
        ++stopCalls;
        if (!stopError.isEmpty()) {
            *error = stopError;
            return false;
        }
        return true;
    }

    bool close(QString *error) override
    {
        ++closeCalls;
        if (!closeError.isEmpty()) {
            *error = closeError;
            return false;
        }
        return true;
    }

    CameraFrameResult latestFrame(CameraFrame &output, QString *error) override
    {
        ++latestFrameCalls;
        if (failFrames.load()) {
            *error = QStringLiteral("Camera transport failed.");
            return CameraFrameResult::Error;
        }
        QMutexLocker locker(&mutex);
        if (frames.isEmpty()) {
            error->clear();
            return CameraFrameResult::NoFrame;
        }
        output = frames.dequeue();
        return CameraFrameResult::Frame;
    }

    void enqueue(CameraFrame frame)
    {
        QMutexLocker locker(&mutex);
        frames.enqueue(std::move(frame));
    }

    QMutex mutex;
    QQueue<CameraFrame> frames;
    QString openError;
    QString startError;
    QString stopError;
    QString closeError;
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
    std::atomic_bool failFrames = false;
    std::atomic_int latestFrameCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

CameraFrame frame(quint64 deliveryId, qint64 timestamp)
{
    CameraFrame result;
    result.pixelFormat = CameraPixelFormat::Mono8;
    result.width = 2;
    result.height = 1;
    result.rowBytes = 2;
    result.bitDepth = 8;
    result.deliveryId = deliveryId;
    result.monotonicTimestampNs = timestamp;
    result.bytes = QByteArray::fromHex("1122");
    return result;
}

QVariantList runCommand(CameraService *service, const char *method, QSignalSpy &spy)
{
    const int before = spy.count();
    QMetaObject::invokeMethod(service, method, Qt::QueuedConnection);
    if (spy.count() == before)
        spy.wait(1000);
    return spy.count() > before ? spy.at(before) : QVariantList{};
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
    QThread worker;
    service->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished, service, &QObject::deleteLater);
    worker.start();

    QSignalSpy commands(service, &CameraService::commandFinished);
    QSignalSpy frames(service, &CameraService::frameReady);
    QSignalSpy frameErrors(service, &CameraService::frameError);

    ok &= check(store.snapshot().camera.status == CameraStatus::Unavailable,
                "Construction must publish Unavailable.");
    QVariantList result = runCommand(service, "open", commands);
    ok &= check(result.value(0).toBool() && fake->openCalls == 1
                    && service->state().status == CameraStatus::Ready,
                "Queued open must publish Ready.");

    fake->enqueue(frame(7, 700));
    result = runCommand(service, "start", commands);
    ok &= check(result.value(0).toBool() && fake->startCalls == 1,
                "Queued start must be accepted by the worker.");
    if (frames.isEmpty())
        frames.wait(1000);
    ok &= check(!frames.isEmpty(), "Worker-affine polling must emit a frame.");
    if (!frames.isEmpty()) {
        const CameraFrame delivered = qvariant_cast<CameraFrame>(frames.first().first());
        ok &= check(delivered.bytes == QByteArray::fromHex("1122")
                        && delivered.deliveryId == 7,
                    "Frame delivery must preserve a deep-owned CameraFrame.");
    }

    fake->enqueue(frame(6, 701));
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    ok &= check(!frameErrors.isEmpty()
                    && frameErrors.takeFirst().first().toString().contains("older"),
                "A regressed delivery identifier must be rejected factually.");

    fake->enqueue(frame(8, 699));
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    ok &= check(!frameErrors.isEmpty()
                    && frameErrors.takeFirst().first().toString().contains("timestamp"),
                "A regressed timestamp must be rejected factually.");

    fake->failFrames = true;
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    const int callsAtFault = fake->latestFrameCalls.load();
    const int errorsAtFault = frameErrors.count();
    QThread::msleep(80);
    QCoreApplication::processEvents();
    ok &= check(service->state().status == CameraStatus::Faulted
                    && service->state().fault == QStringLiteral("Camera transport failed.")
                    && fake->stopCalls == 1 && fake->closeCalls == 1
                    && fake->latestFrameCalls.load() == callsAtFault
                    && errorsAtFault == 1 && frameErrors.count() == errorsAtFault,
                "A polling error must close the stream, publish Faulted, and stop error flooding.");

    fake->failFrames = false;
    result = runCommand(service, "recover", commands);
    ok &= check(result.value(0).toBool() && fake->closeCalls == 2
                    && fake->openCalls == 2
                    && service->state().status == CameraStatus::Ready,
                "Recovery after a polling fault must remain explicit.");

    result = runCommand(service, "start", commands);
    ok &= check(result.value(0).toBool(), "Recovered camera must stream again.");
    result = runCommand(service, "stop", commands);
    ok &= check(result.value(0).toBool() && fake->stopCalls == 2
                    && service->state().status == CameraStatus::Ready,
                "Queued stop must stop polling and publish Ready.");

    fake->startError = QStringLiteral("Camera stream could not be armed.");
    result = runCommand(service, "start", commands);
    ok &= check(!result.value(0).toBool()
                    && result.value(1).toString()
                           == QStringLiteral("Camera stream could not be armed.")
                    && service->state().status == CameraStatus::Faulted,
                "Lifecycle faults must remain factual.");

    runCommand(service, "close", commands);
    worker.quit();
    worker.wait();
    return ok ? 0 : 1;
}
