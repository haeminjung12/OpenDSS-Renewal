#include "dcam_controller.h"
#include <QtGui/QImage>
#include <QtCore/QDebug>

#include <cmath>

DcamController::DcamController(QObject* parent)
    : QObject(parent), hdcam(nullptr), hwait(nullptr), opened(false), frameCounter(0) {}

DcamController::~DcamController() {
    cleanup();
}

QString DcamController::initAndOpen() {
    cleanup();
    DCAMAPI_INIT api = {};
    api.size = sizeof(api);
    DCAMERR err = dcamapi_init(&api);
    if (failed(err))
        return errText("dcamapi_init", err);

    DCAMDEV_OPEN dev = {};
    dev.size = sizeof(dev);
    dev.index = 0;
    err = dcamdev_open(&dev);
    if (failed(err)) {
        dcamapi_uninit();
        return errText("dcamdev_open", err);
    }
    hdcam = dev.hdcam;

    DCAMWAIT_OPEN w = {};
    w.size = sizeof(w);
    w.hdcam = hdcam;
    err = dcamwait_open(&w);
    if (failed(err)) {
        dcamdev_close(hdcam);
        dcamapi_uninit();
        hdcam = nullptr;
        return errText("dcamwait_open", err);
    }
    hwait = w.hwait;

    err = dcambuf_alloc(hdcam, 16);
    if (failed(err)) {
        QString msg = errText("dcambuf_alloc", err);
        cleanup();
        return msg;
    }
    opened = true;
    frameCounter = 0;
    return {};
}

QString DcamController::reconnect() {
    cleanup();
    return initAndOpen();
}

QString DcamController::start() {
    if (!opened)
        return "Camera not opened";
    DCAMERR err = dcamcap_start(hdcam, DCAMCAP_START_SEQUENCE);
    if (failed(err))
        return errText("dcamcap_start", err);
    return {};
}

void DcamController::stop() {
    if (opened)
        dcamcap_stop(hdcam);
}

void DcamController::cleanup() {
    if (opened) {
        dcamcap_stop(hdcam);
    }
    if (hdcam) {
        dcambuf_release(hdcam);
    }
    if (hwait) {
        dcamwait_close(hwait);
    }
    if (hdcam) {
        dcamdev_close(hdcam);
    }
    dcamapi_uninit();
    hwait = nullptr;
    hdcam = nullptr;
    opened = false;
    frameCounter = 0;
}

QString DcamController::apply(const ApplySettings& s) {
    return configure(s, true);
}

QString DcamController::configure(const ApplySettings& s, bool startAfterApply) {
    if (!opened)
        return "Camera not opened";
    stop();

    QStringList warnings;

    auto errTextIf = [&](const QString& label, DCAMERR e) -> QString {
        return failed(e) ? errText(label, e) : QString();
    };

    auto setBinningSample = [&](int32 bin) -> QString {
        double v = bin;
        DCAMERR err = dcamprop_queryvalue(hdcam, DCAM_IDPROP_BINNING, &v);
        if (failed(err))
            return errText("query binning", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING, bin);
        if (failed(err))
            return errText("set binning", err);
        return {};
    };

    auto setSubarraySample = [&](int32 hpos, int32 hsize, int32 vpos, int32 vsize) -> QString {
        DCAMERR err;
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF);
        if (failed(err))
            return errText("set subarray off", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYHPOS, hpos);
        if (failed(err))
            return errText("set hpos", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYHSIZE, hsize);
        if (failed(err))
            return errText("set hsize", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYVPOS, vpos);
        if (failed(err))
            return errText("set vpos", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYVSIZE, vsize);
        if (failed(err))
            return errText("set vsize", err);
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON);
        if (failed(err))
            return errText("set subarray on", err);
        return {};
    };

    auto setAndReadback = [&](int32 prop, double requested, const QString& name, bool requireExact,
                              bool requireQuery) -> QString {
        double queried = requested;
        const DCAMERR queryErr = dcamprop_queryvalue(hdcam, prop, &queried);
        const bool queryOk = !failed(queryErr);
        const DCAMERR setErr = dcamprop_setvalue(hdcam, prop, requested);
        const bool setOk = !failed(setErr);
        double readback = 0.0;
        const DCAMERR readErr = dcamprop_getvalue(hdcam, prop, &readback);
        const bool readOk = !failed(readErr);
        qInfo().noquote()
            << QString("DCAM apply %1: request=%2 query=%3 set=%4 readback=%5")
                   .arg(name)
                   .arg(requested, 0, 'f', 3)
                   .arg(queryOk ? QString::number(queried, 'f', 3) : errText(QStringLiteral("query ") + name, queryErr))
                   .arg(setOk ? QStringLiteral("ok") : errText(QStringLiteral("set ") + name, setErr))
                   .arg(readOk ? QString::number(readback, 'f', 3)
                               : errText(QStringLiteral("readback ") + name, readErr));
        if (requireQuery && !queryOk) {
            return errText(QStringLiteral("query ") + name, queryErr);
        }
        if (!setOk) {
            return errText(QStringLiteral("set ") + name, setErr);
        }
        if (!readOk) {
            return errText(QStringLiteral("readback ") + name, readErr);
        }
        if (requireExact && std::lround(readback) != std::lround(requested)) {
            return QString("readback %1 mismatch: requested %2, got %3")
                .arg(name)
                .arg(requested, 0, 'f', 0)
                .arg(readback, 0, 'f', 0);
        }
        return {};
    };

    // Release buffers before changing ROI/binning.
    dcambuf_release(hdcam);

    // Binning first
    if (s.binningIndependent) {
        DCAMERR err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING_INDEPENDENT, DCAMPROP_MODE__ON);
        if (failed(err)) {
            warnings << errText("set binning independent on", err);
        } else {
            if (s.binH > 0) {
                err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING_HORZ, s.binH);
                if (failed(err))
                    warnings << errText("set binning horz", err);
            }
            if (s.binV > 0) {
                err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING_VERT, s.binV);
                if (failed(err))
                    warnings << errText("set binning vert", err);
            }
        }
    } else {
        dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING_INDEPENDENT, DCAMPROP_MODE__OFF);
        if (s.binning > 0) {
            QString e = setBinningSample(static_cast<int32>(s.binning));
            if (!e.isEmpty())
                warnings << e;
        }
    }

    // ROI
    if (s.enableSubarray && s.width > 0 && s.height > 0) {
        QString e = setSubarraySample(0, s.width, 0, s.height);
        if (!e.isEmpty())
            warnings << e;
    }

    // Pixel type / bits
    if (s.pixelType > 0) {
        QString e =
            setAndReadback(DCAM_IDPROP_IMAGE_PIXELTYPE, s.pixelType, QStringLiteral("pixeltype"), true, true);
        if (!e.isEmpty())
            warnings << e;
    }
    if (s.bits > 0) {
        QString e = setAndReadback(DCAM_IDPROP_BITSPERCHANNEL, s.bits, QStringLiteral("bits"), true, true);
        if (!e.isEmpty())
            warnings << e;
    }

    if (s.readoutSpeed != 0) {
        QString e = setAndReadback(DCAM_IDPROP_READOUTSPEED, s.readoutSpeed, QStringLiteral("readout"), false, false);
        if (!e.isEmpty())
            warnings << e;
    }
    if (s.exposure_s > 0) {
        dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, s.exposure_s);
    }
    if (s.bundleEnabled) {
        if (failed(dcamprop_setvalue(hdcam, DCAM_IDPROP_FRAMEBUNDLE_MODE, DCAMPROP_MODE__ON))) {
            warnings << "frame bundle mode unsupported";
        } else if (s.bundleCount > 0) {
            if (failed(dcamprop_setvalue(hdcam, DCAM_IDPROP_FRAMEBUNDLE_NUMBER, s.bundleCount))) {
                warnings << "frame bundle count set failed";
            }
        }
    } else {
        dcamprop_setvalue(hdcam, DCAM_IDPROP_FRAMEBUNDLE_MODE, DCAMPROP_MODE__OFF);
    }

    if (failed(dcambuf_alloc(hdcam, 16))) {
        warnings << "buffer alloc failed after apply";
    }

    frameCounter = 0;
    if (startAfterApply) {
        QString startErr = start();
        if (!startErr.isEmpty())
            return startErr;
    }
    if (!warnings.isEmpty())
        return QString("WARN: %1").arg(warnings.join("; "));
    return {};
}

