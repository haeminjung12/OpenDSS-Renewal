#include "camera_workspace_controller.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <QtCore/QMetaObject>
#include <QtCore/QSemaphore>
#include <QtCore/QSignalBlocker>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>

#include "app_state.h"
#include "camera_worker.h"
#include "pipeline_runner.h"
#include "app_types.h"
#include "zoom_image_view.h"

CameraWorkspaceController::CameraWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    applyTimer_ = new QTimer(this);
    applyTimer_->setSingleShot(true);
    applyTimer_->setInterval(250);
    connect(applyTimer_, &QTimer::timeout, this, [this]() {
        if (deps_.viewerOnly && *deps_.viewerOnly) {
            return;
        }
        applySettings();
    });

    wireCameraWorker();
    wireControls();
}

int CameraWorkspaceController::currentBits() const {
    const auto& controls = deps_.controls;
    if (!controls.bitsCombo) {
        return 8;
    }
    if (controls.bitsCombo->currentData().isValid()) {
        bool ok = false;
        const int bits = controls.bitsCombo->currentData().toInt(&ok);
        if (ok && bits > 0) {
            return bits;
        }
    }
    const int bits = controls.bitsCombo->currentText().toInt();
    return bits > 0 ? bits : 8;
}

int CameraWorkspaceController::currentPixelType() const {
    return currentBits() > 8 ? DCAM_PIXELTYPE_MONO16 : DCAM_PIXELTYPE_MONO8;
}

int CameraWorkspaceController::lutMinValue() const {
    return lutMinValue_.load();
}

int CameraWorkspaceController::lutMaxValue() const {
    return lutMaxValue_.load();
}

void CameraWorkspaceController::updateLutRange(int bits) {
    const auto& controls = deps_.controls;
    if (!controls.lutMinSpin || !controls.lutMaxSpin || !controls.lutMinSlider || !controls.lutMaxSlider) {
        return;
    }

    const int prevRange = lutRangeMax_.load();
    int maxValue = 255;
    if (bits >= 1 && bits <= 16) {
        maxValue = (1 << bits) - 1;
    }
    lutRangeMax_.store(maxValue);
    if (controls.lutRangeLabel) {
        controls.lutRangeLabel->setText(QString("Scale: 0 - %1").arg(maxValue));
    }
    {
        QSignalBlocker b1(controls.lutMinSpin);
        QSignalBlocker b2(controls.lutMaxSpin);
        QSignalBlocker b3(controls.lutMinSlider);
        QSignalBlocker b4(controls.lutMaxSlider);
        controls.lutMinSpin->setRange(0, maxValue);
        controls.lutMaxSpin->setRange(0, maxValue);
        controls.lutMinSlider->setRange(0, maxValue);
        controls.lutMaxSlider->setRange(0, maxValue);
    }

    int minValue = std::clamp(lutMinValue_.load(), 0, maxValue);
    int currentMaxValue = std::clamp(lutMaxValue_.load(), 0, maxValue);
    if (minValue == 0 && currentMaxValue == prevRange) {
        currentMaxValue = maxValue;
    }
    if (currentMaxValue < minValue) {
        currentMaxValue = minValue;
    }
    {
        QSignalBlocker b1(controls.lutMinSpin);
        QSignalBlocker b2(controls.lutMaxSpin);
        QSignalBlocker b3(controls.lutMinSlider);
        QSignalBlocker b4(controls.lutMaxSlider);
        controls.lutMinSpin->setValue(minValue);
        controls.lutMaxSpin->setValue(currentMaxValue);
        controls.lutMinSlider->setValue(minValue);
        controls.lutMaxSlider->setValue(currentMaxValue);
    }
    lutMinValue_.store(minValue);
    lutMaxValue_.store(currentMaxValue);
    rebuildLut();
}

