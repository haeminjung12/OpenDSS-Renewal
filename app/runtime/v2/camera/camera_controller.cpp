#include "camera_controller.h"

#include "camera_preview_image_provider.h"
#include "camera_service.h"
#include "frame_conversion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <QEventLoop>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTimer>

namespace desktop_app::v2 {
namespace {

struct ResolutionPreset
{
    int width;
    int height;
};

constexpr std::array<ResolutionPreset, 24> kResolutionPresets = {{
    {2304, 2304}, {2304, 1152}, {2304, 576}, {2304, 288},
    {2304, 144}, {2304, 72}, {2304, 36}, {2304, 16},
    {2304, 8}, {2304, 4}, {1152, 1152}, {1152, 576},
    {1152, 288}, {1152, 144}, {576, 576}, {576, 288},
    {576, 144}, {288, 288}, {288, 144}, {144, 144},
    {512, 128}, {512, 64}, {256, 64}, {256, 32},
}};
constexpr int customResolutionIndex = 20;

int presetIndex(int width, int height)
{
    for (int index = 0; index < static_cast<int>(kResolutionPresets.size());
         ++index) {
        if (kResolutionPresets[index].width == width
            && kResolutionPresets[index].height == height) {
            return index < customResolutionIndex ? index : index + 1;
        }
    }
    return customResolutionIndex;
}

QString statusText(int status)
{
    switch (static_cast<CameraStatus>(status)) {
    case CameraStatus::Unavailable:
        return QStringLiteral("Unavailable");
    case CameraStatus::Ready:
        return QStringLiteral("Connected");
    case CameraStatus::Streaming:
        return QStringLiteral("Streaming");
    case CameraStatus::Faulted:
        return QStringLiteral("Unavailable");
    }
    return QStringLiteral("Unavailable");
}

CameraFrame grayscale8Frame(CameraFrame frame, const QImage &image)
{
    frame.pixelFormat = CameraPixelFormat::Mono8;
    frame.bitDepth = 8;
    frame.rowBytes = image.width();
    frame.bytes.resize(static_cast<qsizetype>(image.width()) * image.height());
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(frame.bytes.data() + static_cast<qsizetype>(y) * image.width(),
                    image.constScanLine(y),
                    static_cast<size_t>(image.width()));
    }
    return frame;
}

CameraFrame adjustedGrayscale8Frame(CameraFrame frame, int low, int high)
{
    low = std::clamp(low, 0, 255);
    high = std::clamp(high, 0, 255);
    if ((low == 0 && high == 255) || high <= low)
        return frame;

    std::array<uchar, 256> lookup{};
    for (int value = 0; value < 256; ++value) {
        lookup[value] = static_cast<uchar>(
            value <= low ? 0
            : value >= high ? 255
            : (value - low) * 255 / (high - low));
    }

    frame.bytes.detach();
    for (int y = 0; y < frame.height; ++y) {
        uchar *row = reinterpret_cast<uchar *>(
            frame.bytes.data() + static_cast<qsizetype>(y) * frame.rowBytes);
        for (int x = 0; x < frame.width; ++x)
            row[x] = lookup[row[x]];
    }
    return frame;
}

int packedContrastRange(int low, int high)
{
    return (low << 8) | high;
}

int contrastLow(int range)
{
    return (range >> 8) & 0xff;
}

int contrastHigh(int range)
{
    return range & 0xff;
}

int percentile95(const CameraFrame &frame)
{
    std::array<qsizetype, 256> histogram{};
    qsizetype count = 0;
    for (int y = 0; y < frame.height; ++y) {
        const uchar *row = reinterpret_cast<const uchar *>(
            frame.bytes.constData() + static_cast<qsizetype>(y) * frame.rowBytes);
        for (int x = 0; x < frame.width; ++x) {
            ++histogram[row[x]];
            ++count;
        }
    }
    const qsizetype target = std::max<qsizetype>(1,
        static_cast<qsizetype>(std::ceil(static_cast<double>(count) * 0.95)));
    qsizetype cumulative = 0;
    for (int value = 0; value < 256; ++value) {
        cumulative += histogram[value];
        if (cumulative >= target)
            return value;
    }
    return 255;
}

} // namespace

