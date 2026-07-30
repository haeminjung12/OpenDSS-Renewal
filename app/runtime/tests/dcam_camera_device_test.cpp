#include "v2/camera/dcam_camera_device.h"

#include "dcam_camera.h"

#include <QCoreApplication>
#include <QDebug>

#include <utility>
#include <vector>

namespace fake_dcam {
void reset();
void setInitResult(DCAMERR result);
void setStartResult(DCAMERR result);
void queueAllocationResult(DCAMERR result);
void queueReleaseResult(DCAMERR result);
void setProperty(int32 property, double value);
double property(int32 property);
void setAttribute(int32 property, int32 attributes);
void setAttributeResult(int32 property, DCAMERR result);
void clearAttributeResult(int32 property);
void queueSetResult(int32 property, DCAMERR result);
void queueGetValue(int32 property, double value);
void queueGetResult(int32 property, DCAMERR result);
void setFrames(std::vector<FrameData> frames, int frameCount, int newestFrameIndex);
int openedIndex();
int allocations();
int releases();
int starts();
int stops();
int closes();
const std::vector<std::pair<int32, double>> &propertyWrites();
void clearPropertyWrites();
} // namespace fake_dcam

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

FrameData validFrame(int delivery, uchar pixel)
{
    FrameData frame;
    frame.image = cv::Mat(1, 2, CV_8UC1);
    frame.image.at<uchar>(0, 0) = pixel;
    frame.image.at<uchar>(0, 1) = static_cast<uchar>(pixel + 1);
    frame.meta.bits = 8;
    frame.meta.frameIndex = delivery - 1;
    frame.meta.delivered = delivery;
    return frame;
}

