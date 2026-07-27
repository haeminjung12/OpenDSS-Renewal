// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDesktopServices>
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
#include <QWindow>

#include <memory>
#include <chrono>
#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "autogen/environment.h"
#include "../../detection/droplet_detector_adapters.h"
#include "../../desktop_app/model_registry_service.h"
#include "../../desktop_app/pipeline_runner.h"
#include "../../onnx_classifier.h"
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
#include "../../v2/sequence/capture_workflow_controller.h"
#include "../../v2/sequence_test/sequence_test_controller.h"
#include "../../v2/sequence_test/sequence_test_image_provider.h"
#include "../../v2/sequence_test/sequence_test_service.h"
#include "../../v2/state/application_state_store.h"
#include "../../v2/training/training_controller.h"

namespace {

#ifdef Q_OS_WIN
class RestoredMinimumNativeEventFilter final : public QAbstractNativeEventFilter
{
public:
    explicit RestoredMinimumNativeEventFilter(HWND window)
        : window_(window)
    {
    }

    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override
    {
        auto *nativeMessage = static_cast<MSG *>(message);
        if (!nativeMessage || nativeMessage->hwnd != window_)
            return false;

        const UINT dpi = qMax(GetDpiForWindow(window_),
                              UINT(USER_DEFAULT_SCREEN_DPI));
        const LONG minimumWidth =
            MulDiv(1600, dpi, USER_DEFAULT_SCREEN_DPI);
        const LONG minimumHeight =
            MulDiv(900, dpi, USER_DEFAULT_SCREEN_DPI);
        if (nativeMessage->message == WM_GETMINMAXINFO) {
            auto *limits = reinterpret_cast<MINMAXINFO *>(nativeMessage->lParam);
            limits->ptMinTrackSize.x =
                qMax(limits->ptMinTrackSize.x, minimumWidth);
            limits->ptMinTrackSize.y =
                qMax(limits->ptMinTrackSize.y, minimumHeight);
        } else if (nativeMessage->message == WM_WINDOWPOSCHANGING) {
            auto *position = reinterpret_cast<WINDOWPOS *>(nativeMessage->lParam);
            WINDOWPLACEMENT placement{};
            placement.length = sizeof(placement);
            const bool minimizing =
                IsIconic(window_) ||
                (GetWindowPlacement(window_, &placement) &&
                 (placement.showCmd == SW_SHOWMINIMIZED ||
                  placement.showCmd == SW_SHOWMINNOACTIVE ||
                  placement.showCmd == SW_FORCEMINIMIZE));
            if (!(position->flags & SWP_NOSIZE) && !minimizing) {
                position->cx = qMax(position->cx, minimumWidth);
                position->cy = qMax(position->cy, minimumHeight);
            }
        }
        return false;
    }

private:
    HWND window_ = nullptr;
};
#endif

QString trainingWorkingDirectory()
{
    return QCoreApplication::applicationDirPath();
}

QString trainingPythonExecutable()
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (localAppData.isEmpty())
        return {};

    const QString installerOwned =
        QDir(localAppData).filePath(QStringLiteral("OpenDSS/training-venv-gpu/Scripts/python.exe"));
    return QFileInfo(installerOwned).isFile() ? installerOwned : QString();
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
    snapshot.sha256 = inspection.modelSha256;
    for (const auto &modelClass : inspection.classes)
        snapshot.classes.push_back({modelClass.id, modelClass.displayLabel});
    if ((snapshot.classes.size() != 2 && snapshot.classes.size() != 3) ||
        snapshot.sha256.size() != 64) {
        if (error)
            *error = QStringLiteral("The Active Model provenance is incomplete.");
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
    if (daqController.canApply())
        daqController.apply();
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
    desktop_app::v2::SettingsController settingsController(
        settingsRepository, applicationStateStore,
        [](const QUrl &url) { return QDesktopServices::openUrl(url); });
    desktop_app::v2::results::RunRepository runRepository(applicationStateStore);
    desktop_app::v2::results::RunsResultsController runsResultsController(
        runRepository, applicationStateStore,
        [](const QUrl &url) { return QDesktopServices::openUrl(url); });
    desktop_app::v2::sequence::SequenceViewerController sequenceViewerController;
    desktop_app::v2::dataset::DatasetLabelController datasetLabelController(operationCoordinator,
                                                                            applicationStateStore);
    const QJsonObject registry = loadModelRegistry();
    const DefaultWorkspacePaths workspacePaths = ensureDefaultWorkspaceAssets(
        registry.value(QStringLiteral("entries")).toArray());
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
        modelLibraryController, trainingPythonExecutable(), trainingWorkingDirectory());
    desktop_app::v2::model_test::ModelTestController modelTestController(
        operationCoordinator, modelLoadService, QCoreApplication::applicationVersion(),
        trainingPythonExecutable(), trainingWorkingDirectory());
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

