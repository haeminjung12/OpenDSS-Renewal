#include "../v2/camera/camera_controller.h"
#include "../v2/camera/camera_preview_image_provider.h"
#include "../v2/camera/camera_service.h"
#include "../v2/live/live_sorting_controller.h"
#include "../v2/live/live_sorting_service.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/run/run_manifest_v2.h"
#include "../v2/state/application_state_store.h"
#include "../detection/droplet_detector.h"
#include "../desktop_app/json_persistence.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char* message) {
    if (!condition)
        fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

bool waitFor(const std::function<bool()>& predicate, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();
    return predicate();
}

bool waitForIdle(CameraController& camera) {
    return waitFor([&camera] { return !camera.busy(); });
}

class ControlledCamera final : public ICameraDevice {
public:
    QString deviceId() const override { return QStringLiteral("controlled-camera"); }
    bool open(QString*) override {
        opened_ = true;
        return true;
    }
    bool start(QString*) override {
        streaming_ = true;
        return true;
    }
    bool stop(QString*) override {
        streaming_ = false;
        return true;
    }
    bool close(QString*) override {
        streaming_ = false;
        opened_ = false;
        return true;
    }
    CameraFrameResult latestFrame(CameraFrame& frame, QString* error) override {
        const quint64 available = delivery.load(std::memory_order_acquire);
        if (!streaming_ || available == 0 || available == lastDelivered_) {
            if (error)
                error->clear();
            return CameraFrameResult::NoFrame;
        }
        lastDelivered_ = available;
        frame.pixelFormat = CameraPixelFormat::Mono8;
        frame.width = 8;
        frame.height = 8;
        frame.rowBytes = 8;
        frame.bitDepth = 8;
        frame.deliveryId = available;
        frame.monotonicTimestampNs =
            timestampNs.load(std::memory_order_acquire);
        frame.bytes = QByteArray(64, static_cast<char>(
                                       pixel.load(std::memory_order_acquire)));
        return CameraFrameResult::Frame;
    }

    std::atomic<quint64> delivery{0};
    std::atomic<qint64> timestampNs{0};
    std::atomic<int> pixel{0};

private:
    bool opened_ = false;
    bool streaming_ = false;
    quint64 lastDelivered_ = 0;
};

class RecordingDetector final : public IDropletDetector {
public:
    void reset() override {
        std::lock_guard lock(mutex_);
        pixels_.clear();
        processed.store(0);
    }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat& frame) override {
        {
            std::lock_guard lock(mutex_);
            pixels_.push_back(frame.at<uchar>(0, 0));
        }
        processed.fetch_add(1, std::memory_order_release);
        return {};
    }
    QVector<int> pixels() const {
        std::lock_guard lock(mutex_);
        return pixels_;
    }

    std::atomic_int processed{0};

private:
    mutable std::mutex mutex_;
    QVector<int> pixels_;
};

