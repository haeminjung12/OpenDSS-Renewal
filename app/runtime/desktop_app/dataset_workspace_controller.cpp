#include "dataset_workspace_controller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QTextCursor>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>

#include "dataset_labeler_dialog.h"
#include "json_persistence.h"
#include "model_registry_service.h"

#include <algorithm>

namespace {

struct TrainerModelOption {
    QString registryEntryId;
    QString displayName;
    QString detailText;
    QString modelPath;
    QString metadataPath;
    QString checkpointPath;
    QString architectureId;
    QString pretrainedWeightId;
    QString pretrainedWeightSha256;
    bool isStarter = false;
    bool isLiveModel = false;
    bool supportsContinue = false;
    bool defaultPretrained = true;
};

QString sha256FileHex(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

QString officialWeightCachePath(const QString& architectureId) {
    const QString filename = architectureId == "mobilenet_v3_small"
                                 ? QStringLiteral("mobilenet_v3_small-047dcff4.pth")
                                 : architectureId == "efficientnet_b0"
                                       ? QStringLiteral("efficientnet_b0_rwightman-7f5810bc.pth")
                                       : QString();
    return filename.isEmpty() ? QString() : QDir::home().absoluteFilePath(".cache/torch/hub/checkpoints/" + filename);
}

QString readRegistryPath(const QJsonObject& entry, const QString& key) {
    const QString configured = registryString(entry, key);
    if (!configured.isEmpty())
        return resolvePackagedPathFromRegistryPath(configured);
    if (key == "checkpoint_path") {
        const QString metadata = resolvePackagedPathFromRegistryPath(registryString(entry, "metadata_path"));
        if (!metadata.isEmpty())
            return QFileInfo(metadata).dir().filePath("checkpoint.pth");
    }
    return {};
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
    const QString directId = metadataDoc.value("architecture").toObject().value("id").toString().trimmed();
    if (!directId.isEmpty())
        return directId;
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

QJsonObject datasetClassContract(const QString& selectedPath) {
    QString manifestPath = selectedPath;
    if (QFileInfo(selectedPath).isDir()) {
        const QDir dir(selectedPath);
        manifestPath = QFileInfo(dir.filePath("metadata/dataset_manifest.json")).isFile()
                           ? dir.filePath("metadata/dataset_manifest.json")
                           : dir.filePath("manifest.json");
    }
    const QJsonObject manifest = loadJsonObjectFile(manifestPath);
    QJsonObject schema = manifest.value("class_schema").toObject();
    QJsonArray rawClasses = schema.value("classes").toArray();
    if (rawClasses.isEmpty())
        rawClasses = manifest.value("classes").toArray();
    QJsonObject labels = schema.value("display_labels").toObject();
    if (labels.isEmpty()) labels = manifest.value("display_labels").toObject();
    if (labels.isEmpty()) labels = manifest.value("class_semantics").toObject();
    struct IndexedClass { int index; QString id; QString displayName; };
    QVector<IndexedClass> indexedClasses;
    for (int position = 0; position < rawClasses.size(); ++position) {
        const QJsonValue value = rawClasses.at(position);
        const QJsonObject object = value.toObject();
        const QString id = value.isObject() ? object.value("id").toVariant().toString().trimmed()
                                            : value.toVariant().toString().trimmed();
        if (id.isEmpty())
            continue;
        indexedClasses.push_back({value.isObject() ? object.value("index").toInt(position) : position,
                                  id, object.value("display_name").toString().trimmed()});
    }
    std::sort(indexedClasses.begin(), indexedClasses.end(), [](const IndexedClass& left, const IndexedClass& right) {
        return left.index < right.index || (left.index == right.index && left.id < right.id);
    });
    QJsonArray classes;
    for (const IndexedClass& entry : indexedClasses) {
        classes.append(entry.id);
        if (!entry.displayName.isEmpty())
            labels[entry.id] = entry.displayName;
    }
    if (classes.isEmpty()) {
        QSet<QString> observed;
        QJsonArray items = manifest.value("items").toArray();
        if (items.isEmpty())
            items = manifest.value("records").toArray();
        for (const QJsonValue& value : items) {
            const QJsonObject item = value.toObject();
            QString id = item.value("class_id").toVariant().toString().trimmed();
            if (id.isEmpty()) id = item.value("label").toVariant().toString().trimmed();
            if (!id.isEmpty()) observed.insert(id);
        }
        QStringList sorted = observed.values();
        std::sort(sorted.begin(), sorted.end());
        for (const QString& id : sorted) classes.append(id);
    }
    QJsonObject result;
    result["classes"] = classes;
    result["display_labels"] = labels;
    result["count"] = classes.size();
    return result;
}

QString architectureIdFromRegistryEntry(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    const QString metadataArchitecture = architectureIdFromMetadata(metadataDoc);
    if (!metadataArchitecture.isEmpty()) {
        return metadataArchitecture;
    }

    const QString registryArchitecture = registryString(entry, "architecture_id").trimmed();
    if (!registryArchitecture.isEmpty()) {
        return registryArchitecture;
    }

    const QString entryId = registryString(entry, "registry_entry_id");
    const QString modelPath = registryString(entry, "model_path");
    if (entryId.startsWith("blank_squeezenet_template_seed42", Qt::CaseInsensitive) ||
        entryId.startsWith("pre_binary_promotion_backup", Qt::CaseInsensitive) ||
        modelPath.contains("squeezenet", Qt::CaseInsensitive) ||
        modelPath.contains("pre_binary_promotion_backup", Qt::CaseInsensitive)) {
        return "squeezenet1_1";
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
    return entry.value("active").toBool(false) || entry.value("selectable_for_normal_live_sorting").toBool(false) ||
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
    return QString();
}

QString trainerDatasetBrowseStartPath(const DatasetWorkspaceController::Dependencies& deps) {
    Q_UNUSED(deps);
    return defaultOpenDssDatasetsPath();
}

QString validateHyperparameterConfig(const QJsonObject& config) {
    if (config.value("schema_version").toInt() != 2)
        return "Hyperparameter JSON must use schema_version 2.";
    const int batchSize = config.value("batch_size").toInt();
    if (batchSize < 1 || batchSize > 256)
        return "Batch size must be between 1 and 256.";
    if (config.value("patience").toInt() < 1)
        return "Early-stopping patience must be positive.";
    if (config.value("weight_decay").toDouble(-1.0) < 0.0)
        return "Weight decay must be zero or positive.";
    const QJsonArray inputSize = config.value("input_size").toArray();
    if (inputSize != QJsonArray{96, 96, 3})
        return "Input size must remain [96, 96, 3] for the packaged models.";
    const QJsonArray stages = config.value("stages").toArray();
    if (stages.size() != 2)
        return "Exactly two staged-training entries are required.";
    int totalEpochs = 0;
    for (const QJsonValue& value : stages) {
        const QJsonObject stage = value.toObject();
        if (stage.value("name").toString().trimmed().isEmpty() || stage.value("epochs").toInt() < 1 ||
            stage.value("learning_rate").toDouble() <= 0.0) {
            return "Each training stage needs a name, positive epoch budget, and positive learning rate.";
        }
        totalEpochs += stage.value("epochs").toInt();
    }
    if (totalEpochs < 2 || totalEpochs > 1000)
        return "The combined staged epoch budget is invalid.";
    const QString imbalanceMode = config.value("imbalance").toObject().value("mode").toString();
    const QSet<QString> supportedImbalance = {"none", "class_weighted_loss", "effective_number",
                                               "balanced_sampler", "balanced_sampler_effective_number", "focal_loss"};
    if (!supportedImbalance.contains(imbalanceMode))
        return "The selected imbalance strategy is not supported.";
    const double samplerAlpha = config.value("imbalance").toObject().value("sampler_alpha").toDouble(1.0);
    if (samplerAlpha < 0.0 || samplerAlpha > 1.0)
        return "Sampler alpha must be between 0 and 1.";
    return {};
}

} // namespace

DatasetWorkspaceController::DatasetWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    loadTrainerSettings();
    wireDatasetActions();
    wireTrainerPathButtons();
    wireTrainerSettingsPersistence();
    wireTrainerModelRenameAction();
    refreshTrainerSummary();
    const QString verifyTrainerRename = qEnvironmentVariable("OVDS_VERIFY_TRAINER_MODEL_RENAME").trimmed();
    if (!verifyTrainerRename.isEmpty() && verifyTrainerRename != "0" &&
        verifyTrainerRename.compare("false", Qt::CaseInsensitive) != 0) {
        QTimer::singleShot(0, this, [this]() { runTrainerModelRenameVerifier(); });
    }
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
        !deps_.trainerHyperparameterJsonEdit) {
        return;
    }

    QSettings settings;
    settings.setValue("settings/pythonTrainer", deps_.trainerPythonEdit->text().trimmed());
    settings.setValue("settings/datasetsRoot", deps_.trainerDatasetEdit->text().trimmed());
    settings.setValue("trainer/outputDir", deps_.trainerOutputEdit->text().trimmed());
    if (deps_.trainerStartingModelCombo) {
        settings.setValue("trainer/startingModelId", deps_.trainerStartingModelCombo->currentData().toString());
    }
    if (deps_.trainerTrainingModeCombo) {
        settings.setValue("trainer/trainingMode", trainerModeKey(deps_.trainerTrainingModeCombo));
    }
    settings.setValue("trainer/hyperparametersJson", deps_.trainerHyperparameterJsonEdit->toPlainText());
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
    if (!deps_.trainerOutputEdit || !deps_.trainerHyperparameterJsonEdit) {
        return {};
    }

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
            selectedOption.checkpointPath = readRegistryPath(entry, "checkpoint_path");
            selectedOption.architectureId = architectureIdFromRegistryEntry(entry, metadataDoc);
            selectedOption.isStarter = metadataLooksLikeStarter(entry, metadataDoc);
            selectedOption.isLiveModel = entryIsLiveModel(entry);
            selectedOption.supportsContinue = QFileInfo(selectedOption.checkpointPath).isFile();
            selectedOption.defaultPretrained =
                metadataDoc.value("training_config").toObject().value("pretrained").toBool(selectedOption.isStarter);
            const QJsonObject initialization = metadataDoc.value("initialization").toObject();
            selectedOption.pretrainedWeightId = initialization.value("weight_id").toString().trimmed();
            selectedOption.pretrainedWeightSha256 = initialization.value("source_checkpoint_sha256").toString().trimmed();
            break;
        }
    }

    const QString architecture = selectedOption.architectureId;
    if (architecture != "mobilenet_v3_small" && architecture != "efficientnet_b0") {
        if (deps_.trainerStatusLabel)
            deps_.trainerStatusLabel->setText(trainerSummaryText(
                "Could not prepare the training setup.", "The selected model architecture is not supported for new training."));
        return {};
    }

    const bool usesSelectedWeights = !selectedOption.isStarter && selectedOption.supportsContinue;

    QJsonParseError parseError;
    const QJsonDocument parsedConfig =
        QJsonDocument::fromJson(deps_.trainerHyperparameterJsonEdit->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsedConfig.isObject()) {
        if (deps_.trainerStatusLabel)
            deps_.trainerStatusLabel->setText(trainerSummaryText(
                "Could not prepare the training setup.", "Hyperparameter Settings contains malformed JSON."));
        return {};
    }
    QJsonObject config = parsedConfig.object();
    const QString configError = validateHyperparameterConfig(config);
    if (!configError.isEmpty()) {
        if (deps_.trainerStatusLabel)
            deps_.trainerStatusLabel->setText(trainerSummaryText("Could not prepare the training setup.", configError));
        return {};
    }
    QDir().mkpath(outputDir);
    int totalEpochs = 0;
    for (const QJsonValue& value : config.value("stages").toArray())
        totalEpochs += value.toObject().value("epochs").toInt();
    config["epochs"] = totalEpochs;
    config["architecture"] = architecture;
    for (const QString& ambiguousKey : {QStringLiteral("pretrained"), QStringLiteral("source_checkpoint"),
                                        QStringLiteral("source_checkpoint_path"), QStringLiteral("source_model_path"),
                                        QStringLiteral("pretrained_weight_id"), QStringLiteral("pretrained_weight_path"),
                                        QStringLiteral("pretrained_weight_sha256"),
                                        QStringLiteral("classifier_initialization")}) {
        config.remove(ambiguousKey);
    }
    config["device_request"] = QSettings().value("settings/computeDevice", "auto").toString();
    const QJsonObject classContract = datasetClassContract(deps_.trainerDatasetEdit->text().trimmed());
    const int datasetClassCount = classContract.value("count").toInt();
    if (datasetClassCount != 2 && datasetClassCount != 3) {
        if (deps_.trainerStatusLabel)
            deps_.trainerStatusLabel->setText(trainerSummaryText("Could not prepare the training setup.",
                "The selected dataset metadata must define exactly 2 or 3 classes."));
        return {};
    }
    const int sourceClassCount = loadJsonObjectFile(selectedOption.metadataPath)
                                     .value("architecture").toObject().value("num_classes").toInt();
    if (usesSelectedWeights && sourceClassCount != datasetClassCount) {
        if (deps_.trainerStatusLabel)
            deps_.trainerStatusLabel->setText(trainerSummaryText("Could not prepare the training setup.",
                QString("This model has %1 outputs but the dataset defines %2 classes. Choose a matching model or an ImageNet starter.")
                    .arg(sourceClassCount).arg(datasetClassCount)));
        return {};
    }
    config["classes"] = classContract.value("classes").toArray();
    config["display_labels"] = classContract.value("display_labels").toObject();

    QJsonObject initialization;
    if (!usesSelectedWeights) {
        const QString weightPath = QDir(QFileInfo(selectedOption.metadataPath).absolutePath())
                                       .filePath("imagenet_weights.pth");
        if (!QFileInfo(weightPath).isFile()) {
            if (deps_.trainerStatusLabel) {
                deps_.trainerStatusLabel->setText(trainerSummaryText(
                    "Could not prepare the training setup.",
                    "The selected Blank model package is missing imagenet_weights.pth. Repair or reinstall OpenDSS."));
            }
            return {};
        }
        initialization["mode"] = "imagenet";
        initialization["weight_id"] = selectedOption.pretrainedWeightId;
        initialization["weight_path"] = QFileInfo(weightPath).absoluteFilePath();
    } else {
        initialization["mode"] = "checkpoint";
        initialization["checkpoint_path"] = QFileInfo(selectedOption.checkpointPath).absoluteFilePath();
    }
    config["initialization"] = initialization;

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
        startingModel["uses_selected_weights"] = usesSelectedWeights;
        startingModel["preserve_original"] = true;
        config["starting_model"] = startingModel;
    }

    const QString path = QDir(outputDir).absoluteFilePath("trainer_gui_config.json");
    QString writeError;
    if (!desktop_app::writeJsonObjectAtomically(path, config, &writeError)) {
        if (deps_.trainerStatusLabel) {
            deps_.trainerStatusLabel->setText(
                trainerSummaryText("Could not prepare the training setup.", writeError));
        }
        return {};
    }
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
        args << "--smoke";
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
    QString headline = stateHeadline.trimmed();
    if (headline.isEmpty()) {
        headline = ready ? "Setup ready. You can check setup or train the model."
                         : "Setup not ready yet.";
    }

    QStringList lines;
    lines << headline;

    const QString detail = stateDetail.trimmed();
    if (!detail.isEmpty()) {
        lines << detail;
    } else if (!ready && !issues.isEmpty()) {
        lines << "Still needed: " + issues.join(", ") + ".";
    }

    if (deps_.trainerDatasetEdit && !deps_.trainerDatasetEdit->text().trimmed().isEmpty()) {
        lines << "Selected dataset: " + QDir::toNativeSeparators(deps_.trainerDatasetEdit->text().trimmed());
        const TrainerDatasetCounts counts = collectTrainerDatasetCounts();
        if (counts.available && counts.totalCount > 0) {
            lines << QString("Class counts: Empty %1 | Single %2 | MoreThanOne %3 | Total %4")
                         .arg(counts.class0Count).arg(counts.class1Count).arg(counts.class2Count).arg(counts.totalCount);
            const int minimumClass = std::min({counts.class0Count, counts.class1Count, counts.class2Count});
            if (minimumClass < 50 || static_cast<double>(minimumClass) / counts.totalCount < 0.02) {
                lines << "WARNING: Severe class imbalance detected. Verify the dataset identity before launching training. Accepted combined manifest: C:\\Users\\goals\\Documents\\OpenDSS\\runs\\publication-experiments\\organoid_wellplate_combined\\prep\\derived_manifest.json";
            }
        }
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
        missing << "choose a training dataset file";
    } else if (!QFileInfo(deps_.trainerDatasetEdit->text().trimmed()).exists()) {
        missing << "fix the training dataset file path";
    }

    if (!deps_.trainerOutputEdit || deps_.trainerOutputEdit->text().trimmed().isEmpty()) {
        missing << "choose where to save the model";
    }

    if (!deps_.trainerHyperparameterJsonEdit) {
        missing << "load Hyperparameter Settings";
    } else {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(deps_.trainerHyperparameterJsonEdit->toPlainText().toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            missing << "fix malformed Hyperparameter Settings JSON";
        } else {
            const QString validationError = validateHyperparameterConfig(document.object());
            if (!validationError.isEmpty())
                missing << validationError;
        }
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

        QJsonArray items = doc.object().value("items").toArray();
        if (items.isEmpty())
            items = doc.object().value("records").toArray();
        int class0 = 0;
        int class1 = 0;
        int class2 = 0;
        for (const QJsonValue& value : items) {
            const QJsonObject item = value.toObject();
            QString label = item.value("reviewed_label").toString().trimmed().toLower();
            if (label.isEmpty()) {
                label = item.value("auto_label").toString().trimmed().toLower();
            }
            if (label.isEmpty())
                label = item.value("class_id").toString().trimmed().toLower();
            if (label.isEmpty())
                label = item.value("label").toString().trimmed().toLower();
            if (label == "0" || label == "empty" || label == "non-target a")
                ++class0;
            else if (label == "1" || label == "single" || label == "target" || label == "hit")
                ++class1;
            else if (label == "2" || label == "morethanone" || label == "non-target b" || label == "multiple")
                ++class2;
        }

        counts.class0Count = class0;
        counts.class1Count = class1;
        counts.class2Count = class2;
        counts.totalCount = class0 + class1 + class2;
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
        counts.class0Count = waste;
        counts.class1Count = hit;
        counts.class2Count = 0;
        counts.totalCount = hit + waste;
        counts.available = true;
    }

    return counts;
}

