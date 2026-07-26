// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include <memory>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "autogen/environment.h"
#include "../../detection/droplet_detector_adapters.h"
#include "../../desktop_app/model_registry_service.h"
#include "../../desktop_app/pipeline_runner.h"
#include "../../v2/camera/camera_controller.h"
#include "../../v2/camera/camera_preview_image_provider.h"
#include "../../v2/camera/camera_service.h"
#include "../../v2/camera/dcam_camera_device.h"
#include "../../v2/camera/single_image_capture_controller.h"
#include "../../v2/camera/single_image_capture_service.h"
#include "../../v2/dataset/dataset_label_controller.h"
#include "../../v2/hardware/daq_controller.h"
#include "../../v2/hardware/daq_output.h"
#include "../../v2/hardware/daq_service.h"
#include "../../v2/live/live_sorting_controller.h"
#include "../../v2/live/live_sorting_service.h"
#include "../../v2/model/model_library_controller.h"
#include "../../v2/model/model_load_service.h"
#include "../../v2/model_test/model_test_controller.h"
#include "../../v2/operation/operation_coordinator.h"
#include "../../v2/settings/settings_controller.h"
#include "../../v2/settings/settings_repository.h"
#include "../../v2/results/run_repository.h"
#include "../../v2/results/runs_results_controller.h"
#include "../../v2/sequence/sequence_viewer_controller.h"
#include "../../v2/sequence/sequence_viewer_image_provider.h"
#include "../../v2/sequence_test/sequence_test_controller.h"
#include "../../v2/sequence_test/sequence_test_service.h"
#include "../../v2/state/application_state_store.h"
#include "../../v2/training/training_controller.h"

namespace {

QString repositoryRoot()
{
    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 10; ++level) {
        if (QFileInfo(directory.filePath(
                          QStringLiteral("training/python/droplet_trainer/__main__.py")))
                .isFile()) {
            return directory.absolutePath();
        }
        if (!directory.cdUp())
            break;
    }
    return {};
}

QString trainingPythonExecutable()
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (localAppData.isEmpty())
        return {};

    const QDir root(localAppData);
    const QString gpuPython = root.filePath(
        QStringLiteral("OpenVisualDropletSorter/training-venv-gpu/Scripts/python.exe"));
    if (QFileInfo(gpuPython).isFile())
        return gpuPython;

    const QString cpuPython = root.filePath(
        QStringLiteral("OpenVisualDropletSorter/training-venv/Scripts/python.exe"));
    return QFileInfo(cpuPython).isFile() ? cpuPython : QString{};
}

QJsonObject provisionalFastDetectorSettings(const FastEventConfig &config)
{
    return {
        {QStringLiteral("configuration_id"),
         QStringLiteral("fast_event_detector_defaults_v1")},
        {QStringLiteral("qualification"), QStringLiteral("PROVISIONAL")},
        {QStringLiteral("bg_frames"), config.bgFrames},
        {QStringLiteral("bg_update_frames"), config.bgUpdateFrames},
        {QStringLiteral("reset_frames"), config.resetFrames},
        {QStringLiteral("min_area"), config.minArea},
        {QStringLiteral("min_area_fraction"), config.minAreaFrac},
        {QStringLiteral("max_area_fraction"), config.maxAreaFrac},
        {QStringLiteral("minimum_bounding_box"), config.minBbox},
        {QStringLiteral("margin"), config.margin},
        {QStringLiteral("difference_threshold"), config.diffThresh},
        {QStringLiteral("blur_radius"), config.blurRadius},
        {QStringLiteral("morphology_radius"), config.morphRadius},
        {QStringLiteral("contour_extraction"), config.useContourExtraction},
        {QStringLiteral("scale"), config.scale},
        {QStringLiteral("gap_fire_shift"), config.gapFireShift},
    };
}