QImage CameraWorkspaceController::applyLutToImage(const QImage& image) const {
    if (image.isNull() || !lutEnabled_.load()) {
        return image;
    }

    QImage output = image;
    if (output.format() != QImage::Format_Grayscale8) {
        output = output.convertToFormat(QImage::Format_Grayscale8);
    } else {
        output.detach();
    }

    std::array<unsigned char, 256> tableCopy;
    {
        QMutexLocker lock(&lutMutex_);
        tableCopy = lutTable_;
    }
    const int width = output.width();
    const int height = output.height();
    for (int y = 0; y < height; ++y) {
        unsigned char* row = output.scanLine(y);
        for (int x = 0; x < width; ++x) {
            row[x] = tableCopy[row[x]];
        }
    }
    return output;
}

void CameraWorkspaceController::applyFrameToPreviewWorkspaces(const QImage& image, FrameMeta meta, double fps) {
    if (!image.isNull()) {
        const QImage viewImage = applyLutToImage(image);
        if (deps_.liveImageView) {
            deps_.liveImageView->setImage(viewImage);
        }
        if (deps_.cameraImageView) {
            deps_.cameraImageView->setImage(viewImage);
        }
        storeLastFrame(viewImage, meta);
        if (deps_.liveViewerEmpty) {
            deps_.liveViewerEmpty->hide();
        }
        if (deps_.cameraViewerEmpty) {
            deps_.cameraViewerEmpty->hide();
        }
    } else {
        lastMeta_ = meta;
    }

    const QString resolutionText = QString("RES %1 x %2\nCAM %3")
                                       .arg(meta.width)
                                       .arg(meta.height)
                                       .arg(meta.delivered > 0 ? "LIVE" : "IDLE");
    const double exposureMs = deps_.controls.exposureSpin ? deps_.controls.exposureSpin->value() : 0.0;
    const QString frameTimeText = QString("EXP %1 ms\nPROC -- ms").arg(exposureMs, 0, 'f', 3);
    const QString fpsText = QString("FPS %1\nFRAME %2\nDROP %3")
                                .arg(fps, 0, 'f', 1)
                                .arg(meta.frameIndex)
                                .arg(meta.dropped);
    if (deps_.liveHudResolution) {
        deps_.liveHudResolution->setText(resolutionText);
    }
    if (deps_.cameraHudResolution) {
        deps_.cameraHudResolution->setText(resolutionText);
    }
    if (deps_.liveHudFrameTime) {
        deps_.liveHudFrameTime->setText(frameTimeText);
    }
    if (deps_.cameraHudFrameTime) {
        deps_.cameraHudFrameTime->setText(frameTimeText);
    }
    if (deps_.liveHudFps) {
        deps_.liveHudFps->setText(fpsText);
    }
    if (deps_.cameraHudFps) {
        deps_.cameraHudFps->setText(fpsText);
    }
    if (deps_.statsLabel) {
        deps_.statsLabel->setText(QString("Resolution: %1 x %2\nBinning: %3\nBits: %4\nFPS: %5 (Cam: %6)\nFrame: %7\nDelivered: %8 Dropped: %9\nReadout: %10")
                                      .arg(meta.width)
                                      .arg(meta.height)
                                      .arg(meta.binning, 0, 'f', 1)
                                      .arg(meta.bits)
                                      .arg(fps, 0, 'f', 1)
                                      .arg(meta.internalFps, 0, 'f', 1)
                                      .arg(meta.frameIndex)
                                      .arg(meta.delivered)
                                      .arg(meta.dropped)
                                      .arg(meta.readoutSpeed, 0, 'f', 0));
    }
    if (meta.frameIndex % 100 == 0) {
        log(QString("Frame=%1 FPS=%2 camfps=%3 delivered=%4 dropped=%5")
                .arg(meta.frameIndex)
                .arg(fps, 0, 'f', 1)
                .arg(meta.internalFps, 0, 'f', 1)
                .arg(meta.delivered)
                .arg(meta.dropped));
    }
}