CameraController::CameraController(CameraService &service,
                                   CameraPreviewImageProvider &previewProvider,
                                   QObject *parent)
    : QObject(parent)
    , service_(service)
    , previewProvider_(previewProvider)
    , status_(static_cast<int>(CameraStatus::Unavailable))
{
    connect(this, &CameraController::openRequested,
            &service_, &CameraService::open, Qt::QueuedConnection);
    connect(this, &CameraController::startRequested,
            &service_, &CameraService::start, Qt::QueuedConnection);
    connect(this, &CameraController::stopRequested,
            &service_, &CameraService::stop, Qt::QueuedConnection);
    connect(this, &CameraController::recoverRequested,
            &service_, &CameraService::recover, Qt::QueuedConnection);
    connect(this, &CameraController::closeRequested,
            &service_, &CameraService::close, Qt::QueuedConnection);
    connect(this, &CameraController::configurationRequested,
            &service_, &CameraService::applyConfiguration, Qt::QueuedConnection);
    connect(&service_, &CameraService::stateChanged,
            this, &CameraController::updateState, Qt::QueuedConnection);
    connect(&service_, &CameraService::frameReady,
            this, &CameraController::acceptFrame, Qt::DirectConnection);
    connect(&service_, &CameraService::frameError,
            this, &CameraController::setError, Qt::QueuedConnection);
    connect(&service_, &CameraService::configurationChanged,
            this, &CameraController::updateConfiguration, Qt::QueuedConnection);
    connect(&service_, &CameraService::exposureLimitsChanged,
            this, &CameraController::updateExposureLimits, Qt::QueuedConnection);
    autoExposureTimeout_.setSingleShot(true);
    connect(&autoExposureTimeout_, &QTimer::timeout, this, [this] {
        if (autoExposureActive())
            finishAutoExposure(QStringLiteral("Auto Exposure timed out after 3 seconds."));
    });
    connect(&service_, &CameraService::commandFinished, this,
            [this](bool success, const QString &error) {
                if (autoExposureApplyPending_) {
                    autoExposureApplyPending_ = false;
                    if (!success) {
                        setBusy(false);
                        if (autoExposureActive()) {
                            finishAutoExposure(error.isEmpty()
                                ? QStringLiteral("Auto Exposure could not apply the exposure.")
                                : error);
                        }
                        return;
                    }
                    if (autoExposureActive())
                        setError({});
                    setBusy(false);
                    return;
                }
                if (defaultBitDepthInitializationPending_) {
                    if (success && configurationAvailable_
                        && appliedSettings_.bitDepth != 8) {
                        CameraAppliedSettings requested = appliedSettings_;
                        requested.bitDepth = 8;
                        requested.pixelType = CameraPixelType::Mono8;
                        emit configurationRequested(requested);
                        return;
                    }
                    defaultBitDepthInitializationPending_ = false;
                    defaultBitDepthInitialized_ =
                        success && configurationAvailable_
                        && appliedSettings_.bitDepth == 8;
                }
                if (pendingExplicitBitDepth_) {
                    if (success && configurationAvailable_
                        && appliedSettings_.bitDepth
                            == *pendingExplicitBitDepth_) {
                        defaultBitDepthInitialized_ = true;
                    }
                    pendingExplicitBitDepth_.reset();
                }
                if (pendingCustomResolutionSelected_) {
                    if (success) {
                        customResolutionSelected_ = *pendingCustomResolutionSelected_;
                        emit stateChanged();
                    }
                    pendingCustomResolutionSelected_.reset();
                }
                if (profileApplyTimedOut_)
                    profileApplyTimedOut_ = false;
                else
                    setError(error);
                setBusy(false);
            },
            Qt::QueuedConnection);
}

