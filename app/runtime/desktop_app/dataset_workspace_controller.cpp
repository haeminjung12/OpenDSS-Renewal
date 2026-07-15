#include "dataset_workspace_controller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QSignalBlocker>
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
#include "model_registry_service.h"

#include <algorithm>

namespace {

struct TrainerModelOption {
    QString registryEntryId;
    QString displayName;
    QString detailText;
    QString modelPath;
    QString metadataPath;
    QString architectureId;
    bool isStarter = false;
    bool isLiveModel = false;
    bool supportsContinue = false;
    bool defaultPretrained = true;
};

QString readRegistryPath(const QJsonObject& entry, const QString& key) {
    return resolvePackagedPathFromRegistryPath(registryString(entry, key));
}

QJsonObject loadJsonObjectFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString architectureIdFromMetadata(const QJsonObject& metadataDoc) {
    const QString configArchitecture = metadataDoc.value("training_config").toObject().value("architecture").toString().trimmed();
    if (!configArchitecture.isEmpty()) {
        return configArchitecture;
    }

    const QJsonObject architecture = metadataDoc.value("architecture").toObject();
    const QString family = architecture.value("family").toString().trimmed();
    const QString variant = architecture.value("variant").toString().trimmed();
    if (family.compare("SqueezeNet", Qt::CaseInsensitive) == 0) {
        if (variant == "1_0" || variant == "1.0") {
            return "squeezenet1_0";
        }
        if (variant == "1_1" || variant == "1.1" || variant.isEmpty()) {
            return "squeezenet1_1";
        }
    }
    return QString();
}

bool metadataLooksLikeStarter(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    const QString metadataStatus = metadataDoc.value("status").toString().trimmed();
    const QString metadataSummary = registryString(entry, "metadata_status").trimmed();
    const QString validationSummary = registryString(entry, "validation_status").trimmed();
    return metadataStatus.contains("transfer_start", Qt::CaseInsensitive) ||
           metadataStatus.contains("untrained", Qt::CaseInsensitive) ||
           metadataStatus.contains("template", Qt::CaseInsensitive) ||
           metadataSummary.contains("starter", Qt::CaseInsensitive) ||
           validationSummary.contains("starter", Qt::CaseInsensitive) ||
           registryString(entry, "live_use_mode").compare("blocked", Qt::CaseInsensitive) == 0;
}

bool entryIsLiveModel(const QJsonObject& entry) {
    return entry.value("selectable_for_normal_live_sorting").toBool(false) ||
           registryString(entry, "state").contains("promoted", Qt::CaseInsensitive) ||
           registryString(entry, "promotion_status").contains("current", Qt::CaseInsensitive);
}

QString trainerModeKey(const QComboBox* combo) {
    if (!combo) {
        return "new_copy";
    }
    const QString key = combo->currentData().toString().trimmed();
    return key.isEmpty() ? "new_copy" : key;
}

QString trainerModeLabel(const QString& modeKey) {
    return modeKey == "continue_copy" ? "Keep training the selected model"
                                      : "Start a new trained copy";
}

QString fallbackDisplayName(const QJsonObject& entry) {
    const QString display = registryString(entry, "display_name").trimmed();
    if (!display.isEmpty()) {
        return display;
    }
    const QString entryId = registryString(entry, "registry_entry_id").trimmed();
    return entryId.isEmpty() ? QString("Unnamed model") : entryId;
}

QString normalizedDatasetSelectionPath(const QString& path) {
    const QFileInfo info(path.trimmed());
    if (!info.exists()) {
        return path.trimmed();
    }
    if (info.isFile()) {
        return info.absoluteFilePath();
    }
    const QDir dir(info.absoluteFilePath());
    const QString metadataManifest = dir.filePath("metadata/dataset_manifest.json");
    if (QFileInfo::exists(metadataManifest)) {
        return metadataManifest;
    }
    const QString rootManifest = dir.filePath("manifest.json");
    if (QFileInfo::exists(rootManifest)) {
        return rootManifest;
    }
    for (const QString& starterName : {QString("droplet_target_nontarget_binary_starter"),
                                       QString("droplet_target_nontarget_3class_starter")}) {
        const QString starterManifest = dir.filePath(starterName + "/metadata/dataset_manifest.json");
        if (QFileInfo::exists(starterManifest)) {
            return starterManifest;
        }
    }
    return info.absoluteFilePath();
}

} // namespace