void CameraWorkspaceController::storeLastFrame(const QImage& image, const FrameMeta& meta) {
    lastFrame_ = image;
    lastMeta_ = meta;
}

QImage CameraWorkspaceController::lastFrame() const {
    return lastFrame_;
}

FrameMeta CameraWorkspaceController::lastMeta() const {
    return lastMeta_;
}

bool CameraWorkspaceController::initializeCamera() {
    if (!deps_.cameraWorker) {
        return false;
    }
    const bool skipStartup = deps_.hardwareFreeMode || (deps_.options && deps_.options->noStartupPrompts);
    if (skipStartup) {
        setHardwareFreeMode();
        return true;
    }
    if (deps_.statusLabel) {
        deps_.statusLabel->setText("Initializing camera...");
    }
    if (deps_.cameraStatusItem) {
        deps_.cameraStatusItem->setText("Camera: startup pending");
    }
    QMetaObject::invokeMethod(deps_.cameraWorker,
                              [worker = deps_.cameraWorker, bits = currentBits(), pixel = currentPixelType()]() {
                                  worker->initAndOpen(bits, pixel);
                              },
                              Qt::QueuedConnection);
    return true;
}

void CameraWorkspaceController::shutdownCameraThread(QThread& cameraThread) {
    if (!deps_.cameraWorker || !cameraThread.isRunning()) {
        return;
    }

    auto shutdownAck = std::make_shared<QSemaphore>();
    const bool queued = QMetaObject::invokeMethod(deps_.cameraWorker,
                                                  [worker = deps_.cameraWorker, shutdownAck]() {
                                                      worker->shutdown();
                                                      shutdownAck->release();
                                                  },
                                                  Qt::QueuedConnection);
    if (queued && !shutdownAck->tryAcquire(1, 5000)) {
        systemLog("Timed out waiting for camera worker shutdown acknowledgement.");
    } else if (!queued) {
        systemLog("Failed to queue camera worker shutdown.");
    }

    cameraThread.quit();
    if (!cameraThread.wait(5000)) {
        systemLog("Timed out waiting for camera worker thread to stop.");
    }
}

