#pragma once

#include "dcamapi4.h"

constexpr int DCAMPROP_READOUTSPEED__SLOWEST = 1;
constexpr int DCAMPROP_READOUTSPEED__FASTEST = 0x7fffffff;
constexpr int DCAMPROP_TRIGGERSOURCE__INTERNAL = 1;
constexpr int DCAMPROP_TRIGGER_MODE__NORMAL = 1;
constexpr int DCAMPROP_TRIGGERACTIVE__EDGE = 1;
constexpr int DCAMPROP_MODE__OFF = 1;
constexpr int DCAMPROP_MODE__ON = 2;

constexpr int32 DCAMPROP_ATTR_READABLE = 0x00010000;
constexpr int32 DCAMPROP_ATTR_WRITABLE = 0x00020000;

constexpr int32 DCAM_IDPROP_SUBARRAYMODE = 1;
constexpr int32 DCAM_IDPROP_SUBARRAYHPOS = 2;
constexpr int32 DCAM_IDPROP_SUBARRAYVPOS = 3;
constexpr int32 DCAM_IDPROP_SUBARRAYHSIZE = 4;
constexpr int32 DCAM_IDPROP_SUBARRAYVSIZE = 5;
constexpr int32 DCAM_IDPROP_BINNING = 6;
constexpr int32 DCAM_IDPROP_IMAGE_PIXELTYPE = 7;
constexpr int32 DCAM_IDPROP_BITSPERCHANNEL = 8;
constexpr int32 DCAM_IDPROP_READOUTSPEED = 9;
constexpr int32 DCAM_IDPROP_EXPOSURETIME = 10;
constexpr int32 DCAM_IDPROP_TRIGGERSOURCE = 11;
constexpr int32 DCAM_IDPROP_TRIGGER_MODE = 12;
constexpr int32 DCAM_IDPROP_TRIGGERACTIVE = 13;
constexpr int32 DCAM_IDPROP_FRAMEBUNDLE_MODE = 14;
constexpr int32 DCAM_IDPROP_FRAMEBUNDLE_NUMBER = 15;
constexpr int32 DCAM_IDPROP_INTERNALFRAMERATE = 16;

struct DCAMPROP_ATTR {
    int32 cbSize = 0;
    int32 iProp = 0;
    int32 option = 0;
    int32 iReserved1 = 0;
    int32 attribute = 0;
};

DCAMERR dcamprop_getattr(HDCAM camera, DCAMPROP_ATTR *attribute);
DCAMERR dcamprop_getvalue(HDCAM camera, int32 property, double *value);
DCAMERR dcamprop_setvalue(HDCAM camera, int32 property, double value);
