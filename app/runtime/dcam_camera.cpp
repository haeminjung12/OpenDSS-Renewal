#include "dcam_camera.h"

#include <cmath>
#include <cstdio>

DcamCamera::DcamCamera()
    : hdcam_(nullptr),
      hwait_(nullptr),
      opened_(false),
      bufferCount_(16),
      frameCounter_(0) {}

DcamCamera::~DcamCamera() {
    cleanup();
}

std::string DcamCamera::init(int deviceIndex) {
    cleanup();

    DCAMAPI_INIT api = {};
    api.size = sizeof(api);
    int32 initOptions[] = {
        DCAMAPI_INITOPTION_APIVER__LATEST,
        DCAMAPI_INITOPTION_ENDMARK
    };
    api.initoption = initOptions;
    api.initoptionbytes = sizeof(initOptions);
    DCAMERR err = dcamapi_init(&api);
    if (failed(err)) return errText("dcamapi_init", err);
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
    if (!opened_) return "Camera not opened";

    stop();
    dcambuf_release(hdcam_);
    bufferCount_ = settings.bufferCount > 0 ? settings.bufferCount : bufferCount_;

    auto setProp = [&](int32 id, double v, const char* label)->std::string {
        DCAMERR err = dcamprop_setvalue(hdcam_, id, v);
        if (failed(err)) return errText(label, err);
        return {};
    };

    std::string warn;

    if (settings.enableSubarray && settings.width > 0 && settings.height > 0) {
        setProp(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF, "subarray off");
        if (!setProp(DCAM_IDPROP_SUBARRAYHPOS, 0, "subarray hpos").empty()) warn = "subarray hpos";
        if (!setProp(DCAM_IDPROP_SUBARRAYVPOS, 0, "subarray vpos").empty()) warn = "subarray vpos";
        if (!setProp(DCAM_IDPROP_SUBARRAYHSIZE, settings.width, "subarray hsize").empty()) warn = "subarray hsize";
        if (!setProp(DCAM_IDPROP_SUBARRAYVSIZE, settings.height, "subarray vsize").empty()) warn = "subarray vsize";
        if (!setProp(DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON, "subarray on").empty()) warn = "subarray on";
    }

    if (settings.binning > 0) {
        std::string e = setProp(DCAM_IDPROP_BINNING, settings.binning, "binning");
        if (!e.empty()) warn = "binning";
    }

    if (settings.pixelType > 0) {
        std::string e = setProp(DCAM_IDPROP_IMAGE_PIXELTYPE, settings.pixelType, "pixel type");
        if (!e.empty()) warn = "pixel type";
    }
    if (settings.bits > 0) {
        std::string e = setProp(DCAM_IDPROP_BITSPERCHANNEL, settings.bits, "bits");
        if (!e.empty()) warn = "bits";
    }

    if (settings.readoutSpeed != 0) {
        setProp(DCAM_IDPROP_READOUTSPEED, settings.readoutSpeed, "readout speed");
    }
    if (settings.exposureMs > 0) {
        setProp(DCAM_IDPROP_EXPOSURETIME, settings.exposureMs / 1000.0, "exposure");
    }
    if (settings.triggerSource > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGERSOURCE, settings.triggerSource, "trigger source");
        if (!e.empty()) warn = "trigger source";
    }
    if (settings.triggerMode > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGER_MODE, settings.triggerMode, "trigger mode");
        if (!e.empty()) warn = "trigger mode";
    }
    if (settings.triggerActive > 0) {
        std::string e = setProp(DCAM_IDPROP_TRIGGERACTIVE, settings.triggerActive, "trigger active");
        if (!e.empty()) warn = "trigger active";
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

std::string DcamCamera::start() {
    if (!opened_) return "Camera not opened";
    DCAMERR err = dcamcap_start(hdcam_, DCAMCAP_START_SEQUENCE);
    if (failed(err)) return errText("dcamcap_start", err);
    return {};
}

void DcamCamera::stop() {
    if (opened_) dcamcap_stop(hdcam_);
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
    if (!opened_) return false;
    DCAMWAIT_START wait = {};
    wait.size = sizeof(wait);
    wait.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    wait.timeout = timeoutMs;
    return !failed(dcamwait_start(hwait_, &wait));
}

bool DcamCamera::getLatestFrame(FrameData& out) {
    if (!opened_) return false;

    DCAMBUF_FRAME bf = {};
    bf.size = sizeof(bf);
    bf.iFrame = -1;
    DCAMERR err = dcambuf_lockframe(hdcam_, &bf);
    if (failed(err)) return false;

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
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s failed: 0x%08X", label, static_cast<unsigned int>(err));
    return std::string(buf);
}
