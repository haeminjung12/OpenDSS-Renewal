#include "frame_grabber.h"
#include "dcam_controller.h"

FrameGrabber::FrameGrabber(DcamController* ctrl, QObject* parent)
    : QObject(parent), controller(ctrl), running(false), displayEvery(1) {}

void FrameGrabber::setDisplayEvery(int n) {
    displayEvery = std::max(1, n);
}

void FrameGrabber::startGrabbing() {
    running = true;
}

void FrameGrabber::stopGrabbing() {
    running = false;
}
