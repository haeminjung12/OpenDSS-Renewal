#include "dataset_workspace_controller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtGui/QAction>
#include <QtGui/QTextCursor>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>

#include "dataset_labeler_dialog.h"

DatasetWorkspaceController::DatasetWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    loadTrainerSettings();
    wireDatasetActions();
    wireTrainerPathButtons();
    wireTrainerSettingsPersistence();
}

void DatasetWorkspaceController::openDatasetLabelerPath(const QString& preferredPath) {
    QString initialDataset = deps_.trainerDatasetEdit ? deps_.trainerDatasetEdit->text().trimmed() : QString();
    if (!preferredPath.trimmed().isEmpty()) {
        initialDataset = preferredPath.trimmed();
    }
    if (initialDataset.isEmpty() || !QFileInfo(initialDataset).exists()) {
        initialDataset = deps_.defaultTrainerDataset;
    }
    if (!activeDatasetLabelerDialog_.isNull()) {
        activeDatasetLabelerDialog_->close();
    }
    auto* dialog =
        new DatasetLabelerDialog(deps_.window, QFileInfo(initialDataset).exists() ? initialDataset : QString());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    activeDatasetLabelerDialog_ = dialog;
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, dialog, &QDialog::close);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void DatasetWorkspaceController::openDatasetLabeler() {
    openDatasetLabelerPath(QString());
}

void DatasetWorkspaceController::appendTrainerLog(const QString& text) {
    if (text.isEmpty() || !deps_.trainerResultText) {
        return;
    }
    deps_.trainerResultText->moveCursor(QTextCursor::End);
    deps_.trainerResultText->insertPlainText(text);
    deps_.trainerResultText->moveCursor(QTextCursor::End);
}

void DatasetWorkspaceController::saveTrainerSettings() const {
    if (!deps_.trainerPythonEdit || !deps_.trainerDatasetEdit || !deps_.trainerOutputEdit ||
        !deps_.trainerArchitectureCombo || !deps_.trainerPretrainedImageNetBtn || !deps_.trainerEpochsSpin ||
        !deps_.trainerBatchSpin || !deps_.trainerLrSpin || !deps_.trainerFlipCheck || !deps_.trainerRotationCheck ||
        !deps_.trainerColorJitterCheck || !deps_.trainerRandomCropCheck || !deps_.trainerSchedulerCombo) {
        return;
    }

    QSettings settings;
    settings.setValue("settings/pythonTrainer", deps_.trainerPythonEdit->text().trimmed());
    settings.setValue("settings/datasetsRoot", deps_.trainerDatasetEdit->text().trimmed());
    settings.setValue("trainer/outputDir", deps_.trainerOutputEdit->text().trimmed());
    settings.setValue("trainer/architecture", deps_.trainerArchitectureCombo->currentData().toString());
    settings.setValue("trainer/pretrained", deps_.trainerPretrainedImageNetBtn->isChecked());
    settings.setValue("trainer/epochs", deps_.trainerEpochsSpin->value());
    settings.setValue("trainer/batchSize", deps_.trainerBatchSpin->value());
    settings.setValue("trainer/learningRate", deps_.trainerLrSpin->value());
    settings.setValue("trainer/augment/randomFlip", deps_.trainerFlipCheck->isChecked());
    settings.setValue("trainer/augment/randomRotation", deps_.trainerRotationCheck->isChecked());
    settings.setValue("trainer/augment/colorJitter", deps_.trainerColorJitterCheck->isChecked());
    settings.setValue("trainer/augment/randomCrop", deps_.trainerRandomCropCheck->isChecked());
    settings.setValue("trainer/scheduler", deps_.trainerSchedulerCombo->currentText());
}