void DatasetWorkspaceController::loadTrainerSettings() const {
    if (!deps_.trainerPythonEdit || !deps_.trainerDatasetEdit || !deps_.trainerOutputEdit ||
        !deps_.trainerHyperparameterJsonEdit) {
        return;
    }

    QSettings settings;
    deps_.trainerPythonEdit->setText(
        settings.value("settings/pythonTrainer", deps_.trainerPythonEdit->text()).toString());
    const QString defaultDatasetSelection =
        QDir::toNativeSeparators(normalizedDatasetSelectionPath(deps_.defaultTrainerDataset));
    const bool verifyingDefaultPaths = qEnvironmentVariableIntValue("OVDS_VERIFY_DEFAULT_PATHS") != 0;
    QString savedDatasetSelection = verifyingDefaultPaths
                                        ? defaultDatasetSelection
                                        : settings.value("settings/datasetsRoot", defaultDatasetSelection).toString();
    if (isDeveloperInternalDefaultPath(savedDatasetSelection)) {
        savedDatasetSelection = defaultDatasetSelection;
    }
    deps_.trainerDatasetEdit->setText(
        QDir::toNativeSeparators(normalizedDatasetSelectionPath(savedDatasetSelection)));
    QString trainerOutput = verifyingDefaultPaths
                                ? QDir::toNativeSeparators(deps_.defaultTrainerOutput)
                                : settings.value("trainer/outputDir", QDir::toNativeSeparators(deps_.defaultTrainerOutput))
                                      .toString();
    if (isDeveloperInternalDefaultPath(trainerOutput)) {
        trainerOutput = QDir::toNativeSeparators(deps_.defaultTrainerOutput);
    }
    deps_.trainerOutputEdit->setText(trainerOutput);
    const QString savedHyperparameters = settings.value("trainer/hyperparametersJson").toString().trimmed();
    if (!savedHyperparameters.isEmpty())
        deps_.trainerHyperparameterJsonEdit->setPlainText(savedHyperparameters);

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
            const QString startPath = trainerDatasetBrowseStartPath(deps_);
            const QString file = QFileDialog::getOpenFileName(
                deps_.window, "Select training dataset file", startPath,
                "Dataset files (*.json);;All files (*.*)");
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
    if (deps_.trainerHyperparameterJsonEdit)
        connect(deps_.trainerHyperparameterJsonEdit, &QPlainTextEdit::textChanged, this, save);
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

void DatasetWorkspaceController::wireTrainerModelRenameAction() {
    if (!deps_.trainerStartingModelCombo) {
        return;
    }

    deps_.trainerStartingModelCombo->setContextMenuPolicy(Qt::CustomContextMenu);
    deps_.trainerStartingModelCombo->setToolTip("Choose a starting model. Right-click to rename the selected model.");
    connect(deps_.trainerStartingModelCombo, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(deps_.trainerStartingModelCombo);
        QAction* renameAction = menu.addAction("Rename selected model");
        renameAction->setEnabled(!deps_.trainerStartingModelCombo->currentData().toString().trimmed().isEmpty());
        connect(renameAction, &QAction::triggered, this, [this]() {
            if (!renameSelectedStartingModel()) {
                return;
            }
            refreshTrainerUi();
        });
        menu.exec(deps_.trainerStartingModelCombo->mapToGlobal(pos));
    });
}