live::PreparedLiveModel preparedModel() {
    live::PreparedLiveModel model;
    model.snapshot = {
        QStringLiteral("model-id"), QStringLiteral("Active Model"),
        QString(64, QLatin1Char('a')),
        {{QStringLiteral("c0"), QStringLiteral("Zero")},
         {QStringLiteral("c1"), QStringLiteral("One")}}};
    model.classify = [](const cv::Mat&, QString*) {
        return std::optional<live::LiveInferenceResult>(
            live::LiveInferenceResult{{0.8, 0.2}});
    };
    return model;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    bool ok = true;

    QTemporaryDir runs;
    ApplicationStateStore stateStore;
    OperationCoordinator operations;
    RecordingDetector detector;
    bool daqReady = false;
    int pulseCalls = 0;
    std::mutex persistenceMutex;
    std::condition_variable persistenceCondition;
    bool blockPersistence = false;
    bool persistenceEntered = false;
    bool releasePersistence = false;
    auto device = std::make_unique<ControlledCamera>();
    ControlledCamera* controlled = device.get();
    auto* cameraService = new CameraService(std::move(device), stateStore);
    CameraPreviewImageProvider previewProvider;
    CameraController camera(*cameraService, previewProvider);
    QThread cameraThread;
    cameraService->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished, cameraService,
                     &QObject::deleteLater);
    cameraThread.start();

    live::LiveSortingService service(
        operations, detector, nullptr,
        [&](bool outputEnabled, QString*) {
            ++pulseCalls;
            return outputEnabled ? run::DaqPulseStatus::Issued
                                 : run::DaqPulseStatus::SuppressedNotIssued;
        },
        [](QString*) {
            return std::optional<live::PreparedLiveModel>(preparedModel());
        },
        [&](QString*) {
            std::unique_lock lock(persistenceMutex);
            if (!blockPersistence)
                return true;
            persistenceEntered = true;
            persistenceCondition.notify_all();
            persistenceCondition.wait(lock, [&] { return releasePersistence; });
            return true;
        },
        {}, [&](QString* error) {
            if (!daqReady && error)
                *error = QStringLiteral("DAQ fixture is not ready.");
            return daqReady;
        });

    live::LiveControllerFacts facts;
    facts.defaultRunRoot = runs.path();
    facts.opendssVersion = QStringLiteral("2");
    facts.hitBoundary = {4.0, run::HitSide::PositiveY, 8, 8};
    facts.detectorSettings = {{QStringLiteral("fixed"), true}};
    facts.cropSettings = {{QStringLiteral("size"), 64}};
    facts.timingSettings = {{QStringLiteral("fixed"), true}};
    facts.cameraSettings = {{QStringLiteral("source"), QStringLiteral("fixture")}};
    facts.daqSettings = {{QStringLiteral("source"), QStringLiteral("fixture")}};
    facts.activeModelId = QStringLiteral("model-id");
    int cameraProfileApplies = 0;
    int daqProfileApplies = 0;
    int modelProfileApplies = 0;
    facts.applyCameraProfile = [&](const QJsonObject& value, QString*) {
        ++cameraProfileApplies;
        return value.value(QStringLiteral("source")).toString() ==
               QStringLiteral("fixture");
    };
    facts.applyDaqProfile = [&](const QJsonObject& value, QString*) {
        ++daqProfileApplies;
        return value.value(QStringLiteral("source")).toString() ==
               QStringLiteral("fixture");
    };
    facts.activateModel = [&](const QString& id, QString*) {
        ++modelProfileApplies;
        facts.activeModelId = id;
        return id == QStringLiteral("model-id");
    };
    facts.nominalCameraFps = 25.0;
    int resultsRefreshes = 0;
    QString refreshedRun;

    auto controller = std::make_unique<live::LiveSortingController>(
        service, camera, [&] { return facts; },
        [&](QString* error) {
            if (!daqReady && error)
                *error = QStringLiteral("DAQ fixture is not ready.");
            return daqReady;
        },
        [&](const QString& runFolder) {
            ++resultsRefreshes;
            refreshedRun = runFolder;
        });
    const auto armPersistenceBlock = [&] {
        std::lock_guard lock(persistenceMutex);
        blockPersistence = true;
        persistenceEntered = false;
        releasePersistence = false;
    };
    const auto waitForPersistenceBlock = [&] {
        return waitFor([&] {
            std::lock_guard lock(persistenceMutex);
            return persistenceEntered;
        });
    };
    const auto releasePersistenceBlock = [&] {
        std::lock_guard lock(persistenceMutex);
        blockPersistence = false;
        releasePersistence = true;
        persistenceCondition.notify_all();
    };

    const QString profilePath =
        QDir(runs.path()).filePath(QStringLiteral("setup-profile.json"));
    controller->setRunName(QStringLiteral("Profile Run"));
    controller->setSaveLocation(runs.path());
    controller->setHitClassId(QStringLiteral("c1"));
    controller->setTriggerEveryDroplet(false);
    controller->setDaqOutputEnabled(true);
    controller->setRecordFullImageSequence(true);
    ok &= check(controller->saveProfileAs(QUrl::fromLocalFile(profilePath)) &&
                    controller->canSaveProfile() &&
                    QFileInfo::exists(profilePath),
                "Save Profile As must atomically create a current v2 profile.");
    QFile profileFile(profilePath);
    ok &= check(profileFile.open(QIODevice::ReadOnly),
                "Saved Setup Profile must be readable.");
    const QJsonObject savedProfile =
        QJsonDocument::fromJson(profileFile.readAll()).object();
    profileFile.close();
    ok &= check(savedProfile.value(QStringLiteral("schema_version")).toString()
                            == QStringLiteral("opendss.setup_profile.v2")
                    && !savedProfile.contains(QStringLiteral("hit_boundary"))
                    && savedProfile.value(QStringLiteral("detector_settings"))
                           == facts.detectorSettings
                    && savedProfile.value(QStringLiteral("crop_settings"))
                           == facts.cropSettings
                    && savedProfile.value(QStringLiteral("timing_settings"))
                           == facts.timingSettings,
                "Saved Setup Profile must include scientific snapshots without Decision Boundary coordinates.");
    controller->setRunName(QStringLiteral("Changed"));
    controller->setTriggerEveryDroplet(true);
    controller->setDaqOutputEnabled(false);
    controller->setRecordFullImageSequence(false);
    facts.activeModelId.clear();
    controller->refresh();
    ok &= check(controller->openProfile(QUrl::fromLocalFile(profilePath)) &&
                    controller->runName() == QStringLiteral("Profile Run") &&
                    !controller->triggerEveryDroplet() &&
                    controller->daqOutputEnabled() &&
                    controller->recordFullImageSequence() &&
                    cameraProfileApplies == 1 && daqProfileApplies == 1 &&
                    modelProfileApplies == 1,
                "Open Profile must round-trip selections and apply readable hardware/model facts.");

    QJsonObject partialProfile = savedProfile;
    partialProfile.insert(QStringLiteral("hit_boundary"),
                          QJsonObject{{QStringLiteral("boundary_y"), 3.0}});
    partialProfile.insert(QStringLiteral("detector_settings"),
                          QStringLiteral("invalid"));
    partialProfile.insert(QStringLiteral("crop_settings"),
                          QJsonObject{{QStringLiteral("size"), 96}});
    partialProfile.insert(QStringLiteral("timing_settings"),
                          QJsonObject{{QStringLiteral("fixed"), false}});
    const QString partialPath =
        QDir(runs.path()).filePath(QStringLiteral("partial-profile.json"));
    QString persistenceError;
    ok &= check(desktop_app::writeJsonObjectAtomically(
                    partialPath, partialProfile, &persistenceError)
                    && controller->openProfile(QUrl::fromLocalFile(partialPath))
                    && controller->profileStatus().contains(
                        QStringLiteral("Detector values not applied")),
                "A partially invalid profile must report the skipped section and retain readable sections.");
    const QString roundTripPath =
        QDir(runs.path()).filePath(QStringLiteral("partial-round-trip.json"));
    ok &= check(controller->saveProfileAs(QUrl::fromLocalFile(roundTripPath)),
                "Partially applied profile state must remain saveable.");
    QFile roundTripFile(roundTripPath);
    ok &= check(roundTripFile.open(QIODevice::ReadOnly),
                "Partially applied profile round trip must be readable.");
    const QJsonObject roundTrip =
        QJsonDocument::fromJson(roundTripFile.readAll()).object();
    roundTripFile.close();
    ok &= check(!roundTrip.contains(QStringLiteral("hit_boundary"))
                    && roundTrip.value(QStringLiteral("detector_settings"))
                           == facts.detectorSettings
                    && roundTrip.value(QStringLiteral("crop_settings"))
                           == QJsonObject{{QStringLiteral("size"), 96}}
                    && roundTrip.value(QStringLiteral("timing_settings"))
                           == QJsonObject{{QStringLiteral("fixed"), false}},
                "Profile load must apply valid scientific sections without retaining Decision Boundary coordinates.");
    controller->setDaqOutputEnabled(false);
    controller->setTriggerEveryDroplet(true);

    ok &= check(camera.open() && waitForIdle(camera) &&
                    camera.cameraStatus() != QStringLiteral("Unavailable"),
                "Camera fixture must open to Ready.");
    ok &= check(controller->primaryAction() && waitForIdle(camera) &&
                    waitFor([&] { return controller->cameraStreaming(); }),
                "Ready primary action must start Camera streaming.");
    ok &= check(!controller->decisionBoundaryDefined() &&
                    !controller->startSortingEnabled() &&
                    controller->disabledReason() ==
                        QStringLiteral("No Decision Boundary set") &&
                    controller->setDecisionBoundary(0.25, 0.75) &&
                    controller->decisionBoundaryDefined() &&
                    controller->decisionBoundaryXRatio() == 0.25 &&
                    controller->decisionBoundaryYRatio() == 0.75,
                "Live requires explicit source-relative Decision Boundary placement.");
    ok &= check(controller->presentation() == QStringLiteral("ready") &&
                    controller->startSortingEnabled(),
                "DAQ OFF must permit a technically ready Live start.");

    controller->setRunName(QStringLiteral("Controller Run"));
    controller->setExperimentType(QStringLiteral("sorting"));
    controller->setNotes(QStringLiteral("controller contract"));
    controller->setDuration(QStringLiteral("30"));
    controller->setRecordFullImageSequence(true);
    controller->setDaqOutputEnabled(true);
    ok &= check(!controller->startSortingEnabled() &&
                    !controller->startSorting() &&
                    controller->error().contains(QStringLiteral("DAQ fixture")),
                "DAQ ON must require the readiness callback.");
    controller->setDaqOutputEnabled(false);
    ok &= check(controller->startSortingEnabled() &&
                    controller->secondaryAction() &&
                    controller->presentation() == QStringLiteral("running"),
                "Secondary ready action must construct and start a DAQ-OFF request.");

    controlled->pixel = 11;
    controlled->timestampNs = 1'000'000'000;
    controlled->delivery = 1;
    ok &= check(waitFor([&] {
                    return detector.processed.load() == 1 &&
                           camera.latestDeliveryId() == 1 &&
                           !camera.previewSource().isEmpty();
                }),
                "First CameraFrame must reach the Live service.");
    camera.acknowledgePreviewReady(camera.previewSource());
    controlled->pixel = 22;
    controlled->timestampNs = 1'040'000'000;
    controlled->delivery = 2;
    ok &= check(waitFor([&] {
                    return detector.processed.load() == 2 &&
                           camera.latestDeliveryId() == 2 &&
                           !camera.previewSource().isEmpty();
                }),
                "Second CameraFrame must preserve queued delivery order.");
    camera.acknowledgePreviewReady(camera.previewSource());
    ok &= check(controller->primaryAction() &&
                    waitFor([&] {
                        return controller->presentation() ==
                               QStringLiteral("paused");
                    }),
                "Running primary action must pause the authoritative service.");
    controlled->pixel = 99;
    controlled->timestampNs = 1'080'000'000;
    controlled->delivery = 3;
    ok &= check(waitFor([&] { return camera.latestDeliveryId() == 3; }),
                "Paused CameraFrame must still reach the preview facade.");
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    ok &= check(detector.processed.load() == 2,
                "Camera preview delivery must not enter the service while paused.");
    ok &= check(controller->primaryAction() &&
                    waitFor([&] {
                        return controller->presentation() ==
                               QStringLiteral("running");
                    }),
                "Paused primary action must resume the same Run.");
    controlled->pixel = 44;
    controlled->timestampNs = 1'120'000'000;
    controlled->delivery = 4;
    ok &= check(waitFor([&] { return detector.processed.load() == 3; }),
                "A resumed CameraFrame must continue the same service.");

    ok &= check(controller->secondaryAction() &&
                    waitFor([&] {
                        return controller->presentation() ==
                               QStringLiteral("completed");
                    }) &&
                    controller->stopReason() == QStringLiteral("user") &&
                    !controller->runFolder().isEmpty() &&
                    resultsRefreshes == 1 &&
                    refreshedRun == controller->runFolder(),
                "Stop must finalize, project the result, and refresh Results once.");
    const QVector<int> adaptedPixels = detector.pixels();
    const qint64 adaptedGaps =
        controller->integrity()
            .value(QStringLiteral("sourceFrameGaps"))
            .toMap()
            .value(QStringLiteral("count"))
            .toLongLong();
    if (adaptedPixels != QVector<int>({11, 22, 44}) || adaptedGaps != 0) {
        fprintf(stderr, "adapted pixels:");
        for (const int value : adaptedPixels)
            fprintf(stderr, " %d", value);
        fprintf(stderr, "; gaps=%lld\n", static_cast<long long>(adaptedGaps));
    }
    ok &= check(adaptedPixels == QVector<int>({11, 22, 44}) &&
                    adaptedGaps == 0,
                "Frame adaptation must retain pixels/order and exclude paused delivery gaps.");

    QString manifestError;
    auto manifest = run::RunManifestV2::load(
        QDir(controller->runFolder())
            .filePath(QStringLiteral("run_summary.json")),
        &manifestError);
    ok &= check(manifest.has_value() &&
                    manifest->data().runName == QStringLiteral("Controller Run") &&
                    manifest->data().experimentType == QStringLiteral("sorting") &&
                    manifest->data().notes == QStringLiteral("controller contract") &&
                    !manifest->data().routing.physicalDaqOutputEnabled &&
                    manifest->data().files.sequencePath.has_value(),
                "Controller drafts must construct the exact accepted Live request.");

    controller->startNewRun();
    ok &= check(controller->presentation() == QStringLiteral("ready") &&
                    controller->primaryAction() && waitForIdle(camera) &&
                    !controller->cameraStreaming(),
                "Start New Run must restore pre-run actions and permit Stop Camera.");
    ok &= check(controller->primaryAction() && waitForIdle(camera) &&
                    waitFor([&] { return controller->cameraStreaming(); }),
                "Ready primary action must restart Camera streaming.");

    facts.activeModelLoadable = true;
    facts.activeModelName = QStringLiteral("Active Model");
    facts.activeModelClasses = {
        {QStringLiteral("route-z"), QStringLiteral("Multiple")},
        {QStringLiteral("route-hit"), QStringLiteral("Single")},
        {QStringLiteral("route-a"), QStringLiteral("Empty")}};
    controller->refresh();
    const QVariantList arbitraryClassModel = controller->hitClassModel();
    ok &= check(
        arbitraryClassModel.size() == 3 &&
            arbitraryClassModel.at(0).toMap().value(QStringLiteral("id")).toString() ==
                QStringLiteral("route-z") &&
            arbitraryClassModel.at(1).toMap().value(QStringLiteral("id")).toString() ==
                QStringLiteral("route-hit") &&
            arbitraryClassModel.at(1).toMap().value(QStringLiteral("name")).toString() ==
                QStringLiteral("Single") &&
            arbitraryClassModel.at(2).toMap().value(QStringLiteral("id")).toString() ==
                QStringLiteral("route-a"),
        "Hit Class model must preserve authoritative arbitrary IDs, order, and names.");

    facts.activeModelClasses = {
        {QStringLiteral("c0"), QStringLiteral("Zero")},
        {QStringLiteral("c1"), QStringLiteral("One")}};
    controller->refresh();
    controller->setTriggerEveryDroplet(false);
    controller->setHitClassId(QStringLiteral("c1"));
    controller->setDaqOutputEnabled(true);
    daqReady = true;
    controller->setRecordFullImageSequence(false);
    ok &= check(controller->activeModelText() == QStringLiteral("Active Model") &&
                    controller->hitClassOptions() ==
                        QStringList({QStringLiteral("Zero"), QStringLiteral("One")}) &&
                    controller->startSortingEnabled() &&
                    controller->startSorting(),
                "Class-Based DAQ-ON preflight must use injected model and DAQ facts.");
    ok &= check(controller->stopSorting() &&
                    waitFor([&] {
                        return controller->presentation() ==
                               QStringLiteral("completed");
                    }),
                "Explicit lifecycle actions must stop a Class-Based Run.");
    ok &= check(pulseCalls == 0,
                "No detection event must issue a physical pulse.");

    controller->startNewRun();
    controller->setTriggerEveryDroplet(true);
    controller->setDaqOutputEnabled(false);
    ok &= check(controller->startSorting(),
                "Frame conversion fault fixture Run must start.");
    CameraFrame invalidFrame;
    invalidFrame.pixelFormat = CameraPixelFormat::Mono8;
    invalidFrame.width = 8;
    invalidFrame.height = 8;
    invalidFrame.rowBytes = 1;
    invalidFrame.bitDepth = 8;
    invalidFrame.deliveryId = 5;
    invalidFrame.monotonicTimestampNs = 2'000'000'000;
    invalidFrame.bytes = QByteArray(8, '\0');
    emit camera.frameReady(invalidFrame);
    ok &= check(waitFor([&] {
                    return controller->presentation() ==
                           QStringLiteral("error");
                }) &&
                    !controller->error().isEmpty() &&
                    service.snapshot().lifecycle == OperationLifecycle::Failed &&
                    resultsRefreshes == 3,
                "Frame adaptation faults must fail/finalize truthfully and refresh Results.");

    controller->startNewRun();
    controller->setDuration(QStringLiteral("30"));
    controller->setRecordFullImageSequence(true);
    armPersistenceBlock();
    ok &= check(controller->startSorting(),
                "Responsive lifecycle fixture Run must start.");
    controlled->pixel = 55;
    controlled->timestampNs = 3'000'000'000;
    controlled->delivery = 6;
    ok &= check(waitForPersistenceBlock(),
                "Responsive Pause fixture must block persistence drain.");
    QElapsedTimer actionTimer;
    actionTimer.start();
    ok &= check(controller->pauseSorting() && actionTimer.elapsed() < 100,
                "Pause invokable must return promptly while drain is blocked.");
    releasePersistenceBlock();
    ok &= check(waitFor([&] {
                    return controller->presentation() ==
                           QStringLiteral("paused");
                }),
                "Blocked Pause must eventually project Paused.");
    actionTimer.restart();
    ok &= check(controller->resumeSorting() && actionTimer.elapsed() < 100 &&
                    waitFor([&] {
                        return controller->presentation() ==
                               QStringLiteral("running");
                    }),
                "Resume invokable must return promptly and project Running.");

    armPersistenceBlock();
    controlled->pixel = 66;
    controlled->timestampNs = 3'040'000'000;
    controlled->delivery = 7;
    ok &= check(waitForPersistenceBlock(),
                "Responsive Stop fixture must block persistence drain.");
    actionTimer.restart();
    ok &= check(controller->stopSorting() && actionTimer.elapsed() < 100,
                "Stop invokable must return promptly while drain is blocked.");
    releasePersistenceBlock();
    ok &= check(waitFor([&] {
                    return controller->presentation() ==
                           QStringLiteral("completed");
                }),
                "Blocked Stop must eventually finalize and project completion.");

    controller->startNewRun();
    controller->setDuration(QStringLiteral("0.05"));
    armPersistenceBlock();
    ok &= check(controller->startSorting(),
                "Timed completion fixture Run must start.");
    controlled->pixel = 77;
    controlled->timestampNs = 4'000'000'000;
    controlled->delivery = 8;
    ok &= check(waitForPersistenceBlock(),
                "Timed completion fixture must block final drain.");
    bool uiTimerFired = false;
    QTimer::singleShot(250, [&] { uiTimerFired = true; });
    ok &= check(
        waitFor([&] { return uiTimerFired; }, 1000) &&
            (service.snapshot().lifecycle == OperationLifecycle::Running ||
             service.snapshot().lifecycle == OperationLifecycle::Stopping),
                "Timed service finalization must not block the QObject thread.");
    releasePersistenceBlock();
    ok &= check(waitFor([&] {
                    return controller->presentation() ==
                           QStringLiteral("completed");
                }),
                "Timed completion must eventually project its final state.");

    controller->startNewRun();
    controller->setDuration(QStringLiteral("30"));
    armPersistenceBlock();
    ok &= check(controller->startSorting(), "Teardown fixture Run must start.");
    controlled->pixel = 88;
    controlled->timestampNs = 5'000'000'000;
    controlled->delivery = 9;
    ok &= check(waitForPersistenceBlock(),
                "Teardown fixture must own a blocked service action.");
    std::thread teardownRelease([&] {
        QThread::msleep(50);
        releasePersistenceBlock();
    });
    controller.reset();
    teardownRelease.join();
    ok &= check(service.snapshot().lifecycle == OperationLifecycle::Completed &&
                    !operations.snapshot().kind.has_value(),
                "Controller teardown must safely stop, join, and release Live ownership.");

    camera.close();
    waitForIdle(camera);
    cameraThread.quit();
    cameraThread.wait();
    return ok ? 0 : 1;
}
