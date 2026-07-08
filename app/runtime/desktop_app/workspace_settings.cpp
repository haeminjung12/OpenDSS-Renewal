#include "workspace_settings.h"

#include <QtCore>
#include <QtWidgets>

#include "app_state.h"
#include "object_names.h"
#include "widget_helpers.h"

namespace desktop_app::workspace {
namespace {

QWidget* makePathField(const QString& label, const QString& settingsKey, const QString& defaultValue,
                       const QString& editObjectName, const QString& browseObjectName, bool filePicker,
                       QLineEdit* linkedEdit = nullptr) {
    auto* wrapper = new QWidget;
    auto* layout = new QGridLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);
    auto* fieldLabel = new QLabel(label);
    fieldLabel->setProperty("metricLabel", true);
    auto* edit = new QLineEdit;
    QSettings settings;
    const QString linkedValue = linkedEdit ? linkedEdit->text().trimmed() : QString();
    const QString fallbackValue = !linkedValue.isEmpty() ? linkedValue : defaultValue;
    const QString initialValue = settings.value(settingsKey, fallbackValue).toString();
    if (!initialValue.isEmpty())
        edit->setText(QDir::toNativeSeparators(initialValue));
    edit->setMinimumWidth(0);
    nameWidget(edit, editObjectName.toLatin1().constData());
    auto* browseButton = new QPushButton("Browse");
    nameWidget(browseButton, browseObjectName.toLatin1().constData());
    layout->addWidget(fieldLabel, 0, 0);
    layout->addWidget(edit, 0, 1);
    layout->addWidget(browseButton, 0, 2);
    wrapper->setLayout(layout);

    auto persist = [edit, linkedEdit, settingsKey]() {
        QSettings settings;
        settings.setValue(settingsKey, edit->text().trimmed());
        settings.sync();
        if (linkedEdit)
            linkedEdit->setText(edit->text());
    };
    QObject::connect(edit, &QLineEdit::editingFinished, edit, persist);
    QObject::connect(browseButton, &QPushButton::clicked, edit, [edit, settingsKey, filePicker, persist]() {
        const QString current = edit->text().trimmed();
        const QString selected = filePicker ? QFileDialog::getOpenFileName(edit, "Select file", current)
                                            : QFileDialog::getExistingDirectory(edit, "Select folder", current);
        if (selected.isEmpty())
            return;
        edit->setText(QDir::toNativeSeparators(selected));
        persist();
    });

    return wrapper;
}

QString textOrEmpty(const QLineEdit* edit) {
    return edit ? edit->text().trimmed() : QString();
}

QString modelsFolderFromModelPath(const QString& modelPath) {
    const QFileInfo modelInfo(modelPath);
    if (modelInfo.exists() && modelInfo.isDir())
        return modelInfo.absoluteFilePath();
    if (!modelPath.trimmed().isEmpty())
        return modelInfo.absolutePath();
    return QString();
}

} // namespace

