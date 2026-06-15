#include "camera_worker.h"

#include <dcamapi4.h>
#include <dcamprop.h>

#include <cmath>

namespace {
constexpr qint64 kMinimumEmitIntervalMs = 15;
constexpr int kFrameWaitTimeoutMs = 50;

bool readPropertyAttr(HDCAM handle, int32 prop, DCAMPROP_ATTR& attr) {
    attr = {};
    attr.cbSize = sizeof(attr);
    attr.iProp = prop;
    return !failed(dcamprop_getattr(handle, &attr));
}

QString valueText(HDCAM handle, int32 prop, double value, const QString& fallback) {
    char text[128] = {};
    DCAMPROP_VALUETEXT vt = {};
    vt.cbSize = sizeof(vt);
    vt.iProp = prop;
    vt.value = value;
    vt.text = text;
    vt.textbytes = sizeof(text);
    if (!failed(dcamprop_getvaluetext(handle, &vt)) && text[0] != '\0') {
        return QString::fromLocal8Bit(text);
    }
    return fallback;
}

bool queryExactValue(HDCAM handle, int32 prop, int candidate, int* queriedValue = nullptr) {
    double value = candidate;
    if (failed(dcamprop_queryvalue(handle, prop, &value))) {
        return false;
    }
    const int rounded = static_cast<int>(std::lround(value));
    if (queriedValue) {
        *queriedValue = rounded;
    }
    return rounded == candidate;
}

void appendUniqueOption(QVariantList& options, const QString& label, int value) {
    for (const QVariant& option : options) {
        if (option.toMap().value(QStringLiteral("value")).toInt() == value) {
            return;
        }
    }
    QVariantMap option;
    option.insert(QStringLiteral("label"), label);
    option.insert(QStringLiteral("value"), value);
    options.append(option);
}

void appendUniquePreset(QVariantList& presets, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    for (const QVariant& preset : presets) {
        const QVariantMap map = preset.toMap();
        if (map.value(QStringLiteral("width")).toInt() == width &&
            map.value(QStringLiteral("height")).toInt() == height) {
            return;
        }
    }
    QVariantMap preset;
    preset.insert(QStringLiteral("label"), QStringLiteral("%1 x %2").arg(width).arg(height));
    preset.insert(QStringLiteral("width"), width);
    preset.insert(QStringLiteral("height"), height);
    presets.append(preset);
}

bool sizeMatchesAttr(int value, const DCAMPROP_ATTR& attr) {
    if (value <= 0) {
        return false;
    }
    if ((attr.attribute & DCAMPROP_ATTR_HASRANGE) != 0) {
        if (value < static_cast<int>(std::ceil(attr.valuemin)) || value > static_cast<int>(std::floor(attr.valuemax))) {
            return false;
        }
    }
    if ((attr.attribute & DCAMPROP_ATTR_HASSTEP) != 0 && attr.valuestep > 0.0) {
        const double offset = (value - attr.valuemin) / attr.valuestep;
        if (std::abs(offset - std::round(offset)) > 0.0001) {
            return false;
        }
    }
    return true;
}
} // namespace

CameraWorker::CameraWorker(QObject* parent) : QObject(parent) {
    secondTimer_.start();
    emitTimer_.start();
}

CameraWorker::~CameraWorker() {
    if (controller_) {
        controller_->cleanup();
    }
}

void CameraWorker::setRecordHook(std::function<void(const QImage&, const FrameMeta&, double)> hook) {
    recordHook_ = std::move(hook);
}

void CameraWorker::initAndOpen(int bits, int pixelType) {
    shuttingDown_ = false;
    DcamController* ctrl = controller();
    const QString error = ctrl->initAndOpen();
    if (error.isEmpty()) {
        applyDefaultCameraFormat(bits, pixelType);
        emitFormatOptions();
        emitExposureLimits();
    }
    emit initCompleted(error);
}

