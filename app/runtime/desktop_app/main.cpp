#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QtWidgets>
#include <QtCore>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "app_context.h"
#include "app_options.h"
#include "app_paths.h"
#include "app_state.h"
#include "crash_handler.h"
#include "main_window.h"
#include "json_persistence.h"
#include "model_registry_service.h"
#include "validator_workspace_controller.h"
#include "workspace_dataset.h"
#include "workspace_model.h"
#include "../cli_runner.h"
#include "../metadata_loader.h"
#include "../onnx_classifier.h"

namespace {

int runSequenceStopThreadingVerifier(int argc, char* argv[]) {
    QApplication app(argc, argv);
    std::atomic<bool> running(true);
    std::atomic<bool> stop(false);
    std::thread worker;
    QPushButton stopButton;
    QLabel sequenceStatus;
    QLabel appStatus;

    ValidatorWorkspaceController::Dependencies dependencies;
    dependencies.seqStopBtn = &stopButton;
    dependencies.seqStatusLabel = &sequenceStatus;
    dependencies.statusLabel = &appStatus;
    dependencies.sequenceRunning = &running;
    dependencies.sequenceStop = &stop;
    dependencies.sequenceThread = &worker;
    ValidatorWorkspaceController controller(dependencies);

    worker = std::thread([&]() {
        while (!stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        running.store(false);
    });

    QElapsedTimer timer;
    timer.start();
    controller.stopSequenceTest();
    const qint64 elapsedMs = timer.elapsed();
    const bool firstStopPassed = stop.load() && elapsedMs < 100 && worker.joinable() && !stopButton.isEnabled() &&
                                 sequenceStatus.text() == QStringLiteral("Stopping sequence...");

    controller.stopSequenceTest();
    const bool repeatedStopPassed = stop.load() && worker.joinable();
    controller.waitForSequenceTest();
    const bool shutdownPassed = !worker.joinable() && !running.load();

    std::fprintf(stderr, "Sequence stop threading verifier: stop=%s repeated=%s shutdown=%s elapsed=%lldms\n",
                 firstStopPassed ? "PASS" : "FAIL", repeatedStopPassed ? "PASS" : "FAIL",
                 shutdownPassed ? "PASS" : "FAIL", static_cast<long long>(elapsedMs));
    return firstStopPassed && repeatedStopPassed && shutdownPassed ? 0 : 2;
}

int runOnnxProviderVerifier(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    OnnxClassifier::configureReadinessVerifier(
        qEnvironmentVariable("OVDS_ORT_VERIFY_READINESS").toStdString(),
        qEnvironmentVariableIntValue("OVDS_ORT_VERIFY_FORCE_ACCEPTED") != 0);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString device = qEnvironmentVariable("OVDS_ORT_VERIFY_DEVICE", "cuda").trimmed().toLower();
    const QString packageOverride = qEnvironmentVariable("OVDS_ORT_VERIFY_PACKAGE").trimmed();
    const QStringList architectures = packageOverride.isEmpty()
                                          ? QStringList{QStringLiteral("mobilenet_v3_small"), QStringLiteral("efficientnet_b0")}
                                          : QStringList{QStringLiteral("package")};
    QJsonArray results;
    bool passed = true;
    for (const QString& architecture : architectures) {
        const QString root = packageOverride.isEmpty()
                                 ? QDir(appDir).filePath(QStringLiteral("models/templates/pretrained/%1").arg(architecture))
                                 : QFileInfo(packageOverride).absoluteFilePath();
        Metadata metadata;
        std::string error;
        const bool metadataOk = LoadMetadata(QDir(root).filePath("metadata.json").toStdString(), metadata, error);
        OnnxClassifier classifier;
        const bool initialized = metadataOk && classifier.init(QDir(root).filePath("model.onnx").toStdString(), metadata,
                                                                device.toStdString(), error);
        const QString provider = initialized ? QString::fromStdString(classifier.executionProvider()) : QString();
        const bool cudaAccepted = qEnvironmentVariableIntValue("OVDS_ORT_VERIFY_FORCE_ACCEPTED") != 0 ||
                                  qEnvironmentVariableIntValue("OVDS_ORT_VERIFY_EXPECT_ACCEPTED") != 0;
        const bool unavailable = qEnvironmentVariableIsSet("OVDS_TEST_FORCE_CUDA_UNAVAILABLE") || !cudaAccepted;
        const bool explicitUnavailable = unavailable && device == "cuda";
        const bool autoUnavailable = unavailable && device == "auto";
        bool rowPassed = initialized && ((device == "cpu" && provider == "CPU") ||
                         (device == "cuda" && provider == "CUDA") ||
                         (device == "auto" && provider == (autoUnavailable ? "CPU" : "CUDA")));
        if (explicitUnavailable)
            rowPassed = !initialized &&
                        (QString::fromStdString(error).contains("CUDA provider unavailable") ||
                         QString::fromStdString(error).contains("readiness artifact"));
        if (initialized) {
            cv::Mat input(metadata.inputH, metadata.inputW, CV_8UC3, cv::Scalar(0, 0, 0));
            const ClassificationResult prediction = classifier.classify(input);
            rowPassed = rowPassed && prediction.scores.size() == static_cast<std::size_t>(metadata.classes.size()) &&
                        std::all_of(prediction.scores.begin(), prediction.scores.end(), [](float value) { return std::isfinite(value); });
        }
        passed = passed && rowPassed;
        QJsonObject row{{"architecture", architecture}, {"requested_device", device}, {"initialized", initialized},
                        {"selected_provider", provider}, {"message", QString::fromStdString(error)}, {"passed", rowPassed}};
        results.append(row);
    }
    QJsonObject output{{"passed", passed}, {"results", results}};
    const QString outputPath = qEnvironmentVariable("OVDS_ORT_VERIFY_OUTPUT").trimmed();
    if (!outputPath.isEmpty()) {
        QFile file(outputPath);
        QDir().mkpath(QFileInfo(file).absolutePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            file.write(QJsonDocument(output).toJson(QJsonDocument::Indented)) < 0)
            return 2;
    }
    std::printf("%s\n", QJsonDocument(output).toJson(QJsonDocument::Compact).constData());
    return passed ? 0 : 2;
}

constexpr const char* kOrganizationName = "Hamamatsu";
constexpr const char* kApplicationName = "OpenDSS";
constexpr const char* kApplicationVersion = "0.9.0";
constexpr const char* kLegacyApplicationName = "OpenVisualDropletSorter";
constexpr const char* kLegacySettingsMigrationMarker = "migration/v1/importedOpenVisualDropletSorter";

bool hasArgument(int argc, char* argv[], const QString& expected) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == expected)
            return true;
    }
    return false;
}

