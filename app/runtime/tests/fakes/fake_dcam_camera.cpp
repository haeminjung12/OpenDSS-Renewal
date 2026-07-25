#include "../../dcam_camera.h"

#include <utility>

namespace fake_dcam {
namespace {

struct State {
    std::string initError;
    std::string startError;
    bool waitResult = false;
    bool frameResult = false;
    FrameData frame;
    int initIndex = -1;
    int constructions = 0;
    int cleanups = 0;
    int starts = 0;
    int stops = 0;
};

State state;

} // namespace

void reset()
{
    state = {};
}

void setInitError(std::string error)
{
    state.initError = std::move(error);
}

void setStartError(std::string error)
{
    state.startError = std::move(error);
}

void setWaitResult(bool result)
{
    state.waitResult = result;
}

void setFrameResult(bool result)
{
    state.frameResult = result;
}

void setFrame(FrameData frame)
{
    state.frame = std::move(frame);
}

int initIndex()
{
    return state.initIndex;
}

int constructions()
{
    return state.constructions;
}

int cleanups()
{
    return state.cleanups;
}

int starts()
{
    return state.starts;
}

int stops()
{
    return state.stops;
}

} // namespace fake_dcam

DcamCamera::DcamCamera()
    : hdcam_(nullptr)
    , hwait_(nullptr)
    , opened_(false)
    , bufferCount_(16)
    , frameCounter_(0)
{
    ++fake_dcam::state.constructions;
}

DcamCamera::~DcamCamera()
{
    cleanup();
}

std::string DcamCamera::init(int deviceIndex)
{
    fake_dcam::state.initIndex = deviceIndex;
    if (!fake_dcam::state.initError.empty()) {
        return fake_dcam::state.initError;
    }
    opened_ = true;
    return {};
}

std::string DcamCamera::apply(const CameraSettings &)
{
    return {};
}

std::string DcamCamera::start()
{
    ++fake_dcam::state.starts;
    return fake_dcam::state.startError;
}

void DcamCamera::stop()
{
    ++fake_dcam::state.stops;
}

void DcamCamera::cleanup()
{
    if (opened_) {
        ++fake_dcam::state.cleanups;
    }
    opened_ = false;
}

bool DcamCamera::isOpened() const
{
    return opened_;
}

bool DcamCamera::waitForFrame(int)
{
    return opened_ && fake_dcam::state.waitResult;
}

bool DcamCamera::getLatestFrame(FrameData &out)
{
    if (!opened_ || !fake_dcam::state.frameResult) {
        return false;
    }
    out = fake_dcam::state.frame;
    return true;
}