std::optional<desktop_app::v2::run::ModelSnapshot>
activeModelSnapshot(const QString &registryFilePath,
                    desktop_app::v2::ModelLoadService &modelLoader,
                    QString *error)
{
    if (error)
        error->clear();
    const auto inspection = modelLoader.inspectPersistedActive();
    if (!inspection.loadable) {
        if (error)
            *error = inspection.error;
        return std::nullopt;
    }

    QString warning;
    const QJsonArray entries =
        readModelRegistryEntriesFromPath(registryFilePath, &warning);
    const QJsonObject entry = activeRegistryEntry(entries);
    if (!entry.value(QStringLiteral("active")).toBool(false) ||
        registryString(entry, QStringLiteral("registry_entry_id")) != inspection.id) {
        if (error)
            *error = QStringLiteral("The authoritative Active Model registry entry is unavailable.");
        return std::nullopt;
    }

    desktop_app::v2::run::ModelSnapshot snapshot;
    snapshot.id = inspection.id;
    snapshot.name = inspection.displayName;
    snapshot.sha256 =
        registryString(entry, QStringLiteral("model_sha256")).toLower();
    const QJsonArray classes = entry.value(QStringLiteral("classes")).toArray();
    const QJsonObject labels =
        entry.value(QStringLiteral("display_labels")).toObject();
    for (const QJsonValue &value : classes) {
        const QString id = value.toString().trimmed();
        const QString name = labels.value(id).toString().trimmed();
        if (id.isEmpty() || name.isEmpty()) {
            if (error)
                *error = QStringLiteral("The Active Model class labels are incomplete.");
            return std::nullopt;
        }
        snapshot.classes.push_back({id, name});
    }
    if ((snapshot.classes.size() != 2 && snapshot.classes.size() != 3) ||
        snapshot.sha256.size() != 64) {
        if (error)
            *error = warning.isEmpty()
                         ? QStringLiteral("The Active Model provenance is incomplete.")
                         : warning;
        return std::nullopt;
    }
    return snapshot;
}