CameraAppliedSettings requestedSettings()
{
    CameraAppliedSettings settings;
    settings.width = 2048;
    settings.height = 2048;
    settings.bitDepth = 8;
    settings.pixelType = CameraPixelType::Mono8;
    settings.exposureMs = 5.0;
    settings.readoutMode = CameraReadoutMode::Fast;
    return settings;
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
    ok &= check(device.open(&error) && fake_dcam::openedIndex() == 0
                    && fake_dcam::allocations() == 1,
                "Open must execute the protected init and initial buffer allocation.");

    ok &= check(device.configurationSupport(&error)
                        == CameraConfigurationSupport::Supported
                    && error.isEmpty(),
                "All required readable/writable attributes must enable configuration.");
    fake_dcam::setAttribute(DCAM_IDPROP_EXPOSURETIME,
                            DCAMPROP_ATTR_READABLE);
    ok &= check(device.configurationSupport(&error)
                        == CameraConfigurationSupport::Unsupported
                    && error.isEmpty(),
                "A non-writable approved property must be unsupported without a fault.");
    fake_dcam::setAttribute(
        DCAM_IDPROP_EXPOSURETIME,
        DCAMPROP_ATTR_READABLE | DCAMPROP_ATTR_WRITABLE);
    fake_dcam::setAttributeResult(DCAM_IDPROP_BITSPERCHANNEL,
                                  DCAMERR_NOTSUPPORT);
    ok &= check(device.configurationSupport(&error)
                        == CameraConfigurationSupport::Unsupported
                    && error.isEmpty(),
                "SDK not-supported capability results must not fault camera lifecycle.");
    fake_dcam::setAttributeResult(DCAM_IDPROP_BITSPERCHANNEL,
                                  DCAMERR_INVALIDPROPERTYID);
    ok &= check(device.configurationSupport(&error)
                        == CameraConfigurationSupport::Unsupported
                    && error.isEmpty(),
                "A missing approved property must be unsupported without a fault.");
    fake_dcam::setAttributeResult(DCAM_IDPROP_BITSPERCHANNEL,
                                  DCAMERR_TEST_FAILURE);
    ok &= check(device.configurationSupport(&error)
                        == CameraConfigurationSupport::Error
                    && error.contains(QStringLiteral("bits per channel")),
                "A genuine SDK capability probe fault must remain factual.");
    fake_dcam::clearAttributeResult(DCAM_IDPROP_BITSPERCHANNEL);

    CameraAppliedSettings configuration;
    ok &= check(device.readConfiguration(configuration, &error)
                    && configuration.width == 1024
                    && configuration.height == 1024
                    && configuration.bitDepth == 12
                    && configuration.pixelType == CameraPixelType::Mono16
                    && configuration.exposureMs == 10.0
                    && configuration.readoutMode == CameraReadoutMode::Fast,
                "Protected readback must convert seconds and factual Fast numeric values.");

    fake_dcam::clearPropertyWrites();
    CameraAppliedSettings requested = requestedSettings();
    CameraAppliedSettings applied;
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Applied
                    && applied.width == 2048 && applied.height == 2048
                    && applied.bitDepth == 8 && applied.exposureMs == 5.0,
                "Approved configuration must publish factual protected readback.");
    const std::vector<std::pair<int32, double>> expectedWrites = {
        {DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF},
        {DCAM_IDPROP_SUBARRAYHPOS, 0.0},
        {DCAM_IDPROP_SUBARRAYVPOS, 0.0},
        {DCAM_IDPROP_SUBARRAYHSIZE, 2048.0},
        {DCAM_IDPROP_SUBARRAYVSIZE, 2048.0},
        {DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON},
        {DCAM_IDPROP_IMAGE_PIXELTYPE, DCAM_PIXELTYPE_MONO8},
        {DCAM_IDPROP_BITSPERCHANNEL, 8.0},
        {DCAM_IDPROP_EXPOSURETIME, 0.005},
        {DCAM_IDPROP_READOUTSPEED,
         static_cast<double>(DCAMPROP_READOUTSPEED__FASTEST)},
    };
    ok &= check(fake_dcam::propertyWrites() == expectedWrites
                    && fake_dcam::releases() == 1
                    && fake_dcam::allocations() == 2,
                "Protected apply must use the exact approved property order and buffer cycle.");

    const CameraAppliedSettings retained = applied;
    fake_dcam::queueGetValue(DCAM_IDPROP_SUBARRAYHSIZE, requested.width);
    fake_dcam::queueGetValue(DCAM_IDPROP_SUBARRAYHSIZE, requested.width + 1);
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Rejected
                    && error.contains(
                        QStringLiteral("width requested 2048, read back 2049"))
                    && device.readConfiguration(applied, &error)
                    && applied.width == retained.width,
                "A strict readback mismatch must identify its field and roll back.");

    requested.exposureMs = 7.0;
    fake_dcam::queueSetResult(DCAM_IDPROP_EXPOSURETIME,
                              DCAMERR_TEST_FAILURE);
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Rejected
                    && error.contains(QStringLiteral("exposure"))
                    && device.readConfiguration(applied, &error)
                    && applied.exposureMs == retained.exposureMs,
                "Partial setter failure must roll back and verify prior settings.");

    fake_dcam::queueReleaseResult(DCAMERR_TEST_FAILURE);
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Rejected
                    && error.contains(QStringLiteral("dcambuf_release")),
                "Buffer release failure must be reported and successfully rolled back.");

    fake_dcam::queueAllocationResult(DCAMERR_TEST_FAILURE);
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Rejected
                    && error.contains(QStringLiteral("dcambuf_alloc")),
                "Buffer allocation failure must be reported and successfully rolled back.");

    fake_dcam::queueSetResult(DCAM_IDPROP_EXPOSURETIME,
                              DCAMERR_TEST_FAILURE);
    fake_dcam::queueGetValue(DCAM_IDPROP_SUBARRAYHSIZE, retained.width);
    fake_dcam::queueGetValue(DCAM_IDPROP_SUBARRAYHSIZE, retained.width + 1);
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::StateUnknown
                    && error.contains(QStringLiteral("Rollback verification failed")),
                "Rollback readback mismatch must make device state unknown.");

    requested.width = 0;
    const size_t writesBeforeValidation = fake_dcam::propertyWrites().size();
    ok &= check(device.applyConfiguration(requested, applied, &error)
                        == CameraConfigurationResult::Rejected
                    && fake_dcam::propertyWrites().size() == writesBeforeValidation,
                "Invalid configuration must be rejected before a vendor setter.");

    ok &= check(device.start(&error) && fake_dcam::starts() == 1,
                "Start must delegate through protected DcamCamera.");
    std::vector<CameraFrame> output;
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::NoFrame
                    && output.empty()
                    && error.isEmpty(),
                "An empty transfer ring must remain NoFrame.");

    std::vector<FrameData> source;
    for (int delivery = 1; delivery <= 4; ++delivery)
        source.push_back(validFrame(delivery, static_cast<uchar>(delivery * 0x10)));
    fake_dcam::setProperty(DCAM_IDPROP_BITSPERCHANNEL, 8.0);
    fake_dcam::setFrames(source, 4, 3);
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::Frame
                    && output.size() == 4
                    && output[0].pixelFormat == CameraPixelFormat::Mono8
                    && output[0].deliveryId == 1
                    && output[1].deliveryId == 2
                    && output[2].deliveryId == 3
                    && output[3].deliveryId == 4
                    && output[0].bytes == QByteArray::fromHex("1011")
                    && output[3].bytes == QByteArray::fromHex("4041")
                    && output[0].monotonicTimestampNs > 0
                    && output[1].monotonicTimestampNs
                        - output[0].monotonicTimestampNs == 10'000'000
                    && output[2].monotonicTimestampNs
                        - output[1].monotonicTimestampNs == 10'000'000
                    && output[3].monotonicTimestampNs
                        - output[2].monotonicTimestampNs == 10'000'000,
                "One drain must map every burst frame in source order with "
                "qualified 100 fps acquisition spacing.");
    const qint64 firstBatchLastTimestamp = output.back().monotonicTimestampNs;
    source[0].image.at<uchar>(0, 0) = 0x7f;
    ok &= check(output[0].bytes == QByteArray::fromHex("1011"),
                "Batch adapter output must not alias ring-buffer memory.");

    source.clear();
    for (int delivery = 5; delivery <= 20; ++delivery)
        source.push_back(validFrame(delivery, static_cast<uchar>(delivery)));
    fake_dcam::setFrames(source, 20, 3);
    output.clear();
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::Frame
                    && output.size() == 16
                    && output.front().deliveryId == 5
                    && output.back().deliveryId == 20
                    && output.front().monotonicTimestampNs
                        - firstBatchLastTimestamp == 10'000'000,
                "A full-capacity ring wrap must drain each unread slot exactly once.");
    bool wrappedPixelsMatch = output.size() == 16;
    bool wrappedTimestampsMatch = output.size() == 16;
    for (int index = 0; index < static_cast<int>(output.size()); ++index) {
        wrappedPixelsMatch = wrappedPixelsMatch
            &&
            static_cast<uchar>(output[index].bytes.at(0))
            == static_cast<uchar>(index + 5);
        if (index > 0) {
            wrappedTimestampsMatch = wrappedTimestampsMatch
                &&
                output[index].monotonicTimestampNs
                    - output[index - 1].monotonicTimestampNs == 10'000'000;
        }
    }
    ok &= check(wrappedPixelsMatch,
                "Ring-wrap output pixels must remain paired with contiguous delivery IDs.");
    ok &= check(wrappedTimestampsMatch,
                "Ring-wrap timestamps must retain qualified per-frame spacing.");

    ok &= check(device.stop(&error) && device.start(&error),
                "Single-frame timestamp setup must restart acquisition cleanly.");
    source = {validFrame(1, 0x51)};
    fake_dcam::setFrames(source, 1, 0);
    output.clear();
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::Frame
                    && output.size() == 1
                    && output.front().deliveryId == 1
                    && output.front().monotonicTimestampNs > 0,
                "One unread frame must retain a valid monotonic timestamp.");
    const qint64 firstSingleTimestamp = output.front().monotonicTimestampNs;
    source = {validFrame(2, 0x52)};
    fake_dcam::setFrames(source, 2, 1);
    output.clear();
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::Frame
                    && output.size() == 1
                    && output.front().deliveryId == 2
                    && output.front().monotonicTimestampNs
                        - firstSingleTimestamp == 10'000'000,
                "Adjacent single-frame drains must preserve qualified spacing.");

    ok &= check(device.stop(&error) && device.start(&error),
                "Overrun setup must restart acquisition cleanly.");
    source.clear();
    for (int delivery = 2; delivery <= 17; ++delivery)
        source.push_back(validFrame(delivery, static_cast<uchar>(delivery)));
    fake_dcam::setFrames(source, 17, 0);
    output.clear();
    ok &= check(device.drainFrames(output, &error) == CameraFrameResult::Error
                    && output.empty()
                    && error.contains(QStringLiteral("ring buffer overrun")),
                "Unread data beyond ring capacity must fault instead of skipping frames.");

    ok &= check(device.stop(&error), "Stop must delegate through protected DcamCamera.");
    const int closesBefore = fake_dcam::closes();
    ok &= check(device.close(&error) && fake_dcam::closes() == closesBefore + 1,
                "Close must execute protected cleanup.");
    ok &= check(device.close(&error) && fake_dcam::closes() == closesBefore + 1,
                "Repeated close must remain deterministic.");

    fake_dcam::reset();
    fake_dcam::queueAllocationResult(DCAMERR_TEMPERATURE_TROUBLE);
    DcamCameraDevice temperatureDevice;
    ok &= check(!temperatureDevice.open(&error)
                    && error.contains(QStringLiteral("camera temperature trouble"))
                    && error.contains(QStringLiteral("0x80000304")),
                "Initial buffer temperature trouble must be translated factually.");

    fake_dcam::reset();
    DcamCameraDevice startErrorDevice;
    ok &= check(startErrorDevice.open(&error), "Start-fault setup must open.");
    fake_dcam::setStartResult(DCAMERR_TEST_FAILURE);
    ok &= check(!startErrorDevice.start(&error)
                    && error.contains(QStringLiteral("dcamcap_start")),
                "Protected start errors must preserve their operation label.");
    startErrorDevice.close(&error);

    return ok ? 0 : 1;
}