void DatasetWorkspaceController::setTrainerBusy(bool busy, bool trainerCommandWasTraining) const {
    if (deps_.trainerEnvCheckBtn)
        deps_.trainerEnvCheckBtn->setEnabled(!busy);
    if (deps_.trainerConfigurePathBtn)
        deps_.trainerConfigurePathBtn->setEnabled(!busy);
    if (deps_.trainerPythonBrowseBtn)
        deps_.trainerPythonBrowseBtn->setEnabled(!busy);
    if (deps_.trainerDatasetBrowseBtn)
        deps_.trainerDatasetBrowseBtn->setEnabled(!busy);
    if (deps_.trainerOutputBrowseBtn)
        deps_.trainerOutputBrowseBtn->setEnabled(!busy);
    if (deps_.trainerStartTrainingBtn) {
        deps_.trainerStartTrainingBtn->setEnabled(!busy);
        deps_.trainerStartTrainingBtn->setText(busy && trainerCommandWasTraining ? "Training..." : "Start Training");
    }
    if (deps_.trainerDryRunBtn)
        deps_.trainerDryRunBtn->setEnabled(!busy);
    if (deps_.trainerCancelBtn)
        deps_.trainerCancelBtn->setEnabled(busy);
    if (deps_.trainerProgressBar) {
        deps_.trainerProgressBar->setRange(busy ? 0 : 0, busy ? 0 : 100);
        deps_.trainerProgressBar->setValue(0);
        deps_.trainerProgressBar->setFormat(busy ? "Running..." : "Idle");
    }
}

QString DatasetWorkspaceController::trainingConfigPath() const {
    if (!deps_.trainerOutputEdit || !deps_.trainerArchitectureCombo || !deps_.trainerEpochsSpin ||
        !deps_.trainerBatchSpin || !deps_.trainerLrSpin || !deps_.trainerPretrainedImageNetBtn ||
        !deps_.trainerFlipCheck || !deps_.trainerRotationCheck || !deps_.trainerColorJitterCheck ||
        !deps_.trainerRandomCropCheck || !deps_.trainerSchedulerCombo) {
        return {};
    }

    const QString outputDir = deps_.trainerOutputEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        return {};
    }

    QDir().mkpath(outputDir);
    QJsonObject config;
    config["schema_version"] = 1;
    config["architecture"] = deps_.trainerArchitectureCombo->currentData().toString();
    config["batch_size"] = deps_.trainerBatchSpin->value();
    config["epochs"] = deps_.trainerEpochsSpin->value();
    config["pretrained"] = deps_.trainerPretrainedImageNetBtn->isChecked();

    QJsonArray stages;
    QJsonObject stage;
    stage["name"] = "gui";
    stage["epochs"] = deps_.trainerEpochsSpin->value();
    stage["learning_rate"] = deps_.trainerLrSpin->value();
    stage["trainable"] = "fine_tune";
    stages.append(stage);
    config["stages"] = stages;

    QJsonObject augmentation;
    augmentation["random_flip"] = deps_.trainerFlipCheck->isChecked();
    augmentation["random_rotation"] = deps_.trainerRotationCheck->isChecked();
    augmentation["color_jitter"] = deps_.trainerColorJitterCheck->isChecked();
    augmentation["random_crop"] = deps_.trainerRandomCropCheck->isChecked();
    config["augmentation"] = augmentation;
    config["scheduler"] = deps_.trainerSchedulerCombo->currentText();

    const QString path = QDir(outputDir).absoluteFilePath("trainer_gui_config.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (deps_.trainerStatusLabel) {
            deps_.trainerStatusLabel->setText("Unable to write trainer config: " + file.errorString());
        }
        return {};
    }
    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();
    return path;
}

QStringList DatasetWorkspaceController::trainerTrainArgs(bool dryRun) const {
    if (!deps_.trainerDatasetEdit || !deps_.trainerOutputEdit) {
        return {};
    }

    QStringList args = {"-m",
                        "droplet_trainer",
                        "train",
                        "--dataset",
                        deps_.trainerDatasetEdit->text().trimmed(),
                        "--output",
                        deps_.trainerOutputEdit->text().trimmed(),
                        "--config",
                        trainingConfigPath(),
                        "--jsonl"};
    if (dryRun) {
        args << "--dry-run";
    }
    return args;
}

