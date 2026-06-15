#include "image_validation_dialog.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
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

#include "object_names.h"

ImageValidationDialog::ImageValidationDialog(QWidget* parent, const QString& initialPython, const QString& initialModel,
                                             const QString& initialMetadata, const QString& initialDataset,
                                             const QString& initialOutput, const QString& trainerPythonPath)
    : QDialog(parent), pythonPath(trainerPythonPath) {
    setWindowTitle("Image Validation");
    resize(920, 700);
    setMinimumSize(760, 540);

    pythonEdit = new QLineEdit(initialPython.isEmpty() ? "python" : initialPython);
    modelEdit = new QLineEdit(initialModel);
    metadataEdit = new QLineEdit(initialMetadata);
    datasetEdit = new QLineEdit(initialDataset);
    outputEdit = new QLineEdit(initialOutput);
    deviceCombo = new QComboBox;
    deviceCombo->addItems({"auto", "cpu", "cuda"});
    schemaCombo = new QComboBox;
    schemaCombo->addItems({"default binary 0,1", "legacy Empty,Single,MoreThanTwo", "custom classes"});
    classesEdit = new QLineEdit("0,1");

    nameWidget(pythonEdit, "ValidatorPythonExecutableEdit");
    nameWidget(modelEdit, "ValidatorModelEdit");
    nameWidget(metadataEdit, "ValidatorMetadataEdit");
    nameWidget(datasetEdit, "ValidatorDatasetEdit");
    nameWidget(outputEdit, "ValidatorOutputEdit");
    nameWidget(deviceCombo, "ValidatorDeviceComboBox");
    nameWidget(schemaCombo, "ValidatorClassSchemaComboBox");
    nameWidget(classesEdit, "ValidatorClassesEdit");

    auto* form = new QGridLayout;
    addPathRow(form, 0, "Python", pythonEdit, false, "Python executable");
    addPathRow(form, 1, "Model", modelEdit, false, "ONNX model");
    addPathRow(form, 2, "Metadata", metadataEdit, false, "Model metadata JSON");
    addPathRow(form, 3, "Dataset", datasetEdit, true, "Labeled dataset folder");
    addPathRow(form, 4, "Output", outputEdit, true, "Validation output folder");
    form->addWidget(new QLabel("Device"), 5, 0);
    form->addWidget(deviceCombo, 5, 1);
    form->addWidget(new QLabel("Class Schema"), 6, 0);
    form->addWidget(schemaCombo, 6, 1);
    form->addWidget(classesEdit, 6, 2);

    statusLabel = new QLabel("Idle");
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    nameWidget(statusLabel, "ValidatorStatusLabel");
    commandPreview = new QPlainTextEdit;
    commandPreview->setReadOnly(true);
    commandPreview->setMaximumHeight(90);
    nameWidget(commandPreview, "ValidatorCommandPreview");
    logText = new QPlainTextEdit;
    logText->setReadOnly(true);
    nameWidget(logText, "ValidatorLogTextEdit");
    artifactLabel = new QLabel("Artifacts: not available");
    artifactLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    artifactLabel->setWordWrap(true);
    nameWidget(artifactLabel, "ValidatorArtifactLabel");

    startButton = new QPushButton("Run Image Validation");
    cancelButton = new QPushButton("Cancel");
    openSummaryButton = new QPushButton("Open Summary");
    openOutputButton = new QPushButton("Open Output Folder");
    cancelButton->setEnabled(false);
    openSummaryButton->setEnabled(false);
    openOutputButton->setEnabled(false);
    nameWidget(startButton, "ValidatorRunImageButton");
    nameWidget(cancelButton, "ValidatorCancelButton");
    nameWidget(openSummaryButton, "ValidatorOpenSummaryButton");
    nameWidget(openOutputButton, "ValidatorOpenOutputButton");

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(startButton);
    buttons->addWidget(cancelButton);
    buttons->addStretch(1);
    buttons->addWidget(openSummaryButton);
    buttons->addWidget(openOutputButton);

    auto* note = new QLabel("Sequence validation remains unavailable here: runner-wrapped replay is not implemented, "
                            "and existing artifact comparison is internal/provisional only.");
    note->setWordWrap(true);
    note->setStyleSheet("color:#6b4f00;");

    auto* layout = new QVBoxLayout;
    layout->addLayout(form);
    layout->addWidget(new QLabel("Command Preview"));
    layout->addWidget(commandPreview);
    layout->addWidget(statusLabel);
    layout->addWidget(logText, 1);
    layout->addWidget(artifactLabel);
    layout->addWidget(note);
    layout->addLayout(buttons);
    setLayout(layout);

    auto update = [this]() {
        terminalStatus.clear();
        updatePreviewAndGate();
    };
    for (auto* edit : {pythonEdit, modelEdit, metadataEdit, datasetEdit, outputEdit}) {
        QObject::connect(edit, &QLineEdit::textChanged, update);
    }
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
    classesEdit->setEnabled(schemaCombo->currentIndex() == 2);
    updatePreviewAndGate();
}

ImageValidationDialog::~ImageValidationDialog() {
    stopProcess(1000);
}