QString CameraController::cameraStatus() const
{
    return statusText(status_);
}

QString CameraController::deviceId() const
{
    return deviceId_;
}

QString CameraController::error() const
{
    return serviceFault_.isEmpty() ? actionError_ : serviceFault_;
}

bool CameraController::streaming() const
{
    return static_cast<CameraStatus>(status_) == CameraStatus::Streaming;
}

bool CameraController::busy() const
{
    return busy_;
}

QString CameraController::previewSource() const
{
    return previewSource_;
}

bool CameraController::configurationAvailable() const
{
    return configurationAvailable_;
}

QString CameraController::resolution() const
{
    if (!configurationAvailable_)
        return {};
    if (customResolutionSelected_
        || presetIndex(appliedSettings_.width, appliedSettings_.height)
            == customResolutionIndex) {
        return QStringLiteral("Custom");
    }
    return QStringLiteral("%1 x %2")
        .arg(appliedSettings_.width)
        .arg(appliedSettings_.height);
}

QString CameraController::customWidth() const
{
    return configurationAvailable_ ? QString::number(appliedSettings_.width) : QString();
}

QString CameraController::customHeight() const
{
    return configurationAvailable_ ? QString::number(appliedSettings_.height) : QString();
}

QString CameraController::bitDepth() const
{
    return configurationAvailable_
        ? QStringLiteral("%1-bit").arg(appliedSettings_.bitDepth)
        : QString();
}

QString CameraController::exposureMs() const
{
    return configurationAvailable_
        ? QString::number(appliedSettings_.exposureMs, 'g', 12)
        : QString();
}

QString CameraController::readoutMode() const
{
    if (!configurationAvailable_)
        return {};
    return appliedSettings_.readoutMode == CameraReadoutMode::Fast
        ? QStringLiteral("Fast")
        : QStringLiteral("Slow");
}

QStringList CameraController::resolutionPresets() const
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(kResolutionPresets.size()) + 1);
    for (int index = 0; index <= static_cast<int>(kResolutionPresets.size());
         ++index) {
        if (index == customResolutionIndex) {
            result.append(QStringLiteral("Custom"));
            continue;
        }
        const int preset = index < customResolutionIndex ? index : index - 1;
        result.append(QStringLiteral("%1 x %2")
                          .arg(kResolutionPresets[preset].width)
                          .arg(kResolutionPresets[preset].height));
    }
    return result;
}

int CameraController::resolutionPresetIndex() const
{
    if (!configurationAvailable_)
        return -1;
    return customResolutionSelected_
        ? customResolutionIndex
        : presetIndex(appliedSettings_.width, appliedSettings_.height);
}

int CameraController::contrastMinimum() const
{
    return contrastLow(contrastRange_.load(std::memory_order_relaxed));
}

int CameraController::contrastMaximum() const
{
    return contrastHigh(contrastRange_.load(std::memory_order_relaxed));
}

bool CameraController::autoExposureActive() const
{
    return autoExposureActive_.load(std::memory_order_relaxed);
}

bool CameraController::hasFrame() const
{
    return hasFrame_;
}

quint64 CameraController::latestDeliveryId() const
{
    return latestDeliveryId_;
}

bool CameraController::open()
{
    const bool requested = request(&CameraController::openRequested);
    if (requested && !defaultBitDepthInitialized_)
        defaultBitDepthInitializationPending_ = true;
    return requested;
}

bool CameraController::start()
{
    return request(&CameraController::startRequested);
}

bool CameraController::stop()
{
    cancelAutoExposure();
    return request(&CameraController::stopRequested);
}

bool CameraController::recover()
{
    cancelAutoExposure();
    const bool requested = request(&CameraController::recoverRequested);
    if (requested && !defaultBitDepthInitialized_)
        defaultBitDepthInitializationPending_ = true;
    return requested;
}

bool CameraController::close()
{
    cancelAutoExposure();
    return request(&CameraController::closeRequested);
}

