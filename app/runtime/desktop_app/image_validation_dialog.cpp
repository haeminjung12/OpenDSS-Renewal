#include "image_validation_dialog.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

#include <array>

#include "model_registry_service.h"
#include "object_names.h"

namespace {

QStringList jsonStringList(const QJsonArray& values) {
    QStringList result;
    for (const QJsonValue& value : values)
        result << value.toString();
    return result;
}

QJsonObject loadJsonObjectFile(const QString& path) {
    QFile file(path.trimmed());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

QString defaultDisplayLabelForClassId(const QStringList& classIds, const QString& classId) {
    if (classIds == QStringList{"0", "1"}) {
        if (classId == "0")
            return "Non-target";
        if (classId == "1")
            return "Target";
    }
    if (classIds == QStringList{"0", "1", "2"}) {
        if (classId == "0")
            return "Non-target A";
        if (classId == "1")
            return "Target";
        if (classId == "2")
            return "Non-target B";
    }
    return classId;
}

} // namespace

ImageValidationWidget::ImageValidationWidget(QWidget* parent, const QString& initialPython, const QString& initialModel,
                                             const QString& initialMetadata, const QString& initialDataset,
                                             const QString& initialOutput, const QString& trainerPythonPath,
                                             ObjectNameMode objectNameMode)
    : QWidget(parent), pythonPath(trainerPythonPath), workspaceMode(objectNameMode == ObjectNameMode::Workspace) {
    pythonEdit = new QLineEdit(initialPython.isEmpty() ? "python" : initialPython);
    modelEdit = new QLineEdit(initialModel);
    metadataEdit = new QLineEdit(initialMetadata);
    datasetEdit = new QLineEdit(initialDataset);
    outputEdit = new QLineEdit(initialOutput);
    deviceCombo = new QComboBox;
    deviceCombo->addItems({"auto", "cpu", "cuda"});
    schemaCombo = new QComboBox;
    schemaCombo->addItems({"Target / Non-target", "Legacy 3-label model", "Custom labels"});
    classesEdit = new QLineEdit("0,1");

    const bool workspaceNames = workspaceMode;
    nameWidget(pythonEdit, workspaceNames ? "ValidatorWorkspacePythonExecutableEdit" : "ValidatorPythonExecutableEdit");
    nameWidget(modelEdit, workspaceNames ? "ValidatorWorkspaceModelEdit" : "ValidatorModelEdit");
    nameWidget(metadataEdit, workspaceNames ? "ValidatorWorkspaceMetadataEdit" : "ValidatorMetadataEdit");
    nameWidget(datasetEdit, workspaceNames ? "ValidatorWorkspaceDatasetEdit" : "ValidatorDatasetEdit");
    nameWidget(outputEdit, workspaceNames ? "ValidatorWorkspaceOutputEdit" : "ValidatorOutputEdit");
    nameWidget(deviceCombo, workspaceNames ? "ValidatorWorkspaceDeviceComboBox" : "ValidatorDeviceComboBox");
    nameWidget(schemaCombo, workspaceNames ? "ValidatorWorkspaceClassSchemaComboBox" : "ValidatorClassSchemaComboBox");
    nameWidget(classesEdit, workspaceNames ? "ValidatorWorkspaceClassesEdit" : "ValidatorClassesEdit");

    auto* form = new QGridLayout;
    if (workspaceMode) {
        addPathRow(form, 0, "Model", modelEdit, false, "Model file", defaultOpenDssModelsPath(),
                   findPackagedAppPath("models"));
        addPathRow(form, 1, "Training images", datasetEdit, true, "Training images folder",
                   defaultOpenDssPreparedDatasetsPath(), findPackagedAppPath("datasets/prepared"));
        addPathRow(form, 2, "Results folder", outputEdit, true, "Validation output folder",
                   defaultOpenDssValidationRunsPath());
    } else {
        addPathRow(form, 0, "Python", pythonEdit, false, "Python executable");
        addPathRow(form, 1, "Model", modelEdit, false, "Model file", defaultOpenDssModelsPath(),
                   findPackagedAppPath("models"));
        addPathRow(form, 2, "Model details", metadataEdit, false, "Model details JSON", defaultOpenDssModelsPath(),
                   findPackagedAppPath("models"));
        addPathRow(form, 3, "Training images", datasetEdit, true, "Training images folder",
                   defaultOpenDssPreparedDatasetsPath(), findPackagedAppPath("datasets/prepared"));
        addPathRow(form, 4, "Results folder", outputEdit, true, "Validation output folder",
                   defaultOpenDssValidationRunsPath());
        form->addWidget(new QLabel("Device"), 5, 0);
        form->addWidget(deviceCombo, 5, 1);
        form->addWidget(new QLabel("Class setup"), 6, 0);
        form->addWidget(schemaCombo, 6, 1);
        form->addWidget(classesEdit, 6, 2);
    }
    form->setColumnStretch(1, 1);
    form->setColumnStretch(2, 1);

    statusLabel = new QLabel("Idle");
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    nameWidget(statusLabel, workspaceNames ? "ValidatorWorkspaceStatusLabel" : "ValidatorStatusLabel");
    commandPreview = new QPlainTextEdit;
    commandPreview->setReadOnly(true);
    commandPreview->setMaximumHeight(90);
    nameWidget(commandPreview, workspaceNames ? "ValidatorWorkspaceCommandPreview" : "ValidatorCommandPreview");
    logText = new QPlainTextEdit;
    logText->setReadOnly(true);
    nameWidget(logText, workspaceNames ? "ValidatorWorkspaceLogTextEdit" : "ValidatorLogTextEdit");
    artifactLabel = new QLabel(workspaceMode ? "No validation results yet." : "Results: not available");
    artifactLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    artifactLabel->setWordWrap(true);
    nameWidget(artifactLabel, workspaceNames ? "ValidatorWorkspaceArtifactLabel" : "ValidatorArtifactLabel");

    startButton = new QPushButton(workspaceMode ? "Run Validation" : "Run Image Validation");
    cancelButton = new QPushButton("Cancel");
    openSummaryButton = new QPushButton(workspaceMode ? "Open Results" : "Open Summary");
    openOutputButton = new QPushButton(workspaceMode ? "Open Folder" : "Open Output Folder");
    cancelButton->setEnabled(false);
    openSummaryButton->setEnabled(false);
    openOutputButton->setEnabled(false);
    nameWidget(startButton, workspaceNames ? "ValidatorWorkspaceOpenImageValidationButton" : "ValidatorRunImageButton");
    nameWidget(cancelButton, workspaceNames ? "ValidatorWorkspaceCancelButton" : "ValidatorCancelButton");
    nameWidget(openSummaryButton, workspaceNames ? "ValidatorWorkspaceOpenSummaryButton" : "ValidatorOpenSummaryButton");
    nameWidget(openOutputButton, workspaceNames ? "ValidatorWorkspaceOpenOutputButton" : "ValidatorOpenOutputButton");
    const std::array<QWidget*, 16> ownedWidgets = {
        pythonEdit,       modelEdit,     metadataEdit, datasetEdit,     outputEdit,       deviceCombo,
        schemaCombo,      classesEdit,   statusLabel,  commandPreview,  logText,          artifactLabel,
        startButton,      cancelButton,  openSummaryButton, openOutputButton,
    };
    for (QWidget* widget : ownedWidgets) {
        widget->setParent(this);
    }

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    for (auto* button : {startButton, cancelButton, openSummaryButton, openOutputButton}) {
        button->setMinimumHeight(30);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    }
    buttons->addWidget(startButton);
    buttons->addWidget(cancelButton);
    buttons->addStretch(1);
    buttons->addWidget(openSummaryButton);
    buttons->addWidget(openOutputButton);

    auto* layout = new QVBoxLayout;
    layout->addLayout(form);
    layout->addWidget(statusLabel);
    layout->addWidget(artifactLabel);
    layout->addLayout(buttons);
    if (!workspaceMode) {
        layout->addWidget(new QLabel("Command Preview"));
        layout->addWidget(commandPreview);
        layout->addWidget(logText, 1);
        auto* note = new QLabel("Sequence validation is not available here yet.");
        note->setWordWrap(true);
        note->setStyleSheet("color:#6b4f00;");
        layout->addWidget(note);
    } else {
        pythonEdit->hide();
        metadataEdit->hide();
        deviceCombo->hide();
        schemaCombo->hide();
        classesEdit->hide();
        commandPreview->hide();
        detailsLabel = new QLabel("Details");
        detailsLabel->setProperty("metricLabel", true);
        detailsLabel->hide();
        logText->hide();
        logText->setMaximumHeight(140);
        layout->addWidget(detailsLabel);
        layout->addWidget(logText);
    }
    setLayout(layout);

    auto update = [this]() {
        terminalStatus.clear();
        updatePreviewAndGate();
    };
    for (auto* edit : {pythonEdit, modelEdit, datasetEdit, outputEdit}) {
        QObject::connect(edit, &QLineEdit::textChanged, update);
    }
    QObject::connect(metadataEdit, &QLineEdit::textChanged, [this, update]() {
        syncSchemaFromMetadata();
        update();
    });
    QObject::connect(classesEdit, &QLineEdit::textChanged, update);
    QObject::connect(deviceCombo, &QComboBox::currentTextChanged, update);
    QObject::connect(schemaCombo, &QComboBox::currentTextChanged, [this, update]() {
        classesEdit->setEnabled(schemaCombo->currentIndex() == 2);
        update();
    });
    QObject::connect(startButton, &QPushButton::clicked, [this]() { startValidation(); });
    QObject::connect(cancelButton, &QPushButton::clicked, [this]() { cancelValidation(); });
    QObject::connect(openSummaryButton, &QPushButton::clicked, [this]() {
        if (!summaryPath.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(summaryPath));
    });
    QObject::connect(openOutputButton, &QPushButton::clicked, [this]() {
        QString path = outputEdit->text().trimmed();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
    });

    loadSettings();
    syncSchemaFromMetadata();
    classesEdit->setEnabled(schemaCombo->currentIndex() == 2);
    updatePreviewAndGate();
}

ImageValidationWidget::~ImageValidationWidget() {
    stopProcess(1000);
}

void ImageValidationWidget::setSummaryChangedCallback(SummaryChangedCallback callback) {
    summaryChangedCallback = std::move(callback);
}

void ImageValidationWidget::addPathRow(QGridLayout* layout, int row, const QString& label, QLineEdit* edit,
                                       bool directory, const QString& dialogTitle, const QString& workspacePath,
                                       const QString& packagedPath) {
    auto* browse = new QPushButton("Browse");
    layout->addWidget(new QLabel(label), row, 0);
    layout->addWidget(edit, row, 1);
    layout->addWidget(browse, row, 2);
    QObject::connect(browse, &QPushButton::clicked,
                     this, [this, edit, directory, dialogTitle, workspacePath, packagedPath]() {
        const QString current = edit->text().trimmed();
        QString selected;
        if (directory) {
            const QString startDir = chooseExistingDirectoryDialogPath(current, workspacePath, packagedPath);
            selected = QFileDialog::getExistingDirectory(this, dialogTitle, startDir);
        } else {
            const QString startPath = chooseOpenFileDialogPath(current, workspacePath, packagedPath);
            selected = QFileDialog::getOpenFileName(this, dialogTitle, startPath);
        }
        if (!selected.isEmpty())
            edit->setText(QDir::toNativeSeparators(selected));
    });
}

QStringList ImageValidationWidget::commandArguments() const {
    QStringList args = {"-m",
                        "droplet_trainer",
                        "validate-images",
                        "--model",
                        modelEdit->text().trimmed(),
                        "--metadata",
                        metadataEdit->text().trimmed(),
                        "--dataset",
                        datasetEdit->text().trimmed(),
                        "--output",
                        outputEdit->text().trimmed(),
                        "--device",
                        deviceCombo->currentText(),
                        "--json"};
    if (schemaCombo->currentIndex() == 1) {
        args << "--legacy-schema";
    } else if (schemaCombo->currentIndex() == 2) {
        args << "--classes" << classesEdit->text().trimmed();
    }
    return args;
}

void ImageValidationWidget::syncSchemaFromMetadata() {
    const QJsonObject metadata = loadJsonObjectFile(metadataEdit->text());
    const QStringList classes = jsonStringList(metadata.value("classes").toArray());
    if (classes.isEmpty())
        return;

    int schemaIndex = 2;
    if (classes == QStringList{"Empty", "Single", "MoreThanTwo"}) {
        schemaIndex = 1;
    } else if (classes == QStringList{"0", "1"}) {
        schemaIndex = 0;
    }

    const QJsonObject displayLabels = metadata.value("display_labels").toObject();
    QStringList labelSummary;
    for (const QString& classId : classes) {
        const QString displayLabel = displayLabels.value(classId).toString(defaultDisplayLabelForClassId(classes, classId));
        labelSummary << (displayLabel == classId ? classId : QString("%1 (%2)").arg(displayLabel, classId));
    }

    {
        QSignalBlocker comboBlocker(schemaCombo);
        QSignalBlocker classBlocker(classesEdit);
        schemaCombo->setCurrentIndex(schemaIndex);
        classesEdit->setText(classes.join(","));
    }
    classesEdit->setEnabled(schemaIndex == 2);

    if (workspaceMode && summaryPath.isEmpty() && terminalStatus.isEmpty() && !labelSummary.isEmpty()) {
        artifactLabel->setText("Model labels: " + labelSummary.join(", "));
    }
}

QString ImageValidationWidget::missingInputs() const {
    QStringList missing;
    if (pythonEdit->text().trimmed().isEmpty())
        missing << (workspaceMode ? "validation tool" : "Python executable");
    if (!QFileInfo(modelEdit->text().trimmed()).isFile())
        missing << "model file";
    if (!QFileInfo(metadataEdit->text().trimmed()).isFile())
        missing << "model details file";
    if (!QFileInfo(datasetEdit->text().trimmed()).isDir())
        missing << "training images folder";
    if (outputEdit->text().trimmed().isEmpty())
        missing << "output folder";
    if (schemaCombo->currentIndex() == 2 && classesEdit->text().trimmed().isEmpty())
        missing << "custom class list";
    if (!pythonPath.isEmpty() && !QFileInfo(pythonPath).isDir())
        missing << (workspaceMode ? "validator support files" : "training/python module path");
    return missing.join(", ");
}

void ImageValidationWidget::updatePreviewAndGate() {
    if (!workspaceMode) {
        QString preview = pythonEdit->text().trimmed();
        for (const QString& arg : commandArguments()) {
            QString quoted = arg;
            quoted.replace("\"", "\\\"");
            if (quoted.contains(' ')) {
                quoted = "\"" + quoted + "\"";
            }
            preview += " " + quoted;
        }
        commandPreview->setPlainText(preview);
    }

    const QString missing = missingInputs();
    const bool running = process && process->state() != QProcess::NotRunning;
    startButton->setEnabled(missing.isEmpty() && !running);
    cancelButton->setEnabled(running);
    if (!running) {
        if (!missing.isEmpty()) {
            statusLabel->setText("Blocked: missing " + missing);
        } else if (!terminalStatus.isEmpty()) {
            statusLabel->setText(terminalStatus);
        } else {
            statusLabel->setText("Ready");
        }
    }
}

void ImageValidationWidget::loadSettings() {
    QSettings settings;
    pythonEdit->setText(settings.value("validator/pythonExecutable", pythonEdit->text()).toString());
    datasetEdit->setText(settings.value("validator/imageDataset", datasetEdit->text()).toString());
    outputEdit->setText(settings.value("validator/outputFolder", outputEdit->text()).toString());
    const QString device = settings.value("validator/device", deviceCombo->currentText()).toString();
    int index = deviceCombo->findText(device);
    if (index >= 0)
        deviceCombo->setCurrentIndex(index);
    schemaCombo->setCurrentIndex(settings.value("validator/schemaMode", 0).toInt());
    classesEdit->setText(settings.value("validator/classes", classesEdit->text()).toString());
}

void ImageValidationWidget::saveSettings() const {
    QSettings settings;
    settings.setValue("validator/pythonExecutable", pythonEdit->text().trimmed());
    settings.setValue("validator/imageDataset", datasetEdit->text().trimmed());
    settings.setValue("validator/outputFolder", outputEdit->text().trimmed());
    settings.setValue("validator/device", deviceCombo->currentText());
    settings.setValue("validator/schemaMode", schemaCombo->currentIndex());
    settings.setValue("validator/classes", classesEdit->text().trimmed());
}

void ImageValidationWidget::startValidation() {
    const QString missing = missingInputs();
    if (!missing.isEmpty()) {
        statusLabel->setText("Blocked: missing " + missing);
        return;
    }
    saveSettings();
    terminalStatus.clear();
    summaryPath.clear();
    openSummaryButton->setEnabled(false);
    openOutputButton->setEnabled(false);
    logText->clear();
    if (workspaceMode) {
        logText->hide();
        if (detailsLabel)
            detailsLabel->hide();
        artifactLabel->setText("Validation is running. Results will appear here when the run finishes.");
        statusLabel->setText("Running validation...");
    } else {
        artifactLabel->setText("Results: pending");
        statusLabel->setText("Running image validation...");
    }

    process.reset(new QProcess(this));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!pythonPath.isEmpty()) {
        QString existing = env.value("PYTHONPATH");
        env.insert("PYTHONPATH", existing.isEmpty() ? pythonPath : pythonPath + QDir::listSeparator() + existing);
    }
    process->setProcessEnvironment(env);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(process.get(), &QProcess::readyReadStandardOutput, this,
                     [this]() { appendLog(QString::fromUtf8(process->readAllStandardOutput())); });
    QObject::connect(process.get(), &QProcess::readyReadStandardError, this,
                     [this]() { appendLog(QString::fromUtf8(process->readAllStandardError())); });
    QObject::connect(process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error);
        terminalStatus = "Failed to start the validation tool: " + process->errorString();
        statusLabel->setText(terminalStatus);
        appendLog("PROCESS ERROR: " + process->errorString() + "\n");
        updatePreviewAndGate();
    });
    QObject::connect(process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                     [this](int exitCode, QProcess::ExitStatus exitStatus) { finishValidation(exitCode, exitStatus); });
    canceled = false;
    process->start(pythonEdit->text().trimmed(), commandArguments());
    updatePreviewAndGate();
}