QString DatasetWorkspaceController::trainerCommandPreview(const QString& program, const QStringList& args) const {
    QStringList pieces{quoteTrainerArg(program)};
    for (const QString& arg : args) {
        pieces << quoteTrainerArg(arg);
    }
    return pieces.join(" ");
}

QString DatasetWorkspaceController::quoteTrainerArg(QString arg) {
    arg.replace("\"", "\\\"");
    return arg.contains(' ') ? "\"" + arg + "\"" : arg;
}

void DatasetWorkspaceController::loadTrainerSettings() const {
    if (!deps_.trainerPythonEdit || !deps_.trainerDatasetEdit || !deps_.trainerOutputEdit ||
        !deps_.trainerArchitectureCombo || !deps_.trainerPretrainedImageNetBtn || !deps_.trainerPretrainedNoneBtn ||
        !deps_.trainerEpochsSpin || !deps_.trainerBatchSpin || !deps_.trainerLrSpin || !deps_.trainerFlipCheck ||
        !deps_.trainerRotationCheck || !deps_.trainerColorJitterCheck || !deps_.trainerRandomCropCheck ||
        !deps_.trainerSchedulerCombo) {
        return;
    }

    QSettings settings;
    deps_.trainerPythonEdit->setText(
        settings.value("settings/pythonTrainer", deps_.trainerPythonEdit->text()).toString());
    if (QFileInfo(deps_.defaultTrainerDataset).isDir()) {
        deps_.trainerDatasetEdit->setText(
            settings.value("settings/datasetsRoot", QDir::toNativeSeparators(deps_.defaultTrainerDataset)).toString());
    } else {
        deps_.trainerDatasetEdit->setText(
            settings.value("settings/datasetsRoot", deps_.trainerDatasetEdit->text()).toString());
    }
    deps_.trainerOutputEdit->setText(
        settings.value("trainer/outputDir", QDir::toNativeSeparators(deps_.defaultTrainerOutput)).toString());

    const QString arch =
        settings.value("trainer/architecture", deps_.trainerArchitectureCombo->currentData().toString()).toString();
    const int archIndex = deps_.trainerArchitectureCombo->findData(arch);
    if (archIndex >= 0) {
        deps_.trainerArchitectureCombo->setCurrentIndex(archIndex);
    }

    const bool pretrained = settings.value("trainer/pretrained", true).toBool();
    deps_.trainerPretrainedImageNetBtn->setChecked(pretrained);
    deps_.trainerPretrainedNoneBtn->setChecked(!pretrained);
    deps_.trainerEpochsSpin->setValue(settings.value("trainer/epochs", deps_.trainerEpochsSpin->value()).toInt());
    deps_.trainerBatchSpin->setValue(settings.value("trainer/batchSize", deps_.trainerBatchSpin->value()).toInt());
    deps_.trainerLrSpin->setValue(settings.value("trainer/learningRate", deps_.trainerLrSpin->value()).toDouble());
    deps_.trainerFlipCheck->setChecked(
        settings.value("trainer/augment/randomFlip", deps_.trainerFlipCheck->isChecked()).toBool());
    deps_.trainerRotationCheck->setChecked(
        settings.value("trainer/augment/randomRotation", deps_.trainerRotationCheck->isChecked()).toBool());
    deps_.trainerColorJitterCheck->setChecked(
        settings.value("trainer/augment/colorJitter", deps_.trainerColorJitterCheck->isChecked()).toBool());
    deps_.trainerRandomCropCheck->setChecked(
        settings.value("trainer/augment/randomCrop", deps_.trainerRandomCropCheck->isChecked()).toBool());

    const QString scheduler =
        settings.value("trainer/scheduler", deps_.trainerSchedulerCombo->currentText()).toString();
    const int schedulerIndex = deps_.trainerSchedulerCombo->findText(scheduler);
    if (schedulerIndex >= 0) {
        deps_.trainerSchedulerCombo->setCurrentIndex(schedulerIndex);
    }
}

