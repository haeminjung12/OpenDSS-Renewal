#include "dcam_camera.h"

#include <cmath>
#include <cstdio>

DcamCamera::DcamCamera() : hdcam_(nullptr), hwait_(nullptr), opened_(false), bufferCount_(16), frameCounter_(0) {}

DcamCamera::~DcamCamera() {
    cleanup();
}

std::string DcamCamera::init(int deviceIndex) {
    cleanup();

    DCAMAPI_INIT api = {};
    api.size = sizeof(api);
    int32 initOptions[] = {DCAMAPI_INITOPTION_APIVER__LATEST, DCAMAPI_INITOPTION_ENDMARK};
    api.initoption = initOptions;
    api.initoptionbytes = sizeof(initOptions);
    DCAMERR err = dcamapi_init(&api);
    if (failed(err))
        return errText("dcamapi_init", err);
    if (api.iDeviceCount <= 0) {
        dcamapi_uninit();
        return "dcamapi_init: no camera detected (device count 0)";
    }
    if (deviceIndex < 0 || deviceIndex >= api.iDeviceCount) {
        dcamapi_uninit();
        return "dcamapi_init: device index out of range (count " + std::to_string(api.iDeviceCount) + ")";
    }

    DCAMDEV_OPEN dev = {};
    dev.size = sizeof(dev);
    dev.index = deviceIndex;
    err = dcamdev_open(&dev);
    if (failed(err)) {
        dcamapi_uninit();
        return errText("dcamdev_open", err);
    }
    hdcam_ = dev.hdcam;

    DCAMWAIT_OPEN w = {};
    w.size = sizeof(w);
    w.hdcam = hdcam_;
    err = dcamwait_open(&w);
    if (failed(err)) {
        dcamdev_close(hdcam_);
        dcamapi_uninit();
        hdcam_ = nullptr;
        return errText("dcamwait_open", err);
    }
    hwait_ = w.hwait;

    err = dcambuf_alloc(hdcam_, bufferCount_);
    if (failed(err)) {
        std::string msg = errText("dcambuf_alloc", err);
        cleanup();
        return msg;
    }

    opened_ = true;
    frameCounter_ = 0;
    return {};
}

std::string DcamCamera::apply(const CameraSettings& settings) {
    if (!opened_)
        return "Camera not opened";

    stop();
    dcambuf_release(hdcam_);
    bufferCount_ = settings.bufferCount > 0 ? settings.bufferCount : bufferCount_;

    auto setProp = [&](int32 id, double v, const char* label) -> std::string {
        DCAMERR err = dcamprop_setvalue(hdcam_, id, v);
        if (failed(err))
            return errText(label, err);
        return {};
    };

    std::string warn;

    if (settings.enableSubarray && settings.width > 0 && settings.height > 0) {
        setProp(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF, "subarray off");
        if (!setProp(DCAM_IDPROP_SUBARRAYHPOS, 0, "subarray hpos").empty())
            warn = "subarray hpos";
        if (!setProp(DCAM_IDPROP_SUBARRAYVPOS, 0, "subarray vpos").empty())
            warn = "subarray vpos";
        if (!setProp(DCAM_IDPROP_SUBARRAYHSIZE, settings.width, "subarray hsize").empty())
            warn = "subarray hsize";
        if (!setProp(DCAM_IDPROP_SUBARRAYVSIZE, settings.height, "subarray vsize").empty())
            warn = "subarray vsize";
        if (!setProp(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON, "subarray on").empty())
            warn = "subarray on";
    }

    if (settings.binning > 0) {
        std::string e = setProp(DCAM_IDPROP_BINNING, settings.binning, "binning");
        if (!e.empty())
            warn = "binning";
    }

    if (settings.pixelType > 0) {
        std::string e = setProp(DCAM_IDPROP_IMAGE_PIXELTYPE, settings.pixelType, "pixel type");
        if (!e.empty())
            warn = "pixel type";
    }
    if (settings.bits > 0) {
        std::string e = setProp(DCAM_IDPROP_BITSPERCHANNEL, settings.bits, "bits");
        if (!e.empty())
            warn = "bits";
    }

    if (settings.readoutSpeed != 0) {
        setProp(DCAM_IDPROP_READOUTSPEED, settings.readoutSpeed, "readout speed");
    }
    if (settings.exposureMs > 0) {
        setProp(DCAM_IDPROP_EXPOSURETIME, settings.exposureMs / 1000.0, "exposure");
    }
    if (settings.triggerSource > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGERSOURCE, settings.triggerSource, "trigger source");
        if (!e.empty())
            warn = "trigger source";
    }
    if (settings.triggerMode > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGER_MODE, settings.triggerMode, "trigger mode");
        if (!e.empty())
            warn = "trigger mode";
    }
    if (settings.triggerActive > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGERACTIVE, settings.triggerActive, "trigger active");
        if (!e.empty())
            warn = "trigger active";
    }

    if (settings.bundleEnabled) {
        if (!setProp(DCAM_IDPROP_FRAMEBUNDLE_MODE, DCAMPROP_MODE__ON, "bundle mode").empty()) {
            warn = "bundle mode";
        } else if (settings.bundleCount > 0) {
            if (!setProp(DCAM_IDPROP_FRAMEBUNDLE_NUMBER, settings.bundleCount, "bundle count").empty()) {
                warn = "bundle count";
            }
        }
    } else {
        setProp(DCAM_IDPROP_FRAMEBUNDLE_MODE, DCAMPROP_MODE__OFF, "bundle mode off");
    }

    if (failed(dcambuf_alloc(hdcam_, bufferCount_))) {
        return "buffer alloc failed after apply";
    }

    frameCounter_ = 0;
    return warn.empty() ? std::string() : ("WARN: " + warn);
}

