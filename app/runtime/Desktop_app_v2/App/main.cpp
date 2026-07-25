// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QVariant>

#include "autogen/environment.h"
#include "../../v2/settings/settings_controller.h"
#include "../../v2/settings/settings_repository.h"
#include "../../v2/results/run_repository.h"
#include "../../v2/results/runs_results_controller.h"
#include "../../v2/state/application_state_store.h"

int main(int argc, char *argv[])
{
    set_qt_environment();
    QApplication app(argc, argv);

    desktop_app::v2::ApplicationStateStore applicationStateStore;
    const QString preferencesFilePath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                            .filePath(QStringLiteral("preferences.json"));
    desktop_app::v2::SettingsRepository settingsRepository(preferencesFilePath, applicationStateStore);
    settingsRepository.load();
    desktop_app::v2::SettingsController settingsController(settingsRepository, applicationStateStore);
    desktop_app::v2::results::RunRepository runRepository(applicationStateStore);
    desktop_app::v2::results::RunsResultsController runsResultsController(runRepository, applicationStateStore);

    QQmlApplicationEngine engine;
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
                                 {QStringLiteral("runsResultsController"), QVariant::fromValue(&runsResultsController)}});
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