    int latestCameraWidth = 0;
    int latestCameraHeight = 0;
    int latestCameraBitDepth = 0;
    desktop_app::v2::sequence::CaptureWorkflowController captureWorkflowController(
        *cameraService, cameraController, operationCoordinator, fastDetector,
        [] {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        },
        [&] {
            const int acceptedWidth = latestCameraWidth > 0
                ? latestCameraWidth : cameraController.customWidth().toInt();
            const int acceptedHeight = latestCameraHeight > 0
                ? latestCameraHeight : cameraController.customHeight().toInt();
            const int acceptedBitDepth = latestCameraBitDepth > 0
                ? latestCameraBitDepth
                : cameraController.bitDepth().section(QLatin1Char('-'), 0, 0).toInt();
            return QJsonObject{
                {QStringLiteral("device_id"), cameraController.deviceId()},
                {QStringLiteral("image_width"), acceptedWidth},
                {QStringLiteral("image_height"), acceptedHeight},
                {QStringLiteral("bit_depth"), acceptedBitDepth},
                {QStringLiteral("source"), QStringLiteral("production_controller")},
            };
        },
        QCoreApplication::applicationVersion());
    captureWorkflowController.setSequenceLocation(workspacePaths.collections);
    captureWorkflowController.setDatasetLocation(workspacePaths.datasets);

    desktop_app::v2::live::LiveSortingService liveSortingService(
        operationCoordinator, fastDetector, &modelLoadService, hitPulse, {}, {},
        {}, daqReadiness);
    desktop_app::v2::sequence_test::SequenceTestService sequenceTestService(
        operationCoordinator, fastDetector, &modelLoadService, {}, hitPulse,
        daqReadiness);