void CameraWorkspaceController::wireCameraWorker() {
    if (!deps_.cameraWorker) {
        return;
    }

    connect(deps_.cameraWorker, &CameraWorker::exposureLimitsReady, this,
            [this](double minimumMs, double maximumMs, double currentMs) {
                if (!deps_.controls.exposureSpin) {
                    return;
                }
                deps_.controls.exposureSpin->setMinimum(minimumMs);
                deps_.controls.exposureSpin->setMaximum(maximumMs);
                deps_.controls.exposureSpin->setValue(currentMs);
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::formatOptionsReady, this,
            [this](const QVariantMap& options) {
                const auto& controls = deps_.controls;
                const QString summary = desktop_app::workspace::refreshCameraFormatOptions(controls.presetCombo,
                                                                                            controls.bitsCombo,
                                                                                            controls.readoutCombo,
                                                                                            controls.customWidthSpin,
                                                                                            controls.customHeightSpin,
                                                                                            controls.exposureSpin,
                                                                                            options);
                updateLutRange(currentBits());
                log(summary);
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::readbackReady, this,
            [this](const QString& text) {
                log(text);
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::initCompleted, this,
            [this](const QString& error) {
                if (!error.isEmpty()) {
                    if (deps_.cameraOpened) {
                        *deps_.cameraOpened = false;
                    }
                    if (deps_.statusLabel) {
                        deps_.statusLabel->setText("Init error: " + error);
                    }
                    if (deps_.cameraStatusItem) {
                        deps_.cameraStatusItem->setText("Camera: error");
                    }
                    showStatusMessage("Camera initialization failed");
                    auto choice = QMessageBox::question(deps_.window,
                                                        "Init failed",
                                                        "Camera init failed:\n" + error + "\n\nLaunch viewer-only mode?",
                                                        QMessageBox::Yes | QMessageBox::No,
                                                        QMessageBox::Yes);
                    if (choice == QMessageBox::Yes) {
                        log("Init failed; switching to viewer-only mode.");
                        setViewerOnly();
                        return;
                    }
                    if (deps_.app) {
                        QMetaObject::invokeMethod(deps_.app, "quit", Qt::QueuedConnection);
                    }
                    return;
                }
                if (deps_.cameraOpened) {
                    *deps_.cameraOpened = true;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Initialized.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: connected");
                }
                showStatusMessage("Camera initialized");
                if (deps_.controls.exposureSpin) {
                    deps_.controls.exposureSpin->setValue(10.0);
                }
                autoApplyCamera_ = true;
                log("Init success");
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::reconnectCompleted, this,
            [this](const QString& error) {
                if (!error.isEmpty()) {
                    if (deps_.cameraOpened) {
                        *deps_.cameraOpened = false;
                    }
                    if (deps_.statusLabel) {
                        deps_.statusLabel->setText("Reconnect error: " + error);
                    }
                    if (deps_.cameraStatusItem) {
                        deps_.cameraStatusItem->setText("Camera: error");
                    }
                    showStatusMessage("Camera reconnect failed");
                    return;
                }
                if (deps_.cameraOpened) {
                    *deps_.cameraOpened = true;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Reconnected.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: connected");
                }
                showStatusMessage("Camera reconnected");
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::startCompleted, this,
            [this](const QString& error) {
                if (!error.isEmpty()) {
                    if (deps_.appState) {
                        deps_.appState->cameraStreaming = false;
                    }
                    if (deps_.statusLabel) {
                        deps_.statusLabel->setText("Start error: " + error);
                    }
                    if (deps_.cameraStatusItem) {
                        deps_.cameraStatusItem->setText("Camera: error");
                    }
                    showStatusMessage("Capture start failed");
                    return;
                }
                if (deps_.appState) {
                    deps_.appState->cameraStreaming = true;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Capture started.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: acquiring");
                }
                if (deps_.runStatusItem) {
                    deps_.runStatusItem->setText("Run: capture");
                }
                showStatusMessage("Capture started");
                resetPipelineIfReady("Pipeline: warming (capture start)");
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::stopCompleted, this,
            [this]() {
                if (deps_.appState) {
                    deps_.appState->cameraStreaming = false;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Capture stopped.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText(deps_.cameraOpened && *deps_.cameraOpened ? "Camera: connected"
                                                                                              : "Camera: unavailable");
                }
                if (deps_.pipelineEnabled && !deps_.pipelineEnabled->load() && deps_.runStatusItem) {
                    deps_.runStatusItem->setText("Run: idle");
                }
                showStatusMessage("Capture stopped");
                if (deps_.pipelineEnabled && deps_.pipelineEnabled->load() && deps_.pipelineStatusLabel) {
                    deps_.pipelineStatusLabel->setText("Pipeline: paused");
                }
            },
            Qt::QueuedConnection);

    connect(deps_.cameraWorker, &CameraWorker::applyCompleted, this,
            [this](const QString& error) {
                if (!error.isEmpty()) {
                    if (error.startsWith("WARN:")) {
                        if (deps_.appState) {
                            deps_.appState->cameraStreaming = true;
                        }
                        if (deps_.statusLabel) {
                            deps_.statusLabel->setText("Applied with warnings: " + error.mid(5));
                        }
                        if (deps_.cameraStatusItem) {
                            deps_.cameraStatusItem->setText("Camera: acquiring");
                        }
                    } else {
                        if (deps_.appState) {
                            deps_.appState->cameraStreaming = false;
                        }
                        if (deps_.statusLabel) {
                            deps_.statusLabel->setText("Apply error: " + error);
                        }
                    }
                } else {
                    if (deps_.appState) {
                        deps_.appState->cameraStreaming = true;
                    }
                    if (deps_.statusLabel) {
                        deps_.statusLabel->setText("Applied. Streaming");
                    }
                    if (deps_.cameraStatusItem) {
                        deps_.cameraStatusItem->setText("Camera: acquiring");
                    }
                }
            },
            Qt::QueuedConnection);
}

void CameraWorkspaceController::wireControls() {
    const auto& controls = deps_.controls;

    if (controls.presetCombo && controls.customWidthSpin && controls.customHeightSpin) {
        connect(controls.presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            const bool isCustom = deps_.controls.presetCombo->currentData().toSize().width() < 0;
            deps_.controls.customWidthSpin->setEnabled(isCustom);
            deps_.controls.customHeightSpin->setEnabled(isCustom);
            scheduleApplySettings();
        });
        const bool restoredCustom = controls.presetCombo->currentData().toSize().width() < 0;
        controls.customWidthSpin->setEnabled(restoredCustom);
        controls.customHeightSpin->setEnabled(restoredCustom);
    }

    if (deps_.reconnectButton) {
        connect(deps_.reconnectButton, &QPushButton::clicked, this, [this]() {
            if (deps_.hardwareFreeMode) {
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Mock camera reconnected.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: mock");
                }
                showStatusMessage("Mock camera reconnected");
                log("Mock camera reconnect requested.");
                return;
            }
            if (deps_.statusLabel) {
                deps_.statusLabel->setText("Reconnecting camera...");
            }
            QMetaObject::invokeMethod(deps_.cameraWorker,
                                      [worker = deps_.cameraWorker, bits = currentBits(), pixel = currentPixelType()]() {
                                          worker->reconnect(bits, pixel);
                                      },
                                      Qt::QueuedConnection);
        });
    }

    if (deps_.startButton) {
        connect(deps_.startButton, &QPushButton::clicked, this, [this]() {
            if (deps_.viewerOnly && *deps_.viewerOnly) {
                return;
            }
            if (deps_.hardwareFreeMode) {
                if (deps_.appState) {
                    deps_.appState->cameraStreaming = true;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Mock preview started.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: mock acquiring");
                }
                if (deps_.runStatusItem) {
                    deps_.runStatusItem->setText("Run: capture");
                }
                showStatusMessage("Mock preview started");
                log("Mock preview started.");
                return;
            }
            if (deps_.statusLabel) {
                deps_.statusLabel->setText("Starting capture...");
            }
            QMetaObject::invokeMethod(deps_.cameraWorker,
                                      [worker = deps_.cameraWorker,
                                       bits = currentBits(),
                                       pixel = currentPixelType(),
                                       displayEvery = deps_.controls.displayEverySpin ? deps_.controls.displayEverySpin->value() : 1]() {
                                          worker->setDisplayEvery(displayEvery);
                                          worker->startCapture(bits, pixel);
                                      },
                                      Qt::QueuedConnection);
        });
    }

    if (deps_.stopButton) {
        connect(deps_.stopButton, &QPushButton::clicked, this, [this]() {
            if (deps_.viewerOnly && *deps_.viewerOnly) {
                return;
            }
            if (deps_.hardwareFreeMode) {
                if (deps_.appState) {
                    deps_.appState->cameraStreaming = false;
                }
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Mock preview stopped.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: mock");
                }
                if (deps_.pipelineEnabled && !deps_.pipelineEnabled->load() && deps_.runStatusItem) {
                    deps_.runStatusItem->setText("Run: idle");
                }
                showStatusMessage("Mock preview stopped");
                log("Mock preview stopped.");
                return;
            }
            if (deps_.statusLabel) {
                deps_.statusLabel->setText("Stopping capture...");
            }
            QMetaObject::invokeMethod(deps_.cameraWorker,
                                      [worker = deps_.cameraWorker]() {
                                          worker->stopCapture();
                                      },
                                      Qt::QueuedConnection);
        });
    }

    if (deps_.applyButton) {
        connect(deps_.applyButton, &QPushButton::clicked, this, [this]() {
            if (deps_.viewerOnly && *deps_.viewerOnly) {
                return;
            }
            if (deps_.hardwareFreeMode) {
                if (deps_.statusLabel) {
                    deps_.statusLabel->setText("Mock camera settings applied.");
                }
                if (deps_.cameraStatusItem) {
                    deps_.cameraStatusItem->setText("Camera: mock");
                }
                showStatusMessage("Mock camera settings applied");
                log("Mock camera settings applied.");
                return;
            }
            applySettings();
        });
    }

    if (controls.customWidthSpin) {
        connect(controls.customWidthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            scheduleApplySettings();
        });
    }
    if (controls.customHeightSpin) {
        connect(controls.customHeightSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            scheduleApplySettings();
        });
    }
    if (controls.binCombo) {
        connect(controls.binCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            scheduleApplySettings();
        });
    }
    if (controls.bitsCombo) {
        connect(controls.bitsCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            updateLutRange(currentBits());
            scheduleApplySettings();
        });
    }
    if (controls.lutMinSpin) {
        connect(controls.lutMinSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            setLutMin(value);
        });
    }
    if (controls.lutMaxSpin) {
        connect(controls.lutMaxSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            setLutMax(value);
        });
    }
    if (controls.lutMinSlider) {
        connect(controls.lutMinSlider, &QSlider::valueChanged, this, [this](int value) {
            setLutMin(value);
        });
    }
    if (controls.lutMaxSlider) {
        connect(controls.lutMaxSlider, &QSlider::valueChanged, this, [this](int value) {
            setLutMax(value);
        });
    }
    if (controls.exposureSpin) {
        connect(controls.exposureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() {
            scheduleApplySettings();
        });
    }
    if (controls.readoutCombo) {
        connect(controls.readoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            scheduleApplySettings();
        });
    }
}