void ImageValidationWidget::cancelValidation() {
    if (!process || process->state() == QProcess::NotRunning)
        return;
    canceled = true;
    statusLabel->setText(workspaceMode ? "Canceling validation..." : "Canceling image validation...");
    stopProcess(2500);
}

void ImageValidationWidget::stopProcess(int timeoutMs) {
    if (!process || process->state() == QProcess::NotRunning)
        return;
    process->terminate();
    if (!process->waitForFinished(timeoutMs)) {
        process->kill();
        process->waitForFinished(1000);
    }
}

void ImageValidationWidget::finishValidation(int exitCode, QProcess::ExitStatus exitStatus) {
    appendLog(QString::fromUtf8(process->readAllStandardOutput()));
    appendLog(QString::fromUtf8(process->readAllStandardError()));
    const bool crashed = exitStatus == QProcess::CrashExit;
    if (canceled) {
        terminalStatus = "Canceled.";
    } else if (crashed) {
        terminalStatus = "Failed: the validation tool stopped unexpectedly.";
    } else if (exitCode == 0) {
        terminalStatus = "Completed.";
    } else {
        terminalStatus = QString("Failed: the validation tool exited with code %1.").arg(exitCode);
    }
    statusLabel->setText(terminalStatus);
    const bool summaryLoaded = loadSummaryArtifacts();
    if (!canceled && !crashed && exitCode == 0 && !summaryLoaded) {
        terminalStatus = "Failed: validation_summary.json was not found.";
        statusLabel->setText(terminalStatus);
    }
    canceled = false;
    updatePreviewAndGate();
}