bool CameraController::applyResolution(int width, int height)
{
    CameraAppliedSettings requested = appliedSettings_;
    requested.width = width;
    requested.height = height;
    const bool custom = presetIndex(width, height) == customResolutionIndex;
    if (!requestConfiguration(requested))
        return false;
    pendingCustomResolutionSelected_ = custom;
    return true;
}

bool CameraController::selectResolutionPreset(int index)
{
    if (index == customResolutionIndex)
        return selectCustomResolution();
    if (index < 0 || index > static_cast<int>(kResolutionPresets.size())) {
        setError(QStringLiteral("Camera resolution preset is invalid."));
        return false;
    }
    const int preset = index < customResolutionIndex ? index : index - 1;
    return applyResolution(kResolutionPresets[preset].width,
                           kResolutionPresets[preset].height);
}

bool CameraController::selectCustomResolution()
{
    if (!configurationAvailable_ || busy_)
        return false;
    if (!customResolutionSelected_) {
        customResolutionSelected_ = true;
        emit stateChanged();
    }
    return true;
}

bool CameraController::applyBitDepth(int bitDepth)
{
    CameraAppliedSettings requested = appliedSettings_;
    requested.bitDepth = bitDepth;
    requested.pixelType =
        bitDepth == 8 ? CameraPixelType::Mono8 : CameraPixelType::Mono16;
    if (!requestConfiguration(requested))
        return false;
    pendingExplicitBitDepth_ = bitDepth;
    return true;
}

bool CameraController::applyExposureMs(double exposureMs)
{
    if (autoExposureActive()) {
        setError(QStringLiteral("Manual exposure is unavailable while Auto Exposure is active."));
        return false;
    }
    CameraAppliedSettings requested = appliedSettings_;
    requested.exposureMs = exposureMs;
    return requestConfiguration(requested);
}

bool CameraController::autoExposure()
{
    if (autoExposureActive())
        return false;
    if (!streaming() || !configurationAvailable_) {
        setError(QStringLiteral("Auto Exposure requires a streaming Camera."));
        return false;
    }
    if (busy_) {
        setError(QStringLiteral("Auto Exposure cannot start while the Camera is busy."));
        return false;
    }
    if (!exposureLimitsAvailable_) {
        setError(exposureLimitsError_.isEmpty()
            ? QStringLiteral("Camera exposure limits are not available.")
            : exposureLimitsError_);
        return false;
    }
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        autoExposureLastTimestampNs_ = latestUnadjustedFrame_
            ? latestUnadjustedFrame_->monotonicTimestampNs : 0;
    }
    autoExposureApplications_ = 0;
    autoExposureLastP95_ = -1;
    autoExposureNoProgress_ = 0;
    autoExposureApplyPending_ = false;
    autoExposureElapsed_.start();
    autoExposureTimeout_.start(3000);
    setError({});
    autoExposureActive_.store(true, std::memory_order_relaxed);
    emit autoExposureActiveChanged();
    return true;
}

void CameraController::cancelAutoExposure()
{
    if (autoExposureActive())
        finishAutoExposure();
}

bool CameraController::applyReadoutMode(const QString &readoutMode)
{
    CameraAppliedSettings requested = appliedSettings_;
    if (readoutMode == QStringLiteral("Fast"))
        requested.readoutMode = CameraReadoutMode::Fast;
    else if (readoutMode == QStringLiteral("Slow"))
        requested.readoutMode = CameraReadoutMode::Slow;
    else {
        setError(QStringLiteral("Camera readout mode must be Fast or Slow."));
        return false;
    }
    return requestConfiguration(requested);
}