std::string DcamCamera::applyApprovedSettings(const CameraSettings& settings) {
    if (!opened_)
        return "Camera not opened";

    stop();
    const DCAMERR releaseError = dcambuf_release(hdcam_);
    if (failed(releaseError))
        return errText("dcambuf_release", releaseError);

    std::string firstError;
    auto setRequired = [&](int32 id, double value, const char* label) {
        const DCAMERR err = dcamprop_setvalue(hdcam_, id, value);
        if (firstError.empty() && failed(err))
            firstError = errText(label, err);
    };
    setRequired(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF, "subarray off");
    setRequired(DCAM_IDPROP_SUBARRAYHPOS, 0, "subarray hpos");
    setRequired(DCAM_IDPROP_SUBARRAYVPOS, 0, "subarray vpos");
    setRequired(DCAM_IDPROP_SUBARRAYHSIZE, settings.width, "subarray hsize");
    setRequired(DCAM_IDPROP_SUBARRAYVSIZE, settings.height, "subarray vsize");
    setRequired(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON, "subarray on");
    setRequired(DCAM_IDPROP_IMAGE_PIXELTYPE, settings.pixelType, "pixel type");
    setRequired(DCAM_IDPROP_BITSPERCHANNEL, settings.bits, "bits");
    setRequired(DCAM_IDPROP_EXPOSURETIME, settings.exposureMs / 1000.0, "exposure");
    setRequired(DCAM_IDPROP_READOUTSPEED, settings.readoutSpeed, "readout speed");

    const DCAMERR allocError = dcambuf_alloc(hdcam_, bufferCount_);
    if (firstError.empty() && failed(allocError))
        firstError = errText("dcambuf_alloc", allocError);
    frameCounter_ = 0;
    return firstError;
}

CameraSettingsSupport DcamCamera::approvedSettingsSupport(std::string& error) const {
    error.clear();
    if (!opened_) {
        error = "Camera not opened";
        return CameraSettingsSupport::Error;
    }

    struct Requirement {
        int32 id;
        bool readable;
        const char* label;
    };
    const Requirement requirements[] = {
        {DCAM_IDPROP_SUBARRAYMODE, false, "subarray mode"},
        {DCAM_IDPROP_SUBARRAYHPOS, false, "subarray horizontal position"},
        {DCAM_IDPROP_SUBARRAYVPOS, false, "subarray vertical position"},
        {DCAM_IDPROP_SUBARRAYHSIZE, true, "subarray width"},
        {DCAM_IDPROP_SUBARRAYVSIZE, true, "subarray height"},
        {DCAM_IDPROP_IMAGE_PIXELTYPE, true, "pixel type"},
        {DCAM_IDPROP_BITSPERCHANNEL, true, "bits per channel"},
        {DCAM_IDPROP_EXPOSURETIME, true, "exposure"},
        {DCAM_IDPROP_READOUTSPEED, true, "readout speed"},
    };
    for (const Requirement& requirement : requirements) {
        DCAMPROP_ATTR attribute = {};
        attribute.cbSize = sizeof(attribute);
        attribute.iProp = requirement.id;
        const DCAMERR result = dcamprop_getattr(hdcam_, &attribute);
        if (failed(result)) {
            if (result == DCAMERR_INVALIDPROPERTYID || result == DCAMERR_NOTSUPPORT)
                return CameraSettingsSupport::Unsupported;
            error = errText(requirement.label, result);
            return CameraSettingsSupport::Error;
        }
        const bool writable = (attribute.attribute & DCAMPROP_ATTR_WRITABLE) != 0;
        const bool readable = (attribute.attribute & DCAMPROP_ATTR_READABLE) != 0;
        if (!writable || (requirement.readable && !readable))
            return CameraSettingsSupport::Unsupported;
    }
    return CameraSettingsSupport::Supported;
}