DatasetWorkspaceController::DatasetWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    loadTrainerSettings();
    wireDatasetActions();
    wireTrainerPathButtons();
    wireTrainerSettingsPersistence();
    refreshTrainerSummary();
}

void DatasetWorkspaceController::openDatasetLabelerPath(const QString& preferredPath) {
    QString initialDataset = deps_.trainerDatasetEdit ? deps_.trainerDatasetEdit->text().trimmed() : QString();
    if (!preferredPath.trimmed().isEmpty()) {
        initialDataset = preferredPath.trimmed();
    }
    initialDataset = normalizedDatasetSelectionPath(initialDataset);
    if (initialDataset.isEmpty() || !QFileInfo(initialDataset).exists()) {
        initialDataset = normalizedDatasetSelectionPath(deps_.defaultTrainerDataset);
    }
    if (!activeDatasetLabelerDialog_.isNull()) {
        activeDatasetLabelerDialog_->close();
    }
    auto* dialog = new DatasetLabelerDialog(deps_.window, QFileInfo(initialDataset).exists() ? initialDataset : QString(),
                                            deps_.defaultTrainerDataset);
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
        !deps_.trainerEpochsSpin || !deps_.trainerBatchSpin || !deps_.trainerLrSpin || !deps_.trainerFlipCheck ||
        !deps_.trainerRotationCheck || !deps_.trainerColorJitterCheck || !deps_.trainerRandomCropCheck ||
        !deps_.trainerSchedulerCombo) {
        return;
    }

    QSettings settings;
    settings.setValue("settings/pythonTrainer", deps_.trainerPythonEdit->text().trimmed());
    settings.setValue("settings/datasetsRoot", deps_.trainerDatasetEdit->text().trimmed());
    settings.setValue("trainer/outputDir", deps_.trainerOutputEdit->text().trimmed());
    settings.setValue("trainer/architecture",
                      deps_.trainerArchitectureCombo ? deps_.trainerArchitectureCombo->currentData().toString()
                                                     : QString("squeezenet1_1"));
    settings.setValue("trainer/pretrained",
                      deps_.trainerPretrainedImageNetBtn ? deps_.trainerPretrainedImageNetBtn->isChecked() : true);
    if (deps_.trainerStartingModelCombo) {
        settings.setValue("trainer/startingModelId", deps_.trainerStartingModelCombo->currentData().toString());
    }
    if (deps_.trainerTrainingModeCombo) {
        settings.setValue("trainer/trainingMode", trainerModeKey(deps_.trainerTrainingModeCombo));
    }
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
        deps_.trainerStartTrainingBtn->setText(busy && trainerCommandWasTraining ? "Training model..."
                                                                                : "Train model");
    }
    if (deps_.trainerDryRunBtn)
        deps_.trainerDryRunBtn->setEnabled(!busy);
    if (deps_.trainerCancelBtn)
        deps_.trainerCancelBtn->setEnabled(busy);
    if (deps_.trainerProgressBar) {
        deps_.trainerProgressBar->setRange(busy ? 0 : 0, busy ? 0 : 100);
        deps_.trainerProgressBar->setValue(0);
        deps_.trainerProgressBar->setFormat(busy ? "Working..." : "Not running");
    }
}