bool CameraController::setContrastRange(int low, int high)
{
    if (low < 0 || high > 255 || low >= high) {
        setError(QStringLiteral("Contrast LOW must be less than HIGH within 0 to 255."));
        return false;
    }
    const int requestedRange = packedContrastRange(low, high);
    const int previousRange = contrastRange_.exchange(
        requestedRange, std::memory_order_relaxed);
    setError({});
    if (previousRange == requestedRange)
        return true;

    emit contrastChanged();

    std::optional<CameraFrame> latestFrame;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        latestFrame = latestUnadjustedFrame_;
    }
    std::optional<CameraFrame> adjustedPreview;
    if (latestFrame) {
        const QImage image = applyLinearContrast(
            convertCameraFrame(*latestFrame), low, high);
        adjustedPreview = grayscale8Frame(*latestFrame, image);
    }

    bool publishImmediately = false;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        if (adjustedPreview && latestUnadjustedFrame_
            && latestUnadjustedFrame_->deliveryId == adjustedPreview->deliveryId
            && contrastRange_.load(std::memory_order_relaxed) == requestedRange) {
            pendingPreviewFrame_ = std::move(adjustedPreview);
        }
        if (pendingPreviewFrame_ && !previewRevisionInFlight_ && !previewDeliveryScheduled_) {
            previewDeliveryScheduled_ = true;
            publishImmediately = true;
        }
    }
    if (publishImmediately)
        updateFrame();
    return true;
}

bool CameraController::autoContrast()
{
    QImage image;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        if (!latestUnadjustedFrame_) {
            setError(QStringLiteral("Auto Contrast requires a camera frame."));
            return false;
        }
        image = convertCameraFrame(*latestUnadjustedFrame_);
    }
    const auto range = autoContrastRange(image);
    return setContrastRange(range.first, range.second);
}

void CameraController::acknowledgePreviewReady(const QString &previewSource)
{
    if (previewSource != previewSource_)
        return;

    bool scheduleDelivery = false;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        if (!previewRevisionInFlight_)
            return;
        previewRevisionInFlight_ = false;
        if (pendingPreviewFrame_ && !previewDeliveryScheduled_) {
            previewDeliveryScheduled_ = true;
            scheduleDelivery = true;
        }
    }
    if (scheduleDelivery) {
        QMetaObject::invokeMethod(this, [this] { updateFrame(); },
                                  Qt::QueuedConnection);
    }
}

bool CameraController::applyProfileSettings(
    const CameraAppliedSettings &settings, int contrastMinimum, int contrastMaximum,
    int timeoutMs)
{
    if (!requestConfiguration(settings))
        return false;
    pendingExplicitBitDepth_ = settings.bitDepth;

    QEventLoop waitLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(this, &CameraController::busyChanged, &waitLoop, [this, &waitLoop] {
        if (!busy_)
            waitLoop.quit();
    });
    connect(&timeout, &QTimer::timeout, &waitLoop, &QEventLoop::quit);
    timeout.start(std::max(1, timeoutMs));
    if (busy_)
        waitLoop.exec();
    if (busy_) {
        profileApplyTimedOut_ = true;
        setError(QStringLiteral("Timed out waiting for the camera to apply the Setup Profile."));
        return false;
    }
    const bool accepted = error().isEmpty()
        && configurationAvailable_
        && appliedSettings_.width == settings.width
        && appliedSettings_.height == settings.height
        && appliedSettings_.bitDepth == settings.bitDepth
        && appliedSettings_.pixelType == settings.pixelType
        && std::abs(appliedSettings_.exposureMs - settings.exposureMs) < 1e-9
        && appliedSettings_.readoutMode == settings.readoutMode;
    if (!accepted) {
        if (error().isEmpty()) {
            setError(QStringLiteral(
                "The camera did not accept every value from the Setup Profile."));
        }
        return false;
    }
    return setContrastRange(contrastMinimum, contrastMaximum);
}

bool CameraController::request(void (CameraController::*signal)())
{
    if (busy_)
        return false;
    setError({});
    setBusy(true);
    emit (this->*signal)();
    return true;
}

bool CameraController::requestConfiguration(CameraAppliedSettings requested)
{
    if (!configurationAvailable_) {
        setError(QStringLiteral(
            "Camera configuration can only be changed while the camera is connected."));
        return false;
    }
    if (busy_)
        return false;
    setError({});
    setBusy(true);
    emit configurationRequested(requested);
    return true;
}