bool DatasetWorkspaceController::renameSelectedStartingModel() const {
    if (!deps_.trainerStartingModelCombo) {
        return false;
    }

    const QString selectedId = deps_.trainerStartingModelCombo->currentData().toString().trimmed();
    if (selectedId.isEmpty()) {
        QMessageBox::information(deps_.window, "Rename model", "Choose a starting model before renaming it.");
        return false;
    }

    QJsonArray registryEntries;
    if (!deps_.trainerRegistryFilePath.trimmed().isEmpty()) {
        registryEntries = loadJsonObjectFile(deps_.trainerRegistryFilePath).value("entries").toArray();
    }
    if (registryEntries.isEmpty() && deps_.trainerRegistryEntries) {
        registryEntries = *deps_.trainerRegistryEntries;
    }

    QString currentName = deps_.trainerStartingModelCombo->currentText().trimmed();
    for (const auto& value : registryEntries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(selectedId, Qt::CaseInsensitive) == 0) {
            currentName = fallbackDisplayName(entry);
            break;
        }
    }

    bool ok = false;
    const QString newName =
        QInputDialog::getText(deps_.window, "Rename model", "Model name:", QLineEdit::Normal, currentName, &ok).trimmed();
    if (!ok) {
        return false;
    }
    if (newName.isEmpty()) {
        QMessageBox::warning(deps_.window, "Rename model", "Model name cannot be empty.");
        return false;
    }

    QString error;
    if (!renameRegistryEntryDisplayName(deps_.trainerRegistryFilePath, selectedId, newName, &error)) {
        QMessageBox::warning(deps_.window, "Rename model", error);
        return false;
    }

    const QSignalBlocker blocker(deps_.trainerStartingModelCombo);
    populateTrainerModelOptions();
    const int row = deps_.trainerStartingModelCombo->findData(selectedId);
    if (row >= 0) {
        deps_.trainerStartingModelCombo->setCurrentIndex(row);
    }
    saveTrainerSettings();
    return true;
}