QString DatasetWorkspaceController::trainingConfigPath() const {
    if (!deps_.trainerOutputEdit || !deps_.trainerEpochsSpin || !deps_.trainerBatchSpin || !deps_.trainerLrSpin ||
        !deps_.trainerFlipCheck || !deps_.trainerRotationCheck || !deps_.trainerColorJitterCheck ||
        !deps_.trainerRandomCropCheck || !deps_.trainerSchedulerCombo) {
        return {};
    }

    populateTrainerModelOptions();

    const QString outputDir = deps_.trainerOutputEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        return {};
    }

    const QString selectedModelId =
        deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->currentData().toString().trimmed() : QString();
    const QString selectedMode = trainerModeKey(deps_.trainerTrainingModeCombo);
    if (deps_.trainerStartingModelCombo && selectedModelId.isEmpty()) {
        if (deps_.trainerStatusLabel) {
            deps_.trainerStatusLabel->setText(
                trainerSummaryText("Choose a starting model before continuing."));
        }
        return {};
    }

    TrainerModelOption selectedOption;
    QJsonArray registryEntries;
    if (!deps_.trainerRegistryFilePath.trimmed().isEmpty()) {
        registryEntries = loadJsonObjectFile(deps_.trainerRegistryFilePath).value("entries").toArray();
    }
    if (registryEntries.isEmpty() && deps_.trainerRegistryEntries) {
        registryEntries = *deps_.trainerRegistryEntries;
    }

    if (!registryEntries.isEmpty()) {
        for (const auto& value : registryEntries) {
            const QJsonObject entry = value.toObject();
            if (registryString(entry, "registry_entry_id").trimmed() != selectedModelId) {
                continue;
            }
            const QJsonObject metadataDoc = loadJsonObjectFile(readRegistryPath(entry, "metadata_path"));
            selectedOption.registryEntryId = selectedModelId;
            selectedOption.displayName = fallbackDisplayName(entry);
            selectedOption.modelPath = readRegistryPath(entry, "model_path");
            selectedOption.metadataPath = readRegistryPath(entry, "metadata_path");
            selectedOption.architectureId = architectureIdFromMetadata(metadataDoc);
            selectedOption.isStarter = metadataLooksLikeStarter(entry, metadataDoc);
            selectedOption.isLiveModel = entryIsLiveModel(entry);
            selectedOption.supportsContinue = QFileInfo(selectedOption.modelPath).isFile();
            selectedOption.defaultPretrained =
                metadataDoc.value("training_config").toObject().value("pretrained").toBool(selectedOption.isStarter);
            break;
        }
    }

    QString architecture = selectedOption.architectureId;
    if (architecture.isEmpty() && deps_.trainerArchitectureCombo) {
        architecture = deps_.trainerArchitectureCombo->currentData().toString().trimmed();
    }
    if (architecture.isEmpty()) {
        architecture = "squeezenet1_1";
    }

    bool pretrained = deps_.trainerPretrainedImageNetBtn ? deps_.trainerPretrainedImageNetBtn->isChecked() : true;
    if (!selectedOption.registryEntryId.isEmpty() && selectedMode == "new_copy" && selectedOption.isStarter) {
        pretrained = selectedOption.defaultPretrained;
    }
    if (selectedMode == "continue_copy") {
        pretrained = false;
    }

    QDir().mkpath(outputDir);
    QJsonObject config;
    config["schema_version"] = 1;
    config["architecture"] = architecture;
    config["batch_size"] = deps_.trainerBatchSpin->value();
    config["epochs"] = deps_.trainerEpochsSpin->value();
    config["pretrained"] = pretrained;

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

    QJsonObject outputBehavior;
    outputBehavior["mode"] = "new_run_folder";
    outputBehavior["preserve_source_model"] = true;
    outputBehavior["output_parent_dir"] = QDir(outputDir).absolutePath();
    config["output_behavior"] = outputBehavior;

    if (!selectedOption.registryEntryId.isEmpty()) {
        QJsonObject startingModel;
        startingModel["registry_entry_id"] = selectedOption.registryEntryId;
        startingModel["display_name"] = selectedOption.displayName;
        startingModel["model_path"] = selectedOption.modelPath;
        startingModel["metadata_path"] = selectedOption.metadataPath;
        startingModel["mode"] = selectedMode;
        startingModel["mode_label"] = trainerModeLabel(selectedMode);
        startingModel["architecture"] = architecture;
        startingModel["uses_selected_weights"] = selectedMode == "continue_copy";
        startingModel["preserve_original"] = true;
        config["starting_model"] = startingModel;
        if (selectedMode == "continue_copy" && !selectedOption.modelPath.isEmpty()) {
            config["source_model_path"] = selectedOption.modelPath;
        }
    }

    const QString path = QDir(outputDir).absoluteFilePath("trainer_gui_config.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (deps_.trainerStatusLabel) {
            deps_.trainerStatusLabel->setText(
                trainerSummaryText("Could not prepare the training setup.", file.errorString()));
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

QString DatasetWorkspaceController::trainerSummaryText(const QString& stateHeadline, const QString& stateDetail) const {
    populateTrainerModelOptions();
    refreshTrainerModeHint();

    QStringList issues;
    const bool ready = trainerSetupReady(&issues);
    const TrainerDatasetCounts counts = collectTrainerDatasetCounts();
    const QString selectedModel = deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->currentText().trimmed()
                                                                 : QString();
    const QString selectedMode = trainerModeLabel(trainerModeKey(deps_.trainerTrainingModeCombo));

    QString headline = stateHeadline.trimmed();
    if (headline.isEmpty()) {
        headline = ready ? "Setup ready. You can check setup or train the model."
                         : "Setup not ready yet.";
    }

    QStringList lines;
    lines << headline;

    if (counts.available) {
        lines << QString("Training image set: %1 total, %2 Target examples, %3 Non-target examples.")
                     .arg(counts.totalCount)
                     .arg(counts.hitCount)
                     .arg(counts.wasteCount);
    } else if (deps_.trainerDatasetEdit && !deps_.trainerDatasetEdit->text().trimmed().isEmpty()) {
        lines << "Training image set: selected, but counts are not available for this file yet.";
    } else {
        lines << "Training image set: choose an image set file with Target and Non-target examples.";
    }

    if (deps_.trainerOutputEdit && !deps_.trainerOutputEdit->text().trimmed().isEmpty()) {
        lines << "Save new model in: " + QDir::toNativeSeparators(deps_.trainerOutputEdit->text().trimmed());
    } else {
        lines << "Save new model in: choose a folder for the trained model.";
    }

    if (!selectedModel.isEmpty()) {
        lines << "Start from: " + selectedModel;
    } else {
        lines << "Start from: choose a starter or trained model from Model workspace.";
    }

    lines << "Training plan: " + selectedMode + ".";
    lines << "Original model stays unchanged. Training saves a new copy in the selected folder.";

    if (deps_.trainerStartingModelCombo && deps_.trainerStartingModelCombo->currentData().toString().trimmed().isEmpty()) {
        lines << "Starting model: no compatible model is available in Model workspace yet.";
    }

    const QString detail = stateDetail.trimmed();
    if (!detail.isEmpty()) {
        lines << detail;
    } else if (!ready && !issues.isEmpty()) {
        lines << "Still needed: " + issues.join(", ") + ".";
    }

    return lines.join("\n");
}

QString DatasetWorkspaceController::quoteTrainerArg(QString arg) {
    arg.replace("\"", "\\\"");
    return arg.contains(' ') ? "\"" + arg + "\"" : arg;
}

void DatasetWorkspaceController::refreshTrainerSummary() const {
    if (deps_.trainerStatusLabel) {
        deps_.trainerStatusLabel->setText(trainerSummaryText());
    }
}

bool DatasetWorkspaceController::trainerSetupReady(QStringList* issues) const {
    QStringList missing;

    populateTrainerModelOptions();

    if (!deps_.trainerPythonEdit || deps_.trainerPythonEdit->text().trimmed().isEmpty()) {
        missing << "choose Python setup";
    }

    if (deps_.trainerStartingModelCombo &&
        deps_.trainerStartingModelCombo->currentData().toString().trimmed().isEmpty()) {
        missing << "choose a starting model";
    }

    if (!deps_.trainerDatasetEdit || deps_.trainerDatasetEdit->text().trimmed().isEmpty()) {
        missing << "choose a training image set file";
    } else if (!QFileInfo(deps_.trainerDatasetEdit->text().trimmed()).exists()) {
        missing << "fix the training image set file path";
    }

    if (!deps_.trainerOutputEdit || deps_.trainerOutputEdit->text().trimmed().isEmpty()) {
        missing << "choose where to save the model";
    }

    if (issues) {
        *issues = missing;
    }
    return missing.isEmpty();
}

DatasetWorkspaceController::TrainerDatasetCounts DatasetWorkspaceController::collectTrainerDatasetCounts() const {
    TrainerDatasetCounts counts;
    if (!deps_.trainerDatasetEdit) {
        return counts;
    }

    const QString datasetPath = deps_.trainerDatasetEdit->text().trimmed();
    if (datasetPath.isEmpty()) {
        return counts;
    }

    auto countImagesRecursively = [](const QString& folderPath) {
        static const QSet<QString> suffixes = {"png", "jpg", "jpeg", "bmp", "tif", "tiff", "webp"};
        int total = 0;
        QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            if (suffixes.contains(info.suffix().toLower())) {
                ++total;
            }
        }
        return total;
    };

    auto countManifestRows = [&](const QString& manifestPath) {
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            return false;
        }

        const QJsonArray items = doc.object().value("items").toArray();
        int hit = 0;
        int waste = 0;
        for (const QJsonValue& value : items) {
            const QJsonObject item = value.toObject();
            QString label = item.value("reviewed_label").toString().trimmed().toLower();
            if (label.isEmpty()) {
                label = item.value("auto_label").toString().trimmed().toLower();
            }
            if (label == "1") {
                label = "hit";
            } else if (label == "0" || label == "empty") {
                label = "waste";
            }
            if (label == "hit") {
                ++hit;
            } else if (label == "waste") {
                ++waste;
            }
        }

        counts.hitCount = hit;
        counts.wasteCount = waste;
        counts.totalCount = hit + waste;
        counts.available = counts.totalCount >= 0;
        return true;
    };

    const QFileInfo datasetInfo(datasetPath);
    if (datasetInfo.isFile()) {
        countManifestRows(datasetInfo.absoluteFilePath());
        return counts;
    }

    const QDir datasetDir(datasetInfo.absoluteFilePath());
    const QString metadataManifest = datasetDir.filePath("metadata/dataset_manifest.json");
    const QString rootManifest = datasetDir.filePath("manifest.json");
    if (QFileInfo::exists(metadataManifest) && countManifestRows(metadataManifest)) {
        return counts;
    }
    if (QFileInfo::exists(rootManifest) && countManifestRows(rootManifest)) {
        return counts;
    }

    const QStringList hitFolders = {"hit", "hits"};
    const QStringList wasteFolders = {"waste", "empty"};
    int hit = 0;
    int waste = 0;
    for (const QString& split : {"train", "val", "test"}) {
        const QDir splitDir(datasetDir.filePath(split));
        if (!splitDir.exists()) {
            continue;
        }
        for (const QString& folder : hitFolders) {
            const QString path = splitDir.filePath(folder);
            if (QFileInfo::exists(path)) {
                hit += countImagesRecursively(path);
            }
        }
        for (const QString& folder : wasteFolders) {
            const QString path = splitDir.filePath(folder);
            if (QFileInfo::exists(path)) {
                waste += countImagesRecursively(path);
            }
        }
    }

    if (hit == 0 && waste == 0) {
        for (const QString& folder : hitFolders) {
            const QString path = datasetDir.filePath(folder);
            if (QFileInfo::exists(path)) {
                hit += countImagesRecursively(path);
            }
        }
        for (const QString& folder : wasteFolders) {
            const QString path = datasetDir.filePath(folder);
            if (QFileInfo::exists(path)) {
                waste += countImagesRecursively(path);
            }
        }
    }

    if (hit > 0 || waste > 0) {
        counts.hitCount = hit;
        counts.wasteCount = waste;
        counts.totalCount = hit + waste;
        counts.available = true;
    }

    return counts;
}

