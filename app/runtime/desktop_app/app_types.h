#pragma once

#include <QImage>
#include <QMap>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <vector>

#include <opencv2/core.hpp>

#include "frame_types.h"
#include "pipeline_runner.h"

struct SequenceFrame {
    QImage image;
    QString path;
};

struct AppOptions {
    bool testMode = false;
    bool mockCamera = false;
    bool verifyCameraWorkspace = false;
    bool verifyDaqSettings = false;
    bool noDaq = false;
    bool noStartupPrompts = false;
    QString datasetBuilderReviewPath;
    QString initialWorkspace;
};

struct StatsTracker {
    QMap<QString, int> classCounts;
    int totalEvents = 0;
    int hitCount = 0;
    int wasteCount = 0;
    bool eventActive = false;
    int missCount = 0;
    int currentEventId = 0;
    cv::Point2f startCentroid = {0.0f, 0.0f};
    cv::Point2f lastCentroid = {0.0f, 0.0f};
    bool hasCentroid = false;
    double cumulativeDy = 0.0;
    double lastY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    int frameHeight = 0;
    QString currentLabel;
    QString lastEventDir = "Unknown";
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
};

struct StatsSnapshot {
    int totalEvents = 0;
    int hitCount = 0;
    int wasteCount = 0;
    bool eventActive = false;
    QString classText;
    QString lastText;
    QMap<QString, int> classCounts;
    QString lastEventDir;
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
};

inline QString normalizeEventLabel(const QString& label) {
    return label.isEmpty() ? "(unclassified)" : label;
}

inline QString decideEventDirection(double cumulativeDy, double lastY, int frameHeight, bool hasCentroid) {
    if (!hasCentroid)
        return "Unknown";
    double threshold = 2.0;
    if (frameHeight > 0) {
        threshold = std::max(threshold, frameHeight * 0.02);
    }
    bool movedUp = cumulativeDy < -threshold;
    bool movedDown = cumulativeDy > threshold;
    bool hasFrame = (frameHeight > 0);
    double mid = hasFrame ? frameHeight * 0.5 : 0.0;
    if (movedUp && (!hasFrame || lastY < mid)) {
        return "Waste";
    }
    if (movedDown && (!hasFrame || lastY >= mid)) {
        return "Hit";
    }
    if (hasFrame) {
        return (lastY < mid) ? "Waste" : "Hit";
    }
    return (cumulativeDy < 0.0) ? "Waste" : "Hit";
}

struct SequenceEventRecord {
    int eventId = 0;
    QString label;
    int startFrame = -1;
    int decisionFrame = -1;
    QString decisionDir;
    int firedFrame = -1;
    int framesTracked = 0;
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double cumulativeDy = 0.0;
    double pathLength = 0.0;
    int frameHeight = 0;
};

struct SequenceEventTracker {
    int resetFrames = 2;
    bool eventActive = false;
    int missCount = 0;
    int currentEventId = 0;
    cv::Point2f startCentroid = {0.0f, 0.0f};
    cv::Point2f lastCentroid = {0.0f, 0.0f};
    bool hasCentroid = false;
    double cumulativeDy = 0.0;
    double lastY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double pathLength = 0.0;
    int frameHeight = 0;
    QString currentLabel;
    int startFrame = -1;
    int firedFrame = -1;
    int framesTracked = 0;
    QString lastEventDir = "Unknown";
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
    int lastFrameNumber = -1;
    std::vector<SequenceEventRecord> events;

    void reset(int resetFramesIn) {
        resetFrames = resetFramesIn;
        eventActive = false;
        missCount = 0;
        currentEventId = 0;
        hasCentroid = false;
        cumulativeDy = 0.0;
        lastY = 0.0;
        minY = 0.0;
        maxY = 0.0;
        pathLength = 0.0;
        frameHeight = 0;
        currentLabel.clear();
        startFrame = -1;
        firedFrame = -1;
        framesTracked = 0;
        lastEventDir = "Unknown";
        lastEventLabel.clear();
        lastDecisionFrame = -1;
        lastDecisionEventId = 0;
        lastFrameNumber = -1;
        events.clear();
    }

