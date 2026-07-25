// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include <memory>

#include "autogen/environment.h"
#include "../../desktop_app/model_registry_service.h"
#include "../../v2/camera/camera_controller.h"
#include "../../v2/camera/camera_preview_image_provider.h"
#include "../../v2/camera/camera_service.h"
#include "../../v2/camera/dcam_camera_device.h"
#include "../../v2/camera/single_image_capture_controller.h"
#include "../../v2/camera/single_image_capture_service.h"
#include "../../v2/dataset/dataset_label_controller.h"
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

} // namespace

int main(int argc, char *argv[])
{
    set_qt_environment();
    QApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0"));

    desktop_app::v2::ApplicationStateStore applicationStateStore;
    desktop_app::v2::OperationCoordinator operationCoordinator;
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
    desktop_app::v2::training::TrainingController trainingController(
        operationCoordinator, applicationStateStore, trainingPythonExecutable(), repositoryRoot());
    loadModelRegistry();
    const QString registryFilePath = modelRegistryPath();
    desktop_app::v2::ModelLibraryController modelLibraryController(
        registryFilePath, operationCoordinator);
    desktop_app::v2::ModelLoadService modelLoadService(registryFilePath);
    desktop_app::v2::model_test::ModelTestController modelTestController(
        operationCoordinator, modelLoadService, QCoreApplication::applicationVersion());
    QObject::connect(&modelLibraryController,
                     &desktop_app::v2::ModelLibraryController::changed,
                     &modelTestController,
                     &desktop_app::v2::model_test::ModelTestController::refreshPreflight);

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
                                 {QStringLiteral("runsResultsController"), QVariant::fromValue(&runsResultsController)},
                                 {QStringLiteral("sequenceViewerController"), QVariant::fromValue(&sequenceViewerController)},
                                 {QStringLiteral("datasetLabelController"), QVariant::fromValue(&datasetLabelController)},
                                 {QStringLiteral("trainingController"), QVariant::fromValue(&trainingController)},
                                 {QStringLiteral("modelLibraryController"), QVariant::fromValue(&modelLibraryController)},
                                 {QStringLiteral("modelTestController"), QVariant::fromValue(&modelTestController)}});
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