void DatasetWorkspaceController::loadTrainerSettings() const {
    if (!deps_.trainerPythonEdit || !deps_.trainerDatasetEdit || !deps_.trainerOutputEdit ||
        !deps_.trainerEpochsSpin || !deps_.trainerBatchSpin || !deps_.trainerLrSpin || !deps_.trainerFlipCheck ||
        !deps_.trainerRotationCheck || !deps_.trainerColorJitterCheck || !deps_.trainerRandomCropCheck ||
        !deps_.trainerSchedulerCombo) {
        return;
    }

    QSettings settings;
    deps_.trainerPythonEdit->setText(
        settings.value("settings/pythonTrainer", deps_.trainerPythonEdit->text()).toString());
    const QString defaultDatasetSelection =
        QDir::toNativeSeparators(normalizedDatasetSelectionPath(deps_.defaultTrainerDataset));
    const QString savedDatasetSelection = settings.value("settings/datasetsRoot", defaultDatasetSelection).toString();
    deps_.trainerDatasetEdit->setText(
        QDir::toNativeSeparators(normalizedDatasetSelectionPath(savedDatasetSelection)));
    deps_.trainerOutputEdit->setText(
        settings.value("trainer/outputDir", QDir::toNativeSeparators(deps_.defaultTrainerOutput)).toString());

    if (deps_.trainerArchitectureCombo) {
        const QString arch =
            settings.value("trainer/architecture", deps_.trainerArchitectureCombo->currentData().toString()).toString();
        const int archIndex = deps_.trainerArchitectureCombo->findData(arch);
        if (archIndex >= 0) {
            deps_.trainerArchitectureCombo->setCurrentIndex(archIndex);
        }
    }

    const bool pretrained = settings.value("trainer/pretrained", true).toBool();
    if (deps_.trainerPretrainedImageNetBtn) {
        deps_.trainerPretrainedImageNetBtn->setChecked(pretrained);
    }
    if (deps_.trainerPretrainedNoneBtn) {
        deps_.trainerPretrainedNoneBtn->setChecked(!pretrained);
    }
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

    populateTrainerModelOptions();
    if (deps_.trainerTrainingModeCombo) {
        const QString savedMode = settings.value("trainer/trainingMode", "new_copy").toString();
        const int modeIndex = deps_.trainerTrainingModeCombo->findData(savedMode);
        if (modeIndex >= 0) {
            deps_.trainerTrainingModeCombo->setCurrentIndex(modeIndex);
        }
    }
    refreshTrainerModeHint();
    refreshTrainerUi();
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
            const QString startPath = chooseOpenFileDialogPath(
                normalizedDatasetSelectionPath(deps_.trainerDatasetEdit->text()),
                normalizedDatasetSelectionPath(deps_.defaultTrainerDataset), findPackagedAppPath("datasets/prepared"));
            const QString file = QFileDialog::getOpenFileName(
                deps_.window, "Select training image set file", startPath,
                "Image set files (*.json);;All files (*.*)");
            if (!file.isEmpty()) {
                deps_.trainerDatasetEdit->setText(QDir::toNativeSeparators(file));
            }
        });
    }
    if (deps_.trainerOutputBrowseBtn && deps_.trainerOutputEdit) {
        connect(deps_.trainerOutputBrowseBtn, &QPushButton::clicked, this, [this]() {
            const QString startDir =
                chooseExistingDirectoryDialogPath(deps_.trainerOutputEdit->text(), deps_.defaultTrainerOutput);
            const QString dir =
                QFileDialog::getExistingDirectory(deps_.window, "Select training output directory", startDir);
            if (!dir.isEmpty()) {
                deps_.trainerOutputEdit->setText(QDir::toNativeSeparators(dir));
            }
        });
    }
}