void CameraController::updateState(int status, const QString &deviceId,
                                   const QString &fault)
{
    const bool unavailable =
        static_cast<CameraStatus>(status) == CameraStatus::Unavailable
        || static_cast<CameraStatus>(status) == CameraStatus::Faulted;
    const bool projectionChanged =
        status_ != status || deviceId_ != deviceId || serviceFault_ != fault;
    status_ = status;
    deviceId_ = deviceId;
    serviceFault_ = fault;
    if (unavailable) {
        if (autoExposureActive()) {
            finishAutoExposure(fault.isEmpty()
                ? QStringLiteral("Auto Exposure stopped because the Camera became unavailable.")
                : fault);
        }
        configurationAvailable_ = false;
        hasFrame_ = false;
        latestDeliveryId_ = 0;
        {
            QMutexLocker locker(&pendingPreviewFrameMutex_);
            pendingPreviewFrame_.reset();
            latestUnadjustedFrame_.reset();
            previewDeliveryScheduled_ = false;
            previewRevisionInFlight_ = false;
        }
        if (!previewSource_.isEmpty()) {
            previewSource_.clear();
            emit previewSourceChanged();
        }
    }
    if (projectionChanged) {
        emit stateChanged();
        emit errorChanged();
    }
}

void CameraController::updateConfiguration(bool available,
                                           CameraAppliedSettings appliedSettings)
{
    const bool changed = configurationAvailable_ != available
        || appliedSettings_.width != appliedSettings.width
        || appliedSettings_.height != appliedSettings.height
        || appliedSettings_.bitDepth != appliedSettings.bitDepth
        || appliedSettings_.pixelType != appliedSettings.pixelType
        || appliedSettings_.exposureMs != appliedSettings.exposureMs
        || appliedSettings_.readoutMode != appliedSettings.readoutMode;
    configurationAvailable_ = available;
    appliedSettings_ = appliedSettings;
    if (!available)
        customResolutionSelected_ = false;
    if (changed)
        emit stateChanged();
}

void CameraController::updateExposureLimits(bool available,
                                            CameraExposureLimits limits,
                                            const QString &error)
{
    exposureLimitsAvailable_ = available
        && std::isfinite(limits.minimumMs) && std::isfinite(limits.maximumMs)
        && limits.minimumMs > 0.0 && limits.maximumMs >= limits.minimumMs;
    exposureLimits_ = limits;
    exposureLimitsError_ = exposureLimitsAvailable_ ? QString()
        : error.isEmpty() ? QStringLiteral("Camera reported invalid exposure limits.") : error;
    if (!exposureLimitsAvailable_ && autoExposureActive()) {
        finishAutoExposure(exposureLimitsError_.isEmpty()
            ? QStringLiteral("Camera exposure limits became unavailable.")
            : exposureLimitsError_);
    }
}