void preferBundledQtPlugins(int argc, char* argv[]) {
    QString executablePath;
#ifdef _WIN32
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (modulePathLength > 0 && modulePathLength < MAX_PATH)
        executablePath = QString::fromWCharArray(modulePath, static_cast<int>(modulePathLength));
#endif
    if (argc > 0 && argv && argv[0])
        executablePath = executablePath.isEmpty() ? QString::fromLocal8Bit(argv[0]) : executablePath;
    const QFileInfo executableInfo(executablePath);
    const QString appDirPath = executableInfo.exists() ? executableInfo.absolutePath() : QDir::currentPath();
    const QDir appDir(appDirPath);
    const QString platformsDir = appDir.filePath(QStringLiteral("platforms"));
    if (!QFileInfo(appDir.filePath(QStringLiteral("platforms/qwindows.dll"))).isFile())
        return;

    QStringList libraryPaths;
    libraryPaths << appDir.absolutePath();
    const QString pluginRoot = appDir.filePath(QStringLiteral("plugins"));
    if (QFileInfo(pluginRoot).isDir())
        libraryPaths << QFileInfo(pluginRoot).absoluteFilePath();
    QCoreApplication::setLibraryPaths(libraryPaths);
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFileInfo(platformsDir).absoluteFilePath().toLocal8Bit());
}