void CameraWorker::reconnect(int bits, int pixelType) {
    running_ = false;
    DcamController* ctrl = controller();
    const QString error = ctrl->reconnect();
    if (error.isEmpty()) {
        applyDefaultCameraFormat(bits, pixelType);
        emitFormatOptions();
        emitExposureLimits();
    }
    emit reconnectCompleted(error);
}

void CameraWorker::startCapture(int bits, int pixelType) {
    DcamController* ctrl = controller();
    if (ctrl->isOpened()) {
        applyDefaultCameraFormat(bits, pixelType);
    }
    const QString error = ctrl->start();
    if (error.isEmpty()) {
        running_ = true;
        displayCounter_ = 0;
        framesThisSecond_ = 0;
        currentFps_ = 0.0;
        secondTimer_.restart();
        emitTimer_.restart();
        lastEmitMs_ = 0;
        scheduleGrab();
    }
    emit startCompleted(error);
}

void CameraWorker::stopCapture() {
    running_ = false;
    if (controller_) {
        controller_->stop();
    }
    emit stopCompleted();
}

void CameraWorker::applySettings(const ApplySettings& settings) {
    DcamController* ctrl = controller();
    const QString error = ctrl->apply(settings);
    if (error.isEmpty() || error.startsWith(QStringLiteral("WARN:"))) {
        running_ = true;
        displayCounter_ = 0;
        framesThisSecond_ = 0;
        currentFps_ = 0.0;
        secondTimer_.restart();
        emitTimer_.restart();
        lastEmitMs_ = 0;
        scheduleGrab();
    }
    emitReadback();
    emit applyCompleted(error);
}

void CameraWorker::setDisplayEvery(int n) {
    displayEvery_ = std::max(1, n);
}

void CameraWorker::shutdown() {
    shuttingDown_ = true;
    running_ = false;
    if (controller_) {
        controller_->stop();
        controller_->cleanup();
    }
    emit shutdownDone();
}

void CameraWorker::grabOnce() {
    if (!running_ || shuttingDown_) {
        return;
    }

    DcamController* ctrl = controller();
    if (!ctrl->isOpened()) {
        scheduleGrab(50);
        return;
    }

    DCAMWAIT_START wait = {};
    wait.size = sizeof(wait);
    wait.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    wait.timeout = kFrameWaitTimeoutMs;
    const DCAMERR waitError = dcamwait_start(ctrl->waitHandle(), &wait);
    if (failed(waitError)) {
        scheduleGrab(5);
        return;
    }

    QImage image;
    FrameMeta meta;
    if (ctrl->lockLatestFrame(image, meta)) {
        if (recordHook_) {
            recordHook_(image, meta, currentFps_);
        }
        ++framesThisSecond_;
        if (secondTimer_.elapsed() >= 1000) {
            currentFps_ = framesThisSecond_ * 1000.0 / secondTimer_.elapsed();
            framesThisSecond_ = 0;
            secondTimer_.restart();
        }
        ++displayCounter_;
        if (displayCounter_ >= displayEvery_ && emitTimer_.elapsed() - lastEmitMs_ >= kMinimumEmitIntervalMs) {
            displayCounter_ = 0;
            lastEmitMs_ = emitTimer_.elapsed();
            emit frameReady(image.copy(), meta, currentFps_);
        }
    }

    scheduleGrab();
}

DcamController* CameraWorker::controller() {
    if (!controller_) {
        controller_ = std::make_unique<DcamController>();
    }
    return controller_.get();
}

void CameraWorker::emitExposureLimits() {
    DcamController* ctrl = controller();
    if (!ctrl->isOpened()) {
        return;
    }

    DCAMPROP_ATTR attr = {};
    attr.cbSize = sizeof(attr);
    attr.iProp = DCAM_IDPROP_EXPOSURETIME;
    double current = 0.0;
    if (!failed(dcamprop_getattr(ctrl->handle(), &attr)) &&
        !failed(dcamprop_getvalue(ctrl->handle(), DCAM_IDPROP_EXPOSURETIME, &current))) {
        emit exposureLimitsReady(attr.valuemin * 1000.0, attr.valuemax * 1000.0, current * 1000.0);
    }
}