void DatasetWorkspaceController::wireTrainerSettingsPersistence() {
    const auto save = [this]() {
        saveTrainerSettings();
        refreshTrainerModeHint();
        refreshTrainerSummary();
    };

    for (auto* edit : {deps_.trainerPythonEdit, deps_.trainerDatasetEdit, deps_.trainerOutputEdit}) {
        if (edit) {
            connect(edit, &QLineEdit::textChanged, this, save);
        }
    }
    if (deps_.trainerStartingModelCombo) {
        connect(deps_.trainerStartingModelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, save);
    }
    if (deps_.trainerTrainingModeCombo) {
        connect(deps_.trainerTrainingModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, save);
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

void DatasetWorkspaceController::populateTrainerModelOptions() const {
    if (!deps_.trainerStartingModelCombo) {
        return;
    }

    const QString currentId = deps_.trainerStartingModelCombo->currentData().toString().trimmed();
    QSettings settings;
    const QString savedId = settings.value("trainer/startingModelId").toString().trimmed();

    QVector<TrainerModelOption> options;
    QJsonArray registryEntries;
    if (!deps_.trainerRegistryFilePath.trimmed().isEmpty()) {
        registryEntries = loadJsonObjectFile(deps_.trainerRegistryFilePath).value("entries").toArray();
    }
    if (registryEntries.isEmpty() && deps_.trainerRegistryEntries) {
        registryEntries = *deps_.trainerRegistryEntries;
    }

    if (!registryEntries.isEmpty()) {
        for (const auto& value : registryEntries) {
            const QJsonObject entry = value.toObject();
            const QString modelPath = readRegistryPath(entry, "model_path");
            const QString metadataPath = readRegistryPath(entry, "metadata_path");
            if (!QFileInfo(modelPath).isFile() || !QFileInfo(metadataPath).isFile()) {
                continue;
            }

            const QJsonObject metadataDoc = loadJsonObjectFile(metadataPath);
            const QString architectureId = architectureIdFromMetadata(metadataDoc);
            if (architectureId != "squeezenet1_0" && architectureId != "squeezenet1_1") {
                continue;
            }

            TrainerModelOption option;
            option.registryEntryId = registryString(entry, "registry_entry_id").trimmed();
            option.displayName = fallbackDisplayName(entry);
            option.modelPath = modelPath;
            option.metadataPath = metadataPath;
            option.architectureId = architectureId;
            option.isStarter = metadataLooksLikeStarter(entry, metadataDoc);
            option.isLiveModel = entryIsLiveModel(entry);
            option.supportsContinue = true;
            option.defaultPretrained =
                metadataDoc.value("training_config").toObject().value("pretrained").toBool(option.isStarter);

            QStringList detailParts;
            detailParts << (option.isStarter ? "Starter" : "Trained");
            if (option.isLiveModel) {
                detailParts << "current live model";
            }
            const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label").trimmed();
            if (!targetDisplay.isEmpty()) {
                detailParts << ("target: " + targetDisplay);
            }
            option.detailText = detailParts.join("  |  ");
            options.push_back(option);
        }
    }

    std::stable_sort(options.begin(), options.end(), [](const TrainerModelOption& lhs, const TrainerModelOption& rhs) {
        const int lhsRank = lhs.isStarter ? 0 : (lhs.isLiveModel ? 2 : 1);
        const int rhsRank = rhs.isStarter ? 0 : (rhs.isLiveModel ? 2 : 1);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        return lhs.displayName.compare(rhs.displayName, Qt::CaseInsensitive) < 0;
    });

    const QSignalBlocker blocker(deps_.trainerStartingModelCombo);
    deps_.trainerStartingModelCombo->clear();
    for (const auto& option : options) {
        const QString label = option.detailText.isEmpty() ? option.displayName
                                                          : QString("%1  |  %2").arg(option.displayName, option.detailText);
        deps_.trainerStartingModelCombo->addItem(label, option.registryEntryId);
    }

    int selectedIndex = -1;
    const auto findIndexById = [&](const QString& id) {
        if (id.isEmpty()) {
            return -1;
        }
        return deps_.trainerStartingModelCombo->findData(id);
    };
    selectedIndex = findIndexById(currentId);
    if (selectedIndex < 0) {
        selectedIndex = findIndexById(savedId);
    }
    if (selectedIndex < 0) {
        for (int i = 0; i < options.size(); ++i) {
            if (options.at(i).isStarter && !options.at(i).isLiveModel) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0 && !options.isEmpty()) {
        selectedIndex = 0;
    }

    if (selectedIndex >= 0) {
        deps_.trainerStartingModelCombo->setCurrentIndex(selectedIndex);
    } else {
        deps_.trainerStartingModelCombo->addItem("No compatible training model found", QString());
        deps_.trainerStartingModelCombo->setCurrentIndex(0);
    }
}

void DatasetWorkspaceController::refreshTrainerModeHint() const {
    if (!deps_.trainerStartingModelHintLabel) {
        return;
    }

    const QString modelText = deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->currentText().trimmed()
                                                              : QString();
    const QString modelId = deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->currentData().toString().trimmed()
                                                            : QString();
    const QString mode = trainerModeKey(deps_.trainerTrainingModeCombo);

    if (modelId.isEmpty()) {
        deps_.trainerStartingModelHintLabel->setText(
            "Add or keep a SqueezeNet model in Model workspace before training from the Trainer workspace.");
        return;
    }

    if (mode == "continue_copy") {
        deps_.trainerStartingModelHintLabel->setText(
            QString("Loads weights from \"%1\" and keeps the original file unchanged. Training saves a new copy.")
                .arg(modelText));
        return;
    }

    deps_.trainerStartingModelHintLabel->setText(
        QString("Uses \"%1\" as the model choice for this run and saves a new trained copy without changing the original file.")
            .arg(modelText));
}

void DatasetWorkspaceController::refreshTrainerUi() const {
    populateTrainerModelOptions();
    refreshTrainerModeHint();
    refreshTrainerSummary();
}