void setOpenDssApplicationIdentity() {
    QCoreApplication::setOrganizationName(kOrganizationName);
    QCoreApplication::setApplicationName(kApplicationName);
    QCoreApplication::setApplicationVersion(kApplicationVersion);
}

void configureSettingsRootFromEnv() {
    const QString settingsRoot = qEnvironmentVariable("OVDS_SETTINGS_ROOT_PATH").trimmed();
    if (settingsRoot.isEmpty())
        return;
    const QString absoluteSettingsRoot = QFileInfo(settingsRoot).absoluteFilePath();
    QDir().mkpath(absoluteSettingsRoot);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, absoluteSettingsRoot);
}

bool isVerifierProcess(int argc, char* argv[]) {
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString& key : environment.keys()) {
        if (!key.startsWith(QStringLiteral("OVDS_VERIFY_"), Qt::CaseInsensitive))
            continue;
        const QString value = environment.value(key).trimmed();
        if (!value.isEmpty() && value != QStringLiteral("0") &&
            value.compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0)
            return true;
    }
    for (int index = 1; index < argc; ++index) {
        if (QString::fromLocal8Bit(argv[index]).startsWith(QStringLiteral("--verify-")))
            return true;
    }
    return false;
}

bool copyDirectoryRecursively(const QString& sourcePath, const QString& destinationPath) {
    const QDir source(sourcePath);
    if (!source.exists() || !QDir().mkpath(destinationPath))
        return false;
    QDirIterator iterator(sourcePath, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QFileInfo sourceInfo(iterator.next());
        const QString relativePath = source.relativeFilePath(sourceInfo.absoluteFilePath());
        const QString destination = QDir(destinationPath).absoluteFilePath(relativePath);
        if (sourceInfo.isDir()) {
            if (!QDir().mkpath(destination))
                return false;
        } else {
            QDir().mkpath(QFileInfo(destination).absolutePath());
            QFile::remove(destination);
            if (!QFile::copy(sourceInfo.absoluteFilePath(), destination))
                return false;
        }
    }
    return true;
}

std::unique_ptr<QTemporaryDir> isolateVerifierState(int argc, char* argv[]) {
    if (!isVerifierProcess(argc, argv))
        return {};
    auto sandbox = std::make_unique<QTemporaryDir>(
        QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("opendss-verifier-state-XXXXXX")));
    if (!sandbox->isValid())
        return {};

    if (qEnvironmentVariable("OVDS_SETTINGS_ROOT_PATH").trimmed().isEmpty()) {
        const QString settingsRoot = QDir(sandbox->path()).absoluteFilePath(QStringLiteral("settings"));
        QDir().mkpath(settingsRoot);
        qputenv("OVDS_SETTINGS_ROOT_PATH", settingsRoot.toUtf8());
    }
    if (qEnvironmentVariable("OVDS_MODEL_REGISTRY_PATH").trimmed().isEmpty()) {
        const QString productionModels = QDir::home().absoluteFilePath(QStringLiteral("Documents/OpenDSS/models"));
        const QString isolatedModels = QDir(sandbox->path()).absoluteFilePath(QStringLiteral("models"));
        copyDirectoryRecursively(productionModels, isolatedModels);
        qputenv("OVDS_MODELS_ROOT_PATH", isolatedModels.toUtf8());
        qputenv("OVDS_MODEL_REGISTRY_PATH",
                QDir(isolatedModels).absoluteFilePath(QStringLiteral("model_registry.json")).toUtf8());
    }
    return sandbox;
}