void CameraWorkspaceController::rebuildLut() {
    int rangeMax = lutRangeMax_.load();
    if (rangeMax <= 0) {
        rangeMax = 255;
    }
    const int minValue = std::clamp(lutMinValue_.load(), 0, rangeMax);
    const int maxValue = std::clamp(lutMaxValue_.load(), 0, rangeMax);
    bool enabled = (minValue > 0 || maxValue < rangeMax);
    std::array<unsigned char, 256> temp{};
    if (!enabled || maxValue <= minValue) {
        for (int i = 0; i < 256; ++i) {
            temp[i] = static_cast<unsigned char>(i);
        }
        enabled = false;
    } else {
        const double scale = 255.0 / static_cast<double>(rangeMax);
        const int min8 = std::clamp(static_cast<int>(std::lround(minValue * scale)), 0, 255);
        const int max8 = std::clamp(static_cast<int>(std::lround(maxValue * scale)), 0, 255);
        if (max8 <= min8) {
            for (int i = 0; i < 256; ++i) {
                temp[i] = static_cast<unsigned char>(i);
            }
            enabled = false;
        } else {
            for (int i = 0; i < 256; ++i) {
                if (i <= min8) {
                    temp[i] = 0;
                } else if (i >= max8) {
                    temp[i] = 255;
                } else {
                    temp[i] = static_cast<unsigned char>((i - min8) * 255 / (max8 - min8));
                }
            }
        }
    }
    {
        QMutexLocker lock(&lutMutex_);
        lutTable_ = temp;
    }
    lutEnabled_.store(enabled);
}