void CameraController::processAutoExposureFrame(qint64 monotonicTimestampNs)
{
    autoExposureFrameScheduled_.store(false, std::memory_order_release);
    if (!autoExposureActive() || autoExposureApplyPending_ || busy_
        || autoExposureElapsed_.elapsed() >= 3000
        || monotonicTimestampNs <= autoExposureLastTimestampNs_) {
        return;
    }

    CameraFrame frame;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        if (!latestUnadjustedFrame_
            || latestUnadjustedFrame_->monotonicTimestampNs
                <= autoExposureLastTimestampNs_) {
            return;
        }
        frame = *latestUnadjustedFrame_;
    }
    autoExposureLastTimestampNs_ = frame.monotonicTimestampNs;
    const int p95 = percentile95(frame);
    if (std::abs(p95 - 180) <= 8) {
        finishAutoExposure();
        return;
    }
    if (p95 == autoExposureLastP95_)
        ++autoExposureNoProgress_;
    else
        autoExposureNoProgress_ = 0;
    autoExposureLastP95_ = p95;
    if (autoExposureNoProgress_ >= 2) {
        finishAutoExposure(QStringLiteral("Auto Exposure stopped because image brightness did not change."));
        return;
    }
    if (autoExposureApplications_ >= 8) {
        finishAutoExposure(QStringLiteral("Auto Exposure stopped after 8 exposure adjustments."));
        return;
    }

    const double factor = p95 == 0 ? 2.0
        : std::clamp(180.0 / static_cast<double>(p95), 0.5, 2.0);
    const double current = appliedSettings_.exposureMs;
    const double candidate = std::clamp(current * factor,
        exposureLimits_.minimumMs, exposureLimits_.maximumMs);
    if (std::abs(candidate - current) <= 1e-9) {
        finishAutoExposure(QStringLiteral("Auto Exposure reached the Camera exposure limit before convergence."));
        return;
    }

    CameraAppliedSettings requested = appliedSettings_;
    requested.exposureMs = candidate;
    autoExposureApplyPending_ = true;
    ++autoExposureApplications_;
    if (!requestConfiguration(requested)) {
        autoExposureApplyPending_ = false;
        finishAutoExposure(error().isEmpty()
            ? QStringLiteral("Auto Exposure could not apply the next exposure.") : error());
    }
}

void CameraController::finishAutoExposure(const QString &error)
{
    if (!autoExposureActive())
        return;
    autoExposureTimeout_.stop();
    autoExposureActive_.store(false, std::memory_order_relaxed);
    if (!error.isNull())
        setError(error);
    emit autoExposureActiveChanged();
}

void CameraController::acceptFrame(CameraFrame frame)
{
    QString conversionError;
    const QImage unadjusted = convertCameraFrame(frame, &conversionError);
    if (unadjusted.isNull()) {
        setError(conversionError);
        return;
    }
    const CameraFrame normalized = grayscale8Frame(std::move(frame), unadjusted);
    const int contrastRange = contrastRange_.load(std::memory_order_relaxed);
    const CameraFrame adjusted = adjustedGrayscale8Frame(
        normalized, contrastLow(contrastRange), contrastHigh(contrastRange));
    emit frameReady(adjusted);

    bool queueDelivery = false;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        latestUnadjustedFrame_ = normalized;
        pendingPreviewFrame_ = adjusted;
        if (!previewDeliveryScheduled_ && !previewRevisionInFlight_) {
            previewDeliveryScheduled_ = true;
            queueDelivery = true;
        }
    }
    if (autoExposureActive()
        && !autoExposureFrameScheduled_.exchange(true, std::memory_order_acq_rel)) {
        QMetaObject::invokeMethod(this, [this, timestamp = normalized.monotonicTimestampNs] {
            processAutoExposureFrame(timestamp);
        }, Qt::QueuedConnection);
    }
    if (queueDelivery) {
        QMetaObject::invokeMethod(this, [this] { updateFrame(); },
                                  Qt::QueuedConnection);
    }
}

void CameraController::updateFrame()
{
    std::optional<CameraFrame> frame;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        previewDeliveryScheduled_ = false;
        if (previewRevisionInFlight_)
            return;
        if (!pendingPreviewFrame_) {
            return;
        }
        frame = std::move(pendingPreviewFrame_);
        pendingPreviewFrame_.reset();
        previewRevisionInFlight_ = true;
    }

    setError({});
    quint64 revision = 0;
    if (frame) {
        latestDeliveryId_ = frame->deliveryId;
        hasFrame_ = true;
        revision = previewProvider_.updateFrame(std::move(*frame));
    }
    previewSource_ =
        QStringLiteral("image://camera-preview/frame?r=%1").arg(revision);
    emit previewSourceChanged();
}

void CameraController::setError(const QString &error)
{
    if (actionError_ == error) {
        return;
    }
    actionError_ = error;
    emit errorChanged();
}

void CameraController::setBusy(bool busy)
{
    if (busy_ == busy)
        return;
    busy_ = busy;
    emit busyChanged();
    emit stateChanged();
}

} // namespace desktop_app::v2