bool migrateLegacyOpenVisualDropletSorterSettings(QString* errorMessage = nullptr) {
    QSettings currentSettings;
    QSettings legacySettings(QString::fromLatin1(kOrganizationName), QString::fromLatin1(kLegacyApplicationName));
    const QStringList legacyKeys = legacySettings.allKeys();

    for (const QString& key : legacyKeys) {
        if (!currentSettings.contains(key))
            currentSettings.setValue(key, legacySettings.value(key));
    }

    if (!legacyKeys.isEmpty()) {
        currentSettings.setValue(QString::fromLatin1(kLegacySettingsMigrationMarker),
                                 QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }
    currentSettings.sync();

    if (currentSettings.status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to write migrated OpenDSS settings.");
        return false;
    }
    return true;
}

int runSettingsMigrationVerifier(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::fprintf(stderr, "SETTINGS MIGRATION VERIFY FAIL: temporary settings directory unavailable\n");
        return 2;
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
    setOpenDssApplicationIdentity();

    QSettings currentSettings;
    currentSettings.clear();
    currentSettings.setValue(QStringLiteral("settings/pythonTrainer"), QStringLiteral("C:/new/python.exe"));
    currentSettings.setValue(QStringLiteral("settings/computeDevice"), QStringLiteral("cuda"));
    currentSettings.sync();

    QSettings legacySettings(QString::fromLatin1(kOrganizationName), QString::fromLatin1(kLegacyApplicationName));
    legacySettings.clear();
    legacySettings.setValue(QStringLiteral("settings/pythonTrainer"), QStringLiteral("C:/legacy/python.exe"));
    legacySettings.setValue(QStringLiteral("validator/device"), QStringLiteral("cpu"));
    legacySettings.sync();

    QString errorMessage;
    const bool migrated = migrateLegacyOpenVisualDropletSorterSettings(&errorMessage);
    QSettings reloadedSettings;

    QStringList failures;
    auto require = [&](bool condition, const QString& message) {
        if (!condition)
            failures << message;
    };

    require(migrated, errorMessage.isEmpty() ? QStringLiteral("migration completed") : errorMessage);
    require(QCoreApplication::applicationName() == QString::fromLatin1(kApplicationName),
            QStringLiteral("active application name is OpenDSS"));
    require(reloadedSettings.value(QStringLiteral("settings/pythonTrainer")).toString() == QStringLiteral("C:/new/python.exe"),
            QStringLiteral("new settings are not overwritten by legacy values"));
    require(reloadedSettings.value(QStringLiteral("validator/device")).toString() == QStringLiteral("cpu"),
            QStringLiteral("missing new settings are copied from legacy namespace"));
    require(reloadedSettings.contains(QString::fromLatin1(kLegacySettingsMigrationMarker)),
            QStringLiteral("migration marker is written"));

    if (!failures.isEmpty()) {
        std::fprintf(stderr, "SETTINGS MIGRATION VERIFY FAIL: %s\n", failures.join("; ").toLocal8Bit().constData());
        return 2;
    }
    std::printf("Settings migration verifier passed.\n");
    return 0;
}

int runDatasetWorkspaceMetadataOnlyVerifier(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    setOpenDssApplicationIdentity();

    QStringList failures;
    auto require = [&failures](bool condition, const QString& message) {
        if (!condition)
            failures << message;
    };

    auto datasetRootForManifest = [](const QString& manifestPath) {
        const QFileInfo info(manifestPath);
        QString root = info.absolutePath();
        if (info.fileName() == QStringLiteral("dataset_manifest.json") &&
            info.dir().dirName() == QStringLiteral("metadata")) {
            QDir dir(info.dir());
            if (dir.cdUp())
                root = dir.absolutePath();
        }
        return root;
    };

    auto isSupportedMetadataManifest = [](const QJsonObject& root) {
        if (!root.value(QStringLiteral("items")).isArray())
            return false;
        const QString schema = root.value(QStringLiteral("schema_version")).toString().trimmed();
        if (schema == QStringLiteral("dataset-builder-manifest-v1") || schema == QStringLiteral("dataset-manifest-v1"))
            return true;
        if (root.value(QStringLiteral("class_schema")).isObject() || root.value(QStringLiteral("dataset_id")).isString() ||
            root.value(QStringLiteral("source_folder")).isString())
            return true;
        const QJsonArray items = root.value(QStringLiteral("items")).toArray();
        if (items.isEmpty())
            return false;
        const QJsonObject first = items.first().toObject();
        return first.contains(QStringLiteral("crop_path")) || first.contains(QStringLiteral("image_path")) ||
               first.contains(QStringLiteral("relative_path")) || first.contains(QStringLiteral("path")) ||
               first.contains(QStringLiteral("reviewed_label")) || first.contains(QStringLiteral("auto_label")) ||
               first.contains(QStringLiteral("label")) || first.contains(QStringLiteral("class_id"));
    };

    auto readMetadataManifest = [&isSupportedMetadataManifest](const QString& path, QJsonDocument* doc, QString* errorMessage) {
        const QString trimmedPath = path.trimmed();
        if (trimmedPath.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Choose a dataset metadata JSON file.");
            return false;
        }

        const QFileInfo info(trimmedPath);
        if (!info.exists()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Dataset file not found. Choose a compatible metadata JSON file.");
            return false;
        }
        if (!info.isFile()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Choose a dataset metadata JSON file, not a folder.");
            return false;
        }

        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Dataset file could not be read. Choose a compatible metadata JSON file.");
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Dataset file could not be read. Choose a compatible metadata JSON file.");
            return false;
        }
        if (!isSupportedMetadataManifest(parsed.object())) {
            if (errorMessage)
                *errorMessage = QStringLiteral("This JSON file is not a supported OpenDSS dataset manifest.");
            return false;
        }
        if (doc)
            *doc = parsed;
        return true;
    };

    const QString manifest = qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed();
    const QString outputPath = qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_OUT").trimmed();
    QJsonDocument manifestDoc;
    QString manifestError;
    const bool manifestAccepted = readMetadataManifest(manifest, &manifestDoc, &manifestError);
    require(manifestAccepted, manifestError.isEmpty() ? QStringLiteral("metadata JSON manifest was not accepted")
                                                      : manifestError);

    QString activeManifestPath;
    QString activeDatasetRoot;
    int loadedItems = -1;
    if (manifestAccepted) {
        activeManifestPath = QFileInfo(manifest).absoluteFilePath();
        activeDatasetRoot = datasetRootForManifest(activeManifestPath);
        loadedItems = manifestDoc.object().value(QStringLiteral("items")).toArray().size();
        require(QFileInfo(activeManifestPath).isFile(), QStringLiteral("accepted dataset path is not a file"));
    }

    const QString activeManifestBeforeFolderAttempt = activeManifestPath;
    const QString activeRootBeforeFolderAttempt = activeDatasetRoot;
    const QString folderPath = manifestAccepted ? QFileInfo(activeManifestPath).absolutePath()
                                                : QFileInfo(manifest).absolutePath();
    QJsonDocument folderDoc;
    QString folderError;
    const bool folderAccepted = readMetadataManifest(folderPath, &folderDoc, &folderError);
    require(!folderAccepted, QStringLiteral("folder path was accepted as a dataset metadata manifest"));

    const bool folderLoadRejected =
        !folderAccepted && activeManifestPath == activeManifestBeforeFolderAttempt &&
        activeDatasetRoot == activeRootBeforeFolderAttempt;
    require(folderLoadRejected,
            QStringLiteral("folder load changed active dataset state after metadata JSON load"));
    require(folderError.contains(QStringLiteral("not a folder"), Qt::CaseInsensitive),
            QStringLiteral("folder rejection did not explain that a metadata JSON file is required"));

    QJsonObject result;
    result[QStringLiteral("ok")] = failures.isEmpty();
    result[QStringLiteral("failures")] = QJsonArray::fromStringList(failures);
    result[QStringLiteral("mode")] = QStringLiteral("metadata_only");
    result[QStringLiteral("manifest_path")] = activeManifestPath;
    result[QStringLiteral("dataset_root")] = activeDatasetRoot;
    result[QStringLiteral("loaded_items")] = loadedItems;
    result[QStringLiteral("folder_path")] = folderPath;
    result[QStringLiteral("folder_load_rejected")] = folderLoadRejected;
    result[QStringLiteral("folder_rejection_message")] = folderError;

    if (!outputPath.isEmpty()) {
        QString writeError;
        desktop_app::writeJsonObjectAtomically(outputPath, result, &writeError);
    }

    if (!failures.isEmpty()) {
        std::fprintf(stderr, "Dataset metadata-only verifier failed: %s\n",
                     failures.join("; ").toLocal8Bit().constData());
        return 2;
    }
    std::printf("Dataset metadata-only verifier passed.\n");
    return 0;
}