void ImageValidationWidget::appendLog(const QString& text) {
    if (text.isEmpty())
        return;
    if (workspaceMode) {
        if (detailsLabel)
            detailsLabel->show();
        logText->show();
    }
    logText->moveCursor(QTextCursor::End);
    logText->insertPlainText(text);
    logText->moveCursor(QTextCursor::End);
}

bool ImageValidationWidget::loadSummaryArtifacts() {
    QString discovered = QDir(outputEdit->text().trimmed()).filePath("image_validation/validation_summary.json");
    if (!QFileInfo::exists(discovered)) {
        QRegularExpression re("\"summary_path\"\\s*:\\s*\"([^\"]+)\"");
        QRegularExpressionMatch match = re.match(logText->toPlainText());
        if (match.hasMatch()) {
            discovered = match.captured(1);
        }
    }
    if (!QFileInfo::exists(discovered)) {
        artifactLabel->setText(workspaceMode ? "Validation finished, but the results summary was not found."
                                             : "Results summary was not found. Check the details above.");
        openOutputButton->setEnabled(QFileInfo(outputEdit->text().trimmed()).exists());
        return false;
    }
    summaryPath = QFileInfo(discovered).absoluteFilePath();
    QFile file(summaryPath);
    QString status = "unknown";
    QString metrics;
    int samplesTotal = 0;
    int samplesEvaluated = 0;
    int samplesFailed = 0;
    int samplesIncorrect = 0;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            status = obj.value("status").toString(status);
            const QJsonObject dataset = obj.value("dataset").toObject();
            QJsonObject m = obj.value("metrics").toObject();
            samplesTotal = dataset.value("samples_total").toInt();
            samplesEvaluated = dataset.value("samples_evaluated").toInt();
            samplesFailed = dataset.value("samples_failed").toInt();
            samplesIncorrect = m.value("samples_incorrect").toInt();
            if (!m.isEmpty()) {
                metrics = QString(" accuracy=%1 macro_f1=%2")
                              .arg(m.value("accuracy").toDouble(), 0, 'f', 4)
                              .arg(m.value("macro_f1").toDouble(), 0, 'f', 4);
            }
        }
    }
    if (workspaceMode) {
        const QString reviewSummary =
            samplesFailed > 0 ? "Fail" : (samplesIncorrect > 0 ? "Needs review" : (samplesEvaluated > 0 ? "Pass" : "--"));
        QStringList lines;
        lines << "Latest validation report ready.";
        lines << QString("Checked %1 of %2 images.").arg(samplesEvaluated).arg(samplesTotal);
        lines << QString("Review summary: %1.").arg(reviewSummary);
        if (samplesIncorrect > 0 || samplesFailed > 0) {
            lines << QString("Needs attention: %1 misclassified, %2 failed to evaluate.")
                         .arg(samplesIncorrect)
                         .arg(samplesFailed);
        }
        artifactLabel->setText(lines.join("\n"));
    } else {
        artifactLabel->setText(QString("Results summary: %1\nSummary status: %2%3\nDetailed result files are in the results folder.")
                                   .arg(summaryPath, status, metrics));
    }
    openSummaryButton->setEnabled(true);
    openOutputButton->setEnabled(true);
    if (summaryChangedCallback)
        summaryChangedCallback(summaryPath);
    return true;
}

ImageValidationDialog::ImageValidationDialog(QWidget* parent, const QString& initialPython, const QString& initialModel,
                                             const QString& initialMetadata, const QString& initialDataset,
                                             const QString& initialOutput, const QString& trainerPythonPath)
    : QDialog(parent) {
    setWindowTitle("Image Validation");
    resize(920, 700);
    setMinimumSize(760, 540);

    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(new ImageValidationWidget(this, initialPython, initialModel, initialMetadata, initialDataset,
                                                initialOutput, trainerPythonPath));
    setLayout(layout);
}

ImageValidationDialog::~ImageValidationDialog() = default;