std::string DcamCamera::readApprovedSettings(CameraSettings& settings) const {
    if (!opened_)
        return "Camera not opened";

    auto getProp = [&](int32 id, double& value, const char* label) -> std::string {
        const DCAMERR err = dcamprop_getvalue(hdcam_, id, &value);
        if (failed(err))
            return errText(label, err);
        return {};
    };

    double width = 0.0;
    double height = 0.0;
    double bits = 0.0;
    double pixelType = 0.0;
    double exposureSeconds = 0.0;
    double readoutSpeed = 0.0;
    const struct {
        int32 id;
        double* value;
        const char* label;
    } properties[] = {
        {DCAM_IDPROP_SUBARRAYHSIZE, &width, "subarray hsize readback"},
        {DCAM_IDPROP_SUBARRAYVSIZE, &height, "subarray vsize readback"},
        {DCAM_IDPROP_BITSPERCHANNEL, &bits, "bits readback"},
        {DCAM_IDPROP_IMAGE_PIXELTYPE, &pixelType, "pixel type readback"},
        {DCAM_IDPROP_EXPOSURETIME, &exposureSeconds, "exposure readback"},
        {DCAM_IDPROP_READOUTSPEED, &readoutSpeed, "readout speed readback"},
    };
    for (const auto& property : properties) {
        const std::string error = getProp(property.id, *property.value, property.label);
        if (!error.empty())
            return error;
    }

    settings.width = static_cast<int>(std::lround(width));
    settings.height = static_cast<int>(std::lround(height));
    settings.bits = static_cast<int>(std::lround(bits));
    settings.pixelType = static_cast<int>(std::lround(pixelType));
    settings.exposureMs = exposureSeconds * 1000.0;
    settings.readoutSpeed = static_cast<int>(std::lround(readoutSpeed));
    return {};
}

std::string DcamCamera::start() {
    if (!opened_)
        return "Camera not opened";
    DCAMERR err = dcamcap_start(hdcam_, DCAMCAP_START_SEQUENCE);
    if (failed(err))
        return errText("dcamcap_start", err);
    return {};
}

void DcamCamera::stop() {
    if (opened_)
        dcamcap_stop(hdcam_);
}

void DcamCamera::cleanup() {
    if (opened_) {
        dcamcap_stop(hdcam_);
    }
    if (hdcam_) {
        dcambuf_release(hdcam_);
    }
    if (hwait_) {
        dcamwait_close(hwait_);
    }
    if (hdcam_) {
        dcamdev_close(hdcam_);
    }
    dcamapi_uninit();
    hwait_ = nullptr;
    hdcam_ = nullptr;
    opened_ = false;
    frameCounter_ = 0;
}

bool DcamCamera::isOpened() const {
    return opened_;
}

bool DcamCamera::waitForFrame(int timeoutMs) {
    if (!opened_)
        return false;
    DCAMWAIT_START wait = {};
    wait.size = sizeof(wait);
    wait.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    wait.timeout = timeoutMs;
    return !failed(dcamwait_start(hwait_, &wait));
}

bool DcamCamera::getLatestFrame(FrameData& out) {
    if (!opened_)
        return false;

    DCAMBUF_FRAME bf = {};
    bf.size = sizeof(bf);
    bf.iFrame = -1;
    DCAMERR err = dcambuf_lockframe(hdcam_, &bf);
    if (failed(err))
        return false;

    FrameMeta meta;
    meta.width = static_cast<int>(bf.width);
    meta.height = static_cast<int>(bf.height);
    meta.rowBytes = static_cast<int>(bf.rowbytes);
    meta.frameIndex = frameCounter_++;

    double bin = 1.0;
    double bits = 0.0;
    dcamprop_getvalue(hdcam_, DCAM_IDPROP_BINNING, &bin);
    dcamprop_getvalue(hdcam_, DCAM_IDPROP_BITSPERCHANNEL, &bits);
    meta.binning = bin;
    meta.bits = static_cast<int>(std::lround(bits));

    DCAMCAP_TRANSFERINFO ti = {};
    ti.size = sizeof(ti);
    if (!failed(dcamcap_transferinfo(hdcam_, &ti))) {
        meta.delivered = ti.nFrameCount;
        meta.dropped = 0;
    }

    double fps = 0.0;
    double rds = 0.0;
    dcamprop_getvalue(hdcam_, DCAM_IDPROP_INTERNALFRAMERATE, &fps);
    dcamprop_getvalue(hdcam_, DCAM_IDPROP_READOUTSPEED, &rds);
    meta.internalFps = fps;
    meta.readoutSpeed = rds;

    int type = (meta.bits <= 8) ? CV_8UC1 : CV_16UC1;
    cv::Mat img(meta.height, meta.width, type, bf.buf, bf.rowbytes);
    out.image = img.clone();
    out.meta = meta;
    return true;
}

std::string DcamCamera::errText(const char* label, DCAMERR err) const {
    if (err == DCAMERR_NOCAMERA) {
        return std::string(label) + " failed: no camera (0x80000206)";
    }
    if (err == DCAMERR_TEMPERATURE_TROUBLE) {
        return std::string(label)
            + " failed: camera temperature trouble; allow the camera to stabilize and retry"
              " (0x80000304)";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s failed: 0x%08X", label, static_cast<unsigned int>(err));
    return std::string(buf);
}