void DatasetWorkspaceController::runTrainerModelRenameVerifier() const {
    QStringList failures;
    auto require = [&](bool condition, const QString& message) {
        if (!condition) {
            failures << message;
        }
    };

    populateTrainerModelOptions();
    const QString selectedId =
        deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->currentData().toString().trimmed() : QString();
    require(deps_.trainerStartingModelCombo != nullptr, "Trainer starting-model combo exists");
    require(!selectedId.isEmpty(), "Trainer starting-model combo has a selected registry id");
    require(!deps_.trainerRegistryFilePath.trimmed().isEmpty(), "Trainer registry file path is available");

    QJsonObject beforeEntry;
    QJsonArray beforeEntries;
    if (!deps_.trainerRegistryFilePath.trimmed().isEmpty()) {
        beforeEntries = loadJsonObjectFile(deps_.trainerRegistryFilePath).value("entries").toArray();
    }
    for (const auto& value : beforeEntries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(selectedId, Qt::CaseInsensitive) == 0) {
            beforeEntry = entry;
            break;
        }
    }
    require(!beforeEntry.isEmpty(), "Selected trainer model exists in the registry before rename");

    const QString renamed = QString("Trainer verifier renamed model %1").arg(QCoreApplication::applicationPid());
    QString error;
    if (!selectedId.isEmpty()) {
        require(renameRegistryEntryDisplayName(deps_.trainerRegistryFilePath, selectedId, renamed, &error),
                QString("Trainer rename writes registry display_name: %1").arg(error));
    }

    refreshTrainerUi();
    const int renamedIndex = deps_.trainerStartingModelCombo ? deps_.trainerStartingModelCombo->findData(selectedId) : -1;
    require(renamedIndex >= 0, "Trainer rename keeps the selected model in the starting-model list");
    if (renamedIndex >= 0 && deps_.trainerStartingModelCombo) {
        deps_.trainerStartingModelCombo->setCurrentIndex(renamedIndex);
        require(deps_.trainerStartingModelCombo->currentText().contains(renamed),
                "Trainer starting-model list refreshes to show the renamed display name");
    }

    QJsonObject afterEntry;
    const QJsonArray afterEntries = loadJsonObjectFile(deps_.trainerRegistryFilePath).value("entries").toArray();
    for (const auto& value : afterEntries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(selectedId, Qt::CaseInsensitive) == 0) {
            afterEntry = entry;
            break;
        }
    }
    require(registryString(afterEntry, "display_name") == renamed,
            "Trainer rename persists display_name in the registry");
    require(registryString(afterEntry, "model_path") == registryString(beforeEntry, "model_path"),
            "Trainer rename leaves model_path unchanged");
    require(registryString(afterEntry, "metadata_path") == registryString(beforeEntry, "metadata_path"),
            "Trainer rename leaves metadata_path unchanged");

    if (failures.isEmpty()) {
        qInfo().noquote() << "Trainer model rename verifier passed.";
        QCoreApplication::exit(0);
        return;
    }
    qWarning().noquote() << "Trainer model rename verifier failed:" << failures.join("; ");
    QCoreApplication::exit(2);
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
            const QString architectureId = architectureIdFromRegistryEntry(entry, metadataDoc);
            if (architectureId != "mobilenet_v3_small" && architectureId != "efficientnet_b0") {
                continue;
            }

            TrainerModelOption option;
            option.registryEntryId = registryString(entry, "registry_entry_id").trimmed();
            option.displayName = fallbackDisplayName(entry);
            option.modelPath = modelPath;
            option.metadataPath = metadataPath;
            option.checkpointPath = readRegistryPath(entry, "checkpoint_path");
            option.architectureId = architectureId;
            option.isStarter = metadataLooksLikeStarter(entry, metadataDoc);
            if (!option.isStarter && !QFileInfo(option.checkpointPath).isFile()) {
                continue;
            }
            option.isLiveModel = entryIsLiveModel(entry);
            option.supportsContinue = option.isStarter || QFileInfo(option.checkpointPath).isFile();
            option.defaultPretrained =
                metadataDoc.value("training_config").toObject().value("pretrained").toBool(option.isStarter);

            QStringList detailParts;
            detailParts << (option.isStarter ? "ImageNet weights" : "Previously trained weights");
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
        deps_.trainerStartingModelCombo->addItem(option.displayName, option.registryEntryId);
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
        if (deps_.trainerSelectedArchitectureValue) {
            const QString architecture = options.at(selectedIndex).architectureId;
            deps_.trainerSelectedArchitectureValue->setText(
                architecture == "mobilenet_v3_small" ? "MobileNetV3-Small" : "EfficientNet-B0");
        }
    } else {
        deps_.trainerStartingModelCombo->addItem("No compatible training model found", QString());
        deps_.trainerStartingModelCombo->setCurrentIndex(0);
        if (deps_.trainerSelectedArchitectureValue)
            deps_.trainerSelectedArchitectureValue->setText("No supported model selected");
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
            "Add a MobileNetV3-Small or EfficientNet-B0 model in Library before training.");
        return;
    }

    if (mode == "continue_copy") {
        deps_.trainerStartingModelHintLabel->setText(
            QString("Loads weights from \"%1\" and keeps the original file unchanged. Training saves a new copy.")
                .arg(modelText));
        return;
    }

    deps_.trainerStartingModelHintLabel->setText(
        QString("Uses ImageNet weights for \"%1\" and saves a new trained copy.").arg(modelText));
}

void DatasetWorkspaceController::refreshTrainerUi() const {
    populateTrainerModelOptions();
    refreshTrainerModeHint();
    refreshTrainerSummary();
}