int runDatasetWorkspaceWidgetVerifier(int argc, char* argv[]) {
    QApplication app(argc, argv);
    setOpenDssApplicationIdentity();

    desktop_app::workspace::DatasetWorkspaceControls datasetWorkspaceControls;
    std::unique_ptr<QWidget> datasetWorkspace(
        desktop_app::workspace::buildDatasetWorkspace(datasetWorkspaceControls));
    app.processEvents();
    const QVariant exitCode = qApp->property("ovdsDatasetWorkspaceVerifyExitCode");
    if (exitCode.isValid())
        return exitCode.toInt();
    std::fprintf(stderr, "Dataset workspace widget verifier did not produce an exit code.\n");
    return 2;
}

int runModelWorkspaceWidgetVerifier(int argc, char* argv[]) {
    const bool mutationVerifier =
        qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_WORKSPACE_ADD_BUTTONS") != 0 ||
         qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_WORKSPACE_LIST_MANAGEMENT") != 0 ||
         qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_ACTIVE_SIMPLIFICATION") != 0;
    const QString registryOverride = qEnvironmentVariable("OVDS_MODEL_REGISTRY_PATH").trimmed();
    if (mutationVerifier && registryOverride.isEmpty()) {
        std::fprintf(stderr,
                     "Model workspace mutation verifier requires OVDS_MODEL_REGISTRY_PATH; refusing to modify the production registry.\n");
        return 2;
    }
    if (mutationVerifier && qEnvironmentVariable("OVDS_MODELS_ROOT_PATH").trimmed().isEmpty()) {
        const QString isolatedModelsRoot = QFileInfo(registryOverride).absolutePath();
        qputenv("OVDS_MODELS_ROOT_PATH", isolatedModelsRoot.toUtf8());
    }
    QApplication app(argc, argv);
    setOpenDssApplicationIdentity();

    QString registryFilePath;
    QString registryLoadWarning;
    QJsonObject modelRegistry = loadModelRegistry(&registryFilePath, &registryLoadWarning);
    QJsonArray registryEntries = modelRegistry.value("entries").toArray();
    if (registryEntries.isEmpty()) {
        modelRegistry = temporaryStaticModelRegistry();
        registryEntries = modelRegistry.value("entries").toArray();
        registryLoadWarning = "Model registry had no rows; using temporary static fallback.";
    }

    desktop_app::AppState appState;
    desktop_app::workspace::ModelWorkspaceControls modelWorkspaceControls;
    modelWorkspaceControls.registryEntries = registryEntries;
    modelWorkspaceControls.registryFilePath = registryFilePath;
    modelWorkspaceControls.registryLoadWarning = registryLoadWarning;
    modelWorkspaceControls.appState = &appState;

    std::unique_ptr<QWidget> modelWorkspace(
        desktop_app::workspace::buildModelWorkspace(modelWorkspaceControls));
    return app.exec();
}

} // namespace