void CameraWorkspaceController::setLutMin(int value) {
    const auto& controls = deps_.controls;
    if (!controls.lutMinSpin || !controls.lutMinSlider || !controls.lutMaxSpin || !controls.lutMaxSlider) {
        return;
    }
    const int rangeMax = lutRangeMax_.load();
    value = std::clamp(value, 0, rangeMax);
    int maxValue = lutMaxValue_.load();
    if (value > maxValue) {
        maxValue = value;
        QSignalBlocker b1(controls.lutMaxSpin);
        QSignalBlocker b2(controls.lutMaxSlider);
        controls.lutMaxSpin->setValue(maxValue);
        controls.lutMaxSlider->setValue(maxValue);
        lutMaxValue_.store(maxValue);
    }
    {
        QSignalBlocker b1(controls.lutMinSpin);
        QSignalBlocker b2(controls.lutMinSlider);
        controls.lutMinSpin->setValue(value);
        controls.lutMinSlider->setValue(value);
    }
    lutMinValue_.store(value);
    rebuildLut();
}

void CameraWorkspaceController::setLutMax(int value) {
    const auto& controls = deps_.controls;
    if (!controls.lutMinSpin || !controls.lutMinSlider || !controls.lutMaxSpin || !controls.lutMaxSlider) {
        return;
    }
    const int rangeMax = lutRangeMax_.load();
    value = std::clamp(value, 0, rangeMax);
    int minValue = lutMinValue_.load();
    if (value < minValue) {
        minValue = value;
        QSignalBlocker b1(controls.lutMinSpin);
        QSignalBlocker b2(controls.lutMinSlider);
        controls.lutMinSpin->setValue(minValue);
        controls.lutMinSlider->setValue(minValue);
        lutMinValue_.store(minValue);
    }
    {
        QSignalBlocker b1(controls.lutMaxSpin);
        QSignalBlocker b2(controls.lutMaxSlider);
        controls.lutMaxSpin->setValue(value);
        controls.lutMaxSlider->setValue(value);
    }
    lutMaxValue_.store(value);
    rebuildLut();
}