    void startEvent(const PipelineEvent& evt) {
        eventActive = true;
        missCount = 0;
        currentEventId++;
        startCentroid = evt.centroid;
        lastCentroid = evt.centroid;
        hasCentroid = true;
        cumulativeDy = 0.0;
        lastY = evt.centroid.y;
        minY = evt.centroid.y;
        maxY = evt.centroid.y;
        pathLength = 0.0;
        framesTracked = 1;
        if (evt.frameHeight > 0)
            frameHeight = evt.frameHeight;
        currentLabel = normalizeEventLabel(QString::fromStdString(evt.label));
        startFrame = static_cast<int>(evt.frameNumber);
        firedFrame = evt.fired ? static_cast<int>(evt.frameNumber) : -1;
    }

    void endEvent(int decisionFrame) {
        if (!eventActive)
            return;
        QString dir = decideEventDirection(cumulativeDy, lastY, frameHeight, hasCentroid);

        SequenceEventRecord rec;
        rec.eventId = currentEventId;
        rec.label = normalizeEventLabel(currentLabel);
        rec.startFrame = startFrame;
        rec.decisionFrame = decisionFrame;
        rec.decisionDir = dir;
        rec.firedFrame = firedFrame;
        rec.framesTracked = framesTracked;
        rec.startX = startCentroid.x;
        rec.startY = startCentroid.y;
        rec.endX = lastCentroid.x;
        rec.endY = lastCentroid.y;
        rec.minY = minY;
        rec.maxY = maxY;
        rec.cumulativeDy = cumulativeDy;
        rec.pathLength = pathLength;
        rec.frameHeight = frameHeight;
        events.push_back(rec);

        lastEventDir = dir;
        lastEventLabel = rec.label;
        lastDecisionFrame = decisionFrame;
        lastDecisionEventId = currentEventId;
        eventActive = false;
        hasCentroid = false;
        missCount = 0;
        currentLabel.clear();
        cumulativeDy = 0.0;
        pathLength = 0.0;
        framesTracked = 0;
        startFrame = -1;
        firedFrame = -1;
    }

    void update(const PipelineEvent& evt, bool processed) {
        if (!processed)
            return;
        lastFrameNumber = static_cast<int>(evt.frameNumber);
        if (evt.fired) {
            if (eventActive) {
                endEvent(static_cast<int>(evt.frameNumber));
            }
            startEvent(evt);
            return;
        }
        if (evt.detected) {
            if (!eventActive) {
                startEvent(evt);
            } else {
                double dx = static_cast<double>(evt.centroid.x - lastCentroid.x);
                double dy = static_cast<double>(evt.centroid.y - lastCentroid.y);
                pathLength += std::sqrt(dx * dx + dy * dy);
                cumulativeDy += dy;
                lastCentroid = evt.centroid;
                hasCentroid = true;
                lastY = evt.centroid.y;
                minY = std::min(minY, static_cast<double>(evt.centroid.y));
                maxY = std::max(maxY, static_cast<double>(evt.centroid.y));
                framesTracked++;
                if (evt.frameHeight > 0)
                    frameHeight = evt.frameHeight;
                missCount = 0;
            }
        } else if (eventActive) {
            missCount++;
            if (missCount >= resetFrames) {
                endEvent(static_cast<int>(evt.frameNumber));
            }
        }
    }

    void finalize() {
        if (!eventActive)
            return;
        int decisionFrame = (lastFrameNumber >= 0) ? lastFrameNumber : startFrame;
        endEvent(decisionFrame);
    }
};

struct LiveLogRecord {
    QString wallTime;
    qint64 elapsedMs = 0;
    qint64 frameIndex = 0;
    qint64 delivered = 0;
    qint64 dropped = 0;
    double fps = 0.0;
    double camFps = 0.0;
    double procMs = 0.0;
    bool processed = false;
    bool pipelineEnabled = false;
    bool pipelineReady = false;
    QString skipReason;
    bool detected = false;
    bool fired = false;
    double area = 0.0;
    int bboxX = 0;
    int bboxY = 0;
    int bboxW = 0;
    int bboxH = 0;
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    QString cropPath;
    QString label;
    double score = 0.0;
    bool triggered = false;
    bool triggerOk = false;
    int bgRemaining = 0;
    QString eventDir;
    int decisionFrame = -1;
    int decisionEventId = 0;
    int hitCount = 0;
    int wasteCount = 0;
};