    desktop_app::v2::live::LiveSortingController liveSortingController(
        liveSortingService, cameraController,
        [&] {
            desktop_app::v2::live::LiveControllerFacts facts;
            facts.defaultRunRoot = defaultOpenDssRunsPath();
            facts.opendssVersion = QCoreApplication::applicationVersion();
            facts.minimumContourArea = fastDetector.minimumContourArea();
            facts.applyMinimumContourArea =
                [&fastDetector](int area, QString *) {
                    fastDetector.setMinimumContourArea(area);
                    return true;
                };
            QString modelError;
            if (const auto model =
                    activeModelSnapshot(registryFilePath, modelLoadService,
                                        &modelError)) {
                facts.activeModelName = model->name;
                facts.activeModelClasses = model->classes;
                facts.activeModelLoadable = true;
            }
            facts.activeModelId = modelLibraryController.activeId();
            facts.applyCameraProfile =
                [&cameraController](const QJsonObject &camera, QString *error) {
                    desktop_app::v2::CameraAppliedSettings settings;
                    settings.width =
                        camera.value(QStringLiteral("image_width")).toInt();
                    settings.height =
                        camera.value(QStringLiteral("image_height")).toInt();
                    settings.bitDepth =
                        camera.value(QStringLiteral("bit_depth")).toInt();
                    settings.pixelType = settings.bitDepth > 8
                        ? desktop_app::v2::CameraPixelType::Mono16
                        : desktop_app::v2::CameraPixelType::Mono8;
                    settings.exposureMs =
                        camera.value(QStringLiteral("exposure_ms")).toDouble();
                    settings.readoutMode =
                        camera.value(QStringLiteral("readout_mode")).toString()
                                .compare(QStringLiteral("slow"),
                                         Qt::CaseInsensitive) == 0
                        ? desktop_app::v2::CameraReadoutMode::Slow
                        : desktop_app::v2::CameraReadoutMode::Fast;
                    const int lutMinimum =
                        camera.value(QStringLiteral("preview_lut_min"))
                            .toInt(cameraController.previewLutMinimum());
                    const int lutMaximum =
                        camera.value(QStringLiteral("preview_lut_max"))
                            .toInt(cameraController.previewLutMaximum());
                    const bool applied = cameraController.applyProfileSettings(
                        settings, lutMinimum, lutMaximum);
                    if (!applied && error)
                        *error = cameraController.error();
                    return applied;
                };
            facts.applyDaqProfile =
                [&daqService](const QJsonObject &daq, QString *error) {
                    desktop_app::v2::DaqAppliedSettings settings;
                    settings.outputChannel =
                        daq.value(QStringLiteral("channel")).toString();
                    settings.frequencyHz =
                        daq.value(QStringLiteral("frequency_hz")).toDouble();
                    settings.durationMs =
                        daq.value(QStringLiteral("duration_ms")).toDouble();
                    settings.delayMs =
                        daq.value(QStringLiteral("delay_ms")).toDouble();
                    settings.amplitudeVpp =
                        daq.value(QStringLiteral("amplitude_vpp")).toDouble();
                    return daqService.applySettings(settings, error);
                };
            facts.activateModel =
                [&modelLibraryController, &registryFilePath](
                    const QString &id, QString *error) {
                    const bool activated =
                        activateModelRegistryEntry(registryFilePath, id, error);
                    if (activated)
                        modelLibraryController.refresh();
                    return activated;
                };
            const int width = latestCameraWidth > 0
                ? latestCameraWidth : cameraController.customWidth().toInt();
            const int height = latestCameraHeight > 0
                ? latestCameraHeight : cameraController.customHeight().toInt();
            const int bitDepth = latestCameraBitDepth > 0
                ? latestCameraBitDepth
                : cameraController.bitDepth().section(QLatin1Char('-'), 0, 0).toInt();
            if (width > 0 && height > 0) {
                facts.hitBoundary = {
                    -1.0,
                    desktop_app::v2::run::HitSide::NegativeY,
                    width, height};
            }
            facts.detectorSettings = detectorSettings;
            facts.cropSettings = cropSettings;
            facts.timingSettings = timingSettings;
            facts.cameraSettings = {
                {QStringLiteral("device_id"), cameraController.deviceId()},
                {QStringLiteral("image_width"), width},
                {QStringLiteral("image_height"), height},
                {QStringLiteral("bit_depth"), bitDepth},
                {QStringLiteral("exposure_ms"),
                 cameraController.exposureMs().toDouble()},
                {QStringLiteral("readout_mode"),
                 cameraController.readoutMode()},
                {QStringLiteral("preview_lut_min"),
                 cameraController.previewLutMinimum()},
                {QStringLiteral("preview_lut_max"),
                 cameraController.previewLutMaximum()},
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
        &sequenceTestController,
        &desktop_app::v2::sequence_test::SequenceTestController::
            openRunFolderRequested,
        &app, [](const QUrl &folder) { QDesktopServices::openUrl(folder); });
    QObject::connect(
        &modelLibraryController, &desktop_app::v2::ModelLibraryController::changed,
        &liveSortingController,
        &desktop_app::v2::live::LiveSortingController::refresh);
    QObject::connect(
        &modelLibraryController, &desktop_app::v2::ModelLibraryController::changed,
        &sequenceTestController,
        &desktop_app::v2::sequence_test::SequenceTestController::refreshPreflight);

    const QString pythonExecutable = trainingPythonExecutable();
    const QString automaticDevice =
        QString::fromStdString(OnnxClassifier::plannedAutomaticDevice());
    settingsController.setDiagnostics({
        pythonExecutable.isEmpty()
            ? QStringLiteral("Unavailable — Python runtime not found")
            : QStringLiteral("Available — Python runtime found"),
        automaticDevice == QStringLiteral("GPU")
            ? QStringLiteral("Available — CUDA provider found")
            : QStringLiteral("Unavailable — CUDA provider not found; automatic inference uses CPU"),
        QCoreApplication::applicationDirPath(),
    });
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
    engine.addImageProvider(
        QStringLiteral("sequence-test-preview"),
        new desktop_app::v2::sequence_test::SequenceTestImageProvider(
            sequenceTestController));
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
                                 {QStringLiteral("captureWorkflowController"), QVariant::fromValue(&captureWorkflowController)},
                                 {QStringLiteral("datasetLabelController"), QVariant::fromValue(&datasetLabelController)},
                                  {QStringLiteral("trainingController"), QVariant::fromValue(&trainingController)},
                                  {QStringLiteral("modelLibraryController"), QVariant::fromValue(&modelLibraryController)},
                                  {QStringLiteral("modelTestController"), QVariant::fromValue(&modelTestController)},
                                  {QStringLiteral("liveSortingController"), QVariant::fromValue(&liveSortingController)},
                                  {QStringLiteral("sequenceTestController"), QVariant::fromValue(&sequenceTestController)}});
    engine.load(url);

#ifdef Q_OS_WIN
    std::unique_ptr<RestoredMinimumNativeEventFilter> restoredMinimumFilter;
#endif
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
            window->setMinimumSize(QSize(1600, 900));
#ifdef Q_OS_WIN
            restoredMinimumFilter =
                std::make_unique<RestoredMinimumNativeEventFilter>(
                    reinterpret_cast<HWND>(window->winId()));
            app.installNativeEventFilter(restoredMinimumFilter.get());
#endif
            QTimer::singleShot(0, window, &QWindow::showMaximized);
        }
    }

    const int exitCode = engine.rootObjects().isEmpty() ? -1 : app.exec();
#ifdef Q_OS_WIN
    if (restoredMinimumFilter)
        app.removeNativeEventFilter(restoredMinimumFilter.get());
#endif

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