void CameraWorkspaceController::applySettings() {
    const auto& controls = deps_.controls;
    if (!deps_.cameraWorker || !controls.presetCombo || !controls.binCombo || !controls.exposureSpin ||
        !controls.readoutCombo || !controls.customWidthSpin || !controls.customHeightSpin) {
        return;
    }

    const QSize preset = controls.presetCombo->currentData().toSize();
    const bool isCustom = preset.width() < 0 || preset.height() < 0;
    const int bits = currentBits();
    const double exposureMs = controls.exposureSpin->value();
    ApplySettings settings;
    settings.width = isCustom ? controls.customWidthSpin->value() : preset.width();
    settings.height = isCustom ? controls.customHeightSpin->value() : preset.height();
    settings.binning = controls.binCombo->currentText().toInt();
    settings.binningIndependent = false;
    settings.binH = settings.binning;
    settings.binV = settings.binning;
    settings.bits = bits;
    settings.pixelType = bits > 8 ? DCAM_PIXELTYPE_MONO16 : DCAM_PIXELTYPE_MONO8;
    settings.exposure_s = exposureMs / 1000.0;
    settings.readoutSpeed = controls.readoutCombo->currentData().toInt();
    settings.bundleEnabled = false;
    settings.bundleCount = 0;
    log(QString("Apply: preset=%1x%2 bin=%3 binH=%4 binV=%5 bits=%6 pixType=%7 exp_ms=%8 readout=%9")
            .arg(settings.width)
            .arg(settings.height)
            .arg(settings.binning)
            .arg(settings.binH)
            .arg(settings.binV)
            .arg(settings.bits)
            .arg(settings.pixelType)
            .arg(exposureMs, 0, 'f', 3)
            .arg(settings.readoutSpeed));
    if (deps_.statusLabel) {
        deps_.statusLabel->setText("Applying camera settings...");
    }
    QMetaObject::invokeMethod(deps_.cameraWorker,
                              [worker = deps_.cameraWorker,
                               settings,
                               displayEvery = controls.displayEverySpin ? controls.displayEverySpin->value() : 1]() {
                                  worker->setDisplayEvery(displayEvery);
                                  worker->applySettings(settings);
                              },
                              Qt::QueuedConnection);
    resetPipelineIfReady("Pipeline: warming (settings changed)");
}