QWidget* buildSettingsWorkspace(const SettingsWorkspaceControls& controls) {
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    auto settingsWorkspacePage = new QWidget;
    nameWidget(settingsWorkspacePage, "SettingsWorkspace");
    auto settingsWorkspaceOuterLayout = new QVBoxLayout;
    settingsWorkspaceOuterLayout->setContentsMargins(10, 10, 10, 10);
    settingsWorkspaceOuterLayout->setSpacing(0);
    auto settingsScroll = new QScrollArea;
    nameWidget(settingsScroll, "SettingsWorkspaceScrollArea");
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto settingsStack = new QWidget;
    nameWidget(settingsStack, "SettingsWorkspaceStack");
    settingsStack->setMaximumWidth(780);
    auto settingsStackLayout = new QVBoxLayout;
    settingsStackLayout->setContentsMargins(0, 0, 0, 0);
    settingsStackLayout->setSpacing(12);

    auto settingsPathsPanel = makePanel("Paths");
    settingsPathsPanel->setObjectName("SettingsPathsPanel");
    auto settingsPathsBody = makePanelBody(settingsPathsPanel);
    settingsPathsBody->setSpacing(8);
    settingsPathsBody->addWidget(
        makePathField("Output root", "settings/outputRoot", controls.outputRoot, "SettingsWorkspaceOutputRootEdit",
                      "SettingsWorkspaceOutputRootBrowseButton", false, controls.outputRootEdit));
    settingsPathsBody->addWidget(
        makePathField("Models folder", "settings/modelsFolder", modelsFolderFromModelPath(controls.modelPath),
                      "SettingsWorkspaceModelsFolderEdit", "SettingsWorkspaceModelsFolderBrowseButton", false));
    settingsPathsBody->addWidget(makePathField(
        "Datasets root", "settings/datasetsRoot",
        controls.datasetsRoot.isEmpty() ? textOrEmpty(controls.trainerDatasetRootEdit) : controls.datasetsRoot,
        "SettingsWorkspaceDatasetsRootEdit", "SettingsWorkspaceDatasetsRootBrowseButton", false,
        controls.trainerDatasetRootEdit));
    settingsPathsBody->addWidget(
        makePathField("Python trainer", "settings/pythonTrainer", textOrEmpty(controls.trainerPythonEdit),
                      "SettingsWorkspacePythonTrainerEdit", "SettingsWorkspacePythonTrainerBrowseButton", true,
                      controls.trainerPythonEdit));
    settingsStackLayout->addWidget(settingsPathsPanel);

    QSettings settings;

    auto settingsHardwarePanel = makePanel("Hardware");
    settingsHardwarePanel->setObjectName("SettingsHardwarePanel");
    auto settingsHardwareBody = makePanelBody(settingsHardwarePanel);
    auto settingsHardwareGrid = new QGridLayout;
    settingsHardwareGrid->setContentsMargins(0, 0, 0, 0);
    settingsHardwareGrid->setHorizontalSpacing(8);
    settingsHardwareGrid->setVerticalSpacing(8);

    auto addControlRow = [&](int row, const QString& label, QWidget* control) {
        auto* labelWidget = new QLabel(label);
        labelWidget->setProperty("metricLabel", true);
        settingsHardwareGrid->addWidget(labelWidget, row, 0);
        if (control)
            settingsHardwareGrid->addWidget(control, row, 1);
    };

    if (controls.daqChannelEdit) {
        controls.daqChannelEdit->setText(
            settings.value("settings/daqChannel", controls.daqChannelEdit->text()).toString());
        QObject::connect(controls.daqChannelEdit, &QLineEdit::editingFinished, controls.daqChannelEdit,
                         [edit = controls.daqChannelEdit]() {
                             QSettings settings;
                             settings.setValue("settings/daqChannel", edit->text().trimmed());
                             settings.sync();
                         });
    }
    if (controls.amplitudeSpin) {
        controls.amplitudeSpin->setRange(0.1, 10.0);
        controls.amplitudeSpin->setValue(
            settings.value("settings/daqAmplitudeV", controls.amplitudeSpin->value()).toDouble());
        QObject::connect(controls.amplitudeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         controls.amplitudeSpin, [](double value) {
                             QSettings settings;
                             settings.setValue("settings/daqAmplitudeV", value);
                             settings.sync();
                         });
    }
    if (controls.frequencySpin) {
        controls.frequencySpin->setRange(1.0, 100.0);
        controls.frequencySpin->setValue(
            settings.value("settings/daqFrequencyKhz", controls.frequencySpin->value()).toDouble());
        QObject::connect(controls.frequencySpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         controls.frequencySpin, [](double value) {
                             QSettings settings;
                             settings.setValue("settings/daqFrequencyKhz", value);
                             settings.sync();
                         });
    }
    if (controls.durationSpin) {
        controls.durationSpin->setRange(0.1, 50.0);
        controls.durationSpin->setValue(
            settings.value("settings/daqDurationMs", controls.durationSpin->value()).toDouble());
        QObject::connect(controls.durationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), controls.durationSpin,
                         [](double value) {
                             QSettings settings;
                             settings.setValue("settings/daqDurationMs", value);
                             settings.sync();
                         });
    }
    if (controls.delaySpin) {
        controls.delaySpin->setRange(0.0, 100.0);
        controls.delaySpin->setValue(settings.value("settings/daqDelayMs", controls.delaySpin->value()).toDouble());
        QObject::connect(controls.delaySpin, qOverload<double>(&QDoubleSpinBox::valueChanged), controls.delaySpin,
                         [](double value) {
                             QSettings settings;
                             settings.setValue("settings/daqDelayMs", value);
                             settings.sync();
                         });
    }

    addControlRow(0, "DAQ device", controls.daqDeviceCombo);
    addControlRow(1, "DAQ device channel", controls.daqChannelEdit);
    addControlRow(2, "DAQ amplitude", controls.amplitudeSpin);
    addControlRow(3, "DAQ frequency", controls.frequencySpin);
    addControlRow(4, "DAQ duration", controls.durationSpin);
    addControlRow(5, "DAQ delay", controls.delaySpin);
    auto settingsTestModeCheck = new QCheckBox("Test mode");
    nameWidget(settingsTestModeCheck, "SettingsWorkspaceTestModeCheckBox");
    settingsTestModeCheck->setToolTip(
        "Saves the test mode preference for future startups. Launch-only no-hardware flags are not fully reapplied "
        "to the current session.");
    settingsTestModeCheck->setChecked(settings.value("settings/testMode", controls.hardwareFreeMode).toBool());
    if (controls.appState)
        controls.appState->testMode = settingsTestModeCheck->isChecked();
    QObject::connect(settingsTestModeCheck, &QCheckBox::toggled, settingsTestModeCheck,
                     [appState = controls.appState](bool checked) {
                         QSettings settings;
                         settings.setValue("settings/testMode", checked);
                         settings.sync();
                         if (appState)
                             appState->testMode = checked;
                     });
    addControlRow(6, "Test mode preference", settingsTestModeCheck);
    settingsHardwareGrid->setColumnStretch(1, 1);
    settingsHardwareBody->addLayout(settingsHardwareGrid);
    settingsStackLayout->addWidget(settingsHardwarePanel);

    auto settingsLoggingPanel = makePanel("Logging");
    settingsLoggingPanel->setObjectName("SettingsLoggingPanel");
    auto settingsLoggingBody = makePanelBody(settingsLoggingPanel);
    if (controls.logCheck) {
        controls.logCheck->setText("Mirror stdout to session_log.txt");
        controls.logCheck->setChecked(settings.value("settings/mirrorStdout", controls.logCheck->isChecked()).toBool());
        QObject::connect(controls.logCheck, &QCheckBox::toggled, controls.logCheck, [](bool checked) {
            QSettings settings;
            settings.setValue("settings/mirrorStdout", checked);
            settings.sync();
        });
        settingsLoggingBody->addWidget(controls.logCheck);
    }
    auto settingsPruneLogsCheck = new QCheckBox("Prune old session logs");
    nameWidget(settingsPruneLogsCheck, "SettingsWorkspacePruneLogsCheckBox");
    settingsPruneLogsCheck->setChecked(settings.value("settings/pruneOldSessionLogs", true).toBool());
    QObject::connect(settingsPruneLogsCheck, &QCheckBox::toggled, settingsPruneLogsCheck, [](bool checked) {
        QSettings settings;
        settings.setValue("settings/pruneOldSessionLogs", checked);
        settings.sync();
    });
    settingsLoggingBody->addWidget(settingsPruneLogsCheck);
    settingsStackLayout->addWidget(settingsLoggingPanel);
    settingsStackLayout->addStretch(1);
    settingsStack->setLayout(settingsStackLayout);
    auto settingsScrollHost = new QWidget;
    auto settingsScrollHostLayout = new QHBoxLayout;
    settingsScrollHostLayout->setContentsMargins(0, 0, 0, 0);
    settingsScrollHostLayout->addStretch(1);
    settingsScrollHostLayout->addWidget(settingsStack, 1);
    settingsScrollHostLayout->addStretch(1);
    settingsScrollHost->setLayout(settingsScrollHostLayout);
    settingsScroll->setWidget(settingsScrollHost);
    settingsWorkspaceOuterLayout->addWidget(settingsScroll, 1);
    settingsWorkspacePage->setLayout(settingsWorkspaceOuterLayout);
    return settingsWorkspacePage;
}

} // namespace desktop_app::workspace