int main(int argc, char* argv[]) {
    preferBundledQtPlugins(argc, argv);
    const std::unique_ptr<QTemporaryDir> verifierState = isolateVerifierState(argc, argv);
    configureSettingsRootFromEnv();
    const QString verifierTracePath = qEnvironmentVariable("OVDS_VERIFY_TRACE_PATH").trimmed();
    const auto verifierTrace = [verifierTracePath](const QString& message) {
        if (verifierTracePath.isEmpty())
            return;
        QFile trace(verifierTracePath);
        if (trace.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            trace.write((message + QLatin1Char('\n')).toUtf8());
            trace.flush();
        }
    };
    verifierTrace(QStringLiteral("main: entered"));
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_SEQUENCE_STOP_THREADING") != 0 ||
        hasArgument(argc, argv, QStringLiteral("--verify-sequence-stop-threading"))) {
        return runSequenceStopThreadingVerifier(argc, argv);
    }
    if (hasArgument(argc, argv, QStringLiteral("--verify-onnx-provider")))
        return runOnnxProviderVerifier(argc, argv);
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_SETTINGS_MIGRATION") != 0 ||
        hasArgument(argc, argv, QStringLiteral("--verify-settings-migration"))) {
        return runSettingsMigrationVerifier(argc, argv);
    }
    if (!qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed().isEmpty() &&
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_METADATA_ONLY") == "1" &&
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_IMAGE_IMPORT") == "1") {
        return runDatasetWorkspaceWidgetVerifier(argc, argv);
    }
    if (!qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed().isEmpty() &&
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_METADATA_ONLY") == "1" &&
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_IMAGE_IMPORT") != "1") {
        return runDatasetWorkspaceMetadataOnlyVerifier(argc, argv);
    }
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_WORKSPACE_ADD_BUTTONS") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_WORKSPACE_LIST_MANAGEMENT") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_ACTIVE_SIMPLIFICATION") != 0) {
        return runModelWorkspaceWidgetVerifier(argc, argv);
    }

    bool verifyTrainerSetupStatus = qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_SETUP_STATUS") != 0;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--verify-trainer-setup-status")) {
            verifyTrainerSetupStatus = true;
            break;
        }
    }
    if (verifyTrainerSetupStatus) {
        setOpenDssApplicationIdentity();
        migrateLegacyOpenVisualDropletSorterSettings();
        return runTrainerSetupStatusVerifierAppOwned();
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cli") {
#ifdef _WIN32
            if (GetConsoleWindow() == nullptr) {
                AllocConsole();
                FILE* out = nullptr;
                FILE* err = nullptr;
                freopen_s(&out, "CONOUT$", "w", stdout);
                freopen_s(&err, "CONOUT$", "w", stderr);
            }
#endif
            return run_cli(argc, argv);
        }
    }
    const bool verifyFullShell =
        qEnvironmentVariableIntValue("OVDS_VERIFY_MODELS_WORKSPACE_CONSOLIDATION") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_DEFAULT_PATHS") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_NAVIGATION_INFO") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_LAUNCH") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_MODEL_SELECTION") != 0 ||
        qEnvironmentVariableIntValue("OVDS_VERIFY_COMPUTE_SETTINGS") != 0;
    if (verifyFullShell && qEnvironmentVariable("QT_QPA_PLATFORM").compare("offscreen", Qt::CaseInsensitive) == 0) {
        qputenv("QT_QPA_PLATFORM", "windows");
        verifierTrace(QStringLiteral("main: verifier platform changed from offscreen to bundled windows"));
    }
    AppOptions options = parseAppOptions(argc, argv);
    QApplication app(argc, argv);
    verifierTrace(QStringLiteral("main: QApplication created"));
    setOpenDssApplicationIdentity();
    migrateLegacyOpenVisualDropletSorterSettings();
    verifierTrace(QStringLiteral("main: settings migrated"));
    QSettings runtimeSettings;
    desktop_app::AppState appState;
    appState.targetClassId =
        runtimeSettings.value("runtime/v1/model/targetClassId", QStringLiteral("1")).toString().trimmed();
    if (appState.targetClassId.isEmpty())
        appState.targetClassId = QStringLiteral("1");
    appState.sortNonTarget = runtimeSettings.value("runtime/v1/sorting/sortNonTarget", false).toBool();
    appState.daqDisabled = false;
