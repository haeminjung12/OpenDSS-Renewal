#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QtWidgets>
#include <QtCore>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>
#include <memory>
#include <string>

#include "app_context.h"
#include "app_options.h"
#include "app_paths.h"
#include "app_state.h"
#include "crash_handler.h"
#include "main_window.h"
#include "model_registry_service.h"
#include "workspace_dataset.h"
#include "../cli_runner.h"

namespace {

constexpr const char* kOrganizationName = "Hamamatsu";
constexpr const char* kApplicationName = "OpenDSS";
constexpr const char* kLegacyApplicationName = "OpenVisualDropletSorter";
constexpr const char* kLegacySettingsMigrationMarker = "migration/v1/importedOpenVisualDropletSorter";

bool hasArgument(int argc, char* argv[], const QString& expected) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == expected)
            return true;
    }
    return false;
}

void setOpenDssApplicationIdentity() {
    QCoreApplication::setOrganizationName(kOrganizationName);
    QCoreApplication::setApplicationName(kApplicationName);
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

    auto readMetadataManifest = [](const QString& path, QJsonDocument* doc, QString* errorMessage) {
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
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
        QFile out(outputPath);
        if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            out.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    }

    if (!failures.isEmpty()) {
        std::fprintf(stderr, "Dataset metadata-only verifier failed: %s\n",
                     failures.join("; ").toLocal8Bit().constData());
        return 2;
    }
    std::printf("Dataset metadata-only verifier passed.\n");
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_SETTINGS_MIGRATION") != 0 ||
        hasArgument(argc, argv, QStringLiteral("--verify-settings-migration"))) {
        return runSettingsMigrationVerifier(argc, argv);
    }
    if (!qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed().isEmpty() &&
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_METADATA_ONLY") == "1") {
        return runDatasetWorkspaceMetadataOnlyVerifier(argc, argv);
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
    AppOptions options = parseAppOptions(argc, argv);
    QApplication app(argc, argv);
    setOpenDssApplicationIdentity();
    migrateLegacyOpenVisualDropletSorterSettings();
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
    QJsonArray registryEntries = modelRegistry.value("entries").toArray();
    if (registryEntries.isEmpty()) {
        modelRegistry = temporaryStaticModelRegistry();
        registryEntries = modelRegistry.value("entries").toArray();
        registryLoadWarning = "Model registry had no rows; using temporary static fallback.";
    }
    const AppContext appContext(options, resolveAppPaths(registryEntries));

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
    return window.runSetupAndEventLoop(app, runtimeSettings, appState, registryEntries, registryFilePath,
                                       registryLoadWarning, splash, splashTimer);
}