QString DcamController::readProps(QString& out) {
    if (!opened)
        return "Camera not opened";
    double w = 0, h = 0, bin = 0, bits = 0, ptype = 0;
    dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_WIDTH, &w);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_HEIGHT, &h);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_BINNING, &bin);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_BITSPERCHANNEL, &bits);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_IMAGE_PIXELTYPE, &ptype);
    out = QString("Width: %1\nHeight: %2\nBinning: %3\nBits: %4\nPixelType: %5")
              .arg(w, 0, 'f', 0)
              .arg(h, 0, 'f', 0)
              .arg(bin, 0, 'f', 0)
              .arg(bits, 0, 'f', 0)
              .arg(ptype, 0, 'f', 0);
    return {};
}

bool DcamController::lockLatestFrame(QImage& outImage, FrameMeta& meta) {
    if (!opened)
        return false;

    DCAMBUF_FRAME bf = {};
    bf.size = sizeof(bf);
    bf.iFrame = -1; // latest
    DCAMERR err = dcambuf_lockframe(hdcam, &bf);
    if (failed(err))
        return false;

    meta.width = static_cast<int>(bf.width);
    meta.height = static_cast<int>(bf.height);
    double bin = 1, bits = 0;
    dcamprop_getvalue(hdcam, DCAM_IDPROP_BINNING, &bin);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_BITSPERCHANNEL, &bits);
    meta.binning = bin;
    meta.bits = static_cast<int>(bits);
    meta.frameIndex = frameCounter;
    DCAMCAP_TRANSFERINFO ti = {};
    ti.size = sizeof(ti);
    if (!failed(dcamcap_transferinfo(hdcam, &ti))) {
        meta.delivered = ti.nFrameCount;
        meta.dropped = 0;
    }
    double fps = 0, rds = 0;
    dcamprop_getvalue(hdcam, DCAM_IDPROP_INTERNALFRAMERATE, &fps);
    dcamprop_getvalue(hdcam, DCAM_IDPROP_READOUTSPEED, &rds);
    meta.internalFps = fps;
    meta.readoutSpeed = rds;

    frameCounter = (frameCounter + 1) % 10000;

    if (bits <= 8) {
        QImage img(reinterpret_cast<uchar*>(bf.buf), bf.width, bf.height, bf.rowbytes, QImage::Format_Grayscale8);
        outImage = img.copy();
    } else {
        QImage img(reinterpret_cast<uchar*>(bf.buf), bf.width, bf.height, bf.rowbytes, QImage::Format_Grayscale16);
        outImage = img.convertToFormat(QImage::Format_Grayscale8);
    }
    return true;
}

QString DcamController::errText(const QString& label, DCAMERR err) const {
    return QString("%1 failed: 0x%2").arg(label).arg(err, 8, 16, QChar('0'));
}