quint64 availablePhysicalMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullAvailPhys : 0;
#else
    return 0;
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    set_qt_environment();
    QApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0"));

    desktop_app::v2::ApplicationStateStore applicationStateStore;
    desktop_app::v2::OperationCoordinator operationCoordinator;
    desktop_app::v2::DaqService daqService(
        operationCoordinator, applicationStateStore,
        std::make_unique<desktop_app::v2::DaqTriggerOutput>());
    desktop_app::v2::DaqController daqController(
        daqService, applicationStateStore, operationCoordinator);
    QThread cameraThread;
    auto *cameraService = new desktop_app::v2::CameraService(
        std::make_unique<desktop_app::v2::DcamCameraDevice>(), applicationStateStore);
    auto *cameraPreviewProvider = new desktop_app::v2::CameraPreviewImageProvider;
    desktop_app::v2::CameraController cameraController(*cameraService,
                                                       *cameraPreviewProvider);
    desktop_app::v2::SingleImageCaptureService singleImageCaptureService;
    desktop_app::v2::SingleImageCaptureController singleImageCaptureController(
        singleImageCaptureService, cameraController, operationCoordinator);
    singleImageCaptureController.initializeDefaultOutputFolder(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    cameraService->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished,
                     cameraService, &QObject::deleteLater);
    cameraThread.start();
    cameraController.open();

    const QString preferencesFilePath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                            .filePath(QStringLiteral("preferences.json"));
    desktop_app::v2::SettingsRepository settingsRepository(preferencesFilePath, applicationStateStore);
    settingsRepository.load();
    desktop_app::v2::SettingsController settingsController(settingsRepository, applicationStateStore);
    desktop_app::v2::results::RunRepository runRepository(applicationStateStore);
    desktop_app::v2::results::RunsResultsController runsResultsController(runRepository, applicationStateStore);
    desktop_app::v2::sequence::SequenceViewerController sequenceViewerController;
    desktop_app::v2::dataset::DatasetLabelController datasetLabelController(operationCoordinator,
                                                                            applicationStateStore);
    loadModelRegistry();
    const QString registryFilePath = modelRegistryPath();
    desktop_app::v2::ModelLibraryController modelLibraryController(
        registryFilePath, operationCoordinator);
    desktop_app::v2::ModelLoadService modelLoadService(registryFilePath);
    PipelineRunner pipeline;
    modelLibraryController.setActiveModelClearedCallback([&applicationStateStore,
                                                          &pipeline]() {
        pipeline.clear();
        pipeline.installInference(nullptr);
        applicationStateStore.publishActiveModel({});
    });
    desktop_app::v2::training::TrainingController trainingController(
        operationCoordinator, applicationStateStore, modelLoadService, pipeline,
        modelLibraryController, trainingPythonExecutable(), repositoryRoot());
    desktop_app::v2::model_test::ModelTestController modelTestController(
        operationCoordinator, modelLoadService, QCoreApplication::applicationVersion());
    QObject::connect(&modelLibraryController,
                     &desktop_app::v2::ModelLibraryController::changed,
                     &modelTestController,
                     &desktop_app::v2::model_test::ModelTestController::refreshPreflight);

    const FastEventConfig fastEventConfig{};
    FastEventDetectorAdapter fastDetector(fastEventConfig);
    const QJsonObject detectorSettings =
        provisionalFastDetectorSettings(fastEventConfig);
    const QJsonObject cropSettings{
        {QStringLiteral("configuration_id"),
         QStringLiteral("fast_event_detector_bbox_crop_v1")},
        {QStringLiteral("qualification"), QStringLiteral("PROVISIONAL")},
    };
    const QJsonObject timingSettings{
        {QStringLiteral("configuration_id"),
         QStringLiteral("fast_event_detector_timing_v1")},
        {QStringLiteral("qualification"), QStringLiteral("PROVISIONAL")},
    };
    const auto daqReadiness = [&daqService](QString *error) {
        const bool ready = daqService.ready();
        if (!ready && error)
            *error = QStringLiteral("DAQ is not ready.");
        return ready;
    };
    const auto hitPulse = [&daqService](bool enabled, QString *error) {
        return daqService.issueLiveHit(enabled, error);
    };
    desktop_app::v2::live::LiveSortingService liveSortingService(
        operationCoordinator, fastDetector, &modelLoadService, hitPulse, {}, {},
        {}, daqReadiness);
    desktop_app::v2::sequence_test::SequenceTestService sequenceTestService(
        operationCoordinator, fastDetector, &modelLoadService, {}, hitPulse,
        daqReadiness);

    int latestCameraWidth = 0;
    int latestCameraHeight = 0;
    int latestCameraBitDepth = 0;

    desktop_app::v2::live::LiveSortingController liveSortingController(
        liveSortingService, cameraController,
        [&] {
            desktop_app::v2::live::LiveControllerFacts facts;
            facts.defaultRunRoot = defaultOpenDssRunsPath();
            facts.opendssVersion = QCoreApplication::applicationVersion();
            QString modelError;
            if (const auto model =
                    activeModelSnapshot(registryFilePath, modelLoadService,
                                        &modelError)) {
                facts.activeModelName = model->name;
                facts.activeModelClasses = model->classes;
                facts.activeModelLoadable = true;
            }
            const int width = latestCameraWidth;
            const int height = latestCameraHeight;
            if (width > 0 && height > 0) {
                facts.hitBoundary = {
                    height / 2.0,
                    desktop_app::v2::run::HitSide::PositiveY,
                    width, height};
            }
            facts.detectorSettings = detectorSettings;
            facts.cropSettings = cropSettings;
            facts.timingSettings = timingSettings;
            facts.cameraSettings = {
                {QStringLiteral("device_id"), cameraController.deviceId()},
                {QStringLiteral("image_width"), width},
                {QStringLiteral("image_height"), height},
                {QStringLiteral("bit_depth"), latestCameraBitDepth},
                {QStringLiteral("source"), QStringLiteral("production_controller")},
            };
            facts.daqSettings = daqService.settingsSnapshot();
            return facts;
        },
        daqReadiness,
        [&runsResultsController](const QString &) {
            runsResultsController.refresh();
        });
    QObject::connect(
        &cameraController, &desktop_app::v2::CameraController::frameReady,
        &liveSortingController,
        [&](const desktop_app::v2::CameraFrame &frame) {
            const bool dimensionsChanged =
                latestCameraWidth != frame.width ||
                latestCameraHeight != frame.height ||
                latestCameraBitDepth != frame.bitDepth;
            latestCameraWidth = frame.width;
            latestCameraHeight = frame.height;
            latestCameraBitDepth = frame.bitDepth;
            if (dimensionsChanged && frame.width > 0 && frame.height > 0)
                liveSortingController.refresh();
        });

    desktop_app::v2::sequence_test::SequenceTestController
        sequenceTestController(
            sequenceTestService,
            [&] (QString *error) {
                return activeModelSnapshot(registryFilePath, modelLoadService,
                                           error);
            },
            [&runsResultsController] { runsResultsController.refresh(); },
            [] { return defaultOpenDssRunsPath(); },
            [] { return availablePhysicalMemory(); }, daqReadiness,
            detectorSettings, cropSettings, timingSettings,
            QCoreApplication::applicationVersion());
    QObject::connect(
        &modelLibraryController, &desktop_app::v2::ModelLibraryController::changed,
        &liveSortingController,
        &desktop_app::v2::live::LiveSortingController::refresh);
    QObject::connect(
        &modelLibraryController, &desktop_app::v2::ModelLibraryController::changed,
        &sequenceTestController,
        &desktop_app::v2::sequence_test::SequenceTestController::refreshPreflight);
    QObject::connect(
        &daqController, &desktop_app::v2::DaqController::stateChanged,
        &liveSortingController,
        &desktop_app::v2::live::LiveSortingController::refresh);
    QObject::connect(
        &daqController, &desktop_app::v2::DaqController::stateChanged,
        &sequenceTestController,
        &desktop_app::v2::sequence_test::SequenceTestController::refreshPreflight);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("camera-preview"), cameraPreviewProvider);
    engine.addImageProvider(QStringLiteral("sequence-frame"),
                            new desktop_app::v2::sequence::SequenceViewerImageProvider(sequenceViewerController));
    engine.rootContext()->setContextProperty(QStringLiteral("cameraRuntimeController"),
                                             &cameraController);
    engine.rootContext()->setContextProperty(QStringLiteral("singleImageRuntimeController"),
                                             &singleImageCaptureController);
    const QUrl url(mainQmlFile);
    QObject::connect(
                &engine, &QQmlApplicationEngine::objectCreated, &app,
                [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(":/");
    engine.setInitialProperties({{QStringLiteral("settingsController"), QVariant::fromValue(&settingsController)},
                                 {QStringLiteral("daqController"), QVariant::fromValue(&daqController)},
                                 {QStringLiteral("runsResultsController"), QVariant::fromValue(&runsResultsController)},
                                 {QStringLiteral("sequenceViewerController"), QVariant::fromValue(&sequenceViewerController)},
                                 {QStringLiteral("datasetLabelController"), QVariant::fromValue(&datasetLabelController)},
                                  {QStringLiteral("trainingController"), QVariant::fromValue(&trainingController)},
                                  {QStringLiteral("modelLibraryController"), QVariant::fromValue(&modelLibraryController)},
                                  {QStringLiteral("modelTestController"), QVariant::fromValue(&modelTestController)},
                                  {QStringLiteral("liveSortingController"), QVariant::fromValue(&liveSortingController)},
                                  {QStringLiteral("sequenceTestController"), QVariant::fromValue(&sequenceTestController)}});
    engine.load(url);

    const int exitCode = engine.rootObjects().isEmpty() ? -1 : app.exec();

    auto waitForCameraCommand = [&cameraController]() {
        if (!cameraController.busy())
            return;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&cameraController, &desktop_app::v2::CameraController::busyChanged,
                         &loop, [&]() {
                             if (!cameraController.busy())
                                 loop.quit();
                         });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        loop.exec();
    };

    waitForCameraCommand();
    if (cameraController.close())
        waitForCameraCommand();
    cameraThread.quit();
    cameraThread.wait();
    return exitCode;
}