void DatasetWorkspaceController::wireDatasetActions() {
    if (deps_.datasetOpenAction) {
        connect(deps_.datasetOpenAction, &QAction::triggered, this, [this]() { openDatasetLabeler(); });
    }
    if (deps_.datasetBuildAction) {
        connect(deps_.datasetBuildAction, &QAction::triggered, this, [this]() { openDatasetLabeler(); });
    }
    if (deps_.datasetLabelDatasetAction) {
        connect(deps_.datasetLabelDatasetAction, &QAction::triggered, this, [this]() { openDatasetLabeler(); });
    }
}

void DatasetWorkspaceController::wireTrainerPathButtons() {
    if (deps_.trainerPythonBrowseBtn && deps_.trainerPythonEdit) {
        connect(deps_.trainerPythonBrowseBtn, &QPushButton::clicked, this, [this]() {
            const QString file =
                QFileDialog::getOpenFileName(deps_.window, "Select Python executable", deps_.trainerPythonEdit->text(),
                                             "Python executable (python.exe python);;All files (*.*)");
            if (!file.isEmpty()) {
                deps_.trainerPythonEdit->setText(QDir::toNativeSeparators(file));
            }
        });
    }
    if (deps_.trainerDatasetBrowseBtn && deps_.trainerDatasetEdit) {
        connect(deps_.trainerDatasetBrowseBtn, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(deps_.window, "Select dataset directory",
                                                                  deps_.trainerDatasetEdit->text());
            if (!dir.isEmpty()) {
                deps_.trainerDatasetEdit->setText(QDir::toNativeSeparators(dir));
            }
        });
    }
    if (deps_.trainerOutputBrowseBtn && deps_.trainerOutputEdit) {
        connect(deps_.trainerOutputBrowseBtn, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(deps_.window, "Select training output directory",
                                                                  deps_.trainerOutputEdit->text());
            if (!dir.isEmpty()) {
                deps_.trainerOutputEdit->setText(QDir::toNativeSeparators(dir));
            }
        });
    }
}

void DatasetWorkspaceController::wireTrainerSettingsPersistence() {
    const auto save = [this]() { saveTrainerSettings(); };

    for (auto* edit : {deps_.trainerPythonEdit, deps_.trainerDatasetEdit, deps_.trainerOutputEdit}) {
        if (edit) {
            connect(edit, &QLineEdit::textChanged, this, save);
        }
    }
    if (deps_.trainerArchitectureCombo) {
        connect(deps_.trainerArchitectureCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, save);
    }
    if (deps_.trainerPretrainedImageNetBtn) {
        connect(deps_.trainerPretrainedImageNetBtn, &QPushButton::toggled, this, save);
    }
    if (deps_.trainerEpochsSpin) {
        connect(deps_.trainerEpochsSpin, qOverload<int>(&QSpinBox::valueChanged), this, save);
    }
    if (deps_.trainerBatchSpin) {
        connect(deps_.trainerBatchSpin, qOverload<int>(&QSpinBox::valueChanged), this, save);
    }
    if (deps_.trainerLrSpin) {
        connect(deps_.trainerLrSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, save);
    }
    if (deps_.trainerFlipCheck) {
        connect(deps_.trainerFlipCheck, &QCheckBox::toggled, this, save);
    }
    if (deps_.trainerRotationCheck) {
        connect(deps_.trainerRotationCheck, &QCheckBox::toggled, this, save);
    }
    if (deps_.trainerColorJitterCheck) {
        connect(deps_.trainerColorJitterCheck, &QCheckBox::toggled, this, save);
    }
    if (deps_.trainerRandomCropCheck) {
        connect(deps_.trainerRandomCropCheck, &QCheckBox::toggled, this, save);
    }
    if (deps_.trainerSchedulerCombo) {
        connect(deps_.trainerSchedulerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, save);
    }
}
