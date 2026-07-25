#include "v2/camera/dcam_camera_device.h"

#include "dcam_camera.h"

#include <QCoreApplication>
#include <QDebug>

namespace fake_dcam {
void reset();
void setInitError(std::string error);
void setStartError(std::string error);
void setWaitResult(bool result);
void setFrameResult(bool result);
void setFrame(FrameData frame);
int initIndex();
int constructions();
int cleanups();
int starts();
int stops();
} // namespace fake_dcam

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

FrameData validFrame()
{
    FrameData frame;
    frame.image = cv::Mat(1, 2, CV_8UC1);
    frame.image.at<uchar>(0, 0) = 0x11;
    frame.image.at<uchar>(0, 1) = 0x22;
    frame.meta.bits = 8;
    frame.meta.frameIndex = 3;
    frame.meta.delivered = 4;
    return frame;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;

    fake_dcam::reset();
    DcamCameraDevice device;
    ok &= check(device.deviceId() == QStringLiteral("DCAM:0"),
                "Adapter identity must be fixed to DCAM index 0.");
    ok &= check(device.open(&error) && fake_dcam::initIndex() == 0
                    && fake_dcam::constructions() == 1,
                "Open must create one protected camera and initialize index 0.");
    ok &= check(device.start(&error) && fake_dcam::starts() == 1,
                "Start must delegate to DcamCamera.");

    CameraFrame output;
    error = QStringLiteral("stale");
    ok &= check(device.latestFrame(output, &error) == CameraFrameResult::NoFrame
                    && error.isEmpty(),
                "A wait without a new frame must return NoFrame without an error.");

    fake_dcam::setWaitResult(true);
    fake_dcam::setFrameResult(true);
    FrameData source = validFrame();
    fake_dcam::setFrame(source);
    ok &= check(device.latestFrame(output, &error) == CameraFrameResult::Frame
                    && output.pixelFormat == CameraPixelFormat::Mono8
                    && output.deliveryId == 4
                    && output.bytes == QByteArray::fromHex("1122"),
                "A delivered frame must be mapped into an owned CameraFrame.");
    source.image.at<uchar>(0, 0) = 0x7f;
    ok &= check(output.bytes == QByteArray::fromHex("1122"),
                "Adapter output must not alias the caller's frame.");

    ok &= check(device.stop(&error) && fake_dcam::stops() == 1,
                "Stop must delegate serially.");
    ok &= check(device.close(&error) && fake_dcam::cleanups() == 1,
                "Close must clean up and release the protected camera.");
    ok &= check(device.close(&error) && fake_dcam::cleanups() == 1,
                "Repeated close must be deterministic.");
    ok &= check(device.open(&error) && fake_dcam::constructions() == 2
                    && fake_dcam::initIndex() == 0,
                "Open after close must recreate the protected camera at index 0.");

    ok &= check(device.close(&error), "Error-translation setup must close.");
    fake_dcam::setInitError("DCAM init failed.");
    ok &= check(!device.open(&error) && error == QStringLiteral("DCAM init failed."),
                "Init failure text must be preserved.");
    fake_dcam::setInitError({});
    ok &= check(device.open(&error), "Start-error setup must reopen.");
    fake_dcam::setStartError("DCAM start failed.");
    ok &= check(!device.start(&error) && error == QStringLiteral("DCAM start failed."),
                "Start failure text must be preserved.");
    ok &= check(device.close(&error), "Final close must succeed.");

    return ok ? 0 : 1;
}
