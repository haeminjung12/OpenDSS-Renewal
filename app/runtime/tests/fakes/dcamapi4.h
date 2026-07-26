#pragma once

#include <cstddef>
#include <cstdint>

using int32 = std::int32_t;
using DCAMERR = std::int32_t;
using HDCAM = void *;
using HDCAMWAIT = void *;

constexpr DCAMERR DCAMERR_SUCCESS = 1;
constexpr DCAMERR DCAMERR_NOCAMERA = static_cast<DCAMERR>(0x80000206u);
constexpr DCAMERR DCAMERR_TEMPERATURE_TROUBLE =
    static_cast<DCAMERR>(0x80000304u);
constexpr DCAMERR DCAMERR_INVALIDPROPERTYID =
    static_cast<DCAMERR>(0x80000821u);
constexpr DCAMERR DCAMERR_NOTSUPPORT = static_cast<DCAMERR>(0x80000f03u);
constexpr DCAMERR DCAMERR_TEST_FAILURE = static_cast<DCAMERR>(0x80000101u);

inline bool failed(DCAMERR error) { return error < 0; }

constexpr int DCAM_PIXELTYPE_MONO8 = 1;
constexpr int DCAM_PIXELTYPE_MONO16 = 2;

constexpr int32 DCAMAPI_INITOPTION_APIVER__LATEST = 1;
constexpr int32 DCAMAPI_INITOPTION_ENDMARK = 0;
constexpr int32 DCAMCAP_START_SEQUENCE = -1;
constexpr int32 DCAMWAIT_CAPEVENT_FRAMEREADY = 1;

struct DCAMAPI_INIT {
    int32 size = 0;
    int32 iDeviceCount = 0;
    int32 *initoption = nullptr;
    int32 initoptionbytes = 0;
};

struct DCAMDEV_OPEN {
    int32 size = 0;
    int32 index = 0;
    HDCAM hdcam = nullptr;
};

struct DCAMWAIT_OPEN {
    int32 size = 0;
    HDCAM hdcam = nullptr;
    HDCAMWAIT hwait = nullptr;
};

struct DCAMWAIT_START {
    int32 size = 0;
    int32 eventmask = 0;
    int32 timeout = 0;
};

struct DCAMBUF_FRAME {
    int32 size = 0;
    int32 iFrame = 0;
    void *buf = nullptr;
    int32 rowbytes = 0;
    int32 width = 0;
    int32 height = 0;
};

struct DCAMCAP_TRANSFERINFO {
    int32 size = 0;
    int32 nNewestFrameIndex = 0;
    int32 nFrameCount = 0;
};

DCAMERR dcamapi_init(DCAMAPI_INIT *api);
DCAMERR dcamapi_uninit();
DCAMERR dcamdev_open(DCAMDEV_OPEN *device);
DCAMERR dcamdev_close(HDCAM camera);
DCAMERR dcamwait_open(DCAMWAIT_OPEN *wait);
DCAMERR dcamwait_close(HDCAMWAIT wait);
DCAMERR dcamwait_start(HDCAMWAIT wait, DCAMWAIT_START *start);
DCAMERR dcambuf_alloc(HDCAM camera, int32 count);
DCAMERR dcambuf_release(HDCAM camera);
DCAMERR dcambuf_lockframe(HDCAM camera, DCAMBUF_FRAME *frame);
DCAMERR dcamcap_start(HDCAM camera, int32 mode);
DCAMERR dcamcap_stop(HDCAM camera);
DCAMERR dcamcap_transferinfo(HDCAM camera, DCAMCAP_TRANSFERINFO *info);
