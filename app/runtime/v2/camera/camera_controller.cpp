#include "camera_controller.h"

#include "camera_preview_image_provider.h"
#include "camera_service.h"

#include <algorithm>
#include <array>
#include <cmath>

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
    connect(&service_, &CameraService::commandFinished, this,
            [this](bool success, const QString &error) {
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

int CameraController::previewLutMinimum() const
{
    return previewLutMinimum_;
}

int CameraController::previewLutMaximum() const
{
    return previewLutMaximum_;
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
    return request(&CameraController::openRequested);
}

bool CameraController::start()
{
    return request(&CameraController::startRequested);
}

bool CameraController::stop()
{
    return request(&CameraController::stopRequested);
}

bool CameraController::recover()
{
    return request(&CameraController::recoverRequested);
}

bool CameraController::close()
{
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
    return requestConfiguration(requested);
}

bool CameraController::applyExposureMs(double exposureMs)
{
    CameraAppliedSettings requested = appliedSettings_;
    requested.exposureMs = exposureMs;
    return requestConfiguration(requested);
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

void CameraController::setPreviewLutRange(int blackLevel, int whiteLevel)
{
    blackLevel = std::clamp(blackLevel, 0, 255);
    whiteLevel = std::clamp(whiteLevel, 0, 255);
    if (whiteLevel < blackLevel)
        whiteLevel = blackLevel;
    if (previewLutMinimum_ == blackLevel && previewLutMaximum_ == whiteLevel)
        return;

    previewLutMinimum_ = blackLevel;
    previewLutMaximum_ = whiteLevel;
    const quint64 revision =
        previewProvider_.setPreviewLutRange(blackLevel, whiteLevel);
    emit previewLutChanged();
    if (hasFrame_) {
        {
            QMutexLocker locker(&pendingPreviewFrameMutex_);
            previewRevisionInFlight_ = true;
        }
        previewSource_ =
            QStringLiteral("image://camera-preview/frame?r=%1").arg(revision);
        emit previewSourceChanged();
    }
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
    const CameraAppliedSettings &settings, int lutMinimum, int lutMaximum,
    int timeoutMs)
{
    if (!requestConfiguration(settings))
        return false;

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
    setPreviewLutRange(lutMinimum, lutMaximum);
    return true;
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
        configurationAvailable_ = false;
        hasFrame_ = false;
        latestDeliveryId_ = 0;
        {
            QMutexLocker locker(&pendingPreviewFrameMutex_);
            pendingPreviewFrame_.reset();
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

void CameraController::acceptFrame(CameraFrame frame)
{
    emit frameReady(frame);

    bool queueDelivery = false;
    {
        QMutexLocker locker(&pendingPreviewFrameMutex_);
        pendingPreviewFrame_ = std::move(frame);
        if (!previewDeliveryScheduled_ && !previewRevisionInFlight_) {
            previewDeliveryScheduled_ = true;
            queueDelivery = true;
        }
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
    latestDeliveryId_ = frame->deliveryId;
    hasFrame_ = true;
    const quint64 revision = previewProvider_.updateFrame(std::move(*frame));
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