void CameraWorker::emitFormatOptions() {
    DcamController* ctrl = controller();
    if (!ctrl->isOpened()) {
        return;
    }

    HDCAM handle = ctrl->handle();
    DCAMPROP_ATTR widthAttr = {};
    DCAMPROP_ATTR heightAttr = {};
    DCAMPROP_ATTR exposureAttr = {};
    readPropertyAttr(handle, DCAM_IDPROP_SUBARRAYHSIZE, widthAttr);
    readPropertyAttr(handle, DCAM_IDPROP_SUBARRAYVSIZE, heightAttr);
    readPropertyAttr(handle, DCAM_IDPROP_EXPOSURETIME, exposureAttr);

    double currentWidth = 0.0;
    double currentHeight = 0.0;
    double exposure = 0.0;
    dcamprop_getvalue(handle, DCAM_IDPROP_IMAGE_WIDTH, &currentWidth);
    dcamprop_getvalue(handle, DCAM_IDPROP_IMAGE_HEIGHT, &currentHeight);
    dcamprop_getvalue(handle, DCAM_IDPROP_EXPOSURETIME, &exposure);

    const int maximumWidth = widthAttr.valuemax > 0.0 ? static_cast<int>(std::floor(widthAttr.valuemax))
                                                      : static_cast<int>(std::lround(currentWidth));
    const int maximumHeight = heightAttr.valuemax > 0.0 ? static_cast<int>(std::floor(heightAttr.valuemax))
                                                        : static_cast<int>(std::lround(currentHeight));

    QVariantList presets;
    appendUniquePreset(presets, static_cast<int>(std::lround(currentWidth)),
                       static_cast<int>(std::lround(currentHeight)));
    appendUniquePreset(presets, maximumWidth, maximumHeight);
    const QList<QSize> commonPresets = {
        {2304, 2304}, {2304, 1152}, {2304, 576},  {2304, 288}, {2304, 144}, {2304, 72},  {2304, 36}, {2304, 16},
        {2304, 8},    {2304, 4},    {1152, 1152}, {1152, 576}, {1152, 288}, {1152, 144}, {576, 576}, {576, 288},
        {576, 144},   {512, 128},   {512, 64},    {288, 288},  {288, 144},  {256, 64},   {256, 32},  {144, 144},
    };
    for (const QSize& size : commonPresets) {
        if (size.width() <= maximumWidth && size.height() <= maximumHeight &&
            sizeMatchesAttr(size.width(), widthAttr) && sizeMatchesAttr(size.height(), heightAttr)) {
            appendUniquePreset(presets, size.width(), size.height());
        }
    }

    QVariantList bitDepths;
    for (const int bits : {8, 10, 12, 14, 16}) {
        if (queryExactValue(handle, DCAM_IDPROP_BITSPERCHANNEL, bits)) {
            appendUniqueOption(bitDepths, valueText(handle, DCAM_IDPROP_BITSPERCHANNEL, bits, QString::number(bits)),
                               bits);
        }
    }

    QVariantList readoutSpeeds;
    double queriedReadout = DCAMPROP_READOUTSPEED__SLOWEST;
    if (!failed(dcamprop_queryvalue(handle, DCAM_IDPROP_READOUTSPEED, &queriedReadout))) {
        appendUniqueOption(readoutSpeeds, QStringLiteral("Slow"), static_cast<int>(std::lround(queriedReadout)));
    }
    queriedReadout = DCAMPROP_READOUTSPEED__FASTEST;
    if (!failed(dcamprop_queryvalue(handle, DCAM_IDPROP_READOUTSPEED, &queriedReadout))) {
        appendUniqueOption(readoutSpeeds, QStringLiteral("Fast"), static_cast<int>(std::lround(queriedReadout)));
    }

    QVariantMap options;
    options.insert(QStringLiteral("presets"), presets);
    options.insert(QStringLiteral("bitDepths"), bitDepths);
    options.insert(QStringLiteral("readoutSpeeds"), readoutSpeeds);
    options.insert(QStringLiteral("maximumWidth"), maximumWidth);
    options.insert(QStringLiteral("maximumHeight"), maximumHeight);
    options.insert(QStringLiteral("minimumExposureMs"), exposureAttr.valuemin * 1000.0);
    options.insert(QStringLiteral("maximumExposureMs"), exposureAttr.valuemax * 1000.0);
    options.insert(QStringLiteral("currentExposureMs"), exposure * 1000.0);
    emit formatOptionsReady(options);
}