#ifdef HAVE_NIDAQMX
    constexpr bool kDaqBuildEnabled = true;
#else
    constexpr bool kDaqBuildEnabled = false;
#endif
    const QString initialDaqStatusText =
        kDaqBuildEnabled ? QStringLiteral("DAQ: unchecked") : QStringLiteral("DAQ: unavailable");
    appState.daqFault = !kDaqBuildEnabled;
    appState.daqStatusText = initialDaqStatusText;
    QString registryFilePath;
    QString registryLoadWarning;
    QJsonObject modelRegistry = loadModelRegistry(&registryFilePath, &registryLoadWarning);
    verifierTrace(QStringLiteral("main: registry loaded"));
    QJsonArray registryEntries = modelRegistry.value("entries").toArray();
    if (registryEntries.isEmpty()) {
        modelRegistry = temporaryStaticModelRegistry();
        registryEntries = modelRegistry.value("entries").toArray();
        registryLoadWarning = "Model registry had no rows; using temporary static fallback.";
    }
    const AppContext appContext(options, resolveAppPaths(registryEntries));
    verifierTrace(QStringLiteral("main: app context resolved"));

    QPixmap splashPixmap(560, 340);
    splashPixmap.fill(QColor("#0B1F5E"));
    {
        QPainter painter(&splashPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPixmap icon(":/branding/opendss-icon-512.png");
        if (!icon.isNull()) {
            painter.drawPixmap(QRect(42, 38, 76, 76), icon, icon.rect());
        }
        painter.setPen(QColor("#FFFFFF"));
        QFont titleFont("Inter", 26, QFont::Bold);
        painter.setFont(titleFont);
        painter.drawText(QRect(136, 46, 360, 38), Qt::AlignLeft | Qt::AlignVCenter, "OpenDSS");
        QFont descriptorFont("Inter", 13, QFont::Medium);
        painter.setFont(descriptorFont);
        painter.setPen(QColor("#E5E7EB"));
        painter.drawText(QRect(138, 86, 360, 26), Qt::AlignLeft | Qt::AlignVCenter, "Droplet sorting workflow suite");
        QPen flowPen(QColor("#7DD3FC"));
        flowPen.setWidth(2);
        painter.setPen(flowPen);
        painter.drawLine(QPointF(54, 250), QPointF(156, 220));
        painter.drawLine(QPointF(156, 220), QPointF(264, 238));
        painter.drawLine(QPointF(264, 238), QPointF(386, 200));
        painter.setBrush(QColor("#2563EB"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(54, 250), 5, 5);
        painter.drawEllipse(QPointF(156, 220), 6, 6);
        painter.setBrush(QColor("#14B8A6"));
        painter.drawEllipse(QPointF(264, 238), 6, 6);
        painter.drawEllipse(QPointF(386, 200), 5, 5);
        painter.setBrush(QColor("#2563EB"));
        painter.drawRoundedRect(QRect(42, 286, 210, 5), 2, 2);
        painter.setBrush(QColor("#14B8A6"));
        painter.drawRoundedRect(QRect(252, 286, 126, 5), 2, 2);
        painter.setFont(QFont("Inter", 11, QFont::Medium));
        painter.setPen(QColor("#FFFFFF"));
        painter.drawText(QRect(42, 304, 360, 20), Qt::AlignLeft | Qt::AlignVCenter, "Loading instrument modules...");
    }
    QSplashScreen splash(splashPixmap);
    splash.setObjectName("OpenDssSplashScreen");
    QElapsedTimer splashTimer;
    splashTimer.start();
    splash.show();
    app.processEvents();

    const QString& logPath = appContext.paths.sessionLogPath;
    initializeCrashAndLogHandling(logPath);
    logMessage(QString("Log file: %1").arg(logPath));

    MainWindow window(appContext);
    verifierTrace(QStringLiteral("main: entering window setup"));
    return window.runSetupAndEventLoop(app, runtimeSettings, appState, registryEntries, registryFilePath,
                                       registryLoadWarning, splash, splashTimer);
}