void CameraWorkspaceController::scheduleApplySettings() {
    if (!autoApplyCamera_) {
        return;
    }
    if (deps_.viewerOnly && *deps_.viewerOnly) {
        return;
    }
    if (!deps_.cameraOpened || !*deps_.cameraOpened) {
        return;
    }
    applyTimer_->start();
}

void CameraWorkspaceController::setViewerOnly() {
    if (deps_.viewerOnly) {
        *deps_.viewerOnly = true;
    }
    if (deps_.appState) {
        deps_.appState->cameraStreaming = false;
    }
    if (deps_.statusLabel) {
        deps_.statusLabel->setText("Viewer-only mode (camera init failed).");
    }
    if (deps_.cameraStatusItem) {
        deps_.cameraStatusItem->setText("Camera: unavailable");
    }
    if (deps_.runStatusItem) {
        deps_.runStatusItem->setText("Run: viewer-only");
    }
    showStatusMessage("Viewer-only mode");
    if (deps_.startButton) {
        deps_.startButton->setEnabled(false);
    }
    if (deps_.stopButton) {
        deps_.stopButton->setEnabled(false);
    }
    if (deps_.reconnectButton) {
        deps_.reconnectButton->setEnabled(false);
    }
    if (deps_.applyButton) {
        deps_.applyButton->setEnabled(false);
    }
    if (deps_.operationalTabs) {
        deps_.operationalTabs->setEnabled(false);
    }
}

void CameraWorkspaceController::setHardwareFreeMode() {
    if (deps_.appState) {
        deps_.appState->testMode = true;
        deps_.appState->cameraStreaming = false;
        deps_.appState->daqAvailable = false;
        deps_.appState->daqDisabled = deps_.options && deps_.options->noDaq;
        deps_.appState->daqFault = deps_.options && !deps_.options->noDaq && !deps_.daqBuildEnabled;
        deps_.appState->daqStatusText = deps_.initialDaqStatusText;
    }
    const bool mockCamera = deps_.options && deps_.options->mockCamera;
    if (deps_.statusLabel) {
        deps_.statusLabel->setText(mockCamera ? "Test mode: mock camera active."
                                              : "Test mode: camera startup skipped.");
    }
    if (deps_.cameraStatusItem) {
        deps_.cameraStatusItem->setText(mockCamera ? "Camera: mock" : "Camera: unavailable");
    }
    if (deps_.cameraOpened) {
        *deps_.cameraOpened = false;
    }
    if (deps_.modelStatusItem) {
        deps_.modelStatusItem->setText("Model: not loaded");
    }
    if (deps_.daqStatusItem) {
        deps_.daqStatusItem->setText(deps_.initialDaqStatusText);
    }
    if (deps_.runStatusItem) {
        deps_.runStatusItem->setText("Run: idle");
    }
    showStatusMessage("Hardware-free test mode");
    log("Hardware-free GUI test mode active; startup camera prompts suppressed.");
}

void CameraWorkspaceController::resetPipelineIfReady(const QString& statusText) {
    if (!deps_.pipeline || !deps_.pipeline->isReady() || !deps_.pipelineMutex) {
        return;
    }
    QMutexLocker lock(deps_.pipelineMutex);
    deps_.pipeline->reset();
    if (deps_.pipelineStatusLabel) {
        deps_.pipelineStatusLabel->setText(statusText);
    }
}

void CameraWorkspaceController::showStatusMessage(const QString& message) {
    if (deps_.statusBar) {
        deps_.statusBar->showMessage(message);
    }
}

void CameraWorkspaceController::log(const QString& message) {
    if (deps_.logLine) {
        deps_.logLine(message);
    }
}

void CameraWorkspaceController::systemLog(const QString& message) {
    if (deps_.systemLogLine) {
        deps_.systemLogLine(message);
    }
}