void CameraWorker::emitReadback() {
    DcamController* ctrl = controller();
    if (!ctrl->isOpened()) {
        return;
    }

    HDCAM handle = ctrl->handle();
    double width = 0;
    double height = 0;
    double binning = 0;
    double bits = 0;
    double pixelType = 0;
    double fps = 0;
    double readout = 0;
    double exposure = 0;
    double binH = 0;
    double binV = 0;
    dcamprop_getvalue(handle, DCAM_IDPROP_IMAGE_WIDTH, &width);
    dcamprop_getvalue(handle, DCAM_IDPROP_IMAGE_HEIGHT, &height);
    dcamprop_getvalue(handle, DCAM_IDPROP_BINNING, &binning);
    dcamprop_getvalue(handle, DCAM_IDPROP_BITSPERCHANNEL, &bits);
    dcamprop_getvalue(handle, DCAM_IDPROP_IMAGE_PIXELTYPE, &pixelType);
    dcamprop_getvalue(handle, DCAM_IDPROP_INTERNALFRAMERATE, &fps);
    dcamprop_getvalue(handle, DCAM_IDPROP_READOUTSPEED, &readout);
    dcamprop_getvalue(handle, DCAM_IDPROP_EXPOSURETIME, &exposure);
    dcamprop_getvalue(handle, DCAM_IDPROP_BINNING_HORZ, &binH);
    dcamprop_getvalue(handle, DCAM_IDPROP_BINNING_VERT, &binV);
    emit readbackReady(
        QString("Readback: w=%1 h=%2 bin=%3 binH=%4 binV=%5 bits=%6 pixType=%7 exp_ms=%8 camfps=%9 readout=%10")
            .arg(width, 0, 'f', 0)
            .arg(height, 0, 'f', 0)
            .arg(binning, 0, 'f', 1)
            .arg(binH, 0, 'f', 1)
            .arg(binV, 0, 'f', 1)
            .arg(bits, 0, 'f', 0)
            .arg(pixelType, 0, 'f', 0)
            .arg(exposure * 1000.0, 0, 'f', 3)
            .arg(fps, 0, 'f', 1)
            .arg(readout, 0, 'f', 0));
}

void CameraWorker::applyDefaultCameraFormat(int bits, int pixelType) {
    DcamController* ctrl = controller();
    if (!ctrl->isOpened()) {
        return;
    }
    dcamprop_setvalue(ctrl->handle(), DCAM_IDPROP_EXPOSURETIME, 0.010);
    dcamprop_setvalue(ctrl->handle(), DCAM_IDPROP_IMAGE_PIXELTYPE, pixelType);
    dcamprop_setvalue(ctrl->handle(), DCAM_IDPROP_BITSPERCHANNEL, bits);
}

void CameraWorker::scheduleGrab(int delayMs) {
    if (!running_ || shuttingDown_) {
        return;
    }
    QTimer::singleShot(delayMs, this, &CameraWorker::grabOnce);
}
