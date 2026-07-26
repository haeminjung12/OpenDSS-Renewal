#include "../../dcam_camera.h"

#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace fake_dcam {
namespace {

struct GetResult {
    DCAMERR result = DCAMERR_SUCCESS;
    double value = 0.0;
    bool overrideValue = false;
};

struct State {
    DCAMERR initResult = DCAMERR_SUCCESS;
    DCAMERR openResult = DCAMERR_SUCCESS;
    DCAMERR waitOpenResult = DCAMERR_SUCCESS;
    DCAMERR startResult = DCAMERR_SUCCESS;
    DCAMERR waitResult = DCAMERR_TEST_FAILURE;
    DCAMERR lockFrameResult = DCAMERR_TEST_FAILURE;
    int deviceCount = 1;
    int openedIndex = -1;
    int allocations = 0;
    int releases = 0;
    int starts = 0;
    int stops = 0;
    int closes = 0;
    int uninitializes = 0;
    int frameCount = 0;
    FrameData frame;
    std::map<int32, double> properties;
    std::map<int32, int32> attributes;
    std::map<int32, DCAMERR> attributeResults;
    std::map<int32, std::deque<DCAMERR>> setResults;
    std::map<int32, std::deque<GetResult>> getResults;
    std::deque<DCAMERR> allocationResults;
    std::deque<DCAMERR> releaseResults;
    std::vector<std::pair<int32, double>> propertyWrites;
};

State makeDefaultState()
{
    State value;
    const int32 readWrite = DCAMPROP_ATTR_READABLE | DCAMPROP_ATTR_WRITABLE;
    for (const int32 property : {
             DCAM_IDPROP_SUBARRAYMODE,
             DCAM_IDPROP_SUBARRAYHPOS,
             DCAM_IDPROP_SUBARRAYVPOS,
             DCAM_IDPROP_SUBARRAYHSIZE,
             DCAM_IDPROP_SUBARRAYVSIZE,
             DCAM_IDPROP_IMAGE_PIXELTYPE,
             DCAM_IDPROP_BITSPERCHANNEL,
             DCAM_IDPROP_EXPOSURETIME,
             DCAM_IDPROP_READOUTSPEED,
         }) {
        value.attributes[property] = readWrite;
    }
    value.properties = {
        {DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON},
        {DCAM_IDPROP_SUBARRAYHPOS, 0.0},
        {DCAM_IDPROP_SUBARRAYVPOS, 0.0},
        {DCAM_IDPROP_SUBARRAYHSIZE, 1024.0},
        {DCAM_IDPROP_SUBARRAYVSIZE, 1024.0},
        {DCAM_IDPROP_BINNING, 1.0},
        {DCAM_IDPROP_IMAGE_PIXELTYPE, DCAM_PIXELTYPE_MONO16},
        {DCAM_IDPROP_BITSPERCHANNEL, 12.0},
        {DCAM_IDPROP_EXPOSURETIME, 0.010},
        {DCAM_IDPROP_READOUTSPEED, 3.0},
        {DCAM_IDPROP_INTERNALFRAMERATE, 100.0},
    };
    return value;
}

State state = makeDefaultState();

} // namespace

void reset()
{
    state = makeDefaultState();
}

void setInitResult(DCAMERR result) { state.initResult = result; }
void setStartResult(DCAMERR result) { state.startResult = result; }
void setWaitResult(bool ready)
{
    state.waitResult = ready ? DCAMERR_SUCCESS : DCAMERR_TEST_FAILURE;
}
void setLockFrameResult(bool available)
{
    state.lockFrameResult = available ? DCAMERR_SUCCESS : DCAMERR_TEST_FAILURE;
}
void queueReleaseResult(DCAMERR result)
{
    state.releaseResults.push_back(result);
}
void queueAllocationResult(DCAMERR result)
{
    state.allocationResults.push_back(result);
}
void setProperty(int32 property, double value) { state.properties[property] = value; }
double property(int32 property) { return state.properties[property]; }
void setAttribute(int32 property, int32 attributes)
{
    state.attributes[property] = attributes;
}
void setAttributeResult(int32 property, DCAMERR result)
{
    state.attributeResults[property] = result;
}
void clearAttributeResult(int32 property)
{
    state.attributeResults.erase(property);
}
void queueSetResult(int32 property, DCAMERR result)
{
    state.setResults[property].push_back(result);
}
void queueGetValue(int32 property, double value)
{
    state.getResults[property].push_back({DCAMERR_SUCCESS, value, true});
}
void queueGetResult(int32 property, DCAMERR result)
{
    state.getResults[property].push_back({result, 0.0, false});
}
void setFrame(FrameData frame)
{
    state.frame = std::move(frame);
    state.lockFrameResult = DCAMERR_SUCCESS;
    state.waitResult = DCAMERR_SUCCESS;
}

