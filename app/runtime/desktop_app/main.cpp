#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QtWidgets>
#include <QtCore>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>
#include <string>

#include "app_context.h"
#include "app_options.h"
#include "app_paths.h"
#include "app_state.h"
#include "crash_handler.h"
#include "main_window.h"
#include "model_registry_service.h"
#include "../cli_runner.h"

int main(int argc, char* argv[]) {
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
    QCoreApplication::setOrganizationName("Hamamatsu");
    QCoreApplication::setApplicationName("OpenVisualDropletSorter");
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
        painter.drawText(QRect(138, 86, 360, 26), Qt::AlignLeft | Qt::AlignVCenter, "Open Visual Droplet Sorter Suite");
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
