#pragma once

#include <QMainWindow>

class QApplication;
class QElapsedTimer;
class QJsonArray;
class QSettings;
class QSplashScreen;
class QString;

struct AppContext;

namespace desktop_app {
struct AppState;
}

class MainWindow : public QMainWindow {
  public:
    explicit MainWindow(const AppContext& context, QWidget* parent = nullptr);

    const AppContext& appContext() const;
    int runSetupAndEventLoop(QApplication& app, QSettings& runtimeSettings, desktop_app::AppState& appState,
                             const QJsonArray& registryEntries, const QString& registryFilePath,
                             const QString& registryLoadWarning, QSplashScreen& splash, QElapsedTimer& splashTimer);

  private:
    const AppContext& context_;
};
