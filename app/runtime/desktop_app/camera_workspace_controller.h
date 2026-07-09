#pragma once

#include <array>
#include <atomic>
#include <functional>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QImage>

#include "frame_types.h"
#include "workspace_camera.h"

class QApplication;
class CameraWorker;
class QLabel;
class QPushButton;
class QStatusBar;
class QTabWidget;
class QTimer;
class QThread;
class QWidget;
class PipelineRunner;
class QMutex;
class ZoomImageView;

struct AppOptions;

namespace desktop_app {
struct AppState;
}

class CameraWorkspaceController : public QObject {
    Q_OBJECT

  public:
    using LogCallback = std::function<void(const QString&)>;

    struct Dependencies {
        QApplication* app = nullptr;
        QWidget* window = nullptr;
        QStatusBar* statusBar = nullptr;
        CameraWorker* cameraWorker = nullptr;
        const AppOptions* options = nullptr;
        desktop_app::AppState* appState = nullptr;
        desktop_app::workspace::CameraWorkspaceControls controls;
        bool* viewerOnly = nullptr;
        bool* cameraOpened = nullptr;
        bool daqBuildEnabled = false;
        QString initialDaqStatusText;
        QLabel* statusLabel = nullptr;
        QLabel* cameraStatusItem = nullptr;
        QLabel* modelStatusItem = nullptr;
        QLabel* daqStatusItem = nullptr;
        QLabel* runStatusItem = nullptr;
        QLabel* pipelineStatusLabel = nullptr;
        QLabel* statsLabel = nullptr;
        ZoomImageView* liveImageView = nullptr;
        ZoomImageView* cameraImageView = nullptr;
        QLabel* liveViewerEmpty = nullptr;
        QLabel* cameraViewerEmpty = nullptr;
        QLabel* liveHudResolution = nullptr;
        QLabel* cameraHudResolution = nullptr;
        QLabel* liveHudFrameTime = nullptr;
        QLabel* cameraHudFrameTime = nullptr;
        QLabel* liveHudFps = nullptr;
        QLabel* cameraHudFps = nullptr;
        QPushButton* startButton = nullptr;
        QPushButton* reconnectButton = nullptr;
        QPushButton* applyButton = nullptr;
        QTabWidget* operationalTabs = nullptr;
        PipelineRunner* pipeline = nullptr;
        QMutex* pipelineMutex = nullptr;
        std::atomic<bool>* pipelineEnabled = nullptr;
        LogCallback logLine;
        LogCallback systemLogLine;
    };

    explicit CameraWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

    int currentBits() const;
    int currentPixelType() const;
    int lutMinValue() const;
    int lutMaxValue() const;
    void updateLutRange(int bits);
    QImage applyLutToImage(const QImage& image) const;
    void applyFrameToPreviewWorkspaces(const QImage& image, FrameMeta meta, double fps);
    void storeLastFrame(const QImage& image, const FrameMeta& meta);
    QImage lastFrame() const;
    FrameMeta lastMeta() const;
    bool initializeCamera();
    void shutdownCameraThread(QThread& cameraThread);

  private:
    void wireCameraWorker();
    void wireControls();
    void rebuildLut();
    void setLutMin(int value);
    void setLutMax(int value);
    void autoSetLutFromCurrentFrame();
    void autoSetExposureFromCurrentFrame();
    void applySettings();
    void scheduleApplySettings();
    void setViewerOnly();
    void updateCameraActionState();
    void resetPipelineIfReady(const QString& statusText);
    void showStatusMessage(const QString& message);
    void log(const QString& message);
    void systemLog(const QString& message);

    Dependencies deps_;
    QTimer* applyTimer_ = nullptr;
    bool autoApplyCamera_ = false;
    QImage lastFrame_;
    FrameMeta lastMeta_{};
    std::array<unsigned char, 256> lutTable_{};
    mutable QMutex lutMutex_;
    std::atomic<int> lutMinValue_{0};
    std::atomic<int> lutMaxValue_{255};
    std::atomic<int> lutRangeMax_{255};
    std::atomic<bool> lutEnabled_{false};
};