int openedIndex() { return state.openedIndex; }
int allocations() { return state.allocations; }
int releases() { return state.releases; }
int starts() { return state.starts; }
int stops() { return state.stops; }
int closes() { return state.closes; }
const std::vector<std::pair<int32, double>> &propertyWrites()
{
    return state.propertyWrites;
}
void clearPropertyWrites() { state.propertyWrites.clear(); }

} // namespace fake_dcam

DCAMERR dcamapi_init(DCAMAPI_INIT *api)
{
    if (failed(fake_dcam::state.initResult))
        return fake_dcam::state.initResult;
    api->iDeviceCount = fake_dcam::state.deviceCount;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamapi_uninit()
{
    ++fake_dcam::state.uninitializes;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamdev_open(DCAMDEV_OPEN *device)
{
    if (failed(fake_dcam::state.openResult))
        return fake_dcam::state.openResult;
    fake_dcam::state.openedIndex = device->index;
    device->hdcam = reinterpret_cast<HDCAM>(1);
    return DCAMERR_SUCCESS;
}

DCAMERR dcamdev_close(HDCAM)
{
    ++fake_dcam::state.closes;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamwait_open(DCAMWAIT_OPEN *wait)
{
    if (failed(fake_dcam::state.waitOpenResult))
        return fake_dcam::state.waitOpenResult;
    wait->hwait = reinterpret_cast<HDCAMWAIT>(2);
    return DCAMERR_SUCCESS;
}

DCAMERR dcamwait_close(HDCAMWAIT)
{
    return DCAMERR_SUCCESS;
}

DCAMERR dcamwait_start(HDCAMWAIT, DCAMWAIT_START *)
{
    return fake_dcam::state.waitResult;
}

DCAMERR dcambuf_alloc(HDCAM, int32)
{
    ++fake_dcam::state.allocations;
    if (fake_dcam::state.allocationResults.empty())
        return DCAMERR_SUCCESS;
    const DCAMERR result = fake_dcam::state.allocationResults.front();
    fake_dcam::state.allocationResults.pop_front();
    return result;
}

DCAMERR dcambuf_release(HDCAM)
{
    ++fake_dcam::state.releases;
    if (fake_dcam::state.releaseResults.empty())
        return DCAMERR_SUCCESS;
    const DCAMERR result = fake_dcam::state.releaseResults.front();
    fake_dcam::state.releaseResults.pop_front();
    return result;
}

DCAMERR dcambuf_lockframe(HDCAM, DCAMBUF_FRAME *frame)
{
    if (failed(fake_dcam::state.lockFrameResult))
        return fake_dcam::state.lockFrameResult;
    frame->buf = fake_dcam::state.frame.image.data;
    frame->rowbytes = static_cast<int32>(fake_dcam::state.frame.image.step);
    frame->width = fake_dcam::state.frame.image.cols;
    frame->height = fake_dcam::state.frame.image.rows;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamcap_start(HDCAM, int32)
{
    ++fake_dcam::state.starts;
    return fake_dcam::state.startResult;
}

DCAMERR dcamcap_stop(HDCAM)
{
    ++fake_dcam::state.stops;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamcap_transferinfo(HDCAM, DCAMCAP_TRANSFERINFO *info)
{
    info->nFrameCount = fake_dcam::state.frame.meta.delivered;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamprop_getattr(HDCAM, DCAMPROP_ATTR *attribute)
{
    const auto result = fake_dcam::state.attributeResults.find(attribute->iProp);
    if (result != fake_dcam::state.attributeResults.end())
        return result->second;
    const auto value = fake_dcam::state.attributes.find(attribute->iProp);
    attribute->attribute =
        value == fake_dcam::state.attributes.end() ? 0 : value->second;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamprop_getvalue(HDCAM, int32 property, double *value)
{
    auto &results = fake_dcam::state.getResults[property];
    if (!results.empty()) {
        const fake_dcam::GetResult result = results.front();
        results.pop_front();
        if (failed(result.result))
            return result.result;
        if (result.overrideValue) {
            *value = result.value;
            return DCAMERR_SUCCESS;
        }
    }
    const auto found = fake_dcam::state.properties.find(property);
    if (found == fake_dcam::state.properties.end())
        return DCAMERR_TEST_FAILURE;
    *value = found->second;
    return DCAMERR_SUCCESS;
}

DCAMERR dcamprop_setvalue(HDCAM, int32 property, double value)
{
    fake_dcam::state.propertyWrites.emplace_back(property, value);
    auto &results = fake_dcam::state.setResults[property];
    if (!results.empty()) {
        const DCAMERR result = results.front();
        results.pop_front();
        if (failed(result))
            return result;
    }
    fake_dcam::state.properties[property] = value;
    return DCAMERR_SUCCESS;
}
