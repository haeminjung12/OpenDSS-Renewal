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
    const QString savedValue = settings.value(settingsKey, fallbackValue).toString();
    const QString initialValue =
        settingsKey == QLatin1String("settings/pythonTrainer") && !linkedValue.isEmpty() ? linkedValue : savedValue;
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
    if (linkedEdit) {
        QObject::connect(linkedEdit, &QLineEdit::textChanged, edit, [edit](const QString& text) {
            if (edit->text() == text)
                return;
            QSignalBlocker blocker(edit);
            edit->setText(text);
        });
    }
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

QString normalizedComputeDevice(QString value) {
    value = value.trimmed().toLower();
    if (value == "gpu" || value == "cuda")
        return QStringLiteral("cuda");
    if (value == "cpu")
        return QStringLiteral("cpu");
    return QStringLiteral("auto");
}

void persistComputeDevice(const QString& value) {
    QSettings settings;
    const QString normalized = normalizedComputeDevice(value);
    settings.setValue("settings/computeDevice", normalized);
    settings.setValue("validator/device", normalized);
    settings.sync();
}

void prepareComputeDeviceCombo(QComboBox* combo) {
    if (!combo)
        return;
    if (combo->count() == 0) {
        combo->addItem("Auto", QStringLiteral("auto"));
        combo->addItem("CPU", QStringLiteral("cpu"));
        combo->addItem("GPU", QStringLiteral("cuda"));
    }
    combo->setToolTip("Shared compute device for live inference, training, and validation.");
    QSettings settings;
    const QString initial =
        normalizedComputeDevice(settings.value("settings/computeDevice", settings.value("validator/device", "auto")).toString());
    const int index = combo->findData(initial);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    nameWidget(combo, "SettingsWorkspaceComputeDeviceComboBox");
    persistComputeDevice(combo->currentData().toString());
    QObject::connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), combo, [combo](int) {
        persistComputeDevice(combo->currentData().toString());
    });
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

    auto settingsComputePanel = makePanel("Compute");
    settingsComputePanel->setObjectName("SettingsComputePanel");
    auto settingsComputeBody = makePanelBody(settingsComputePanel);
    prepareComputeDeviceCombo(controls.computeDeviceCombo);
    if (controls.computeDeviceCombo) {
        auto* computeRow = new QGridLayout;
        computeRow->setContentsMargins(0, 0, 0, 0);
        computeRow->setHorizontalSpacing(8);
        auto* computeLabel = new QLabel("Compute device");
        computeLabel->setProperty("metricLabel", true);
        computeRow->addWidget(computeLabel, 0, 0);
        computeRow->addWidget(controls.computeDeviceCombo, 0, 1);
        computeRow->setColumnStretch(1, 1);
        settingsComputeBody->addLayout(computeRow);
    } else {
        settingsComputeBody->addWidget(new QLabel("Compute device: Auto"));
    }
    settingsStackLayout->addWidget(settingsComputePanel);

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
    settingsHardwareGrid->setColumnStretch(1, 1);
    settingsHardwareBody->addLayout(settingsHardwareGrid);
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
    settingsStackLayout->addStretch(1);
    settingsStack->setLayout(settingsStackLayout);

    auto settingsDetailScroll = new QScrollArea;
    nameWidget(settingsDetailScroll, "SettingsWorkspaceDetailScrollArea");
    settingsDetailScroll->setWidgetResizable(true);
    settingsDetailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto settingsDetailStack = new QWidget;
    nameWidget(settingsDetailStack, "SettingsWorkspaceDetailStack");
    auto settingsDetailLayout = new QVBoxLayout;
    settingsDetailLayout->setContentsMargins(0, 0, 0, 0);
    settingsDetailLayout->setSpacing(12);
    settingsDetailLayout->addWidget(settingsHardwarePanel);
    settingsDetailLayout->addWidget(settingsLoggingPanel);
    settingsDetailLayout->addStretch(1);
    settingsDetailStack->setLayout(settingsDetailLayout);
    settingsDetailScroll->setWidget(settingsDetailStack);

    settingsStack->setMinimumWidth(380);
    settingsDetailStack->setMinimumWidth(320);
    settingsScroll->setWidget(settingsStack);

    auto* settingsWorkspaceSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(settingsWorkspaceSplitter, "SettingsWorkspaceSplitter");
    settingsWorkspaceSplitter->addWidget(settingsScroll);
    settingsWorkspaceSplitter->addWidget(settingsDetailScroll);
    settingsWorkspaceSplitter->setStretchFactor(0, 1);
    settingsWorkspaceSplitter->setStretchFactor(1, 0);
    desktop_app::ui::configureWorkspaceSplitter(settingsWorkspaceSplitter, "workspace/settings/splitter",
                                                {620, 420}, {380, 320});
    settingsWorkspaceOuterLayout->addWidget(settingsWorkspaceSplitter, 1);
    settingsWorkspacePage->setLayout(settingsWorkspaceOuterLayout);
    return settingsWorkspacePage;
}

} // namespace desktop_app::workspace