void ImageValidationDialog::addPathRow(QGridLayout* layout, int row, const QString& label, QLineEdit* edit,
                                       bool directory, const QString& dialogTitle) {
    auto* browse = new QPushButton("Browse");
    layout->addWidget(new QLabel(label), row, 0);
    layout->addWidget(edit, row, 1);
    layout->addWidget(browse, row, 2);
    QObject::connect(browse, &QPushButton::clicked, this, [this, edit, directory, dialogTitle]() {
        QString current = edit->text().trimmed();
        QString selected;
        if (directory) {
            selected = QFileDialog::getExistingDirectory(this, dialogTitle, current);
        } else {
            selected = QFileDialog::getOpenFileName(this, dialogTitle, current);
        }
        if (!selected.isEmpty())
            edit->setText(QDir::toNativeSeparators(selected));
    });
}

QStringList ImageValidationDialog::commandArguments() const {
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

QString ImageValidationDialog::missingInputs() const {
    QStringList missing;
    if (pythonEdit->text().trimmed().isEmpty())
        missing << "Python executable";
    if (!QFileInfo(modelEdit->text().trimmed()).isFile())
        missing << "model file";
    if (!QFileInfo(metadataEdit->text().trimmed()).isFile())
        missing << "metadata file";
    if (!QFileInfo(datasetEdit->text().trimmed()).isDir())
        missing << "dataset folder";
    if (outputEdit->text().trimmed().isEmpty())
        missing << "output folder";
    if (schemaCombo->currentIndex() == 2 && classesEdit->text().trimmed().isEmpty())
        missing << "custom class list";
    if (!pythonPath.isEmpty() && !QFileInfo(pythonPath).isDir())
        missing << "training/python module path";
    return missing.join(", ");
}

void ImageValidationDialog::updatePreviewAndGate() {
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

void ImageValidationDialog::loadSettings() {
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

void ImageValidationDialog::saveSettings() const {
    QSettings settings;
    settings.setValue("validator/pythonExecutable", pythonEdit->text().trimmed());
    settings.setValue("validator/imageDataset", datasetEdit->text().trimmed());
    settings.setValue("validator/outputFolder", outputEdit->text().trimmed());
    settings.setValue("validator/device", deviceCombo->currentText());
    settings.setValue("validator/schemaMode", schemaCombo->currentIndex());
    settings.setValue("validator/classes", classesEdit->text().trimmed());
}

void ImageValidationDialog::startValidation() {
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
    artifactLabel->setText("Artifacts: pending");
    statusLabel->setText("Running image validation...");

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
        terminalStatus = "Failed to start validator: " + process->errorString();
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

void ImageValidationDialog::cancelValidation() {
    if (!process || process->state() == QProcess::NotRunning)
        return;
    canceled = true;
    statusLabel->setText("Canceling image validation...");
    stopProcess(2500);
}

void ImageValidationDialog::stopProcess(int timeoutMs) {
    if (!process || process->state() == QProcess::NotRunning)
        return;
    process->terminate();
    if (!process->waitForFinished(timeoutMs)) {
        process->kill();
        process->waitForFinished(1000);
    }
}

void ImageValidationDialog::finishValidation(int exitCode, QProcess::ExitStatus exitStatus) {
    appendLog(QString::fromUtf8(process->readAllStandardOutput()));
    appendLog(QString::fromUtf8(process->readAllStandardError()));
    const bool crashed = exitStatus == QProcess::CrashExit;
    if (canceled) {
        terminalStatus = "Canceled.";
    } else if (crashed) {
        terminalStatus = "Failed: validator process crashed.";
    } else if (exitCode == 0) {
        terminalStatus = "Completed.";
    } else {
        terminalStatus = QString("Failed: validator exited with code %1.").arg(exitCode);
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

void ImageValidationDialog::appendLog(const QString& text) {
    if (text.isEmpty())
        return;
    logText->moveCursor(QTextCursor::End);
    logText->insertPlainText(text);
    logText->moveCursor(QTextCursor::End);
}

bool ImageValidationDialog::loadSummaryArtifacts() {
    QString discovered = QDir(outputEdit->text().trimmed()).filePath("image_validation/validation_summary.json");
    if (!QFileInfo::exists(discovered)) {
        QRegularExpression re("\"summary_path\"\\s*:\\s*\"([^\"]+)\"");
        QRegularExpressionMatch match = re.match(logText->toPlainText());
        if (match.hasMatch()) {
            discovered = match.captured(1);
        }
    }
    if (!QFileInfo::exists(discovered)) {
        artifactLabel->setText("Artifacts: validation_summary.json was not found. Check diagnostic output above.");
        openOutputButton->setEnabled(QFileInfo(outputEdit->text().trimmed()).exists());
        return false;
    }
    summaryPath = QFileInfo(discovered).absoluteFilePath();
    QFile file(summaryPath);
    QString status = "unknown";
    QString metrics;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            status = obj.value("status").toString(status);
            QJsonObject m = obj.value("metrics").toObject();
            if (!m.isEmpty()) {
                metrics = QString(" accuracy=%1 macro_f1=%2")
                              .arg(m.value("accuracy").toDouble(), 0, 'f', 4)
                              .arg(m.value("macro_f1").toDouble(), 0, 'f', 4);
            }
        }
    }
    artifactLabel->setText(QString("Artifacts: %1\nSummary status: %2%3\nExpected CSVs: predictions.csv, "
                                   "confusion_matrix.csv, class_metrics.csv, failure_cases.csv")
                               .arg(summaryPath, status, metrics));
    openSummaryButton->setEnabled(true);
    openOutputButton->setEnabled(true);
    return true;
}
