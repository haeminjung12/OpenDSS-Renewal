#include "main_window.h"
#include "trainer_plot_math.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QtWidgets>
#include <QtCore>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QScrollArea>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStandardPaths>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <functional>
#include <atomic>
#include <exception>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <utility>
#include <chrono>
#include <cmath>
#include <string>
#include <opencv2/core.hpp>
#include "app_state.h"
#include "crash_handler.h"
#include "icons.h"
#include "theme.h"
#include "widget_helpers.h"
#include "workspace_camera.h"
#include "workspace_model.h"
#include "workspace_dataset.h"
#include "workspace_validator.h"
#include "workspace_reports.h"
#include "workspace_settings.h"
#include "pipeline_runner.h"
#include "object_names.h"
#include "app_options.h"
#include "app_paths.h"
#include "app_context.h"
#include "app_types.h"
#include "app_utils.h"
#include "collection_postprocessor.h"
#include "json_persistence.h"
#include "live_data_collection_writer.h"
#include "live_frame_dispatcher.h"
#include "live_log_writer.h"
#include "model_registry_service.h"
#include "sequence_summary_writer.h"
#include "camera_workspace_controller.h"
#include "dataset_workspace_controller.h"
#include "reports_workspace_controller.h"
#include "settings_workspace_controller.h"
#include "validator_workspace_controller.h"
#include "frame_types.h"
#include "background_task_registry.h"
#include "camera_worker.h"
#include "dataset_labeler_dialog.h"
#include "image_validation_dialog.h"
#include "stats_figure_window.h"
#include "viewer_window.h"
#include "zoom_image_view.h"
#include "../dataset_capture_session.h"

namespace {

class MainWindowCloseFilter : public QObject {
  public:
    explicit MainWindowCloseFilter(QObject* parent = nullptr) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Close) {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (!widget || widget == watched || !widget->isVisible())
                    continue;
                if (qobject_cast<QDialog*>(widget)) {
                    widget->close();
                }
            }
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
        return QObject::eventFilter(watched, event);
    }
};

class HeaderChipClickFilter : public QObject {
  public:
    HeaderChipClickFilter(std::function<void()> onClick, QObject* parent)
        : QObject(parent), onClick_(std::move(onClick)) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && onClick_) {
                onClick_();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    std::function<void()> onClick_;
};

class WheelEventForwarder : public QObject {
  public:
    explicit WheelEventForwarder(QWidget* target, QObject* parent = nullptr) : QObject(parent), target_(target) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Wheel && target_ && target_->isVisible()) {
            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            const QPointF targetPos = target_->mapFromGlobal(wheelEvent->globalPosition().toPoint());
            QWheelEvent forwardedEvent(targetPos, wheelEvent->globalPosition(), wheelEvent->pixelDelta(),
                                       wheelEvent->angleDelta(), wheelEvent->buttons(), wheelEvent->modifiers(),
                                       wheelEvent->phase(), wheelEvent->inverted(), wheelEvent->source(),
                                       wheelEvent->pointingDevice());
            QCoreApplication::sendEvent(target_, &forwardedEvent);
            if (forwardedEvent.isAccepted()) {
                event->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    QWidget* target_ = nullptr;
};

struct CollectionSaveDialogUi {
    QDialog* dialog = nullptr;
    QLineEdit* nameEdit = nullptr;
    QCheckBox* createMetadataCheck = nullptr;
};

CollectionSaveDialogUi buildCollectionSaveDialog(QWidget* parent, const QString& defaultName) {
    CollectionSaveDialogUi ui;
    ui.dialog = new QDialog(parent);
    ui.dialog->setWindowTitle("Save Dataset As");
    nameWidget(ui.dialog, "SaveDatasetAsDialog");

    auto* layout = new QVBoxLayout(ui.dialog);
    auto* prompt = new QLabel("Choose a name for the saved collection and optional training dataset.");
    prompt->setWordWrap(true);
    layout->addWidget(prompt);

    auto* form = new QFormLayout;
    ui.nameEdit = new QLineEdit(defaultName);
    nameWidget(ui.nameEdit, "CollectionNameEdit");
    form->addRow("Name", ui.nameEdit);
    layout->addLayout(form);

    ui.createMetadataCheck = new QCheckBox("Create Metadata for Training");
    ui.createMetadataCheck->setChecked(true);
    nameWidget(ui.createMetadataCheck, "CreateTrainingMetadataCheckBox");
    layout->addWidget(ui.createMetadataCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    nameWidget(buttons, "SaveDatasetAsButtons");
    QObject::connect(buttons, &QDialogButtonBox::accepted, ui.dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, ui.dialog, &QDialog::reject);
    layout->addWidget(buttons);

    return ui;
}

constexpr int kRuntimeSettingsSchemaVersion = 1;
constexpr const char* kRuntimeSettingsSchemaVersionKey = "runtime/v1/schemaVersion";

struct TrainerCompletionArtifacts {
    bool complete = false;
    QString runDir;
    QString modelOnnxPath;
    QString metadataJsonPath;
    QString metricsCsvPath;
    QString metricsJsonPath;
    QString trainingConfigJsonPath;
    QString classMetricsCsvPath;
    QString confusionMatrixCsvPath;
};

QString localAppDataRootForTrainer() {
    QString root = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (!root.isEmpty())
        return QFileInfo(root).absoluteFilePath();
#ifdef _WIN32
    const QString home = QDir::homePath();
    if (!home.isEmpty())
        return QFileInfo(QDir(home).filePath("AppData/Local")).absoluteFilePath();
#endif
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

QString documentedTrainerPythonExecutable(const QString& venvName) {
    const QString root = localAppDataRootForTrainer();
    if (root.isEmpty())
        return QStringLiteral("python");
    return QDir(root).absoluteFilePath(QStringLiteral("OpenDSS/%1/Scripts/python.exe").arg(venvName));
}

QString legacyTrainerPythonExecutable(const QString& venvName) {
    const QString root = localAppDataRootForTrainer();
    if (root.isEmpty())
        return QString();
    return QDir(root).absoluteFilePath(
        QStringLiteral("OpenVisualDropletSorter/%1/Scripts/python.exe").arg(venvName));
}

bool sameCleanPath(const QString& left, const QString& right) {
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath())
               .compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()), Qt::CaseInsensitive) == 0;
}

QString resolvedTrainerPythonExecutable(const QString& savedValue, const QString& computeDevice) {
    const QString saved = savedValue.trimmed();
    const QString cpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv"));
    const QString gpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv-gpu"));
    const QString legacyCpuPython = legacyTrainerPythonExecutable(QStringLiteral("training-venv"));
    const QString legacyGpuPython = legacyTrainerPythonExecutable(QStringLiteral("training-venv-gpu"));
    const bool cpuExists = QFileInfo(cpuPython).isFile();
    const bool gpuExists = QFileInfo(gpuPython).isFile();
    const bool legacyCpuExists = QFileInfo(legacyCpuPython).isFile();
    const bool legacyGpuExists = QFileInfo(legacyGpuPython).isFile();
    const QString normalizedDevice = computeDevice.trimmed().toLower();
    const bool wantsGpu = normalizedDevice == QLatin1String("cuda");
    const QString preferred = wantsGpu
                                  ? (gpuExists ? gpuPython : (legacyGpuExists ? legacyGpuPython : gpuPython))
                                  : (cpuExists ? cpuPython : (legacyCpuExists ? legacyCpuPython : cpuPython));

    if (QFileInfo(saved).isFile()) {
        const bool savedIsKnownCpu = sameCleanPath(saved, cpuPython) || sameCleanPath(saved, legacyCpuPython);
        const bool savedIsKnownGpu = sameCleanPath(saved, gpuPython) || sameCleanPath(saved, legacyGpuPython);
        // Auto accepts either validated environment. Only an explicit device
        // selection may replace a known environment with its matching peer.
        if ((savedIsKnownCpu || savedIsKnownGpu) &&
            ((normalizedDevice == QLatin1String("cpu") && savedIsKnownGpu) ||
             (normalizedDevice == QLatin1String("cuda") && savedIsKnownCpu)))
            return preferred;
        return saved;
    }
    if (saved.isEmpty() || saved.compare(QStringLiteral("python"), Qt::CaseInsensitive) == 0)
        return preferred;
    if (sameCleanPath(saved, gpuPython) && normalizedDevice != QLatin1String("cuda") && cpuExists)
        return cpuPython;
    if (normalizedDevice == QLatin1String("cuda") && gpuExists)
        return gpuPython;
    // Old installs may still point at the previous local-app-data trainer path.
    if (cpuExists || legacyCpuExists || saved.contains(QStringLiteral("OpenVisualDropletSorter"), Qt::CaseInsensitive))
        return preferred;
    // Never allow a stale persisted executable to reach QProcess. Returning the
    // documented path also gives setup UI one stable, actionable location.
    return preferred;
}

struct TrainerUiEvent {
    QString type;
    QString errorCode;
    QString stage;
    int stageEpoch = 0;
    int globalEpoch = 0;
    int epoch = 0;
    int epochs = 0;
    int batch = 0;
    int batches = 0;
    double percent = -1.0;
    double trainLoss = -1.0;
    double validationLoss = -1.0;
    double accuracy = -1.0;
    double macroF1 = -1.0;
    double elapsedSeconds = -1.0;
};

bool parseTrainerUiEvent(const QString& line, TrainerUiEvent* parsed) {
    if (!parsed)
        return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const QJsonObject object = document.object();
    const QJsonObject metrics = object.value("metrics").toObject();
    const QJsonObject errorObject = object.value("error").toObject();
    const QJsonObject checkpoint = object.value("checkpoint").toObject();
    parsed->type = object.value("event").toString(object.value("type").toString()).trimmed().toLower();
    parsed->errorCode = object.value("error_code").toString(
        object.value("code").toString(errorObject.value("code").toString())).trimmed().toLower();
    parsed->stage = object.value("stage").toString(object.value("phase").toString()).trimmed();
    parsed->stageEpoch = object.value("epoch").toInt(object.value("current").toInt());
    parsed->globalEpoch = object.value("global_epoch").toInt(parsed->stageEpoch);
    parsed->epoch = parsed->stageEpoch;
    parsed->epochs = object.value("epochs").toInt(object.value("total_epochs").toInt(object.value("total").toInt()));
    parsed->batch = object.value("batch").toInt();
    parsed->batches = object.value("batches").toInt(object.value("total_batches").toInt());
    parsed->percent = object.value("percent").toDouble(-1.0);
    parsed->trainLoss = object.value("train_loss").toDouble(
        metrics.value("train_loss").toDouble(object.value("loss").toDouble(-1.0)));
    parsed->validationLoss = object.value("validation_loss").toDouble(
        metrics.value("val_loss").toDouble(object.value("val_loss").toDouble(-1.0)));
    parsed->accuracy = object.value("accuracy").toDouble(
        metrics.value("val_accuracy").toDouble(object.value("validation_accuracy").toDouble(-1.0)));
    parsed->macroF1 = object.value("macro_f1").toDouble(metrics.value("val_macro_f1").toDouble(-1.0));
    parsed->elapsedSeconds = object.value("elapsed_seconds").toDouble(metrics.value("elapsed_seconds").toDouble(-1.0));
    if (parsed->macroF1 < 0.0 && checkpoint.value("metric").toString() == "val_macro_f1")
        parsed->macroF1 = checkpoint.value("value").toDouble(-1.0);
    return !parsed->type.isEmpty();
}

QString trainerPlainLanguageError(const QString& code) {
    if (code.contains("cuda") || code.contains("gpu"))
        return QStringLiteral("GPU training is unavailable.");
    if (code.contains("memory"))
        return QStringLiteral("The GPU did not have enough memory. Reduce the batch size or use the CPU.");
    if (code.contains("dataset"))
        return QStringLiteral("This dataset cannot be used for training. Check its metadata and images.");
    if (code.contains("python") || code.contains("environment") || code.contains("package"))
        return QStringLiteral("Training tools are not installed or configured.");
    return QStringLiteral("Training could not continue. Open Detailed log for technical information.");
}

QString conciseModelLoadFailure(const QString& detail) {
    const QString normalized = detail.simplified();
    if (normalized.contains("external data", Qt::CaseInsensitive) ||
        normalized.contains(".onnx.data", Qt::CaseInsensitive))
        return QStringLiteral("Model data file is missing");
    if (normalized.contains("metadata", Qt::CaseInsensitive) &&
        normalized.contains("missing", Qt::CaseInsensitive))
        return QStringLiteral("Model details are missing");
    if (normalized.contains("model", Qt::CaseInsensitive) &&
        normalized.contains("missing", Qt::CaseInsensitive))
        return QStringLiteral("Model file is missing");
    if (normalized.contains("label", Qt::CaseInsensitive) || normalized.contains("class", Qt::CaseInsensitive))
        return QStringLiteral("Model labels are invalid");
    if (normalized.contains("onnx", Qt::CaseInsensitive))
        return QStringLiteral("Model file could not be opened");
    return QStringLiteral("Model could not be loaded");
}

void renderTrainerCurves(QLabel* canvas, const QVector<QPointF>& first, const QVector<QPointF>& second,
                         const QColor& firstColor, const QColor& secondColor) {
    if (!canvas)
        return;
    QPixmap plot(760, 240);
    plot.fill(Qt::transparent);
    QPainter painter(&plot);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds(62, 24, 670, 170);
    const QColor gridColor(125, 135, 145, 105);
    painter.setFont(QFont(painter.font().family(), 9));
    painter.setPen(QPen(gridColor, 1));
    for (int tick = 0; tick <= 4; ++tick) {
        const qreal y = bounds.bottom() - bounds.height() * tick / 4.0;
        painter.drawLine(QPointF(bounds.left(), y), QPointF(bounds.right(), y));
    }
    painter.drawLine(bounds.bottomLeft(), bounds.bottomRight());
    painter.drawLine(bounds.bottomLeft(), bounds.topLeft());

    double maximum = 0.0;
    int maximumEpoch = 1;
    for (const auto& series : {first, second}) {
        for (const QPointF& point : series) {
            maximum = qMax(maximum, point.y());
            maximumEpoch = qMax(maximumEpoch, qRound(point.x()));
        }
    }
    maximum = qMax(maximum, 0.0001);
    painter.setPen(QColor(75, 82, 90));
    QVector<int> epochTicks;
    if (maximumEpoch <= 5) {
        for (int epoch = 1; epoch <= maximumEpoch; ++epoch)
            epochTicks.push_back(epoch);
    } else {
        for (int tick = 0; tick <= 4; ++tick) {
            const int epoch = qRound(1.0 + (maximumEpoch - 1.0) * tick / 4.0);
            if (epochTicks.isEmpty() || epochTicks.constLast() != epoch)
                epochTicks.push_back(epoch);
        }
    }
    const auto epochX = [&](double epoch) {
        return desktop_app::trainerEpochX(bounds.left(), bounds.width(), maximumEpoch, epoch);
    };
    for (int epoch : epochTicks) {
        const qreal x = epochX(epoch);
        painter.setPen(QPen(gridColor, 1));
        painter.drawLine(QPointF(x, bounds.top()), QPointF(x, bounds.bottom()));
        painter.setPen(QColor(75, 82, 90));
        painter.drawText(QRectF(x - 25, bounds.bottom() + 4, 50, 18), Qt::AlignCenter,
                         QString::number(epoch));
    }
    for (int tick = 0; tick <= 4; ++tick) {
        const qreal y = bounds.bottom() - bounds.height() * tick / 4.0;
        painter.drawText(QRectF(2, y - 9, 54, 18), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(maximum * tick / 4.0, 'g', 3));
    }
    painter.drawText(QRectF(bounds.left(), 215, bounds.width(), 20), Qt::AlignCenter, "Epoch");
    painter.save();
    painter.translate(14, bounds.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-bounds.height() / 2, -10, bounds.height(), 20), Qt::AlignCenter,
                     canvas->objectName().contains("Loss") ? "Loss" : "Score");
    painter.restore();
    const auto drawSeries = [&](const QVector<QPointF>& series, const QColor& color) {
        if (series.isEmpty())
            return;
        QPainterPath path;
        for (int index = 0; index < series.size(); ++index) {
            const QPointF source = series.at(index);
            const QPointF mapped(epochX(source.x()),
                                 bounds.bottom() - bounds.height() * source.y() / maximum);
            index == 0 ? path.moveTo(mapped) : path.lineTo(mapped);
        }
        painter.setPen(QPen(color, 3));
        painter.drawPath(path);
        painter.setBrush(color);
        for (const QPointF& source : series) {
            const QPointF mapped(epochX(source.x()),
                                 bounds.bottom() - bounds.height() * source.y() / maximum);
            painter.drawEllipse(mapped, 3.5, 3.5);
        }
    };
    drawSeries(first, firstColor);
    drawSeries(second, secondColor);
    const bool lossPlot = canvas->objectName().contains("Loss");
    const QString firstLegend = lossPlot ? QStringLiteral("Training loss") : QStringLiteral("Validation accuracy");
    const QString secondLegend = lossPlot ? QStringLiteral("Validation loss") : QStringLiteral("Macro F1");
    const auto drawLegend = [&](int x, const QColor& color, const QString& label) {
        painter.setPen(QPen(color, 3));
        painter.drawLine(x, 12, x + 20, 12);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x + 10, 12), 3.0, 3.0);
        painter.setPen(QColor(75, 82, 90));
        painter.drawText(x + 25, 17, label);
    };
    drawLegend(lossPlot ? 430 : 390, firstColor, firstLegend);
    drawLegend(lossPlot ? 575 : 570, secondColor, secondLegend);
    if (first.isEmpty() && second.isEmpty()) {
        painter.drawText(bounds.adjusted(12, 12, -12, -12), Qt::AlignCenter,
                         "Training results will appear here.");
    }
    canvas->setText(QString());
    canvas->setPixmap(plot);
    canvas->setScaledContents(true);
}

void upsertTrainerHistoryPoint(QVector<QPointF>& history, int globalEpoch, double value) {
    if (globalEpoch <= 0)
        return;
    for (QPointF& point : history) {
        if (qRound(point.x()) == globalEpoch) {
            point.setY(value);
            return;
        }
    }
    history.push_back(QPointF(globalEpoch, value));
    std::sort(history.begin(), history.end(), [](const QPointF& left, const QPointF& right) {
        return left.x() < right.x();
    });
}

QString modelsRootForSaveModelUi() {
    const QString overridePath = qEnvironmentVariable("OVDS_MODELS_ROOT_PATH").trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();
    return defaultOpenDssModelsPath();
}

QString trainedModelFolderNameForUi(const QString& modelName) {
    QString folder;
    folder.reserve(modelName.size());
    for (const QChar ch : modelName.trimmed()) {
        if (ch.isLetterOrNumber() || ch == ' ' || ch == '_' || ch == '-') {
            folder.append(ch);
        } else if (!folder.endsWith('_')) {
            folder.append('_');
        }
    }
    while (folder.startsWith('.') || folder.startsWith(' ') || folder.startsWith('_'))
        folder.remove(0, 1);
    while (folder.endsWith('.') || folder.endsWith(' ') || folder.endsWith('_'))
        folder.chop(1);
    return folder.isEmpty() ? QString("trained_model") : folder;
}

QString promptForTrainedModelName(QWidget* parent, const QString& defaultName) {
    const QString verifierName = qEnvironmentVariable("OVDS_VERIFY_SAVE_MODEL_AS_NAME").trimmed();
    if (!verifierName.isEmpty())
        return verifierName;

    QDialog dialog(parent);
    dialog.setWindowTitle("Save Model As");
    nameWidget(&dialog, "SaveModelAsDialog");

    auto* layout = new QVBoxLayout(&dialog);
    auto* prompt = new QLabel("Choose the name for the trained model in your OpenDSS model workspace.");
    prompt->setWordWrap(true);
    layout->addWidget(prompt);

    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(defaultName);
    nameWidget(nameEdit, "SaveModelNameEdit");
    form->addRow("Model name", nameEdit);
    layout->addLayout(form);

    auto* folderLabel = new QLabel;
    folderLabel->setProperty("mutedText", true);
    folderLabel->setWordWrap(true);
    nameWidget(folderLabel, "SaveModelFolderPreviewLabel");
    layout->addWidget(folderLabel);

    const QString modelsRoot = modelsRootForSaveModelUi();
    auto updateFolderPreview = [nameEdit, folderLabel, modelsRoot]() {
        const QString folderName = trainedModelFolderNameForUi(nameEdit->text());
        folderLabel->setText("Folder: " + QDir::toNativeSeparators(QDir(modelsRoot).filePath(folderName)));
    };
    QObject::connect(nameEdit, &QLineEdit::textChanged, &dialog, updateFolderPreview);
    updateFolderPreview();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    nameWidget(buttons, "SaveModelAsButtons");
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString modelName = nameEdit->text().trimmed();
        if (modelName.isEmpty()) {
            QMessageBox::warning(&dialog, "Save Model As", "Model name cannot be empty.");
            return;
        }
        const QString destination = QDir(modelsRoot).filePath(trainedModelFolderNameForUi(modelName));
        if (QFileInfo::exists(destination)) {
            QMessageBox::warning(&dialog, "Save Model As",
                                 "A model folder already exists for this name. Choose a different model name.");
            return;
        }
        dialog.accept();
    });
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        const auto discard = QMessageBox::question(
            parent, "Discard trained model?",
            "Discard this completed training result? The diagnostic log will be kept.",
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (discard != QMessageBox::Discard)
            return promptForTrainedModelName(parent, nameEdit->text().trimmed());
        return {};
    }
    return nameEdit->text().trimmed();
}

bool promptUseTrainedModelForSortingNow(QWidget* parent) {
    const QString verifierAnswer = qEnvironmentVariable("OVDS_VERIFY_USE_TRAINED_MODEL_NOW").trimmed().toLower();
    if (verifierAnswer == "1" || verifierAnswer == "true" || verifierAnswer == "yes")
        return true;
    if (verifierAnswer == "0" || verifierAnswer == "false" || verifierAnswer == "no")
        return false;

    return QMessageBox::question(parent, "Use trained model", "Use this model for sorting now?",
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
}

QString artifactPathFromRunDir(const QString& path, const QString& runDir) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (QFileInfo(trimmed).isAbsolute())
        return QFileInfo(trimmed).absoluteFilePath();
    return QDir(runDir).absoluteFilePath(trimmed);
}

TrainerCompletionArtifacts parseSuccessfulTrainingArtifactsJsonl(const QString& jsonl) {
    TrainerCompletionArtifacts result;
    const QStringList lines = jsonl.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith('{'))
            continue;
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        const QJsonObject event = doc.object();
        if (event.value("event").toString() != "run_finished" || event.value("status").toString() != "ok")
            continue;
        const QString runDir = QFileInfo(event.value("run_dir").toString()).absoluteFilePath();
        const QJsonObject artifacts = event.value("artifacts").toObject();
        const QString modelPath = artifactPathFromRunDir(artifacts.value("model_onnx").toString(), runDir);
        const QString metadataPath = artifactPathFromRunDir(artifacts.value("metadata_json").toString(), runDir);
        const QString metricsPath = artifactPathFromRunDir(artifacts.value("metrics_csv").toString(), runDir);
        const QString metricsJsonPath = artifactPathFromRunDir(artifacts.value("metrics_json").toString(), runDir);
        const QString trainingConfigPath =
            artifactPathFromRunDir(artifacts.value("training_config_json").toString(), runDir);
        const QString classMetricsPath =
            artifactPathFromRunDir(artifacts.value("class_metrics_csv").toString(), runDir);
        const QString confusionMatrixPath =
            artifactPathFromRunDir(artifacts.value("confusion_matrix_csv").toString(), runDir);
        if (!runDir.isEmpty() && !modelPath.isEmpty() && !metadataPath.isEmpty()) {
            result.complete = true;
            result.runDir = runDir;
            result.modelOnnxPath = modelPath;
            result.metadataJsonPath = metadataPath;
            result.metricsCsvPath = metricsPath;
            result.metricsJsonPath = metricsJsonPath;
            result.trainingConfigJsonPath = trainingConfigPath;
            result.classMetricsCsvPath = classMetricsPath;
            result.confusionMatrixCsvPath = confusionMatrixPath;
        }
    }
    return result;
}

QJsonObject loadRegistryObjectForVerifier(const QString& registryFilePath) {
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonObject registryEntryByIdForVerifier(const QJsonArray& entries, const QString& entryId) {
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").compare(entryId, Qt::CaseInsensitive) == 0)
            return entry;
    }
    return {};
}

void runTrainerResultModelRegistrationVerifier(QWidget* modelWorkspacePage,
                                               DatasetWorkspaceController* datasetController,
                                               QComboBox* trainerStartingModelCombo,
                                               QComboBox* liveModelCombo,
                                               const std::function<void(const QString&)>& refreshLiveModelsFromRegistry,
                                               desktop_app::AppState* appState,
                                               const QString& registryFilePath) {
    QStringList failures;
    auto require = [&](bool condition, const QString& message) {
        if (!condition)
            failures << message;
    };

    require(!qEnvironmentVariable("OVDS_MODEL_REGISTRY_PATH").trimmed().isEmpty(),
            "Verifier uses OVDS_MODEL_REGISTRY_PATH so user registry is not modified");
    require(modelWorkspacePage != nullptr, "Model workspace exists");
    require(datasetController != nullptr, "Dataset workspace controller exists");
    require(trainerStartingModelCombo != nullptr, "Trainer starting-model combo exists");

    QTemporaryDir tempDir(QDir::tempPath() + "/ovds_trainer_registration_verify_XXXXXX");
    const QString persistentRoot = qEnvironmentVariable("OVDS_VERIFY_LIFECYCLE_ROOT").trimmed();
    require(tempDir.isValid() || !persistentRoot.isEmpty(), "Verifier run directory is available");
    const QString runDir = persistentRoot.isEmpty() ? tempDir.path() : QDir(persistentRoot).filePath("completed_run");
    require(QDir().mkpath(runDir), "Verifier creates isolated completed-run folder");
    const QString modelPath = QDir(runDir).filePath("model.onnx");
    const QString modelSidecarPath = QDir(runDir).filePath("model.onnx.data");
    const QString metadataPath = QDir(runDir).filePath("metadata.json");
    const QString metricsPath = QDir(runDir).filePath("metrics.csv");
    const QString metricsJsonPath = QDir(runDir).filePath("metrics.json");
    const QString trainingConfigPath = QDir(runDir).filePath("training_config.json");
    const QString classMetricsPath = QDir(runDir).filePath("class_metrics.csv");
    const QString confusionMatrixPath = QDir(runDir).filePath("confusion_matrix.csv");
    const QString realPackagePath = qEnvironmentVariable("OVDS_VERIFY_REAL_MODEL_PACKAGE").trimmed();
    const bool usingRealPackage = !realPackagePath.isEmpty();
    if (usingRealPackage) {
        const QDir package(realPackagePath);
        require(QFileInfo(package.filePath("model.onnx")).isFile(), "Real verifier package contains model.onnx");
        require(QFileInfo(package.filePath("checkpoint.pth")).isFile(), "Real verifier package contains checkpoint.pth");
        require(QFileInfo(package.filePath("metadata.json")).isFile(), "Real verifier package contains metadata.json");
        require(QFile::copy(package.filePath("model.onnx"), modelPath), "Verifier copies real embedded ONNX artifact");
        require(QFile::copy(package.filePath("checkpoint.pth"), QDir(runDir).filePath("checkpoint.pth")),
                "Verifier copies real checkpoint artifact");
        require(QFile::copy(package.filePath("metadata.json"), metadataPath), "Verifier copies real metadata artifact");
    }

    QFile modelFile(modelPath);
    if (!usingRealPackage) {
    require(modelFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Verifier can write synthetic ONNX artifact");
    if (modelFile.isOpen()) {
        modelFile.write("synthetic verifier model");
        modelFile.close();
    }
    }
    QFile sidecarFile(modelSidecarPath);
    if (!usingRealPackage) {
    require(sidecarFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "Verifier can write synthetic ONNX external-data sidecar");
    if (sidecarFile.isOpen()) {
        sidecarFile.write("synthetic verifier external data");
        sidecarFile.close();
    }
    }

    QJsonObject labels;
    labels["0"] = "Empty";
    labels["1"] = "Single";
    labels["2"] = "MoreThanOne";
    QJsonObject targetPolicy;
    targetPolicy["mode"] = "trigger_on_target_class";
    targetPolicy["target_class_id"] = "1";
    targetPolicy["target_display_label"] = "Single";
    targetPolicy["non_target_class_ids"] = QJsonArray{"0", "2"};
    targetPolicy["trigger_rule"] = "trigger_on_target_class";
    QJsonObject architecture;
    architecture["family"] = "MobileNetV3";
    architecture["variant"] = "small";
    QJsonObject imageValidation;
    imageValidation["status"] = "training_evaluation";
    imageValidation["accuracy"] = 0.95;
    imageValidation["macro_f1"] = 0.94;
    QJsonObject sequenceValidation;
    sequenceValidation["status"] = "not_run";
    QJsonObject validationSummary;
    validationSummary["image_validation"] = imageValidation;
    validationSummary["sequence_validation"] = sequenceValidation;

    const QString modelId = QString("trainer_registration_verifier_%1").arg(QCoreApplication::applicationPid());
    QJsonObject metadata;
    metadata["schema_version"] = "model-metadata-v1";
    metadata["model_id"] = modelId;
    metadata["model_name"] = "Trainer registration verifier model";
    metadata["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    metadata["class_count"] = 3;
    metadata["classes"] = QJsonArray{"0", "1", "2"};
    metadata["class_ids"] = QJsonArray{"0", "1", "2"};
    metadata["display_labels"] = labels;
    metadata["label_schema_version"] = "droplet-labels-target-nontarget-3class-v1";
    metadata["sorting_policy"] = targetPolicy;
    metadata["architecture"] = architecture;
    metadata["training_config"] = QJsonObject{{"architecture", "mobilenet_v3_small"}, {"pretrained", true}};
    metadata["validation_summary"] = validationSummary;
    metadata["limitations"] = QJsonArray{"Verifier synthetic metadata."};

    QString metadataWriteError;
    if (!usingRealPackage)
        require(desktop_app::writeJsonObjectAtomically(metadataPath, metadata, &metadataWriteError),
                "Verifier can write synthetic metadata artifact: " + metadataWriteError);
    QFile checkpointFile(QDir(runDir).filePath("checkpoint.pth"));
    if (!usingRealPackage) {
    require(checkpointFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "Verifier can write synthetic checkpoint artifact");
    if (checkpointFile.isOpen()) {
        checkpointFile.write("synthetic checkpoint");
        checkpointFile.close();
    }
    }
    QFile metricsFile(metricsPath);
    require(metricsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
            "Verifier can write synthetic metrics artifact");
    if (metricsFile.isOpen()) {
        metricsFile.write("stage,epoch,val_accuracy,val_macro_f1\nverify,1,0.95,0.94\n");
        metricsFile.close();
    }
    QString metricsJsonWriteError;
    require(desktop_app::writeJsonObjectAtomically(metricsJsonPath, QJsonObject{{"schema_version", 1}},
                                                   &metricsJsonWriteError),
            "Verifier can write synthetic metrics JSON artifact: " + metricsJsonWriteError);
    QString trainingConfigWriteError;
    require(desktop_app::writeJsonObjectAtomically(trainingConfigPath, QJsonObject{{"architecture", "mobilenet_v3_small"}},
                                                   &trainingConfigWriteError),
            "Verifier can write synthetic training config artifact: " + trainingConfigWriteError);
    QFile classMetricsFile(classMetricsPath);
    require(classMetricsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
            "Verifier can write synthetic class metrics artifact");
    if (classMetricsFile.isOpen()) {
        classMetricsFile.write("class,precision,recall,f1,support\n0,1,1,1,1\n1,1,1,1,1\n2,1,1,1,1\n");
        classMetricsFile.close();
    }
    QFile confusionMatrixFile(confusionMatrixPath);
    require(confusionMatrixFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate),
            "Verifier can write synthetic confusion matrix artifact");
    if (confusionMatrixFile.isOpen()) {
        confusionMatrixFile.write("true_label,0,1,2\n0,1,0,0\n1,0,1,0\n2,0,0,1\n");
        confusionMatrixFile.close();
    }

    const QJsonObject beforeRegistry = loadRegistryObjectForVerifier(registryFilePath);
    const int beforeCount = beforeRegistry.value("entries").toArray().size();
    const QString successfulJsonl =
        QString("{\"event\":\"run_finished\",\"status\":\"ok\",\"run_dir\":\"%1\",\"artifacts\":"
                "{\"model_onnx\":\"%2\",\"metadata_json\":\"%3\",\"metrics_csv\":\"%4\","
                "\"training_config_json\":\"%5\",\"metrics_json\":\"%6\","
                "\"class_metrics_csv\":\"%7\",\"confusion_matrix_csv\":\"%8\"}}\n")
            .arg(runDir, modelPath, metadataPath, metricsPath, trainingConfigPath, metricsJsonPath, classMetricsPath,
                 confusionMatrixPath);
    const TrainerCompletionArtifacts parsed = parseSuccessfulTrainingArtifactsJsonl(successfulJsonl);
    require(parsed.complete, "Successful run_finished JSONL is detected");
    require(QDir::cleanPath(parsed.modelOnnxPath) == QDir::cleanPath(modelPath),
            "Successful JSONL model_onnx path is captured");
    require(QDir::cleanPath(parsed.metadataJsonPath) == QDir::cleanPath(metadataPath),
            "Successful JSONL metadata_json path is captured");
    require(QDir::cleanPath(parsed.metricsCsvPath) == QDir::cleanPath(metricsPath),
            "Successful JSONL metrics_csv path is captured");
    require(QDir::cleanPath(parsed.trainingConfigJsonPath) == QDir::cleanPath(trainingConfigPath),
            "Successful JSONL training_config_json path is captured");
    require(QDir::cleanPath(parsed.metricsJsonPath) == QDir::cleanPath(metricsJsonPath),
            "Successful JSONL metrics_json path is captured");
    require(QDir::cleanPath(parsed.classMetricsCsvPath) == QDir::cleanPath(classMetricsPath),
            "Successful JSONL class_metrics_csv path is captured");
    require(QDir::cleanPath(parsed.confusionMatrixCsvPath) == QDir::cleanPath(confusionMatrixPath),
            "Successful JSONL confusion_matrix_csv path is captured");
    require(!parseSuccessfulTrainingArtifactsJsonl(
                 "{\"event\":\"run_finished\",\"status\":\"failed\",\"artifacts\":{\"model_onnx\":\"x\"}}\n")
                 .complete,
            "Failed run_finished JSONL is ignored");

    auto* refreshButton = modelWorkspacePage ? modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceInternalReloadButton")
                                             : nullptr;
    require(refreshButton != nullptr && refreshButton->isHidden(),
            "Model workspace internal reload control exists and is not user-visible");
    auto* modelTable = modelWorkspacePage ? modelWorkspacePage->findChild<QTableWidget*>("ModelWorkspaceRegistryTable")
                                          : nullptr;
    require(modelTable != nullptr, "Model workspace registry table exists");

    const QString modelRootPath = QDir(runDir).filePath("models");
    qputenv("OVDS_MODELS_ROOT_PATH", QFileInfo(modelRootPath).absoluteFilePath().toUtf8());
    const QString savedName = QString("Auto saved trained model %1").arg(QCoreApplication::applicationPid());
    qputenv("OVDS_VERIFY_SAVE_MODEL_AS_NAME", savedName.toUtf8());
    const QString promptedName = promptForTrainedModelName(modelWorkspacePage, "Ignored default");
    require(promptedName == savedName, "Save Model As verifier name is used for saved trained model");

    QString entryId;
    QString saveError;
    require(saveTrainedModelArtifacts(registryFilePath, parsed.runDir, parsed.modelOnnxPath, parsed.metadataJsonPath,
                                      parsed.metricsCsvPath, parsed.trainingConfigJsonPath, parsed.metricsJsonPath,
                                      parsed.classMetricsCsvPath, parsed.confusionMatrixCsvPath, promptedName, &entryId,
                                      &saveError),
            QString("Training artifacts save into user-ready model folder: %1").arg(saveError));
    const QString savedFolderPath = QDir(modelRootPath).filePath(trainedModelFolderNameForUi(savedName));
    const QDir savedFolder(savedFolderPath);
    require(QFileInfo(savedFolder.filePath("model.onnx")).isFile(), "Saved folder contains model.onnx");
    require(QFileInfo(savedFolder.filePath("checkpoint.pth")).isFile(), "Saved folder contains checkpoint.pth");
    require(!QFileInfo(savedFolder.filePath("model.onnx.data")).exists(),
            "Saved package uses an embedded ONNX model");
    require(QFileInfo(savedFolder.filePath("metadata.json")).isFile(), "Saved folder contains metadata.json");
    require(QFileInfo(savedFolder.filePath("metrics.csv")).isFile(), "Saved folder contains metrics.csv");
    require(QFileInfo(savedFolder.filePath("metrics.json")).isFile(), "Saved folder contains metrics.json");
    require(QFileInfo(savedFolder.filePath("class_metrics.csv")).isFile(),
            "Saved folder contains class_metrics.csv");
    require(QFileInfo(savedFolder.filePath("confusion_matrix.csv")).isFile(),
            "Saved folder contains confusion_matrix.csv");
    require(QFileInfo(savedFolder.filePath("training_config.json")).isFile(),
            "Saved folder contains training_config.json");

    const QJsonObject savedMetadata = loadRegistryObjectForVerifier(savedFolder.filePath("metadata.json"));
    const QJsonObject savedArtifact = savedMetadata.value("artifact").toObject();
    require(savedArtifact.value("checkpoint_file").toString() == "checkpoint.pth",
            "Saved metadata names the training checkpoint");
    require(savedArtifact.value("external_data_files").toArray().isEmpty(),
            "Saved metadata requires no ONNX external-data sidecar");

    const QJsonObject afterRegistry = loadRegistryObjectForVerifier(registryFilePath);
    const QJsonArray afterEntries = afterRegistry.value("entries").toArray();
    const QJsonObject entry = registryEntryByIdForVerifier(afterEntries, entryId);
    require(afterEntries.size() == beforeCount + 1, "Registry row count increases for saved trained model");
    require(!entry.isEmpty(), "Saved trained model registry entry exists");
    require(QDir::cleanPath(registryString(entry, "package_path")) == QDir::cleanPath(savedFolderPath),
            "Registry stores the saved package folder");
    require(!entry.value("active").toBool(false),
            "Saved trained model is not active before use-now confirmation");
    require(QDir::cleanPath(registryString(entry, "model_path")) ==
                QDir::cleanPath(savedFolder.filePath("model.onnx")),
            "Registry entry points to saved ONNX");
    require(QDir::cleanPath(registryString(entry, "metadata_path")) ==
                QDir::cleanPath(savedFolder.filePath("metadata.json")),
            "Registry entry points to saved metadata");

    if (refreshButton) {
        refreshButton->click();
        QCoreApplication::processEvents();
    }
    bool promotedVisible = false;
    if (modelTable) {
        for (int row = 0; row < modelTable->rowCount(); ++row) {
            auto* item = modelTable->item(row, 0);
            if (item && item->text().contains(savedName, Qt::CaseInsensitive)) {
                promotedVisible = true;
                break;
            }
        }
    }
    require(promotedVisible, "Model workspace shows the saved trained model");

    if (datasetController)
        datasetController->refreshTrainerUi();
    require(trainerStartingModelCombo && trainerStartingModelCombo->findData(entryId) >= 0,
            "Trainer starting-model list refreshes to include the saved trained model");

    qputenv("OVDS_VERIFY_USE_TRAINED_MODEL_NOW", "no");
    require(!promptUseTrainedModelForSortingNow(modelWorkspacePage),
            "Use-now prompt supports deterministic no response");
    const QJsonObject noPathEntry =
        registryEntryByIdForVerifier(readModelRegistryEntriesFromPath(registryFilePath, nullptr), entryId);
    require(!noPathEntry.value("active").toBool(false),
            "No response leaves the saved trained model inactive");

    qputenv("OVDS_VERIFY_USE_TRAINED_MODEL_NOW", "yes");
    require(promptUseTrainedModelForSortingNow(modelWorkspacePage),
            "Use-now prompt supports deterministic yes response");
    QString activationError;
    require(activateModelRegistryEntry(registryFilePath, entryId, &activationError),
            QString("Saved trained model can be set active through registry activation path: %1").arg(activationError));
    const QJsonArray activeEntries = readModelRegistryEntriesFromPath(registryFilePath, nullptr);
    const QJsonObject activeEntry = registryEntryByIdForVerifier(activeEntries, entryId);
    require(activeEntry.value("active").toBool(false),
            "Yes response marks the saved trained model active");
    require(registryString(activeRegistryEntry(activeEntries), "registry_entry_id").compare(entryId, Qt::CaseInsensitive) == 0,
            "Registry active model points to the saved trained model");
    if (refreshLiveModelsFromRegistry)
        refreshLiveModelsFromRegistry(entryId);
    require(liveModelCombo != nullptr, "Live View model selector exists");
    require(liveModelCombo && liveModelCombo->currentData(Qt::UserRole + 1).toString().compare(entryId, Qt::CaseInsensitive) == 0,
            "Live View model selector refreshes to the active saved trained model");
    require(appState && appState->activeModelId.compare(entryId, Qt::CaseInsensitive) == 0,
            "Live View current active model state refreshes to the saved trained model");

    QString collisionEntryId;
    QString collisionError;
    require(!saveTrainedModelArtifacts(registryFilePath, parsed.runDir, parsed.modelOnnxPath, parsed.metadataJsonPath,
                                       parsed.metricsCsvPath, parsed.trainingConfigJsonPath, parsed.metricsJsonPath,
                                       parsed.classMetricsCsvPath, parsed.confusionMatrixCsvPath, promptedName,
                                       &collisionEntryId, &collisionError),
            "Saved model folder name collisions are blocked");
    require(collisionError.contains("already exists", Qt::CaseInsensitive),
            "Collision block reports the existing model folder");

    const int exitCode = failures.isEmpty() ? 0 : 2;
    if (failures.isEmpty()) {
        qInfo().noquote() << "Trainer result model-registration verifier passed.";
    } else {
        qWarning().noquote() << "Trainer result model-registration verifier failed:" << failures.join("; ");
    }
    std::exit(exitCode);
}

void verifyValidationWritebackToModelRegistry(const std::function<void(bool, const QString&)>& require) {
    QTemporaryDir tempDir(QDir::tempPath() + "/ovds_validation_writeback_verify_XXXXXX");
    require(tempDir.isValid(), "Validation writeback verifier temp directory is available");
    if (!tempDir.isValid())
        return;

    const QString modelPath = QDir(tempDir.path()).filePath("validated_model.onnx");
    const QString metadataPath = QDir(tempDir.path()).filePath("metadata.json");
    const QString registryPath = QDir(tempDir.path()).filePath("model_registry.json");
    const QString summaryPath = QDir(tempDir.path()).filePath("validation_summary.json");
    const QString failedSummaryPath = QDir(tempDir.path()).filePath("failed_validation_summary.json");
    const QString errorSummaryPath = QDir(tempDir.path()).filePath("error_validation_summary.json");

    QFile modelFile(modelPath);
    require(modelFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Validation writeback verifier can write model");
    if (modelFile.isOpen()) {
        modelFile.write("synthetic validation model");
        modelFile.close();
    }

    QJsonObject metadata;
    metadata["schema_version"] = "model-metadata-v1";
    metadata["model_id"] = "validation_writeback_verifier";
    metadata["model_name"] = "Validation writeback verifier model";
    metadata["classes"] = QJsonArray{"0", "1"};
    metadata["display_labels"] = QJsonObject{{"0", "Non-target"}, {"1", "Target"}};
    QString metadataWriteError;
    require(desktop_app::writeJsonObjectAtomically(metadataPath, metadata, &metadataWriteError),
            "Validation writeback verifier can write metadata: " + metadataWriteError);

    QJsonObject entry;
    entry["registry_entry_id"] = "validation_writeback_verifier";
    entry["display_name"] = "Validation writeback verifier model";
    entry["model_path"] = modelPath;
    entry["metadata_path"] = metadataPath;
    entry["validation_status"] = "Not validated";
    QJsonObject registry;
    registry["schema_version"] = "model-registry-v1";
    registry["entries"] = QJsonArray{entry};
    QString registryWriteError;
    require(desktop_app::writeJsonObjectAtomically(registryPath, registry, &registryWriteError),
            "Validation writeback verifier can write registry: " + registryWriteError);

    QJsonObject summary;
    summary["schema_version"] = "validator.v1";
    summary["status"] = "completed_with_errors";
    summary["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    summary["model"] = QJsonObject{{"model_path", modelPath}, {"metadata_path", metadataPath}};
    summary["dataset"] = QJsonObject{{"samples_total", 5}, {"samples_evaluated", 5}, {"samples_failed", 1}};
    summary["metrics"] = QJsonObject{{"accuracy", 0.8}, {"macro_f1", 0.75}, {"samples_incorrect", 1}};
    QString summaryWriteError;
    require(desktop_app::writeJsonObjectAtomically(summaryPath, summary, &summaryWriteError),
            "Validation writeback verifier can write successful summary: " + summaryWriteError);

    QString updatedEntryId;
    QString error;
    require(updateModelRegistryImageValidationSummary(registryPath, summaryPath, &updatedEntryId, &error),
            "Successful image validation summary writes back to model registry: " + error);
    require(updatedEntryId == "validation_writeback_verifier",
            "Validation writeback returns the matched registry entry id");

    const QJsonObject updatedMetadata = loadRegistryObjectForVerifier(metadataPath);
    const QJsonObject imageValidation =
        updatedMetadata.value("validation_summary").toObject().value("image_validation").toObject();
    require(imageValidation.value("accuracy").toDouble() == 0.8,
            "Validation writeback stores accuracy in model metadata");
    require(imageValidation.value("macro_f1").toDouble() == 0.75,
            "Validation writeback stores macro F1 in model metadata");
    require(imageValidation.value("status").toString() == "completed_with_errors",
            "Completed validation with mismatches remains a completed image result");

    const QJsonObject updatedRegistry = loadRegistryObjectForVerifier(registryPath);
    const QJsonObject updatedEntry =
        registryEntryByIdForVerifier(updatedRegistry.value("entries").toArray(), "validation_writeback_verifier");
    require(!registryString(updatedEntry, "validation_status").contains("Not validated", Qt::CaseInsensitive),
            "Validation writeback clears Not validated registry status");
    require(updatedEntry.value("validation_evidence").toObject().value("image_validation").isObject(),
            "Validation writeback stores image evidence in registry");

    QJsonObject failedSummary = summary;
    failedSummary["status"] = "failed";
    QString failedSummaryWriteError;
    require(desktop_app::writeJsonObjectAtomically(failedSummaryPath, failedSummary, &failedSummaryWriteError),
            "Validation writeback verifier can write failed summary: " + failedSummaryWriteError);
    error.clear();
    require(!updateModelRegistryImageValidationSummary(registryPath, failedSummaryPath, nullptr, &error),
            "Failed image validation summary is not written back as validated");

    QJsonObject errorSummary = summary;
    errorSummary["status"] = "error";
    QString errorSummaryWriteError;
    require(desktop_app::writeJsonObjectAtomically(errorSummaryPath, errorSummary, &errorSummaryWriteError),
            "Validation writeback verifier can write error summary: " + errorSummaryWriteError);
    error.clear();
    require(!updateModelRegistryImageValidationSummary(registryPath, errorSummaryPath, nullptr, &error),
            "Errored image validation summary is not written back as validated");
}

QMutex liveEventMutex;
SequenceEventTracker liveEventTracker;

} // namespace

MainWindow::MainWindow(const AppContext& context, QWidget* parent) : QMainWindow(parent), context_(context) {
    nameWidget(this, "MainWindow");
    setWindowTitle("OpenDSS");
    setWindowIcon(QIcon(":/branding/opendss-icon-512.png"));
    installEventFilter(new MainWindowCloseFilter(this));
    resize(1280, 720);
    setMinimumSize(1100, 650);
}

const AppContext& MainWindow::appContext() const {
    return context_;
}

int runTrainerSetupStatusVerifierAppOwned() {
    QSettings settings;
    const QString previousPython = settings.value("settings/pythonTrainer").toString();
    const QString previousComputeDevice = settings.value("settings/computeDevice", "auto").toString();
    const QString previousValidatorDevice = settings.value("validator/device", previousComputeDevice).toString();

    settings.setValue("settings/computeDevice", QStringLiteral("auto"));
    settings.setValue("validator/device", QStringLiteral("auto"));

    auto restoreSettings = [&]() {
        settings.setValue("settings/pythonTrainer", previousPython);
        settings.setValue("settings/computeDevice", previousComputeDevice);
        settings.setValue("validator/device", previousValidatorDevice);
        settings.sync();
    };

    const QString expectedPython = resolvedTrainerPythonExecutable(QString(), QStringLiteral("auto"));
    settings.setValue("settings/pythonTrainer", expectedPython);
    settings.sync();

    const QString trainerPythonField = QDir::toNativeSeparators(expectedPython);
    const QString settingsTrainerPythonField = trainerPythonField;
    const QString trainerPythonStatus =
        QFileInfo(expectedPython).isFile() ? "Found: " + QDir::toNativeSeparators(expectedPython)
                                           : "Missing: " + QDir::toNativeSeparators(expectedPython);
    const QString trainerHelperStatus = "Found: app-owned verifier uses packaged training/python status source";
    const QString trainerDeviceStatus = "Auto";
    const QString trainerPackagesStatus = "Run Check Python setup to inspect training and ONNX packages.";
    const QString trainerEnvCheckStatus = "Env check: not run this session";

    QStringList failures;
    auto require = [&](bool condition, const QString& message) {
        if (!condition) {
            failures << message;
            std::printf("TRAINER SETUP STATUS VERIFY FAIL: %s\n", message.toLocal8Bit().constData());
            qCritical().noquote() << "TRAINER SETUP STATUS VERIFY FAIL:" << message;
        } else {
            std::printf("TRAINER SETUP STATUS VERIFY PASS: %s\n", message.toLocal8Bit().constData());
            qInfo().noquote() << "TRAINER SETUP STATUS VERIFY PASS:" << message;
        }
    };

    QTemporaryDir configuredEnvironment;
    const QString configuredGpuPython = configuredEnvironment.filePath(QStringLiteral("training-venv-gpu/python.exe"));
    QDir().mkpath(QFileInfo(configuredGpuPython).absolutePath());
    QFile configuredPythonFile(configuredGpuPython);
    require(configuredPythonFile.open(QIODevice::WriteOnly),
            "Verifier creates a bounded configured GPU interpreter fixture");
    configuredPythonFile.close();
    require(sameCleanPath(resolvedTrainerPythonExecutable(configuredGpuPython, QStringLiteral("auto")),
                          configuredGpuPython),
            "Auto preserves an existing valid configured GPU interpreter");
    const QString nonexistentPython = configuredEnvironment.filePath(QStringLiteral("missing/python.exe"));
    const QString recoveredMissing = resolvedTrainerPythonExecutable(nonexistentPython, QStringLiteral("auto"));
    require(!QFileInfo(recoveredMissing).isFile() || !sameCleanPath(recoveredMissing, nonexistentPython),
            "A nonexistent configured interpreter cannot be treated as Ready or persisted as itself");

    require(QFileInfo(expectedPython).isFile() &&
                (sameCleanPath(expectedPython, documentedTrainerPythonExecutable(QStringLiteral("training-venv"))) ||
                 sameCleanPath(expectedPython, legacyTrainerPythonExecutable(QStringLiteral("training-venv")))),
            "Default Trainer Python resolves to a valid OpenDSS or compatible legacy CPU venv");
    require(sameCleanPath(trainerPythonField, expectedPython),
            "Trainer Python field uses resolved documented path");
    require(!settingsTrainerPythonField.trimmed().isEmpty(), "Settings Trainer Python field exists");
    require(sameCleanPath(settingsTrainerPythonField, expectedPython),
            "Settings Trainer Python field mirrors resolved path");
    require(!trainerPythonStatus.trimmed().isEmpty(), "Python setup status label is populated");
    require(!trainerHelperStatus.contains("Checked automatically", Qt::CaseInsensitive),
            "Training helper status is not stale placeholder text");
    require(!trainerDeviceStatus.contains("Checked automatically", Qt::CaseInsensitive),
            "Compute device status is not stale placeholder text");
    require(trainerPackagesStatus.contains("Check Python setup", Qt::CaseInsensitive),
            "Package status tells user how to run env-check");
    require(trainerEnvCheckStatus.contains("not run", Qt::CaseInsensitive),
            "Env-check status reports not run");

    restoreSettings();
    const int exitCode = failures.isEmpty() ? 0 : 2;
    if (failures.isEmpty()) {
        std::printf("Trainer setup-status app-owned verifier passed.\n");
        qInfo().noquote() << "Trainer setup-status app-owned verifier passed.";
    } else {
        std::printf("Trainer setup-status app-owned verifier failed: %s\n", failures.join("; ").toLocal8Bit().constData());
        qWarning().noquote() << "Trainer setup-status app-owned verifier failed:" << failures.join("; ");
    }
    return exitCode;
}

int MainWindow::runSetupAndEventLoop(QApplication& app, QSettings& runtimeSettings, desktop_app::AppState& appState,
                                     const QJsonArray& registryEntries, const QString& registryFilePath,
                                     const QString& registryLoadWarning, QSplashScreen& splash,
                                     QElapsedTimer& splashTimer) {
    const AppOptions& options = context_.options;
    const bool verifyDatasetWorkspace =
        !qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_MANIFEST").trimmed().isEmpty();
    const bool verifyDatasetWorkspaceMetadataOnly =
        qEnvironmentVariable("OVDS_DATASET_WORKSPACE_VERIFY_METADATA_ONLY") == "1";
    const bool verifyTrainerLaunch = qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_LAUNCH") != 0;
    const bool verifyTrainerSetupStatus =
        qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_SETUP_STATUS") != 0 ||
        QCoreApplication::arguments().contains(QStringLiteral("--verify-trainer-setup-status"));
    const bool verifyDefaultPaths = qEnvironmentVariableIntValue("OVDS_VERIFY_DEFAULT_PATHS") != 0;
    const bool verifyComputeSettings = qEnvironmentVariableIntValue("OVDS_VERIFY_COMPUTE_SETTINGS") != 0;
    const bool verifyCollectionMode = qEnvironmentVariableIntValue("OVDS_VERIFY_COLLECTION_MODE") != 0;
    const bool verifyCollectionPostprocessor = qEnvironmentVariableIntValue("OVDS_VERIFY_COLLECTION_POSTPROCESSOR") != 0;
    const bool verifyDatasetHandoff = qEnvironmentVariableIntValue("OVDS_VERIFY_DATASET_HANDOFF") != 0;
    const bool verifyWorkspaceSplitters = qEnvironmentVariableIntValue("OVDS_VERIFY_WORKSPACE_SPLITTERS") != 0;
    const bool verifyModelsWorkspaceConsolidation =
        qEnvironmentVariableIntValue("OVDS_VERIFY_MODELS_WORKSPACE_CONSOLIDATION") != 0;
    const bool verifyProductionModelStatus =
        qEnvironmentVariableIntValue("OVDS_VERIFY_PRODUCTION_MODEL_STATUS") != 0;
    const QString verifierTracePath = qEnvironmentVariable("OVDS_VERIFY_TRACE_PATH").trimmed();
    const auto verifierTrace = [verifierTracePath](const QString& message) {
        if (verifierTracePath.isEmpty())
            return;
        QFile trace(verifierTracePath);
        if (trace.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            trace.write((message + QLatin1Char('\n')).toUtf8());
            trace.flush();
        }
    };
    verifierTrace(QStringLiteral("startup: verifier flags parsed"));
    const bool verifyResetLayout = qEnvironmentVariableIntValue("OVDS_VERIFY_RESET_LAYOUT") != 0;
    const bool verifyNavigationInfo = qEnvironmentVariableIntValue("OVDS_VERIFY_NAVIGATION_INFO") != 0;
    const bool verifyBoundedFullShell = verifyModelsWorkspaceConsolidation || verifyDefaultPaths ||
                                        verifyNavigationInfo || verifyProductionModelStatus || verifyTrainerLaunch ||
                                        verifyComputeSettings ||
                                        qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_MODEL_SELECTION") != 0;
    if (verifyBoundedFullShell) {
        const int requestedWatchdogMs = qEnvironmentVariableIntValue("OVDS_VERIFY_WATCHDOG_MS");
        const int watchdogMs = requestedWatchdogMs > 0 ? requestedWatchdogMs : 45000;
        QTimer::singleShot(watchdogMs, this, [this, &app, verifierTrace, watchdogMs]() {
            verifierTrace(QStringLiteral("verifier-watchdog: timed out after %1 ms").arg(watchdogMs));
            qCritical().noquote() << "VERIFIER WATCHDOG TIMEOUT after" << watchdogMs << "ms";
            QCoreApplication::exit(124);
        });
    }
    const DefaultWorkspacePaths& defaultWorkspacePaths = context_.paths.defaultWorkspacePaths;
    const QString& logPath = context_.paths.sessionLogPath;
    const QString initialDaqStatusText = appState.daqStatusText;
    if (verifyDatasetWorkspace && verifyDatasetWorkspaceMetadataOnly) {
        desktop_app::workspace::DatasetWorkspaceControls datasetWorkspaceControls;
        std::unique_ptr<QWidget> datasetWorkspace(
            desktop_app::workspace::buildDatasetWorkspace(datasetWorkspaceControls));
        app.processEvents();
        const QVariant exitCode = qApp->property("ovdsDatasetWorkspaceVerifyExitCode");
        if (exitCode.isValid())
            return exitCode.toInt();
        return 2;
    }
#ifdef HAVE_NIDAQMX
    constexpr bool kDaqBuildEnabled = true;
#else
    constexpr bool kDaqBuildEnabled = false;
#endif
    bool viewerOnly = false;
    auto currentThemeMode =
        runtimeSettings.value("shell/theme", "dark").toString().compare("light", Qt::CaseInsensitive) == 0
            ? desktop_app::theme::ThemeMode::Light
            : desktop_app::theme::ThemeMode::Dark;
    std::function<void()> refreshThemeDependentChrome = []() {};
    auto applyShellTheme = [&]() {
        app.setPalette(desktop_app::theme::palette(currentThemeMode));
        this->setStyleSheet(desktop_app::theme::shellStyleSheet(currentThemeMode));
        refreshThemeDependentChrome();
    };
    applyShellTheme();

    // Live view area with zoomable/pannable view
    auto imageView = new ZoomImageView;
    nameWidget(imageView, "LiveImageView");
    imageView->setFrameShape(QFrame::NoFrame);
    imageView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    imageView->viewport()->setAutoFillBackground(false);
    imageView->setImageLabelObjectName("LiveImageLabel");
    imageView->setMinimumSize(420, 320);
    imageView->setStyleSheet("background:transparent;");
    imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto cameraImageView = new ZoomImageView;
    nameWidget(cameraImageView, "CameraPreviewImageView");
    cameraImageView->setFrameShape(QFrame::NoFrame);
    cameraImageView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    cameraImageView->viewport()->setAutoFillBackground(false);
    cameraImageView->setImageLabelObjectName("CameraPreviewImageLabel");
    cameraImageView->setMinimumSize(420, 320);
    cameraImageView->setStyleSheet("background:transparent;");
    cameraImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Info panel
    auto statusLabel = new QLabel("Status: Not initialized");
    nameWidget(statusLabel, "RuntimeStatusLabel");
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statusLabel->setTextFormat(Qt::PlainText);
    auto statsLabel = new QLabel("Resolution: --\nFPS: --\nFrame: --");
    nameWidget(statsLabel, "RuntimeStatsLabel");
    statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsLabel->setTextFormat(Qt::PlainText);
    statsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    statsLabel->setMinimumWidth(220);
    // Buttons
    auto startBtn = new QPushButton("Start");
    auto pipelineStartBtn = new QPushButton("Start Sorting");
    auto collectionToggleBtn = new QPushButton("Start Data Collection");
    auto pipelineStopBtn = new QPushButton("Stop Sorting");
    pipelineStopBtn->setEnabled(false);
    auto reconnectBtn = new QPushButton("Reconnect");
    auto applyBtn = new QPushButton("Apply Camera Settings");
    auto viewerBtn = new QPushButton("Viewer");
    nameWidget(startBtn, "CameraStartButton");
    nameWidget(pipelineStartBtn, "PipelineStartButton");
    nameWidget(collectionToggleBtn, "LiveDataCollectionButton");
    nameWidget(pipelineStopBtn, "PipelineStopButton");
    nameWidget(reconnectBtn, "CameraReconnectButton");
    nameWidget(applyBtn, "CameraApplySettingsButton");
    nameWidget(viewerBtn, "OpenViewerButton");
    std::atomic<bool> collectionActive(false);

    auto addDisabledAction = [](QMenu* menu, const QString& text, const char* objectName,
                                const QString& statusTip = QString()) {
        QAction* action = menu->addAction(text);
        nameAction(action, objectName);
        action->setEnabled(false);
        if (!statusTip.isEmpty())
            action->setStatusTip(statusTip);
        return action;
    };

    auto fileMenu = this->menuBar()->addMenu("&File");
    auto openViewerAction = fileMenu->addAction("Open &Viewer");
    auto openOutputAction = fileMenu->addAction("Open Current &Output Folder");
    fileMenu->addSeparator();
    addDisabledAction(fileMenu, "New Project", "FileNewProjectAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Open Project", "FileOpenProjectAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Recent Projects", "FileRecentProjectsAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Save Session", "FileSaveSessionAction", "Session packaging is a future workflow.");
    addDisabledAction(fileMenu, "Export Support Bundle", "FileExportSupportBundleAction",
                      "Support bundle export is a future workflow.");
    fileMenu->addSeparator();
    auto exitAction = fileMenu->addAction("E&xit");

    auto cameraMenu = this->menuBar()->addMenu("&Camera");
    auto reconnectAction = cameraMenu->addAction("&Reconnect");
    auto startPreviewAction = cameraMenu->addAction("Start Preview");
    auto stopPreviewAction = cameraMenu->addAction("Stop Preview");
    auto captureStillAction = cameraMenu->addAction("Capture Still");
    addDisabledAction(cameraMenu, "Camera Presets", "CameraPresetsAction",
                      "Camera preset management is not wired in this shell step.");

    auto datasetMenu = this->menuBar()->addMenu("&Dataset");
    addDisabledAction(datasetMenu, "New Dataset", "DatasetNewDatasetAction",
                      "Dataset workflows are placeholder-only in this shell step.");
    auto datasetOpenAction = datasetMenu->addAction("Open Image Set");
    datasetOpenAction->setStatusTip("Open the Image Set review workspace.");
    auto datasetBuildAction = datasetMenu->addAction("Build Image Set");
    datasetBuildAction->setStatusTip("Open collected images for Image Set review.");
    addDisabledAction(datasetMenu, "Import Images", "DatasetImportImagesAction",
                      "Dataset workflows are placeholder-only in this shell step.");
    auto datasetCaptureFromCameraAction = datasetMenu->addAction("Capture From Camera");
    nameAction(datasetCaptureFromCameraAction, "DatasetCaptureFromCameraAction");
    datasetCaptureFromCameraAction->setStatusTip("Start a live Image Set capture session from the camera stream.");
    auto datasetLabelDatasetAction = datasetMenu->addAction("Review Image Set");
    datasetLabelDatasetAction->setStatusTip(
        "Open the Image Set review workspace. Image lists can save reviewed labels.");
    auto datasetReadinessAction = datasetMenu->addAction("Readiness Check");

    auto trainingMenu = this->menuBar()->addMenu("&Training");
    auto trainingEnvironmentSettingsAction = trainingMenu->addAction("Open Trainer Setup");
    auto trainingValidateEnvironmentAction = trainingMenu->addAction("Check Python Setup");
    auto trainingNewRunAction =
        addDisabledAction(trainingMenu, "New Training Run", "TrainingNewRunAction",
                          "Full GUI-launched training is intentionally unavailable in this readiness prototype.");
    addDisabledAction(trainingMenu, "Open Training Output", "TrainingOpenOutputAction",
                      "Trainer outputs are not wired in this shell step.");

    auto validationMenu = new QMenu(this);
    validationMenu->setTitle("Model Testing");
    auto imageValidationAction = validationMenu->addAction("Test Model");
    imageValidationAction->setStatusTip("Open the Model Testing workspace for image-level model checks.");
    auto sequenceValidationAction = addDisabledAction(
        validationMenu, "Sequence Testing", "ValidationSequenceValidationAction",
        "Runner-wrapped sequence testing is not available; artifact comparison remains internal/provisional.");
    addDisabledAction(validationMenu, "Compare Models", "ValidationCompareModelsAction",
                      "Model comparison is not wired in this shell step.");
    addDisabledAction(validationMenu, "Export Test Report", "ValidationExportReportAction",
                      "Model testing reports are not wired in this shell step.");

    auto sortingMenu = this->menuBar()->addMenu("&Sorting");
    auto startSortingAction = sortingMenu->addAction("Start Sorting");
    auto stopSortingAction = sortingMenu->addAction("Stop Sorting");
    auto triggerDisabledAction = addDisabledAction(sortingMenu, "Trigger Disabled", "SortingTriggerDisabledAction",
                                                   "DAQ trigger output is disabled until manually armed.");
    auto armTriggerAction = addDisabledAction(sortingMenu, "Arm Trigger", "SortingArmTriggerAction",
                                              "Trigger arming is not introduced in this declutter pass.");
    auto manualTriggerAction = sortingMenu->addAction("Manual Trigger");
    manualTriggerAction->setStatusTip("Fires the configured DAQ waveform from Live View when hardware is available.");
    auto openRunFolderAction = sortingMenu->addAction("Open Run Folder");

    auto viewMenu = this->menuBar()->addMenu("&View");
    auto showLogsAction = viewMenu->addAction("Debug Log");
    auto showDiagnosticsAction = viewMenu->addAction("Diagnostics");
    viewMenu->addSeparator();
    auto resetLayoutAction = viewMenu->addAction("Reset Layout");

    auto settingsMenu = this->menuBar()->addMenu("&Settings");
    addDisabledAction(settingsMenu, "Preferences", "SettingsPreferencesAction",
                      "Preferences are placeholder-only in this shell step.");
    addDisabledAction(settingsMenu, "Paths", "SettingsPathsAction",
                      "Path settings are still controlled by the existing runtime fields.");
    addDisabledAction(settingsMenu, "Hardware Configuration", "SettingsHardwareConfigurationAction",
                      "Hardware settings are still controlled by the existing runtime fields.");

    auto toolsMenu = this->menuBar()->addMenu("&Tools");
    auto systemDiagnosticsAction = toolsMenu->addAction("System Diagnostics");
    addDisabledAction(toolsMenu, "Model Artifact Verification", "ToolsModelArtifactVerificationAction",
                      "Model verification is not wired in this shell step.");
    addDisabledAction(toolsMenu, "Image List Check", "ToolsDatasetManifestVerificationAction",
                      "Image list checks are not wired in this shell step.");

    auto helpMenu = this->menuBar()->addMenu("&Help");
    auto aboutAction = helpMenu->addAction("&About");
    auto documentationAction = helpMenu->addAction("&Documentation");

    nameWidget(this->menuBar(), "MainMenuBar");
    nameObject(fileMenu, "FileMenu");
    nameObject(cameraMenu, "CameraMenu");
    nameObject(datasetMenu, "DatasetMenu");
    nameObject(trainingMenu, "TrainingMenu");
    nameObject(validationMenu, "ValidationMenu");
    nameAction(imageValidationAction, "ValidationImageValidationAction");
    nameAction(sequenceValidationAction, "ValidationSequenceValidationAction");
    nameObject(sortingMenu, "SortingMenu");
    nameObject(viewMenu, "ViewMenu");
    nameObject(settingsMenu, "SettingsMenu");
    nameObject(toolsMenu, "ToolsMenu");
    nameObject(helpMenu, "HelpMenu");
    nameAction(openViewerAction, "FileOpenViewerAction");
    nameAction(openOutputAction, "FileOpenOutputFolderAction");
    nameAction(exitAction, "FileExitAction");
    nameAction(reconnectAction, "CameraReconnectAction");
    nameAction(startPreviewAction, "CameraStartPreviewAction");
    nameAction(stopPreviewAction, "CameraStopPreviewAction");
    nameAction(captureStillAction, "CameraCaptureStillAction");
    nameAction(datasetOpenAction, "DatasetOpenDatasetAction");
    nameAction(datasetBuildAction, "DatasetBuildDatasetAction");
    nameAction(datasetLabelDatasetAction, "DatasetLabelDatasetAction");
    nameAction(datasetReadinessAction, "DatasetReadinessCheckAction");
    nameAction(trainingEnvironmentSettingsAction, "TrainingEnvironmentSettingsAction");
    nameAction(trainingValidateEnvironmentAction, "TrainingValidateEnvironmentAction");
    nameAction(trainingNewRunAction, "TrainingNewRunAction");
    nameAction(startSortingAction, "SortingStartLiveAction");
    nameAction(stopSortingAction, "SortingStopLiveAction");
    nameAction(triggerDisabledAction, "SortingTriggerDisabledAction");
    nameAction(armTriggerAction, "SortingArmTriggerAction");
    nameAction(manualTriggerAction, "SortingForceTriggerAction");
    nameAction(openRunFolderAction, "SortingOpenRunFolderAction");
    nameAction(showLogsAction, "ViewShowLogsDockAction");
    nameAction(showDiagnosticsAction, "ViewShowDiagnosticsDockAction");
    nameAction(resetLayoutAction, "ViewResetLayoutAction");
    nameAction(systemDiagnosticsAction, "ToolsSystemDiagnosticsAction");
    nameAction(aboutAction, "HelpAboutAction");
    nameAction(documentationAction, "HelpDocumentationAction");

    auto commandStrip = new QToolBar("Command Strip", this);
    commandStrip->setObjectName("CommandStrip");
    commandStrip->setMovable(false);
    commandStrip->setIconSize(QSize(16, 16));
    commandStrip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    commandStrip->addAction(reconnectAction);
    commandStrip->addAction(startPreviewAction);
    commandStrip->addAction(stopPreviewAction);
    commandStrip->addSeparator();
    commandStrip->addAction(startSortingAction);
    commandStrip->addAction(stopSortingAction);
    commandStrip->addSeparator();
    commandStrip->addAction(triggerDisabledAction);
    commandStrip->addAction(armTriggerAction);
    commandStrip->addAction(manualTriggerAction);
    commandStrip->addSeparator();
    commandStrip->addAction(captureStillAction);
    commandStrip->addAction(openViewerAction);
    this->addToolBar(Qt::TopToolBarArea, commandStrip);

    auto displayStrip = new QToolBar("Display Tools", this);
    displayStrip->setObjectName("DisplayToolsStrip");
    displayStrip->setMovable(false);
    displayStrip->setIconSize(QSize(16, 16));
    displayStrip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto copyFrameAction = displayStrip->addAction("Copy");
    auto copyDocumentAction = displayStrip->addAction("Copy Doc");
    displayStrip->addSeparator();
    auto fitAction = displayStrip->addAction("Fit");
    auto oneToOneAction = displayStrip->addAction("1x");
    auto zoomInAction = displayStrip->addAction("Zoom +");
    auto zoomOutAction = displayStrip->addAction("Zoom -");
    displayStrip->addSeparator();
    auto imageRegionAction = displayStrip->addAction("Image Region");
    imageRegionAction->setCheckable(true);
    auto crosshairAction = displayStrip->addAction("Crosshair");
    crosshairAction->setCheckable(true);
    displayStrip->addSeparator();
    auto calibrationAction = displayStrip->addAction("Calibration");
    nameAction(copyFrameAction, "DisplayCopyFrameAction");
    nameAction(copyDocumentAction, "DisplayCopyDocumentAction");
    nameAction(fitAction, "DisplayFitAction");
    nameAction(oneToOneAction, "DisplayOneToOneAction");
    nameAction(zoomInAction, "DisplayZoomInAction");
    nameAction(zoomOutAction, "DisplayZoomOutAction");
    nameAction(imageRegionAction, "DisplayImageRegionAction");
    nameAction(crosshairAction, "DisplayCrosshairAction");
    nameAction(calibrationAction, "DisplayCalibrationAction");
    for (auto* action : {copyFrameAction, copyDocumentAction, fitAction, oneToOneAction, zoomInAction, zoomOutAction,
                         imageRegionAction, crosshairAction, calibrationAction}) {
        action->setStatusTip("Display shell control; runtime behavior is unchanged in this alignment step.");
    }
    fitAction->setStatusTip("Fit the live image inside the available viewer area.");
    crosshairAction->setStatusTip("Show or hide the Live center crosshair.");
    this->addToolBarBreak(Qt::TopToolBarArea);
    this->addToolBar(Qt::TopToolBarArea, displayStrip);
    commandStrip->hide();
    displayStrip->hide();

    // Settings controls
    auto presetCombo = new QComboBox;
    presetCombo->addItem("2304 x 2304", QVariant::fromValue(QSize(2304, 2304)));
    presetCombo->addItem("2304 x 1152", QVariant::fromValue(QSize(2304, 1152)));
    presetCombo->addItem("2304 x 576", QVariant::fromValue(QSize(2304, 576)));
    presetCombo->addItem("2304 x 288", QVariant::fromValue(QSize(2304, 288)));
    presetCombo->addItem("2304 x 144", QVariant::fromValue(QSize(2304, 144)));
    presetCombo->addItem("2304 x 72", QVariant::fromValue(QSize(2304, 72)));
    presetCombo->addItem("2304 x 36", QVariant::fromValue(QSize(2304, 36)));
    presetCombo->addItem("2304 x 16", QVariant::fromValue(QSize(2304, 16)));
    presetCombo->addItem("2304 x 8", QVariant::fromValue(QSize(2304, 8)));
    presetCombo->addItem("2304 x 4", QVariant::fromValue(QSize(2304, 4)));
    presetCombo->addItem("1152 x 1152", QVariant::fromValue(QSize(1152, 1152)));
    presetCombo->addItem("1152 x 576", QVariant::fromValue(QSize(1152, 576)));
    presetCombo->addItem("1152 x 288", QVariant::fromValue(QSize(1152, 288)));
    presetCombo->addItem("1152 x 144", QVariant::fromValue(QSize(1152, 144)));
    presetCombo->addItem("576 x 576", QVariant::fromValue(QSize(576, 576)));
    presetCombo->addItem("576 x 288", QVariant::fromValue(QSize(576, 288)));
    presetCombo->addItem("576 x 144", QVariant::fromValue(QSize(576, 144)));
    presetCombo->addItem("288 x 288", QVariant::fromValue(QSize(288, 288)));
    presetCombo->addItem("288 x 144", QVariant::fromValue(QSize(288, 144)));
    presetCombo->addItem("144 x 144", QVariant::fromValue(QSize(144, 144)));
    presetCombo->addItem("Custom", QVariant::fromValue(QSize(-1, -1)));

    auto customWidthSpin = new QSpinBox;
    customWidthSpin->setRange(1, 4096);
    customWidthSpin->setValue(2304);
    auto customHeightSpin = new QSpinBox;
    customHeightSpin->setRange(1, 4096);
    customHeightSpin->setValue(2304);
    presetCombo->addItem("512 x 128", QVariant::fromValue(QSize(512, 128)));
    presetCombo->addItem("512 x 64", QVariant::fromValue(QSize(512, 64)));
    presetCombo->addItem("256 x 64", QVariant::fromValue(QSize(256, 64)));
    presetCombo->addItem("256 x 32", QVariant::fromValue(QSize(256, 32)));

    auto binCombo = new QComboBox;
    binCombo->addItems({"1", "2", "4"});
    binCombo->setCurrentIndex(0);

    auto bitsCombo = new QComboBox;
    bitsCombo->addItem("8", 8);
    bitsCombo->addItem("12", 12);
    bitsCombo->addItem("16", 16);
    bitsCombo->setCurrentIndex(0); // default 8-bit

    auto lutMinSpin = new QSpinBox;
    auto lutMaxSpin = new QSpinBox;
    auto lutMinSlider = new QSlider(Qt::Horizontal);
    auto lutMaxSlider = new QSlider(Qt::Horizontal);
    auto lutRangeLabel = new QLabel("Scale: 0 - 255");
    lutMinSpin->setRange(0, 255);
    lutMaxSpin->setRange(0, 255);
    lutMinSpin->setValue(0);
    lutMaxSpin->setValue(255);
    lutMinSlider->setRange(0, 255);
    lutMaxSlider->setRange(0, 255);
    lutMinSlider->setValue(0);
    lutMaxSlider->setValue(255);
    lutMinSlider->setTickPosition(QSlider::TicksBelow);
    lutMaxSlider->setTickPosition(QSlider::TicksBelow);

    auto exposureSpin = new QDoubleSpinBox;
    exposureSpin->setSuffix(" ms");
    exposureSpin->setDecimals(3);
    exposureSpin->setSingleStep(0.1);
    exposureSpin->setMinimum(0.01);
    exposureSpin->setMaximum(10000.0);
    exposureSpin->setValue(10.0);
    auto autoExposureBtn = new QPushButton("Auto Exposure");
    autoExposureBtn->setToolTip("Adjust exposure from the current live frame.");

    auto readoutCombo = new QComboBox;
    readoutCombo->addItem("Fast", DCAMPROP_READOUTSPEED__FASTEST);
    readoutCombo->setCurrentIndex(0);
    readoutCombo->setEnabled(false);
    readoutCombo->setToolTip("Readout is fixed to Fast for live camera use.");

    auto lutAutoSetBtn = new QPushButton("Auto Set");
    lutAutoSetBtn->setToolTip("Set black and white display range from the current frame.");

    auto logCheck = new QCheckBox("Enable logging (session_log.txt)");
    logCheck->setChecked(true);

    // Save controls
    QString defaultSaveDir = defaultWorkspacePaths.collections.isEmpty() ? defaultWorkspacePaths.root
                                                                         : defaultWorkspacePaths.collections;
    auto savePathEdit = new QLineEdit(defaultSaveDir);
    auto saveBrowseBtn = new QPushButton("...");
    auto saveOpenBtn = new QPushButton("Open Folder");
    auto saveStartBtn = new QPushButton("Start Save");
    auto saveStopBtn = new QPushButton("Stop Save");
    saveStopBtn->setEnabled(false);
    auto captureBtn = new QPushButton("Capture Frame");
    auto saveInfoLabel = new QLabel("Elapsed: 0.0 s\nFrames: 0");
    QDialog* savingDialog = nullptr;
    QLabel* savingDialogLabel = nullptr;
    QProgressBar* savingProgress = nullptr;

    auto displayEverySpin = new QSpinBox;
    displayEverySpin->setMinimum(1);
    displayEverySpin->setMaximum(1000);
    displayEverySpin->setValue(1);

    auto controlLayout = new QVBoxLayout;

    auto grid = new QGridLayout;
    grid->addWidget(new QLabel("Preset"), 0, 0);
    grid->addWidget(presetCombo, 0, 1);
    grid->addWidget(new QLabel("Custom W/H"), 1, 0);
    auto customLayout = new QHBoxLayout;
    customLayout->addWidget(customWidthSpin);
    customLayout->addWidget(customHeightSpin);
    grid->addLayout(customLayout, 1, 1);
    grid->addWidget(new QLabel("Binning"), 2, 0);
    grid->addWidget(binCombo, 2, 1);
    grid->addWidget(new QLabel("Bits"), 5, 0);
    grid->addWidget(bitsCombo, 5, 1);
    grid->addWidget(new QLabel("Exposure (ms)"), 6, 0);
    grid->addWidget(exposureSpin, 6, 1);
    grid->addWidget(new QLabel("Readout speed"), 7, 0);
    grid->addWidget(readoutCombo, 7, 1);
    grid->addWidget(logCheck, 9, 0, 1, 2);
    // Pipeline defaults (fast event detection)
    FastEventConfig pipelineDetectCfg;
    pipelineDetectCfg.bgFrames = 100;
    pipelineDetectCfg.bgUpdateFrames = 50;
    pipelineDetectCfg.resetFrames = 2;
    pipelineDetectCfg.minArea = -1.0;
    pipelineDetectCfg.minAreaFrac = 0.0;
    pipelineDetectCfg.maxAreaFrac = 0.10;
    pipelineDetectCfg.minBbox = 32;
    pipelineDetectCfg.margin = 5;
    pipelineDetectCfg.diffThresh = 15;
    pipelineDetectCfg.blurRadius = 1;
    pipelineDetectCfg.morphRadius = 1;
    pipelineDetectCfg.scale = 0.5;
    pipelineDetectCfg.gapFireShift = 0;

    // Pipeline controls (event detection + ONNX + DAQ)
    auto pipelineEnableCheck = new QCheckBox("Enable pipeline");
    pipelineEnableCheck->setChecked(false);
    auto pipelineStatusLabel = new QLabel("Pipeline: not loaded");
    pipelineStatusLabel->setWordWrap(true);

    auto onnxEdit = new QLineEdit;
    auto onnxBrowseBtn = new QPushButton("...");
    auto metaEdit = new QLineEdit;
    auto metaBrowseBtn = new QPushButton("...");
    auto outputEdit = new QLineEdit;
    auto outputBrowseBtn = new QPushButton("...");
    auto liveModelCombo = new QComboBox;
    liveModelCombo->setEditable(false);
    liveModelCombo->setMinimumContentsLength(32);
    liveModelCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto openLiveModelManagerBtn = new QPushButton("Models");
    openLiveModelManagerBtn->setText("...");
    auto refreshLiveModelsBtn = new QPushButton("Refresh Models");
    auto liveModelSummaryText = new QTextEdit;
    liveModelSummaryText->setReadOnly(true);
    liveModelSummaryText->setMaximumHeight(116);
    liveModelSummaryText->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto targetClassCombo = new QComboBox;
    targetClassCombo->setEditable(false);
    targetClassCombo->setMinimumContentsLength(12);
    targetClassCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto saveCropCheck = new QCheckBox("Save crops");
    auto sortNonTargetCheck = new QCheckBox("Sort Non-target");
    sortNonTargetCheck->setToolTip("When checked, sorting triggers on every class except the selected target class.");
    auto saveOverlayCheck = new QCheckBox("Save overlays");
    auto liveConfigureSettingsBtn = new QPushButton("Configure");
    nameWidget(liveConfigureSettingsBtn, "LiveConfigureSettingsButton");
    for (auto* check : {saveCropCheck, sortNonTargetCheck, saveOverlayCheck}) {
        check->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    }
    auto datasetCaptureModeCombo = new QComboBox;
    datasetCaptureModeCombo->addItems({"mixed", "hit-only", "waste-only"});
    auto datasetBatchTargetSpin = new QSpinBox;
    datasetBatchTargetSpin->setRange(1, 100000);
    datasetBatchTargetSpin->setValue(100);
    auto datasetStartCaptureBtn = new QPushButton("Start Dataset Capture");
    auto datasetStopCaptureBtn = new QPushButton("Stop and Review");
    datasetStopCaptureBtn->setEnabled(false);
    auto datasetCaptureStatusLabel = new QLabel("Image Set capture: idle");
    datasetCaptureStatusLabel->setWordWrap(true);
    auto loadPipelineBtn = new QPushButton("Load Pipeline");
    auto computeDeviceCombo = new QComboBox;
    auto normalizeComputeDevice = [](QString value) {
        value = value.trimmed().toLower();
        if (value == "gpu" || value == "cuda")
            return QStringLiteral("cuda");
        if (value == "cpu")
            return QStringLiteral("cpu");
        return QStringLiteral("auto");
    };
    auto selectedComputeDevice = [computeDeviceCombo, normalizeComputeDevice]() {
        const QVariant data = computeDeviceCombo ? computeDeviceCombo->currentData() : QVariant();
        if (data.isValid())
            return normalizeComputeDevice(data.toString());
        QSettings settings;
        return normalizeComputeDevice(
            settings.value("settings/computeDevice", settings.value("validator/device", "auto")).toString());
    };
    auto persistComputeDevice = [normalizeComputeDevice](const QString& value) {
        QSettings settings;
        const QString normalized = normalizeComputeDevice(value);
        settings.setValue("settings/computeDevice", normalized);
        settings.setValue("validator/device", normalized);
        settings.sync();
    };
    {
        QSettings settings;
        persistComputeDevice(
            settings.value("settings/computeDevice", settings.value("validator/device", "auto")).toString());
    }
    auto withComputeDeviceArg = [selectedComputeDevice](QStringList args) {
        const int existing = args.indexOf("--device");
        if (existing >= 0) {
            if (existing + 1 < args.size())
                args[existing + 1] = selectedComputeDevice();
            else
                args << selectedComputeDevice();
        } else {
            args << "--device" << selectedComputeDevice();
        }
        return args;
    };

    auto frameSkipSpin = new QSpinBox;
    frameSkipSpin->setRange(0, 1000);
    frameSkipSpin->setValue(0);

    auto daqDeviceCombo = new QComboBox;
    daqDeviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    auto daqChannelEdit = new QLineEdit("Dev1/ao0");
    auto amplitudeSpin = new QDoubleSpinBox;
    amplitudeSpin->setDecimals(3);
    amplitudeSpin->setRange(0.0, 10.0);
    amplitudeSpin->setValue(5.0);
    amplitudeSpin->setSuffix(" V");
    auto freqSpin = new QDoubleSpinBox;
    freqSpin->setDecimals(3);
    freqSpin->setRange(0.001, 200.0);
    freqSpin->setValue(10.0);
    freqSpin->setSuffix(" kHz");
    auto durationSpin = new QDoubleSpinBox;
    durationSpin->setDecimals(3);
    durationSpin->setRange(0.1, 10000.0);
    durationSpin->setValue(5.0);
    durationSpin->setSuffix(" ms");
    auto delaySpin = new QDoubleSpinBox;
    delaySpin->setDecimals(3);
    delaySpin->setRange(0.0, 10000.0);
    delaySpin->setValue(0.0);
    delaySpin->setSuffix(" ms");

    auto pipelineLayout = new QGridLayout;
    int row = 0;
    pipelineLayout->addWidget(pipelineEnableCheck, row++, 0, 1, 4);
    pipelineLayout->addWidget(new QLabel("Live model"), row, 0);
    pipelineLayout->addWidget(liveModelCombo, row, 1, 1, 3);
    row++;
    pipelineLayout->addWidget(openLiveModelManagerBtn, row, 1);
    pipelineLayout->addWidget(refreshLiveModelsBtn, row++, 2);
    pipelineLayout->addWidget(new QLabel("Selected model"), row, 0);
    pipelineLayout->addWidget(liveModelSummaryText, row++, 1, 1, 3);
    pipelineLayout->addWidget(new QLabel("Model file"), row, 0);
    pipelineLayout->addWidget(onnxEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(onnxBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Model details"), row, 0);
    pipelineLayout->addWidget(metaEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(metaBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Output dir"), row, 0);
    pipelineLayout->addWidget(outputEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(outputBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Target class"), row, 0);
    pipelineLayout->addWidget(targetClassCombo, row++, 1, 1, 3);
    pipelineLayout->addWidget(saveCropCheck, row, 0);
    pipelineLayout->addWidget(sortNonTargetCheck, row, 1);
    pipelineLayout->addWidget(saveOverlayCheck, row++, 2, 1, 2);
    pipelineLayout->addWidget(new QLabel("Dataset capture"), row, 0);
    pipelineLayout->addWidget(datasetCaptureModeCombo, row, 1);
    pipelineLayout->addWidget(datasetBatchTargetSpin, row, 2);
    pipelineLayout->addWidget(datasetStartCaptureBtn, row++, 3);
    pipelineLayout->addWidget(datasetStopCaptureBtn, row, 1);
    pipelineLayout->addWidget(datasetCaptureStatusLabel, row++, 2, 1, 2);
    pipelineLayout->addWidget(new QLabel("Frame skip"), row, 0);
    pipelineLayout->addWidget(frameSkipSpin, row++, 1, 1, 2);
    pipelineLayout->addWidget(loadPipelineBtn, row++, 0, 1, 2);
    pipelineLayout->addWidget(pipelineStatusLabel, row++, 0, 1, 4);

    auto pipelineWidget = new QWidget;
    pipelineWidget->setLayout(pipelineLayout);

    auto labviewStatusDot = new QLabel;
    labviewStatusDot->setFixedSize(14, 14);
    labviewStatusDot->setStyleSheet("background:#666;border-radius:7px;border:1px solid #94A3B8;");
    auto labviewStatusText = new QLabel("Disconnected");
    auto labviewStatusRow = new QHBoxLayout;
    labviewStatusRow->setContentsMargins(0, 0, 0, 0);
    labviewStatusRow->addWidget(labviewStatusDot);
    labviewStatusRow->addWidget(labviewStatusText, 1);

    auto labviewOutputLabel = new QLabel("Output: --");
    labviewOutputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    auto labviewLayout = new QGridLayout;
    int labRow = 0;
    labviewLayout->addWidget(new QLabel("Status"), labRow, 0);
    labviewLayout->addLayout(labviewStatusRow, labRow++, 1, 1, 2);
    labviewLayout->addWidget(labviewOutputLabel, labRow++, 0, 1, 3);
    labviewLayout->addWidget(new QLabel("Output range"), labRow, 0);
    labviewLayout->addWidget(new QLabel("-10 V to +10 V"), labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("AO channel"), labRow, 0);
    labviewLayout->addWidget(daqChannelEdit, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Amplitude"), labRow, 0);
    labviewLayout->addWidget(amplitudeSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Frequency (kHz)"), labRow, 0);
    labviewLayout->addWidget(freqSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Duration"), labRow, 0);
    labviewLayout->addWidget(durationSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Delay"), labRow, 0);
    labviewLayout->addWidget(delaySpin, labRow++, 1, 1, 2);
    auto labviewTestBtn = new QPushButton("Manual Trigger");
    labviewTestBtn->setEnabled(false);
    labviewTestBtn->setVisible(false);
    labviewTestBtn->setToolTip("Internal DAQ trigger endpoint; use Live View Manual Trigger.");
    labviewLayout->addWidget(labviewTestBtn, labRow++, 0, 1, 2);
    auto labviewReconnectBtn = new QPushButton("Reconnect DAQ");
    labviewLayout->addWidget(labviewReconnectBtn, labRow++, 0, 1, 2);

    auto labviewWidget = new QWidget;
    labviewWidget->setLayout(labviewLayout);

    auto bgFramesSpin = new QSpinBox;
    bgFramesSpin->setRange(1, 10000);
    bgFramesSpin->setValue(pipelineDetectCfg.bgFrames);
    bgFramesSpin->setSuffix(" frames");
    auto bgUpdateSpin = new QSpinBox;
    bgUpdateSpin->setRange(0, 10000);
    bgUpdateSpin->setValue(pipelineDetectCfg.bgUpdateFrames);
    bgUpdateSpin->setSuffix(" frames");
    auto resetFramesSpin = new QSpinBox;
    resetFramesSpin->setRange(1, 1000);
    resetFramesSpin->setValue(pipelineDetectCfg.resetFrames);
    resetFramesSpin->setSuffix(" frames");
    auto minAreaSpin = new QDoubleSpinBox;
    minAreaSpin->setDecimals(1);
    minAreaSpin->setRange(-1.0, 1e9);
    minAreaSpin->setValue(pipelineDetectCfg.minArea);
    minAreaSpin->setSuffix(" px^2");
    minAreaSpin->setToolTip("Minimum detected pixel area. Use -1 for the automatic detector default.");
    auto minAreaFracSpin = new QDoubleSpinBox;
    minAreaFracSpin->setDecimals(4);
    minAreaFracSpin->setRange(0.0, 1.0);
    minAreaFracSpin->setSingleStep(0.001);
    minAreaFracSpin->setValue(pipelineDetectCfg.minAreaFrac);
    auto maxAreaFracSpin = new QDoubleSpinBox;
    maxAreaFracSpin->setDecimals(4);
    maxAreaFracSpin->setRange(0.0, 1.0);
    maxAreaFracSpin->setSingleStep(0.001);
    maxAreaFracSpin->setValue(pipelineDetectCfg.maxAreaFrac);
    maxAreaFracSpin->setToolTip("Maximum detected object area as a fraction of the frame area.");
    auto minBboxSpin = new QSpinBox;
    minBboxSpin->setRange(1, 10000);
    minBboxSpin->setValue(pipelineDetectCfg.minBbox);
    minBboxSpin->setSuffix(" px");
    minBboxSpin->setToolTip(
        "Minimum bounding rectangle width and height. A detected object must be at least this many pixels wide and "
        "this many pixels high.");
    auto marginSpin = new QSpinBox;
    marginSpin->setRange(0, 10000);
    marginSpin->setValue(pipelineDetectCfg.margin);
    marginSpin->setSuffix(" px");
    auto diffThreshSpin = new QSpinBox;
    diffThreshSpin->setRange(0, 255);
    diffThreshSpin->setValue(pipelineDetectCfg.diffThresh);
    diffThreshSpin->setToolTip("Grayscale difference threshold, 0-255.");
    auto blurRadiusSpin = new QSpinBox;
    blurRadiusSpin->setRange(0, 25);
    blurRadiusSpin->setValue(pipelineDetectCfg.blurRadius);
    blurRadiusSpin->setSuffix(" px");
    auto morphRadiusSpin = new QSpinBox;
    morphRadiusSpin->setRange(0, 25);
    morphRadiusSpin->setValue(pipelineDetectCfg.morphRadius);
    morphRadiusSpin->setSuffix(" px");
    auto scaleSpin = new QDoubleSpinBox;
    scaleSpin->setDecimals(3);
    scaleSpin->setRange(0.05, 1.0);
    scaleSpin->setSingleStep(0.05);
    scaleSpin->setValue(pipelineDetectCfg.scale);
    auto gapFireSpin = new QSpinBox;
    gapFireSpin->setRange(0, 10000);
    gapFireSpin->setValue(pipelineDetectCfg.gapFireShift);
    gapFireSpin->setSuffix(" px");

    auto detectLayout = new QGridLayout;
    int detRow = 0;
    detectLayout->addWidget(new QLabel("Background frames"), detRow, 0);
    detectLayout->addWidget(bgFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("BG update frames"), detRow, 0);
    detectLayout->addWidget(bgUpdateSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Reset frames"), detRow, 0);
    detectLayout->addWidget(resetFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area px^2 (-1=auto)"), detRow, 0);
    detectLayout->addWidget(minAreaSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area frac"), detRow, 0);
    detectLayout->addWidget(minAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Max area frame fraction"), detRow, 0);
    detectLayout->addWidget(maxAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min rectangle size"), detRow, 0);
    detectLayout->addWidget(minBboxSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Margin px"), detRow, 0);
    detectLayout->addWidget(marginSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Diff threshold 0-255"), detRow, 0);
    detectLayout->addWidget(diffThreshSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Blur radius px"), detRow, 0);
    detectLayout->addWidget(blurRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Morph radius px"), detRow, 0);
    detectLayout->addWidget(morphRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Scale"), detRow, 0);
    detectLayout->addWidget(scaleSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Gap fire shift px"), detRow, 0);
    detectLayout->addWidget(gapFireSpin, detRow++, 1);
    auto detectWidget = new QWidget;
    detectWidget->setLayout(detectLayout);
    std::function<void()> scheduleDetectorApply = []() {};

    auto statsEventsLabel = new QLabel("Events: 0");
    auto statsClassLabel = new QLabel("Classes:\n(none)");
    auto statsHitLabel = new QLabel("Classified Sort: 0\nClassified Pass: 0\nWent to Sort: 0\nWent to Pass: 0");
    auto statsLastLabel = new QLabel("Last event: --");
    statsEventsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsClassLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsHitLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsLastLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsClassLabel->setWordWrap(true);
    statsLastLabel->setWordWrap(true);
    auto statsResetBtn = new QPushButton("Reset Stats");
    auto statsShowBtn = new QPushButton("Show Figures");

    auto statsLayout = new QVBoxLayout;
    statsLayout->addWidget(statsEventsLabel);
    statsLayout->addWidget(statsHitLabel);
    statsLayout->addWidget(statsLastLabel);
    statsLayout->addWidget(statsClassLabel, 1);
    statsLayout->addWidget(statsShowBtn);
    statsLayout->addWidget(statsResetBtn);
    auto statsWidget = new QWidget;
    statsWidget->setLayout(statsLayout);

    auto seqFolderEdit = new QLineEdit;
    seqFolderEdit->setPlaceholderText("Select recorded sequence folder...");
    auto seqBrowseBtn = new QPushButton("Browse");
    auto seqLoadBtn = new QPushButton("Load into memory");
    auto seqStartBtn = new QPushButton("Run Recorded Sequence");
    seqStartBtn->setToolTip("Replay recorded frames through the detector/classifier with DAQ output disabled.");
    seqStartBtn->setProperty("daqOutputMode", "disabled-for-replay");
    auto seqStopBtn = new QPushButton("Stop Replay");
    seqStartBtn->setEnabled(false);
    seqStopBtn->setEnabled(false);

    auto seqFpsSpin = new QDoubleSpinBox;
    seqFpsSpin->setDecimals(2);
    seqFpsSpin->setRange(0.1, 100000.0);
    seqFpsSpin->setValue(500.0);

    auto seqStatusLabel = new QLabel("No sequence loaded.");
    seqStatusLabel->setWordWrap(true);
    auto seqLogLabel = new QLabel("Log: (none)");
    seqLogLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    auto seqLayout = new QGridLayout;
    int seqRow = 0;
    seqLayout->addWidget(new QLabel("Folder"), seqRow, 0);
    seqLayout->addWidget(seqFolderEdit, seqRow, 1, 1, 2);
    seqLayout->addWidget(seqBrowseBtn, seqRow++, 3);
    seqLayout->addWidget(new QLabel("FPS"), seqRow, 0);
    seqLayout->addWidget(seqFpsSpin, seqRow++, 1, 1, 2);
    seqLayout->addWidget(seqLoadBtn, seqRow, 0, 1, 2);
    seqLayout->addWidget(seqStartBtn, seqRow, 2);
    seqLayout->addWidget(seqStopBtn, seqRow++, 3);
    seqLayout->addWidget(seqStatusLabel, seqRow++, 0, 1, 4);
    seqLayout->addWidget(seqLogLabel, seqRow++, 0, 1, 4);
    seqLayout->setColumnStretch(1, 1);
    seqLayout->setColumnStretch(2, 1);
    auto seqWidget = new QWidget;
    seqWidget->setLayout(seqLayout);

    QSettings trainerPythonSettings;
    const QString initialTrainerPython =
        resolvedTrainerPythonExecutable(trainerPythonSettings.value("settings/pythonTrainer").toString(),
                                        selectedComputeDevice());
    if (QFileInfo(initialTrainerPython).isFile()) {
        trainerPythonSettings.setValue("settings/pythonTrainer", initialTrainerPython);
        trainerPythonSettings.sync();
    }
    auto trainerPythonEdit = new QLineEdit(QDir::toNativeSeparators(initialTrainerPython));
    auto trainerPythonBrowseBtn = new QPushButton("Browse");
    auto trainerStartingModelCombo = new QComboBox;
    trainerStartingModelCombo->setMinimumContentsLength(24);
    trainerStartingModelCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto trainerTrainingModeCombo = new QComboBox;
    trainerTrainingModeCombo->addItem("Start a new trained copy", "new_copy");
    trainerTrainingModeCombo->hide();
    auto trainerStartingModelHintLabel = new QLabel;
    trainerStartingModelHintLabel->setWordWrap(true);
    trainerStartingModelHintLabel->setProperty("mutedText", true);
    auto trainerDatasetEdit = new QLineEdit;
    trainerDatasetEdit->setPlaceholderText("Choose the training dataset file (.json)...");
    auto trainerDatasetBrowseBtn = new QPushButton("Browse");
    auto trainerOutputEdit = new QLineEdit;
    trainerOutputEdit->setPlaceholderText("Choose where to save the new trained model...");
    auto trainerOutputBrowseBtn = new QPushButton("Browse");
    for (auto* edit : {trainerPythonEdit, trainerDatasetEdit, trainerOutputEdit}) {
        edit->setMinimumWidth(0);
        edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    auto trainerEnvCheckBtn = new QPushButton("Check Python setup");
    auto trainerConfigurePathBtn = new QPushButton("Open app settings");
    trainerConfigurePathBtn->setFlat(true);
    trainerConfigurePathBtn->setCursor(Qt::PointingHandCursor);
    auto trainerCancelBtn = new QPushButton("Cancel");
    trainerCancelBtn->setEnabled(false);
    auto trainerStartTrainingBtn = new QPushButton("Train model");
    auto trainerDryRunBtn = new QPushButton("Check setup");
    auto trainerStatusLabel = new QLabel("Setup not ready yet.");
    trainerStatusLabel->setWordWrap(true);
    trainerStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto trainerResultText = new QPlainTextEdit;
    trainerResultText->setReadOnly(true);
    trainerResultText->setMinimumHeight(210);
    trainerResultText->setPlainText("Detailed command output appears here.");
    auto trainerProgressBar = new QProgressBar;
    trainerProgressBar->setRange(0, 100);
    trainerProgressBar->setValue(0);
    trainerProgressBar->setTextVisible(true);
    trainerProgressBar->setFormat("Ready");

    auto trainerPathsLayout = new QGridLayout;
    trainerPathsLayout->setColumnStretch(0, 0);
    trainerPathsLayout->setColumnStretch(1, 1);
    trainerPathsLayout->setColumnStretch(2, 0);
    trainerPathsLayout->setColumnStretch(3, 0);
    int trainerRow = 0;
    trainerPathsLayout->addWidget(new QLabel("Starting model"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerStartingModelCombo, trainerRow++, 1, 1, 3);
    trainerPathsLayout->addWidget(new QLabel("Dataset"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerDatasetEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerDatasetBrowseBtn, trainerRow++, 3);
    auto trainerDeviceCombo = new QComboBox;
    trainerDeviceCombo->addItem("Auto", "auto");
    trainerDeviceCombo->addItem("CPU", "cpu");
    trainerDeviceCombo->addItem("GPU", "cuda");
    const int initialTrainerDeviceIndex = trainerDeviceCombo->findData(selectedComputeDevice());
    trainerDeviceCombo->setCurrentIndex(initialTrainerDeviceIndex >= 0 ? initialTrainerDeviceIndex : 0);
    nameWidget(trainerDeviceCombo, "TrainerSetupDeviceComboBox");
    trainerPathsLayout->addWidget(new QLabel("Device"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerDeviceCombo, trainerRow++, 1, 1, 3);
    trainerOutputEdit->hide();
    trainerOutputBrowseBtn->hide();
    auto trainerPathsGroup = new QGroupBox;
    trainerPathsGroup->setMinimumWidth(0);
    trainerPathsGroup->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    trainerPathsGroup->setLayout(trainerPathsLayout);

    auto trainerActionsLayout = new QHBoxLayout;
    trainerActionsLayout->addWidget(trainerEnvCheckBtn);
    trainerActionsLayout->addStretch(1);

    auto makeTrainerSectionToggle = [&](const QString& label, bool expanded) {
        auto* button = new QToolButton;
        button->setText(label);
        button->setCheckable(true);
        button->setChecked(expanded);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        return button;
    };

    auto trainerEnvironmentPanel = new QFrame;
    trainerEnvironmentPanel->setProperty("panel", true);
    auto trainerEnvironmentLayout = new QVBoxLayout;
    trainerEnvironmentLayout->setContentsMargins(12, 12, 12, 12);
    trainerEnvironmentLayout->setSpacing(10);
    auto trainerEnvironmentTitle = new QLabel("SETUP DETAILS");
    trainerEnvironmentTitle->setProperty("panelTitle", true);
    auto trainerEnvironmentSubtitle =
        new QLabel("Current Python, helper, package, and device status for model training.");
    trainerEnvironmentSubtitle->setProperty("mutedText", true);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentTitle);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentSubtitle);
    auto trainerPythonLayout = new QGridLayout;
    trainerPythonLayout->setColumnStretch(1, 1);
    trainerPythonLayout->addWidget(new QLabel("Python setup"), 0, 0);
    trainerPythonLayout->addWidget(trainerPythonEdit, 0, 1);
    trainerPythonLayout->addWidget(trainerPythonBrowseBtn, 0, 2);
    trainerEnvironmentLayout->addLayout(trainerPythonLayout);
    auto* trainerPythonStatusValue = new QLabel;
    auto* trainerHelperStatusValue = new QLabel;
    auto* trainerDeviceStatusValue = new QLabel;
    auto* trainerPackagesStatusValue = new QLabel;
    auto* trainerEnvCheckStatusValue = new QLabel("Env check: not run this session");
    const QVector<QPair<QString, QLabel*>> trainerCheckRows = {
        {"Python executable", trainerPythonStatusValue},
        {"Training helper", trainerHelperStatusValue},
        {"Compute device", trainerDeviceStatusValue},
        {"Packages", trainerPackagesStatusValue},
        {"Env check", trainerEnvCheckStatusValue},
    };
    for (const auto& row : trainerCheckRows) {
        auto* checkRow = new QFrame;
        checkRow->setProperty("trainerCheckRow", true);
        auto* checkLayout = new QHBoxLayout;
        checkLayout->setContentsMargins(8, 6, 8, 6);
        checkLayout->setSpacing(8);
        auto* dot = new QLabel("!");
        dot->setProperty("statusDot", true);
        auto* label = new QLabel(row.first);
        label->setProperty("panelTitle", true);
        auto* value = row.second;
        value->setProperty("mutedText", true);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        checkLayout->addWidget(dot);
        checkLayout->addWidget(label, 1);
        checkLayout->addWidget(value, 2);
        checkRow->setLayout(checkLayout);
        trainerEnvironmentLayout->addWidget(checkRow);
    }
    trainerActionsLayout->addWidget(trainerConfigurePathBtn);
    trainerEnvironmentLayout->addLayout(trainerActionsLayout);
    auto trainerLastCheckedLabel = new QLabel("Last checked: this session only");
    trainerLastCheckedLabel->setProperty("mutedText", true);
    trainerEnvironmentLayout->addWidget(trainerLastCheckedLabel, 0, Qt::AlignRight);
    trainerEnvironmentPanel->setLayout(trainerEnvironmentLayout);
    auto trainerSetupDetailsToggle = makeTrainerSectionToggle("Show setup details", false);
    trainerEnvironmentPanel->setVisible(false);
    QObject::connect(trainerSetupDetailsToggle, &QToolButton::toggled, trainerEnvironmentPanel,
                     [trainerSetupDetailsToggle, trainerEnvironmentPanel](bool checked) {
                         trainerEnvironmentPanel->setVisible(checked);
                         trainerSetupDetailsToggle->setText(checked ? "Hide setup details" : "Show setup details");
                         trainerSetupDetailsToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
                     });

    auto trainerFormPanel = new QFrame;
    trainerFormPanel->setProperty("panel", true);
    auto trainerFormLayout = new QVBoxLayout;
    trainerFormLayout->setContentsMargins(12, 12, 12, 12);
    trainerFormLayout->setSpacing(10);
    auto trainerFormTitle = new QLabel("SETUP");
    trainerFormTitle->setProperty("panelTitle", true);
    trainerFormLayout->addWidget(trainerFormTitle);
    trainerFormLayout->addWidget(trainerPathsGroup);
    auto trainerHyperGrid = new QGridLayout;
    trainerHyperGrid->setHorizontalSpacing(10);
    trainerHyperGrid->setVerticalSpacing(8);
    auto trainerArchitectureCombo = new QComboBox;
    trainerArchitectureCombo->addItem("MobileNetV3-Small", "mobilenet_v3_small");
    trainerArchitectureCombo->addItem("EfficientNet-B0", "efficientnet_b0");
    auto trainerPretrainedGroup = new QButtonGroup(this);
    trainerPretrainedGroup->setExclusive(true);
    auto trainerPretrainedImageNetBtn = new QPushButton("Recommended start");
    auto trainerPretrainedNoneBtn = new QPushButton("Blank start");
    for (auto* button : {trainerPretrainedImageNetBtn, trainerPretrainedNoneBtn}) {
        button->setCheckable(true);
        button->setMinimumHeight(28);
    }
    trainerPretrainedImageNetBtn->setChecked(true);
    trainerPretrainedGroup->addButton(trainerPretrainedImageNetBtn, 1);
    trainerPretrainedGroup->addButton(trainerPretrainedNoneBtn, 0);
    auto trainerPretrainedSegment = new QWidget;
    auto trainerPretrainedLayout = new QHBoxLayout;
    trainerPretrainedLayout->setContentsMargins(0, 0, 0, 0);
    trainerPretrainedLayout->setSpacing(2);
    trainerPretrainedLayout->addWidget(trainerPretrainedImageNetBtn);
    trainerPretrainedLayout->addWidget(trainerPretrainedNoneBtn);
    trainerPretrainedSegment->setLayout(trainerPretrainedLayout);
    auto trainerEpochsSpin = new QSpinBox;
    trainerEpochsSpin->setRange(1, 500);
    trainerEpochsSpin->setValue(50);
    auto trainerBatchSpin = new QSpinBox;
    trainerBatchSpin->setRange(1, 256);
    trainerBatchSpin->setValue(64);
    auto trainerLrSpin = new QDoubleSpinBox;
    trainerLrSpin->setDecimals(5);
    trainerLrSpin->setRange(0.0001, 1.0);
    trainerLrSpin->setValue(0.001);
    auto trainerSelectedArchitectureValue = new QLabel("From selected model");
    trainerSelectedArchitectureValue->setProperty("mutedText", true);
    auto trainerHyperparameterJsonEdit = new QPlainTextEdit;
    trainerHyperparameterJsonEdit->setMinimumHeight(250);
    trainerHyperparameterJsonEdit->setMaximumHeight(340);
    trainerHyperparameterJsonEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    trainerHyperparameterJsonEdit->setToolTip(
        "Versioned training settings consumed directly by the OpenDSS trainer. Architecture and device are derived separately.");
    QString hyperparameterSchemaPath = findPackagedAppPath("models/trainer_hyperparameters.schema.json");
    QFile hyperparameterSchemaFile(hyperparameterSchemaPath);
    if (hyperparameterSchemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument schemaDocument = QJsonDocument::fromJson(hyperparameterSchemaFile.readAll());
        trainerHyperparameterJsonEdit->setPlainText(
            QString::fromUtf8(QJsonDocument(schemaDocument.object().value("editable_defaults").toObject())
                                  .toJson(QJsonDocument::Indented)));
    }
    auto addTrainerFormCell = [&](int row, int column, const QString& labelText, QWidget* editor) {
        auto* label = new QLabel(labelText);
        label->setProperty("metricLabel", true);
        trainerHyperGrid->addWidget(label, row, column);
        trainerHyperGrid->addWidget(editor, row, column + 1);
    };
    trainerArchitectureCombo->hide();
    trainerPretrainedSegment->hide();
    auto* trainerEpochsLabel = new QLabel("Training rounds");
    trainerEpochsLabel->setProperty("metricLabel", true);
    auto* trainerBatchLabel = new QLabel("Batch size");
    trainerBatchLabel->setProperty("metricLabel", true);
    auto* trainerLrLabel = new QLabel("Learning rate");
    trainerLrLabel->setProperty("metricLabel", true);
    trainerEpochsSpin->hide();
    trainerBatchSpin->hide();
    trainerLrSpin->hide();
    trainerEpochsLabel->hide();
    trainerBatchLabel->hide();
    trainerLrLabel->hide();
    addTrainerFormCell(0, 0, "Architecture", trainerSelectedArchitectureValue);
    trainerHyperGrid->addWidget(new QLabel("Editable versioned JSON"), 1, 0, 1, 4);
    trainerHyperGrid->addWidget(trainerHyperparameterJsonEdit, 2, 0, 1, 4);
    auto trainerAdvancedBasicsPanel = new QWidget;
    trainerAdvancedBasicsPanel->setLayout(trainerHyperGrid);
    auto trainerLaunchRow = new QHBoxLayout;
    trainerLaunchRow->addWidget(trainerStartTrainingBtn);
    trainerLaunchRow->addWidget(trainerDryRunBtn);
    trainerLaunchRow->addWidget(trainerCancelBtn);
    trainerLaunchRow->addStretch(1);
    trainerFormLayout->addLayout(trainerLaunchRow);
    trainerFormPanel->setLayout(trainerFormLayout);
    trainerFormPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto trainerLogPanel = new QFrame;
    trainerLogPanel->setProperty("panel", true);
    auto trainerLogLayout = new QVBoxLayout;
    trainerLogLayout->setContentsMargins(12, 12, 12, 12);
    trainerLogLayout->setSpacing(10);
    auto trainerLogTitle = new QLabel("STATUS");
    trainerLogTitle->setProperty("panelTitle", true);
    auto trainerLogToggle = makeTrainerSectionToggle("Show detailed log", false);
    trainerResultText->setVisible(false);
    QObject::connect(trainerLogToggle, &QToolButton::toggled, trainerResultText,
                     [trainerLogToggle, trainerResultText](bool checked) {
                         trainerResultText->setVisible(checked);
                         trainerLogToggle->setText(checked ? "Hide detailed log" : "Show detailed log");
                         trainerLogToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
                     });
    trainerLogLayout->addWidget(trainerLogTitle);
    trainerLogLayout->addWidget(trainerStatusLabel);
    trainerLogLayout->addWidget(trainerProgressBar);
    trainerLogLayout->addWidget(trainerLogToggle);
    trainerLogLayout->addWidget(trainerResultText, 1);
    trainerLogPanel->setLayout(trainerLogLayout);

    auto trainerResultsPanel = new QFrame;
    trainerResultsPanel->setProperty("panel", true);
    auto trainerResultsLayout = new QVBoxLayout;
    trainerResultsLayout->setContentsMargins(12, 12, 12, 12);
    trainerResultsLayout->setSpacing(10);
    auto trainerResultsTitle = new QLabel("RESULTS");
    trainerResultsTitle->setProperty("panelTitle", true);
    trainerResultsLayout->addWidget(trainerResultsTitle);
    auto trainerResultMetrics = new QGridLayout;
    const QStringList trainerMetricNames = {"Accuracy", "Macro F1", "Best epoch", "Duration"};
    QVector<QLabel*> trainerResultMetricValues;
    for (int column = 0; column < trainerMetricNames.size(); ++column) {
        auto* value = new QLabel("--");
        trainerResultMetricValues.push_back(value);
        value->setProperty("metricValue", true);
        nameWidget(value, QString("TrainerResultMetric%1").arg(column).toUtf8().constData());
        auto* label = new QLabel(trainerMetricNames.at(column));
        label->setProperty("metricLabel", true);
        trainerResultMetrics->addWidget(value, 0, column);
        trainerResultMetrics->addWidget(label, 1, column);
    }
    trainerResultsLayout->addLayout(trainerResultMetrics);
    const auto makeTrainingPlot = [](const QString& title, const QString& objectName) {
        auto* frame = new QFrame;
        frame->setProperty("panel", true);
        frame->setObjectName(objectName);
        frame->setMinimumHeight(220);
        frame->setMaximumHeight(260);
        auto* layout = new QVBoxLayout;
        layout->setContentsMargins(10, 8, 10, 8);
        auto* heading = new QLabel(title);
        heading->setProperty("metricLabel", true);
        auto* empty = new QLabel;
        empty->setObjectName(objectName + "Values");
        empty->setAlignment(Qt::AlignCenter);
        empty->setProperty("mutedText", true);
        frame->setProperty("xAxisLabel", "Epoch");
        frame->setProperty("yAxisLabel", objectName.contains("Loss") ? "Loss" : "Score");
        frame->setProperty("gridVisible", true);
        frame->setProperty("legendVisible", true);
        renderTrainerCurves(empty, {}, {}, objectName.contains("Loss") ? QColor(42, 124, 201) : QColor(38, 151, 96),
                            objectName.contains("Loss") ? QColor(222, 118, 42) : QColor(112, 83, 196));
        layout->addWidget(heading);
        layout->addWidget(empty, 1);
        frame->setLayout(layout);
        return frame;
    };
    trainerResultsLayout->addWidget(makeTrainingPlot("Training and validation loss", "TrainerLossCurve"));
    trainerResultsLayout->addWidget(makeTrainingPlot("Validation accuracy and Macro F1", "TrainerPerformanceCurve"));
    auto trainerSavedModelLabel = new QLabel;
    trainerSavedModelLabel->setProperty("mutedText", true);
    nameWidget(trainerSavedModelLabel, "TrainerSavedModelLabel");
    auto trainerUseModelButton = new QPushButton("Use Model");
    nameWidget(trainerUseModelButton, "TrainerUseModelButton");
    trainerUseModelButton->setEnabled(false);
    auto trainerResultActions = new QHBoxLayout;
    trainerResultActions->addWidget(trainerSavedModelLabel, 1);
    trainerResultActions->addWidget(trainerUseModelButton);
    trainerResultsLayout->addLayout(trainerResultActions);
    trainerResultsPanel->setLayout(trainerResultsLayout);
    trainerResultsPanel->setObjectName("TrainerResultsPanel");

    auto trainerAdvancedPanel = new QGroupBox("More settings");
    auto trainerAdvancedLayout = new QVBoxLayout;
    auto trainerFlipCheck = new QCheckBox("Random horizontal flip");
    trainerFlipCheck->setChecked(true);
    auto trainerRotationCheck = new QCheckBox("Random rotation +/-15 deg");
    trainerRotationCheck->setChecked(true);
    auto trainerColorJitterCheck = new QCheckBox("Color changes");
    auto trainerRandomCropCheck = new QCheckBox("Random crop");
    trainerRandomCropCheck->setChecked(true);
    auto trainerSchedulerCombo = new QComboBox;
    trainerSchedulerCombo->addItems({"StepLR", "CosineAnnealing", "None"});
    trainerAdvancedLayout->addWidget(trainerFlipCheck);
    trainerAdvancedLayout->addWidget(trainerRotationCheck);
    trainerAdvancedLayout->addWidget(trainerColorJitterCheck);
    trainerAdvancedLayout->addWidget(trainerRandomCropCheck);
    trainerAdvancedLayout->addWidget(new QLabel("Learning schedule"));
    trainerAdvancedLayout->addWidget(trainerSchedulerCombo);
    trainerAdvancedPanel->setLayout(trainerAdvancedLayout);
    auto trainerSettingsBody = new QWidget;
    auto trainerSettingsLayout = new QVBoxLayout;
    trainerSettingsLayout->setContentsMargins(12, 8, 12, 8);
    trainerSettingsLayout->setSpacing(10);
    trainerAdvancedBasicsPanel->setVisible(true);
    trainerSettingsLayout->addWidget(trainerAdvancedBasicsPanel);
    trainerSettingsLayout->addWidget(trainerAdvancedPanel);
    trainerSettingsBody->setLayout(trainerSettingsLayout);
    trainerSettingsBody->setVisible(false);
    auto trainerSettingsToggle = makeTrainerSectionToggle("Hyperparameter Settings", false);
    auto trainerAdvancedToggle = trainerSettingsToggle;
    QObject::connect(trainerSettingsToggle, &QToolButton::toggled, trainerSettingsBody,
                     [trainerSettingsToggle, trainerSettingsBody](bool checked) {
                         trainerSettingsBody->setVisible(checked);
                         trainerSettingsToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
                     });
    auto trainerRecentRunsPanel = new QFrame;
    trainerRecentRunsPanel->hide();

    auto trainerWidget = new QWidget;
    auto trainerLayout = new QHBoxLayout;
    trainerLayout->setContentsMargins(16, 16, 16, 16);
    trainerLayout->setSpacing(12);
    auto trainerLeftScroll = new QScrollArea;
    trainerLeftScroll->setObjectName("TrainerWorkspaceLeftScrollArea");
    trainerLeftScroll->setWidgetResizable(true);
    trainerLeftScroll->setFrameShape(QFrame::NoFrame);
    trainerLeftScroll->setMinimumWidth(520);
    trainerLeftScroll->setMaximumWidth(980);
    trainerLeftScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto trainerLeftContent = new QWidget;
    trainerLeftContent->setMinimumWidth(0);
    trainerLeftContent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto trainerLeftLayout = new QVBoxLayout;
    trainerLeftLayout->setContentsMargins(0, 0, 0, 0);
    trainerLeftLayout->setSpacing(12);
    trainerLeftLayout->addWidget(trainerFormPanel);
    trainerLeftLayout->addWidget(trainerLogPanel);
    trainerLeftLayout->addWidget(trainerSettingsToggle);
    trainerLeftLayout->addWidget(trainerSettingsBody);
    trainerLeftLayout->addWidget(trainerResultsPanel);
    trainerLeftContent->setLayout(trainerLeftLayout);
    trainerLeftScroll->setWidget(trainerLeftContent);
    trainerLayout->addWidget(trainerLeftScroll, 1);
    trainerLayout->addStretch(1);
    trainerWidget->setLayout(trainerLayout);

    auto trainerDockProxy = new QWidget;
    auto trainerDockProxyLayout = new QVBoxLayout;
    trainerDockProxyLayout->setContentsMargins(12, 12, 12, 12);
    auto trainerDockProxyLabel =
        new QLabel("Use the main Trainer workspace to check setup and train a model.");
    trainerDockProxyLabel->setWordWrap(true);
    auto trainerDockProxyButton = new QPushButton("Open Trainer workspace");
    nameWidget(trainerDockProxyButton, "TrainerDockOpenWorkspaceButton");
    trainerDockProxyLayout->addWidget(trainerDockProxyLabel);
    trainerDockProxyLayout->addWidget(trainerDockProxyButton);
    trainerDockProxyLayout->addStretch(1);
    trainerDockProxy->setLayout(trainerDockProxyLayout);

    nameWidget(presetCombo, "CameraPresetComboBox");
    nameWidget(customWidthSpin, "CameraCustomWidthSpinBox");
    nameWidget(customHeightSpin, "CameraCustomHeightSpinBox");
    nameWidget(binCombo, "CameraBinningComboBox");
    nameWidget(bitsCombo, "CameraBitsComboBox");
    nameWidget(exposureSpin, "CameraExposureSpinBox");
    nameWidget(autoExposureBtn, "CameraAutoExposureButton");
    nameWidget(readoutCombo, "CameraReadoutSpeedComboBox");
    nameWidget(displayEverySpin, "CameraDisplayEverySpinBox");
    nameWidget(lutMinSpin, "CameraLutMinSpinBox");
    nameWidget(lutMaxSpin, "CameraLutMaxSpinBox");
    nameWidget(lutAutoSetBtn, "CameraLutAutoSetButton");
    nameWidget(lutMinSlider, "CameraLutMinSlider");
    nameWidget(lutMaxSlider, "CameraLutMaxSlider");
    nameWidget(lutRangeLabel, "CameraLutRangeLabel");
    nameWidget(logCheck, "CameraLoggingCheckBox");
    nameWidget(savePathEdit, "SavePathEdit");
    nameWidget(saveBrowseBtn, "SaveBrowseButton");
    nameWidget(saveOpenBtn, "SaveOpenFolderButton");
    nameWidget(saveStartBtn, "SaveStartButton");
    nameWidget(saveStopBtn, "SaveStopButton");
    nameWidget(captureBtn, "SaveCaptureFrameButton");
    nameWidget(saveInfoLabel, "SaveInfoLabel");
    nameWidget(pipelineEnableCheck, "PipelineEnableCheckBox");
    nameWidget(pipelineStatusLabel, "PipelineStatusLabel");
    nameWidget(onnxEdit, "PipelineOnnxPathEdit");
    nameWidget(onnxBrowseBtn, "PipelineOnnxBrowseButton");
    nameWidget(metaEdit, "PipelineMetadataPathEdit");
    nameWidget(metaBrowseBtn, "PipelineMetadataBrowseButton");
    nameWidget(outputEdit, "PipelineOutputDirEdit");
    nameWidget(outputBrowseBtn, "PipelineOutputBrowseButton");
    nameWidget(liveModelCombo, "LiveModelSelectionComboBox");
    nameWidget(openLiveModelManagerBtn, "LiveModelOpenModelManagerButton");
    nameWidget(refreshLiveModelsBtn, "LiveModelRefreshButton");
    nameWidget(liveModelSummaryText, "LiveModelSummaryText");
    nameWidget(targetClassCombo, "PipelineTargetClassComboBox");
    nameWidget(frameSkipSpin, "PipelineFrameSkipSpinBox");
    nameWidget(saveCropCheck, "PipelineSaveCropsCheckBox");
    nameWidget(sortNonTargetCheck, "PipelineSortNonTargetCheckBox");
    nameWidget(saveOverlayCheck, "PipelineSaveOverlaysCheckBox");
    nameWidget(datasetCaptureModeCombo, "DatasetCaptureModeComboBox");
    nameWidget(datasetBatchTargetSpin, "DatasetCaptureBatchTargetSpinBox");
    nameWidget(datasetStartCaptureBtn, "DatasetCaptureStartButton");
    nameWidget(datasetStopCaptureBtn, "DatasetCaptureStopReviewButton");
    nameWidget(datasetCaptureStatusLabel, "DatasetCaptureStatusLabel");
    nameWidget(loadPipelineBtn, "PipelineLoadButton");
    nameWidget(computeDeviceCombo, "SettingsWorkspaceComputeDeviceComboBox");
    nameWidget(pipelineWidget, "PipelineConfigTab");
    nameWidget(labviewStatusDot, "DaqStatusDot");
    nameWidget(labviewStatusText, "DaqStatusTextLabel");
    nameWidget(labviewOutputLabel, "DaqOutputLabel");
    nameWidget(daqDeviceCombo, "DaqDeviceComboBox");
    nameWidget(daqChannelEdit, "DaqChannelEdit");
    nameWidget(amplitudeSpin, "DaqAmplitudeSpinBox");
    nameWidget(freqSpin, "DaqFrequencySpinBox");
    nameWidget(durationSpin, "DaqDurationSpinBox");
    nameWidget(delaySpin, "DaqDelaySpinBox");
    nameWidget(labviewTestBtn, "DaqManualTriggerButton");
    nameWidget(labviewReconnectBtn, "DaqReconnectButton");
    nameWidget(labviewWidget, "LabviewTab");
    nameWidget(bgFramesSpin, "DetectorBackgroundFramesSpinBox");
    nameWidget(bgUpdateSpin, "DetectorBackgroundUpdateFramesSpinBox");
    nameWidget(resetFramesSpin, "DetectorResetFramesSpinBox");
    nameWidget(minAreaSpin, "DetectorMinAreaSpinBox");
    nameWidget(minAreaFracSpin, "DetectorMinAreaFractionSpinBox");
    nameWidget(maxAreaFracSpin, "DetectorMaxAreaFractionSpinBox");
    nameWidget(minBboxSpin, "DetectorMinBboxSpinBox");
    nameWidget(marginSpin, "DetectorMarginSpinBox");
    nameWidget(diffThreshSpin, "DetectorDiffThresholdSpinBox");
    nameWidget(blurRadiusSpin, "DetectorBlurRadiusSpinBox");
    nameWidget(morphRadiusSpin, "DetectorMorphRadiusSpinBox");
    nameWidget(scaleSpin, "DetectorScaleSpinBox");
    nameWidget(gapFireSpin, "DetectorGapFireShiftSpinBox");
    nameWidget(detectWidget, "EventDetectionTab");
    nameWidget(statsEventsLabel, "StatsEventsLabel");
    nameWidget(statsClassLabel, "StatsClassCountsLabel");
    nameWidget(statsHitLabel, "StatsHitWasteLabel");
    nameWidget(statsLastLabel, "StatsLastEventLabel");
    nameWidget(statsShowBtn, "StatsShowFiguresButton");
    nameWidget(statsResetBtn, "StatsResetButton");
    nameWidget(statsWidget, "StatsTab");
    nameWidget(seqFolderEdit, "SequenceFolderEdit");
    nameWidget(seqBrowseBtn, "SequenceBrowseButton");
    nameWidget(seqLoadBtn, "SequenceLoadButton");
    nameWidget(seqStartBtn, "SequenceStartTestButton");
    nameWidget(seqStopBtn, "SequenceStopButton");
    nameWidget(seqFpsSpin, "SequenceFpsSpinBox");
    nameWidget(seqStatusLabel, "SequenceStatusLabel");
    nameWidget(seqLogLabel, "SequenceLogLabel");
    nameWidget(seqWidget, "SequenceTestTab");
    nameWidget(trainerWidget, "TrainerReadinessTab");
    nameWidget(trainerPathsGroup, "TrainerReadinessInputsGroup");
    nameWidget(trainerPythonEdit, "TrainerPythonPathEdit");
    nameWidget(trainerPythonBrowseBtn, "TrainerPythonBrowseButton");
    nameWidget(trainerStartingModelCombo, "TrainerStartingModelCombo");
    nameWidget(trainerTrainingModeCombo, "TrainerTrainingModeCombo");
    nameWidget(trainerStartingModelHintLabel, "TrainerStartingModelHintLabel");
    nameWidget(trainerDatasetEdit, "TrainerDatasetPathEdit");
    nameWidget(trainerDatasetBrowseBtn, "TrainerDatasetBrowseButton");
    nameWidget(trainerOutputEdit, "TrainerOutputDirEdit");
    nameWidget(trainerOutputBrowseBtn, "TrainerOutputBrowseButton");
    nameWidget(trainerEnvCheckBtn, "TrainerEnvCheckButton");
    nameWidget(trainerConfigurePathBtn, "TrainerConfigurePathButton");
    nameWidget(trainerCancelBtn, "TrainerCancelCheckButton");
    nameWidget(trainerStartTrainingBtn, "TrainerStartTrainingButton");
    nameWidget(trainerDryRunBtn, "TrainerDryRunButton");
    nameWidget(trainerStatusLabel, "TrainerReadinessStatusLabel");
    nameWidget(trainerResultText, "TrainerReadinessResultText");
    nameWidget(trainerProgressBar, "TrainerWorkspaceProgressBar");
    nameWidget(trainerSetupDetailsToggle, "TrainerSetupDetailsToggleButton");
    nameWidget(trainerAdvancedToggle, "TrainerAdvancedSetupToggleButton");
    nameWidget(trainerLogToggle, "TrainerDetailedLogToggleButton");
    nameWidget(trainerEnvironmentPanel, "TrainerEnvironmentPanel");
    nameWidget(trainerPythonStatusValue, "TrainerSetupPythonStatusLabel");
    nameWidget(trainerHelperStatusValue, "TrainerSetupHelperStatusLabel");
    nameWidget(trainerDeviceStatusValue, "TrainerSetupDeviceStatusLabel");
    nameWidget(trainerPackagesStatusValue, "TrainerSetupPackagesStatusLabel");
    nameWidget(trainerEnvCheckStatusValue, "TrainerSetupEnvCheckStatusLabel");
    nameWidget(trainerFormPanel, "TrainerRunTrainingPanel");
    nameWidget(trainerLogPanel, "TrainerProgressLogPanel");
    nameWidget(trainerArchitectureCombo, "TrainerArchitectureCombo");
    nameWidget(trainerPretrainedSegment, "TrainerPretrainedSegmentedControl");
    nameWidget(trainerPretrainedImageNetBtn, "TrainerPretrainedImageNetButton");
    nameWidget(trainerPretrainedNoneBtn, "TrainerPretrainedNoneButton");
    nameWidget(trainerEpochsSpin, "TrainerEpochsSpinBox");
    nameWidget(trainerBatchSpin, "TrainerBatchSizeSpinBox");
    nameWidget(trainerLrSpin, "TrainerLearningRateSpinBox");
    nameWidget(trainerSelectedArchitectureValue, "TrainerSelectedArchitectureValue");
    nameWidget(trainerHyperparameterJsonEdit, "TrainerHyperparameterJsonEdit");
    nameWidget(trainerRecentRunsPanel, "TrainerRecentRunsPanel");
    nameWidget(trainerAdvancedPanel, "TrainerAdvancedAugmentationSchedulerGroup");
    nameWidget(trainerFlipCheck, "TrainerRandomHorizontalFlipCheckBox");
    nameWidget(trainerRotationCheck, "TrainerRandomRotationCheckBox");
    nameWidget(trainerColorJitterCheck, "TrainerColorJitterCheckBox");
    nameWidget(trainerRandomCropCheck, "TrainerRandomCropCheckBox");
    nameWidget(trainerSchedulerCombo, "TrainerSchedulerCombo");
    auto runStateGroup = new QGroupBox("Run State");
    nameWidget(runStateGroup, "RunStateGroup");
    auto runStateLayout = new QVBoxLayout;
    runStateLayout->addWidget(statusLabel);
    runStateLayout->addWidget(pipelineStatusLabel);
    runStateGroup->setLayout(runStateLayout);

    auto liveMetricsGroup = new QGroupBox("Live Metrics");
    nameWidget(liveMetricsGroup, "LiveMetricsGroup");
    auto liveMetricsLayout = new QVBoxLayout;
    liveMetricsLayout->addWidget(statsLabel);
    liveMetricsLayout->addWidget(statsEventsLabel);
    liveMetricsLayout->addWidget(statsHitLabel);
    liveMetricsLayout->addWidget(statsLastLabel);
    liveMetricsGroup->setLayout(liveMetricsLayout);

    auto currentConfigGroup = new QGroupBox("Current Configuration");
    nameWidget(currentConfigGroup, "CurrentConfigurationGroup");
    auto currentConfigLayout = new QVBoxLayout;
    auto modelSummaryLabel = new QLabel("Model/target: configure in Analysis");
    auto cameraSummaryLabel = new QLabel("Camera preset: configure in Devices");
    auto outputSummaryLabel = new QLabel("Output folder: configure in Analysis or Capture");
    auto triggerSummaryLabel = new QLabel("Trigger: disabled");
    for (auto* item : {modelSummaryLabel, cameraSummaryLabel, outputSummaryLabel, triggerSummaryLabel}) {
        item->setWordWrap(true);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        currentConfigLayout->addWidget(item);
    }
    currentConfigGroup->setLayout(currentConfigLayout);

    auto blockersGroup = new QGroupBox("Blockers");
    nameWidget(blockersGroup, "BlockersGroup");
    auto blockersLayout = new QVBoxLayout;
    auto blockersLabel =
        new QLabel("Camera, model, and sorting hardware readiness appear here when startup or run actions are blocked.");
    blockersLabel->setWordWrap(true);
    blockersLayout->addWidget(blockersLabel);
    blockersGroup->setLayout(blockersLayout);

    controlLayout->addWidget(runStateGroup);
    controlLayout->addWidget(liveMetricsGroup);
    controlLayout->addWidget(currentConfigGroup);
    controlLayout->addWidget(blockersGroup);
    controlLayout->addStretch(1);

    auto rightWidget = new QWidget;
    nameWidget(rightWidget, "RuntimePanel");
    rightWidget->setLayout(controlLayout);
    rightWidget->setMinimumWidth(320);

    auto zoomStatusLabel = new QLabel("Zoom 100%");
    auto scaleStatusLabel = new QLabel("SF: 1.000 Px");
    auto profileStatusLabel = new QLabel("Default");
    nameWidget(zoomStatusLabel, "ImageZoomStatusLabel");
    nameWidget(scaleStatusLabel, "ImageScaleFactorStatusLabel");
    nameWidget(profileStatusLabel, "ImageProfileStatusLabel");
    for (auto* item : {zoomStatusLabel, scaleStatusLabel, profileStatusLabel}) {
        item->setFrameStyle(QFrame::NoFrame);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }

    auto imageStatusStrip = new QHBoxLayout;
    nameObject(imageStatusStrip, "ImageStatusStrip");
    imageStatusStrip->setContentsMargins(8, 3, 8, 3);
    imageStatusStrip->setSpacing(8);
    imageStatusStrip->addWidget(new QLabel("Zoom"));
    imageStatusStrip->addWidget(zoomStatusLabel);
    imageStatusStrip->addSpacing(8);
    imageStatusStrip->addWidget(scaleStatusLabel);
    imageStatusStrip->addWidget(profileStatusLabel);
    imageStatusStrip->addStretch(1);

    auto imageOverlayStatusFrame = new QFrame;
    nameWidget(imageOverlayStatusFrame, "LiveImageOverlayStatusStrip");
    imageOverlayStatusFrame->setLayout(imageStatusStrip);

    auto liveViewerStack = new QFrame;
    nameWidget(liveViewerStack, "LiveViewerStack");
    QPixmap viewerPattern(36, 36);
    viewerPattern.fill(QColor("#0A0A0A"));
    {
        QPainter painter(&viewerPattern);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(QRect(0, 0, 36, 36), QColor("#0A0A0A"));
        painter.setPen(QPen(QColor(31, 35, 43, 150), 2));
        painter.drawLine(-8, 36, 36, -8);
        painter.drawLine(10, 46, 46, 10);
        painter.setPen(QPen(QColor(20, 184, 166, 28), 1));
        painter.drawLine(-12, 22, 22, -12);
        painter.drawLine(22, 48, 48, 22);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(125, 211, 252, 42));
        painter.drawEllipse(QPointF(18, 18), 1.3, 1.3);
    }
    liveViewerStack->setAutoFillBackground(true);
    QPalette viewerPalette = liveViewerStack->palette();
    viewerPalette.setBrush(QPalette::Window, QBrush(viewerPattern));
    liveViewerStack->setPalette(viewerPalette);
    auto liveViewerStackLayout = new QStackedLayout;
    liveViewerStackLayout->setStackingMode(QStackedLayout::StackAll);
    liveViewerStackLayout->setContentsMargins(0, 0, 0, 0);
    liveViewerStackLayout->addWidget(imageView);

    auto liveViewerOverlay = new QWidget;
    nameWidget(liveViewerOverlay, "LiveViewerHudOverlay");
    liveViewerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    auto liveViewerOverlayLayout = new QVBoxLayout;
    liveViewerOverlayLayout->setContentsMargins(12, 10, 12, 0);
    liveViewerOverlayLayout->setSpacing(0);

    auto liveHudTop = new QHBoxLayout;
    liveHudTop->setContentsMargins(0, 0, 0, 0);
    liveHudTop->setSpacing(8);
    auto liveHudResolution = new QLabel("RES -- x --\nCAM IDLE");
    auto liveHudFrameTime = new QLabel("EXP -- ms\nPROC -- ms");
    auto liveHudToolbar = new QFrame;
    auto liveHudFps = new QLabel("FPS --\nFRAME --\nDROP --");
    nameWidget(liveHudResolution, "LiveViewerHudResolutionLabel");
    nameWidget(liveHudFrameTime, "LiveViewerHudFrameTimeLabel");
    nameWidget(liveHudToolbar, "LiveViewerHudToolbar");
    nameWidget(liveHudFps, "LiveViewerHudFpsLabel");
    for (auto* item : {liveHudResolution, liveHudFrameTime, liveHudFps}) {
        item->setProperty("hudPill", true);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }
    liveHudToolbar->setProperty("hudPill", true);
    auto liveHudToolbarLayout = new QHBoxLayout;
    liveHudToolbarLayout->setContentsMargins(5, 3, 5, 3);
    liveHudToolbarLayout->setSpacing(2);
    auto addViewerTool = [&](const QString& tip, const QString& iconKey, bool checked = false) {
        auto* button = new QToolButton;
        button->setProperty("viewerTool", true);
        button->setIcon(makeBrandIcon(iconKey, QColor("#FFFFFF"), QColor("#7DD3FC")));
        button->setIconSize(QSize(14, 14));
        button->setToolTip(tip);
        button->setCheckable(checked);
        button->setChecked(checked);
        button->setAutoRaise(true);
        liveHudToolbarLayout->addWidget(button);
        return button;
    };
    auto liveFitTool = addViewerTool("Fit to View: fit the live image inside the viewer.", "fit");
    auto liveCrosshairTool = addViewerTool("Crosshair: show or hide the center reticle.", "crosshair", false);
    auto liveOpenViewerTool = addViewerTool("Open Viewer", "viewer");
    liveCrosshairTool->setCheckable(true);
    nameWidget(liveFitTool, "LiveViewerFitButton");
    nameWidget(liveCrosshairTool, "LiveViewerCrosshairToggle");
    nameWidget(liveOpenViewerTool, "LiveViewerOpenViewerButton");
    liveFitTool->setAccessibleName("Live Fit to View");
    liveCrosshairTool->setAccessibleName("Live Crosshair");
    liveOpenViewerTool->setAccessibleName("Open Viewer");
    QObject::connect(liveFitTool, &QToolButton::clicked, fitAction, &QAction::trigger);
    QObject::connect(liveOpenViewerTool, &QToolButton::clicked, openViewerAction, &QAction::trigger);
    liveHudToolbar->setLayout(liveHudToolbarLayout);
    liveHudTop->addWidget(liveHudResolution);
    liveHudTop->addWidget(liveHudFrameTime);
    liveHudTop->addStretch(1);
    liveHudTop->addWidget(liveHudToolbar, 0, Qt::AlignTop);
    liveHudTop->addStretch(1);
    liveHudTop->addWidget(liveHudFps, 0, Qt::AlignTop);
    liveViewerOverlayLayout->addLayout(liveHudTop);
    liveViewerOverlayLayout->addStretch(1);

    auto liveViewerEmpty = new QLabel("NO LIVE FRAMES  |  PRESS START");
    nameWidget(liveViewerEmpty, "LiveViewerEmptyState");
    liveViewerEmpty->setAlignment(Qt::AlignCenter);
    liveViewerOverlayLayout->addWidget(liveViewerEmpty, 0, Qt::AlignHCenter);

    auto cameraHudResolution = new QLabel("RES -- x --\nCAM IDLE");
    auto cameraHudFrameTime = new QLabel("EXP -- ms\nPROC -- ms");
    auto cameraHudFps = new QLabel("FPS --\nFRAME --\nDROP --");
    auto cameraViewerEmpty = new QLabel("NO LIVE FRAMES  |  PRESS START PREVIEW");
    nameWidget(cameraHudResolution, "CameraViewerHudResolutionLabel");
    nameWidget(cameraHudFrameTime, "CameraViewerHudFrameTimeLabel");
    nameWidget(cameraHudFps, "CameraViewerHudFpsLabel");
    nameWidget(cameraViewerEmpty, "CameraViewerEmptyState");
    cameraViewerEmpty->setAlignment(Qt::AlignCenter);
    auto liveRunBarSlot = new QWidget;
    nameWidget(liveRunBarSlot, "LiveRunControlBarSlot");
    liveRunBarSlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    liveRunBarSlot->setFixedHeight(0);
    auto liveRunBarSlotLayout = new QVBoxLayout;
    liveRunBarSlotLayout->setContentsMargins(12, 8, 12, 0);
    liveRunBarSlotLayout->setSpacing(0);
    liveRunBarSlot->setLayout(liveRunBarSlotLayout);
    liveViewerOverlayLayout->addWidget(liveRunBarSlot);
    liveViewerOverlayLayout->addSpacing(12);
    liveViewerOverlayLayout->addWidget(imageOverlayStatusFrame);
    liveViewerOverlay->setLayout(liveViewerOverlayLayout);
    auto* liveViewerWheelForwarder = new WheelEventForwarder(imageView->viewport(), liveViewerOverlay);
    liveViewerOverlay->installEventFilter(liveViewerWheelForwarder);
    for (QWidget* widget : liveViewerOverlay->findChildren<QWidget*>()) {
        widget->installEventFilter(liveViewerWheelForwarder);
    }
    liveViewerStackLayout->addWidget(liveViewerOverlay);

    auto liveCrosshairOverlay = new QWidget;
    nameWidget(liveCrosshairOverlay, "LiveViewerCrosshairOverlay");
    liveCrosshairOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    liveCrosshairOverlay->setVisible(liveCrosshairTool->isChecked());
    auto liveCrosshairGrid = new QGridLayout;
    liveCrosshairGrid->setContentsMargins(0, 0, 0, 0);
    liveCrosshairGrid->setSpacing(0);
    liveCrosshairGrid->setRowStretch(0, 1);
    liveCrosshairGrid->setRowStretch(2, 1);
    liveCrosshairGrid->setColumnStretch(0, 1);
    liveCrosshairGrid->setColumnStretch(2, 1);
    auto* crosshairVertical = new QFrame;
    nameWidget(crosshairVertical, "LiveViewerCrosshairVerticalLine");
    crosshairVertical->setFixedWidth(1);
    crosshairVertical->setStyleSheet("background:rgba(125,211,252,0.82);");
    auto* crosshairHorizontal = new QFrame;
    nameWidget(crosshairHorizontal, "LiveViewerCrosshairHorizontalLine");
    crosshairHorizontal->setFixedHeight(1);
    crosshairHorizontal->setStyleSheet("background:rgba(125,211,252,0.82);");
    liveCrosshairGrid->addWidget(crosshairVertical, 0, 1, 3, 1);
    liveCrosshairGrid->addWidget(crosshairHorizontal, 1, 0, 1, 3);
    liveCrosshairOverlay->setLayout(liveCrosshairGrid);
    liveViewerStackLayout->addWidget(liveCrosshairOverlay);
    QObject::connect(liveCrosshairTool, &QToolButton::toggled, crosshairAction, &QAction::setChecked);
    QObject::connect(crosshairAction, &QAction::toggled, liveCrosshairTool, &QToolButton::setChecked);
    QObject::connect(crosshairAction, &QAction::toggled, liveCrosshairOverlay, &QWidget::setVisible);
    crosshairAction->setChecked(liveCrosshairTool->isChecked());

    auto liveDetectorDrawer = new QFrame;
    nameWidget(liveDetectorDrawer, "LiveDetectorTuningDrawer");
    liveDetectorDrawer->setProperty("panel", true);
    liveDetectorDrawer->setFixedWidth(320);
    liveDetectorDrawer->setMinimumHeight(0);
    liveDetectorDrawer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    liveDetectorDrawer->setVisible(true);
    auto liveDetectorDrawerLayout = new QVBoxLayout;
    liveDetectorDrawerLayout->setContentsMargins(12, 10, 12, 12);
    liveDetectorDrawerLayout->setSpacing(8);
    auto liveDetectorHeader = new QHBoxLayout;
    liveDetectorHeader->setContentsMargins(0, 0, 0, 0);
    auto liveDetectorTitle = new QLabel("Detector settings");
    liveDetectorTitle->setProperty("panelTitle", true);
    auto liveDetectorClose = new QToolButton;
    nameWidget(liveDetectorClose, "LiveDetectorTuningCloseButton");
    liveDetectorClose->setText("x");
    liveDetectorClose->setAutoRaise(true);
    liveDetectorHeader->addWidget(liveDetectorTitle);
    liveDetectorHeader->addStretch(1);
    liveDetectorHeader->addWidget(liveDetectorClose);
    liveDetectorDrawerLayout->addLayout(liveDetectorHeader);
    auto liveDetectorBanner = new QLabel("Changes auto-apply after 250 ms. Live capture continues.");
    nameWidget(liveDetectorBanner, "LiveDetectorTuningDebounceLabel");
    liveDetectorBanner->setWordWrap(true);
    liveDetectorBanner->setProperty("mutedText", true);
    liveDetectorDrawerLayout->addWidget(liveDetectorBanner);
    auto liveDetectorGrid = new QGridLayout;
    liveDetectorGrid->setContentsMargins(0, 0, 0, 0);
    liveDetectorGrid->setHorizontalSpacing(8);
    liveDetectorGrid->setVerticalSpacing(6);
    int liveDetectorRow = 0;
    auto addLiveDetectorSpin = [&](const QString& label, QAbstractSpinBox* spin, const char* objectName) {
        auto* labelWidget = new QLabel(label);
        labelWidget->setProperty("mutedText", true);
        nameWidget(spin, objectName);
        if (!spin->toolTip().isEmpty())
            labelWidget->setToolTip(spin->toolTip());
        liveDetectorGrid->addWidget(labelWidget, liveDetectorRow, 0);
        liveDetectorGrid->addWidget(spin, liveDetectorRow++, 1);
    };
    auto makeLinkedIntSpin = [&](QSpinBox* source, int min, int max) {
        auto* spin = new QSpinBox;
        spin->setRange(min, max);
        spin->setValue(source->value());
        spin->setSuffix(source->suffix());
        spin->setToolTip(source->toolTip());
        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [=, &scheduleDetectorApply](int value) {
            if (source->value() != value)
                source->setValue(value);
            scheduleDetectorApply();
        });
        QObject::connect(source, qOverload<int>(&QSpinBox::valueChanged), spin, &QSpinBox::setValue);
        return spin;
    };
    auto makeLinkedDoubleSpin = [&](QDoubleSpinBox* source, double min, double max, int decimals, double step) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(min, max);
        spin->setDecimals(decimals);
        spin->setSingleStep(step);
        spin->setValue(source->value());
        spin->setSuffix(source->suffix());
        spin->setToolTip(source->toolTip());
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         [=, &scheduleDetectorApply](double value) {
                             if (!qFuzzyCompare(source->value() + 1.0, value + 1.0))
                                 source->setValue(value);
                             scheduleDetectorApply();
                         });
        QObject::connect(source, qOverload<double>(&QDoubleSpinBox::valueChanged), spin, &QDoubleSpinBox::setValue);
        return spin;
    };
    addLiveDetectorSpin("BG frames", makeLinkedIntSpin(bgFramesSpin, 1, 10000), "LiveDetectorBgFramesSpinBox");
    addLiveDetectorSpin("Diff threshold 0-255", makeLinkedIntSpin(diffThreshSpin, 0, 255),
                        "LiveDetectorDiffThresholdSpinBox");
    addLiveDetectorSpin("Min area (-1 auto)", makeLinkedDoubleSpin(minAreaSpin, -1.0, 1e9, 1, 1.0),
                        "LiveDetectorMinAreaSpinBox");
    addLiveDetectorSpin("Max area frame frac", makeLinkedDoubleSpin(maxAreaFracSpin, 0.0, 1.0, 4, 0.001),
                        "LiveDetectorMaxAreaSpinBox");
    addLiveDetectorSpin("Blur radius", makeLinkedIntSpin(blurRadiusSpin, 0, 25), "LiveDetectorBlurRadiusSpinBox");
    addLiveDetectorSpin("Min rectangle size", makeLinkedIntSpin(minBboxSpin, 1, 10000),
                        "LiveDetectorMinRectangleSizeSpinBox");
    liveDetectorDrawerLayout->addLayout(liveDetectorGrid);
    liveDetectorDrawerLayout->addStretch(1);
    liveDetectorDrawer->setLayout(liveDetectorDrawerLayout);
    auto liveDetectorDrawerOverlay = new QWidget;
    nameWidget(liveDetectorDrawerOverlay, "LiveDetectorTuningDrawerOverlay");
    liveDetectorDrawerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    liveDetectorDrawerOverlay->setVisible(false);
    auto liveDetectorOverlayLayout = new QHBoxLayout;
    liveDetectorOverlayLayout->setContentsMargins(0, 0, 0, 0);
    liveDetectorOverlayLayout->addStretch(1);
    liveDetectorOverlayLayout->addWidget(liveDetectorDrawer, 0, Qt::AlignRight | Qt::AlignTop);
    liveDetectorDrawerOverlay->setLayout(liveDetectorOverlayLayout);
    liveViewerStackLayout->addWidget(liveDetectorDrawerOverlay);
    liveViewerOverlay->raise();
    liveDetectorDrawerOverlay->raise();
    liveViewerStack->setLayout(liveViewerStackLayout);

    auto imageDisplayWidget = new QWidget;
    nameWidget(imageDisplayWidget, "ImageDisplayWidget");
    imageDisplayWidget->setProperty("viewerCanvas", true);
    imageDisplayWidget->setMinimumHeight(360);
    imageDisplayWidget->setMaximumHeight(QWIDGETSIZE_MAX);
    imageDisplayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto imageDisplayLayout = new QVBoxLayout;
    imageDisplayLayout->setContentsMargins(0, 0, 0, 0);
    imageDisplayLayout->setSpacing(0);
    imageDisplayLayout->addWidget(liveViewerStack, 1);
    imageDisplayWidget->setLayout(imageDisplayLayout);

    QMdiSubWindow* imageSubWindow = nullptr;

    using desktop_app::ui::makeCollapsedGroup;
    using desktop_app::ui::makeMetric;
    using desktop_app::ui::makeMutedLabel;
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;
    using desktop_app::ui::makeStatusRow;
    using desktop_app::ui::makeToolButton;
    using desktop_app::ui::makeWorkspacePlaceholder;

    auto liveImagePanel = makePanel("Live Image", "Idle");
    liveImagePanel->setObjectName("LiveImagePanel");
    liveImagePanel->setMinimumWidth(480);
    liveImagePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto liveImageBody = makePanelBody(liveImagePanel, 0, 0, 0, 0);
    liveImageBody->addWidget(imageDisplayWidget, 1);

    auto liveRunBar = new QFrame;
    nameWidget(liveRunBar, "LiveRunControlBar");
    liveRunBar->setProperty("panel", true);
    liveRunBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    liveRunBar->setMinimumHeight(54);
    liveRunBar->setMaximumHeight(64);
    auto liveRunLayout = new QHBoxLayout;
    liveRunLayout->setContentsMargins(10, 6, 10, 6);
    liveRunLayout->setSpacing(8);
    pipelineStartBtn->setText("Start Sorting");
    pipelineStartBtn->setEnabled(false);
    collectionToggleBtn->setText("Start Data Collection");
    pipelineStopBtn->setText("Stop Sorting");
    auto collectionStatusLabel = new QLabel("Collection: idle");
    nameWidget(collectionStatusLabel, "LiveDataCollectionStatusLabel");
    collectionStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    collectionStatusLabel->setTextFormat(Qt::PlainText);
    auto liveForceTriggerBtn = new QPushButton("Manual Trigger");
    nameWidget(liveForceTriggerBtn, "LiveForceTriggerButton");
    liveForceTriggerBtn->setEnabled(false);
    auto liveSnapshotBtn = new QPushButton("Snapshot");
    nameWidget(liveSnapshotBtn, "LiveSnapshotButton");
    auto liveOpenRunBtn = new QPushButton("Open Run");
    nameWidget(liveOpenRunBtn, "LiveOpenRunButton");
    liveOpenRunBtn->setEnabled(false);
    openRunFolderAction->setEnabled(false);
    auto liveDetectorTuningBtn = new QPushButton("Detector");
    nameWidget(liveDetectorTuningBtn, "LiveDetectorTuningButton");
    liveDetectorTuningBtn->setToolTip("Open detector settings.");
    for (auto* button : {startBtn, pipelineStartBtn, collectionToggleBtn, pipelineStopBtn, liveForceTriggerBtn,
                         liveSnapshotBtn, liveOpenRunBtn, liveDetectorTuningBtn}) {
        button->setFixedHeight(34);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    startBtn->setText("Start Camera");
    startBtn->setMinimumWidth(138);
    startBtn->setMaximumWidth(150);
    pipelineStartBtn->setMaximumWidth(150);
    collectionToggleBtn->setFixedWidth(190);
    pipelineStopBtn->setMaximumWidth(138);
    liveForceTriggerBtn->setMaximumWidth(132);
    liveSnapshotBtn->setMaximumWidth(110);
    liveOpenRunBtn->setMaximumWidth(108);
    liveDetectorTuningBtn->setMaximumWidth(112);
    liveRunLayout->addWidget(startBtn);
    liveRunLayout->addWidget(pipelineStartBtn);
    liveRunLayout->addWidget(collectionToggleBtn);
    liveRunLayout->addWidget(pipelineStopBtn);
    liveRunLayout->addSpacing(4);
    liveRunLayout->addWidget(liveForceTriggerBtn);
    liveRunLayout->addWidget(collectionStatusLabel);
    liveRunLayout->addStretch(1);
    liveRunLayout->addWidget(liveSnapshotBtn);
    liveRunLayout->addWidget(liveOpenRunBtn);
    liveRunLayout->addWidget(liveDetectorTuningBtn);
    liveRunBar->setLayout(liveRunLayout);
    liveImageBody->addWidget(liveRunBar, 0);

    auto updateLiveRunStartStopVisibility = [&]() {
        const bool running = pipelineEnableCheck->isChecked();
        pipelineStartBtn->setVisible(!running);
        pipelineStopBtn->setVisible(running);
    };
    updateLiveRunStartStopVisibility();

    std::function<void()> updateForceTriggerState = []() {};
    updateForceTriggerState = [&]() {
        const bool waveformValid = !daqChannelEdit->text().trimmed().isEmpty() && amplitudeSpin->value() > 0.0 &&
                                   freqSpin->value() > 0.0 && durationSpin->value() > 0.0;
        appState.daqWaveformValid = waveformValid;
        const bool manualTriggerReady =
            appState.daqAvailable && !appState.daqDisabled && !appState.daqFault && !collectionActive.load();
        QStringList manualBlockers;
        if (collectionActive.load())
            manualBlockers << "data collection is active";
        if (!appState.daqAvailable || appState.daqDisabled)
            manualBlockers << "DAQ is not available";
        if (appState.daqFault)
            manualBlockers << (appState.daqFaultText.isEmpty() ? QStringLiteral("DAQ fault is active")
                                                               : appState.daqFaultText);
        const QString waveformInvalidTip =
            QStringLiteral("Waveform settings are incomplete; click will block before output.");
        const QString manualTriggerTip =
            manualTriggerReady
                ? (waveformValid ? QStringLiteral("Send the configured manual DAQ trigger.")
                                 : waveformInvalidTip)
                : QStringLiteral("Manual Trigger disabled: %1.").arg(manualBlockers.join("; "));
        labviewTestBtn->setEnabled(manualTriggerReady);
        labviewTestBtn->setToolTip(manualTriggerTip);
        labviewTestBtn->setStatusTip(manualTriggerTip);
        const QString liveManualTriggerTip =
            manualTriggerReady
                ? (waveformValid ? QStringLiteral("Send the configured manual DAQ trigger from Live View.")
                                 : QStringLiteral("Live View Manual Trigger: %1").arg(waveformInvalidTip))
                : QStringLiteral("Live View Manual Trigger disabled: %1.").arg(manualBlockers.join("; "));
        liveForceTriggerBtn->setEnabled(manualTriggerReady);
        liveForceTriggerBtn->setToolTip(liveManualTriggerTip);
        liveForceTriggerBtn->setStatusTip(liveManualTriggerTip);
        manualTriggerAction->setEnabled(manualTriggerReady);
        manualTriggerAction->setStatusTip(liveManualTriggerTip);
        manualTriggerAction->setToolTip(liveManualTriggerTip);
    };
    updateForceTriggerState();

    auto eventsMetricLabel = new QLabel("0");
    auto classifiedHitMetricLabel = new QLabel("0");
    auto classifiedWasteMetricLabel = new QLabel("0");
    auto wentToHitMetricLabel = new QLabel("0");
    auto wentToWasteMetricLabel = new QLabel("0");
    auto trigMetricLabel = new QLabel("--");
    nameWidget(eventsMetricLabel, "LiveRunEventsMetricLabel");
    nameWidget(classifiedHitMetricLabel, "LiveRunClassifiedHitMetricLabel");
    nameWidget(classifiedWasteMetricLabel, "LiveRunClassifiedWasteMetricLabel");
    nameWidget(wentToHitMetricLabel, "LiveRunWentToHitMetricLabel");
    nameWidget(wentToWasteMetricLabel, "LiveRunWentToWasteMetricLabel");
    nameWidget(trigMetricLabel, "LiveRunTriggerRateMetricLabel");

    auto runPanel = makePanel("Run");
    runPanel->setObjectName("LiveRunPanel");
    auto runBody = makePanelBody(runPanel);
    auto metricGrid = new QGridLayout;
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setSpacing(1);
    metricGrid->addWidget(makeMetric("Events", eventsMetricLabel), 0, 0);
    metricGrid->addWidget(makeMetric("Classified Sort", classifiedHitMetricLabel), 0, 1);
    metricGrid->addWidget(makeMetric("Classified Pass", classifiedWasteMetricLabel), 1, 0);
    metricGrid->addWidget(makeMetric("Went to Sort", wentToHitMetricLabel), 1, 1);
    metricGrid->addWidget(makeMetric("Went to Pass", wentToWasteMetricLabel), 2, 0);
    metricGrid->addWidget(makeMetric("Trig/s", trigMetricLabel), 2, 1);
    runBody->addLayout(metricGrid);
    auto runStateResetButton = new QPushButton("Reset Counters");
    nameWidget(runStateResetButton, "RunStateResetCountersButton");
    runStateResetButton->setToolTip("Reset the visible live run counters to zero.");
    runStateResetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    runBody->addWidget(runStateResetButton, 0, Qt::AlignRight);
    auto lastDecisionCard = new QFrame;
    nameWidget(lastDecisionCard, "LiveLastDecisionCard");
    auto lastDecisionLayout = new QHBoxLayout;
    lastDecisionLayout->setContentsMargins(10, 8, 10, 8);
    lastDecisionLayout->setSpacing(10);
    auto lastDecisionThumb = new QLabel("64x64");
    nameWidget(lastDecisionThumb, "LiveLastDecisionThumbnail");
    lastDecisionThumb->setAlignment(Qt::AlignCenter);
    lastDecisionThumb->setFixedSize(54, 42);
    auto lastDecisionText = new QVBoxLayout;
    lastDecisionText->setContentsMargins(0, 0, 0, 0);
    lastDecisionText->setSpacing(1);
    auto lastDecisionTitle = new QLabel("Last decision");
    lastDecisionTitle->setProperty("metricLabel", true);
    auto lastDecisionValue = new QLabel("--");
    nameWidget(lastDecisionValue, "LiveLastDecisionValueLabel");
    lastDecisionValue->setProperty("metricValue", true);
    lastDecisionValue->setStyleSheet("font-size:16px;");
    statsLastLabel->setProperty("mutedText", true);
    statsLastLabel->setWordWrap(true);
    lastDecisionText->addWidget(lastDecisionTitle);
    lastDecisionText->addWidget(lastDecisionValue);
    lastDecisionText->addWidget(statsLastLabel);
    lastDecisionText->addStretch(1);
    lastDecisionLayout->addWidget(lastDecisionThumb);
    lastDecisionLayout->addLayout(lastDecisionText, 1);
    lastDecisionCard->setLayout(lastDecisionLayout);
    runBody->addWidget(lastDecisionCard);

    auto pipelinePanel = makePanel("Pipeline");
    pipelinePanel->setObjectName("LivePipelinePanel");
    auto pipelineBody = makePanelBody(pipelinePanel);
    auto pipelineGrid = new QGridLayout;
    pipelineGrid->setContentsMargins(0, 0, 0, 0);
    pipelineGrid->setHorizontalSpacing(8);
    pipelineGrid->setVerticalSpacing(8);
    pipelineGrid->setColumnStretch(1, 1);
    pipelineGrid->setColumnStretch(2, 1);
    pipelineGrid->addWidget(new QLabel("Target class"), 0, 0);
    pipelineGrid->addWidget(targetClassCombo, 0, 1);
    pipelineGrid->addWidget(new QLabel("Model"), 1, 0);
    pipelineGrid->addWidget(liveModelCombo, 1, 1);
    pipelineGrid->addWidget(openLiveModelManagerBtn, 1, 2);
    pipelineGrid->addWidget(new QLabel("Output"), 2, 0);
    pipelineGrid->addWidget(outputEdit, 2, 1, 1, 2);
    pipelineGrid->addWidget(saveCropCheck, 3, 0);
    pipelineGrid->addWidget(saveOverlayCheck, 3, 1, 1, 2);
    pipelineGrid->addWidget(sortNonTargetCheck, 4, 0, 1, 3);
    pipelineGrid->addWidget(loadPipelineBtn, 5, 1);
    pipelineGrid->addWidget(liveConfigureSettingsBtn, 5, 2);
    pipelineBody->addLayout(pipelineGrid);
    pipelineBody->addWidget(pipelineStatusLabel);

    statusLabel->setWordWrap(true);
    statusLabel->setProperty("mutedText", true);
    statsLabel->setProperty("mutedText", true);
    statsLabel->setWordWrap(true);
    statsLabel->setMaximumHeight(72);
    labviewOutputLabel->setProperty("mutedText", true);
    labviewOutputLabel->setWordWrap(true);
    blockersLabel->setProperty("mutedText", true);
    blockersLabel->setWordWrap(true);
    auto rightScroll = new QScrollArea;
    nameWidget(rightScroll, "LiveRightMetricsScrollArea");
    rightScroll->setWidgetResizable(true);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(360);
    rightScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto rightStack = new QWidget;
    nameWidget(rightStack, "LiveRightMetricsStack");
    rightStack->setMinimumWidth(340);
    auto rightStackLayout = new QVBoxLayout;
    rightStackLayout->setContentsMargins(0, 0, 2, 0);
    rightStackLayout->setSpacing(12);
    rightStackLayout->addWidget(runPanel);
    rightStackLayout->addWidget(pipelinePanel);
    rightStackLayout->addStretch(1);
    rightStack->setLayout(rightStackLayout);
    rightScroll->setWidget(rightStack);

    auto mainSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(mainSplitter, "MainSplitter");
    mainSplitter->addWidget(liveImagePanel);
    mainSplitter->addWidget(rightScroll);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 0);
    desktop_app::ui::configureWorkspaceSplitter(mainSplitter, "workspace/live/splitter", {760, 340}, {520, 360});

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(mainSplitter, 1);
    auto liveWorkspacePage = new QWidget;
    nameWidget(liveWorkspacePage, "LiveWorkspace");
    liveWorkspacePage->setLayout(mainLayout);

    auto leftCaptureTab = new QWidget;
    nameWidget(leftCaptureTab, "OperationalCaptureTab");
    auto leftCaptureLayout = new QVBoxLayout;
    leftCaptureLayout->setContentsMargins(8, 8, 8, 8);
    auto captureContextGroup = new QGroupBox("Acquisition");
    auto captureContextLayout = new QGridLayout;
    auto captureDepthCombo = new QComboBox;
    captureDepthCombo->addItems({"Auto Depth", "8-bit", "12-bit", "16-bit"});
    auto captureTargetCombo = new QComboBox;
    captureTargetCombo->addItems({"Disk", "Memory", "Viewer only"});
    auto leftLoadBtn = makeToolButton("Load");
    auto leftClearBtn = makeToolButton("Clear");
    nameWidget(captureDepthCombo, "CaptureDepthComboBox");
    nameWidget(captureTargetCombo, "CaptureTargetComboBox");
    nameWidget(leftLoadBtn, "CaptureLoadButton");
    nameWidget(leftClearBtn, "CaptureClearButton");
    leftClearBtn->setEnabled(false);
    captureContextLayout->addWidget(new QLabel("Depth"), 0, 0);
    captureContextLayout->addWidget(captureDepthCombo, 0, 1);
    captureContextLayout->addWidget(new QLabel("Target"), 1, 0);
    captureContextLayout->addWidget(captureTargetCombo, 1, 1);
    captureContextLayout->addWidget(leftLoadBtn, 2, 0);
    captureContextLayout->addWidget(leftClearBtn, 2, 1);
    captureContextGroup->setLayout(captureContextLayout);
    leftCaptureLayout->addWidget(captureContextGroup);
    leftCaptureLayout->addStretch(1);
    leftCaptureTab->setLayout(leftCaptureLayout);

    auto leftDevicesTab = new QWidget;
    nameWidget(leftDevicesTab, "OperationalDevicesTab");
    auto leftDevicesLayout = new QVBoxLayout;
    leftDevicesLayout->setContentsMargins(8, 8, 8, 8);
    auto deviceSummaryGroup = new QGroupBox("Camera");
    auto deviceSummaryLayout = new QVBoxLayout;
    auto leftReconnectBtn = makeToolButton("Reconnect Camera");
    nameWidget(leftReconnectBtn, "DevicesReconnectCameraButton");
    deviceSummaryLayout->addWidget(new QLabel("Camera/DCAM startup and reconnect use the existing runtime path."));
    deviceSummaryLayout->addWidget(leftReconnectBtn);
    deviceSummaryGroup->setLayout(deviceSummaryLayout);
    leftDevicesLayout->addWidget(deviceSummaryGroup);
    auto pipelineLabviewGroup = makeCollapsedGroup("DAQ / Trigger", labviewWidget);
    nameWidget(pipelineLabviewGroup, "PipelineLabviewGroup");
    if (auto* pipelineLabviewToggle = pipelineLabviewGroup->findChild<QToolButton*>()) {
        nameWidget(pipelineLabviewToggle, "PipelineLabviewToggleButton");
        pipelineLabviewToggle->setAccessibleName("DAQ / Trigger");
    }
    leftDevicesLayout->addWidget(pipelineLabviewGroup);
    leftDevicesLayout->addStretch(1);
    leftDevicesTab->setLayout(leftDevicesLayout);

    auto leftAnalysisTab = new QWidget;
    nameWidget(leftAnalysisTab, "OperationalAnalysisTab");
    auto leftAnalysisLayout = new QVBoxLayout;
    leftAnalysisLayout->setContentsMargins(8, 8, 8, 8);
    leftAnalysisLayout->addWidget(pipelineWidget);
    auto detectorGroup = makeCollapsedGroup("Detector", detectWidget);
    nameWidget(detectorGroup, "DetectorGroup");
    if (auto* detectorToggle = detectorGroup->findChild<QToolButton*>()) {
        nameWidget(detectorToggle, "DetectorToggleButton");
        detectorToggle->setAccessibleName("Detector");
    }
    leftAnalysisLayout->addWidget(detectorGroup);
    auto detailedStatsGroup = makeCollapsedGroup("Detailed Stats", statsWidget);
    nameWidget(detailedStatsGroup, "DetailedStatsGroup");
    if (auto* detailedStatsToggle = detailedStatsGroup->findChild<QToolButton*>()) {
        nameWidget(detailedStatsToggle, "DetailedStatsToggleButton");
        detailedStatsToggle->setAccessibleName("Detailed Stats");
    }
    leftAnalysisLayout->addWidget(detailedStatsGroup);
    leftAnalysisLayout->addStretch(1);
    leftAnalysisTab->setLayout(leftAnalysisLayout);

    auto operationalTabs = new QTabWidget;
    operationalTabs->setObjectName("OperationalTabs");
    operationalTabs->setAccessibleName("OperationalTabs");
    operationalTabs->addTab(leftCaptureTab, "Capture");
    operationalTabs->addTab(leftDevicesTab, "Devices");
    operationalTabs->addTab(leftAnalysisTab, "Analysis");
    operationalTabs->addTab(trainerDockProxy, "Trainer");

    auto operationDock = new QDockWidget("Capture", this);
    operationDock->setObjectName("OperationalDock");
    operationDock->setAccessibleName("OperationalDock");
    operationDock->setWidget(operationalTabs);
    operationDock->setMinimumWidth(260);
    this->addDockWidget(Qt::LeftDockWidgetArea, operationDock);
    operationDock->hide();

    desktop_app::workspace::CameraWorkspaceControls cameraWorkspaceControls;
    cameraWorkspaceControls.presetCombo = presetCombo;
    cameraWorkspaceControls.bitsCombo = bitsCombo;
    cameraWorkspaceControls.customWidthSpin = customWidthSpin;
    cameraWorkspaceControls.customHeightSpin = customHeightSpin;
    cameraWorkspaceControls.exposureSpin = exposureSpin;
    cameraWorkspaceControls.autoExposureButton = autoExposureBtn;
    cameraWorkspaceControls.readoutCombo = readoutCombo;
    cameraWorkspaceControls.binCombo = binCombo;
    cameraWorkspaceControls.lutMinSpin = lutMinSpin;
    cameraWorkspaceControls.lutMaxSpin = lutMaxSpin;
    cameraWorkspaceControls.lutAutoSetButton = lutAutoSetBtn;
    cameraWorkspaceControls.lutMinSlider = lutMinSlider;
    cameraWorkspaceControls.lutMaxSlider = lutMaxSlider;
    cameraWorkspaceControls.displayEverySpin = displayEverySpin;
    cameraWorkspaceControls.lutRangeLabel = lutRangeLabel;
    cameraWorkspaceControls.savePathEdit = savePathEdit;
    cameraWorkspaceControls.saveBrowseButton = saveBrowseBtn;
    cameraWorkspaceControls.saveOpenButton = saveOpenBtn;
    cameraWorkspaceControls.saveStartButton = saveStartBtn;
    cameraWorkspaceControls.saveStopButton = saveStopBtn;
    cameraWorkspaceControls.saveInfoLabel = saveInfoLabel;
    cameraWorkspaceControls.sequenceWidget = seqWidget;
    cameraWorkspaceControls.sequenceFolderEdit = seqFolderEdit;
    cameraWorkspaceControls.sequenceBrowseButton = seqBrowseBtn;
    cameraWorkspaceControls.sequenceLoadButton = seqLoadBtn;
    cameraWorkspaceControls.sequenceStartButton = seqStartBtn;
    cameraWorkspaceControls.sequenceStopButton = seqStopBtn;
    cameraWorkspaceControls.sequenceFpsSpin = seqFpsSpin;
    cameraWorkspaceControls.sequenceStatusLabel = seqStatusLabel;
    cameraWorkspaceControls.sequenceLogLabel = seqLogLabel;
    auto cameraControlsStack = desktop_app::workspace::buildCameraControlsStack(cameraWorkspaceControls);
    rightStackLayout->insertWidget(2, cameraControlsStack);

    desktop_app::workspace::DatasetWorkspaceControls datasetWorkspaceControls;
    datasetWorkspaceControls.datasetReviewAction = datasetLabelDatasetAction;
    datasetWorkspaceControls.operationDock = operationDock;
    datasetWorkspaceControls.operationalTabs = operationalTabs;
    datasetWorkspaceControls.captureTab = leftCaptureTab;
    auto datasetWorkspacePage = desktop_app::workspace::buildDatasetWorkspace(datasetWorkspaceControls);

    auto trainerWorkspacePage = new QWidget;
    nameWidget(trainerWorkspacePage, "TrainerWorkspace");
    auto trainerWorkspaceLayout = new QVBoxLayout;
    trainerWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
    trainerWorkspaceLayout->setSpacing(0);
    trainerWorkspaceLayout->addWidget(trainerWidget, 1);
    trainerWorkspacePage->setLayout(trainerWorkspaceLayout);

    auto validatorResolveAppRelative = [](const QString& path) -> QString {
        if (path.isEmpty())
            return path;
        QFileInfo info(path);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = dir.filePath(path);
            if (QFileInfo::exists(candidate))
                return QFileInfo(candidate).absoluteFilePath();
            const QString modelCandidate = dir.filePath("models/" + QFileInfo(path).fileName());
            if (QFileInfo::exists(modelCandidate))
                return QFileInfo(modelCandidate).absoluteFilePath();
            if (!dir.cdUp())
                break;
        }
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path);
    };
    auto validatorTrainerPythonPath = []() -> QString {
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = dir.filePath("training/python");
            if (QFileInfo(candidate).isDir())
                return QFileInfo(candidate).absoluteFilePath();
            if (!dir.cdUp())
                break;
        }
        QDir cwd(QDir::currentPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = cwd.filePath("training/python");
            if (QFileInfo(candidate).isDir())
                return QFileInfo(candidate).absoluteFilePath();
            if (!cwd.cdUp())
                break;
        }
        return QString();
    };

    QWidget* modelWorkspacePage = nullptr;
    desktop_app::workspace::ValidatorWorkspaceControls validatorWorkspaceControls;
    validatorWorkspaceControls.modelPath = validatorResolveAppRelative(onnxEdit->text().trimmed());
    validatorWorkspaceControls.metadataPath = validatorResolveAppRelative(metaEdit->text().trimmed());
    validatorWorkspaceControls.pythonExecutable =
        runtimeSettings.value("validator/pythonExecutable", "python").toString();
    validatorWorkspaceControls.datasetPath =
        runtimeSettings.value("validator/imageDataset", defaultWorkspacePaths.preparedDatasetManifest).toString();
    validatorWorkspaceControls.outputPath =
        runtimeSettings
            .value("validator/outputFolder",
                   QDir(defaultWorkspacePaths.validationRuns)
                       .filePath("validation_gui_image_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")))
            .toString();
    validatorWorkspaceControls.trainerPythonPath = validatorTrainerPythonPath();
    validatorWorkspaceControls.registryEntries = registryEntries;
    validatorWorkspaceControls.imageValidationAction = imageValidationAction;
    validatorWorkspaceControls.imageSummaryChangedCallback = [registryFilePath, &modelWorkspacePage](const QString& summaryPath) {
        QString updatedEntryId;
        QString error;
        if (!updateModelRegistryImageValidationSummary(registryFilePath, summaryPath, &updatedEntryId, &error)) {
            qWarning().noquote() << "Model registry validation writeback skipped:" << error;
            return;
        }
        qInfo().noquote() << "Model registry validation writeback updated" << updatedEntryId;
        if (modelWorkspacePage) {
            if (auto* refreshButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceRefreshButton"))
                refreshButton->click();
        }
    };
    auto validatorWorkspacePage = desktop_app::workspace::buildValidatorWorkspace(validatorWorkspaceControls);
    auto* validatorWorkspaceModelEdit = validatorWorkspacePage->findChild<QLineEdit*>("ValidatorWorkspaceModelEdit");
    auto* validatorWorkspaceMetadataEdit =
        validatorWorkspacePage->findChild<QLineEdit*>("ValidatorWorkspaceMetadataEdit");
    auto* validatorWorkspaceImageWidget = static_cast<ImageValidationWidget*>(
        validatorWorkspacePage->findChild<QWidget*>("ValidatorWorkspaceImageValidationWidget"));
    auto* validatorWorkspaceModelCombo = validatorWorkspacePage->findChild<QComboBox*>("ValidatorWorkspaceModelCombo");
    auto syncValidatorWorkspaceRuntimeModel = [&]() {
        const QString runtimeModel = QDir::cleanPath(validatorResolveAppRelative(onnxEdit->text().trimmed()));
        if (validatorWorkspaceModelCombo) {
            for (int i = 0; i < validatorWorkspaceModelCombo->count(); ++i) {
                const QVariantMap item = validatorWorkspaceModelCombo->itemData(i, Qt::UserRole + 1).toMap();
                if (QDir::cleanPath(item.value("model_path").toString()).compare(runtimeModel, Qt::CaseInsensitive) == 0) {
                    validatorWorkspaceModelCombo->setCurrentIndex(i);
                    return;
                }
            }
        }
    };

    DatasetWorkspaceController* datasetController = nullptr;
    desktop_app::workspace::ModelWorkspaceControls modelWorkspaceControls;
    modelWorkspaceControls.registryEntries = registryEntries;
    modelWorkspaceControls.registryFilePath = registryFilePath;
    modelWorkspaceControls.registryLoadWarning = registryLoadWarning;
    modelWorkspaceControls.targetClassCombo = targetClassCombo;
    modelWorkspaceControls.imageValidationAction = imageValidationAction;
    modelWorkspaceControls.validatorWorkspace = validatorWorkspacePage;
    modelWorkspaceControls.appState = &appState;
    modelWorkspaceControls.registryChangedCallback = [&datasetController, registryFilePath, validatorWorkspaceImageWidget]() {
        if (datasetController)
            datasetController->refreshTrainerUi();
        if (validatorWorkspaceImageWidget) {
            QString warning;
            validatorWorkspaceImageWidget->refreshModelRegistry(
                readModelRegistryEntriesFromPath(registryFilePath, &warning));
        }
    };
    verifierTrace(QStringLiteral("startup: building model library"));
    auto* modelLibraryPage = desktop_app::workspace::buildModelWorkspace(modelWorkspaceControls);
    verifierTrace(QStringLiteral("startup: model library built"));
    modelLibraryPage->setObjectName("ModelLibraryPage");
    auto* modelWorkspaceTabs = new QTabWidget;
    nameWidget(modelWorkspaceTabs, "ModelWorkspaceTabs");
    modelWorkspaceTabs->addTab(modelLibraryPage, "Library");
    modelWorkspaceTabs->addTab(trainerWorkspacePage, "Train");
    modelWorkspaceTabs->addTab(validatorWorkspacePage, "Test");
    modelWorkspaceTabs->setDocumentMode(true);
    modelWorkspaceTabs->tabBar()->setExpanding(false);
    modelWorkspaceTabs->tabBar()->setDrawBase(false);
    modelWorkspaceTabs->tabBar()->setFocusPolicy(Qt::StrongFocus);
    modelWorkspaceTabs->tabBar()->setMinimumHeight(44);
    modelWorkspaceTabs->tabBar()->setMaximumWidth(480);
    modelWorkspaceTabs->setProperty("openDssSegmentedTabs", true);
    modelWorkspaceTabs->setStyleSheet(
        "QTabWidget#ModelWorkspaceTabs::pane { border: 0; top: -1px; }"
        "QTabWidget#ModelWorkspaceTabs QTabBar::tab {"
        "  min-width: 112px; min-height: 34px; padding: 4px 16px; margin: 4px 2px;"
        "  color: palette(button-text); background: palette(button);"
        "  border: 1px solid palette(mid); border-radius: 7px; font-size: 14px;"
        "}"
        "QTabWidget#ModelWorkspaceTabs QTabBar::tab:hover { background: palette(alternate-base); }"
        "QTabWidget#ModelWorkspaceTabs QTabBar::tab:selected {"
        "  color: palette(highlighted-text); background: palette(highlight); font-weight: 600;"
        "}"
        "QTabWidget#ModelWorkspaceTabs QTabBar::tab:disabled { color: palette(mid); }"
        "QTabWidget#ModelWorkspaceTabs QTabBar:focus { outline: 1px solid palette(highlight); }");
    modelWorkspaceTabs->setCurrentIndex(0);
    modelWorkspacePage = new QWidget;
    nameWidget(modelWorkspacePage, "ModelWorkspace");
    auto* modelWorkspaceHostLayout = new QVBoxLayout;
    modelWorkspaceHostLayout->setContentsMargins(0, 0, 0, 0);
    modelWorkspaceHostLayout->addWidget(modelWorkspaceTabs);
    modelWorkspacePage->setLayout(modelWorkspaceHostLayout);

    desktop_app::workspace::ReportsWorkspaceControls reportsWorkspaceControls;
    reportsWorkspaceControls.logPath = logPath;
    reportsWorkspaceControls.viewerOnly = viewerOnly;
    reportsWorkspaceControls.outputRoot = defaultWorkspacePaths.runs;
    reportsWorkspaceControls.showLogsAction = showLogsAction;
    reportsWorkspaceControls.showDiagnosticsAction = showDiagnosticsAction;
    reportsWorkspaceControls.openRunFolderAction = openRunFolderAction;
    reportsWorkspaceControls.outputRootEdit = outputEdit;
    auto reportsWorkspacePage = desktop_app::workspace::buildReportsWorkspace(reportsWorkspaceControls);
    verifierTrace(QStringLiteral("startup: reports workspace built"));

    desktop_app::workspace::SettingsWorkspaceControls settingsWorkspaceControls;
    settingsWorkspaceControls.outputRoot = defaultWorkspacePaths.runs;
    settingsWorkspaceControls.modelPath = defaultWorkspacePaths.models;
    settingsWorkspaceControls.metadataPath = metaEdit->text().trimmed();
    settingsWorkspaceControls.datasetsRoot = defaultWorkspacePaths.datasets;
    settingsWorkspaceControls.logPath = logPath;
    settingsWorkspaceControls.cameraSavePathEdit = savePathEdit;
    settingsWorkspaceControls.cameraPresetCombo = presetCombo;
    settingsWorkspaceControls.computeDeviceCombo = computeDeviceCombo;
    settingsWorkspaceControls.exposureSpin = exposureSpin;
    settingsWorkspaceControls.daqDeviceCombo = daqDeviceCombo;
    settingsWorkspaceControls.daqChannelEdit = daqChannelEdit;
    settingsWorkspaceControls.amplitudeSpin = amplitudeSpin;
    settingsWorkspaceControls.frequencySpin = freqSpin;
    settingsWorkspaceControls.durationSpin = durationSpin;
    settingsWorkspaceControls.delaySpin = delaySpin;
    settingsWorkspaceControls.logCheck = logCheck;
    settingsWorkspaceControls.outputRootEdit = outputEdit;
    settingsWorkspaceControls.trainerPythonEdit = trainerPythonEdit;
    settingsWorkspaceControls.trainerDatasetRootEdit = trainerDatasetEdit;
    settingsWorkspaceControls.operationDock = operationDock;
    settingsWorkspaceControls.resetLayoutAction = resetLayoutAction;
    settingsWorkspaceControls.operationalTabs = operationalTabs;
    settingsWorkspaceControls.analysisTab = leftAnalysisTab;
    settingsWorkspaceControls.devicesTab = leftDevicesTab;
    auto settingsWorkspacePage = desktop_app::workspace::buildSettingsWorkspace(settingsWorkspaceControls);
    verifierTrace(QStringLiteral("startup: settings workspace built"));
    auto* validatorWorkspaceDeviceCombo =
        validatorWorkspacePage->findChild<QComboBox*>("ValidatorWorkspaceDeviceComboBox");
    auto syncValidatorComputeDevice = [validatorWorkspaceDeviceCombo, selectedComputeDevice]() {
        if (!validatorWorkspaceDeviceCombo)
            return;
        const int index = validatorWorkspaceDeviceCombo->findText(selectedComputeDevice());
        if (index >= 0 && validatorWorkspaceDeviceCombo->currentIndex() != index) {
            QSignalBlocker blocker(validatorWorkspaceDeviceCombo);
            validatorWorkspaceDeviceCombo->setCurrentIndex(index);
        }
    };
    auto syncTrainerComputeDevice = [trainerDeviceCombo, selectedComputeDevice]() {
        if (!trainerDeviceCombo)
            return;
        const int index = trainerDeviceCombo->findData(selectedComputeDevice());
        if (index >= 0 && trainerDeviceCombo->currentIndex() != index) {
            QSignalBlocker blocker(trainerDeviceCombo);
            trainerDeviceCombo->setCurrentIndex(index);
        }
    };
    syncValidatorComputeDevice();
    syncTrainerComputeDevice();
    QObject::connect(computeDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                     [persistComputeDevice, selectedComputeDevice, syncValidatorComputeDevice,
                      syncTrainerComputeDevice, trainerPythonEdit](int) {
                         persistComputeDevice(selectedComputeDevice());
                         const QString currentPython = trainerPythonEdit ? trainerPythonEdit->text().trimmed() : QString();
                         const QString cpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv"));
                         const QString gpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv-gpu"));
                         if (trainerPythonEdit &&
                             (currentPython.isEmpty() ||
                              currentPython.compare(QStringLiteral("python"), Qt::CaseInsensitive) == 0 ||
                              sameCleanPath(currentPython, cpuPython) || sameCleanPath(currentPython, gpuPython))) {
                             trainerPythonEdit->setText(QDir::toNativeSeparators(
                                 resolvedTrainerPythonExecutable(currentPython, selectedComputeDevice())));
                          }
                          syncValidatorComputeDevice();
                          syncTrainerComputeDevice();
                      });
    QObject::connect(trainerDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                     [trainerDeviceCombo, computeDeviceCombo](int) {
                         if (!trainerDeviceCombo || !computeDeviceCombo)
                             return;
                         const int settingsIndex = computeDeviceCombo->findData(trainerDeviceCombo->currentData());
                         if (settingsIndex >= 0 && computeDeviceCombo->currentIndex() != settingsIndex)
                             computeDeviceCombo->setCurrentIndex(settingsIndex);
                     });

    auto workspaceStack = new QStackedWidget;
    nameWidget(workspaceStack, "OpenDssWorkspaceStack");
    workspaceStack->addWidget(liveWorkspacePage);
    workspaceStack->addWidget(modelWorkspacePage);
    workspaceStack->addWidget(datasetWorkspacePage);
    workspaceStack->addWidget(reportsWorkspacePage);
    workspaceStack->addWidget(settingsWorkspacePage);
    workspaceStack->setCurrentWidget(liveWorkspacePage);
    auto liveModelMenu = new QMenu(openLiveModelManagerBtn);
    liveModelMenu->addAction("Open Model workspace", [=]() { workspaceStack->setCurrentWidget(modelWorkspacePage); });
    openLiveModelManagerBtn->setMenu(liveModelMenu);
    QObject::connect(liveConfigureSettingsBtn, &QPushButton::clicked,
                     [=]() { workspaceStack->setCurrentWidget(settingsWorkspacePage); });

    auto headerProductLabel = new QLabel("OpenDSS");
    nameWidget(headerProductLabel, "OpenDssHeaderProductTitle");
    auto headerTitleLabel = new QLabel("/ Live View");
    nameWidget(headerTitleLabel, "OpenDssHeaderWorkspaceTitle");
    auto headerStatusText = new QLabel("Live View workspace");
    nameWidget(headerStatusText, "OpenDssHeaderStatusText");
    headerStatusText->setTextInteractionFlags(Qt::NoTextInteraction);
    headerStatusText->setProperty("statusChip", true);
    headerStatusText->setProperty("chipTone", "info");
    headerStatusText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerStatusText->setMinimumWidth(190);
    headerStatusText->setMaximumWidth(300);
    headerStatusText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto headerCameraChip = new QLabel("Camera startup");
    auto headerModelChip = new QLabel("Model not loaded");
    auto headerDaqChip = new QLabel(!kDaqBuildEnabled ? "DAQ unavailable" : "DAQ unchecked");
    auto headerTriggerChip = new QLabel("Manual trigger blocked");
    nameWidget(headerCameraChip, "OpenDssHeaderCameraChip");
    nameWidget(headerModelChip, "OpenDssHeaderModelChip");
    nameWidget(headerDaqChip, "OpenDssHeaderDaqChip");
    nameWidget(headerTriggerChip, "OpenDssHeaderTriggerChip");
    for (auto* chip : {headerCameraChip, headerModelChip, headerDaqChip, headerTriggerChip}) {
        chip->setProperty("statusChip", true);
        chip->setTextInteractionFlags(Qt::NoTextInteraction);
        chip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        chip->setMinimumWidth(92);
        chip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    }
    headerCameraChip->setProperty("chipTone", "warn");
    headerModelChip->setProperty("chipTone", "warn");
    headerDaqChip->setProperty("chipTone", !kDaqBuildEnabled ? "disabled" : "warn");
    headerTriggerChip->setProperty("chipTone", "warn");
    auto diagnosticsHeaderButton = new QToolButton;
    nameWidget(diagnosticsHeaderButton, "OpenDssHeaderDiagnosticsButton");
    diagnosticsHeaderButton->setProperty("headerIcon", true);
    diagnosticsHeaderButton->setProperty("brandIconKey", "info");
    diagnosticsHeaderButton->setIcon(makeBrandIcon("info", QColor("#FFFFFF"), QColor("#7DD3FC")));
    diagnosticsHeaderButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    diagnosticsHeaderButton->setToolTip("Information and support");
    diagnosticsHeaderButton->setAccessibleName("Information and support");
    diagnosticsHeaderButton->setPopupMode(QToolButton::InstantPopup);
    auto informationMenu = new QMenu(diagnosticsHeaderButton);
    nameObject(informationMenu, "OpenDssInformationMenu");
    informationMenu->addAction(aboutAction);
    informationMenu->addAction(showDiagnosticsAction);
    informationMenu->addAction(showLogsAction);
    informationMenu->addAction(documentationAction);
    diagnosticsHeaderButton->setMenu(informationMenu);
    auto themeToggleButton = new QToolButton;
    nameWidget(themeToggleButton, "OpenDssHeaderThemeToggleButton");
    themeToggleButton->setCheckable(true);
    themeToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    themeToggleButton->setMinimumWidth(54);
    themeToggleButton->setAccessibleName("OpenDssHeaderThemeToggleButton");
    auto updateThemeToggleButton = [&]() {
        const bool lightTheme = currentThemeMode == desktop_app::theme::ThemeMode::Light;
        themeToggleButton->setChecked(lightTheme);
        themeToggleButton->setText(lightTheme ? "Dark" : "Light");
        themeToggleButton->setToolTip(lightTheme ? "Switch to dark mode" : "Switch to light mode");
    };
    updateThemeToggleButton();
    QObject::connect(themeToggleButton, &QToolButton::clicked, [&]() {
        currentThemeMode =
            themeToggleButton->isChecked() ? desktop_app::theme::ThemeMode::Light : desktop_app::theme::ThemeMode::Dark;
        runtimeSettings.setValue("shell/theme",
                                 currentThemeMode == desktop_app::theme::ThemeMode::Light ? "light" : "dark");
        applyShellTheme();
        updateThemeToggleButton();
    });
    auto shellHeader = new QFrame;
    nameWidget(shellHeader, "OpenDssHeader");
    shellHeader->setFrameShape(QFrame::NoFrame);
    auto shellHeaderLayout = new QHBoxLayout;
    shellHeaderLayout->setContentsMargins(12, 0, 12, 0);
    shellHeaderLayout->setSpacing(8);
    auto headerLogoLabel = new QLabel;
    nameWidget(headerLogoLabel, "OpenDssHeaderLogo");
    headerLogoLabel->setPixmap(QPixmap(":/branding/opendss-icon-512.png")
                                   .scaled(QSize(24, 24), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    headerLogoLabel->setFixedSize(26, 26);
    headerLogoLabel->setAlignment(Qt::AlignCenter);
    shellHeaderLayout->addWidget(headerLogoLabel);
    shellHeaderLayout->addWidget(headerProductLabel);
    shellHeaderLayout->addWidget(headerTitleLabel);
    shellHeaderLayout->addWidget(headerStatusText);
    shellHeaderLayout->addWidget(headerCameraChip);
    shellHeaderLayout->addWidget(headerModelChip);
    shellHeaderLayout->addWidget(headerDaqChip);
    shellHeaderLayout->addWidget(headerTriggerChip);
    shellHeaderLayout->addStretch(1);
    shellHeaderLayout->addWidget(themeToggleButton);
    shellHeaderLayout->addWidget(diagnosticsHeaderButton);
    shellHeader->setLayout(shellHeaderLayout);

    auto shellStatusStrip = new QFrame;
    nameWidget(shellStatusStrip, "OpenDssStatusStrip");
    shellStatusStrip->setFrameShape(QFrame::NoFrame);
    auto shellStatusLayout = new QHBoxLayout;
    shellStatusLayout->setContentsMargins(12, 0, 12, 0);
    shellStatusLayout->setSpacing(16);
    auto shellRuntimeStatus = new QLabel("Run: idle");
    auto shellCameraStatus = new QLabel("Camera: startup pending");
    auto shellModelStatus = new QLabel("Model: not loaded");
    auto shellDaqStatus = new QLabel(initialDaqStatusText);
    nameWidget(shellRuntimeStatus, "OpenDssShellRunStatusLabel");
    nameWidget(shellCameraStatus, "OpenDssShellCameraStatusLabel");
    nameWidget(shellModelStatus, "OpenDssShellModelStatusLabel");
    nameWidget(shellDaqStatus, "OpenDssShellDaqStatusLabel");
    for (auto* item : {shellRuntimeStatus, shellCameraStatus, shellModelStatus, shellDaqStatus}) {
        item->setTextInteractionFlags(Qt::NoTextInteraction);
        shellStatusLayout->addWidget(item);
    }
    shellStatusLayout->addStretch(1);
    auto shellDiagnosticsStatus = new QLabel("Diagnostics");
    nameWidget(shellDiagnosticsStatus, "OpenDssShellDiagnosticsStatusLabel");
    shellDiagnosticsStatus->setTextInteractionFlags(Qt::NoTextInteraction);
    shellStatusLayout->addWidget(shellDiagnosticsStatus);
    auto shellLogPathStatus = new QLabel("Log: " + QFileInfo(logPath).fileName());
    nameWidget(shellLogPathStatus, "OpenDssShellLogPathLabel");
    shellLogPathStatus->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    shellLogPathStatus->setToolTip(logPath);
    shellStatusLayout->addWidget(shellLogPathStatus);
    shellStatusStrip->setLayout(shellStatusLayout);

    auto navRail = new QFrame;
    nameWidget(navRail, "OpenDssNavigationRail");
    navRail->setFrameShape(QFrame::NoFrame);
    navRail->setMinimumWidth(56);
    navRail->setMaximumWidth(56);
    auto navLayout = new QVBoxLayout;
    navLayout->setContentsMargins(8, 12, 8, 12);
    navLayout->setSpacing(4);
    auto railLogo = new QLabel("DS");
    nameWidget(railLogo, "OpenDssRailLogo");
    railLogo->setAlignment(Qt::AlignCenter);
    railLogo->setFixedSize(36, 36);
    railLogo->setText(QString());
    railLogo->setPixmap(QPixmap(":/branding/opendss-icon-512.png")
                            .scaled(QSize(26, 26), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    navLayout->addWidget(railLogo, 0, Qt::AlignHCenter);
    navLayout->addSpacing(8);
    auto navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    auto addNavButton = [&](const QString& text, const QString& iconKey, QWidget* page, const char* objectName) {
        auto* button = new QPushButton;
        nameWidget(button, objectName);
        button->setCheckable(true);
        button->setProperty("railButton", true);
        button->setProperty("brandIconKey", iconKey);
        button->setIcon(makeBrandIcon(iconKey, QColor("#FFFFFF"), QColor("#14B8A6")));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        navGroup->addButton(button);
        navLayout->addWidget(button, 0, Qt::AlignHCenter);
        QObject::connect(button, &QPushButton::clicked, [=]() {
            workspaceStack->setCurrentWidget(page);
            headerTitleLabel->setText("/ " + text);
            headerStatusText->setText(text + " workspace");
        });
        return button;
    };
    verifierTrace(QStringLiteral("startup: building navigation"));
    auto liveNavButton = addNavButton("Live View", "play", liveWorkspacePage, "NavLiveButton");
    auto modelNavButton = addNavButton("Models", "model", modelWorkspacePage, "NavModelButton");
    auto datasetNavButton = addNavButton("Dataset", "dataset", datasetWorkspacePage, "NavDatasetButton");
    auto trainerNavButton = new QPushButton(modelWorkspacePage);
    nameWidget(trainerNavButton, "ModelsTrainTabRoute");
    trainerNavButton->hide();
    auto validatorNavButton = new QPushButton(modelWorkspacePage);
    nameWidget(validatorNavButton, "ModelsTestTabRoute");
    validatorNavButton->hide();
    QObject::connect(trainerNavButton, &QPushButton::clicked, [=]() {
        workspaceStack->setCurrentWidget(modelWorkspacePage);
        modelWorkspaceTabs->setCurrentIndex(1);
        modelNavButton->setChecked(true);
        headerTitleLabel->setText("/ Models / Train");
        headerStatusText->setText("Train model");
    });
    QObject::connect(validatorNavButton, &QPushButton::clicked, [=]() {
        workspaceStack->setCurrentWidget(modelWorkspacePage);
        modelWorkspaceTabs->setCurrentIndex(2);
        modelNavButton->setChecked(true);
        headerTitleLabel->setText("/ Models / Test");
        headerStatusText->setText("Test model");
    });
    QObject::connect(modelWorkspaceTabs, &QTabWidget::currentChanged, [=](int index) {
        if (workspaceStack->currentWidget() != modelWorkspacePage)
            return;
        static const QStringList tabNames = {"Library", "Train", "Test"};
        const QString tabName = tabNames.value(index, "Library");
        headerTitleLabel->setText("/ Models / " + tabName);
        headerStatusText->setText(tabName == "Library" ? "Models workspace" : tabName);
    });
    auto reportsNavButton = addNavButton("Reports", "reports", reportsWorkspacePage, "NavReportsButton");
    navLayout->addStretch(1);
    auto settingsNavButton = addNavButton("Settings", "settings", settingsWorkspacePage, "NavSettingsButton");

    refreshThemeDependentChrome = [&]() {
        const auto shellColors = desktop_app::theme::colors(currentThemeMode);
        const QColor headerAccent = shellColors.shellIconAccent;
        const QColor railAccent = currentThemeMode == desktop_app::theme::ThemeMode::Light
                                      ? shellColors.shellIconAccent
                                      : shellColors.brandAqua;
        const auto applyIcon = [&](QAbstractButton* button, const QColor& accent) {
            if (!button)
                return;
            const QString iconKey = button->property("brandIconKey").toString().trimmed();
            if (iconKey.isEmpty())
                return;
            button->setIcon(makeBrandIcon(iconKey, shellColors.shellIconFg, accent));
        };

        applyIcon(diagnosticsHeaderButton, headerAccent);
        applyIcon(liveNavButton, railAccent);
        applyIcon(modelNavButton, railAccent);
        applyIcon(datasetNavButton, railAccent);
        applyIcon(trainerNavButton, railAccent);
        applyIcon(validatorNavButton, railAccent);
        applyIcon(reportsNavButton, railAccent);
        applyIcon(settingsNavButton, railAccent);
    };
    verifierTrace(QStringLiteral("startup: navigation built"));
    refreshThemeDependentChrome();
    verifierTrace(QStringLiteral("startup: navigation icons refreshed"));
    auto wireHeaderChipNavigation = [&](QLabel* chip, QPushButton* destination, const QString& tooltip) {
        chip->setCursor(Qt::PointingHandCursor);
        chip->setToolTip(tooltip);
        chip->installEventFilter(new HeaderChipClickFilter(
            [destination]() {
                if (destination)
                    destination->click();
            },
            chip));
    };
    wireHeaderChipNavigation(headerStatusText, liveNavButton, "Open Live View");
    wireHeaderChipNavigation(headerCameraChip, liveNavButton, "Open Live View");
    wireHeaderChipNavigation(headerModelChip, modelNavButton, "Open Model");
    wireHeaderChipNavigation(headerDaqChip, settingsNavButton, "Open Settings hardware");
    wireHeaderChipNavigation(headerTriggerChip, liveNavButton, "Open Live View");
    liveNavButton->setChecked(true);
    if (options.initialWorkspace == "model") {
        workspaceStack->setCurrentWidget(modelWorkspacePage);
        headerTitleLabel->setText("/ Model");
        headerStatusText->setText("Model workspace");
        modelNavButton->setChecked(true);
    } else if (options.initialWorkspace == "dataset") {
        workspaceStack->setCurrentWidget(datasetWorkspacePage);
        headerTitleLabel->setText("/ Dataset");
        headerStatusText->setText("Dataset workspace");
        datasetNavButton->setChecked(true);
    } else if (options.initialWorkspace == "trainer") {
        trainerNavButton->click();
    } else if (options.initialWorkspace == "validator") {
        validatorNavButton->click();
    } else if (options.initialWorkspace == "reports") {
        workspaceStack->setCurrentWidget(reportsWorkspacePage);
        headerTitleLabel->setText("/ Reports");
        headerStatusText->setText("Reports workspace");
        reportsNavButton->setChecked(true);
    } else if (options.initialWorkspace == "settings") {
        workspaceStack->setCurrentWidget(settingsWorkspacePage);
        headerTitleLabel->setText("/ Settings");
        headerStatusText->setText("Settings workspace");
        settingsNavButton->setChecked(true);
    }
    navRail->setLayout(navLayout);

    auto shellContent = new QWidget;
    nameWidget(shellContent, "OpenDssShellContent");
    auto shellContentLayout = new QVBoxLayout;
    shellContentLayout->setContentsMargins(0, 0, 0, 0);
    shellContentLayout->setSpacing(0);
    shellContentLayout->addWidget(shellHeader);
    shellContentLayout->addWidget(workspaceStack, 1);
    shellContentLayout->addWidget(shellStatusStrip);
    shellContent->setLayout(shellContentLayout);

    auto centralWidget = new QWidget;
    nameWidget(centralWidget, "CentralWidget");
    auto shellLayout = new QHBoxLayout;
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(navRail);
    verifierTrace(QStringLiteral("startup: shell content assembled"));
    shellLayout->addWidget(shellContent, 1);
    centralWidget->setLayout(shellLayout);
    this->setCentralWidget(centralWidget);

    std::function<int()> runModelsWorkspaceVerification;
    if (verifyModelsWorkspaceConsolidation) {
        verifierTrace(QStringLiteral("verify-models: entering assertions"));
        runModelsWorkspaceVerification = [=, &app]() {
            QStringList failures;
            const auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "MODELS WORKSPACE VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "MODELS WORKSPACE VERIFY PASS:" << message;
                }
            };
            modelNavButton->click();
            app.processEvents();
            require(workspaceStack->currentWidget() == modelWorkspacePage, "Models navigation opens the consolidated workspace");
            require(modelWorkspaceTabs->count() == 3, "Models workspace owns Library, Train, and Test tabs only");
            require(modelWorkspaceTabs->tabText(0) == "Library" && modelWorkspaceTabs->tabText(1) == "Train" &&
                        modelWorkspaceTabs->tabText(2) == "Test",
                    "Models tabs use the required task names");
            require(modelWorkspaceTabs->tabBar()->minimumHeight() >= 44 &&
                        modelWorkspaceTabs->tabBar()->maximumWidth() <= 480 &&
                        modelWorkspaceTabs->property("openDssSegmentedTabs").toBool() &&
                        !modelWorkspaceTabs->tabBar()->expanding() && !modelWorkspaceTabs->tabBar()->drawBase(),
                    "Models use a compact constrained OpenDSS segmented tab control");
            require(modelWorkspaceTabs->styleSheet().contains("palette(highlight)") &&
                        modelWorkspaceTabs->styleSheet().contains("tab:selected") &&
                        modelWorkspaceTabs->styleSheet().contains("tab:hover") &&
                        !modelWorkspaceTabs->styleSheet().contains("border-bottom: 3") &&
                        modelWorkspaceTabs->tabBar()->focusPolicy() == Qt::StrongFocus,
                    "Models tabs expose themed selected, hover, and keyboard-focus states without heavy separators");
            require(modelWorkspaceTabs->currentIndex() == 0, "Library is the default Models tab");
            require(this->findChild<QPushButton*>("NavTrainerButton") == nullptr &&
                        this->findChild<QPushButton*>("NavValidatorButton") == nullptr,
                    "Trainer and Model Testing no longer have top-level navigation entries");
            auto* registryTable = this->findChild<QTableWidget*>("ModelWorkspaceRegistryTable");
            require(registryTable != nullptr, "Library model list exists");
            bool namesOnly = registryTable != nullptr;
            bool activeIconFound = false;
            bool activeEntryExpected = false;
            for (const QJsonValue& value : registryEntries) {
                const QJsonObject entry = value.toObject();
                activeEntryExpected = activeEntryExpected || entry.value("selectable_for_normal_live_sorting").toBool() ||
                                      registryString(entry, "state").contains("promoted", Qt::CaseInsensitive) ||
                                      registryString(entry, "promotion_status").contains("current", Qt::CaseInsensitive);
            }
            if (registryTable) {
                for (int row = 0; row < registryTable->rowCount(); ++row) {
                    const auto* item = registryTable->item(row, 0);
                    namesOnly = namesOnly && item && !item->text().contains('\n') && !item->text().contains("target:") &&
                                !item->text().contains("current live model", Qt::CaseInsensitive);
                    activeIconFound = activeIconFound || (item && !item->icon().isNull() && item->toolTip() == "Active model");
                }
            }
            require(namesOnly, "Library rows contain model names only");
            require(!activeEntryExpected || activeIconFound,
                    "An active model uses a separate check icon when the isolated registry has one");
            this->resize(1600, 1000);
            this->show();
            app.processEvents();
            auto* librarySplitter = modelWorkspacePage->findChild<QSplitter*>("ModelWorkspaceSplitter");
            require(librarySplitter && librarySplitter->width() * 3 <= modelWorkspacePage->width() * 2 + 6,
                    "Library primary content is constrained to two-thirds of the usable width");
            auto* libraryAddButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceAddModelButton");
            auto* libraryAddBlankButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceAddBlankModelButton");
            auto* libraryAddPretrainedButton =
                modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceAddPretrainedModelButton");
            auto* libraryRemoveButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceRemoveModelButton");
            auto* librarySetActiveButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceSetActiveButton");
            require(!libraryAddButton && libraryAddBlankButton && !libraryAddBlankButton->isHidden() &&
                        libraryAddPretrainedButton && !libraryAddPretrainedButton->isHidden() &&
                        libraryRemoveButton && !libraryRemoveButton->isHidden() &&
                        libraryRemoveButton->text() == "Remove model" && librarySetActiveButton &&
                        !librarySetActiveButton->isHidden() && librarySetActiveButton->text() == "Set Active",
                    "Library exposes explicit Blank/Pre-trained, Remove, and Set Active actions only");
            trainerNavButton->click();
            require(workspaceStack->currentWidget() == modelWorkspacePage && modelWorkspaceTabs->currentIndex() == 1,
                    "Trainer routes to the Models Train tab");
            require(trainerTrainingModeCombo->isHidden(), "Training-plan choice is removed from the Train setup");
            require(trainerAdvancedToggle->text() == "Hyperparameter Settings" && !trainerAdvancedToggle->isChecked(),
                    "Train uses one collapsed Hyperparameter Settings section");
            require(trainerFormTitle->text() == "SETUP" && trainerPathsGroup->title().isEmpty(),
                    "Setup is the actual Train section title without a redundant nested title");
            require(trainerDeviceCombo && trainerDeviceCombo->isVisible() &&
                        trainerDeviceCombo->findData("auto") >= 0 && trainerDeviceCombo->findData("cpu") >= 0 &&
                        trainerDeviceCombo->findData("cuda") >= 0,
                    "Train Setup exposes the shared Auto, CPU, and GPU device selector");
            require(!trainerEnvironmentPanel->isVisible() &&
                        trainerWorkspacePage->findChild<QWidget*>("TrainerEnvironmentPanel") == nullptr,
                    "Train Setup Details panel is absent");
            require(trainerLeftLayout->indexOf(trainerLogPanel) >= 0 && trainerLogPanel->isVisible() &&
                        !trainerLogToggle->isChecked() && trainerResultText->isHidden(),
                    "Train keeps controlled Status and a collapsed Detailed Log as the only raw surface");
            QJsonParseError hyperparameterParseError;
            const QJsonDocument hyperparameterDocument = QJsonDocument::fromJson(
                trainerHyperparameterJsonEdit->toPlainText().toUtf8(), &hyperparameterParseError);
            require(trainerArchitectureCombo->isHidden() && trainerPretrainedSegment->isHidden() &&
                        !trainerHyperparameterJsonEdit->isHidden() &&
                        hyperparameterParseError.error == QJsonParseError::NoError && hyperparameterDocument.isObject() &&
                        hyperparameterDocument.object().value("schema_version").toInt() == 2 &&
                        hyperparameterDocument.object().value("batch_size").toInt() == 64 &&
                        hyperparameterDocument.object().value("input_size").toArray() == QJsonArray{96, 96, 3} &&
                        hyperparameterDocument.object().value("stages").toArray().size() == 2 &&
                        qAbs(hyperparameterDocument.object().value("imbalance").toObject()
                                 .value("sampler_alpha").toDouble() - 0.65) < 0.0001,
                    "Train derives architecture from the selected model and exposes valid versioned staged JSON");
            require(!trainerStatusLabel->text().contains("Start from:", Qt::CaseInsensitive) &&
                        !trainerStatusLabel->text().contains("Training plan:", Qt::CaseInsensitive) &&
                        !trainerStatusLabel->text().contains("Save new model in:", Qt::CaseInsensitive),
                    "Main Status excludes legacy setup prose and paths");
            TrainerUiEvent actualSchemaEvent;
            require(parseTrainerUiEvent(
                        R"({"schema_version":1,"event":"epoch_metrics","stage":"fine_tune","epoch":3,"metrics":{"train_loss":0.42,"val_loss":0.51,"val_accuracy":0.875,"val_macro_f1":0.81,"elapsed_seconds":12.5}})",
                        &actualSchemaEvent) && actualSchemaEvent.type == "epoch_metrics" &&
                        actualSchemaEvent.stage == "fine_tune" && actualSchemaEvent.epoch == 3 &&
                        qAbs(actualSchemaEvent.trainLoss - 0.42) < 0.0001 &&
                        qAbs(actualSchemaEvent.validationLoss - 0.51) < 0.0001 &&
                        qAbs(actualSchemaEvent.accuracy - 0.875) < 0.0001 &&
                        qAbs(actualSchemaEvent.macroF1 - 0.81) < 0.0001,
                    "Actual trainer epoch_metrics schema populates progress metrics and plots");
            QVector<QPointF> twoStageLossHistory;
            QVector<QPointF> twoStageScoreHistory;
            bool twoStageParsed = true;
            for (int globalEpoch = 1; globalEpoch <= 14; ++globalEpoch) {
                const int stageEpoch = globalEpoch <= 6 ? globalEpoch : globalEpoch - 6;
                const QString stage = globalEpoch <= 6 ? QStringLiteral("head") : QStringLiteral("fine_tune");
                TrainerUiEvent parsed;
                const QString json = QString(
                    "{\"event\":\"epoch_metrics\",\"stage\":\"%1\",\"epoch\":%2,"
                    "\"global_epoch\":%3,\"metrics\":{\"train_loss\":0.4,\"val_accuracy\":0.8}}")
                    .arg(stage).arg(stageEpoch).arg(globalEpoch);
                twoStageParsed = twoStageParsed && parseTrainerUiEvent(json, &parsed) &&
                                 parsed.stageEpoch == stageEpoch && parsed.globalEpoch == globalEpoch;
                upsertTrainerHistoryPoint(twoStageLossHistory, parsed.globalEpoch, parsed.trainLoss);
                upsertTrainerHistoryPoint(twoStageScoreHistory, parsed.globalEpoch, parsed.accuracy);
            }
            upsertTrainerHistoryPoint(twoStageLossHistory, 7, 0.39);
            bool monotonicUnique = twoStageLossHistory.size() == 14 && twoStageScoreHistory.size() == 14;
            for (int index = 0; index < twoStageLossHistory.size(); ++index) {
                monotonicUnique = monotonicUnique && qRound(twoStageLossHistory.at(index).x()) == index + 1 &&
                                  qRound(twoStageScoreHistory.at(index).x()) == index + 1;
            }
            require(twoStageParsed && monotonicUnique,
                    "Two-stage plot histories use unique monotonic global epochs 1..14 without backward segments");
            for (const QString& eventName : {QString("run_started"), QString("environment"),
                                             QString("dataset_summary"), QString("warning")}) {
                TrainerUiEvent parsed;
                require(parseTrainerUiEvent(QString("{\"event\":\"%1\"}").arg(eventName), &parsed) &&
                            parsed.type == eventName,
                        "Trainer parses actual event " + eventName);
            }
            bool trainerNamesOnly = true;
            for (int index = 0; index < trainerStartingModelCombo->count(); ++index) {
                const QString text = trainerStartingModelCombo->itemText(index);
                trainerNamesOnly = trainerNamesOnly && !text.contains('|') && !text.contains("target:", Qt::CaseInsensitive) &&
                                   !text.contains("current live model", Qt::CaseInsensitive);
            }
            require(trainerNamesOnly, "Train model choices show names only");
            auto* lossCurve = this->findChild<QFrame*>("TrainerLossCurve");
            auto* performanceCurve = this->findChild<QFrame*>("TrainerPerformanceCurve");
            require(trainerLeftLayout->indexOf(trainerResultsPanel) >= 0 && lossCurve && performanceCurve,
                    "Train retains Results metrics and both learning-curve plots");
            auto* lossCanvas = this->findChild<QLabel*>("TrainerLossCurveValues");
            auto* performanceCanvas = this->findChild<QLabel*>("TrainerPerformanceCurveValues");
            require(trainerResultsPanel->isVisible() && lossCurve->isVisible() && performanceCurve->isVisible() &&
                        lossCanvas && !lossCanvas->pixmap().isNull() && performanceCanvas &&
                        !performanceCanvas->pixmap().isNull(),
                    "Train Results and both blank plot canvases are visible before training");
            require(lossCurve->property("xAxisLabel") == "Epoch" && lossCurve->property("yAxisLabel") == "Loss" &&
                        performanceCurve->property("xAxisLabel") == "Epoch" &&
                        performanceCurve->property("yAxisLabel") == "Score" &&
                        lossCurve->property("gridVisible").toBool() && lossCurve->property("legendVisible").toBool() &&
                        performanceCurve->property("gridVisible").toBool() &&
                        performanceCurve->property("legendVisible").toBool(),
                    "Blank plots retain Epoch/Loss/Score axes, numeric grids, and legends");
            require(lossCurve && performanceCurve && lossCurve->minimumHeight() >= 220 &&
                        lossCurve->maximumHeight() <= 260 && performanceCurve->minimumHeight() >= 220 &&
                        performanceCurve->maximumHeight() <= 260,
                    "Train learning-curve regions use constrained full-width heights");
            const QString plotMetricsPath =
                qEnvironmentVariable("OVDS_VERIFY_TRAINER_PLOT_METRICS_JSON").trimmed();
            if (!plotMetricsPath.isEmpty()) {
                const QJsonObject metrics = loadRegistryObjectForVerifier(plotMetricsPath);
                QVector<QPointF> trainLoss;
                QVector<QPointF> validationLoss;
                QVector<QPointF> validationAccuracy;
                QVector<QPointF> macroF1;
                for (const QJsonValue& value : metrics.value("history").toArray()) {
                    const QJsonObject row = value.toObject();
                    const int epoch = row.value("global_epoch").toInt(row.value("epoch").toInt());
                    if (epoch <= 0)
                        continue;
                    trainLoss.push_back(QPointF(epoch, row.value("train_loss").toDouble()));
                    validationLoss.push_back(QPointF(epoch, row.value("val_loss").toDouble()));
                    validationAccuracy.push_back(QPointF(epoch, row.value("val_accuracy").toDouble()));
                    macroF1.push_back(QPointF(epoch, row.value("val_macro_f1").toDouble()));
                }
                require(!trainLoss.isEmpty(), "Completed trainer metrics provide plot epochs");
                renderTrainerCurves(lossCanvas, trainLoss, validationLoss,
                                    QColor(42, 124, 201), QColor(222, 118, 42));
                renderTrainerCurves(performanceCanvas, validationAccuracy, macroF1,
                                    QColor(38, 151, 96), QColor(112, 83, 196));
            }
            validatorNavButton->click();
            require(workspaceStack->currentWidget() == modelWorkspacePage && modelWorkspaceTabs->currentIndex() == 2,
                    "Model testing routes to the Models Test tab");
            auto* classMetrics = this->findChild<QTableWidget*>("ValidatorWorkspaceClassMetricsTable");
            require(classMetrics && classMetrics->columnCount() == 5 &&
                        classMetrics->horizontalHeaderItem(0)->text() == "Class" &&
                        classMetrics->horizontalHeaderItem(4)->text() == "Correct / Total",
                    "Test exposes the required per-class performance columns");
            QStringList testLabels;
            for (auto* label : validatorWorkspacePage->findChildren<QLabel*>())
                testLabels << label->text();
            require(testLabels.contains("Accuracy") && testLabels.contains("Macro F1") &&
                        testLabels.contains("Correct / Total") && testLabels.contains("Incorrect"),
                    "Test prioritizes Accuracy, Macro F1, Correct / Total, and Incorrect");
            require(!testLabels.contains("Images checked") && !testLabels.contains("Review summary"),
                    "Test removes count and review-summary cards");
            bool rawTestTextHidden = true;
            for (auto* rawText : validatorWorkspacePage->findChildren<QPlainTextEdit*>())
                rawTestTextHidden = rawTestTextHidden && rawText->isHidden();
            require(!testLabels.contains("Details") && rawTestTextHidden,
                    "Test workspace has no visible Details label or raw JSON viewer");
            const QString captureDirectory = qEnvironmentVariable("OVDS_CAPTURE_MODELS_UI_DIR").trimmed();
            if (!captureDirectory.isEmpty()) {
                require(QDir().mkpath(captureDirectory), "Release UI capture directory exists");
                this->resize(1600, 1000);
                this->show();
                const QStringList captureNames = {"Library", "Train", "Test"};
                for (int tab = 0; tab < captureNames.size(); ++tab) {
                    modelWorkspaceTabs->setCurrentIndex(tab);
                    if (tab == 1) {
                        trainerLeftScroll->verticalScrollBar()->setValue(
                            trainerLeftScroll->verticalScrollBar()->maximum());
                    }
                    app.processEvents();
                    const QString capturePath = QDir(captureDirectory).filePath(captureNames.at(tab) + ".png");
                    require(this->grab().save(capturePath), "Captured Release " + captureNames.at(tab) + " UI");
                    auto* captureModelCombo =
                        tab == 2 ? this->findChild<QComboBox*>("ValidatorWorkspaceModelCombo") : nullptr;
                    if (captureModelCombo && captureModelCombo->count() > 0) {
                        captureModelCombo->showPopup();
                        app.processEvents();
                        const QString openComboCapture =
                            QDir(captureDirectory).filePath("TestModelsOpen.png");
                        require(captureModelCombo->view() &&
                                    captureModelCombo->view()->grab().save(openComboCapture),
                                "Captured Test model list with all eligible names visible");
                        captureModelCombo->hidePopup();
                    }
                }
            }
            if (!failures.isEmpty())
                verifierTrace(QStringLiteral("verify-models: failures: %1").arg(failures.join("; ")));
            verifierTrace(QStringLiteral("verify-models: assertions complete"));
            return failures.isEmpty() ? 0 : 2;
        };
        const int verifierResult = runModelsWorkspaceVerification();
        verifierTrace(QStringLiteral("verify-models: returning %1").arg(verifierResult));
        return verifierResult;
    }

    QObject::connect(trainerDockProxyButton, &QPushButton::clicked, [&]() {
        trainerNavButton->click();
        trainerPythonEdit->setFocus();
    });
    auto logDock = new QDockWidget("Logs", this);
    logDock->setObjectName("LogsDock");
    auto logDockText = new QPlainTextEdit;
    nameWidget(logDockText, "LogsTextEdit");
    logDockText->setObjectName("LogsText");
    logDockText->setReadOnly(true);
    logDockText->setPlainText("Session log: " + logPath +
                              "\n\nLive log streaming remains handled by session_log.txt in this shell step.");
    logDock->setWidget(logDockText);
    logDock->setMinimumHeight(36);
    this->addDockWidget(Qt::BottomDockWidgetArea, logDock);
    this->resizeDocks({logDock}, {72}, Qt::Vertical);
    logDock->hide();

    auto diagnosticsDock = new QDockWidget("System Diagnostics", this);
    diagnosticsDock->setObjectName("SystemDiagnosticsDock");
    auto diagnosticsLabel =
        new QLabel("Application: shell loaded\n"
                   "Camera/DCAM: checked by existing startup path\n"
                   "Model: loaded through existing Pipeline controls\n"
                   "DAQ: configured in Devices > DAQ / Trigger\n"
                   "Python trainer: readiness checks available in Trainer tab\n"
                   "Model Testing workspace: open from Model Testing > Test Model\n"
                   "Training launch, runner-wrapped sequence validation, and active-model management: disabled placeholders");
    nameWidget(diagnosticsLabel, "SystemDiagnosticsLabel");
    diagnosticsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    diagnosticsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    diagnosticsLabel->setWordWrap(true);
    diagnosticsDock->setWidget(diagnosticsLabel);
    this->addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
    diagnosticsDock->hide();

    auto cameraStatusItem = new QLabel("Camera: startup pending");
    auto modelStatusItem = new QLabel("Model: not loaded");
    auto daqStatusItem = new QLabel(initialDaqStatusText);
    auto pythonStatusItem = new QLabel("Python: not configured");
    auto runStatusItem = new QLabel("Run: idle");
    nameWidget(cameraStatusItem, "CameraStatusBarLabel");
    nameWidget(modelStatusItem, "ModelStatusBarLabel");
    nameWidget(daqStatusItem, "DaqStatusBarLabel");
    nameWidget(pythonStatusItem, "PythonStatusBarLabel");
    nameWidget(runStatusItem, "RunStatusBarLabel");
    nameWidget(this->statusBar(), "StatusBar");
    for (auto* item : {cameraStatusItem, modelStatusItem, daqStatusItem, pythonStatusItem, runStatusItem}) {
        item->setFrameStyle(QFrame::NoFrame);
        this->statusBar()->addPermanentWidget(item);
    }
    this->statusBar()->showMessage("Shell ready");
    this->menuBar()->hide();
    this->statusBar()->hide();

    auto setHeaderChipText = [](QLabel* label, const QString& text, int maximumWidth = 220) {
        if (!label)
            return;
        const int horizontalPadding = 28;
        const int textWidth = qMax(24, maximumWidth - horizontalPadding);
        const QString visibleText = label->fontMetrics().elidedText(text, Qt::ElideRight, textWidth);
        const int targetWidth =
            qMin(maximumWidth, label->fontMetrics().horizontalAdvance(visibleText) + horizontalPadding);
        label->setMinimumWidth(qMax(72, targetWidth));
        label->setToolTip(text);
        label->setText(visibleText);
    };
    auto statusValue = [](QString text, const QString& prefix) {
        text = text.simplified();
        const QString marker = prefix + ":";
        if (text.startsWith(marker, Qt::CaseInsensitive)) {
            text = text.mid(marker.size()).trimmed();
        }
        return text;
    };
    auto titleCaseStatus = [](QString text) {
        text = text.simplified();
        if (text.isEmpty())
            return text;
        text[0] = text[0].toUpper();
        return text;
    };
    auto runHeaderText = [&](const QString& runText, const QString& statusText) {
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("live view"))
            return QStringLiteral("Live View");
        if (run.contains("capture"))
            return QStringLiteral("Camera capture");
        if (run.contains("viewer-only"))
            return QStringLiteral("Camera viewer-only");
        if (run == QStringLiteral("idle"))
            return QStringLiteral("Idle");
        const QString simplifiedStatus = statusText.simplified();
        return simplifiedStatus.isEmpty() ? QStringLiteral("Idle") : simplifiedStatus;
    };
    auto cameraHeaderText = [&](const QString& cameraText, const QString& runText) {
        const QString camera = statusValue(cameraText, "Camera").toLower();
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("viewer-only") || camera.contains("unavailable"))
            return QStringLiteral("Camera viewer-only");
        if (camera.contains("acquiring"))
            return QStringLiteral("Camera acquiring");
        if (camera.contains("connected"))
            return QStringLiteral("Camera connected");
        if (camera.contains("error"))
            return QStringLiteral("Camera error");
        return QStringLiteral("Camera startup");
    };
    auto modelHeaderText = [&](const QString& modelText) {
        const QString modelValue = statusValue(modelText, "Model");
        const QString model = modelValue.toLower();
        if (model == QStringLiteral("loaded")) {
            QString selectedModel;
            const QString selectedId = liveModelCombo
                                           ? liveModelCombo->currentData(Qt::UserRole + 1).toString().trimmed()
                                           : QString();
            for (const QJsonValue& value : registryEntries) {
                const QJsonObject entry = value.toObject();
                if (registryString(entry, "registry_entry_id").compare(selectedId, Qt::CaseInsensitive) == 0) {
                    selectedModel = registryString(entry, "display_name").trimmed();
                    break;
                }
            }
            return selectedModel.isEmpty() ? QStringLiteral("Model loaded") : selectedModel;
        }
        if (model.contains("missing") || model.contains("invalid") || model.contains("could not"))
            return titleCaseStatus(modelValue);
        return QStringLiteral("Model not loaded");
    };
    auto daqHeaderText = [&](const QString& daqText) {
        const QString daq = statusValue(daqText, "DAQ").toLower();
        if (daq.contains("disabled") || daq.contains("unavailable"))
            return QStringLiteral("DAQ unavailable");
        if (daq.contains("available"))
            return QStringLiteral("DAQ available");
        return QStringLiteral("DAQ unchecked");
    };
    auto triggerHeaderText = [&](const QString& triggerText) {
        const QString trigger = triggerText.simplified().toLower();
        if (trigger.contains("queued"))
            return QStringLiteral("Trigger queued");
        if (trigger.contains("sent"))
            return QStringLiteral("Trigger sent");
        if (trigger.contains("failed"))
            return QStringLiteral("Trigger failed");
        if (appState.daqDisabled)
            return QStringLiteral("DAQ disabled");
        if (liveForceTriggerBtn->isEnabled())
            return QStringLiteral("Manual trigger ready");
        return QStringLiteral("Manual trigger blocked");
    };

    auto shellStatusMirrorTimer = new QTimer(this);
    shellStatusMirrorTimer->setInterval(500);
    QObject::connect(shellStatusMirrorTimer, &QTimer::timeout, [=, &appState]() {
        shellRuntimeStatus->setText(runStatusItem->text());
        shellCameraStatus->setText(cameraStatusItem->text());
        shellModelStatus->setText(modelStatusItem->text());
        shellDaqStatus->setText(daqStatusItem->text());
        appState.cameraStreaming = cameraStatusItem->text().contains("acquiring", Qt::CaseInsensitive);
        appState.daqStatusText = daqStatusItem->text();
        const bool daqTextUnavailable = daqStatusItem->text().contains("unavailable", Qt::CaseInsensitive);
        appState.daqAvailable = !daqTextUnavailable && daqStatusItem->text().contains("available", Qt::CaseInsensitive);
        appState.daqDisabled = daqStatusItem->text().contains("disabled", Qt::CaseInsensitive);
        appState.daqFault = daqTextUnavailable;
        updateForceTriggerState();
        const int headerWidth = shellHeader->width();
        const bool compactHeader = headerWidth > 0 && headerWidth < 1380;
        const bool narrowHeader = headerWidth > 0 && headerWidth < 1120;
        headerCameraChip->setVisible(!compactHeader);
        headerTriggerChip->setVisible(!compactHeader);
        headerModelChip->setVisible(!narrowHeader);
        headerDaqChip->setVisible(!narrowHeader);
        setHeaderChipText(headerCameraChip, cameraHeaderText(cameraStatusItem->text(), runStatusItem->text()), 170);
        setHeaderChipText(headerModelChip, modelHeaderText(modelStatusItem->text()), 160);
        setHeaderChipText(headerDaqChip, daqHeaderText(daqStatusItem->text()), 145);
        const QString triggerChipText = triggerHeaderText(statusLabel->text());
        setHeaderChipText(headerTriggerChip, triggerChipText, 170);
        shellHeaderLayout->invalidate();
        headerDaqChip->setProperty(
            "chipTone", (daqStatusItem->text().contains("disabled") || daqStatusItem->text().contains("unavailable"))
                            ? "disabled"
                            : (daqStatusItem->text().contains("available") ? "running" : "warn"));
        headerModelChip->setProperty("chipTone", modelStatusItem->text().contains("loaded")
                                                     ? "running"
                                                     : (modelStatusItem->text().contains("error") ? "error" : "warn"));
        headerCameraChip->setProperty(
            "chipTone", cameraStatusItem->text().contains("connected") || cameraStatusItem->text().contains("acquiring")
                            ? "running"
                            : (cameraStatusItem->text().contains("error") ? "error" : "warn"));
        headerTriggerChip->setProperty(
            "chipTone", triggerChipText.contains("sent", Qt::CaseInsensitive) ||
                                triggerChipText.contains("ready", Qt::CaseInsensitive)
                            ? "running"
                            : (triggerChipText.contains("failed", Qt::CaseInsensitive)
                                   ? "error"
                                   : (triggerChipText.contains("disabled", Qt::CaseInsensitive) ? "disabled" : "warn")));
        headerCameraChip->style()->unpolish(headerCameraChip);
        headerCameraChip->style()->polish(headerCameraChip);
        headerModelChip->style()->unpolish(headerModelChip);
        headerModelChip->style()->polish(headerModelChip);
        headerDaqChip->style()->unpolish(headerDaqChip);
        headerDaqChip->style()->polish(headerDaqChip);
        headerTriggerChip->style()->unpolish(headerTriggerChip);
        headerTriggerChip->style()->polish(headerTriggerChip);

        const QRegularExpression intRe("(\\d+)");
        auto firstNumberAfter = [&](const QString& text, const QString& marker, const QString& fallback) {
            const int markerIndex = text.indexOf(marker);
            if (markerIndex < 0)
                return fallback;
            auto match = intRe.match(text, markerIndex + marker.size());
            return match.hasMatch() ? match.captured(1) : fallback;
        };
        eventsMetricLabel->setText(firstNumberAfter(statsEventsLabel->text(), "Events:", "0"));
        classifiedHitMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Classified Sort:", "0"));
        classifiedWasteMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Classified Pass:", "0"));
        wentToHitMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Went to Sort:", "0"));
        wentToWasteMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Went to Pass:", "0"));
        trigMetricLabel->setText(pipelineEnableCheck->isChecked() ? "live" : "--");
        lastDecisionValue->setText(statsLastLabel->text().contains("--") ? "--" : statsLastLabel->text().simplified());
    });
    shellStatusMirrorTimer->start();

    QObject::connect(fitAction, &QAction::triggered, [&]() {
        imageView->fitToView();
        cameraImageView->fitToView();
        this->statusBar()->showMessage("Preview images fit to view");
    });
    QObject::connect(oneToOneAction, &QAction::triggered, [=]() {
        imageView->resetScale();
        cameraImageView->resetScale();
    });
    QObject::connect(zoomInAction, &QAction::triggered, [=]() {
        imageView->zoomBySteps(1);
        cameraImageView->zoomBySteps(1);
    });
    QObject::connect(zoomOutAction, &QAction::triggered, [=]() {
        imageView->zoomBySteps(-1);
        cameraImageView->zoomBySteps(-1);
    });
    QObject::connect(leftLoadBtn, &QPushButton::clicked, viewerBtn, &QPushButton::click);
    QObject::connect(leftReconnectBtn, &QPushButton::clicked, reconnectBtn, &QPushButton::click);
    QObject::connect(openViewerAction, &QAction::triggered, viewerBtn, &QPushButton::click);
    QObject::connect(reconnectAction, &QAction::triggered, reconnectBtn, &QPushButton::click);
    QObject::connect(startPreviewAction, &QAction::triggered, startBtn, &QPushButton::click);
    QObject::connect(stopPreviewAction, &QAction::triggered, [&]() {
        if (appState.cameraStreaming) {
            startBtn->click();
        }
    });
    QObject::connect(captureStillAction, &QAction::triggered, captureBtn, &QPushButton::click);
    QObject::connect(startSortingAction, &QAction::triggered, pipelineStartBtn, &QPushButton::click);
    QObject::connect(stopSortingAction, &QAction::triggered, pipelineStopBtn, &QPushButton::click);
    QObject::connect(manualTriggerAction, &QAction::triggered, liveForceTriggerBtn, &QPushButton::click);
    QObject::connect(liveSnapshotBtn, &QPushButton::clicked, captureBtn, &QPushButton::click);
    QObject::connect(liveDetectorTuningBtn, &QPushButton::clicked, [&]() {
        liveDetectorDrawerOverlay->setVisible(!liveDetectorDrawerOverlay->isVisible());
        if (liveDetectorDrawerOverlay->isVisible())
            liveDetectorDrawerOverlay->raise();
    });
    QObject::connect(liveDetectorClose, &QToolButton::clicked, liveDetectorDrawerOverlay, &QWidget::hide);
    ReportsWorkspaceController::Dependencies reportsWorkspaceDeps;
    reportsWorkspaceDeps.openRunFolderAction = openRunFolderAction;
    reportsWorkspaceDeps.openOutputAction = openOutputAction;
    reportsWorkspaceDeps.liveOpenRunButton = liveOpenRunBtn;
    reportsWorkspaceDeps.showLogsAction = showLogsAction;
    reportsWorkspaceDeps.showDiagnosticsAction = showDiagnosticsAction;
    reportsWorkspaceDeps.systemDiagnosticsAction = systemDiagnosticsAction;
    reportsWorkspaceDeps.logDock = logDock;
    reportsWorkspaceDeps.diagnosticsDock = diagnosticsDock;
    reportsWorkspaceDeps.statusLabel = statusLabel;
    reportsWorkspaceDeps.statusBar = this->statusBar();
    ReportsWorkspaceController reportsWorkspaceController(reportsWorkspaceDeps, this);
    QObject::connect(resetLayoutAction, &QAction::triggered, [&]() {
        this->addDockWidget(Qt::BottomDockWidgetArea, logDock);
        this->addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
        logDock->show();
        diagnosticsDock->hide();
        if (imageSubWindow) {
            imageSubWindow->show();
            imageSubWindow->resize(760, 620);
            imageSubWindow->move(16, 16);
        }
    });
    QObject::connect(exitAction, &QAction::triggered, this, &QWidget::close);
    QObject::connect(aboutAction, &QAction::triggered, [&]() {
        auto* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("About OpenDSS");
        dialog->setMinimumWidth(520);
        nameWidget(dialog, "OpenDssAboutDialog");
        auto* layout = new QVBoxLayout(dialog);
        auto* title = new QLabel("OpenDSS");
        title->setProperty("sectionTitle", true);
        layout->addWidget(title);
        const QString github = QStringLiteral("https://github.com/haeminjung12/OpenDSS_clean");
        const QString docs = github + QStringLiteral("#readme");
        const QString support = QStringLiteral("haeminjung@tamu.edu");
        const QString version = QCoreApplication::applicationVersion();
        auto* details = new QLabel(
            QStringLiteral("<b>Software version:</b> %4<br><br>"
                           "<b>GitHub Repository:</b><br><a href=\"%1\">%1</a><br><br>"
                           "<b>Documentation:</b><br><a href=\"%2\">%2</a><br><br>"
                           "<b>Support:</b><br><a href=\"mailto:%3\">%3</a>")
                .arg(github, docs, support, version));
        nameWidget(details, "OpenDssAboutDetails");
        details->setTextFormat(Qt::RichText);
        details->setTextInteractionFlags(Qt::TextBrowserInteraction);
        details->setOpenExternalLinks(true);
        details->setWordWrap(true);
        layout->addWidget(details);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        auto* copyButton = buttons->addButton("Copy information", QDialogButtonBox::ActionRole);
        nameWidget(copyButton, "OpenDssAboutCopyButton");
        QObject::connect(copyButton, &QPushButton::clicked, dialog, [github, docs, support, version]() {
            QGuiApplication::clipboard()->setText(
                QStringLiteral("OpenDSS\nSoftware version: %4\nGitHub Repository: %1\nDocumentation: %2\nSupport: %3")
                    .arg(github, docs, support, version));
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog->open();
    });
    QObject::connect(documentationAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/haeminjung12/OpenDSS_clean#readme")));
    });

    // Logging helper
    auto logLine = [&](const QString& msg) {
        if (!logCheck->isChecked())
            return;
        logMessage(msg);
    };

    auto buildRunOutputDir = [&](const QString& prefix) -> QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty())
            base = QCoreApplication::applicationDirPath();
        QDir baseDir(base);
        QString leaf = baseDir.dirName();
        if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
            baseDir.cdUp();
        }
        baseDir.mkpath(".");
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString runName = QString("%1_%2").arg(prefix, stamp);
        QString runDir = baseDir.filePath(runName);
        baseDir.mkpath(runName);
        return runDir;
    };
    reportsWorkspaceController.refreshOpenRunAvailability();

    auto buildDatasetBuilderDir = [&](QString* datasetIdOut) -> QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty())
            base = QCoreApplication::applicationDirPath();
        QDir baseDir(base);
        QString leaf = baseDir.dirName();
        if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
            baseDir.cdUp();
        }
        QString shortId = QUuid::createUuid().toString(QUuid::Id128).left(6).toLower();
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString datasetId = QString("builder_%1_live_%2").arg(stamp, shortId);
        if (datasetIdOut)
            *datasetIdOut = datasetId;
        QDir root(baseDir.filePath("datasets/builder"));
        root.mkpath(".");
        root.mkpath(datasetId);
        return root.filePath(datasetId);
    };

    QString appDir = QCoreApplication::applicationDirPath();
    auto findModelUpwards = [&](const QString& filename) -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 6; ++i) {
            QString candidate = dir.filePath("models/" + filename);
            if (QFileInfo::exists(candidate))
                return candidate;
            if (!dir.cdUp())
                break;
        }
        const QString projectRoot = findProjectRootFromApp();
        if (!projectRoot.isEmpty()) {
            const QString promotedArtifact = runtimeModelArtifactPath(projectRoot, "app/runtime/models/" + filename);
            if (!promotedArtifact.isEmpty())
                return promotedArtifact;
        }
        return QString();
    };
    auto findOutputsRootUpwards = [&]() -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 8; ++i) {
            QString outputsDir = dir.filePath("outputs");
            if (QFileInfo(outputsDir).isDir()) {
                return QDir(outputsDir).filePath("pipeline_output");
            }
            if (!dir.cdUp())
                break;
        }
        return QString();
    };
    auto findProjectRootUpwards = [&]() -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return dir.absolutePath();
            }
            if (!dir.cdUp())
                break;
        }
        QDir cwd(QDir::currentPath());
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(cwd.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return cwd.absolutePath();
            }
            if (!cwd.cdUp())
                break;
        }
        return QString();
    };
    const auto trainerModulePath = [&]() -> QString { return validatorTrainerPythonPath(); };
    QString projectRoot = findProjectRootUpwards();
    if (projectRoot.isEmpty()) {
        const QString trainerPythonPath = trainerModulePath();
        if (!trainerPythonPath.isEmpty()) {
            QDir dir(trainerPythonPath);
            dir.cdUp();
            dir.cdUp();
            projectRoot = dir.absolutePath();
        }
    }
    auto refreshTrainerSetupDetails = [=]() {
        const QString python = trainerPythonEdit->text().trimmed();
        const QString helperPath = trainerModulePath();
        const QString device = selectedComputeDevice();
        const QString cpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv"));
        const QString gpuPython = documentedTrainerPythonExecutable(QStringLiteral("training-venv-gpu"));

        if (python.isEmpty()) {
            trainerPythonStatusValue->setText("Missing: choose a Python executable.");
        } else if (QFileInfo(python).isFile()) {
            trainerPythonStatusValue->setText("Found: " + QDir::toNativeSeparators(python));
        } else {
            trainerPythonStatusValue->setText("Missing: " + QDir::toNativeSeparators(python));
        }

        trainerHelperStatusValue->setText(
            helperPath.isEmpty() ? "Missing: packaged training/python helper folder was not found."
                                 : "Found: " + QDir::toNativeSeparators(helperPath));

        QString deviceText = device == QLatin1String("cuda") ? QStringLiteral("GPU")
                           : device == QLatin1String("cpu") ? QStringLiteral("CPU")
                                                            : QStringLiteral("Auto");
        if (device != QLatin1String("cuda") && sameCleanPath(python, gpuPython) && QFileInfo(cpuPython).isFile()) {
            deviceText += "; GPU venv is selected while CPU venv is available.";
        } else if (device == QLatin1String("cuda") && !QFileInfo(gpuPython).isFile() && QFileInfo(cpuPython).isFile()) {
            deviceText += "; GPU venv missing, CPU venv is available.";
        }
        trainerDeviceStatusValue->setText(deviceText);
        if (trainerPackagesStatusValue->text().trimmed().isEmpty())
            trainerPackagesStatusValue->setText("Run Check Python setup to inspect training and ONNX packages.");
    };
    auto updateTrainerSetupDetailsFromEnvCheck = [=](const QString& output, int exitCode, bool crashed) {
        refreshTrainerSetupDetails();
        QString envStatus = crashed ? QStringLiteral("Env check crashed.") : QString("Env check exit code: %1").arg(exitCode);
        QString packageStatus;
        QString deviceStatus;
        const QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        for (const QString& rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (!line.startsWith('{'))
                continue;
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
                continue;
            const QJsonObject payload = doc.object();
            if (payload.value("command").toString() != QLatin1String("env-check"))
                continue;

            envStatus = payload.value("status").toString(exitCode == 0 ? "ok" : "error");
            const QJsonObject packages = payload.value("packages").toObject();
            QStringList missingPackages;
            for (auto it = packages.constBegin(); it != packages.constEnd(); ++it) {
                if (it.value().toObject().value("status").toString() != QLatin1String("ok"))
                    missingPackages << it.key();
            }
            packageStatus = missingPackages.isEmpty() ? QStringLiteral("Training and ONNX packages are importable.")
                                                      : QStringLiteral("Missing: ") + missingPackages.join(", ");

            const QJsonObject devices = payload.value("devices").toObject();
            const bool cudaAvailable = devices.value("cuda_available").toBool(false);
            const QString selected = devices.value("selected").toString("cpu");
            const QString cudaVersion = devices.value("cuda_version").toString();
            const QJsonArray gpuNames = devices.value("gpu_names").toArray();
            QStringList gpuNameStrings;
            for (const auto& nameValue : gpuNames)
                gpuNameStrings << nameValue.toString();
            deviceStatus = QString("Selected: %1; CUDA: %2")
                               .arg(selected, cudaAvailable ? QStringLiteral("available") : QStringLiteral("unavailable"));
            if (!cudaVersion.isEmpty())
                deviceStatus += "; CUDA " + cudaVersion;
            if (!gpuNameStrings.isEmpty())
                deviceStatus += "; GPU: " + gpuNameStrings.join(", ");
            break;
        }
        trainerEnvCheckStatusValue->setText(envStatus);
        if (!packageStatus.isEmpty())
            trainerPackagesStatusValue->setText(packageStatus);
        if (!deviceStatus.isEmpty())
            trainerDeviceStatusValue->setText(deviceStatus);
    };
    refreshTrainerSetupDetails();
    QObject::connect(trainerPythonEdit, &QLineEdit::textChanged, this, refreshTrainerSetupDetails);
    QObject::connect(computeDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                     [refreshTrainerSetupDetails](int) { refreshTrainerSetupDetails(); });
    auto projectPath = [&](const QString& rel) -> QString {
        return projectRoot.isEmpty() ? QString() : QDir(projectRoot).absoluteFilePath(rel);
    };
    QString defaultTrainerOutput = defaultWorkspacePaths.trainingRuns;
    QString defaultTrainerDataset = defaultWorkspacePaths.preparedDataset;
    datasetController = new DatasetWorkspaceController(
        DatasetWorkspaceController::Dependencies{
            this,
            defaultTrainerDataset,
            defaultTrainerOutput,
            trainerPythonEdit,
            trainerDatasetEdit,
            trainerOutputEdit,
            trainerPythonBrowseBtn,
            trainerDatasetBrowseBtn,
            trainerOutputBrowseBtn,
            trainerEnvCheckBtn,
            trainerConfigurePathBtn,
            trainerCancelBtn,
            trainerStartTrainingBtn,
            trainerDryRunBtn,
            trainerStatusLabel,
            trainerResultText,
            trainerProgressBar,
            &registryEntries,
            registryFilePath,
            trainerStartingModelCombo,
            trainerTrainingModeCombo,
            trainerStartingModelHintLabel,
            trainerArchitectureCombo,
            trainerPretrainedImageNetBtn,
            trainerPretrainedNoneBtn,
            trainerEpochsSpin,
            trainerBatchSpin,
            trainerLrSpin,
            trainerHyperparameterJsonEdit,
            trainerSelectedArchitectureValue,
            trainerFlipCheck,
            trainerRotationCheck,
            trainerColorJitterCheck,
            trainerRandomCropCheck,
            trainerSchedulerCombo,
            datasetOpenAction,
            datasetBuildAction,
            datasetLabelDatasetAction,
        },
        this);
    auto openDatasetLabelerPath = [datasetController](const QString& preferredPath) {
        datasetController->openDatasetLabelerPath(preferredPath);
    };
    QProcess* trainerProcess = nullptr;
    bool trainerCommandWasTraining = false;
    bool trainerCommandWasDryRun = false;
    bool trainerStopRequested = false;
    QString trainerStdoutLog;
    QString trainerStdoutLineBuffer;
    QVector<QPointF> trainerTrainLossHistory;
    QVector<QPointF> trainerValidationLossHistory;
    QVector<QPointF> trainerAccuracyHistory;
    QVector<QPointF> trainerF1History;
    QElapsedTimer trainerElapsed;
    int trainerBestEpoch = 0;
    double trainerBestAccuracy = -1.0;
    QString savedTrainerModelEntryId;
    auto appendTrainerLog = [datasetController](const QString& text) { datasetController->appendTrainerLog(text); };
    auto trainerCommandPreview = [datasetController](const QString& program, const QStringList& args) {
        return datasetController->trainerCommandPreview(program, args);
    };
    auto saveTrainerSettings = [datasetController]() { datasetController->saveTrainerSettings(); };
    auto presentTrainerEvent = [&](const TrainerUiEvent& event) {
        const auto percentageText = [](double value) {
            return value < 0.0 ? QStringLiteral("--") : QString::number(value <= 1.0 ? value * 100.0 : value, 'f', 1) + "%";
        };
        if (event.percent >= 0.0) {
            trainerProgressBar->setRange(0, 100);
            trainerProgressBar->setValue(qBound(0, qRound(event.percent), 100));
            trainerProgressBar->setFormat(QString("%1%").arg(trainerProgressBar->value()));
        } else if (event.epoch > 0 && event.epochs > 0) {
            trainerProgressBar->setRange(0, 100);
            trainerProgressBar->setValue(qBound(0, qRound(100.0 * event.epoch / event.epochs), 100));
            trainerProgressBar->setFormat(QString("Epoch %1 of %2").arg(event.epoch).arg(event.epochs));
        }

        if (event.type.contains("dataset")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText("Preparing the training dataset..."));
        } else if (event.type.contains("validation")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText("Checking model performance..."));
        } else if (event.type == QStringLiteral("checkpoint_loaded")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText(
                "Loaded the selected model weights. Preparing baseline evaluation..."));
        } else if (event.type.contains("checkpoint")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText(
                QString("Best result improved to %1 accuracy.").arg(percentageText(event.accuracy))));
        } else if (event.type.contains("saving") || event.type.contains("export")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText("Saving the trained model..."));
        } else if (event.type.contains("error") || event.type.contains("failed")) {
            trainerStatusLabel->setText(datasetController->trainerSummaryText(
                "Training could not continue.", trainerPlainLanguageError(event.errorCode)));
        } else if (event.epoch > 0) {
            QString detail;
            if (event.batch > 0 && event.batches > 0)
                detail = QString("Batch %1 of %2").arg(event.batch).arg(event.batches);
            trainerStatusLabel->setText(datasetController->trainerSummaryText(
                QString("Training epoch %1%2").arg(event.epoch).arg(event.epochs > 0 ? QString(" of %1").arg(event.epochs) : QString()), detail));
        }

        if (event.trainLoss >= 0.0 || event.validationLoss >= 0.0) {
            if (event.trainLoss >= 0.0)
                upsertTrainerHistoryPoint(trainerTrainLossHistory, event.globalEpoch, event.trainLoss);
            if (event.validationLoss >= 0.0)
                upsertTrainerHistoryPoint(trainerValidationLossHistory, event.globalEpoch, event.validationLoss);
            while (trainerTrainLossHistory.size() > 100)
                trainerTrainLossHistory.removeFirst();
            while (trainerValidationLossHistory.size() > 100)
                trainerValidationLossHistory.removeFirst();
            if (auto* values = this->findChild<QLabel*>("TrainerLossCurveValues"))
                renderTrainerCurves(values, trainerTrainLossHistory, trainerValidationLossHistory,
                                    QColor(42, 124, 201), QColor(222, 118, 42));
        }
        if (event.accuracy >= 0.0 || event.macroF1 >= 0.0) {
            if (event.accuracy >= 0.0)
                upsertTrainerHistoryPoint(trainerAccuracyHistory, event.globalEpoch,
                                          event.accuracy <= 1.0 ? event.accuracy : event.accuracy / 100.0);
            if (event.macroF1 >= 0.0)
                upsertTrainerHistoryPoint(trainerF1History, event.globalEpoch, event.macroF1);
            while (trainerAccuracyHistory.size() > 100)
                trainerAccuracyHistory.removeFirst();
            while (trainerF1History.size() > 100)
                trainerF1History.removeFirst();
            if (auto* values = this->findChild<QLabel*>("TrainerPerformanceCurveValues"))
                renderTrainerCurves(values, trainerAccuracyHistory, trainerF1History,
                                    QColor(38, 151, 96), QColor(112, 83, 196));
            if (event.accuracy > trainerBestAccuracy) {
                trainerBestAccuracy = event.accuracy;
                trainerBestEpoch = event.globalEpoch;
            }
            trainerResultsPanel->show();
            trainerResultMetricValues.value(0)->setText(percentageText(event.accuracy));
            trainerResultMetricValues.value(1)->setText(event.macroF1 >= 0.0 ? QString::number(event.macroF1, 'f', 3) : "--");
            trainerResultMetricValues.value(2)->setText(trainerBestEpoch > 0 ? QString::number(trainerBestEpoch) : "--");
            trainerResultMetricValues.value(3)->setText(trainerElapsed.isValid() ? QTime(0, 0).addMSecs(trainerElapsed.elapsed()).toString("hh:mm:ss") : "--");
        }
        const QString trainerCapturePath = qEnvironmentVariable("OVDS_CAPTURE_TRAINER_UI_PATH").trimmed();
        const bool hasPlotMetric = event.trainLoss >= 0.0 || event.validationLoss >= 0.0 ||
                                   event.accuracy >= 0.0 || event.macroF1 >= 0.0;
        if (hasPlotMetric && event.epoch > 0 && !trainerCapturePath.isEmpty()) {
            trainerNavButton->click();
            QTimer::singleShot(250, this, [this, trainerCapturePath]() {
                QDir().mkpath(QFileInfo(trainerCapturePath).absolutePath());
                this->grab().save(trainerCapturePath);
            });
        }
    };
    auto handOffPreparedDatasetForReview = [trainerDatasetEdit, saveTrainerSettings,
                                            openDatasetLabelerPath](const QString& datasetPath) {
        const QString normalizedPath = datasetPath.trimmed();
        if (normalizedPath.isEmpty())
            return;
        trainerDatasetEdit->setText(QDir::toNativeSeparators(normalizedPath));
        saveTrainerSettings();
        openDatasetLabelerPath(normalizedPath);
    };
    auto refreshTrainerUi = [datasetController]() { datasetController->refreshTrainerUi(); };
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_ADD_PRETRAINED_TRAINER_VISIBILITY") != 0) {
        QTimer::singleShot(0, this, [=]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition)
                    failures.push_back(message);
            };

            auto* addPretrainedButton =
                modelWorkspacePage ? modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceAddPretrainedModelButton")
                                   : nullptr;
            require(addPretrainedButton != nullptr, "Model workspace Add pre-trained model button exists");
            if (addPretrainedButton) {
                addPretrainedButton->click();
                QCoreApplication::processEvents();
            }
            refreshTrainerUi();

            const QString expectedLabel =
                qEnvironmentVariable("OVDS_VERIFY_EXPECT_ADDED_PRETRAINED_LABEL").trimmed().isEmpty()
                    ? QStringLiteral("Pre-trained model")
                    : qEnvironmentVariable("OVDS_VERIFY_EXPECT_ADDED_PRETRAINED_LABEL").trimmed();
            bool foundExpected = false;
            if (trainerStartingModelCombo) {
                for (int i = 0; i < trainerStartingModelCombo->count(); ++i) {
                    if (trainerStartingModelCombo->itemText(i).contains(expectedLabel, Qt::CaseInsensitive) ||
                        trainerStartingModelCombo->itemData(i).toString().contains("pre_binary_promotion_backup",
                                                                                    Qt::CaseInsensitive)) {
                        foundExpected = true;
                        break;
                    }
                }
            }
            require(foundExpected, "Trainer Start from list contains added pre-trained model");

            if (failures.isEmpty()) {
                qInfo().noquote() << "Add pre-trained model Trainer visibility verifier passed.";
                std::exit(0);
            }
            qWarning().noquote()
                << "Add pre-trained model Trainer visibility verifier failed:" << failures.join("; ");
            std::exit(2);
        });
    }
    auto setTrainerBusy = [datasetController, &trainerCommandWasTraining](bool busy) {
        datasetController->setTrainerBusy(busy, trainerCommandWasTraining);
    };
    auto setTrainerSummary = [datasetController, trainerStatusLabel](const QString& headline,
                                                                     const QString& detail = QString()) {
        trainerStatusLabel->setText(datasetController->trainerSummaryText(headline, detail));
    };
    auto trainerTrainArgs = [datasetController, withComputeDeviceArg](bool dryRun) {
        return withComputeDeviceArg(datasetController->trainerTrainArgs(dryRun));
    };
    auto refreshModelWorkspaceAndTrainer = [&](const QString& entryId) {
        if (auto* reloadButton = modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceInternalReloadButton")) {
            reloadButton->click();
            QCoreApplication::processEvents();
        }
        if (auto* modelTable = modelWorkspacePage->findChild<QTableWidget*>("ModelWorkspaceRegistryTable")) {
            for (int row = 0; row < modelTable->rowCount(); ++row) {
                auto* item = modelTable->item(row, 0);
                if (item && item->text().contains(entryId, Qt::CaseInsensitive)) {
                    modelTable->selectRow(row);
                    break;
                }
            }
        }
        refreshTrainerUi();
    };
    std::function<void(const QString&)> refreshLiveModelsFromRegistry;
    auto saveCompletedTrainingArtifacts = [&](const TrainerCompletionArtifacts& artifacts) {
        const QString baseName = trainerStartingModelCombo->currentText().trimmed().isEmpty()
                                     ? QString("Trained model copy")
                                     : trainerStartingModelCombo->currentText().trimmed() + " copy";
        QString defaultName = baseName;
        int suffix = 2;
        while (QFileInfo::exists(QDir(modelsRootForSaveModelUi()).filePath(trainedModelFolderNameForUi(defaultName))))
            defaultName = QString("%1 %2").arg(baseName).arg(suffix++);
        const QString modelName = promptForTrainedModelName(this, defaultName);
        if (modelName.isEmpty()) {
            appendTrainerLog("Model save canceled: no model name was saved.\n");
            for (const QString& artifact : {artifacts.modelOnnxPath, artifacts.metadataJsonPath,
                                            artifacts.metricsCsvPath, artifacts.metricsJsonPath,
                                            artifacts.classMetricsCsvPath, artifacts.confusionMatrixCsvPath}) {
                if (!artifact.trimmed().isEmpty())
                    QFile::remove(artifact);
            }
            setTrainerSummary("Completed training result discarded.", "The diagnostic log was kept.");
            return;
        }

        QString entryId;
        QString saveError;
        if (!saveTrainedModelArtifacts(registryFilePath, artifacts.runDir, artifacts.modelOnnxPath,
                                       artifacts.metadataJsonPath, artifacts.metricsCsvPath,
                                       artifacts.trainingConfigJsonPath, artifacts.metricsJsonPath,
                                       artifacts.classMetricsCsvPath, artifacts.confusionMatrixCsvPath, modelName,
                                       &entryId, &saveError)) {
            appendTrainerLog("Model save failed: " + saveError + "\n");
            setTrainerSummary("Model training completed, but saving failed.", saveError);
            return;
        }

        appendTrainerLog(QString("Saved trained model to the Model workspace: %1\n").arg(entryId));
        refreshModelWorkspaceAndTrainer(entryId);
        savedTrainerModelEntryId = entryId;
        trainerSavedModelLabel->setText("Saved model: " + modelName);
        trainerUseModelButton->setEnabled(true);
        trainerResultsPanel->show();
        setTrainerSummary("Model trained and saved.", "Review the results or use the model for sorting.");
        if (qEnvironmentVariableIntValue("OVDS_VERIFY_AUTO_USE_SAVED_MODEL") != 0)
            QTimer::singleShot(0, trainerUseModelButton, &QPushButton::click);
    };
    QObject::connect(trainerUseModelButton, &QPushButton::clicked, this, [&, trainerUseModelButton]() {
        if (savedTrainerModelEntryId.isEmpty())
            return;
            QString activationError;
            if (!activateModelRegistryEntry(registryFilePath, savedTrainerModelEntryId, &activationError)) {
                appendTrainerLog("Model active selection failed: " + activationError + "\n");
                QMessageBox::warning(this, "Use trained model", activationError);
                setTrainerSummary("The saved model could not be activated.", activationError);
            } else {
                appendTrainerLog(QString("Using trained model for sorting now: %1\n").arg(savedTrainerModelEntryId));
                refreshModelWorkspaceAndTrainer(savedTrainerModelEntryId);
                if (refreshLiveModelsFromRegistry)
                    refreshLiveModelsFromRegistry(savedTrainerModelEntryId);
                trainerUseModelButton->setEnabled(false);
                trainerUseModelButton->setText("Active");
                setTrainerSummary("Model is active.");
            }
    });
    auto startTrainerCommand = [&](const QString& label, const QStringList& args, bool isTraining, bool isDryRun) {
        refreshTrainerUi();
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            setTrainerSummary("A setup or training task is already running.");
            return;
        }
        const QString trainerPythonPath = trainerModulePath();
        if (trainerPythonPath.isEmpty()) {
            setTrainerSummary("Could not find the training helper files.",
                              "Expected a package-relative training/python folder for droplet_trainer.");
            return;
        }
        QString python = trainerPythonEdit->text().trimmed();
        if (python.isEmpty()) {
            setTrainerSummary("Choose Python setup before continuing.");
            return;
        }
        if (!QFileInfo(python).isFile()) {
            const QString recovered = resolvedTrainerPythonExecutable(python, selectedComputeDevice());
            python = recovered;
            trainerPythonEdit->setText(QDir::toNativeSeparators(recovered));
            QSettings settings;
            if (QFileInfo(recovered).isFile()) {
                settings.setValue("settings/pythonTrainer", recovered);
                settings.sync();
            }
        }
        if (!QFileInfo(python).isFile()) {
            setTrainerSummary("Python executable is missing.",
                              "Setup stage: executable check. Path: " + QDir::toNativeSeparators(python));
            pythonStatusItem->setText("Python: setup required");
            return;
        }
        if ((isTraining || isDryRun) &&
            (trainerDatasetEdit->text().trimmed().isEmpty() || trainerOutputEdit->text().trimmed().isEmpty())) {
            setTrainerSummary("Choose a training dataset file and a save location before continuing.");
            return;
        }
        if ((isTraining || isDryRun) && args.contains(QString())) {
            if (trainerStatusLabel->text().trimmed().isEmpty()) {
                setTrainerSummary("Could not prepare the training setup.");
            }
            return;
        }
        saveTrainerSettings();
        trainerCommandWasTraining = isTraining;
        trainerCommandWasDryRun = isDryRun;
        trainerStopRequested = false;
        trainerStdoutLog.clear();
        trainerStdoutLineBuffer.clear();
        trainerTrainLossHistory.clear();
        trainerValidationLossHistory.clear();
        trainerAccuracyHistory.clear();
        trainerF1History.clear();
        renderTrainerCurves(this->findChild<QLabel*>("TrainerLossCurveValues"), {}, {},
                            QColor(42, 124, 201), QColor(222, 118, 42));
        renderTrainerCurves(this->findChild<QLabel*>("TrainerPerformanceCurveValues"), {}, {},
                            QColor(38, 151, 96), QColor(112, 83, 196));
        trainerBestEpoch = 0;
        trainerBestAccuracy = -1.0;
        trainerElapsed.restart();
        trainerResultText->clear();
        appendTrainerLog(QString("Running %1\n%2\n\n").arg(label, trainerCommandPreview(python, args)));
        if (isTraining) {
            setTrainerSummary("Training model...");
        } else if (isDryRun) {
            setTrainerSummary("Checking setup...");
        } else {
            setTrainerSummary("Checking Python setup...");
        }
        pythonStatusItem->setText("Python: checking");
        refreshTrainerSetupDetails();
        if (!isTraining && !isDryRun) {
            trainerEnvCheckStatusValue->setText("Env check running with device: " + selectedComputeDevice());
            trainerPackagesStatusValue->setText("Checking training and ONNX packages...");
        }
        setTrainerBusy(true);

        auto* process = new QProcess(this);
        trainerProcess = process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (QFileInfo(trainerPythonPath).isDir()) {
            QString existing = env.value("PYTHONPATH");
            env.insert("PYTHONPATH",
                       existing.isEmpty() ? trainerPythonPath : trainerPythonPath + QDir::listSeparator() + existing);
        }
        process->setProcessEnvironment(env);
        if (!projectRoot.isEmpty()) {
            process->setWorkingDirectory(projectRoot);
        } else {
            process->setWorkingDirectory(trainerPythonPath);
        }
        QObject::connect(process, &QProcess::readyReadStandardOutput, this, [&, process]() {
            if (trainerProcess != process)
                return;
            const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
            trainerStdoutLog += chunk;
            appendTrainerLog(chunk);
            trainerStdoutLineBuffer += chunk;
            qsizetype newline = -1;
            while ((newline = trainerStdoutLineBuffer.indexOf('\n')) >= 0) {
                const QString line = trainerStdoutLineBuffer.left(newline).trimmed();
                trainerStdoutLineBuffer.remove(0, newline + 1);
                TrainerUiEvent event;
                if (parseTrainerUiEvent(line, &event))
                    presentTrainerEvent(event);
            }
        });
        QObject::connect(process, &QProcess::readyReadStandardError, this, [&, process]() {
            if (trainerProcess != process)
                return;
            appendTrainerLog(QString::fromLocal8Bit(process->readAllStandardError()));
        });
        QObject::connect(
            process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [&, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (trainerProcess != process)
                    return;
                TrainerCompletionArtifacts artifactsForSave;
                bool shouldSaveTrainedModel = false;
                const bool crashed = exitStatus == QProcess::CrashExit;
                appendTrainerLog(
                    QString("\nProcess finished: exit=%1%2\n").arg(exitCode).arg(crashed ? " crashed" : ""));
                const bool ok = exitCode == 0 && !crashed;
                if (trainerCommandWasDryRun) {
                    setTrainerSummary(ok ? "Setup check completed." : "Setup check failed.",
                                      ok ? QString() : "Review the detailed log for the failure.");
                } else if (trainerCommandWasTraining) {
                    setTrainerSummary(trainerStopRequested ? "Training stopped."
                                                           : (ok ? "Model training completed." : "Model training failed."),
                                      trainerStopRequested ? "The incomplete result was discarded. The diagnostic log was kept."
                                                           : (ok ? QString() : "Review the detailed log for the failure."));
                    if (ok && !trainerStopRequested) {
                        const TrainerCompletionArtifacts artifacts = parseSuccessfulTrainingArtifactsJsonl(trainerStdoutLog);
                        if (artifacts.complete) {
                            appendTrainerLog("Training artifacts found. Choose where to save the trained model.\n");
                            artifactsForSave = artifacts;
                            shouldSaveTrainedModel = true;
                            setTrainerSummary("Model training completed.", "Choose a name to save the trained model.");
                        } else {
                            appendTrainerLog("Model save skipped: successful model artifacts were not found in "
                                             "trainer output.\n");
                            setTrainerSummary("Model training completed, but no model artifacts were saved.",
                                              "The trainer output did not include run_finished status ok with model_onnx "
                                              "and metadata_json artifacts.");
                        }
                    }
                } else {
                    const bool requestedCuda = selectedComputeDevice() == QLatin1String("cuda");
                    const bool cudaUnavailable = trainerStdoutLog.contains(QStringLiteral("cuda_available\": false"),
                                                                            Qt::CaseInsensitive) ||
                                                 trainerStdoutLog.contains(QStringLiteral("CUDA: unavailable"),
                                                                            Qt::CaseInsensitive);
                    setTrainerSummary(ok ? "Python setup ready. You can train the model."
                                         : (requestedCuda && cudaUnavailable
                                                ? "Python setup is incompatible with the requested GPU device."
                                                : "Python environment check failed."),
                                      ok ? QString()
                                         : QString("Setup stage: environment/device check. Path: %1")
                                               .arg(QDir::toNativeSeparators(trainerPythonEdit->text().trimmed())));
                    updateTrainerSetupDetailsFromEnvCheck(trainerStdoutLog, exitCode, crashed);
                }
                pythonStatusItem->setText(ok ? "Python: ready" : "Python: issue");
                setTrainerBusy(false);
                trainerProcess = nullptr;
                process->deleteLater();
                if (shouldSaveTrainedModel) {
                    QTimer::singleShot(0, this,
                                       [&, artifactsForSave]() { saveCompletedTrainingArtifacts(artifactsForSave); });
                }
            });
        QObject::connect(process, &QProcess::errorOccurred, this, [&, process](QProcess::ProcessError error) {
            if (trainerProcess != process)
                return;
            Q_UNUSED(error);
            setTrainerSummary("Could not start the training helper.", process->errorString());
            appendTrainerLog("Process error: " + process->errorString() + "\n");
            pythonStatusItem->setText("Python: start failed");
            if (!trainerCommandWasTraining && !trainerCommandWasDryRun) {
                refreshTrainerSetupDetails();
                trainerEnvCheckStatusValue->setText("Env check could not start: " + process->errorString());
                trainerPackagesStatusValue->setText("Not checked because the Python executable did not start.");
            }
            setTrainerBusy(false);
            trainerProcess = nullptr;
            process->deleteLater();
        });
        process->start(python, args);
    };
    QObject::connect(trainerEnvCheckBtn, &QPushButton::clicked, [&]() {
        startTrainerCommand("Python setup check",
                            withComputeDeviceArg({"-m", "droplet_trainer", "env-check", "--require-training",
                                                  "--require-onnx", "--json"}),
                            false, false);
    });
    QObject::connect(trainerDryRunBtn, &QPushButton::clicked,
                     [&]() { startTrainerCommand("Setup check", trainerTrainArgs(true), false, true); });
    QObject::connect(trainerStartTrainingBtn, &QPushButton::clicked,
                     [&]() {
                         QStringList args = trainerTrainArgs(false);
                         if (qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_SMOKE") != 0)
                             args << QStringLiteral("--smoke");
                         startTrainerCommand("Train model", args, true, false);
                     });
    QObject::connect(trainerCancelBtn, &QPushButton::clicked, [&]() {
        if (!trainerProcess || trainerProcess->state() == QProcess::NotRunning)
            return;
        trainerStopRequested = true;
        setTrainerSummary("Stopping the current setup or training task...");
        trainerProcess->terminate();
        QPointer<QProcess> processPtr(trainerProcess);
        QTimer::singleShot(3000, this, [processPtr]() {
            if (!processPtr.isNull() && processPtr->state() != QProcess::NotRunning) {
                processPtr->kill();
            }
        });
    });
    QObject::connect(trainerNavButton, &QPushButton::clicked, [&]() { refreshTrainerUi(); });
    verifierTrace(QStringLiteral("startup: trainer controls wired"));
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_RESULT_MODEL_REGISTRATION") != 0) {
        QTimer::singleShot(0, this, [&]() {
            runTrainerResultModelRegistrationVerifier(modelWorkspacePage, datasetController, trainerStartingModelCombo,
                                                      liveModelCombo, refreshLiveModelsFromRegistry, &appState,
                                                      registryFilePath);
        });
    }
    QObject::connect(datasetReadinessAction, &QAction::triggered, [&]() {
        refreshTrainerUi();
        trainerNavButton->click();
        trainerDryRunBtn->setFocus();
    });
    QObject::connect(trainerConfigurePathBtn, &QPushButton::clicked, [&]() {
        workspaceStack->setCurrentWidget(settingsWorkspacePage);
        headerTitleLabel->setText("/ Settings");
        headerStatusText->setText("Settings workspace");
        settingsNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    QObject::connect(trainingEnvironmentSettingsAction, &QAction::triggered, [&]() {
        refreshTrainerUi();
        trainerNavButton->click();
        trainerPythonEdit->setFocus();
    });
    if (qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_MODEL_SELECTION") != 0) {
        QTimer::singleShot(0, this, [=]() {
            refreshTrainerUi();
            workspaceStack->setCurrentWidget(trainerWorkspacePage);

            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                }
            };

            require(trainerStartingModelCombo->count() > 0, "Trainer starting-model list is present");
            require(!trainerStartingModelCombo->currentData().toString().trimmed().isEmpty(),
                    "Trainer starting-model default selection is non-empty");
            require(trainerTrainingModeCombo->currentData().toString() == "new_copy",
                    "Trainer training-plan default is the safe new-copy mode");
            require(trainerStartingModelHintLabel->isHidden(),
                    "Legacy training-copy explanation is removed from the main Train surface");
            require(!trainerStatusLabel->text().contains("Start from:", Qt::CaseInsensitive) &&
                        !trainerStatusLabel->text().contains("Training plan:", Qt::CaseInsensitive),
                    "Trainer summary contains controlled status only");
            require(trainerOutputEdit->placeholderText().contains("new trained model", Qt::CaseInsensitive),
                    "Trainer output text describes saving a new trained model");
            const QString expectedModel = qEnvironmentVariable("OVDS_VERIFY_TRAINER_EXPECT_MODEL").trimmed();
            if (!expectedModel.isEmpty()) {
                bool foundExpectedModel = false;
                for (int i = 0; i < trainerStartingModelCombo->count(); ++i) {
                    if (trainerStartingModelCombo->itemText(i).contains(expectedModel, Qt::CaseInsensitive) ||
                        trainerStartingModelCombo->itemData(i).toString().contains(expectedModel, Qt::CaseInsensitive)) {
                        foundExpectedModel = true;
                        break;
                    }
                }
                require(foundExpectedModel, "Trainer starting-model list contains requested model: " + expectedModel);

                bool foundExpectedModelWorkspaceRow = false;
                if (auto* modelTable = modelWorkspacePage->findChild<QTableWidget*>("ModelWorkspaceRegistryTable")) {
                    for (int row = 0; row < modelTable->rowCount(); ++row) {
                        for (int column = 0; column < modelTable->columnCount(); ++column) {
                            auto* item = modelTable->item(row, column);
                            if (item && item->text().contains(expectedModel, Qt::CaseInsensitive)) {
                                foundExpectedModelWorkspaceRow = true;
                                break;
                            }
                        }
                        if (foundExpectedModelWorkspaceRow)
                            break;
                    }
                }
                require(foundExpectedModelWorkspaceRow, "Model workspace table contains requested model: " + expectedModel);
            }

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (failures.isEmpty()) {
                qInfo().noquote() << "Trainer model-selection verifier passed.";
            } else {
                qWarning().noquote() << "Trainer model-selection verifier failed:" << failures.join("; ");
            }
            std::exit(exitCode);
        });
    }
    auto runTrainerSetupStatusVerifier = [&]() -> int {
        QSettings verifierSettings;
        const QString previousPython = verifierSettings.value("settings/pythonTrainer").toString();
        const QString previousComputeDevice = verifierSettings.value("settings/computeDevice", "auto").toString();
        const QString previousValidatorDevice = verifierSettings.value("validator/device", previousComputeDevice).toString();

        if (computeDeviceCombo) {
            const int autoIndex = computeDeviceCombo->findData(QStringLiteral("auto"));
            if (autoIndex >= 0)
                computeDeviceCombo->setCurrentIndex(autoIndex);
        }
        const QString stalePython = QStringLiteral("C:/legacy/python.exe");
        verifierSettings.setValue("settings/pythonTrainer", stalePython);
        const QString expectedPython = resolvedTrainerPythonExecutable(stalePython, QStringLiteral("auto"));
        verifierSettings.setValue("settings/pythonTrainer", expectedPython);
        verifierSettings.sync();
        trainerPythonEdit->setText(QDir::toNativeSeparators(expectedPython));
        refreshTrainerUi();
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        refreshTrainerSetupDetails();
        app.processEvents();

        QStringList failures;
        auto require = [&](bool condition, const QString& message) {
            if (!condition)
                failures.push_back(message);
        };

        auto* settingsTrainerPythonEdit = settingsWorkspacePage->findChild<QLineEdit*>("SettingsWorkspacePythonTrainerEdit");
        require(!trainerEnvironmentPanel->isVisible(), "Trainer setup details panel is absent from Train");
        require(QFileInfo(expectedPython).isFile() &&
                    (sameCleanPath(expectedPython, documentedTrainerPythonExecutable(QStringLiteral("training-venv"))) ||
                     sameCleanPath(expectedPython, legacyTrainerPythonExecutable(QStringLiteral("training-venv")))),
                "Default Trainer Python resolves to a valid OpenDSS or compatible legacy CPU venv");
        require(!sameCleanPath(expectedPython, stalePython),
                "A missing persisted Trainer Python path is rejected before process launch");
        require(sameCleanPath(verifierSettings.value("settings/pythonTrainer").toString(), expectedPython),
                "Recovered Trainer Python path is persisted for the next launch");
        require(sameCleanPath(trainerPythonEdit->text().trimmed(), expectedPython),
                "Trainer Python edit uses the resolved documented install path");
        require(settingsTrainerPythonEdit != nullptr, "Settings Trainer Python field exists");
        require(settingsTrainerPythonEdit && sameCleanPath(settingsTrainerPythonEdit->text().trimmed(), expectedPython),
                "Settings Trainer Python field mirrors the resolved path");
        require(trainerPythonStatusValue->text().contains(QFileInfo(expectedPython).fileName(), Qt::CaseInsensitive),
                "Setup details report the resolved Python executable");
        require(!trainerHelperStatusValue->text().contains("Checked automatically", Qt::CaseInsensitive),
                "Training helper status is not stale placeholder text");
        require(!trainerDeviceStatusValue->text().contains("Checked automatically", Qt::CaseInsensitive),
                "Compute device status is not stale placeholder text");
        require(trainerPackagesStatusValue->text().contains("Check Python setup", Qt::CaseInsensitive),
                "Package status tells the user how to run env-check before a check has run");
        require(trainerEnvCheckStatusValue->text().contains("not run", Qt::CaseInsensitive),
                "Env-check status clearly reports that no check has run this session");

        const int exitCode = failures.isEmpty() ? 0 : 2;
        if (failures.isEmpty()) {
            qInfo().noquote() << "Trainer setup-status verifier passed.";
        } else {
            qWarning().noquote() << "Trainer setup-status verifier failed:" << failures.join("; ");
        }
        verifierSettings.setValue("settings/pythonTrainer", previousPython);
        verifierSettings.setValue("settings/computeDevice", previousComputeDevice);
        verifierSettings.setValue("validator/device", previousValidatorDevice);
        verifierSettings.sync();
        return exitCode;
    };
    if (verifyTrainerLaunch) {
        QTimer::singleShot(1000, this, [&, datasetController]() {
            verifierTrace(QStringLiteral("trainer-launch: entered"));
            const QString verifierPrefix = QStringLiteral("TRAINER LAUNCH VERIFY");
            const QString datasetPath = qEnvironmentVariable("OVDS_VERIFY_TRAINER_DATASET").trimmed();
            const QString outputPath = qEnvironmentVariable("OVDS_VERIFY_TRAINER_OUTPUT").trimmed();
            const QString pythonPath = qEnvironmentVariable("OVDS_VERIFY_TRAINER_PYTHON").trimmed();
            const QString requestedModel = qEnvironmentVariable("OVDS_VERIFY_TRAINER_MODEL").trimmed();
            const QString requestedMode = qEnvironmentVariable("OVDS_VERIFY_TRAINER_MODE").trimmed();
            const bool requireSuccessfulLifecycle =
                qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_REQUIRE_SUCCESS") != 0;
            const QString verifierHyperparameters =
                qEnvironmentVariable("OVDS_VERIFY_TRAINER_HYPERPARAMETERS_JSON").trimmed();
            const QByteArray timeoutEnv = qgetenv("OVDS_VERIFY_TRAINER_TIMEOUT_MS");
            const int timeoutMs = timeoutEnv.trimmed().isEmpty() ? 30000 : qMax(1000, timeoutEnv.toInt());

            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    verifierTrace(QStringLiteral("trainer-launch: FAIL: ") + message);
                    qCritical().noquote() << verifierPrefix << "FAIL:" << message;
                } else {
                    verifierTrace(QStringLiteral("trainer-launch: PASS: ") + message);
                    qInfo().noquote() << verifierPrefix << "PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto finish = [&](int exitCode) {
                qInfo().noquote() << verifierPrefix << "INFO: HelperPath=" << trainerModulePath();
                qInfo().noquote() << verifierPrefix << "INFO: Python=" << trainerPythonEdit->text().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: Dataset=" << trainerDatasetEdit->text().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: Output=" << trainerOutputEdit->text().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: Model="
                                  << trainerStartingModelCombo->currentText().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: TrainingMode="
                                  << trainerTrainingModeCombo->currentData().toString().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: Status=" << trainerStatusLabel->text().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: LogBegin";
                qInfo().noquote() << trainerResultText->toPlainText().trimmed();
                qInfo().noquote() << verifierPrefix << "INFO: LogEnd";
                QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
            };
            auto findComboTextContains = [](QComboBox* combo, const QString& text) {
                if (!combo)
                    return -1;
                const QString needle = text.trimmed();
                if (needle.isEmpty())
                    return combo->currentIndex();
                const int exact = combo->findText(needle, Qt::MatchFixedString);
                if (exact >= 0)
                    return exact;
                for (int i = 0; i < combo->count(); ++i) {
                    if (combo->itemText(i).contains(needle, Qt::CaseInsensitive))
                        return i;
                }
                return -1;
            };

            trainerPythonEdit->setText(QDir::toNativeSeparators(pythonPath));
            trainerDatasetEdit->setText(QDir::toNativeSeparators(datasetPath));
            trainerOutputEdit->setText(QDir::toNativeSeparators(outputPath));
            if (!verifierHyperparameters.isEmpty())
                trainerHyperparameterJsonEdit->setPlainText(verifierHyperparameters);
            refreshTrainerUi();
            verifierTrace(QStringLiteral("trainer-launch: initial refresh complete"));
            workspaceStack->setCurrentWidget(trainerWorkspacePage);
            headerTitleLabel->setText("/ Trainer");
            headerStatusText->setText("Trainer workspace");
            trainerNavButton->setChecked(true);
            app.processEvents();
            waitForUi(250);
            verifierTrace(QStringLiteral("trainer-launch: workspace selected"));

            require(!datasetPath.isEmpty(), "Dataset path env is provided");
            require(!outputPath.isEmpty(), "Output path env is provided");
            require(!pythonPath.isEmpty(), "Python path env is provided");
            require(QFileInfo(datasetPath).isFile(), "Dataset manifest path exists");
            require(QFileInfo(outputPath).isDir(), "Training output folder exists");
            require(QFileInfo(pythonPath).isFile(), "Python executable path exists");
            require(!trainerModulePath().isEmpty(), "Trainer helper folder resolves from the app");
            require(trainerStartingModelCombo->count() > 0, "Trainer starting-model list is populated");
            require(trainerTrainingModeCombo->count() > 0, "Trainer training-mode list is populated");

            if (!failures.isEmpty()) {
                finish(2);
                return;
            }

            trainerPythonEdit->setText(QDir::toNativeSeparators(pythonPath));
            trainerDatasetEdit->setText(QDir::toNativeSeparators(datasetPath));
            trainerOutputEdit->setText(QDir::toNativeSeparators(outputPath));

            const int modelIndex = findComboTextContains(trainerStartingModelCombo, requestedModel);
            require(modelIndex >= 0, "Requested trainer model is available");
            if (modelIndex >= 0)
                trainerStartingModelCombo->setCurrentIndex(modelIndex);

            const QString modeValue = requestedMode.isEmpty() ? QStringLiteral("new_copy") : requestedMode;
            const int modeIndex = trainerTrainingModeCombo->findData(modeValue);
            require(modeIndex >= 0, "Requested trainer mode is available");
            if (modeIndex >= 0)
                trainerTrainingModeCombo->setCurrentIndex(modeIndex);

            refreshTrainerUi();
            app.processEvents();
            waitForUi(250);
            verifierTrace(QStringLiteral("trainer-launch: requested model selected"));

            QStringList trainArgs = trainerTrainArgs(false);
            if (qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_SMOKE") != 0)
                trainArgs << QStringLiteral("--smoke");
            verifierTrace(QStringLiteral("trainer-launch: arguments generated"));
            verifierTrace(QStringLiteral("trainer-launch: arguments: %1")
                              .arg(trainArgs.join(QStringLiteral(" | "))));
            require(!trainArgs.isEmpty(), "Trainer arguments are generated");
            require(!trainArgs.contains(QString()), "Trainer arguments do not contain empty values");

            if (qEnvironmentVariableIntValue("OVDS_VERIFY_TRAINER_CONFIG_ONLY") != 0) {
                const int configIndex = trainArgs.indexOf(QStringLiteral("--config"));
                const QString configPath = configIndex >= 0 ? trainArgs.value(configIndex + 1) : QString();
                const QJsonObject generatedConfig = loadRegistryObjectForVerifier(configPath);
                require(QFileInfo(configPath).isFile(), "GUI-generated trainer config exists");
                QStringList generatedClassIds;
                for (const QJsonValue& value : generatedConfig.value("classes").toArray()) {
                    generatedClassIds << (value.isObject() ? value.toObject().value("id").toString()
                                                          : value.toString());
                }
                QStringList expectedClassIds;
                for (int i = 0; i < generatedClassIds.size(); ++i)
                    expectedClassIds << QString::number(i);
                require(generatedClassIds == expectedClassIds &&
                            (generatedClassIds.size() == 2 || generatedClassIds.size() == 3),
                        "GUI-generated config preserves ordered 2/3-class dataset ids");
                const QJsonObject initialization = generatedConfig.value("initialization").toObject();
                if (requestedModel.contains(QStringLiteral("Blank"), Qt::CaseInsensitive)) {
                    require(initialization.value("mode").toString() == QStringLiteral("imagenet") &&
                                !initialization.value("weight_id").toString().isEmpty() &&
                                QFileInfo(initialization.value("weight_path").toString()).isFile(),
                            "GUI-generated Blank config selects available ImageNet weights");
                } else {
                    require(initialization.value("mode").toString() == QStringLiteral("checkpoint") &&
                                QFileInfo(initialization.value("checkpoint_path").toString()).isFile(),
                            "GUI-generated Pre-trained config selects its packaged checkpoint");
                }
                for (const QString& ambiguousKey : {QStringLiteral("pretrained"),
                                                    QStringLiteral("source_checkpoint"),
                                                    QStringLiteral("source_checkpoint_path"),
                                                    QStringLiteral("classifier_initialization")}) {
                    require(!generatedConfig.contains(ambiguousKey),
                            "GUI-generated config omits ambiguous field " + ambiguousKey);
                }
                finish(failures.isEmpty() ? 0 : 2);
                return;
            }

            if (!failures.isEmpty()) {
                finish(2);
                return;
            }

            trainerEnvCheckBtn->click();
            QElapsedTimer setupTimer;
            setupTimer.start();
            while (trainerProcess && trainerProcess->state() != QProcess::NotRunning &&
                   setupTimer.elapsed() < timeoutMs) {
                waitForUi(100);
            }
            const QString setupStatus = trainerStatusLabel->text();
            require(!trainerProcess && setupStatus.contains(QStringLiteral("ready"), Qt::CaseInsensitive),
                    "Successful Setup completes and enables the existing training action");
            require(trainerStartTrainingBtn->isEnabled(),
                    "Train model is enabled after successful Setup");
            if (!failures.isEmpty()) {
                finish(2);
                return;
            }

            qInfo().noquote() << verifierPrefix << "INFO: CommandPreview="
                              << datasetController->trainerCommandPreview(trainerPythonEdit->text().trimmed(), trainArgs);

            trainerStartTrainingBtn->click();
            app.processEvents();
            QElapsedTimer launchTimer;
            launchTimer.start();
            QString launchLog;
            while (launchTimer.elapsed() < 5000) {
                waitForUi(100);
                launchLog = trainerResultText->toPlainText();
                if (launchLog.contains(QStringLiteral("-m droplet_trainer train"), Qt::CaseInsensitive) ||
                    launchLog.contains(QStringLiteral("\"event\": \"run_started\""), Qt::CaseInsensitive) ||
                    (trainerProcess && trainerProcess->state() != QProcess::Starting)) {
                    break;
                }
            }

            require(trainerProcess != nullptr || launchLog.contains(QStringLiteral("Process finished:"), Qt::CaseInsensitive) ||
                        launchLog.contains(QStringLiteral("\"event\": \"run_started\""), Qt::CaseInsensitive),
                    "Trainer launch progressed past the Train click");
            require(launchLog.contains(QStringLiteral("-m droplet_trainer train"), Qt::CaseInsensitive) ||
                        launchLog.contains(QStringLiteral("\"event\": \"run_started\""), Qt::CaseInsensitive),
                    "Trainer log shows python -m droplet_trainer train");

            if (!failures.isEmpty()) {
                finish(2);
                return;
            }

            QElapsedTimer timer;
            timer.start();
            while (trainerProcess && trainerProcess->state() != QProcess::NotRunning && timer.elapsed() < timeoutMs) {
                waitForUi(200);
            }

            if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
                qInfo().noquote() << verifierPrefix << "INFO: Process still running after" << timeoutMs
                                  << "ms; stopping verifier wait window.";
                trainerProcess->terminate();
                waitForUi(3000);
                if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
                    trainerProcess->kill();
                    waitForUi(1000);
                }
                finish(0);
                return;
            }

            const QString finalLog = trainerResultText->toPlainText();
            const bool launchedTrain =
                finalLog.contains(QStringLiteral("-m droplet_trainer train"), Qt::CaseInsensitive);
            require(launchedTrain, "Trainer launch log still shows the train command after process exit");
            if (requireSuccessfulLifecycle) {
                waitForUi(1500);
                const QString completedLog = trainerResultText->toPlainText();
                require(completedLog.contains(QStringLiteral("\"event\": \"checkpoint_loaded\"")),
                        "Pre-trained GUI run emitted checkpoint_loaded");
                require(completedLog.contains(QStringLiteral("\"event\": \"run_finished\"")) &&
                            completedLog.contains(QStringLiteral("\"status\": \"ok\"")),
                        "GUI run emitted strict run_finished status ok");
                const TrainerCompletionArtifacts completed = parseSuccessfulTrainingArtifactsJsonl(completedLog);
                require(completed.complete && QFileInfo(completed.modelOnnxPath).isFile() &&
                            QFileInfo(completed.metadataJsonPath).isFile() &&
                            QFileInfo(QDir(completed.runDir).filePath("checkpoint.pth")).isFile(),
                        "Successful GUI run produced checkpoint.pth, model.onnx, and metadata.json");
                require(!savedTrainerModelEntryId.isEmpty(), "GUI Save/Use registered the real completed package");
                if (!savedTrainerModelEntryId.isEmpty()) {
                    const QJsonArray persistedEntries = readModelRegistryEntriesFromPath(registryFilePath, nullptr);
                    const QJsonObject savedEntry = registryEntryByIdForVerifier(persistedEntries, savedTrainerModelEntryId);
                    require(!savedEntry.isEmpty() && savedEntry.value("active").toBool(false),
                            "GUI Save/Use activated the real completed package");
                }
            }
            finish(failures.isEmpty() ? 0 : 2);
        });
    }
    QObject::connect(trainingValidateEnvironmentAction, &QAction::triggered, [&]() {
        trainerNavButton->click();
        trainerEnvCheckBtn->click();
    });

    const QString documentsOnnxFallback = QDir(defaultWorkspacePaths.models).filePath("pre_binary_promotion_backup.onnx");
    const QString documentsMetaFallback =
        QDir(defaultWorkspacePaths.models).filePath("pre_binary_promotion_backup_metadata.json");
    QString onnxPicked =
        defaultWorkspacePaths.activeModel.isEmpty() ? documentsOnnxFallback : defaultWorkspacePaths.activeModel;
    QString metaPicked =
        defaultWorkspacePaths.activeMetadata.isEmpty() ? documentsMetaFallback : defaultWorkspacePaths.activeMetadata;
    onnxEdit->setText(onnxPicked);
    metaEdit->setText(metaPicked);
    if (outputEdit->text().isEmpty()) {
        outputEdit->setText(defaultWorkspacePaths.runs);
    }

    constexpr int kLiveModelIdRole = Qt::UserRole + 1;
    constexpr int kLiveModelOnnxRole = Qt::UserRole + 2;
    constexpr int kLiveModelMetadataRole = Qt::UserRole + 3;
    constexpr int kLiveModelStateRole = Qt::UserRole + 4;
    constexpr int kLiveModelModeRole = Qt::UserRole + 5;
    constexpr int kLiveModelTargetRole = Qt::UserRole + 6;
    constexpr int kLiveModelSummaryRole = Qt::UserRole + 7;
    constexpr int kLiveModelOnnxHashRole = Qt::UserRole + 8;
    constexpr int kLiveModelMetadataHashRole = Qt::UserRole + 9;

    auto addLiveModelRow = [&](const QString& label, const QString& id, const QString& onnxPath, 
                               const QString& metadataPath, const QString& state, const QString& mode, 
                               const QString& targetClassId, const QString& summary, const QString& onnxSha256, 
                               const QString& metadataSha256, bool selectable) { 
        liveModelCombo->addItem(label); 
        const int index = liveModelCombo->count() - 1; 
        if (!targetClassId.trimmed().isEmpty())
            liveModelCombo->setItemIcon(index, desktop_app::theme::semanticClassIcon(targetClassId, currentThemeMode));
        liveModelCombo->setItemData(index, id, kLiveModelIdRole); 
        liveModelCombo->setItemData(index, onnxPath, kLiveModelOnnxRole); 
        liveModelCombo->setItemData(index, metadataPath, kLiveModelMetadataRole); 
        liveModelCombo->setItemData(index, state, kLiveModelStateRole);
        liveModelCombo->setItemData(index, mode, kLiveModelModeRole);
        liveModelCombo->setItemData(index, targetClassId, kLiveModelTargetRole);
        liveModelCombo->setItemData(index, summary, kLiveModelSummaryRole);
        liveModelCombo->setItemData(index, onnxSha256, kLiveModelOnnxHashRole);
        liveModelCombo->setItemData(index, metadataSha256, kLiveModelMetadataHashRole);
        liveModelCombo->setItemData(index, summary, Qt::ToolTipRole);
        if (!selectable) {
            liveModelCombo->setItemData(index, QColor(Qt::gray), Qt::ForegroundRole);
            if (auto* itemModel = qobject_cast<QStandardItemModel*>(liveModelCombo->model())) {
                if (auto* item = itemModel->item(index)) {
                    item->setEnabled(false);
                }
            }
        }
    };

    for (const auto& value : registryEntries) {
        QJsonObject entry = value.toObject();
        const QString targetId = registryNestedString(entry, "target_policy", "target_class_id");
        const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label");
        QString label = registryString(entry, "display_name");
        if (label.isEmpty())
            label = registryString(entry, "registry_entry_id");
        const ModelPackageInspection package = inspectModelPackage(entry);
        label += " - " + package.status;
        if (!targetId.isEmpty()) {
            label += " - " + (targetDisplay.isEmpty() ? targetId : QString("%1 (%2)").arg(targetDisplay, targetId));
        }
        const bool selectable = package.canActivate;
        addLiveModelRow(label, registryString(entry, "registry_entry_id"),
                        package.onnxPath, package.metadataPath,
                        package.status, selectable ? "normal" : "blocked",
                        targetId, registryEntrySummary(entry, registryFilePath, registryLoadWarning),
                        registryString(entry, "model_sha256"), registryString(entry, "metadata_sha256"), selectable);
    }
    if (liveModelCombo->count() == 0) {
        addLiveModelRow("Temporary static fallback - promoted/current binary runtime - Hits (1)",
                        "run_20260429_221500_wsl2_binary_linuxmirror_onnx", onnxPicked, metaPicked, "promoted_current",
                        "normal", "1", "Temporary static fallback row. Registry file was empty or unavailable.",
                        "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769",
                        "528ac091764c09cd9c2c6ad2a6ff1e38bb009184a26e7352b71b3a025c30902d", true);
    }
    const QString activeRegistryId =
        registryString(activeRegistryEntry(registryEntries), "registry_entry_id").trimmed();
    int activeLiveModelIndex = -1;
    for (int i = 0; i < liveModelCombo->count(); ++i) {
        if (!activeRegistryId.isEmpty() &&
            liveModelCombo->itemData(i, kLiveModelIdRole).toString().compare(
                activeRegistryId, Qt::CaseInsensitive) == 0 &&
            liveModelCombo->itemData(i, kLiveModelModeRole).toString() != "blocked") {
            activeLiveModelIndex = i;
            break;
        }
    }
    for (int i = 0; i < liveModelCombo->count(); ++i) {
        if (activeLiveModelIndex >= 0)
            break;
        if (liveModelCombo->itemData(i, kLiveModelModeRole).toString() != "blocked") {
            activeLiveModelIndex = i;
            break;
        }
    }
    if (activeLiveModelIndex < 0)
        activeLiveModelIndex = 0;
    liveModelCombo->setCurrentIndex(activeLiveModelIndex);
    appState.activeModelId = liveModelCombo->currentData(kLiveModelIdRole).toString();
    liveModelSummaryText->setPlainText(liveModelCombo->currentData(kLiveModelSummaryRole).toString());

    QString pendingTargetClassId = appState.targetClassId;
    auto selectedTargetClassId = [&]() -> QString {
        if (!targetClassCombo)
            return QString();
        QVariant data = targetClassCombo->currentData();
        QString classId = data.isValid() ? data.toString().trimmed() : QString();
        if (!classId.isEmpty())
            return classId;
        classId = targetClassCombo->currentText().trimmed();
        return classId.isEmpty() ? appState.targetClassId : classId;
    };
    auto sortNonTargetEnabled = [&]() -> bool { return sortNonTargetCheck && sortNonTargetCheck->isChecked(); };
    auto currentTargetDisplayText = [&]() -> QString {
        const QString targetText = targetClassCombo ? targetClassCombo->currentText().trimmed() : QString();
        if (!targetText.isEmpty())
            return targetText;
        const QString classId = selectedTargetClassId().trimmed();
        return classId.isEmpty() ? QStringLiteral("target") : classId;
    };
    auto currentTriggerPolicyText = [&]() -> QString {
        const QString targetText = currentTargetDisplayText();
        return sortNonTargetEnabled() ? QString("Sort Non-target: not %1").arg(targetText)
                                      : QString("Sort Target: %1").arg(targetText);
    };
    auto displayDecisionDirection = [](const QString& direction) {
        if (direction.compare("Hit", Qt::CaseInsensitive) == 0)
            return QStringLiteral("Sort");
        if (direction.compare("Waste", Qt::CaseInsensitive) == 0)
            return QStringLiteral("Pass");
        return direction;
    };

    auto setSelectedTargetClassId = [&](const QString& classId) {
        pendingTargetClassId = classId.trimmed();
        if (pendingTargetClassId.isEmpty())
            return;
        appState.targetClassId = pendingTargetClassId;
        for (int i = 0; i < targetClassCombo->count(); ++i) {
            if (targetClassCombo->itemData(i).toString() == pendingTargetClassId) {
                targetClassCombo->setCurrentIndex(i);
                return;
            }
        }
    };

    auto saveRuntimeSettings = [&]() {
        runtimeSettings.setValue(kRuntimeSettingsSchemaVersionKey, kRuntimeSettingsSchemaVersion);
        runtimeSettings.setValue("runtime/v1/model/path", onnxEdit->text().trimmed());
        runtimeSettings.setValue("runtime/v1/model/metadataPath", metaEdit->text().trimmed());
        appState.targetClassId = selectedTargetClassId();
        appState.sortNonTarget = sortNonTargetEnabled();
        runtimeSettings.setValue("runtime/v1/model/targetClassId", appState.targetClassId);
        runtimeSettings.setValue("runtime/v1/sorting/sortNonTarget", appState.sortNonTarget);
        runtimeSettings.setValue("runtime/v1/output/baseDir", runOutputBaseForSettings(outputEdit->text()));
        runtimeSettings.setValue("runtime/v1/output/saveCrops", saveCropCheck->isChecked());
        runtimeSettings.setValue("runtime/v1/output/saveOverlays", saveOverlayCheck->isChecked());

        runtimeSettings.setValue("runtime/v1/camera/presetText", presetCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/customWidth", customWidthSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/customHeight", customHeightSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/binning", binCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/bits", bitsCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/exposureMs", exposureSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/readoutSpeed", readoutCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/displayEvery", displayEverySpin->value());
        runtimeSettings.setValue("runtime/v1/camera/lutMin", lutMinSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/lutMax", lutMaxSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/savePath", savePathEdit->text().trimmed());

        runtimeSettings.setValue("runtime/v1/detector/frameSkip", frameSkipSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/bgFrames", bgFramesSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/bgUpdateFrames", bgUpdateSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/resetFrames", resetFramesSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minArea", minAreaSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minAreaFrac", minAreaFracSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/maxAreaFrac", maxAreaFracSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minBbox", minBboxSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/margin", marginSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/diffThresh", diffThreshSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/blurRadius", blurRadiusSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/morphRadius", morphRadiusSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/scale", scaleSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/gapFireShift", gapFireSpin->value());
        runtimeSettings.sync();
    };

    auto restoreRuntimeSettings = [&]() {
        if (verifyDefaultPaths)
            return;
        if (!runtimeSettings.contains(kRuntimeSettingsSchemaVersionKey))
            return;
        const int schemaVersion = runtimeSettings.value(kRuntimeSettingsSchemaVersionKey, 0).toInt();
        if (schemaVersion < 1 || schemaVersion > kRuntimeSettingsSchemaVersion)
            return;

        auto restoredPathOrDefault = [&](const QString& key, const QString& defaultPath) {
            const QString saved = runtimeSettings.value(key, defaultPath).toString();
            const QString resolved = validatorResolveAppRelative(saved);
            return isDeveloperInternalDefaultPath(saved) || isDeveloperInternalDefaultPath(resolved) ? defaultPath : saved;
        };
        onnxEdit->setText(restoredPathOrDefault("runtime/v1/model/path", onnxEdit->text()));
        metaEdit->setText(restoredPathOrDefault("runtime/v1/model/metadataPath", metaEdit->text()));
        setSelectedTargetClassId(
            runtimeSettings.value("runtime/v1/model/targetClassId", pendingTargetClassId).toString());
        outputEdit->setText(runtimeSettings.value("runtime/v1/output/baseDir", outputEdit->text()).toString());
        sortNonTargetCheck->setChecked(
            runtimeSettings.value("runtime/v1/sorting/sortNonTarget", appState.sortNonTarget).toBool());
        saveCropCheck->setChecked(
            runtimeSettings.value("runtime/v1/output/saveCrops", saveCropCheck->isChecked()).toBool());
        saveOverlayCheck->setChecked(
            runtimeSettings.value("runtime/v1/output/saveOverlays", saveOverlayCheck->isChecked()).toBool());

        setComboTextIfPresent(presetCombo, runtimeSettings.value("runtime/v1/camera/presetText").toString());
        customWidthSpin->setValue(
            runtimeSettings.value("runtime/v1/camera/customWidth", customWidthSpin->value()).toInt());
        customHeightSpin->setValue(
            runtimeSettings.value("runtime/v1/camera/customHeight", customHeightSpin->value()).toInt());
        setComboTextIfPresent(binCombo, runtimeSettings.value("runtime/v1/camera/binning").toString());
        setComboTextIfPresent(bitsCombo, QStringLiteral("8"));
        exposureSpin->setValue(runtimeSettings.value("runtime/v1/camera/exposureMs", exposureSpin->value()).toDouble());
        setComboTextIfPresent(readoutCombo, runtimeSettings.value("runtime/v1/camera/readoutSpeed").toString());
        displayEverySpin->setValue(
            runtimeSettings.value("runtime/v1/camera/displayEvery", displayEverySpin->value()).toInt());
        lutMinSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMin", lutMinSpin->value()).toInt());
        lutMaxSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMax", lutMaxSpin->value()).toInt());
        savePathEdit->setText(runtimeSettings.value("runtime/v1/camera/savePath", savePathEdit->text()).toString());

        frameSkipSpin->setValue(runtimeSettings.value("runtime/v1/detector/frameSkip", frameSkipSpin->value()).toInt());
        bgFramesSpin->setValue(runtimeSettings.value("runtime/v1/detector/bgFrames", bgFramesSpin->value()).toInt());
        bgUpdateSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/bgUpdateFrames", bgUpdateSpin->value()).toInt());
        resetFramesSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/resetFrames", resetFramesSpin->value()).toInt());
        minAreaSpin->setValue(runtimeSettings.value("runtime/v1/detector/minArea", minAreaSpin->value()).toDouble());
        minAreaFracSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/minAreaFrac", minAreaFracSpin->value()).toDouble());
        maxAreaFracSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/maxAreaFrac", maxAreaFracSpin->value()).toDouble());
        minBboxSpin->setValue(runtimeSettings.value("runtime/v1/detector/minBbox", minBboxSpin->value()).toInt());
        marginSpin->setValue(runtimeSettings.value("runtime/v1/detector/margin", marginSpin->value()).toInt());
        diffThreshSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/diffThresh", diffThreshSpin->value()).toInt());
        blurRadiusSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/blurRadius", blurRadiusSpin->value()).toInt());
        morphRadiusSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/morphRadius", morphRadiusSpin->value()).toInt());
        scaleSpin->setValue(runtimeSettings.value("runtime/v1/detector/scale", scaleSpin->value()).toDouble());
        gapFireSpin->setValue(runtimeSettings.value("runtime/v1/detector/gapFireShift", gapFireSpin->value()).toInt());
    };

    auto runtimeSettingsSnapshot = [&](const QString& runMode) -> QJsonObject {
        QJsonObject root;
        root["schema_version"] = kRuntimeSettingsSchemaVersion;
        root["run_mode"] = runMode;
        root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QJsonObject model;
        model["registry_entry_id"] = liveModelCombo->currentData(kLiveModelIdRole).toString();
        model["model_state_at_start"] = liveModelCombo->currentData(kLiveModelStateRole).toString();
        model["live_use_mode"] = liveModelCombo->currentData(kLiveModelModeRole).toString();
        model["path"] = onnxEdit->text().trimmed();
        model["metadata_path"] = metaEdit->text().trimmed();
        model["model_sha256"] = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString();
        model["metadata_sha256"] = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString();
        model["target_class_id"] = selectedTargetClassId();
        model["target_display_label"] = targetClassCombo->currentText().trimmed();
        model["sort_non_target"] = sortNonTargetEnabled();
        model["trigger_policy"] = currentTriggerPolicyText();
        model["selection_summary"] = liveModelCombo->currentData(kLiveModelSummaryRole).toString();
        root["model"] = model;

        QJsonObject output;
        output["run_dir"] = outputEdit->text().trimmed();
        output["base_dir"] = runOutputBaseForSettings(outputEdit->text());
        output["save_crops"] = saveCropCheck->isChecked();
        output["save_overlays"] = saveOverlayCheck->isChecked();
        root["output"] = output;

        QJsonObject camera;
        camera["preset"] = comboSnapshot(presetCombo);
        camera["custom_width"] = customWidthSpin->value();
        camera["custom_height"] = customHeightSpin->value();
        camera["binning"] = binCombo->currentText();
        camera["independent_binning"] = false;
        camera["bin_h"] = std::max(1, binCombo->currentText().toInt());
        camera["bin_v"] = std::max(1, binCombo->currentText().toInt());
        camera["bits"] = bitsCombo->currentText();
        camera["exposure_ms"] = exposureSpin->value();
        camera["readout_speed"] = readoutCombo->currentText();
        camera["display_every"] = displayEverySpin->value();
        camera["lut_min"] = lutMinSpin->value();
        camera["lut_max"] = lutMaxSpin->value();
        camera["save_path"] = savePathEdit->text().trimmed();
        root["camera"] = camera;

        QJsonObject detector;
        detector["frame_skip"] = frameSkipSpin->value();
        detector["bg_frames"] = bgFramesSpin->value();
        detector["bg_update_frames"] = bgUpdateSpin->value();
        detector["reset_frames"] = resetFramesSpin->value();
        detector["min_area"] = minAreaSpin->value();
        detector["min_area_frac"] = minAreaFracSpin->value();
        detector["max_area_frac"] = maxAreaFracSpin->value();
        detector["min_bbox"] = minBboxSpin->value();
        detector["margin"] = marginSpin->value();
        detector["diff_thresh"] = diffThreshSpin->value();
        detector["blur_radius"] = blurRadiusSpin->value();
        detector["morph_radius"] = morphRadiusSpin->value();
        detector["scale"] = scaleSpin->value();
        detector["gap_fire_shift"] = gapFireSpin->value();
        root["detector"] = detector;

        return root;
    };

    auto writeRuntimeSettingsSnapshot = [&](const QString& runDir, const QString& runMode) {
        if (runDir.trimmed().isEmpty())
            return;
        QDir dir(runDir);
        dir.mkpath(".");
        QString writeError;
        if (!desktop_app::writeJsonObjectAtomically(dir.filePath("runtime_settings_snapshot.json"),
                                                    runtimeSettingsSnapshot(runMode), &writeError)) {
            logMessage(QString("Failed to write runtime settings snapshot: %1").arg(writeError));
            return;
        }
    };

    auto resolveAppRelative = [&](const QString& path) -> QString {
        if (path.isEmpty())
            return path;
        QFileInfo info(path);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        QString abs = QDir(appDir).absoluteFilePath(path);
        if (QFileInfo::exists(abs))
            return abs;
        QString fallback = findModelUpwards(QFileInfo(path).fileName());
        if (!fallback.isEmpty())
            return fallback;
        return abs;
    };

    auto populateTargetClassSelector = [&]() {
        QString requestedClassId = pendingTargetClassId;
        if (requestedClassId.isEmpty()) {
            requestedClassId = selectedTargetClassId();
        }

        Metadata metadata;
        std::string err;
        const QString metadataPath = resolveAppRelative(metaEdit->text().trimmed());
        const bool loaded = LoadMetadata(metadataPath.toStdString(), metadata, err);

        QSignalBlocker blocker(targetClassCombo);
        targetClassCombo->clear(); 
 
        if (!loaded || metadata.classes.empty()) { 
            const QString fallbackId = requestedClassId.isEmpty() ? QStringLiteral("Single") : requestedClassId; 
            targetClassCombo->addItem(desktop_app::theme::semanticClassIcon(fallbackId, currentThemeMode), fallbackId,
                                      fallbackId); 
            pendingTargetClassId = fallbackId; 
            logMessage(QString("Target selector using legacy fallback class id '%1': %2") 
                           .arg(fallbackId, QString::fromStdString(err))); 
            return; 
        } 
 
        for (const std::string& classIdStd : metadata.classes) { 
            const std::string displayLabel = DisplayLabelForClassId(metadata, classIdStd); 
            const QString classId = QString::fromStdString(classIdStd); 
            const QString displayText = QString::fromStdString(FormatClassForDisplay(classIdStd, displayLabel)); 
            targetClassCombo->addItem(desktop_app::theme::semanticClassIcon(classId, currentThemeMode), displayText,
                                      classId); 
        } 

        std::string resolvedClassId;
        std::string resolvedDisplayLabel;
        std::string resolveErr;
        if (!ResolveTargetClassId(metadata, requestedClassId.toStdString(), std::string(), resolvedClassId,
                                  resolvedDisplayLabel, resolveErr)) {
            ResolveTargetClassId(metadata, std::string(), std::string(), resolvedClassId, resolvedDisplayLabel,
                                 resolveErr);
        }

        if (!resolvedClassId.empty()) {
            pendingTargetClassId = QString::fromStdString(resolvedClassId);
            for (int i = 0; i < targetClassCombo->count(); ++i) {
                if (targetClassCombo->itemData(i).toString() == pendingTargetClassId) {
                    targetClassCombo->setCurrentIndex(i);
                    break;
                }
            }
        } else if (targetClassCombo->count() > 0) {
            targetClassCombo->setCurrentIndex(0);
            pendingTargetClassId = targetClassCombo->itemData(0).toString();
        }
    };

    auto applyLiveModelSelection = [&]() {
        const QString mode = liveModelCombo->currentData(kLiveModelModeRole).toString();
        const QString summary = liveModelCombo->currentData(kLiveModelSummaryRole).toString();
        appState.activeModelId = liveModelCombo->currentData(kLiveModelIdRole).toString();
        liveModelSummaryText->setPlainText(summary);
        if (mode == "blocked") {
            pipelineStatusLabel->setText(
                "Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            return;
        }
        const QString onnxPath = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString metadataPath = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        // Always load the selected registry artifact in place. A copied "active"
        // alias can be stale and, for ONNX external-data models, separates the
        // graph from its required .data sidecar.
        if (!onnxPath.isEmpty()) {
            onnxEdit->setText(onnxPath);
        }
        if (!metadataPath.isEmpty()) {
            metaEdit->setText(metadataPath);
        }
        const QString targetClassId = liveModelCombo->currentData(kLiveModelTargetRole).toString();
        if (!targetClassId.isEmpty()) {
            pendingTargetClassId = targetClassId;
            appState.targetClassId = targetClassId;
        }
        populateTargetClassSelector();
        saveRuntimeSettings();
        pipelineStatusLabel->setText("Live model selected: " + liveModelCombo->currentText());
    };
    refreshLiveModelsFromRegistry = [&](const QString& selectedEntryId) {
        QString warning;
        const QJsonArray refreshedEntries = readModelRegistryEntriesFromPath(registryFilePath, &warning);
        if (refreshedEntries.isEmpty()) {
            pipelineStatusLabel->setText(warning.isEmpty() ? "Live model refresh found no model entries." : warning);
            return;
        }

        QSignalBlocker blocker(liveModelCombo);
        liveModelCombo->clear();
        for (const auto& value : refreshedEntries) {
            const QJsonObject entry = value.toObject();
            const QString targetId = registryNestedString(entry, "target_policy", "target_class_id");
            const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label");
            QString label = registryString(entry, "display_name");
            if (label.isEmpty())
                label = registryString(entry, "registry_entry_id");
            const ModelPackageInspection package = inspectModelPackage(entry);
            label += " - " + package.status;
            if (!targetId.isEmpty())
                label += " - " + (targetDisplay.isEmpty() ? targetId : QString("%1 (%2)").arg(targetDisplay, targetId));

            const bool selectable = package.canActivate;
            addLiveModelRow(label, registryString(entry, "registry_entry_id"),
                            package.onnxPath, package.metadataPath,
                            package.status, selectable ? "normal" : "blocked",
                            targetId, registryEntrySummary(entry, registryFilePath, registryLoadWarning),
                            registryString(entry, "model_sha256"), registryString(entry, "metadata_sha256"), selectable);
        }

        int selectedIndex = -1;
        const QString preferredEntryId = selectedEntryId.trimmed();
        for (int i = 0; i < liveModelCombo->count(); ++i) {
            if (!preferredEntryId.isEmpty() &&
                liveModelCombo->itemData(i, Qt::UserRole + 1).toString().compare(preferredEntryId, Qt::CaseInsensitive) == 0) {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex < 0) {
            for (int i = 0; i < liveModelCombo->count(); ++i) {
                if (liveModelCombo->itemData(i, Qt::UserRole + 5).toString() != "blocked") {
                    selectedIndex = i;
                    break;
                }
            }
        }
        if (selectedIndex < 0 && liveModelCombo->count() > 0)
            selectedIndex = 0;
        if (selectedIndex >= 0)
            liveModelCombo->setCurrentIndex(selectedIndex);
        blocker.unblock();
        applyLiveModelSelection();
        syncValidatorWorkspaceRuntimeModel();
    };

    QTimer detectorTuningApplyTimer;
    detectorTuningApplyTimer.setSingleShot(true);
    detectorTuningApplyTimer.setInterval(250);

    std::shared_ptr<std::vector<SequenceFrame>> sequenceFrames;
    QMutex sequenceMutex;
    std::atomic<bool> sequenceRunning(false);
    std::atomic<bool> sequenceStarting(false);
    std::atomic<bool> sequenceStop(false);
    std::thread sequenceThread;
    BackgroundTaskRegistry backgroundTasks;
    std::atomic<bool> sequenceLoading(false);
    bool sequencePrevPipelineChecked = false;
    StatsTracker stats;
    QMutex statsMutex;
    QMutex liveLogMutex;
    std::vector<LiveLogRecord> liveLog;
    std::atomic<bool> liveLogging(false);
    QDateTime liveLogStart;
    std::function<void()> startLiveLogging;
    std::function<void()> stopLiveLogging;
    QMutex collectionMutex;
    LiveDataCollectionWriter collectionWriter;
    std::shared_ptr<LiveFrameDispatcher> recordDispatcher;
    QString collectionPreviousDaqStatusText;
    QString collectionPreviousDaqFaultText;
    bool collectionPreviousDaqAvailable = false;
    bool collectionPreviousDaqDisabled = false;
    bool collectionPreviousDaqFault = false;
    QMutex datasetCaptureMutex;
    DatasetCaptureSession datasetCaptureSession;
    std::atomic<bool> datasetCaptureActive(false);
    std::atomic<bool> datasetBatchPromptPending(false);
    QString datasetCaptureDir;
    QString datasetCaptureManifestPath;

    imageView->setZoomChanged([=](double zoom) {
        zoomStatusLabel->setText(QString("%1%").arg(static_cast<int>(std::lround(zoom * 100.0))));
        scaleStatusLabel->setText(QString("SF: %1 Px").arg(zoom, 0, 'f', 3));
    });
    cameraImageView->setZoomChanged([=](double zoom) { Q_UNUSED(zoom); });

    QThread cameraThread;
    cameraThread.setObjectName("CameraWorkerThread");
    auto* cameraWorker = new CameraWorker();
    bool cameraOpened = false;
    PipelineRunner pipeline;
    QMutex pipelineMutex;
    std::atomic<bool> pipelineEnabled(false);
    ValidatorWorkspaceController::Dependencies validatorWorkspaceControllerDeps;
    validatorWorkspaceControllerDeps.parentWindow = this;
    validatorWorkspaceControllerDeps.imageValidationAction = imageValidationAction;
    validatorWorkspaceControllerDeps.onnxEdit = onnxEdit;
    validatorWorkspaceControllerDeps.metaEdit = metaEdit;
    validatorWorkspaceControllerDeps.pythonStatusItem = pythonStatusItem;
    validatorWorkspaceControllerDeps.validatorNavButton = validatorNavButton;
    validatorWorkspaceControllerDeps.workspaceStack = workspaceStack;
    validatorWorkspaceControllerDeps.validatorWorkspace = validatorWorkspacePage;
    validatorWorkspaceControllerDeps.preparedDatasetPath = defaultWorkspacePaths.preparedDatasetManifest;
    validatorWorkspaceControllerDeps.validationRunsRoot = defaultWorkspacePaths.validationRuns;
    validatorWorkspaceControllerDeps.appDir = appDir;
    validatorWorkspaceControllerDeps.resolveAppRelative = resolveAppRelative;
    validatorWorkspaceControllerDeps.seqFolderEdit = seqFolderEdit;
    validatorWorkspaceControllerDeps.seqBrowseBtn = seqBrowseBtn;
    validatorWorkspaceControllerDeps.seqLoadBtn = seqLoadBtn;
    validatorWorkspaceControllerDeps.seqStartBtn = seqStartBtn;
    validatorWorkspaceControllerDeps.seqStopBtn = seqStopBtn;
    validatorWorkspaceControllerDeps.seqStatusLabel = seqStatusLabel;
    validatorWorkspaceControllerDeps.statusLabel = statusLabel;
    validatorWorkspaceControllerDeps.pipelineWidget = pipelineWidget;
    validatorWorkspaceControllerDeps.labviewWidget = labviewWidget;
    validatorWorkspaceControllerDeps.detectWidget = detectWidget;
    validatorWorkspaceControllerDeps.pipelineStartBtn = pipelineStartBtn;
    validatorWorkspaceControllerDeps.pipelineStopBtn = pipelineStopBtn;
    validatorWorkspaceControllerDeps.startBtn = startBtn;
    validatorWorkspaceControllerDeps.stopBtn = nullptr;
    validatorWorkspaceControllerDeps.reconnectBtn = reconnectBtn;
    validatorWorkspaceControllerDeps.applyBtn = applyBtn;
    validatorWorkspaceControllerDeps.viewerOnly = &viewerOnly;
    validatorWorkspaceControllerDeps.pipelineEnabled = &pipelineEnabled;
    validatorWorkspaceControllerDeps.sequenceFrames = &sequenceFrames;
    validatorWorkspaceControllerDeps.sequenceMutex = &sequenceMutex;
    validatorWorkspaceControllerDeps.sequenceRunning = &sequenceRunning;
    validatorWorkspaceControllerDeps.sequenceStop = &sequenceStop;
    validatorWorkspaceControllerDeps.sequenceLoading = &sequenceLoading;
    validatorWorkspaceControllerDeps.sequenceThread = &sequenceThread;
    validatorWorkspaceControllerDeps.backgroundTasks = &backgroundTasks;
    auto* validatorWorkspaceController = new ValidatorWorkspaceController(validatorWorkspaceControllerDeps, this);
    bool labviewTriggerReady = false;
    verifierTrace(QStringLiteral("startup: creating runtime controllers"));
    SettingsWorkspaceController::Dependencies settingsControllerDeps;
    settingsControllerDeps.appState = &appState;
    settingsControllerDeps.daqDeviceCombo = daqDeviceCombo;
    settingsControllerDeps.daqChannelEdit = daqChannelEdit;
    settingsControllerDeps.amplitudeSpin = amplitudeSpin;
    settingsControllerDeps.frequencySpin = freqSpin;
    settingsControllerDeps.durationSpin = durationSpin;
    settingsControllerDeps.delaySpin = delaySpin;
    settingsControllerDeps.labviewStatusDot = labviewStatusDot;
    settingsControllerDeps.labviewStatusText = labviewStatusText;
    settingsControllerDeps.labviewOutputLabel = labviewOutputLabel;
    settingsControllerDeps.daqStatusItem = daqStatusItem;
    settingsControllerDeps.pipelineEnableCheck = pipelineEnableCheck;
    settingsControllerDeps.viewerOnly = &viewerOnly;
    auto* settingsController = new SettingsWorkspaceController(settingsControllerDeps, this);

    CameraWorkspaceController::Dependencies cameraControllerDeps;
    cameraControllerDeps.app = &app;
    cameraControllerDeps.window = this;
    cameraControllerDeps.statusBar = this->statusBar();
    cameraControllerDeps.cameraWorker = cameraWorker;
    cameraControllerDeps.options = &options;
    cameraControllerDeps.appState = &appState;
    cameraControllerDeps.controls = cameraWorkspaceControls;
    cameraControllerDeps.viewerOnly = &viewerOnly;
    cameraControllerDeps.cameraOpened = &cameraOpened;
    cameraControllerDeps.daqBuildEnabled = kDaqBuildEnabled;
    cameraControllerDeps.initialDaqStatusText = initialDaqStatusText;
    cameraControllerDeps.statusLabel = statusLabel;
    cameraControllerDeps.cameraStatusItem = cameraStatusItem;
    cameraControllerDeps.modelStatusItem = modelStatusItem;
    cameraControllerDeps.daqStatusItem = daqStatusItem;
    cameraControllerDeps.runStatusItem = runStatusItem;
    cameraControllerDeps.pipelineStatusLabel = pipelineStatusLabel;
    cameraControllerDeps.statsLabel = statsLabel;
    cameraControllerDeps.liveImageView = imageView;
    cameraControllerDeps.cameraImageView = cameraImageView;
    cameraControllerDeps.liveViewerEmpty = liveViewerEmpty;
    cameraControllerDeps.cameraViewerEmpty = cameraViewerEmpty;
    cameraControllerDeps.liveHudResolution = liveHudResolution;
    cameraControllerDeps.cameraHudResolution = cameraHudResolution;
    cameraControllerDeps.liveHudFrameTime = liveHudFrameTime;
    cameraControllerDeps.cameraHudFrameTime = cameraHudFrameTime;
    cameraControllerDeps.liveHudFps = liveHudFps;
    cameraControllerDeps.cameraHudFps = cameraHudFps;
    cameraControllerDeps.startButton = startBtn;
    cameraControllerDeps.reconnectButton = reconnectBtn;
    cameraControllerDeps.applyButton = applyBtn;
    cameraControllerDeps.operationalTabs = operationalTabs;
    cameraControllerDeps.pipeline = &pipeline;
    cameraControllerDeps.pipelineMutex = &pipelineMutex;
    cameraControllerDeps.pipelineEnabled = &pipelineEnabled;
    cameraControllerDeps.logLine = logLine;
    cameraControllerDeps.systemLogLine = [](const QString& message) { logMessage(message); };
    auto* cameraController = new CameraWorkspaceController(cameraControllerDeps, this);

    cameraController->updateLutRange(cameraController->currentBits());
    restoreRuntimeSettings();
    syncValidatorWorkspaceRuntimeModel();
    auto repairRuntimeModelPaths = [&]() {
        const QString registryOnnx = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString registryMeta = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        if (!registryOnnx.isEmpty()) {
            onnxEdit->setText(registryOnnx);
        } else if (!defaultWorkspacePaths.activeModel.isEmpty()) {
            onnxEdit->setText(defaultWorkspacePaths.activeModel);
        } else {
            onnxEdit->setText(documentsOnnxFallback);
        }
        if (!registryMeta.isEmpty()) {
            metaEdit->setText(registryMeta);
        } else if (!defaultWorkspacePaths.activeMetadata.isEmpty()) {
            metaEdit->setText(defaultWorkspacePaths.activeMetadata);
        } else {
            metaEdit->setText(documentsMetaFallback);
        }
        logMessage(QString("Runtime model paths repaired from selected registry entry: onnx=%1 meta=%2")
                       .arg(onnxEdit->text(), metaEdit->text()));
    };
    repairRuntimeModelPaths();
    syncValidatorWorkspaceRuntimeModel();
    populateTargetClassSelector();
    cameraController->updateLutRange(cameraController->currentBits());

    QPointer<ViewerWindow> viewerWindow;
    QPointer<StatsFigureWindow> statsFigureWindow;
    QObject::connect(viewerBtn, &QPushButton::clicked, [&]() {
        if (viewerWindow) {
            viewerWindow->raise();
            viewerWindow->activateWindow();
            return;
        }
        viewerWindow = new ViewerWindow(nullptr);
        viewerWindow->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(viewerWindow, &QObject::destroyed, [&]() { viewerWindow = nullptr; });
        viewerWindow->show();
    });

    // Save state
    auto saveBuffer = std::make_shared<std::vector<QImage>>();
    auto saveMutex = std::make_shared<QMutex>();
    std::atomic<bool> recording{false};
    std::atomic<bool> saving{false};
    QElapsedTimer recordTimer;
    QDateTime recordStartTime;
    std::atomic<int> recordedFrames{0};
    QTimer saveInfoTimer;
    saveInfoTimer.setInterval(200);

    QObject::connect(saveBrowseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select save directory", savePathEdit->text());
        if (!dir.isEmpty())
            savePathEdit->setText(dir);
    });
    QObject::connect(saveOpenBtn, &QPushButton::clicked, [&]() {
        QString dir = savePathEdit->text();
        if (dir.isEmpty())
            dir = QCoreApplication::applicationDirPath();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    QObject::connect(onnxBrowseBtn, &QPushButton::clicked, [&]() {
        const QString startPath = chooseOpenFileDialogPath(onnxEdit->text(), defaultWorkspacePaths.activeModel.isEmpty()
                                                                               ? defaultWorkspacePaths.models
                                                                               : defaultWorkspacePaths.activeModel,
                                                           findPackagedAppPath("models"));
        QString file = QFileDialog::getOpenFileName(this, "Select ONNX model", startPath, "ONNX Model (*.onnx)");
        if (!file.isEmpty()) {
            onnxEdit->setText(file);
            syncValidatorWorkspaceRuntimeModel();
            saveRuntimeSettings();
        }
    });
    QObject::connect(metaBrowseBtn, &QPushButton::clicked, [&]() {
        const QString startPath = chooseOpenFileDialogPath(
            metaEdit->text(),
            defaultWorkspacePaths.activeMetadata.isEmpty() ? defaultWorkspacePaths.models : defaultWorkspacePaths.activeMetadata,
            findPackagedAppPath("models"));
        QString file = QFileDialog::getOpenFileName(this, "Select metadata JSON", startPath, "JSON (*.json)");
        if (!file.isEmpty()) {
            metaEdit->setText(file);
            syncValidatorWorkspaceRuntimeModel();
            populateTargetClassSelector();
            saveRuntimeSettings();
        }
    });
    QObject::connect(outputBrowseBtn, &QPushButton::clicked, [&]() {
        const QString startDir =
            chooseExistingDirectoryDialogPath(outputEdit->text(), defaultWorkspacePaths.runs);
        QString dir = QFileDialog::getExistingDirectory(this, "Select output directory", startDir);
        if (!dir.isEmpty()) {
            outputEdit->setText(dir);
            saveRuntimeSettings();
        }
    });
    QObject::connect(liveModelCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&]() {
        applyLiveModelSelection();
        syncValidatorWorkspaceRuntimeModel();
    });
    QObject::connect(refreshLiveModelsBtn, &QPushButton::clicked, [&]() {
        if (refreshLiveModelsFromRegistry)
            refreshLiveModelsFromRegistry(QString());
        else {
            applyLiveModelSelection();
            syncValidatorWorkspaceRuntimeModel();
        }
    });

    auto connectRuntimeSettingsPersistence = [&]() {
        QObject::connect(onnxEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(metaEdit, &QLineEdit::editingFinished, [&]() {
            populateTargetClassSelector();
            saveRuntimeSettings();
        });
        QObject::connect(outputEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(targetClassCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&]() {
            pendingTargetClassId = selectedTargetClassId();
            appState.targetClassId = pendingTargetClassId;
            saveRuntimeSettings();
        });
        QObject::connect(savePathEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(saveCropCheck, &QCheckBox::toggled, saveRuntimeSettings);
        QObject::connect(sortNonTargetCheck, &QCheckBox::toggled, saveRuntimeSettings);
        QObject::connect(saveOverlayCheck, &QCheckBox::toggled, saveRuntimeSettings);

        QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(customWidthSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(customHeightSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(binCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(bitsCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(exposureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(readoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(displayEverySpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(lutMinSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(lutMaxSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);

        auto persistAndScheduleDetectorApply = [&]() {
            saveRuntimeSettings();
            scheduleDetectorApply();
        };
        QObject::connect(frameSkipSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(bgFramesSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(bgUpdateSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(resetFramesSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(minAreaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(minAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(maxAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(minBboxSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(marginSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(diffThreshSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(blurRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(morphRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(scaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(gapFireSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        if (!verifyDefaultPaths) {
            QObject::connect(&app, &QCoreApplication::aboutToQuit, saveRuntimeSettings);
        }
    };
    connectRuntimeSettingsPersistence();
    if (!verifyDefaultPaths) {
        saveRuntimeSettings();
    }

    QObject::connect(pipelineEnableCheck, &QCheckBox::toggled, [&](bool enabled) {
        pipelineEnabled.store(enabled);
        updateForceTriggerState();
        updateLiveRunStartStopVisibility();
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        pipelineStartBtn->setEnabled(!enabled && !sequenceRunning.load() && ready);
        pipelineStopBtn->setEnabled(enabled && !sequenceRunning.load());
        if (!enabled) {
            pipelineStatusLabel->setText("Pipeline: paused");
            settingsController->setLabviewStatus("Disabled", "#666");
            if (!sequenceRunning.load() && !sequenceStarting.load()) {
                stopLiveLogging();
            }
        } else if (daqChannelEdit->text().trimmed().isEmpty()) {
            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
        } else {
            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
        }
        if (enabled && !sequenceRunning.load() && !sequenceStarting.load()) {
            if (ready && !liveLogging.load()) {
                startLiveLogging();
            }
        }
    });

    bool daqStartupStateLogged = false;
    auto logDaqStartupState = [&](const QString& stateText) {
        if (daqStartupStateLogged)
            return;
        daqStartupStateLogged = true;
        logMessage("DAQ startup state: " + stateText);
    };

    auto loadPipeline = [&](bool enableAfter, bool forceNoDaq) {
        if (collectionActive.load()) {
            pipelineStatusLabel->setText("Data collection active: sorting pipeline disabled.");
            logMessage("Sorting pipeline init skipped because data collection is active.");
            return;
        }
        logMessage("Pipeline init requested");
        settingsController->refreshDaqDeviceOptions(true);
        if (liveModelCombo->currentData(kLiveModelModeRole).toString() == "blocked") {
            pipelineStatusLabel->setText(
                "Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            logMessage("Pipeline init blocked by live model selection gate: " + liveModelCombo->currentText());
            return;
        }
        PipelineConfig cfg;
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        cfg.onnxPath = onnxResolved.toStdString();
        cfg.metadataPath = metaResolved.toStdString();
        // Live sorting uses the qualified CPU inference path. The shared device
        // selector continues to control training and Model Testing.
        cfg.computeDevice = "cpu";
        appState.targetClassId = selectedTargetClassId();
        appState.sortNonTarget = sortNonTargetEnabled();
        cfg.targetClassId = appState.targetClassId.toStdString();
        cfg.sortNonTarget = appState.sortNonTarget;
        cfg.outputDir = outputEdit->text().toStdString();
        cfg.saveCrop = saveCropCheck->isChecked();
        cfg.saveOverlay = saveOverlayCheck->isChecked();
        cfg.cropSize = 64;
        cfg.frameSkip = frameSkipSpin->value();
        pipelineDetectCfg.bgFrames = bgFramesSpin->value();
        pipelineDetectCfg.bgUpdateFrames = bgUpdateSpin->value();
        pipelineDetectCfg.resetFrames = resetFramesSpin->value();
        pipelineDetectCfg.minArea = minAreaSpin->value();
        pipelineDetectCfg.minAreaFrac = minAreaFracSpin->value();
        pipelineDetectCfg.maxAreaFrac = maxAreaFracSpin->value();
        pipelineDetectCfg.minBbox = minBboxSpin->value();
        pipelineDetectCfg.margin = marginSpin->value();
        pipelineDetectCfg.diffThresh = diffThreshSpin->value();
        pipelineDetectCfg.blurRadius = blurRadiusSpin->value();
        pipelineDetectCfg.morphRadius = morphRadiusSpin->value();
        pipelineDetectCfg.scale = scaleSpin->value();
        pipelineDetectCfg.gapFireShift = gapFireSpin->value();
        cfg.detect = pipelineDetectCfg;
        cfg.daq.channel = daqChannelEdit->text().trimmed().toStdString();
        cfg.daq.rangeMin = -10.0;
        cfg.daq.rangeMax = 10.0;
        cfg.daq.amplitude = amplitudeSpin->value();
        cfg.daq.frequencyHz = freqSpin->value() * 1000.0;
        cfg.daq.durationMs = durationSpin->value();
        cfg.daq.delayMs = delaySpin->value();
        if (forceNoDaq) {
            cfg.daq = DaqConfig{};
        }

        logMessage(QString("Pipeline init paths: onnx=%1 meta=%2").arg(onnxEdit->text(), metaEdit->text()));
        logMessage(QString("Pipeline init resolved paths: onnx=%1 meta=%2").arg(onnxResolved, metaResolved));
        logMessage(QString("Pipeline compute device requested: %1").arg(selectedComputeDevice()));
        logMessage("Pipeline trigger policy: " + currentTriggerPolicyText());
        if (forceNoDaq) {
            logMessage("DAQ config: disabled for recorded sequence replay");
        } else {
            logMessage(QString("DAQ config: channel=%1 range=[-10,10] amp=%2V freq=%3Hz duration=%4ms delay=%5ms")
                           .arg(daqChannelEdit->text().trimmed())
                           .arg(amplitudeSpin->value(), 0, 'f', 3)
                           .arg(freqSpin->value() * 1000.0, 0, 'f', 1)
                           .arg(durationSpin->value(), 0, 'f', 3)
                           .arg(delaySpin->value(), 0, 'f', 3));
        }
        if (!settingsController->discoveredDaqDevices().empty()) {
            logMessage(QString("DAQ discovery: %1").arg(settingsController->describeDiscoveredDaqDevices()));
        } else if (!settingsController->daqDiscoveryError().isEmpty()) {
            logMessage(QString("DAQ discovery: %1").arg(settingsController->daqDiscoveryError()));
        } else {
            logMessage("DAQ discovery: no NI-DAQmx devices detected");
        }

        std::string err;
        {
            QMutexLocker locker(&pipelineMutex);
            try {
                if (!pipeline.init(cfg, err)) {
                    pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(QString::fromStdString(err)));
                    modelStatusItem->setText("Model: " + conciseModelLoadFailure(QString::fromStdString(err)));
                    this->statusBar()->showMessage("Pipeline initialization failed");
                    pipelineEnabled.store(false);
                    pipelineEnableCheck->setChecked(false);
                    pipelineStartBtn->setEnabled(false);
                    labviewTriggerReady = false;
                    settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                    updateForceTriggerState();
                    logMessage(QString("Pipeline init failed: %1").arg(QString::fromStdString(err)));
                    return;
                }
                pipeline.reset();
                labviewTriggerReady = pipeline.isTriggerReady();
                cfg.targetClassId = pipeline.targetClassId();
            } catch (const std::exception& e) {
                const QString exceptionText = QString::fromLocal8Bit(e.what());
                pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(exceptionText));
                modelStatusItem->setText("Model: " + conciseModelLoadFailure(exceptionText));
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QString("DAQ startup exception: %1").arg(exceptionText);
                this->statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage(QString("Pipeline init threw exception: %1").arg(exceptionText));
                return;
            } catch (...) {
                pipelineStatusLabel->setText("Pipeline error: unknown startup exception");
                modelStatusItem->setText("Model: Model could not be loaded");
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QStringLiteral("DAQ startup exception: unknown");
                this->statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage("Pipeline init threw unknown exception");
                return;
            }
        }

        if (!err.empty()) {
            modelStatusItem->setText("Model: loaded");
            const bool daqWarning = !cfg.daq.channel.empty() && !labviewTriggerReady;
            daqStatusItem->setText(daqWarning ? "DAQ: unavailable" : "DAQ: available");
            appState.daqAvailable = !daqWarning;
            appState.daqFault = daqWarning;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText = daqWarning ? QString::fromStdString(err) : QString();
            if (daqWarning)
                logDaqStartupState(daqStatusItem->text() + ": " + QString::fromStdString(err));
            this->statusBar()->showMessage("Pipeline ready with warning");
            logMessage(QString("Pipeline init warning: %1").arg(QString::fromStdString(err)));
        } else {
            modelStatusItem->setText("Model: loaded");
            daqStatusItem->setText("DAQ: available");
            appState.daqAvailable = true;
            appState.daqFault = false;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText.clear();
            this->statusBar()->showMessage("Pipeline ready");
            logMessage("Pipeline init success");
        }
        setSelectedTargetClassId(QString::fromStdString(cfg.targetClassId));
        appState.targetClassId = QString::fromStdString(cfg.targetClassId);
        saveRuntimeSettings();
        const QString triggerPolicyText = currentTriggerPolicyText();
        const QString executionProvider = QString::fromStdString(pipeline.executionProvider());
        logMessage(QString("Pipeline model execution provider: %1").arg(executionProvider));
        if (!err.empty()) {
            pipelineStatusLabel->setText(
                QString("Pipeline ready, %1, model provider %2; warning: %3")
                    .arg(triggerPolicyText, executionProvider, QString::fromStdString(err)));
        } else {
            pipelineStatusLabel->setText(QString("Pipeline ready, %1, model provider %2").arg(triggerPolicyText, executionProvider));
        }
        logMessage("Pipeline ready with trigger policy: " + triggerPolicyText);
        pipelineStartBtn->setEnabled(!enableAfter && !sequenceRunning.load());
        pipelineEnabled.store(enableAfter);
        if (enableAfter) {
            pipelineEnableCheck->setChecked(true);
        }
        if (enableAfter && !sequenceRunning.load() && !sequenceStarting.load() && !liveLogging.load()) {
            startLiveLogging();
        }

        if (cfg.daq.channel.empty()) {
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            logDaqStartupState(daqStatusItem->text());
        } else {
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            if (!appState.daqAvailable) {
                logDaqStartupState(daqStatusItem->text() + (appState.daqFaultText.isEmpty()
                                                                ? QString()
                                                                : QStringLiteral(": ") + appState.daqFaultText));
            }
        }
        appState.daqWaveformValid = !cfg.daq.channel.empty() && cfg.daq.amplitude > 0.0 && cfg.daq.frequencyHz > 0.0 &&
                                    cfg.daq.durationMs > 0.0;
        appState.daqStatusText = daqStatusItem->text();
        settingsController->updateLabviewOutput();
        updateForceTriggerState();
    };

    QObject::connect(&detectorTuningApplyTimer, &QTimer::timeout, [&]() {
        if (viewerOnly)
            return;
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter, false);
        updateForceTriggerState();
    });
    settingsController->setReloadPipelineCallback([&](bool enableAfter) { loadPipeline(enableAfter, false); });
    settingsController->setUpdateForceTriggerCallback(updateForceTriggerState);
    scheduleDetectorApply = [&]() { detectorTuningApplyTimer.start(); };

    QObject::connect(loadPipelineBtn, &QPushButton::clicked,
                     [&]() { loadPipeline(pipelineEnableCheck->isChecked(), false); });

    QObject::connect(pipelineStartBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        if (collectionActive.load()) {
            statusLabel->setText("Start Sorting blocked: data collection is active.");
            this->statusBar()->showMessage("Stop Data Collection before sorting");
            logMessage("Start Sorting blocked because data collection is active.");
            return;
        }
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            loadPipeline(false, false);
            {
                QMutexLocker lock(&pipelineMutex);
                ready = pipeline.isReady();
            }
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: load a valid pipeline first.");
            runStatusItem->setText("Run: idle");
            this->statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked because pipeline is not ready.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        QString runDir = buildRunOutputDir("live");
        if (runDir.isEmpty()) {
            statusLabel->setText("Start Sorting blocked: failed to create run folder.");
            this->statusBar()->showMessage("Start Sorting blocked: no run folder");
            logMessage("Start Sorting blocked because run folder creation failed.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        outputEdit->setText(runDir);
        writeRuntimeSettingsSnapshot(runDir, "live");
        loadPipeline(true, false);
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: pipeline failed after run setup.");
            runStatusItem->setText("Run: idle");
            this->statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked after run setup because pipeline is not ready.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        reportsWorkspaceController.setCurrentRunDir(runDir);
        statusLabel->setText("Pipeline started.");
        updateForceTriggerState();
        runStatusItem->setText("Run: Live View");
        this->statusBar()->showMessage("Live View started");
    });

    QObject::connect(pipelineStopBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        pipelineEnableCheck->setChecked(false);
        updateForceTriggerState();
        statusLabel->setText("Pipeline stopped.");
        runStatusItem->setText("Run: idle");
        reportsWorkspaceController.refreshOpenRunAvailability();
        this->statusBar()->showMessage("Live sorting stopped");
    });

    auto updateCollectionStatus = [&]() {
        QMutexLocker lock(&collectionMutex);
        if (collectionWriter.sessionDir().isEmpty()) {
            collectionStatusLabel->setText("Collection: idle");
            return;
        }
        collectionStatusLabel->setText(QString("Collection: %1 frames, %2 rows")
                                           .arg(static_cast<qulonglong>(collectionWriter.framesSaved()))
                                            .arg(static_cast<qulonglong>(collectionWriter.rowsLogged())));
    };

    auto runCollectionPostprocessing = [&](const QString& sessionDir) {
        if (sessionDir.trimmed().isEmpty())
            return;

        CollectionPostprocessOptions options;
        options.sessionDir = sessionDir;
        options.collectionsRoot = defaultWorkspacePaths.collections;
        options.preparedDatasetsRoot = defaultWorkspacePaths.preparedDatasets;

        while (true) {
            const CollectionSaveDialogUi ui = buildCollectionSaveDialog(this, QFileInfo(sessionDir).fileName());
            const int accepted = ui.dialog->exec();
            if (accepted != QDialog::Accepted) {
                ui.dialog->deleteLater();
                logMessage(QString("Data Collection post-processing skipped by user: %1").arg(sessionDir));
                return;
            }

            options.collectionName = ui.nameEdit ? ui.nameEdit->text().trimmed() : QString();
            options.createTrainingMetadata = ui.createMetadataCheck ? ui.createMetadataCheck->isChecked() : true;
            ui.dialog->deleteLater();

            if (options.collectionName.isEmpty()) {
                QMessageBox::warning(this, "Save Dataset As", "Enter a collection name.");
                continue;
            }

            const QString sanitizedName = options.collectionName;
            const QString finalCollectionDir = QDir(options.collectionsRoot).filePath(sanitizedName);
            const bool sourceIsFinal =
                QFileInfo(finalCollectionDir).absoluteFilePath().compare(QFileInfo(sessionDir).absoluteFilePath(),
                                                                         Qt::CaseInsensitive) == 0;
            if (QFileInfo::exists(finalCollectionDir) && !sourceIsFinal) {
                QMessageBox::warning(this, "Save Dataset As",
                                     QString("Collection folder already exists:\n%1").arg(finalCollectionDir));
                continue;
            }

            const QString finalDatasetDir = QDir(options.preparedDatasetsRoot).filePath(sanitizedName);
            if (options.createTrainingMetadata && QFileInfo::exists(finalDatasetDir)) {
                QMessageBox::warning(this, "Save Dataset As",
                                     QString("Prepared dataset folder already exists:\n%1").arg(finalDatasetDir));
                continue;
            }
            break;
        }

        auto* progress = new QProgressDialog("Processing saved frames and extracting crops...", QString(), 0, 1, this);
        progress->setWindowTitle("Preparing Dataset");
        progress->setWindowModality(Qt::ApplicationModal);
        progress->setCancelButton(nullptr);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setMinimumDuration(0);
        nameWidget(progress, "PreparingDatasetDialog");
        nameWidget(progress->findChild<QProgressBar*>(), "PreparingDatasetProgressBar");
        progress->show();

        auto result = std::make_shared<CollectionPostprocessResult>();
        QPointer<QObject> target(this);
        QPointer<QProgressDialog> progressDialog(progress);
        QThread* thread = QThread::create([options, result, target, progressDialog]() {
            *result = postprocessCollectionForTraining(
                options, [target, progressDialog](int value, int maximum, const QString& message) {
                    if (!target)
                        return;
                    QMetaObject::invokeMethod(
                        target,
                        [progressDialog, value, maximum, message]() {
                            if (!progressDialog)
                                return;
                            progressDialog->setMaximum(std::max(1, maximum));
                            progressDialog->setValue(std::clamp(value, 0, std::max(1, maximum)));
                            if (!message.isEmpty())
                                progressDialog->setLabelText(message);
                        },
                        Qt::QueuedConnection);
                });

            if (!target)
                return;
            QMetaObject::invokeMethod(
                target,
                [result, progressDialog]() {
                    if (progressDialog) {
                        progressDialog->close();
                        progressDialog->deleteLater();
                    }
                },
                Qt::QueuedConnection);
        });
        QObject::connect(thread, &QThread::finished, this, [=]() {
            if (result->ok) {
                statusLabel->setText(QString("Dataset prepared: %1").arg(result->collectionDir));
                collectionStatusLabel->setText(QString("Collection: %1 crops ready").arg(result->resizedCropsWritten));
                logMessage(QString("Data Collection post-processing complete: collection=%1 raw=%2 crops64=%3")
                               .arg(result->collectionDir)
                               .arg(result->rawCropsWritten)
                               .arg(result->resizedCropsWritten));
                if (!result->datasetManifestPath.isEmpty()) {
                    handOffPreparedDatasetForReview(result->datasetManifestPath);
                }
            } else {
                statusLabel->setText(QString("Dataset preparation failed: %1").arg(result->errorMessage));
                logMessage(QString("Data Collection post-processing failed: %1").arg(result->errorMessage));
                QMessageBox::critical(this, "Preparing Dataset Failed", result->errorMessage);
            }
            thread->deleteLater();
        });
        thread->start();
    };

    auto stopDataCollection = [&](const QString& reason) {
        if (!collectionActive.exchange(false))
            return;

        if (recordDispatcher) {
            const std::uint64_t checkpoint = recordDispatcher->closeCollectionBoundary();
            recordDispatcher->waitThrough(checkpoint);
            const auto snapshot = recordDispatcher->integrity();
            LiveDataCollectionWriter::Integrity integrity;
            integrity.handoffAccepted = snapshot.handoffAccepted;
            integrity.sourceGapCount = snapshot.sourceGapCount;
            integrity.queueRejectedCount = snapshot.queueRejectedCount;
            integrity.consumerFailureCount = snapshot.consumerFailureCount;
            for (const auto& range : snapshot.sourceGaps)
                integrity.sourceGaps.push_back({range.first, range.last});
            for (const auto& range : snapshot.queueRejected)
                integrity.queueRejected.push_back({range.first, range.last});
            for (const auto& range : snapshot.consumerFailures)
                integrity.consumerFailures.push_back({range.first, range.last});
            {
                QMutexLocker collectionLock(&collectionMutex);
                collectionWriter.setIntegrity(std::move(integrity));
            }
            auto rangesText = [](const auto& ranges) {
                QStringList values;
                for (const auto& range : ranges)
                    values.append(QString("%1-%2").arg(range.first).arg(range.last));
                return values.join(";");
            };
            if (snapshot.sourceGapCount > 0) {
                logMessage(QString("Record source gaps: count=%1 ranges=%2")
                               .arg(snapshot.sourceGapCount)
                               .arg(rangesText(snapshot.sourceGaps)));
            }
            if (snapshot.queueRejectedCount > 0) {
                logMessage(QString("Record queue rejections: count=%1 ranges=%2")
                               .arg(snapshot.queueRejectedCount)
                               .arg(rangesText(snapshot.queueRejected)));
            }
            if (snapshot.consumerFailureCount > 0) {
                logMessage(QString("Record consumer failures: count=%1 ranges=%2")
                               .arg(snapshot.consumerFailureCount)
                               .arg(rangesText(snapshot.consumerFailures)));
            }
        }

        QString sessionDir;
        std::string finishErr;
        {
            QMutexLocker collectionLock(&collectionMutex);
            sessionDir = collectionWriter.sessionDir();
            if (!collectionWriter.finish(reason, finishErr)) {
                logMessage(QString("Data Collection finalize failed: %1").arg(QString::fromStdString(finishErr)));
            }
        }
        {
            QMutexLocker pipelineLock(&pipelineMutex);
            pipeline.clear();
        }
        pipelineEnabled.store(false);
        collectionToggleBtn->setText("Start Data Collection");
        pipelineStartBtn->setEnabled(!sequenceRunning.load());
        pipelineStopBtn->setEnabled(false);
        appState.daqAvailable = collectionPreviousDaqAvailable;
        appState.daqDisabled = collectionPreviousDaqDisabled;
        appState.daqFault = collectionPreviousDaqFault;
        appState.daqStatusText = collectionPreviousDaqStatusText;
        appState.daqFaultText = collectionPreviousDaqFaultText;
        daqStatusItem->setText(collectionPreviousDaqStatusText.isEmpty() ? "DAQ: available"
                                                                          : collectionPreviousDaqStatusText);
        settingsController->updateLabviewOutput();
        updateForceTriggerState();
        updateCollectionStatus();
        statusLabel->setText(QString("Data Collection stopped. Output: %1").arg(sessionDir));
        runStatusItem->setText("Run: idle");
        pipelineStatusLabel->setText("Pipeline: paused");
        this->statusBar()->showMessage("Data Collection stopped");
        logMessage(QString("Data Collection stopped: %1").arg(sessionDir));
        if (reason == "user_stop")
            runCollectionPostprocessing(sessionDir);
    };

    auto startDataCollection = [&]() {
        if (collectionActive.load()) {
            stopDataCollection("user_stop");
            return;
        }
        if (sequenceRunning.load()) {
            statusLabel->setText("Start Data Collection blocked: sequence replay is active.");
            return;
        }
        if (pipelineEnableCheck->isChecked() || pipelineEnabled.load()) {
            statusLabel->setText("Start Data Collection blocked: stop sorting first.");
            return;
        }
        if (datasetCaptureActive.load()) {
            statusLabel->setText("Start Data Collection blocked: image set capture is active.");
            return;
        }
        if (!appState.cameraStreaming) {
            statusLabel->setText("Start Data Collection blocked: start the camera stream first.");
            this->statusBar()->showMessage("Start camera before Data Collection");
            return;
        }

        std::string err;
        QString sessionDir;
        {
            QMutexLocker collectionLock(&collectionMutex);
            if (!collectionWriter.start(defaultWorkspacePaths.collections, err)) {
                statusLabel->setText(QString("Start Data Collection failed: %1").arg(QString::fromStdString(err)));
                this->statusBar()->showMessage("Data Collection start failed");
                logMessage(QString("Data Collection start failed: %1").arg(QString::fromStdString(err)));
                return;
            }
            sessionDir = collectionWriter.sessionDir();
        }

        PipelineConfig cfg;
        cfg.detectorOnly = true;
        cfg.saveCrop = false;
        cfg.saveOverlay = false;
        cfg.frameSkip = 0;
        pipelineDetectCfg.bgFrames = bgFramesSpin->value();
        pipelineDetectCfg.bgUpdateFrames = bgUpdateSpin->value();
        pipelineDetectCfg.resetFrames = resetFramesSpin->value();
        pipelineDetectCfg.minArea = minAreaSpin->value();
        pipelineDetectCfg.minAreaFrac = minAreaFracSpin->value();
        pipelineDetectCfg.maxAreaFrac = maxAreaFracSpin->value();
        pipelineDetectCfg.minBbox = minBboxSpin->value();
        pipelineDetectCfg.margin = marginSpin->value();
        pipelineDetectCfg.diffThresh = diffThreshSpin->value();
        pipelineDetectCfg.blurRadius = blurRadiusSpin->value();
        pipelineDetectCfg.morphRadius = morphRadiusSpin->value();
        pipelineDetectCfg.scale = scaleSpin->value();
        pipelineDetectCfg.gapFireShift = gapFireSpin->value();
        cfg.detect = pipelineDetectCfg;
        cfg.daq = DaqConfig{};
        cfg.daq.channel.clear();

        {
            QMutexLocker pipelineLock(&pipelineMutex);
            if (!pipeline.init(cfg, err)) {
                QMutexLocker collectionLock(&collectionMutex);
                std::string finishErr;
                collectionWriter.finish("pipeline_init_failed", finishErr);
                statusLabel->setText(QString("Start Data Collection failed: %1").arg(QString::fromStdString(err)));
                this->statusBar()->showMessage("Data Collection pipeline init failed");
                logMessage(QString("Data Collection detector init failed: %1").arg(QString::fromStdString(err)));
                return;
            }
            pipeline.reset();
        }

        collectionPreviousDaqAvailable = appState.daqAvailable;
        collectionPreviousDaqDisabled = appState.daqDisabled;
        collectionPreviousDaqFault = appState.daqFault;
        collectionPreviousDaqStatusText = appState.daqStatusText;
        collectionPreviousDaqFaultText = appState.daqFaultText;
        appState.daqDisabled = true;
        appState.daqAvailable = false;
        appState.daqFault = false;
        appState.daqFaultText.clear();
        appState.daqStatusText = "DAQ: disabled for data collection";
        daqStatusItem->setText(appState.daqStatusText);

        if (recordDispatcher)
            recordDispatcher->openCollectionBoundary();
        collectionActive.store(true);
        pipelineEnabled.store(true);
        collectionToggleBtn->setText("Stop Data Collection");
        pipelineStartBtn->setEnabled(false);
        pipelineStopBtn->setEnabled(false);
        liveForceTriggerBtn->setEnabled(false);
        modelStatusItem->setText("Model: off");
        pipelineStatusLabel->setText("Data Collection: warming detector");
        statusLabel->setText(QString("Data Collection started: %1").arg(sessionDir));
        runStatusItem->setText("Run: Data Collection");
        settingsController->updateLabviewOutput();
        updateForceTriggerState();
        updateCollectionStatus();
        this->statusBar()->showMessage("Data Collection started");
        logMessage(QString("Data Collection started: %1").arg(sessionDir));
    };

    QObject::connect(collectionToggleBtn, &QPushButton::clicked, startDataCollection);

    auto datasetIntegritySnapshot = [&]() {
        DatasetCaptureIntegrity integrity;
        if (!recordDispatcher)
            return integrity;
        const auto snapshot = recordDispatcher->datasetIntegrity();
        integrity.handoffAccepted = snapshot.handoffAccepted;
        integrity.sourceGapCount = snapshot.sourceGapCount;
        integrity.queueRejectedCount = snapshot.queueRejectedCount;
        integrity.consumerFailureCount = snapshot.consumerFailureCount;
        for (const auto& range : snapshot.sourceGaps)
            integrity.sourceGaps.push_back({range.first, range.last});
        for (const auto& range : snapshot.queueRejected)
            integrity.queueRejected.push_back({range.first, range.last});
        for (const auto& range : snapshot.consumerFailures)
            integrity.consumerFailures.push_back({range.first, range.last});
        return integrity;
    };
    auto logDatasetIntegrity = [&](const DatasetCaptureIntegrity& integrity) {
        auto rangesText = [](const auto& ranges) {
            QStringList values;
            for (const auto& range : ranges)
                values.append(QString("%1-%2").arg(range.first).arg(range.last));
            return values.join(";");
        };
        if (integrity.sourceGapCount > 0) {
            logMessage(QString("Dataset source gaps: count=%1 ranges=%2")
                           .arg(integrity.sourceGapCount)
                           .arg(rangesText(integrity.sourceGaps)));
        }
        if (integrity.queueRejectedCount > 0) {
            logMessage(QString("Dataset queue rejections: count=%1 ranges=%2")
                           .arg(integrity.queueRejectedCount)
                           .arg(rangesText(integrity.queueRejected)));
        }
        if (integrity.consumerFailureCount > 0) {
            logMessage(QString("Dataset consumer failures: count=%1 ranges=%2")
                           .arg(integrity.consumerFailureCount)
                           .arg(rangesText(integrity.consumerFailures)));
        }
    };

    auto stopDatasetCapture = [&](const QString& reason, bool openReview) {
        if (!datasetCaptureActive.exchange(false))
            return;
        datasetBatchPromptPending.store(false);
        if (recordDispatcher) {
            const std::uint64_t checkpoint = recordDispatcher->closeDatasetBoundary();
            recordDispatcher->waitThrough(checkpoint);
        }
        DatasetCaptureIntegrity integrity = datasetIntegritySnapshot();
        logDatasetIntegrity(integrity);
        QString reviewPath;
        std::string err;
        {
            QMutexLocker lock(&datasetCaptureMutex);
            datasetCaptureSession.setIntegrity(std::move(integrity));
            datasetCaptureSession.setStopReason(reason.toStdString());
            if (!datasetCaptureSession.finalize(err)) {
                logMessage(QString("Image Set capture finalize failed: %1").arg(QString::fromStdString(err)));
            }
            reviewPath = datasetCaptureManifestPath;
        }
        datasetStartCaptureBtn->setEnabled(true);
        datasetStopCaptureBtn->setEnabled(false);
        datasetCaptureStatusLabel->setText(
            QString("Image Set capture stopped: %1\nImage set file: %2").arg(reason, reviewPath));
        statusLabel->setText("Image Set capture stopped. Review required before trainer handoff.");
        trainerDatasetEdit->setText(reviewPath);
        if (openReview && QFileInfo::exists(reviewPath)) {
            openDatasetLabelerPath(reviewPath);
        }
    };

    auto startDatasetCapture = [&]() {
        if (collectionActive.load()) {
            datasetCaptureStatusLabel->setText("Image Set capture blocked: data collection is active.");
            statusLabel->setText("Stop Data Collection before starting Image Set capture.");
            return;
        }
        if (datasetCaptureActive.load())
            return;
        DatasetCollectionMode mode = DatasetCollectionMode::Mixed;
        std::string modeText = datasetCaptureModeCombo->currentText().toStdString();
        std::string err;
        if (!DatasetCaptureSession::parseCollectionMode(modeText, mode)) {
            datasetCaptureStatusLabel->setText("Invalid image-set collection mode.");
            return;
        }
        if (liveModelCombo->currentData(kLiveModelModeRole).toString() == "blocked") {
            datasetCaptureStatusLabel->setText("Dataset capture blocked: selected model is not live-use eligible.");
            return;
        }
        QString datasetId;
        QString sessionDir = buildDatasetBuilderDir(&datasetId);
        DatasetCaptureConfig cfg;
        cfg.sessionDir = std::filesystem::path(sessionDir.toStdWString());
        cfg.sessionId = datasetId.toStdString();
        cfg.sourceType = "live_stream";
        cfg.sourcePath = "live_camera";
        cfg.collectionMode = mode;
        cfg.batchTarget = static_cast<std::size_t>(datasetBatchTargetSpin->value());
        cfg.modelPath = resolveAppRelative(onnxEdit->text()).toStdString();
        cfg.metadataPath = resolveAppRelative(metaEdit->text()).toStdString();
        cfg.modelId = liveModelCombo->currentData(kLiveModelIdRole).toString().toStdString();
        cfg.modelSha256 = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString().toStdString();
        cfg.metadataSha256 = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString().toStdString();
        {
            QMutexLocker lock(&datasetCaptureMutex);
            if (!datasetCaptureSession.start(cfg, err)) {
                datasetCaptureStatusLabel->setText("Image Set capture failed: " + QString::fromStdString(err));
                return;
            }
            datasetCaptureDir = sessionDir;
            datasetCaptureManifestPath = QDir(sessionDir).filePath("metadata/dataset_manifest.json");
            datasetBatchPromptPending.store(false);
            if (recordDispatcher)
                recordDispatcher->openDatasetBoundary();
            datasetCaptureActive.store(true);
        }
        saveCropCheck->setChecked(true);
        if (!pipelineEnableCheck->isChecked()) {
            pipelineEnableCheck->setChecked(true);
        }
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            loadPipeline(true, false);
        }
        datasetStartCaptureBtn->setEnabled(false);
        datasetStopCaptureBtn->setEnabled(true);
        datasetCaptureStatusLabel->setText(QString("Image Set capture active: 0 / %1 images\n%2")
                                               .arg(datasetBatchTargetSpin->value())
                                               .arg(sessionDir));
        trainerDatasetEdit->setText(datasetCaptureManifestPath);
        statusLabel->setText("Image Set capture is active. Images remain unreviewed until manual review.");
        logMessage("Image Set live capture started: " + sessionDir);
    };

    QObject::connect(datasetStartCaptureBtn, &QPushButton::clicked, startDatasetCapture);
    QObject::connect(datasetCaptureFromCameraAction, &QAction::triggered, startDatasetCapture);
    QObject::connect(datasetStopCaptureBtn, &QPushButton::clicked, [&]() { stopDatasetCapture("cancelled", true); });

    QObject::connect(labviewReconnectBtn, &QPushButton::clicked, [&]() {
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter, false);
    });

    auto runManualDaqTrigger = [&](const QString& triggerSource) {
        updateForceTriggerState();
        const bool waveformValid = !daqChannelEdit->text().trimmed().isEmpty() && amplitudeSpin->value() > 0.0 &&
                                   freqSpin->value() > 0.0 && durationSpin->value() > 0.0;
        QStringList blockers;
        if (!appState.daqAvailable || appState.daqDisabled)
            blockers << QStringLiteral("DAQ is not available");
        if (appState.daqFault)
            blockers << (appState.daqFaultText.isEmpty() ? QStringLiteral("DAQ fault is active")
                                                         : appState.daqFaultText);
        if (!waveformValid)
            blockers << QStringLiteral("waveform settings are incomplete");
        if (!blockers.isEmpty()) {
            const QString message =
                QStringLiteral("%1 blocked: %2.").arg(triggerSource, blockers.join("; "));
            statusLabel->setText(message);
            this->statusBar()->showMessage("Manual trigger blocked");
            logMessage(message);
            updateForceTriggerState();
            return;
        }
        DaqConfig cfg;
        cfg.channel = daqChannelEdit->text().trimmed().toStdString();
        cfg.rangeMin = -10.0;
        cfg.rangeMax = 10.0;
        cfg.amplitude = amplitudeSpin->value();
        cfg.frequencyHz = freqSpin->value() * 1000.0;
        cfg.durationMs = durationSpin->value();
        cfg.delayMs = delaySpin->value();

        statusLabel->setText("DAQ trigger queued...");
        logMessage(QString("%1 queued direct DAQ output on %2.").arg(triggerSource, daqChannelEdit->text().trimmed()));
        QPointer<QWidget> windowPtr(this);
        QPointer<QLabel> statusLabelPtr(statusLabel);
        backgroundTasks.launch("daq-manual-trigger", [&, cfg, windowPtr,
                                                       statusLabelPtr](const BackgroundTaskRegistry::StopFlag& stop) {
            std::string trigErr;
            bool ok = false;
            double actualSampleRateHz = 0.0;
            int finiteSampleCount = 0;
            double finalSampleValue = 0.0;
            if (!stop->load()) {
                DaqTrigger manualTrigger;
                if (!manualTrigger.init(cfg, trigErr)) {
                    ok = false;
                } else {
                    actualSampleRateHz = manualTrigger.sampleRateHz();
                    finiteSampleCount = manualTrigger.finiteSampleCount();
                    finalSampleValue = manualTrigger.finalSampleValue();
                    ok = manualTrigger.fire(trigErr);
                }
            }
            if (stop->load() || windowPtr.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                windowPtr,
                [&, ok, trigErr, statusLabelPtr, actualSampleRateHz, finiteSampleCount, finalSampleValue]() {
                    if (statusLabelPtr.isNull())
                        return;
                    if (ok) {
                        statusLabelPtr->setText("DAQ trigger sent.");
                        qInfo().noquote()
                            << "Manual DAQ trigger waveform:"
                            << "SampleRateHz=" << actualSampleRateHz
                            << "FiniteSampleCount=" << finiteSampleCount
                            << "FinalSampleV=" << finalSampleValue;
                        logMessage(QStringLiteral("Manual DAQ trigger waveform: SampleRateHz=%1 FiniteSampleCount=%2 "
                                                  "FinalSampleV=%3")
                                       .arg(actualSampleRateHz, 0, 'f', 3)
                                       .arg(finiteSampleCount)
                                       .arg(finalSampleValue, 0, 'f', 6));
                        appState.daqAvailable = true;
                        appState.daqDisabled = false;
                        appState.daqFault = false;
                        appState.daqStatusText = "DAQ: available";
                        settingsController->setLabviewStatus("Connected", "#2ecc71");
                        updateForceTriggerState();
                    } else {
                        statusLabelPtr->setText("DAQ trigger failed: " + QString::fromStdString(trigErr));
                        appState.daqAvailable = false;
                        appState.daqFault = true;
                        appState.daqStatusText = "DAQ: unavailable";
                        appState.daqFaultText = QString::fromStdString(trigErr);
                        settingsController->setLabviewStatus("Disconnected", "#c0392b");
                        updateForceTriggerState();
                        logMessage(QString("Manual DAQ trigger failed: %1").arg(QString::fromStdString(trigErr)));
                    }
                },
                Qt::QueuedConnection);
        });
    };

    QObject::connect(labviewTestBtn, &QPushButton::clicked,
                     [&]() { runManualDaqTrigger(QStringLiteral("Internal manual DAQ trigger")); });
    QObject::connect(liveForceTriggerBtn, &QPushButton::clicked, [&]() {
        updateForceTriggerState();
        if (!liveForceTriggerBtn->isEnabled())
            return;
        runManualDaqTrigger(QStringLiteral("Live View Manual Trigger"));
    });

    QObject::connect(captureBtn, &QPushButton::clicked, [&]() {
        const QImage lastFrame = cameraController->lastFrame();
        if (lastFrame.isNull()) {
            statusLabel->setText("No frame to capture");
            return;
        }
        QString baseDir = savePathEdit->text();
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath();
        QDir dir(baseDir);
        dir.mkpath(".");
        QString fname = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".tiff";
        QString outPath = dir.filePath(fname);
        if (lastFrame.save(outPath, "TIFF")) {
            statusLabel->setText("Captured: " + fname);
            logLine("Captured frame to " + outPath);
        } else {
            statusLabel->setText("Capture failed");
        }
    });

    auto startSaving = [&]() {
        if (saving.load()) {
            statusLabel->setText("Already saving to disk");
            return;
        }
        recording = true;
        {
            QMutexLocker lk(saveMutex.get());
            saveBuffer->clear();
        }
        recordedFrames = 0;
        recordTimer.restart();
        recordStartTime = QDateTime::currentDateTime();
        saveStartBtn->setEnabled(false);
        saveStopBtn->setEnabled(true);
        logLine("Recording started");
        statusLabel->setText("Recording...");
        saveInfoLabel->setText("Elapsed: 0.0 s\nFrames: 0");
        saveInfoTimer.start();
    };

    auto stopSaving = [&]() {
        if (!recording.load())
            return;
        recording = false;
        saveStartBtn->setEnabled(true);
        saveStopBtn->setEnabled(false);
        saveInfoTimer.stop();

        std::shared_ptr<std::vector<QImage>> frames = std::make_shared<std::vector<QImage>>();
        {
            QMutexLocker lk(saveMutex.get());
            frames->swap(*saveBuffer);
        }
        if (frames->empty()) {
            statusLabel->setText("No frames to save");
            return;
        }

        QString baseDir = savePathEdit->text();
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath();
        QDir dir(baseDir);
        dir.mkpath(".");
        QString sub = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString outDir = dir.filePath(sub);
        dir.mkpath(outDir);

        saving = true;
        statusLabel->setText("Saving to disk...");
        logLine(QString("Saving %1 frames to %2").arg(frames->size()).arg(outDir));
        if (!savingDialog) {
            savingDialog = new QDialog(this);
            savingDialog->setWindowTitle("Saving...");
            savingDialog->setModal(true);
            auto layout = new QVBoxLayout(savingDialog);
            savingDialogLabel = new QLabel(savingDialog);
            savingProgress = new QProgressBar(savingDialog);
            savingProgress->setMinimum(0);
            layout->addWidget(savingDialogLabel);
            layout->addWidget(savingProgress);
            savingDialog->setLayout(layout);
        }
        int totalFrames = static_cast<int>(frames->size());
        savingDialogLabel->setText(QString("Saving %1 frames...").arg(totalFrames));
        savingProgress->setRange(0, totalFrames);
        savingProgress->setValue(0);
        savingDialog->show();

        FrameMeta metaCopy = cameraController->lastMeta();
        double expMsCopy = exposureSpin->value();
        QString recordStartStr = recordStartTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
        QPointer<QLabel> statusLabelPtr(statusLabel);
        QPointer<QDialog> savingDialogPtr(savingDialog);
        QPointer<QProgressBar> savingProgressPtr(savingProgress);

        backgroundTasks.launch("capture-save-export", [frames, outDir, logLine, statusLabelPtr, savingDialogPtr,
                                                       savingProgressPtr, totalFrames, metaCopy, expMsCopy,
                                                       recordStartStr,
                                                       &saving](const BackgroundTaskRegistry::StopFlag& stop) {
            int width = std::max(6, static_cast<int>(std::ceil(std::log10(std::max<size_t>(1, frames->size())))));
            bool canceled = false;
            for (size_t i = 0; i < frames->size(); ++i) {
                if (stop->load()) {
                    canceled = true;
                    break;
                }
                const QImage& im = frames->at(i);
                QString fname = QString("%1.tiff").arg(static_cast<int>(i), width, 10, QChar('0'));
                QString path = outDir + "/" + fname;
                im.save(path, "TIFF");
                if (!savingProgressPtr.isNull() && (i % 100 == 0 || i + 1 == frames->size())) {
                    int v = static_cast<int>(i + 1);
                    QMetaObject::invokeMethod(
                        savingProgressPtr,
                        [savingProgressPtr, v]() {
                            if (!savingProgressPtr.isNull()) {
                                savingProgressPtr->setValue(v);
                            }
                        },
                        Qt::QueuedConnection);
                }
            }
            // Write metadata file
            QFile infoFile(outDir + "/capture_info.txt");
            if (!canceled && infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&infoFile);
                ts << "Start: " << recordStartStr << "\n";
                ts << "Frames: " << frames->size() << "\n";
                ts << "Resolution: " << metaCopy.width << " x " << metaCopy.height << "\n";
                ts << "Binning: " << metaCopy.binning << "\n";
                ts << "Bits: " << metaCopy.bits << "\n";
                ts << "Exposure(ms): " << expMsCopy << "\n";
                ts << "Internal FPS: " << metaCopy.internalFps << "\n";
                ts << "Readout speed: " << metaCopy.readoutSpeed << "\n";
                ts.flush();
                infoFile.close();
            }
            logLine(canceled ? QString("Save canceled after partial export to %1").arg(outDir)
                             : QString("Saved %1 frames to %2").arg(frames->size()).arg(outDir));
            if (!statusLabelPtr.isNull()) {
                QMetaObject::invokeMethod(
                    statusLabelPtr,
                    [statusLabelPtr, canceled]() {
                        if (!statusLabelPtr.isNull()) {
                            statusLabelPtr->setText(canceled ? "Save canceled" : "Save complete");
                        }
                    },
                    Qt::QueuedConnection);
            }
            if (!savingDialogPtr.isNull()) {
                QMetaObject::invokeMethod(
                    savingDialogPtr,
                    [savingDialogPtr]() {
                        if (!savingDialogPtr.isNull()) {
                            savingDialogPtr->hide();
                        }
                    },
                    Qt::QueuedConnection);
            }
            saving = false;
        });
    };

    QObject::connect(saveStartBtn, &QPushButton::clicked, startSaving);
    QObject::connect(saveStopBtn, &QPushButton::clicked, stopSaving);

    QObject::connect(&saveInfoTimer, &QTimer::timeout, [&]() {
        if (!recording.load())
            return;
        double elapsed = recordTimer.isValid() ? recordTimer.elapsed() / 1000.0 : 0.0;
        saveInfoLabel->setText(QString("Elapsed: %1 s\nFrames: %2").arg(elapsed, 0, 'f', 1).arg(recordedFrames.load()));
    });

    auto updatePipelineStatus = [&](const PipelineEvent& evt, int bgRemaining, bool pipelineReady) {
        QMetaObject::invokeMethod(
            pipelineStatusLabel,
            [pipelineStatusLabel, &pipelineEnabled, &collectionActive, evt, bgRemaining, pipelineReady]() {
                if (!pipelineEnabled.load()) {
                    pipelineStatusLabel->setText("Pipeline: paused");
                    return;
                }
                if (!pipelineReady) {
                    pipelineStatusLabel->setText("Pipeline: not loaded");
                    return;
                }
                if (collectionActive.load()) {
                    if (bgRemaining > 0) {
                        pipelineStatusLabel->setText(QString("Data Collection: warming detector (%1 frames)").arg(bgRemaining));
                    } else if (evt.detected) {
                        pipelineStatusLabel->setText(QString("Data Collection: detected area=%1").arg(evt.area, 0, 'f', 0));
                    } else {
                        pipelineStatusLabel->setText("Data Collection: recording");
                    }
                    return;
                }
                if (bgRemaining > 0) {
                    pipelineStatusLabel->setText(QString("Pipeline: warming (%1 frames)").arg(bgRemaining));
                    return;
                }
                if (evt.fired) {
                    pipelineStatusLabel->setText(QString("Event: %1 (score %2) area=%3")
                                                     .arg(QString::fromStdString(evt.label))
                                                     .arg(evt.score, 0, 'f', 3)
                                                     .arg(evt.area, 0, 'f', 0));
                } else {
                    pipelineStatusLabel->setText("Pipeline: running");
                }
            },
            Qt::QueuedConnection);
    };

    auto buildClassText = [&](const QMap<QString, int>& counts) -> QString {
        if (counts.isEmpty())
            return "Classes:\n(none)";
        QStringList order = {"Empty", "Single", "MoreThanTwo", ">2", "2"};
        QSet<QString> used;
        QString text = "Classes:";
        for (const QString& name : order) {
            if (counts.contains(name)) {
                text += QString("\n%1: %2").arg(name).arg(counts.value(name));
                used.insert(name);
            }
        }
        for (auto it = counts.begin(); it != counts.end(); ++it) {
            if (used.contains(it.key()))
                continue;
            text += QString("\n%1: %2").arg(it.key()).arg(it.value());
        }
        return text;
    };

    auto makeStatsSnapshot = [&](const StatsTracker& s) -> StatsSnapshot {
        StatsSnapshot snap;
        snap.totalEvents = s.totalEvents;
        snap.classifiedHitCount = s.classifiedHitCount;
        snap.classifiedWasteCount = s.classifiedWasteCount;
        snap.wentToHitCount = s.wentToHitCount;
        snap.wentToWasteCount = s.wentToWasteCount;
        snap.eventActive = s.eventActive;
        snap.classText = buildClassText(s.classCounts);
        snap.classCounts = s.classCounts;
        snap.lastEventDir = s.lastEventDir;
        snap.lastEventLabel = s.lastEventLabel;
        snap.lastDecisionFrame = s.lastDecisionFrame;
        snap.lastDecisionEventId = s.lastDecisionEventId;
        if (!s.lastEventLabel.isEmpty()) {
            snap.lastText = QString("Last event: %1 (%2)").arg(s.lastEventLabel, displayDecisionDirection(s.lastEventDir));
        } else {
            snap.lastText = QString("Last event: --");
        }
        return snap;
    };

    auto getStatsSnapshot = [&]() -> StatsSnapshot {
        QMutexLocker lock(&statsMutex);
        return makeStatsSnapshot(stats);
    };

    auto buildStatsFigures = [&](const StatsSnapshot& snap) {
        int hit = snap.wentToHitCount;
        int waste = snap.wentToWasteCount;
        QImage hitWaste = renderPieChart("Went to Sort vs Pass", {"Went to Sort", "Went to Pass"},
                                         {static_cast<double>(hit), static_cast<double>(waste)},
                                         {QColor(46, 204, 113), QColor(192, 57, 43)});

        int empty = 0;
        int single = 0;
        int more = 0;
        for (auto it = snap.classCounts.begin(); it != snap.classCounts.end(); ++it) {
            QString label = it.key().trimmed().toLower();
            int count = it.value();
            if (label.contains("empty")) {
                empty += count;
            } else if (label.contains("single")) {
                single += count;
            } else if (label.contains("more") || label.contains(">") || label == "2") {
                more += count;
            } else if (!label.isEmpty() && label != "(unclassified)") {
                more += count;
            }
        }
        QImage classImg = 
            renderPieChart("Class Distribution", {"0", "1", ">2"}, 
                           {static_cast<double>(empty), static_cast<double>(single), static_cast<double>(more)}, 
                           {desktop_app::theme::semanticClassColor("0"), desktop_app::theme::semanticClassColor("1"),
                            desktop_app::theme::semanticClassColor("2")}); 
        return std::pair<QImage, QImage>(hitWaste, classImg); 
    }; 

    auto saveStatsFigures = [&](const QString& outDir, const QString& prefix, const StatsSnapshot& snap) -> bool {
        if (outDir.isEmpty())
            return false;
        auto figures = buildStatsFigures(snap);
        QDir out(outDir);
        out.mkpath(".");
        QString hitPath = out.filePath(prefix + "_hit_waste.png");
        QString clsPath = out.filePath(prefix + "_class_dist.png");
        bool ok1 = !figures.first.isNull() && figures.first.save(hitPath);
        bool ok2 = !figures.second.isNull() && figures.second.save(clsPath);
        return ok1 && ok2;
    };

    auto updateStatsFigureWindow = [&](const StatsSnapshot& snap) {
        if (!statsFigureWindow)
            return;
        auto figures = buildStatsFigures(snap);
        statsFigureWindow->setImages(figures.first, figures.second);
    };

    auto applyStatsSnapshot = [&](const StatsSnapshot& snap) {
        QMetaObject::invokeMethod(
            statsEventsLabel,
            [=]() {
                statsEventsLabel->setText(
                    QString("Events: %1  Active: %2").arg(snap.totalEvents).arg(snap.eventActive ? "Yes" : "No"));
                statsHitLabel->setText(QString("Classified Sort: %1\nClassified Pass: %2\nWent to Sort: %3\nWent to "
                                               "Pass: %4")
                                           .arg(snap.classifiedHitCount)
                                           .arg(snap.classifiedWasteCount)
                                           .arg(snap.wentToHitCount)
                                           .arg(snap.wentToWasteCount));
                statsClassLabel->setText(snap.classText);
                statsLastLabel->setText(snap.lastText);
            },
            Qt::QueuedConnection);
    };

    auto resetStats = [&]() {
        StatsSnapshot snap;
        {
            QMutexLocker lock(&statsMutex);
            stats = StatsTracker{};
            snap = makeStatsSnapshot(stats);
        }
        applyStatsSnapshot(snap);
    };

    auto showStatsFigures = [&]() {
        StatsSnapshot snap = getStatsSnapshot();
        auto figures = buildStatsFigures(snap);
        if (!statsFigureWindow) {
            statsFigureWindow = new StatsFigureWindow(this);
            statsFigureWindow->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(statsFigureWindow, &QObject::destroyed, [&]() { statsFigureWindow = nullptr; });
            QObject::connect(statsFigureWindow->saveButton(), &QPushButton::clicked, [&]() {
                QString outDir = outputEdit->text().trimmed();
                if (outDir.isEmpty())
                    outDir = QCoreApplication::applicationDirPath();
                QString dir = QFileDialog::getExistingDirectory(statsFigureWindow, "Select output directory", outDir);
                if (dir.isEmpty())
                    return;
                QString prefix = QDateTime::currentDateTime().toString("stats_yyyyMMdd_hhmmss");
                if (statsFigureWindow->saveImages(dir, prefix)) {
                    statusLabel->setText("Saved stats figures to " + dir);
                    logLine("Saved stats figures to " + dir);
                } else {
                    statusLabel->setText("Failed to save stats figures.");
                }
            });
        }
        statsFigureWindow->setImages(figures.first, figures.second);
        statsFigureWindow->show();
        statsFigureWindow->raise();
        statsFigureWindow->activateWindow();
    };

    auto endEventLocked = [&](StatsTracker& s, int decisionFrame) {
        if (!s.eventActive)
            return;
        QString dir = decideEventDirection(s.cumulativeDy, s.lastY, s.frameHeight, s.hasCentroid);
        if (dir == "Waste") {
            s.wentToWasteCount++;
        } else if (dir == "Hit") {
            s.wentToHitCount++;
        }
        s.lastEventDir = dir;
        s.lastEventLabel = s.currentLabel;
        s.lastDecisionFrame = decisionFrame;
        s.lastDecisionEventId = s.currentEventId;
        s.eventActive = false;
        s.hasCentroid = false;
        s.missCount = 0;
        s.currentLabel.clear();
        s.cumulativeDy = 0.0;
    };

    auto updateStatsFromEvent = [&](const PipelineEvent& evt, bool processed) {
        if (!processed)
            return;
        StatsSnapshot snap;
        {
            QMutexLocker lock(&statsMutex);
            if (evt.fired) {
                if (stats.eventActive) {
                    endEventLocked(stats, evt.frameNumber);
                }
                stats.eventActive = true;
                stats.missCount = 0;
                stats.currentEventId++;
                stats.startCentroid = evt.centroid;
                stats.lastCentroid = evt.centroid;
                stats.hasCentroid = true;
                stats.cumulativeDy = 0.0;
                stats.lastY = evt.centroid.y;
                stats.minY = evt.centroid.y;
                stats.maxY = evt.centroid.y;
                if (evt.frameHeight > 0)
                    stats.frameHeight = evt.frameHeight;
                stats.totalEvents++;
                QString label = QString::fromStdString(evt.label);
                if (label.isEmpty())
                    label = "(unclassified)";
                stats.currentLabel = label;
                if (evt.classified) {
                    stats.classCounts[label] = stats.classCounts.value(label) + 1;
                    if (evt.shouldTrigger) {
                        stats.classifiedHitCount++;
                    } else {
                        stats.classifiedWasteCount++;
                    }
                }
            } else if (evt.detected) {
                if (!stats.eventActive) {
                    stats.eventActive = true;
                    stats.missCount = 0;
                    stats.currentEventId++;
                    stats.startCentroid = evt.centroid;
                    stats.lastCentroid = evt.centroid;
                    stats.hasCentroid = true;
                    stats.cumulativeDy = 0.0;
                    stats.lastY = evt.centroid.y;
                    stats.minY = evt.centroid.y;
                    stats.maxY = evt.centroid.y;
                    if (evt.frameHeight > 0)
                        stats.frameHeight = evt.frameHeight;
                    stats.totalEvents++;
                    QString label = QString::fromStdString(evt.label);
                    if (label.isEmpty())
                        label = "(unclassified)";
                    stats.currentLabel = label;
                    if (evt.classified) {
                        stats.classCounts[label] = stats.classCounts.value(label) + 1;
                        if (evt.shouldTrigger) {
                            stats.classifiedHitCount++;
                        } else {
                            stats.classifiedWasteCount++;
                        }
                    }
                } else {
                    stats.cumulativeDy += static_cast<double>(evt.centroid.y - stats.lastCentroid.y);
                    stats.lastCentroid = evt.centroid;
                    stats.hasCentroid = true;
                    stats.lastY = evt.centroid.y;
                    stats.minY = std::min(stats.minY, static_cast<double>(evt.centroid.y));
                    stats.maxY = std::max(stats.maxY, static_cast<double>(evt.centroid.y));
                    if (evt.frameHeight > 0)
                        stats.frameHeight = evt.frameHeight;
                    stats.missCount = 0;
                }
            } else if (stats.eventActive) {
                stats.missCount++;
                if (stats.missCount >= pipelineDetectCfg.resetFrames) {
                    endEventLocked(stats, evt.frameNumber);
                }
            }
            snap = makeStatsSnapshot(stats);
        }
        applyStatsSnapshot(snap);
    };

    auto processPipelineFrame = [&](const QImage& img, PipelineEvent& evt, int& bgRemaining, bool& pipelineReady,
                                    double* procMsOut) -> bool {
        bgRemaining = 0;
        pipelineReady = false;
        if (img.isNull())
            return false;

        QImage lutImg = cameraController->applyLutToImage(img);
        cv::Mat gray(lutImg.height(), lutImg.width(), CV_8UC1, const_cast<uchar*>(lutImg.bits()),
                     lutImg.bytesPerLine());
        cv::Mat grayCopy = gray.clone();

        auto t0 = std::chrono::steady_clock::now();
        bool processed = false;
        {
            QMutexLocker lock(&pipelineMutex);
            pipelineReady = pipeline.isReady();
            if (pipelineReady) {
                processed = pipeline.processFrame(grayCopy, evt);
                bgRemaining = pipeline.backgroundFramesRemaining();
            }
        }
        auto t1 = std::chrono::steady_clock::now();
        if (procMsOut) {
            *procMsOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
        }

        updatePipelineStatus(evt, bgRemaining, pipelineReady);
        updateStatsFromEvent(evt, processed);
        return processed;
    };

    auto currentModelLogFields = [&]() -> RuntimeModelLogFields {
        RuntimeModelLogFields fields;
        fields.registryEntryId = liveModelCombo->currentData(kLiveModelIdRole).toString();
        fields.modelStateAtStart = liveModelCombo->currentData(kLiveModelStateRole).toString();
        fields.liveUseMode = liveModelCombo->currentData(kLiveModelModeRole).toString();
        fields.modelSha256 = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString();
        fields.metadataSha256 = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString();
        return fields;
    };

    startLiveLogging = [&]() {
        QMutexLocker lock(&liveLogMutex);
        liveLog.clear();
        liveLogStart = QDateTime::currentDateTime();
        {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.reset(resetFramesSpin->value());
        }
        liveLogging.store(true);
    };

    stopLiveLogging = [&]() {
        if (!liveLogging.exchange(false))
            return;
        std::vector<LiveLogRecord> records;
        {
            QMutexLocker lock(&liveLogMutex);
            records = liveLog;
        }
        std::vector<SequenceEventRecord> liveEvents;
        {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.finalize();
            liveEvents = liveEventTracker.events;
        }
        StatsSnapshot snap = getStatsSnapshot();
        QString outDir = outputEdit->text().trimmed();
        if (outDir.isEmpty())
            outDir = QCoreApplication::applicationDirPath();
        QString timestamp = liveLogStart.isValid() ? liveLogStart.toString("yyyyMMdd_hhmmss")
                                                   : QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString prefix = "live_" + timestamp;
        QString logPath = writeLiveLogCsv(outDir, prefix, records);
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        QString targetLabel = selectedTargetClassId();
        QString daqChannel = daqChannelEdit->text().trimmed();
        double daqAmp = amplitudeSpin->value();
        double daqFreqHz = freqSpin->value() * 1000.0;
        double daqDuration = durationSpin->value();
        double daqDelay = delaySpin->value();
        int frameSkip = frameSkipSpin->value();
        int bgFrames = bgFramesSpin->value();
        int bgUpdate = bgUpdateSpin->value();
        int resetFrames = resetFramesSpin->value();
        double minArea = minAreaSpin->value();
        double minAreaFrac = minAreaFracSpin->value();
        double maxAreaFrac = maxAreaFracSpin->value();
        int minBbox = minBboxSpin->value();
        int margin = marginSpin->value();
        int diffThresh = diffThreshSpin->value();
        int blurRadius = blurRadiusSpin->value();
        int morphRadius = morphRadiusSpin->value();
        double scale = scaleSpin->value();
        int gapFireShift = gapFireSpin->value();
        int displayEvery = std::max(1, displayEverySpin->value());
        double avgFps = 0.0;
        int fpsCount = 0;
        for (const auto& rec : records) {
            if (rec.fps > 0.0) {
                avgFps += rec.fps;
                fpsCount++;
            }
        }
        if (fpsCount > 0) {
            avgFps /= fpsCount;
        }
        SequenceLogMetadata liveSequenceLogMetadata;
        liveSequenceLogMetadata.displayEvery = displayEvery;
        liveSequenceLogMetadata.onnxResolved = onnxResolved;
        liveSequenceLogMetadata.metadataResolved = metaResolved;
        liveSequenceLogMetadata.targetLabel = targetLabel;
        liveSequenceLogMetadata.model = currentModelLogFields();
        liveSequenceLogMetadata.frameSkip = frameSkip;
        liveSequenceLogMetadata.bgFrames = bgFrames;
        liveSequenceLogMetadata.bgUpdate = bgUpdate;
        liveSequenceLogMetadata.resetFrames = resetFrames;
        liveSequenceLogMetadata.minArea = minArea;
        liveSequenceLogMetadata.minAreaFrac = minAreaFrac;
        liveSequenceLogMetadata.maxAreaFrac = maxAreaFrac;
        liveSequenceLogMetadata.minBbox = minBbox;
        liveSequenceLogMetadata.margin = margin;
        liveSequenceLogMetadata.diffThresh = diffThresh;
        liveSequenceLogMetadata.blurRadius = blurRadius;
        liveSequenceLogMetadata.morphRadius = morphRadius;
        liveSequenceLogMetadata.scale = scale;
        liveSequenceLogMetadata.gapFireShift = gapFireShift;
        liveSequenceLogMetadata.daqChannel = daqChannel;
        liveSequenceLogMetadata.daqAmplitude = daqAmp;
        liveSequenceLogMetadata.daqFrequencyHz = daqFreqHz;
        liveSequenceLogMetadata.daqDurationMs = daqDuration;
        liveSequenceLogMetadata.daqDelayMs = daqDelay;
        QString seqLogPath = writeLiveSequenceLog(outDir, timestamp, records, liveSequenceLogMetadata);
        QString trajPath =
            writeEventTrajectoryCsv(outDir, "sequence_event_trajectory_live_" + timestamp + ".csv", liveEvents);
        SequenceSummaryMetadata liveSummaryMetadata;
        liveSummaryMetadata.targetLabel = targetLabel;
        liveSummaryMetadata.totalFrames = static_cast<int>(records.size());
        liveSummaryMetadata.fps = avgFps;
        liveSummaryMetadata.outputDir = outDir;
        liveSummaryMetadata.onnxResolved = onnxResolved;
        liveSummaryMetadata.metadataResolved = metaResolved;
        liveSummaryMetadata.model = liveSequenceLogMetadata.model;
        QString summaryPath = writeSequenceSummaryCsv(outDir, "sequence_summary_live_" + timestamp + ".csv", liveEvents,
                                                      liveSummaryMetadata);
        saveStatsFigures(outDir, prefix, snap);
        updateStatsFigureWindow(snap);
        if (!logPath.isEmpty()) {
            QString status = "Pipeline stopped. Log: " + logPath;
            if (!seqLogPath.isEmpty()) {
                status += "\nSequence log: " + seqLogPath;
            }
            if (!summaryPath.isEmpty()) {
                status += "\nSummary: " + summaryPath;
            }
            statusLabel->setText(status);
            logLine("Saved live pipeline log to " + logPath);
            if (!seqLogPath.isEmpty()) {
                logLine("Saved live sequence log to " + seqLogPath);
            }
            if (!trajPath.isEmpty()) {
                logLine("Saved live event trajectory to " + trajPath);
            }
            if (!summaryPath.isEmpty()) {
                logLine("Saved live sequence summary to " + summaryPath);
            }
        } else {
            statusLabel->setText("Pipeline stopped. Failed to write log.");
        }
    };

    QObject::connect(statsResetBtn, &QPushButton::clicked, resetStats);
    QObject::connect(statsShowBtn, &QPushButton::clicked, showStatsFigures);
    QObject::connect(runStateResetButton, &QPushButton::clicked, resetStats);

    QObject::connect(seqStartBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        std::shared_ptr<std::vector<SequenceFrame>> frames;
        {
            QMutexLocker lock(&sequenceMutex);
            frames = sequenceFrames;
        }
        if (!frames || frames->empty()) {
            seqStatusLabel->setText("No sequence loaded.");
            return;
        }
        double fps = seqFpsSpin->value();
        if (fps <= 0.0) {
            seqStatusLabel->setText("FPS must be greater than 0.");
            return;
        }

        if (liveLogging.load()) {
            stopLiveLogging();
        }
        QString runDir = buildRunOutputDir("sequence");
        if (!runDir.isEmpty()) {
            outputEdit->setText(runDir);
            writeRuntimeSettingsSnapshot(runDir, "sequence");
            reportsWorkspaceController.setCurrentRunDir(runDir);
        }
        sequencePrevPipelineChecked = pipelineEnableCheck->isChecked();
        sequenceStarting.store(true);
        loadPipeline(true, true);
        bool pipelineReady = false;
        bool triggerReady = false;
        {
            QMutexLocker lock(&pipelineMutex);
            pipelineReady = pipeline.isReady();
            triggerReady = pipeline.isTriggerReady();
        }
        sequenceStarting.store(false);
        if (!pipelineReady) {
            seqStatusLabel->setText("Pipeline not ready. Fix settings and load pipeline.");
            return;
        }
        if (triggerReady) {
            pipelineEnabled.store(false);
            pipelineEnableCheck->setChecked(false);
            pipelineStartBtn->setEnabled(false);
            pipelineStopBtn->setEnabled(false);
            updateLiveRunStartStopVisibility();
            updateForceTriggerState();
            runStatusItem->setText("Run: idle");
            pipelineStatusLabel->setText("Pipeline: paused");
            statusLabel->setText("Sequence replay blocked: DAQ trigger path is still armed.");
            seqStatusLabel->setText("Sequence replay blocked: DAQ trigger path is still armed.");
            this->statusBar()->showMessage("Sequence replay blocked: DAQ trigger path armed");
            reportsWorkspaceController.refreshOpenRunAvailability();
            logMessage("Sequence replay blocked because replay pipeline reported DAQ trigger-ready.");
            return;
        }
        if (sequenceThread.joinable()) {
            sequenceThread.join();
        }
        sequenceStop.store(false);
        sequenceRunning.store(true);
        validatorWorkspaceController->setSequenceUiRunning(true);

        if (!viewerOnly) {
            QMetaObject::invokeMethod(
                cameraWorker, [cameraWorker]() { cameraWorker->stopCapture(); }, Qt::BlockingQueuedConnection);
        }
        statusLabel->setText("Sequence test running.");
        if (pipeline.isReady()) {
            QMutexLocker lock(&pipelineMutex);
            pipeline.reset();
            pipelineStatusLabel->setText("Pipeline: warming (sequence start)");
        }

        QString outDir = outputEdit->text().trimmed();
        if (outDir.isEmpty()) {
            outDir = QCoreApplication::applicationDirPath();
        }
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        QString targetLabel = selectedTargetClassId();
        QString seqFolder = seqFolderEdit->text().trimmed();
        QString daqChannel = daqChannelEdit->text().trimmed();
        double daqAmp = amplitudeSpin->value();
        double daqFreqHz = freqSpin->value() * 1000.0;
        double daqDuration = durationSpin->value();
        double daqDelay = delaySpin->value();
        QDir out(outDir);
        out.mkpath(".");
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString logPath = out.filePath("sequence_test_log_" + timestamp + ".csv");
        seqLogLabel->setText("Log: " + logPath);
        seqStatusLabel->setText(QString("Running %1 frames at %2 fps...").arg(frames->size()).arg(fps, 0, 'f', 2));

        int displayEvery = std::max(1, displayEverySpin->value());

        int frameSkip = frameSkipSpin->value();
        int bgFrames = bgFramesSpin->value();
        int bgUpdate = bgUpdateSpin->value();
        int resetFrames = resetFramesSpin->value();
        double minArea = minAreaSpin->value();
        double minAreaFrac = minAreaFracSpin->value();
        double maxAreaFrac = maxAreaFracSpin->value();
        int minBbox = minBboxSpin->value();
        int margin = marginSpin->value();
        int diffThresh = diffThreshSpin->value();
        int blurRadius = blurRadiusSpin->value();
        int morphRadius = morphRadiusSpin->value();
        double scale = scaleSpin->value();
        int gapFireShift = gapFireSpin->value();

        SequenceLogMetadata sequenceLogMetadata;
        sequenceLogMetadata.sequenceFolder = seqFolder;
        sequenceLogMetadata.fps = fps;
        sequenceLogMetadata.frameCount = static_cast<int>(frames->size());
        sequenceLogMetadata.displayEvery = displayEvery;
        sequenceLogMetadata.outputDir = outDir;
        sequenceLogMetadata.onnxResolved = onnxResolved;
        sequenceLogMetadata.metadataResolved = metaResolved;
        sequenceLogMetadata.targetLabel = targetLabel;
        sequenceLogMetadata.pipelineEnabledBefore = sequencePrevPipelineChecked;
        sequenceLogMetadata.pipelineForced = !sequencePrevPipelineChecked;
        sequenceLogMetadata.frameSkip = frameSkip;
        sequenceLogMetadata.bgFrames = bgFrames;
        sequenceLogMetadata.bgUpdate = bgUpdate;
        sequenceLogMetadata.resetFrames = resetFrames;
        sequenceLogMetadata.minArea = minArea;
        sequenceLogMetadata.minAreaFrac = minAreaFrac;
        sequenceLogMetadata.maxAreaFrac = maxAreaFrac;
        sequenceLogMetadata.minBbox = minBbox;
        sequenceLogMetadata.margin = margin;
        sequenceLogMetadata.diffThresh = diffThresh;
        sequenceLogMetadata.blurRadius = blurRadius;
        sequenceLogMetadata.morphRadius = morphRadius;
        sequenceLogMetadata.scale = scale;
        sequenceLogMetadata.gapFireShift = gapFireShift;
        sequenceLogMetadata.daqChannel = daqChannel;
        sequenceLogMetadata.daqAmplitude = daqAmp;
        sequenceLogMetadata.daqFrequencyHz = daqFreqHz;
        sequenceLogMetadata.daqDurationMs = daqDuration;
        sequenceLogMetadata.daqDelayMs = daqDelay;

        SequenceSummaryMetadata sequenceSummaryMetadata;
        sequenceSummaryMetadata.targetLabel = targetLabel;
        sequenceSummaryMetadata.totalFrames = static_cast<int>(frames->size());
        sequenceSummaryMetadata.fps = fps;
        sequenceSummaryMetadata.sequenceFolder = seqFolder;
        sequenceSummaryMetadata.outputDir = outDir;
        sequenceSummaryMetadata.onnxResolved = onnxResolved;
        sequenceSummaryMetadata.metadataResolved = metaResolved;
        sequenceSummaryMetadata.model = currentModelLogFields();

        sequenceThread = std::thread([&, frames, fps, displayEvery, logPath, outDir, timestamp, sequenceLogMetadata,
                                      sequenceSummaryMetadata, resetFrames]() {
            SequenceLogWriter sequenceLogWriter;
            if (!sequenceLogWriter.open(logPath, sequenceLogMetadata)) {
                validatorWorkspaceController->updateSequenceStatus("Failed to open sequence log.");
                sequenceRunning.store(false);
                QMetaObject::invokeMethod(
                    this,
                    [&, logPath]() {
                        validatorWorkspaceController->setSequenceUiRunning(false);
                        statusLabel->setText("Sequence test failed (log open).");
                        seqLogLabel->setText("Log: " + logPath);
                    },
                    Qt::QueuedConnection);
                return;
            }

            SequenceEventTracker tracker;
            tracker.reset(resetFrames);

            using clock = std::chrono::steady_clock;
            auto start = clock::now();
            std::chrono::duration<double> period(1.0 / fps);

            for (size_t i = 0; i < frames->size(); ++i) {
                if (sequenceStop.load())
                    break;
                auto target = start + period * static_cast<double>(i);
                while (!sequenceStop.load()) {
                    auto now = clock::now();
                    if (now >= target)
                        break;
                    auto remaining = target - now;
                    if (remaining > std::chrono::milliseconds(2)) {
                        std::this_thread::sleep_for(remaining - std::chrono::milliseconds(1));
                    } else {
                        std::this_thread::yield();
                    }
                }
                if (sequenceStop.load())
                    break;

                const SequenceFrame& frame = frames->at(i);
                double scheduledMs = std::chrono::duration<double, std::milli>(period * static_cast<double>(i)).count();
                double actualMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
                double jitterMs = actualMs - scheduledMs;
                QString wallTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

                FrameMeta meta;
                meta.width = frame.image.width();
                meta.height = frame.image.height();
                meta.bits = 8;
                meta.binning = 1.0;
                meta.frameIndex = static_cast<qint64>(i);
                meta.delivered = static_cast<qint64>(i + 1);
                meta.dropped = 0;
                meta.internalFps = fps;

                PipelineEvent evt;
                int bgRemaining = 0;
                bool pipelineReady = false;
                double procMs = 0.0;
                bool processed = processPipelineFrame(frame.image, evt, bgRemaining, pipelineReady, &procMs);
                bool enabledNow = pipelineEnabled.load();
                QString skipReason;
                if (!enabledNow) {
                    skipReason = "pipeline_disabled";
                } else if (!pipelineReady) {
                    skipReason = "pipeline_not_ready";
                } else if (!processed) {
                    skipReason = "frame_skipped";
                }

                tracker.update(evt, processed);

                if (displayEvery > 0 && (static_cast<int>(i) % displayEvery == 0)) {
                    QImage imgCopy = cameraController->applyLutToImage(frame.image);
                    QMetaObject::invokeMethod(
                        this,
                        [&, imgCopy, meta, i, fps, frames]() {
                            imageView->setImage(imgCopy);
                            cameraController->storeLastFrame(imgCopy, meta);
                            statsLabel->setText(
                                QString("Source: Sequence\nResolution: %1 x %2\nBits: %3\nFPS: %4\nFrame: %5 / %6")
                                    .arg(meta.width)
                                    .arg(meta.height)
                                    .arg(meta.bits)
                                    .arg(fps, 0, 'f', 2)
                                    .arg(i + 1)
                                    .arg(frames->size()));
                        },
                        Qt::QueuedConnection);
                }

                QString cropPath = QString::fromStdString(evt.cropPath);
                QString label = QString::fromStdString(evt.label);
                SequenceLogFrameRow row;
                row.index = static_cast<int>(i);
                row.filename = QFileInfo(frame.path).fileName();
                row.scheduledMs = scheduledMs;
                row.actualMs = actualMs;
                row.jitterMs = jitterMs;
                row.wallTime = wallTime;
                row.procMs = procMs;
                row.processed = processed;
                row.pipelineEnabled = enabledNow;
                row.pipelineReady = pipelineReady;
                row.bgRemaining = bgRemaining;
                row.skipReason = skipReason;
                row.detected = evt.detected;
                row.fired = evt.fired;
                row.area = evt.area;
                row.bboxX = evt.bbox.x;
                row.bboxY = evt.bbox.y;
                row.bboxW = evt.bbox.width;
                row.bboxH = evt.bbox.height;
                row.cropX = evt.cropRect.x;
                row.cropY = evt.cropRect.y;
                row.cropW = evt.cropRect.width;
                row.cropH = evt.cropRect.height;
                row.cropPath = cropPath;
                row.label = label;
                row.score = evt.score;
                row.triggered = evt.triggered;
                row.triggerOk = evt.triggerOk;
                row.frameNumber = evt.frameNumber;
                row.eventDir = tracker.lastEventDir;
                row.decisionFrame = tracker.lastDecisionFrame;
                row.decisionEventId = tracker.lastDecisionEventId;
                sequenceLogWriter.writeFrame(row);
                if (i % 50 == 0) {
                    sequenceLogWriter.flush();
                }
            }

            tracker.finalize();
            QString trajPath =
                writeEventTrajectoryCsv(outDir, "sequence_event_trajectory_" + timestamp + ".csv", tracker.events);
            QString summaryPath = writeSequenceSummaryCsv(outDir, "sequence_summary_" + timestamp + ".csv",
                                                          tracker.events, sequenceSummaryMetadata);

            sequenceLogWriter.close();

            const bool sequenceWasStopped = sequenceStop.load();
            sequenceRunning.store(false);
            QMetaObject::invokeMethod(
                this,
                [&, logPath, trajPath, summaryPath, sequenceWasStopped]() {
                    validatorWorkspaceController->setSequenceUiRunning(false);
                    seqStatusLabel->setText(sequenceWasStopped ? "Sequence stopped." : "Sequence finished.");
                    statusLabel->setText(sequenceWasStopped ? "Sequence test stopped." : "Sequence test finished.");
                    QString logText = "Log: " + logPath;
                    if (!trajPath.isEmpty()) {
                        logText += "\nTrajectory: " + trajPath;
                    }
                    if (!summaryPath.isEmpty()) {
                        logText += "\nSummary: " + summaryPath;
                    }
                    seqLogLabel->setText(logText);
                },
                Qt::QueuedConnection);
        });
    });

    auto recordConsumer = [saveMutex, saveBuffer, &recording, &recordedFrames, &pipelineEnabled, &sequenceRunning,
                                 &processPipelineFrame, &liveLogging, &liveLogMutex, &liveLog, &getStatsSnapshot,
                                 &liveLogStart, &datasetCaptureActive, &datasetBatchPromptPending, &datasetCaptureMutex,
                                 &datasetCaptureSession, &recordDispatcher,
                                 &datasetCaptureDir, &datasetCaptureManifestPath, datasetStartCaptureBtn,
                                 datasetStopCaptureBtn, datasetCaptureStatusLabel, statusLabel, trainerDatasetEdit,
                                 &openDatasetLabelerPath, &datasetIntegritySnapshot, &logDatasetIntegrity,
                                 &collectionActive,
                                 &collectionMutex, &collectionWriter,
                                 &stopDataCollection, collectionStatusLabel,
                                 this](const QImage& img, const FrameMeta& meta, double fps, std::uint64_t, LiveFrameDispatcher::Membership membership) {
        if (membership.recording) {
            QMutexLocker lk(saveMutex.get());
            saveBuffer->push_back(img.copy());
            recordedFrames++;
        }

        if (membership.sequenceRunning)
            return;

        PipelineEvent evt;
        int bgRemaining = 0;
        bool pipelineReady = false;
        double procMs = 0.0;
        bool processed = membership.pipelineEnabled && processPipelineFrame(img, evt, bgRemaining, pipelineReady, &procMs);

        int currentEventId = 0;
        QString lastEventDir;
        int lastDecisionFrame = -1;
        int lastDecisionEventId = 0;
        if (membership.datasetCapture || membership.liveLogging) {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.update(evt, processed);
            currentEventId = liveEventTracker.currentEventId;
            lastEventDir = liveEventTracker.lastEventDir;
            lastDecisionFrame = liveEventTracker.lastDecisionFrame;
            lastDecisionEventId = liveEventTracker.lastDecisionEventId;
        }

        if (membership.collection) {
            std::string writeErr;
            bool writeOk = false;
            std::uint64_t framesSavedNow = 0;
            std::uint64_t rowsLoggedNow = 0;
            {
                QMutexLocker collectionLock(&collectionMutex);
                writeOk = collectionWriter.writeFrame(img, evt, processed, meta.frameIndex, writeErr);
                framesSavedNow = collectionWriter.framesSaved();
                rowsLoggedNow = collectionWriter.rowsLogged();
            }
            if (!writeOk) {
                QMetaObject::invokeMethod(this, [&, writeErr]() {
                    logMessage(QString("Data Collection write failed: %1").arg(QString::fromStdString(writeErr)));
                    stopDataCollection("write_error");
                }, Qt::QueuedConnection);
                throw std::runtime_error(writeErr);
            } else if (framesSavedNow % 25 == 0 || evt.detected) {
                QMetaObject::invokeMethod(collectionStatusLabel, [collectionStatusLabel, framesSavedNow, rowsLoggedNow]() {
                    collectionStatusLabel->setText(QString("Collection: %1 frames, %2 rows")
                                                       .arg(static_cast<qulonglong>(framesSavedNow))
                                                       .arg(static_cast<qulonglong>(rowsLoggedNow)));
                }, Qt::QueuedConnection);
            }
        }

        if (membership.datasetCapture && processed && evt.fired && evt.classified && !evt.cropPath.empty()) {
            bool reachedTarget = false;
            bool scheduleBatchPrompt = false;
            std::size_t collected = 0;
            std::size_t target = 0;
            QString addError;
            {
                QMutexLocker captureLock(&datasetCaptureMutex);
                {
                    DatasetCropCandidate candidate;
                    candidate.sourceType = "live_stream";
                    candidate.sourceSequenceId = "live_camera";
                    candidate.sourceFrameIndex = static_cast<int>(meta.frameIndex);
                    candidate.eventId = currentEventId;
                    candidate.classificationFrame = static_cast<int>(evt.frameNumber);
                    candidate.cropX = evt.cropRect.x;
                    candidate.cropY = evt.cropRect.y;
                    candidate.cropW = evt.cropRect.width;
                    candidate.cropH = evt.cropRect.height;
                    candidate.bboxX = evt.bbox.x;
                    candidate.bboxY = evt.bbox.y;
                    candidate.bboxW = evt.bbox.width;
                    candidate.bboxH = evt.bbox.height;
                    candidate.predictedClassId = evt.label;
                    candidate.predictedLabel = evt.label;
                    candidate.confidence = evt.score;
                    candidate.sourceCropPath = evt.cropPath;
                    std::string err;
                    if (!datasetCaptureSession.addCrop(candidate, err)) {
                        addError = QString::fromStdString(err);
                        datasetCaptureSession.setStopReason("error");
                        datasetCaptureActive.store(false);
                    } else {
                        reachedTarget = datasetCaptureSession.targetReached();
                        collected = datasetCaptureSession.collectedCount();
                        target = datasetCaptureSession.currentBatchTarget();
                        if (reachedTarget && datasetCaptureActive.load() &&
                            !datasetBatchPromptPending.exchange(true)) {
                            datasetCaptureActive.store(false);
                            scheduleBatchPrompt = true;
                        }
                    }
                }
            }
            if (!addError.isEmpty()) {
                const std::uint64_t checkpoint = recordDispatcher ? recordDispatcher->closeDatasetBoundary() : 0;
                QMetaObject::invokeMethod(
                    this,
                    [&, addError, checkpoint]() {
                        if (recordDispatcher)
                            recordDispatcher->waitThrough(checkpoint);
                        std::string finalizeError;
                        {
                            QMutexLocker captureLock(&datasetCaptureMutex);
                            DatasetCaptureIntegrity integrity = datasetIntegritySnapshot();
                            logDatasetIntegrity(integrity);
                            datasetCaptureSession.setIntegrity(std::move(integrity));
                            datasetCaptureSession.finalize(finalizeError);
                        }
                        datasetStartCaptureBtn->setEnabled(true);
                        datasetStopCaptureBtn->setEnabled(false);
                        datasetCaptureStatusLabel->setText("Image Set capture stopped after an error: " + addError);
                        statusLabel->setText("Image Set capture stopped after an error.");
                    },
                    Qt::QueuedConnection);
                throw std::runtime_error(addError.toStdString());
            } else {
                QMetaObject::invokeMethod(
                    this,
                    [&, collected, target]() {
                        datasetCaptureStatusLabel->setText(QString("Image Set capture active: %1 / %2 images\n%3")
                                                               .arg(static_cast<qulonglong>(collected))
                                                               .arg(static_cast<qulonglong>(target))
                                                               .arg(datasetCaptureDir));
                    },
                    Qt::QueuedConnection);
            }
            if (scheduleBatchPrompt) {
                const std::uint64_t checkpoint = recordDispatcher ? recordDispatcher->closeDatasetBoundary() : 0;
                QMetaObject::invokeMethod(
                    this,
                    [&, checkpoint]() {
                        if (recordDispatcher)
                            recordDispatcher->waitThrough(checkpoint);
                        if (!datasetBatchPromptPending.load())
                            return;
                        DatasetCaptureIntegrity integrity = datasetIntegritySnapshot();
                        logDatasetIntegrity(integrity);
                        std::size_t collectedNow = 0;
                        {
                            QMutexLocker captureLock(&datasetCaptureMutex);
                            collectedNow = datasetCaptureSession.collectedCount();
                        }
                        QMessageBox prompt(this);
                        prompt.setWindowTitle("Image Set Batch Target Reached");
                        prompt.setText(
                            QString("The image set collected %1 images. Continue collecting or stop and review?")
                                .arg(static_cast<qulonglong>(collectedNow)));
                        QPushButton* continueButton = prompt.addButton("Continue Collecting", QMessageBox::AcceptRole);
                        QPushButton* reviewButton = prompt.addButton("Stop and Review", QMessageBox::RejectRole);
                        prompt.exec();
                        bool continueCollecting = (prompt.clickedButton() == continueButton);
                        std::size_t nextTarget = 0;
                        {
                            QMutexLocker captureLock(&datasetCaptureMutex);
                            if (continueCollecting) {
                                datasetCaptureSession.extendBatchTarget();
                                nextTarget = datasetCaptureSession.currentBatchTarget();
                            } else {
                                datasetCaptureSession.setIntegrity(std::move(integrity));
                                datasetCaptureSession.recordBatchPrompt("stop_for_review");
                                datasetCaptureSession.setStopReason("user_stop_after_batch_prompt");
                                std::string err;
                                datasetCaptureSession.finalize(err);
                            }
                        }
                        if (continueCollecting) {
                            datasetBatchPromptPending.store(false);
                            if (recordDispatcher)
                                recordDispatcher->resumeDatasetBoundary();
                            datasetCaptureActive.store(true);
                            datasetCaptureStatusLabel->setText(
                                QString("Image Set capture continuing to %1 images\n%2")
                                    .arg(static_cast<qulonglong>(nextTarget))
                                    .arg(datasetCaptureDir));
                        } else {
                            datasetBatchPromptPending.store(false);
                            datasetStartCaptureBtn->setEnabled(true);
                            datasetStopCaptureBtn->setEnabled(false);
                            datasetCaptureStatusLabel->setText(
                                "Image Set capture stopped for review.\nImage set file: " + datasetCaptureManifestPath);
                            trainerDatasetEdit->setText(datasetCaptureManifestPath);
                            if (QFileInfo::exists(datasetCaptureManifestPath)) {
                                openDatasetLabelerPath(datasetCaptureManifestPath);
                            }
                        }
                    },
                    Qt::QueuedConnection);
            }
        }

        if (membership.liveLogging) {
            const bool enabledNow = membership.pipelineEnabled;
            QString skipReason;
            if (!enabledNow) {
                skipReason = "pipeline_disabled";
            } else if (!pipelineReady) {
                skipReason = "pipeline_not_ready";
            } else if (!processed) {
                skipReason = "frame_skipped";
            }

            LiveLogRecord rec;
            rec.wallTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            rec.elapsedMs = liveLogStart.isValid() ? liveLogStart.msecsTo(QDateTime::currentDateTime()) : 0;
            rec.frameIndex = meta.frameIndex;
            rec.delivered = meta.delivered;
            rec.dropped = meta.dropped;
            rec.fps = fps;
            rec.camFps = meta.internalFps;
            rec.procMs = procMs;
            rec.processed = processed;
            rec.pipelineEnabled = enabledNow;
            rec.pipelineReady = pipelineReady;
            rec.skipReason = skipReason;
            rec.bgRemaining = bgRemaining;
            rec.detected = evt.detected;
            rec.fired = evt.fired;
            rec.area = evt.area;
            rec.bboxX = evt.bbox.x;
            rec.bboxY = evt.bbox.y;
            rec.bboxW = evt.bbox.width;
            rec.bboxH = evt.bbox.height;
            rec.cropX = evt.cropRect.x;
            rec.cropY = evt.cropRect.y;
            rec.cropW = evt.cropRect.width;
            rec.cropH = evt.cropRect.height;
            rec.cropPath = QString::fromStdString(evt.cropPath);
            rec.label = QString::fromStdString(evt.label);
            rec.score = evt.score;
            rec.triggered = evt.triggered;
            rec.triggerOk = evt.triggerOk;
            StatsSnapshot snap = getStatsSnapshot();
            rec.eventDir = lastEventDir;
            rec.decisionFrame = lastDecisionFrame;
            rec.decisionEventId = lastDecisionEventId;
            rec.hitCount = snap.wentToHitCount;
            rec.wasteCount = snap.wentToWasteCount;
            rec.classifiedHitCount = snap.classifiedHitCount;
            rec.classifiedWasteCount = snap.classifiedWasteCount;
            rec.wentToHitCount = snap.wentToHitCount;
            rec.wentToWasteCount = snap.wentToWasteCount;
            QMutexLocker lk(&liveLogMutex);
            liveLog.push_back(rec);
        }
    };
    recordDispatcher = std::make_shared<LiveFrameDispatcher>(std::move(recordConsumer));
    cameraWorker->setRecordHook([&recording, &sequenceRunning, &pipelineEnabled, &collectionActive, &datasetCaptureActive, &liveLogging, recordDispatcher, this](const QImage& img, const FrameMeta& meta, double fps) {
        LiveFrameDispatcher::Membership membership{recording.load(), sequenceRunning.load(), pipelineEnabled.load(), collectionActive.load(), datasetCaptureActive.load(), liveLogging.load()};
        const auto result = recordDispatcher->offer(img, meta, fps, membership);
        if (result.delta.sourceGapCount || result.delta.queueRejectedCount) {
            QMetaObject::invokeMethod(this, [this, result]() {
                for (const auto& range : result.delta.sourceGaps) {
                    logMessage(QString("Record source gap: count=%1 range=%2-%3")
                                   .arg(range.last - range.first + 1)
                                   .arg(range.first)
                                   .arg(range.last));
                }
                for (const auto& range : result.delta.queueRejected) {
                    logMessage(QString("Record queue rejection: count=%1 range=%2-%3")
                                   .arg(range.last - range.first + 1)
                                   .arg(range.first)
                                   .arg(range.last));
                }
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(
        cameraWorker, &CameraWorker::frameReady, this,
        [&](const QImage& img, FrameMeta meta, double fps) {
            cameraController->applyFrameToPreviewWorkspaces(img, meta, fps);
        },
        Qt::QueuedConnection);

    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        QMetaObject::invokeMethod(cameraWorker, "stopCapture", Qt::BlockingQueuedConnection);
        if (recordDispatcher)
            recordDispatcher->stopAndDrain();
        backgroundTasks.requestStop();
        sequenceStop.store(true);
        recording.store(false);
        if (collectionActive.load()) {
            stopDataCollection("application_exit");
        }
        const bool datasetWasActive = datasetCaptureActive.exchange(false);
        const bool datasetPromptWasPending = datasetBatchPromptPending.exchange(false);
        if (datasetWasActive || datasetPromptWasPending) {
            DatasetCaptureIntegrity integrity = datasetIntegritySnapshot();
            logDatasetIntegrity(integrity);
            QMutexLocker lock(&datasetCaptureMutex);
            datasetCaptureSession.setIntegrity(std::move(integrity));
            datasetCaptureSession.setStopReason("cancelled");
            std::string err;
            datasetCaptureSession.finalize(err);
        }
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            trainerProcess->terminate();
            if (!trainerProcess->waitForFinished(2500)) {
                trainerProcess->kill();
                trainerProcess->waitForFinished(1000);
            }
        }
        validatorWorkspaceController->stopSequenceTest();
        validatorWorkspaceController->waitForSequenceTest();
        backgroundTasks.waitAll();
        stopLiveLogging();
        cameraController->shutdownCameraThread(cameraThread);
        logMessage("Exiting application");
    });

    cameraWorker->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished, cameraWorker, &QObject::deleteLater);
    cameraThread.start();

    Q_UNUSED(splashTimer);
    app.processEvents();
    QThread::msleep(700);
    app.processEvents();
    this->showMaximized();
    splash.finish(this);
    if (options.verifyDirectDaqManualTrigger || options.verifyLiveViewManualTrigger) {
        logMessage("Manual DAQ verifier: camera startup skipped while preserving hardware-required DAQ checks.");
    } else if (options.verifyLiveViewSortPolicy) {
        logMessage("Live View sort policy verifier: camera startup skipped to keep checks no-hardware.");
    } else if (options.verifyValidationWorkspace) {
        logMessage("Validation workspace verifier: camera startup skipped to keep checks no-hardware.");
    } else if (verifyTrainerLaunch || verifyTrainerSetupStatus) {
        logMessage("Trainer verifier: camera startup skipped to keep checks no-hardware.");
    } else if (verifyDefaultPaths) {
        logMessage("Default paths verifier: camera startup skipped to keep checks no-hardware.");
    } else if (verifyDatasetWorkspace) {
        logMessage("Dataset workspace verifier: camera startup skipped to keep checks no-hardware.");
    } else if (verifyWorkspaceSplitters) {
        logMessage("Workspace splitter verifier: camera startup skipped to keep checks no-hardware.");
    } else if (verifyResetLayout || verifyNavigationInfo || verifyModelsWorkspaceConsolidation ||
               verifyProductionModelStatus) {
        logMessage("Reset layout verifier: camera startup skipped to keep checks no-hardware.");
    } else {
        cameraController->initializeCamera();
    }
    verifierTrace(QStringLiteral("startup: scheduling final verifiers"));
    if (!options.verifyDirectDaqManualTrigger && !options.verifyLiveViewManualTrigger && !options.verifyLiveViewSortPolicy &&
        !options.verifyValidationWorkspace && !verifyTrainerLaunch && !verifyTrainerSetupStatus && !verifyDefaultPaths &&
        !verifyDatasetWorkspace && !verifyWorkspaceSplitters && !verifyResetLayout && !verifyNavigationInfo &&
        !verifyModelsWorkspaceConsolidation && !verifyProductionModelStatus) {
        QTimer::singleShot(0, [&]() { loadPipeline(false, false); });
    }
    if (verifyProductionModelStatus) {
        QTimer::singleShot(0, this, [&, verifierTrace]() {
            QStringList failures;
            const auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    verifierTrace(QStringLiteral("production-model-status: FAIL: ") + message);
                    qCritical().noquote() << "PRODUCTION MODEL STATUS VERIFY FAIL:" << message;
                } else {
                    verifierTrace(QStringLiteral("production-model-status: PASS: ") + message);
                    qInfo().noquote() << "PRODUCTION MODEL STATUS VERIFY PASS:" << message;
                }
            };
            const QString selectedId = liveModelCombo->currentData(kLiveModelIdRole).toString();
            const QString selectedOnnx = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
            const QString selectedMetadata = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
            require(!selectedId.isEmpty(), "active registry entry is selected");
            require(QFileInfo(resolveAppRelative(selectedOnnx)).isFile(), "selected registry ONNX exists");
            require(QFileInfo(resolveAppRelative(selectedMetadata)).isFile(), "selected registry metadata exists");
            loadPipeline(false, true);
            QEventLoop headerRefreshLoop;
            QTimer::singleShot(600, &headerRefreshLoop, &QEventLoop::quit);
            headerRefreshLoop.exec();
            verifierTrace(QStringLiteral("production-model-status: entry=%1 onnx=%2 metadata=%3 status=%4 pipeline=%5")
                              .arg(selectedId, selectedOnnx, selectedMetadata, modelStatusItem->text(),
                                   pipelineStatusLabel->text()));
            require(modelStatusItem->text() == QStringLiteral("Model: loaded"),
                    "visible model status is loaded, not generic Model Error (actual: " +
                        modelStatusItem->text() + ")");
            QString selectedDisplayName;
            for (const QJsonValue& value : registryEntries) {
                const QJsonObject entry = value.toObject();
                if (registryString(entry, "registry_entry_id").compare(selectedId, Qt::CaseInsensitive) == 0) {
                    selectedDisplayName = registryString(entry, "display_name").trimmed();
                    break;
                }
            }
            require(!selectedDisplayName.isEmpty() && headerModelChip->toolTip() == selectedDisplayName &&
                        selectedDisplayName.startsWith(headerModelChip->text().chopped(
                            headerModelChip->text().endsWith(QChar(0x2026)) ? 1 : 0), Qt::CaseInsensitive),
                    "header model badge shows the active package name, with the complete name in its tooltip "
                    "(actual: " + headerModelChip->text() + ", expected: " + selectedDisplayName + ")");
            require(!headerModelChip->text().contains(QStringLiteral("SqueezeNet"), Qt::CaseInsensitive),
                    "header model badge is not the removed hard-coded SqueezeNet label");
            require(!pipelineStatusLabel->text().startsWith("Pipeline error", Qt::CaseInsensitive),
                    "pipeline status has no load error (actual: " + pipelineStatusLabel->text() + ")");
            require(sameCleanPath(resolveAppRelative(onnxEdit->text()), resolveAppRelative(selectedOnnx)),
                    "runtime uses the registry-resolved ONNX in place");
            require(sameCleanPath(resolveAppRelative(metaEdit->text()), resolveAppRelative(selectedMetadata)),
                    "runtime uses the registry-resolved metadata in place");
            qInfo().noquote() << "PRODUCTION MODEL STATUS VERIFY ENTRY:" << selectedId
                              << "status=" << modelStatusItem->text()
                              << "pipeline=" << pipelineStatusLabel->text();
            const int exitCode = failures.isEmpty() ? 0 : 2;
            verifierTrace(QStringLiteral("production-model-status: returning %1").arg(exitCode));
            QCoreApplication::exit(exitCode);
        });
    }
    if (verifyTrainerSetupStatus) {
        const int exitCode = runTrainerSetupStatusVerifier();
        cameraController->shutdownCameraThread(cameraThread);
        logMessage(QString("Trainer setup-status verifier exited with code %1").arg(exitCode));
        return exitCode;
    }
    if (verifyDefaultPaths) {
        QTimer::singleShot(0, [&app, trainerDatasetEdit, trainerOutputEdit, defaultWorkspacePaths, verifierTrace]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    verifierTrace(QStringLiteral("default-paths: FAIL: ") + message);
                    qCritical().noquote() << "DEFAULT PATH VERIFY FAIL:" << message;
                } else {
                    verifierTrace(QStringLiteral("default-paths: PASS: ") + message);
                    qInfo().noquote() << "DEFAULT PATH VERIFY PASS:" << message;
                }
            };
            const QString documentsRoot = QDir::fromNativeSeparators(defaultWorkspacePaths.root).toLower();
            auto isDocumentsPath = [&](const QString& path) {
                return QDir::fromNativeSeparators(QDir::cleanPath(path.trimmed())).toLower().startsWith(documentsRoot);
            };

            const QString trainerDatasetPath = trainerDatasetEdit ? trainerDatasetEdit->text().trimmed() : QString();
            const QString trainerOutputPath = trainerOutputEdit ? trainerOutputEdit->text().trimmed() : QString();
            qInfo().noquote() << "DEFAULT PATH VERIFY VALUES:"
                              << "trainerDataset=" << trainerDatasetPath
                              << "trainerOutput=" << trainerOutputPath
                              << "documentsRoot=" << defaultWorkspacePaths.root;

            require(isDocumentsPath(trainerOutputPath), "Trainer output defaults under Documents/OpenDSS");
            require(!isDeveloperInternalDefaultPath(trainerOutputPath),
                    "Trainer output does not use stale AppData verifier path");
            require(isDocumentsPath(trainerDatasetPath), "Trainer image-set defaults under Documents/OpenDSS");
            require(!isDeveloperInternalDefaultPath(trainerDatasetPath),
                    "Trainer image-set does not use repo source dataset path");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyComputeSettings) {
        QTimer::singleShot(0, [&app, computeDeviceCombo, trainerDeviceCombo,
                               validatorWorkspaceDeviceCombo, trainerTrainArgs]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "COMPUTE SETTINGS VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "COMPUTE SETTINGS VERIFY PASS:" << message;
                }
            };
            auto deviceValueAfter = [](const QStringList& args, const QString& option) {
                const int index = args.indexOf(option);
                return (index >= 0 && index + 1 < args.size()) ? args.at(index + 1) : QString();
            };
            QSettings settings;
            const QString previousDevice = settings.value("settings/computeDevice", "auto").toString();
            auto finish = [&](int exitCode) {
                if (computeDeviceCombo) {
                    const int previousIndex = computeDeviceCombo->findData(previousDevice);
                    if (previousIndex >= 0) {
                        computeDeviceCombo->setCurrentIndex(previousIndex);
                        app.processEvents();
                    }
                }
                QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
            };

            require(computeDeviceCombo != nullptr, "Settings compute selector exists");
            require(computeDeviceCombo && computeDeviceCombo->findData("auto") >= 0, "Compute selector has Auto");
            require(computeDeviceCombo && computeDeviceCombo->findData("cpu") >= 0, "Compute selector has CPU");
            require(computeDeviceCombo && computeDeviceCombo->findData("cuda") >= 0, "Compute selector has GPU");
            require(trainerDeviceCombo != nullptr, "Train compute selector exists");
            require(trainerDeviceCombo && trainerDeviceCombo->findData("auto") >= 0,
                    "Train compute selector has Auto");
            require(trainerDeviceCombo && trainerDeviceCombo->findData("cpu") >= 0,
                    "Train compute selector has CPU");
            require(trainerDeviceCombo && trainerDeviceCombo->findData("cuda") >= 0,
                    "Train compute selector has GPU");

            for (const QString& device : {QStringLiteral("auto"), QStringLiteral("cpu"), QStringLiteral("cuda")}) {
                if (trainerDeviceCombo) {
                    const int trainIndex = trainerDeviceCombo->findData(device);
                    require(trainIndex >= 0, "Train can select " + device + " by stable data value");
                    if (trainIndex >= 0)
                        trainerDeviceCombo->setCurrentIndex(trainIndex);
                    app.processEvents();
                }
                require(computeDeviceCombo && computeDeviceCombo->currentData().toString() == device,
                        "Train selection updates Settings for " + device);
                require(settings.value("settings/computeDevice").toString() == device,
                        "Train selection persists " + device);
                require(deviceValueAfter(trainerTrainArgs(false), "--device") == device,
                        "Training command uses Train-selected " + device);

                if (computeDeviceCombo) {
                    const int settingsIndex = computeDeviceCombo->findData(device);
                    require(settingsIndex >= 0, "Settings can select " + device + " by stable data value");
                    if (settingsIndex >= 0)
                        computeDeviceCombo->setCurrentIndex(settingsIndex);
                    app.processEvents();
                }
                require(trainerDeviceCombo && trainerDeviceCombo->currentData().toString() == device,
                        "Settings selection updates Train for " + device);
            }

            require(settings.value("settings/computeDevice").toString() == "cuda",
                    "Compute selection persists in settings/computeDevice");
            require(settings.value("validator/device").toString() == "cuda",
                    "Compute selection mirrors to validator/device");

            const QStringList trainArgs = trainerTrainArgs(false);
            require(deviceValueAfter(trainArgs, "--device") == "cuda", "Training command uses selected GPU device");
            require(validatorWorkspaceDeviceCombo != nullptr, "Validator workspace device selector exists");
            require(validatorWorkspaceDeviceCombo && validatorWorkspaceDeviceCombo->currentText() == "cuda",
                    "Validation command source uses selected GPU device");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            finish(exitCode);
        });
    }
    if (verifyCollectionMode) {
        QTimer::singleShot(0, [&app, collectionToggleBtn, collectionStatusLabel, pipelineStopBtn]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "COLLECTION MODE VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "COLLECTION MODE VERIFY PASS:" << message;
                }
            };

            require(collectionToggleBtn != nullptr, "Live Data Collection button exists");
            require(collectionToggleBtn && collectionToggleBtn->text() == "Start Data Collection",
                    "Collection button starts with Start Data Collection text");
            require(collectionStatusLabel != nullptr, "Live Data Collection status label exists");
            require(collectionStatusLabel && collectionStatusLabel->text().contains("idle"),
                    "Collection status starts idle");

            app.processEvents();
            const QRect collectionStartGeometry = collectionToggleBtn->geometry();
            const QRect adjacentStartGeometry = pipelineStopBtn->geometry();
            collectionToggleBtn->setText("Stop Data Collection");
            app.processEvents();
            require(collectionToggleBtn->geometry() == collectionStartGeometry,
                    "Collection button geometry stays fixed in active/stop state");
            require(pipelineStopBtn->geometry() == adjacentStartGeometry,
                    "Adjacent control geometry stays fixed in active/stop state");
            collectionToggleBtn->setText("Start Data Collection");
            app.processEvents();
            require(collectionToggleBtn->geometry() == collectionStartGeometry,
                    "Collection button geometry stays fixed after reset");
            require(pipelineStopBtn->geometry() == adjacentStartGeometry,
                    "Adjacent control geometry stays fixed after reset");

            PipelineConfig cfg;
            cfg.detectorOnly = true;
            cfg.saveCrop = false;
            cfg.saveOverlay = false;
            cfg.frameSkip = 0;
            cfg.detect.bgFrames = 1;
            cfg.detect.bgUpdateFrames = 0;
            cfg.detect.minArea = 1.0;
            cfg.detect.maxAreaFrac = 1.0;
            cfg.detect.minBbox = 1;
            cfg.detect.margin = 0;
            cfg.detect.scale = 1.0;
            cfg.daq = DaqConfig{};
            cfg.daq.channel.clear();

            PipelineRunner verifierRunner;
            std::string err;
            require(verifierRunner.init(cfg, err), "Detector-only pipeline initializes without model metadata");
            require(verifierRunner.isReady(), "Detector-only pipeline reports ready");
            require(!verifierRunner.isTriggerReady(), "Detector-only pipeline does not arm DAQ trigger");
            require(verifierRunner.classLabels().empty(), "Detector-only pipeline has no classifier labels");

            cv::Mat black = cv::Mat::zeros(8, 8, CV_8UC1);
            PipelineEvent evt;
            int processedFrames = 0;
            if (verifierRunner.processFrame(black, evt))
                processedFrames++;
            if (verifierRunner.processFrame(black, evt))
                processedFrames++;
            require(processedFrames >= 1, "Detector-only pipeline processes frames after background warmup");
            require(!evt.classified, "Detector-only pipeline does not classify");
            require(!evt.triggered && !evt.triggerOk, "Detector-only pipeline does not trigger DAQ");
            require(evt.cropPath.empty() && evt.overlayPath.empty(), "Detector-only pipeline writes no crops or overlays");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyCollectionPostprocessor) {
        QTimer::singleShot(0, this, [this, &app]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "COLLECTION POSTPROCESS VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "COLLECTION POSTPROCESS VERIFY PASS:" << message;
                }
            };

            const CollectionSaveDialogUi saveUi = buildCollectionSaveDialog(this, "synthetic_collection");
            require(saveUi.dialog && saveUi.dialog->windowTitle() == "Save Dataset As", "Save Dataset As popup title");
            require(saveUi.createMetadataCheck && saveUi.createMetadataCheck->isChecked(),
                    "Create Metadata for Training defaults checked");
            saveUi.dialog->deleteLater();

            QTemporaryDir tempRoot;
            require(tempRoot.isValid(), "temporary verifier root is available");
            const QString collectionsRoot = QDir(tempRoot.path()).filePath("collections");
            const QString preparedRoot = QDir(tempRoot.path()).filePath("datasets/prepared");
            QDir().mkpath(collectionsRoot);
            QDir().mkpath(preparedRoot);

            auto makeSyntheticSession = [&](const QString& folderName, const QString& detectedValue) {
                const QString sessionDir = QDir(collectionsRoot).filePath(folderName);
                QDir().mkpath(QDir(sessionDir).filePath("stream"));
                QImage frame(16, 16, QImage::Format_RGB32);
                frame.fill(QColor("#18222b"));
                for (int y = 4; y < 12; ++y) {
                    for (int x = 3; x < 11; ++x)
                        frame.setPixelColor(x, y, QColor("#f4c542"));
                }
                require(frame.save(QDir(sessionDir).filePath("stream/frame_000001.tiff"), "TIFF"),
                        "synthetic TIFF frame written");

                QFile csv(QDir(sessionDir).filePath("detections.csv"));
                require(csv.open(QIODevice::WriteOnly | QIODevice::Text), "synthetic detections CSV opened");
                QTextStream ts(&csv);
                ts << "image,event_detected,crop_id,timestamp_utc,frame_number,x,y,width,height,centroid_x,centroid_y,"
                      "area,crop_raw_path,crop_64_path\n";
                ts << "\"stream/frame_000001.tiff\"," << detectedValue
                   << ",,\"2026-07-16T00:00:00.000Z\",1,3,4,8,8,7.0,8.0,64.0,,\n";
                ts << "\"stream/frame_000001.tiff\",0,,\"2026-07-16T00:00:00.000Z\",2,,,,,,,,,\n";
                csv.close();

                QJsonObject metadata;
                metadata["session_id"] = folderName;
                metadata["mode"] = "live_data_collection";
                QString metadataWriteError;
                require(desktop_app::writeJsonObjectAtomically(QDir(sessionDir).filePath("collection_metadata.json"),
                                                               metadata, &metadataWriteError),
                        "synthetic collection metadata opened: " + metadataWriteError);
                return sessionDir;
            };

            QProgressDialog progress("Processing saved frames and extracting crops...", QString(), 0, 1, this);
            progress.setWindowTitle("Preparing Dataset");
            progress.setCancelButton(nullptr);
            progress.show();
            int lastProgressValue = 0;
            int lastProgressMax = 1;
            const QString sessionDir = makeSyntheticSession("session_a", "1");
            CollectionPostprocessOptions options;
            options.sessionDir = sessionDir;
            options.collectionName = "prepared_a";
            options.collectionsRoot = collectionsRoot;
            options.preparedDatasetsRoot = preparedRoot;
            options.createTrainingMetadata = true;
            const CollectionPostprocessResult result = postprocessCollectionForTraining(
                options, [&](int value, int maximum, const QString& message) {
                    Q_UNUSED(message);
                    lastProgressValue = value;
                    lastProgressMax = std::max(1, maximum);
                    progress.setMaximum(lastProgressMax);
                    progress.setValue(std::clamp(value, 0, lastProgressMax));
                    app.processEvents();
                });

            require(result.ok, "postprocessor succeeds for synthetic 1/0 detection CSV");
            require(result.rawCropsWritten == 1 && result.resizedCropsWritten == 1,
                    "one detected row produces raw and 64x64 crops");
            require(QFileInfo::exists(QDir(result.collectionDir).filePath("crops_raw/crop_000001.tiff")),
                    "raw crop TIFF exists");
            const QString crop64Path = QDir(result.collectionDir).filePath("crops_64/crop_000001.png");
            require(QFileInfo::exists(crop64Path), "64x64 crop PNG exists");
            require(QImage(crop64Path).size() == QSize(64, 64), "dataset crop is 64x64");
            require(QFileInfo::exists(result.datasetManifestPath), "prepared dataset manifest exists");
            require(QFileInfo::exists(QDir(result.datasetDir).filePath("images/crop_000001.png")),
                    "prepared dataset image copied from crops_64");
            require(lastProgressValue == lastProgressMax, "progress reaches completion");

            QFile updatedCsv(QDir(result.collectionDir).filePath("detections.csv"));
            require(updatedCsv.open(QIODevice::ReadOnly | QIODevice::Text), "updated detections CSV readable");
            const QString updatedCsvText = QString::fromUtf8(updatedCsv.readAll());
            require(updatedCsvText.contains("crop_000001") && updatedCsvText.contains("crops_64/crop_000001.png"),
                    "detections CSV includes crop id and paths");

            const QString collisionSession = makeSyntheticSession("session_b", "true");
            QDir().mkpath(QDir(collectionsRoot).filePath("existing_name"));
            CollectionPostprocessOptions collisionOptions;
            collisionOptions.sessionDir = collisionSession;
            collisionOptions.collectionName = "existing_name";
            collisionOptions.collectionsRoot = collectionsRoot;
            collisionOptions.preparedDatasetsRoot = preparedRoot;
            collisionOptions.createTrainingMetadata = true;
            const CollectionPostprocessResult collision = postprocessCollectionForTraining(collisionOptions);
            require(!collision.ok && collision.errorMessage.contains("already exists"),
                    "existing collection name collision is blocked");

            progress.close();
            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyDatasetHandoff) {
        QTimer::singleShot(0, this, [this, &app, handOffPreparedDatasetForReview, trainerDatasetEdit]() {
            const QString verifierPrefix = QStringLiteral("DATASET HANDOFF VERIFY");
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << verifierPrefix << "FAIL:" << message;
                } else {
                    qInfo().noquote() << verifierPrefix << "PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };

            const QString verifyRoot = QDir(QDir::tempPath()).filePath("ovds_dataset_handoff_verify");
            QDir(verifyRoot).removeRecursively();
            const QString collectionsRoot = QDir(verifyRoot).filePath("collections");
            const QString preparedRoot = QDir(verifyRoot).filePath("prepared");
            QDir().mkpath(collectionsRoot);
            QDir().mkpath(preparedRoot);

            const QString sessionDir = QDir(collectionsRoot).filePath("session_handoff");
            QDir().mkpath(QDir(sessionDir).filePath("stream"));

            QImage frame(20, 20, QImage::Format_RGB32);
            frame.fill(QColor("#111111"));
            for (int y = 4; y < 14; ++y) {
                for (int x = 5; x < 15; ++x) {
                    frame.setPixelColor(x, y, QColor("#f5f5f5"));
                }
            }
            require(frame.save(QDir(sessionDir).filePath("stream/frame_000001.tiff")), "synthetic frame saved");

            QFile csv(QDir(sessionDir).filePath("detections.csv"));
            require(csv.open(QIODevice::WriteOnly | QIODevice::Text), "synthetic detections CSV opened");
            if (csv.isOpen()) {
                QTextStream ts(&csv);
                ts << "image,event_detected,crop_id,timestamp_utc,frame_number,x,y,width,height,centroid_x,centroid_y,"
                      "area,crop_raw_path,crop_64_path\n";
                ts << "\"stream/frame_000001.tiff\",1,,\"2026-07-16T00:00:00.000Z\",1,5,4,10,10,10.0,9.0,100.0,,\n";
                csv.close();
            }

            QJsonObject metadata;
            metadata["session_id"] = "session_handoff";
            metadata["mode"] = "live_data_collection";
            QString metadataWriteError;
            require(desktop_app::writeJsonObjectAtomically(QDir(sessionDir).filePath("collection_metadata.json"),
                                                           metadata, &metadataWriteError),
                    "synthetic collection metadata opened: " + metadataWriteError);

            if (!failures.isEmpty()) {
                QDir(verifyRoot).removeRecursively();
                QCoreApplication::exit(2);
                return;
            }

            CollectionPostprocessOptions handoffOptions;
            handoffOptions.sessionDir = sessionDir;
            handoffOptions.collectionName = "prepared_handoff";
            handoffOptions.collectionsRoot = collectionsRoot;
            handoffOptions.preparedDatasetsRoot = preparedRoot;
            handoffOptions.createTrainingMetadata = true;
            const CollectionPostprocessResult handoffResult = postprocessCollectionForTraining(handoffOptions);
            require(handoffResult.ok, "postprocessor creates a prepared dataset");
            require(QFileInfo(handoffResult.datasetManifestPath).isFile(), "prepared dataset file exists");

            if (handoffResult.ok) {
                handOffPreparedDatasetForReview(handoffResult.datasetManifestPath);
                waitForUi(250);
            }

            QWidget* dialog = nullptr;
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (widget && widget->objectName() == "datasetLabelerWorkspace") {
                    dialog = widget;
                    break;
                }
            }
            require(dialog != nullptr, "dataset review dialog opens after handoff");

            if (dialog) {
                auto* pathLabel = dialog->findChild<QLabel*>("datasetLabelerDatasetPathLabel");
                auto* bannerLabel = dialog->findChild<QLabel*>("datasetLabelerReadinessBanner");
                auto* loadStatusLabel = dialog->findChild<QLabel*>("DatasetBuilderReviewLoadStatusLabel");
                auto* browserTable = dialog->findChild<QTableWidget*>("datasetLabelerCropBrowser");
                require(pathLabel != nullptr, "dataset path label is present");
                require(pathLabel && pathLabel->text().contains("Dataset:", Qt::CaseInsensitive),
                        "dataset path label uses dataset wording");
                require(pathLabel &&
                            pathLabel->text().contains(QDir::toNativeSeparators(handoffResult.datasetDir),
                                                       Qt::CaseInsensitive),
                        "dataset path label points at the prepared dataset folder");
                require(loadStatusLabel && loadStatusLabel->text().contains("loaded", Qt::CaseInsensitive),
                        "dataset review dialog reports loaded status");
                require(browserTable && browserTable->rowCount() == 1, "prepared dataset opens with one review row");
                require(browserTable && browserTable->item(0, 3) &&
                            browserTable->item(0, 3)->text().compare("unreviewed", Qt::CaseInsensitive) == 0,
                        "prepared dataset row starts as unreviewed");
                require(bannerLabel && bannerLabel->text().contains("need review", Qt::CaseInsensitive),
                        "prepared dataset opens as needing review");
                dialog->close();
                waitForUi(50);
            }

            require(QDir::fromNativeSeparators(trainerDatasetEdit->text().trimmed())
                        .endsWith("/metadata/dataset_manifest.json", Qt::CaseInsensitive),
                    "trainer dataset field keeps the prepared dataset file");

            QDir(verifyRoot).removeRecursively();
            QCoreApplication::exit(failures.isEmpty() ? 0 : 2);
        });
    }
    if (verifyWorkspaceSplitters) {
        QTimer::singleShot(0, this, [this, &app, workspaceStack, liveWorkspacePage, datasetWorkspacePage,
                                     trainerWorkspacePage, validatorWorkspacePage, modelWorkspacePage, settingsWorkspacePage,
                                     reportsWorkspacePage]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "WORKSPACE SPLITTER VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "WORKSPACE SPLITTER VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto sizesCloseEnough = [](const QList<int>& left, const QList<int>& right) {
                if (left.size() != right.size())
                    return false;
                for (int i = 0; i < left.size(); ++i) {
                    if (std::abs(left.at(i) - right.at(i)) > 6)
                        return false;
                }
                return true;
            };
            struct SplitterCheck {
                QWidget* page = nullptr;
                const char* objectName = nullptr;
                QString settingsKey;
                QString label;
                int expectedCount = 0;
                QList<int> testSizes;
            };
            const QList<SplitterCheck> checks = {
                {liveWorkspacePage, "MainSplitter", "workspace/live/splitter", "Live workspace splitter", 2, {700, 420}},
                {datasetWorkspacePage, "DatasetWorkspaceSplitter", "workspace/dataset/splitter",
                 "Dataset workspace splitter", 3, {280, 560, 360}},
                {trainerWorkspacePage, "TrainerWorkspaceSplitter", "workspace/trainer/splitter",
                 "Trainer workspace splitter", 2, {760, 340}},
                {validatorWorkspacePage, "ValidatorWorkspaceSplitter", "workspace/validator/splitter",
                 "Validation workspace splitter", 2, {760, 400}},
                {modelWorkspacePage, "ModelWorkspaceSplitter", "workspace/model/splitter", "Model workspace splitter", 2,
                 {360, 860}},
                {settingsWorkspacePage, "SettingsWorkspaceSplitter", "workspace/settings/splitter",
                 "Settings workspace splitter", 2, {620, 420}},
                {reportsWorkspacePage, "ReportsWorkspaceSplitter", "workspace/reports/splitter", "Reports workspace splitter",
                 2, {340, 880}},
            };

            QSettings settings;
            struct SavedSetting {
                QString key;
                QVariant value;
            };
            QList<SavedSetting> savedSettings;
            savedSettings.reserve(checks.size());
            for (const SplitterCheck& check : checks)
                savedSettings.push_back({check.settingsKey, settings.value(check.settingsKey)});

            auto restoreSettings = [&settings, &savedSettings]() {
                for (const SavedSetting& saved : savedSettings) {
                    if (saved.value.isValid())
                        settings.setValue(saved.key, saved.value);
                    else
                        settings.remove(saved.key);
                }
                settings.sync();
            };

            for (const SplitterCheck& check : checks) {
                if (workspaceStack && check.page)
                    workspaceStack->setCurrentWidget(check.page);
                app.processEvents();
                waitForUi(60);

                auto* splitter = this->findChild<QSplitter*>(check.objectName);
                require(splitter != nullptr, check.label + " exists");
                if (!splitter)
                    continue;

                require(splitter->count() == check.expectedCount,
                        QString("%1 exposes %2 adjustable sections").arg(check.label).arg(check.expectedCount));
                require(!splitter->childrenCollapsible(), check.label + " disables collapsing");
                for (int index = 0; index < splitter->count(); ++index) {
                    require(!splitter->isCollapsible(index),
                            QString("%1 section %2 is not collapsible").arg(check.label).arg(index));
                }

                splitter->setSizes(check.testSizes);
                app.processEvents();
                const QList<int> persistedSizes = splitter->sizes();
                require(persistedSizes.size() == check.expectedCount,
                        QString("%1 reports %2 live sizes").arg(check.label).arg(check.expectedCount));
                desktop_app::ui::saveWorkspaceSplitterState(splitter);
                require(!settings.value(check.settingsKey).toByteArray().isEmpty(),
                        check.label + " writes persisted splitter state");

                QList<int> temporarySizes;
                temporarySizes.fill(40, check.expectedCount);
                splitter->setSizes(temporarySizes);
                app.processEvents();
                require(desktop_app::ui::restoreWorkspaceSplitterState(splitter),
                        check.label + " restores the persisted splitter state");
                app.processEvents();
                require(sizesCloseEnough(splitter->sizes(), persistedSizes),
                        check.label + " restores the expected section sizes");
            }

            restoreSettings();
            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyResetLayout) {
        QTimer::singleShot(0, this, [this, &app, resetLayoutAction, workspaceStack, modelWorkspacePage, operationDock]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    qCritical().noquote() << "RESET LAYOUT VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "RESET LAYOUT VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };

            require(resetLayoutAction != nullptr, "View reset action exists");
            require(workspaceStack != nullptr, "workspace stack exists");
            require(modelWorkspacePage != nullptr, "Model workspace exists");
            if (workspaceStack && modelWorkspacePage) {
                workspaceStack->setCurrentWidget(modelWorkspacePage);
            }
            if (operationDock) {
                operationDock->hide();
            }
            app.processEvents();
            waitForUi(50);

            if (resetLayoutAction) {
                resetLayoutAction->trigger();
            }
            app.processEvents();
            waitForUi(50);

            require(!operationDock || !operationDock->isVisible(),
                    "View reset does not show the obsolete Capture dock");
            if (auto* operationalTabs = this->findChild<QTabWidget*>("OperationalTabs")) {
                require(!operationalTabs->isVisible(), "View reset does not show obsolete operational tabs");
            }
            if (auto* captureTab = this->findChild<QWidget*>("OperationalCaptureTab")) {
                require(!captureTab->isVisible(), "View reset does not show obsolete Capture tab controls");
            }
            require(!workspaceStack || workspaceStack->currentWidget() == modelWorkspacePage,
                    "View reset keeps the current workspace selected");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyNavigationInfo) {
        QTimer::singleShot(0, this, [this, &app, aboutAction, showDiagnosticsAction, showLogsAction,
                                    documentationAction, diagnosticsDock, logDock, verifierTrace]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures << message;
                    verifierTrace(QStringLiteral("navigation-info: FAIL: ") + message);
                    qCritical().noquote() << "NAVIGATION INFO VERIFY FAIL:" << message;
                } else {
                    verifierTrace(QStringLiteral("navigation-info: PASS: ") + message);
                    qInfo().noquote() << "NAVIGATION INFO VERIFY PASS:" << message;
                }
            };
            auto* infoButton = this->findChild<QToolButton*>("OpenDssHeaderDiagnosticsButton");
            require(this->findChild<QToolButton*>("OpenDssHeaderMenuButton") == nullptr,
                    "redundant hamburger menu is absent");
            require(this->findChild<QPushButton*>("NavSettingsButton") != nullptr,
                    "Settings is available from the navigation rail");
            require(this->findChild<QPushButton*>("SettingsWorkspaceResetLayoutButton") != nullptr,
                    "Reset Layout is available in Settings");
            require(this->findChild<QToolButton*>("LiveViewerOpenViewerButton") != nullptr,
                    "Open Viewer is available in the Live View toolbar");
            require(infoButton && infoButton->menu(), "information button owns a dropdown menu");
            if (infoButton && infoButton->menu()) {
                const QStringList labels = [&]() {
                    QStringList values;
                    for (auto* action : infoButton->menu()->actions())
                        values << action->text().remove('&');
                    return values;
                }();
                require(labels == QStringList({"About", "Diagnostics", "Debug Log", "Documentation"}),
                        "information menu exposes About, Diagnostics, Debug Log, and Documentation actions");
            }
            showDiagnosticsAction->trigger();
            showLogsAction->trigger();
            app.processEvents();
            require(diagnosticsDock->isVisible(), "Diagnostics action opens its dock");
            require(logDock->isVisible(), "Debug Log action opens its dock");
            require(documentationAction->text().contains("Documentation"), "Documentation action is available");
            aboutAction->trigger();
            app.processEvents();
            auto* aboutDialog = this->findChild<QDialog*>("OpenDssAboutDialog");
            auto* aboutDetails = aboutDialog ? aboutDialog->findChild<QLabel*>("OpenDssAboutDetails") : nullptr;
            require(aboutDetails && aboutDetails->text().contains("0.9.0"), "About shows software version 0.9.0");
            require(aboutDetails && aboutDetails->text().contains("OpenDSS_clean"), "About shows clickable repository links");
            require(aboutDetails && aboutDetails->text().contains("haeminjung@tamu.edu"), "About shows support email");
            require(aboutDetails && (aboutDetails->textInteractionFlags() & Qt::TextSelectableByMouse),
                    "About information is selectable and copyable");
            if (aboutDialog)
                aboutDialog->close();
            const int exitCode = failures.isEmpty() ? 0 : 2;
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (verifyDatasetWorkspace) {
        auto* datasetVerifierExitTimer = new QTimer(this);
        datasetVerifierExitTimer->setInterval(50);
        QObject::connect(datasetVerifierExitTimer, &QTimer::timeout, this, [&app, datasetVerifierExitTimer]() {
            const QVariant exitCode = qApp->property("ovdsDatasetWorkspaceVerifyExitCode");
            if (!exitCode.isValid())
                return;
            datasetVerifierExitTimer->stop();
            datasetVerifierExitTimer->deleteLater();
            QTimer::singleShot(0, &app, [code = exitCode.toInt()]() { QCoreApplication::exit(code); });
        });
        datasetVerifierExitTimer->start();
    }
    if (!options.datasetBuilderReviewPath.trimmed().isEmpty()) {
        QTimer::singleShot(0, [&]() {
            logMessage("Opening Image Set review file from command line: " +
                       options.datasetBuilderReviewPath);
            openDatasetLabelerPath(options.datasetBuilderReviewPath);
        });
    }
    if (options.verifyValidationWorkspace) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "VALIDATION VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "VALIDATION VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto workspaceContainsText = [](QWidget* root, const QString& text) {
                if (!root)
                    return false;
                for (auto* label : root->findChildren<QLabel*>()) {
                    if (label->text().contains(text, Qt::CaseInsensitive) && label->isVisible())
                        return true;
                }
                return false;
            };
            auto legacyDialogVisible = [this]() {
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    auto* dialog = qobject_cast<QDialog*>(widget);
                    if (dialog && dialog->isVisible() &&
                        dialog->windowTitle().contains("Image Validation", Qt::CaseInsensitive)) {
                        return true;
                    }
                }
                return false;
            };

            workspaceStack->setCurrentWidget(modelWorkspacePage);
            if (modelNavButton)
                modelNavButton->setChecked(true);
            headerTitleLabel->setText("/ Model");
            headerStatusText->setText("Model workspace");
            app.processEvents();
            waitForUi(250);

            auto* modelValidateButton = this->findChild<QPushButton*>("ModelWorkspaceValidateButton");
            auto* modelAddPretrainedButton = this->findChild<QPushButton*>("ModelWorkspaceAddPretrainedModelButton");
            auto* revalidateButton = this->findChild<QPushButton*>("ModelWorkspaceRevalidateButton");
            auto* openReportButton = this->findChild<QPushButton*>("ModelWorkspaceOpenReportButton");
            auto* runValidationButton = this->findChild<QPushButton*>("ModelWorkspaceRunValidationButton");
            auto* validatorWorkspace = this->findChild<QWidget*>("ValidatorWorkspace");
            auto* validatorRunButton = this->findChild<QPushButton*>("ValidatorWorkspaceOpenImageValidationButton");
            auto* validatorModelEdit = this->findChild<QLineEdit*>("ValidatorWorkspaceModelEdit");
            auto* validatorModelCombo = this->findChild<QComboBox*>("ValidatorWorkspaceModelCombo");
            auto* validatorModelBrowseButton =
                this->findChild<QPushButton*>("ValidatorWorkspaceModelEditBrowseButton");
            auto* validatorDatasetEdit = this->findChild<QLineEdit*>("ValidatorWorkspaceDatasetEdit");
            auto* validatorDatasetBrowseButton =
                this->findChild<QPushButton*>("ValidatorWorkspaceDatasetEditBrowseButton");
            auto* validatorStatusLabel = this->findChild<QLabel*>("ValidatorWorkspaceStatusLabel");
            auto* commandPreview = this->findChild<QPlainTextEdit*>("ValidatorWorkspaceCommandPreview");
            auto* pythonEdit = this->findChild<QLineEdit*>("ValidatorWorkspacePythonExecutableEdit");
            auto* metadataEdit = this->findChild<QLineEdit*>("ValidatorWorkspaceMetadataEdit");
            auto* schemaCombo = this->findChild<QComboBox*>("ValidatorWorkspaceClassSchemaComboBox");
            auto* classesEdit = this->findChild<QLineEdit*>("ValidatorWorkspaceClassesEdit");
            auto* validationMenu = this->findChild<QMenu*>("ValidationMenu");

            require(modelValidateButton == nullptr, "Model workspace does not show a top-level testing button");
            require(revalidateButton == nullptr, "Model workspace does not show a re-validate button");
            require(openReportButton == nullptr, "Model workspace does not show a validation report button");
            require(runValidationButton == nullptr, "Model workspace does not show a run-validation button");
            require(validatorWorkspace != nullptr, "ValidatorWorkspace exists");
            imageValidationAction->trigger();
            waitForUi(250);

            require(workspaceStack->currentWidget() == validatorWorkspace,
                    "Model Testing action focuses ValidatorWorkspace from the Model workspace context");
            require(!legacyDialogVisible(), "Model workspace route does not open the legacy dialog");
            require(validationMenu && validationMenu->title().contains("Model Testing", Qt::CaseInsensitive),
                    "Menu bar shows Model Testing wording");
            require(imageValidationAction && imageValidationAction->text() == "Test Model",
                    "Model Testing menu action uses Test Model wording");
            require(validatorNavButton &&
                        (validatorNavButton->toolTip() == "Model Testing" ||
                         validatorNavButton->accessibleName() == "Model Testing"),
                    "Navigation rail shows Model Testing wording");
            require(headerTitleLabel && headerTitleLabel->text() == "/ Model Testing",
                    "Workspace header shows Model Testing");
            require(validatorRunButton != nullptr, "Validator workspace run button exists");
            require(validatorRunButton && validatorRunButton->text() == "Test Model",
                    "Validator workspace uses Test Model as the primary action");
            int expectedTestingModelCount = 0;
            QStringList expectedTestingNames;
            QString expectedActiveTestingId;
            for (const QJsonValue& value : registryEntries) {
                const QJsonObject entry = value.toObject();
                const ModelPackageInspection package = inspectModelPackage(entry);
                if (!package.canActivate)
                    continue;
                ++expectedTestingModelCount;
                expectedTestingNames << registryString(entry, "display_name");
                if (entry.value("active").toBool(false))
                    expectedActiveTestingId = registryString(entry, "registry_entry_id");
            }
            QStringList actualTestingNames;
            if (validatorModelCombo) {
                for (int index = 0; index < validatorModelCombo->count(); ++index)
                    actualTestingNames << validatorModelCombo->itemText(index);
            }
            require(validatorModelCombo && validatorModelCombo->count() == expectedTestingModelCount &&
                        actualTestingNames == expectedTestingNames,
                    "Model Testing lists every eligible inference package from Library in registry order");
            require(!validatorModelCombo || expectedActiveTestingId.isEmpty() ||
                        validatorModelCombo->currentData().toString() == expectedActiveTestingId,
                    "Model Testing selects the active Library model");
            require(validatorModelBrowseButton == nullptr,
                    "Model Testing does not expose a raw model file selector");
            if (qEnvironmentVariableIntValue("OVDS_VERIFY_MODEL_TESTING_REGISTRY_REFRESH") != 0) {
                require(!qEnvironmentVariable("OVDS_MODEL_REGISTRY_PATH").trimmed().isEmpty(),
                        "Model Testing registry-refresh verifier uses an isolated registry override");
                const int initialTestingModelCount = validatorModelCombo ? validatorModelCombo->count() : -1;
                require(modelAddPretrainedButton != nullptr, "Model workspace Add pre-trained model button exists");
                if (modelAddPretrainedButton)
                    modelAddPretrainedButton->click();
                app.processEvents();
                require(validatorModelCombo && validatorModelCombo->count() == initialTestingModelCount + 1,
                        "Adding a Model workspace entry refreshes Model Testing");
                if (validatorModelCombo && validatorModelCombo->count() > 0) {
                    validatorModelCombo->setCurrentIndex(validatorModelCombo->count() - 1);
                    const QVariantMap selected =
                        validatorModelCombo->currentData(Qt::UserRole + 1).toMap();
                    require(QFileInfo(selected.value("model_path").toString()).isFile(),
                            "Refreshed Model Testing entry resolves its ONNX path");
                    require(QFileInfo(selected.value("metadata_path").toString()).isFile(),
                            "Refreshed Model Testing entry resolves its metadata path");
                }
            }
            require(workspaceContainsText(validatorWorkspace, "Test dataset"),
                    "Validator workspace shows Test dataset wording");
            require(workspaceContainsText(validatorWorkspace, "Test results"),
                    "Validator workspace shows Test results wording");
            require(workspaceContainsText(validatorWorkspace, "Model testing summary"),
                    "Validator workspace summary uses Model testing wording");
            require(!workspaceContainsText(validatorWorkspace, "Training images"),
                    "Validator workspace no longer shows Training images wording");
            require(validatorModelEdit && !validatorModelEdit->text().trimmed().isEmpty(),
                    "Selected model is carried into the Validator workspace");
            const QString validatorDatasetPath =
                validatorDatasetEdit ? validatorDatasetEdit->text().trimmed() : QString();
            const QString normalizedValidatorDatasetPath =
                QDir::fromNativeSeparators(QDir::cleanPath(validatorDatasetPath)).toLower();
            const QString normalizedPreparedDatasetsPath =
                QDir::fromNativeSeparators(QDir::cleanPath(defaultWorkspacePaths.preparedDatasets)).toLower();
            require(validatorDatasetEdit != nullptr, "Validator workspace Test dataset field exists");
            require(validatorDatasetBrowseButton != nullptr,
                    "Validator workspace Test dataset Browse button is named for direct Qt verification");
            require(validatorDatasetBrowseButton &&
                        validatorDatasetBrowseButton->property("pathSelectionMode").toString() == "file",
                    "Validator workspace Test dataset Browse uses a file selector");
            require(validatorDatasetBrowseButton &&
                        validatorDatasetBrowseButton->property("fileDialogFilter").toString().contains("*.json"),
                    "Validator workspace Test dataset Browse filters for JSON manifests");
            require(validatorDatasetBrowseButton &&
                        QDir::cleanPath(validatorDatasetBrowseButton->property("workspacePath").toString()) ==
                            QDir::cleanPath(defaultWorkspacePaths.preparedDatasets),
                    "Validator workspace Test Browse opens exactly at Documents/OpenDSS/datasets/prepared");
            require(normalizedValidatorDatasetPath.endsWith("/metadata/dataset_manifest.json"),
                    "Validator workspace Test dataset defaults to dataset_manifest.json");
            require(normalizedValidatorDatasetPath.startsWith(normalizedPreparedDatasetsPath),
                    "Validator workspace Test dataset default starts in Documents/OpenDSS prepared datasets");
            require(validatorDatasetEdit && !QFileInfo(validatorDatasetPath).isDir(),
                    "Validator workspace Test dataset field does not prefer a folder path");
            if (validatorDatasetEdit && QFileInfo(defaultWorkspacePaths.preparedDataset).isDir()) {
                const QString manifestPath = validatorDatasetEdit->text();
                validatorDatasetEdit->setText(defaultWorkspacePaths.preparedDataset);
                waitForUi(100);
                require(validatorStatusLabel &&
                            validatorStatusLabel->text().contains("training images JSON manifest", Qt::CaseInsensitive),
                        "Folder-only Test dataset selection is blocked as a missing JSON manifest");
                if (validatorRunButton) {
                    require(!validatorRunButton->isEnabled(),
                            "Folder-only Test dataset selection does not enable model testing");
                }
                validatorDatasetEdit->setText(manifestPath);
                waitForUi(100);
            }
            require(commandPreview && !commandPreview->isVisible(), "Command preview is hidden in the Validator workspace");
            require(pythonEdit && !pythonEdit->isVisible(), "Python path is hidden in the Validator workspace");
            require(metadataEdit && !metadataEdit->isVisible(), "Metadata path is hidden in the Validator workspace");
            require(schemaCombo && !schemaCombo->isVisible(), "Class schema selector is hidden in the Validator workspace");
            require(classesEdit && !classesEdit->isVisible(), "Custom classes input is hidden in the Validator workspace");
            require(!workspaceContainsText(validatorWorkspace, "External Python validator"),
                    "Validator workspace no longer shows external Python jargon");
            require(!workspaceContainsText(validatorWorkspace, "ROC AUC"),
                    "Validator workspace no longer shows ROC AUC by default");
            require(!workspaceContainsText(validatorWorkspace, "Latency P99"),
                    "Validator workspace no longer shows latency by default");
            require(!workspaceContainsText(validatorWorkspace, "Sequence validation remains disabled"),
                    "Validator workspace no longer shows sequence validation placeholder notes");
            verifyValidationWritebackToModelRegistry(require);

            workspaceStack->setCurrentWidget(liveWorkspacePage);
            if (liveNavButton)
                liveNavButton->setChecked(true);
            headerTitleLabel->setText("/ Live View");
            headerStatusText->setText("Live View workspace");
            app.processEvents();
            waitForUi(150);

            imageValidationAction->trigger();
            waitForUi(150);
            require(workspaceStack->currentWidget() == validatorWorkspace,
                    "Model Testing menu action focuses ValidatorWorkspace");
            require(!legacyDialogVisible(), "Model Testing menu action does not open the legacy dialog");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (failures.isEmpty()) {
                logMessage("Model Testing workspace verifier passed.");
            } else {
                logMessage("Model Testing workspace verifier failed: " + failures.join("; "));
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyLiveViewSortPolicy) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "LIVE VIEW SORT VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "LIVE VIEW SORT VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };

            workspaceStack->setCurrentWidget(liveWorkspacePage);
            if (liveNavButton)
                liveNavButton->setChecked(true);
            headerTitleLabel->setText("/ Live View");
            headerStatusText->setText("Live View workspace");
            app.processEvents();
            waitForUi(150);

            auto* policyCheck = this->findChild<QCheckBox*>("PipelineSortNonTargetCheckBox");
            auto* cropsCheck = this->findChild<QCheckBox*>("PipelineSaveCropsCheckBox");
            auto* overlaysCheck = this->findChild<QCheckBox*>("PipelineSaveOverlaysCheckBox");
            auto* policyTargetCombo = this->findChild<QComboBox*>("PipelineTargetClassComboBox");
            require(policyCheck != nullptr, "PipelineSortNonTargetCheckBox exists");
            require(policyCheck && policyCheck->text() == "Sort Non-target", "Sort Non-target checkbox label is correct");
            require(cropsCheck != nullptr, "PipelineSaveCropsCheckBox exists");
            require(overlaysCheck != nullptr, "PipelineSaveOverlaysCheckBox exists");
            require(policyTargetCombo != nullptr, "PipelineTargetClassComboBox exists");

            const QString sortPolicyKey = QStringLiteral("runtime/v1/sorting/sortNonTarget");
            const bool originalChecked = policyCheck ? policyCheck->isChecked() : false;
            if (policyCheck) {
                policyCheck->setChecked(false);
                app.processEvents();
                require(!runtimeSettings.value(sortPolicyKey, true).toBool(), "Unchecked Sort Non-target persists false");

                policyCheck->setChecked(true);
                app.processEvents();
                require(runtimeSettings.value(sortPolicyKey, false).toBool(), "Checked Sort Non-target persists true");

                policyCheck->setChecked(originalChecked);
                app.processEvents();
            }

            require(liveSortShouldTrigger("1", "1", false), "Target mode triggers the configured target class");
            require(!liveSortShouldTrigger("0", "1", false), "Target mode does not trigger class 0 when target is 1");
            require(!liveSortShouldTrigger("1", "1", true), "Non-target mode excludes the configured target class");
            require(liveSortShouldTrigger("0", "1", true), "Non-target mode triggers class 0 when target is 1");
            require(liveSortShouldTrigger("2", "1", true), "Non-target mode triggers class 2 when target is 1");
            require(!liveSortShouldTrigger("", "1", true), "Empty predicted class never triggers");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (failures.isEmpty()) {
                logMessage("Live View sort policy verifier passed.");
            } else {
                logMessage("Live View sort policy verifier failed: " + failures.join("; "));
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyCameraWorkspace) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto waitUntil = [&](int timeoutMs, const std::function<bool()>& predicate) {
                QElapsedTimer timer;
                timer.start();
                while (timer.elapsed() < timeoutMs) {
                    app.processEvents();
                    if (predicate()) {
                        return true;
                    }
                    waitForUi(100);
                }
                app.processEvents();
                return predicate();
            };

            workspaceStack->setCurrentWidget(liveWorkspacePage); 
            liveNavButton->setChecked(true); 
            headerTitleLabel->setText("/ Live View"); 
            headerStatusText->setText("Live View workspace"); 
            const int verifierRightWidth = qMax(430, rightScroll ? rightScroll->minimumWidth() : 430);
            rightScroll->setMinimumWidth(verifierRightWidth); 
            rightScroll->setMaximumWidth(verifierRightWidth); 
            mainSplitter->setSizes({qMax(760, this->width() - verifierRightWidth), verifierRightWidth}); 
            app.processEvents(); 

            require(workspaceStack->currentWidget() == liveWorkspacePage,
                    "Live View workspace opens as the active page");
            require(liveNavButton && liveNavButton->toolTip() == "Live View",
                    "Live navigation tooltip reads Live View");
            require(headerTitleLabel && headerTitleLabel->text() == "/ Live View", "Header title reads / Live View");
            require(headerStatusText && headerStatusText->text() == "Live View workspace",
                    "Header status reads Live View workspace");
            require(!liveNavButton->text().contains("Live Sorting"),
                    "Live navigation label no longer shows Live Sorting");
            require(!headerTitleLabel->text().contains("Live Sorting"), "Header title no longer shows Live Sorting");
            require(!headerStatusText->text().contains("Live Sorting"), "Header status no longer shows Live Sorting");

            auto* cameraWorkspace = this->findChild<QWidget*>("CameraWorkspace");
            auto* cameraControlsStack = this->findChild<QWidget*>("CameraControlsStack");
            auto* cameraFormatPanel = this->findChild<QWidget*>("CameraFormatSpeedPanel");
            auto* cameraFormatPanelFrame = this->findChild<QWidget*>("CameraFormatSpeedPanelFrame");
            auto* cameraLutPanel = this->findChild<QWidget*>("CameraLutDisplayPanel");
            auto* cameraLutPanelFrame = this->findChild<QWidget*>("CameraLutDisplayPanelFrame");
            auto* cameraRecordingPanel = this->findChild<QWidget*>("CameraRecordingPanel");
            auto* cameraRecordingPanelFrame = this->findChild<QWidget*>("CameraRecordingPanelFrame");
            auto* cameraSequencePanel = this->findChild<QWidget*>("CameraSequenceTestPanel");
            auto* cameraSequencePanelFrame = this->findChild<QWidget*>("CameraSequenceTestPanelFrame");
            auto* sequenceTestWidget = this->findChild<QWidget*>("SequenceTestTab");
            auto* operationalSequenceTab = this->findChild<QWidget*>("OperationalSequenceTab");
            auto* cameraAdvancedPanel = this->findChild<QWidget*>("CameraAdvancedFrameStatsPanel");
            auto* cameraLutRangeBar = this->findChild<QWidget*>("CameraLutRangeBar");
            auto* cameraNavButton = this->findChild<QPushButton*>("NavCameraButton");
            auto* liveHardwarePanel = this->findChild<QWidget*>("LiveHardwarePanel");
            auto* liveClassDistributionPanel = this->findChild<QWidget*>("LiveClassDistributionPanel");
            auto* cameraIndependentBinningCheck = this->findChild<QCheckBox*>("CameraIndependentBinningCheckBox");
            auto* cameraBinHSpin = this->findChild<QSpinBox*>("CameraBinHSpinBox");
            auto* cameraBinVSpin = this->findChild<QSpinBox*>("CameraBinVSpinBox");
            auto* cameraLutMinSlider = this->findChild<QSlider*>("CameraLutMinSlider");
            auto* cameraLutMaxSlider = this->findChild<QSlider*>("CameraLutMaxSlider");
            auto* cameraPresetCombo = this->findChild<QComboBox*>("CameraPresetComboBox");
            auto* cameraBitsCombo = this->findChild<QComboBox*>("CameraBitsComboBox");
            auto* cameraWidthSpin = this->findChild<QSpinBox*>("CameraCustomWidthSpinBox");
            auto* cameraHeightSpin = this->findChild<QSpinBox*>("CameraCustomHeightSpinBox");
            auto* cameraExposureSpin = this->findChild<QDoubleSpinBox*>("CameraExposureSpinBox");
            auto* cameraAutoExposureButton = this->findChild<QPushButton*>("CameraAutoExposureButton");
            auto* cameraReadoutCombo = this->findChild<QComboBox*>("CameraReadoutSpeedComboBox");
            auto* cameraBinningCombo = this->findChild<QComboBox*>("CameraBinningComboBox");
            auto* cameraLutModeControl = this->findChild<QWidget*>("CameraLutModeSegmentedControl");
            auto* cameraLutMinSpin = this->findChild<QSpinBox*>("CameraLutMinSpinBox");
            auto* cameraLutMaxSpin = this->findChild<QSpinBox*>("CameraLutMaxSpinBox");
            auto* cameraLutAutoSetButton = this->findChild<QPushButton*>("CameraLutAutoSetButton");
            auto* cameraDisplayEverySpin = this->findChild<QSpinBox*>("CameraDisplayEverySpinBox");
            auto* cameraStartButton = this->findChild<QPushButton*>("CameraStartButton");
            auto* cameraReconnectButton = this->findChild<QPushButton*>("CameraReconnectButton");
            auto* cameraApplyButton = this->findChild<QPushButton*>("CameraApplySettingsButton");
            auto* cameraStopButton = this->findChild<QPushButton*>("CameraStopButton");
            if (!cameraReconnectButton) {
                cameraReconnectButton = reconnectBtn;
            }
            if (!cameraApplyButton) {
                cameraApplyButton = applyBtn;
            }
            auto* pipelineStartButton = this->findChild<QPushButton*>("PipelineStartButton");
            auto* pipelineStopButton = this->findChild<QPushButton*>("PipelineStopButton");
            auto* savePathLineEdit = this->findChild<QLineEdit*>("SavePathEdit");
            auto* saveBrowseButton = this->findChild<QPushButton*>("SaveBrowseButton");
            auto* saveOpenFolderButton = this->findChild<QPushButton*>("SaveOpenFolderButton");
            auto* cameraRecordingFormatControl = this->findChild<QWidget*>("CameraRecordingFormatSegmentedControl");
            auto* saveStartButton = this->findChild<QPushButton*>("SaveStartButton");
            auto* saveStopButton = this->findChild<QPushButton*>("SaveStopButton");
            auto* sequenceFolderEdit = this->findChild<QLineEdit*>("SequenceFolderEdit");
            auto* sequenceBrowseButton = this->findChild<QPushButton*>("SequenceBrowseButton");
            auto* sequenceLoadButton = this->findChild<QPushButton*>("SequenceLoadButton");
            auto* sequenceStartButton = this->findChild<QPushButton*>("SequenceStartTestButton");
            auto* sequenceStopButton = this->findChild<QPushButton*>("SequenceStopButton");
            auto* sequenceFpsSpin = this->findChild<QDoubleSpinBox*>("SequenceFpsSpinBox");
            auto* sequenceStatusLabel = this->findChild<QLabel*>("SequenceStatusLabel");
            auto* sequenceLogLabel = this->findChild<QLabel*>("SequenceLogLabel");
            auto* liveRunEventsMetricLabel = this->findChild<QLabel*>("LiveRunEventsMetricLabel");
            auto* liveRunClassifiedHitMetricLabel = this->findChild<QLabel*>("LiveRunClassifiedHitMetricLabel");
            auto* liveRunClassifiedWasteMetricLabel =
                this->findChild<QLabel*>("LiveRunClassifiedWasteMetricLabel");
            auto* liveRunWentToHitMetricLabel = this->findChild<QLabel*>("LiveRunWentToHitMetricLabel");
            auto* liveRunWentToWasteMetricLabel = this->findChild<QLabel*>("LiveRunWentToWasteMetricLabel");
            auto* liveLastDecisionValueLabel = this->findChild<QLabel*>("LiveLastDecisionValueLabel");
            auto* statsClassTextLabel = this->findChild<QLabel*>("StatsClassCountsLabel");
            auto* statsLastEventLabel = this->findChild<QLabel*>("StatsLastEventLabel");
            auto* navRailFrame = this->findChild<QFrame*>("OpenDssNavigationRail");
            auto* headerFrame = this->findChild<QFrame*>("OpenDssHeader");
            auto* statusStripFrame = this->findChild<QFrame*>("OpenDssStatusStrip");
            auto* displayOverlayAction = this->findChild<QAction*>("DisplayOverlayAction");
            auto* displayClearOverlayAction = this->findChild<QAction*>("DisplayClearOverlayAction");
            auto* analysisOverlayCheck = this->findChild<QCheckBox*>("AnalysisOverlayCheckBox");
            auto* liveViewerOverlayToggle = this->findChild<QToolButton*>("LiveViewerOverlayToggle");
            auto* liveViewerDetectionOverlay = this->findChild<QWidget*>("LiveViewerDetectionOverlay");
            auto* liveDetectorSettingsButton = this->findChild<QPushButton*>("LiveDetectorTuningButton");
            auto* liveDetectorSettingsDrawer = this->findChild<QFrame*>("LiveDetectorTuningDrawer");
            auto* liveDetectorMinRectangleSpin = this->findChild<QSpinBox*>("LiveDetectorMinRectangleSizeSpinBox");
            auto* rightViewport = rightScroll ? rightScroll->viewport() : nullptr;

            auto requireContained = [&](QWidget* child, QWidget* parent, const QString& message) {
                if (!child || !parent) {
                    require(false, message + " (missing widget)");
                    return;
                }
                const QRect childRect(child->mapTo(parent, QPoint(0, 0)), child->size());
                const QRect bounds = parent->contentsRect();
                require(bounds.contains(childRect), message);
            };
            auto requireHorizontallyContained = [&](QWidget* child, QWidget* parent, const QString& message) {
                if (!child || !parent) {
                    require(false, message + " (missing widget)");
                    return;
                }
                const QRect childRect(child->mapTo(parent, QPoint(0, 0)), child->size());
                const QRect bounds = parent->contentsRect();
                require(childRect.left() >= bounds.left() && childRect.right() <= bounds.right(), message);
            };
            auto hasLabelText = [](QWidget* root, const QString& text) {
                if (!root)
                    return false;
                for (auto* label : root->findChildren<QLabel*>()) {
                    if (label->text() == text)
                        return true;
                }
                return false;
            };

            require(cameraNavButton == nullptr, "NavCameraButton is absent");
            require(cameraWorkspace == nullptr, "CameraWorkspace is absent");
            require(workspaceStack->currentWidget() == liveWorkspacePage, "LiveWorkspace is selected");
            require(rightViewport != nullptr, "LiveRightMetricsScrollArea viewport exists");
            require(cameraControlsStack != nullptr, "CameraControlsStack exists");
            require(cameraFormatPanel != nullptr, "CameraFormatSpeedPanel exists");
            require(cameraFormatPanel && cameraFormatPanel->isVisibleTo(this), "CameraFormatSpeedPanel is visible");
            require(cameraFormatPanelFrame != nullptr, "CameraFormatSpeedPanelFrame exists");
            require(cameraLutPanel == nullptr, "CameraLutDisplayPanel is absent");
            require(cameraLutPanelFrame == nullptr, "CameraLutDisplayPanelFrame is absent");
            require(cameraRecordingPanel != nullptr, "CameraRecordingPanel exists");
            require(cameraRecordingPanel && cameraRecordingPanel->isVisibleTo(this), "CameraRecordingPanel is visible");
            require(cameraRecordingPanelFrame != nullptr, "CameraRecordingPanelFrame exists");
            require(cameraSequencePanel != nullptr, "CameraSequenceTestPanel exists");
            require(cameraSequencePanel && cameraSequencePanel->isVisibleTo(this), "CameraSequenceTestPanel is visible");
            require(cameraSequencePanelFrame != nullptr, "CameraSequenceTestPanelFrame exists");
            require(sequenceTestWidget && cameraSequencePanelFrame && cameraSequencePanelFrame->isAncestorOf(sequenceTestWidget),
                    "SequenceTestTab controls are parented inside the Live Sequence Test section");
            require(operationalSequenceTab == nullptr, "Operational Sequence tab is absent after moving controls to Live");
            require(cameraLutRangeBar != nullptr, "CameraLutRangeBar exists");
            require(cameraLutRangeBar && cameraLutRangeBar->isVisibleTo(this), "CameraLutRangeBar is visible");
            require(cameraAdvancedPanel == nullptr, "CameraAdvancedFrameStatsPanel is absent");
            require(liveHardwarePanel == nullptr, "LiveHardwarePanel is absent");
            require(liveClassDistributionPanel == nullptr, "LiveClassDistributionPanel is absent");
            require(cameraIndependentBinningCheck == nullptr, "CameraIndependentBinningCheckBox is absent");
            require(cameraBinHSpin == nullptr, "CameraBinHSpinBox is absent");
            require(cameraBinVSpin == nullptr, "CameraBinVSpinBox is absent");
            require(cameraLutMinSlider == nullptr, "CameraLutMinSlider is absent from the visible workspace tree");
            require(cameraLutMaxSlider == nullptr, "CameraLutMaxSlider is absent from the visible workspace tree");
            require(cameraLutModeControl == nullptr, "CameraLutModeSegmentedControl is absent");
            require(cameraDisplayEverySpin == nullptr, "CameraDisplayEverySpinBox is absent from the visible workspace tree");
            require(cameraStopButton == nullptr, "CameraStopButton is absent");
            require(cameraStartButton && cameraStartButton->text() == "Start Camera",
                    "Camera action starts as Start Camera");
            require(cameraStartButton && cameraStartButton->toolTip() == "Start camera acquisition.",
                    "Camera action tooltip starts as Start camera acquisition.");
            require(cameraReconnectButton && cameraReconnectButton->text().contains("Reconnect", Qt::CaseInsensitive),
                    QString("Camera reconnect button keeps expected visible wording (text=%1)")
                        .arg(cameraReconnectButton ? cameraReconnectButton->text() : QStringLiteral("<missing>")));
            require(cameraApplyButton && cameraApplyButton->text().contains("Apply", Qt::CaseInsensitive) &&
                        cameraApplyButton->text().contains("Settings", Qt::CaseInsensitive),
                    QString("Camera apply button keeps expected visible wording (text=%1)")
                        .arg(cameraApplyButton ? cameraApplyButton->text() : QStringLiteral("<missing>")));
            require(cameraAutoExposureButton && cameraAutoExposureButton->text() == "Auto Exposure",
                    "CameraAutoExposureButton exists with expected text");
            require(cameraAutoExposureButton &&
                        cameraAutoExposureButton->toolTip().contains("current live frame", Qt::CaseInsensitive),
                    "CameraAutoExposureButton tooltip explains current-frame behavior");
            require(cameraLutAutoSetButton && cameraLutAutoSetButton->text() == "Auto Set",
                    "CameraLutAutoSetButton exists with expected text");
            require(cameraLutAutoSetButton &&
                        cameraLutAutoSetButton->toolTip().contains("current frame", Qt::CaseInsensitive),
                    "CameraLutAutoSetButton tooltip explains current-frame behavior");
            require(cameraBitsCombo && cameraBitsCombo->count() == 3 && cameraBitsCombo->itemText(0) == "8" &&
                        cameraBitsCombo->itemText(1) == "12" && cameraBitsCombo->itemText(2) == "16",
                    "CameraBitsComboBox offers 8, 12, and 16-bit choices");
            require(cameraBitsCombo && cameraBitsCombo->currentText() == "8", 
                    "CameraBitsComboBox defaults to 8-bit"); 
            require(cameraReadoutCombo && cameraReadoutCombo->count() == 1 && 
                        cameraReadoutCombo->itemText(0).compare("Fast", Qt::CaseInsensitive) == 0 && 
                        !cameraReadoutCombo->isEnabled(), 
                    "CameraReadoutSpeedComboBox is fixed to disabled Fast"); 
            const bool realCameraVerifier = !options.noStartupPrompts; 
            if (!realCameraVerifier) { 
                require(pipelineStartButton != nullptr, "Start Sorting control exists in no-hardware verifier"); 
                require(pipelineStartButton && pipelineStartButton->isVisibleTo(this), 
                        "Sorting action starts as Start Sorting"); 
                require(pipelineStopButton && !pipelineStopButton->isVisibleTo(this), 
                        "Stop Sorting action starts hidden until sorting is active"); 
            } else { 
                require(pipelineStartButton != nullptr, "Start Sorting control exists in real camera verifier");
                require(pipelineStopButton != nullptr, "Stop Sorting control exists in real camera verifier");
            }
            require(navRailFrame != nullptr, "OpenDssNavigationRail exists");
            require(headerFrame != nullptr, "OpenDssHeader exists");
            require(statusStripFrame != nullptr, "OpenDssStatusStrip exists");
            require(displayOverlayAction == nullptr, "DisplayOverlayAction is absent");
            require(displayClearOverlayAction == nullptr, "DisplayClearOverlayAction is absent");
            require(analysisOverlayCheck == nullptr, "AnalysisOverlayCheckBox is absent");
            require(liveViewerOverlayToggle == nullptr, "LiveViewerOverlayToggle is absent");
            require(liveViewerDetectionOverlay == nullptr, "LiveViewerDetectionOverlay is absent");
            require(liveDetectorSettingsButton && liveDetectorSettingsButton->text() == "Detector",
                    "Live detector settings button is discoverable");
            require(liveDetectorSettingsButton && liveDetectorSettingsButton->toolTip() == "Open detector settings.",
                    "Live detector settings button tooltip explains the action");
            require(liveDetectorSettingsDrawer != nullptr, "Live detector settings drawer exists");
            require(hasLabelText(liveDetectorSettingsDrawer, "Detector settings"),
                    "Live detector drawer title reads Detector settings");
            require(hasLabelText(liveDetectorSettingsDrawer, "Min rectangle size"),
                    "Live detector minimum bbox label uses rectangle wording");
            require(!hasLabelText(liveDetectorSettingsDrawer, "Min contour points"),
                    "Live detector drawer no longer labels the bbox filter as contour points");
            require(liveDetectorMinRectangleSpin && liveDetectorMinRectangleSpin->suffix().trimmed() == "px",
                    "Live detector minimum rectangle control shows pixel units");
            require(liveDetectorMinRectangleSpin &&
                        liveDetectorMinRectangleSpin->toolTip().contains("Minimum bounding rectangle width and height"),
                    "Live detector minimum rectangle tooltip explains width and height");
            require(cameraControlsStack && rightViewport && cameraControlsStack->width() <= rightViewport->width(),
                    "CameraControlsStack width does not exceed the Live View right-panel viewport");
            require(rightScroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
                    "Live View right-panel horizontal scrollbar remains disabled");

            requireHorizontallyContained(cameraFormatPanelFrame, rightViewport,
                                         "CameraFormatSpeedPanelFrame fits within the viewport width");
            requireHorizontallyContained(cameraRecordingPanelFrame, rightViewport,
                                         "CameraRecordingPanelFrame fits within the viewport width");
            requireContained(cameraPresetCombo, cameraFormatPanelFrame,
                             "CameraPresetComboBox fits within Format & Speed");
            requireContained(cameraBitsCombo, cameraFormatPanelFrame, "CameraBitsComboBox fits within Format & Speed");
            requireContained(cameraWidthSpin, cameraFormatPanelFrame,
                             "CameraCustomWidthSpinBox fits within Format & Speed");
            requireContained(cameraHeightSpin, cameraFormatPanelFrame,
                             "CameraCustomHeightSpinBox fits within Format & Speed");
            requireContained(cameraExposureSpin, cameraFormatPanelFrame,
                             "CameraExposureSpinBox fits within Format & Speed");
            requireContained(cameraAutoExposureButton, cameraFormatPanelFrame,
                             "CameraAutoExposureButton fits within Format & Speed");
            requireContained(cameraReadoutCombo, cameraFormatPanelFrame,
                             "CameraReadoutSpeedComboBox fits within Format & Speed");
            requireContained(cameraBinningCombo, cameraFormatPanelFrame,
                             "CameraBinningComboBox fits within Format & Speed");
            require(cameraPresetCombo && cameraPresetCombo->width() >= cameraPresetCombo->sizeHint().width(),
                    "CameraPresetComboBox expands enough for the active preset text");
            require(cameraReadoutCombo && cameraReadoutCombo->width() >= cameraReadoutCombo->sizeHint().width(),
                    "CameraReadoutSpeedComboBox expands enough for the active readout text");
            requireContained(cameraLutMinSpin, cameraFormatPanelFrame, "CameraLutMinSpinBox fits within Format & Speed");
            requireContained(cameraLutMaxSpin, cameraFormatPanelFrame, "CameraLutMaxSpinBox fits within Format & Speed");
            requireContained(cameraLutAutoSetButton, cameraFormatPanelFrame,
                             "CameraLutAutoSetButton fits within Format & Speed");
            requireContained(cameraLutRangeBar, cameraFormatPanelFrame, "CameraLutRangeBar fits within Format & Speed");
            requireContained(savePathLineEdit, cameraRecordingPanelFrame, "SavePathEdit fits within Recording");
            requireContained(saveBrowseButton, cameraRecordingPanelFrame, "SaveBrowseButton fits within Recording");
            requireContained(saveOpenFolderButton, cameraRecordingPanelFrame,
                             "SaveOpenFolderButton fits within Recording");
            requireContained(cameraRecordingFormatControl, cameraRecordingPanelFrame,
                             "CameraRecordingFormatSegmentedControl fits within Recording");
            requireContained(saveStartButton, cameraRecordingPanelFrame, "SaveStartButton fits within Recording");
            requireContained(saveStopButton, cameraRecordingPanelFrame, "SaveStopButton fits within Recording");
            requireHorizontallyContained(cameraSequencePanelFrame, rightViewport,
                                         "CameraSequenceTestPanelFrame fits within the viewport width");
            requireContained(sequenceFolderEdit, cameraSequencePanelFrame, "SequenceFolderEdit fits within Sequence Test");
            requireContained(sequenceBrowseButton, cameraSequencePanelFrame,
                             "SequenceBrowseButton fits within Sequence Test");
            requireContained(sequenceLoadButton, cameraSequencePanelFrame, "SequenceLoadButton fits within Sequence Test");
            requireContained(sequenceStartButton, cameraSequencePanelFrame,
                             "SequenceStartTestButton fits within Sequence Test");
            requireContained(sequenceStopButton, cameraSequencePanelFrame, "SequenceStopButton fits within Sequence Test");
            requireContained(sequenceFpsSpin, cameraSequencePanelFrame, "SequenceFpsSpinBox fits within Sequence Test");
            requireContained(sequenceStatusLabel, cameraSequencePanelFrame,
                             "SequenceStatusLabel fits within Sequence Test");
            requireContained(sequenceLogLabel, cameraSequencePanelFrame, "SequenceLogLabel fits within Sequence Test");
            require(sequenceStartButton && sequenceStartButton->text().contains("Recorded Sequence"),
                    "Sequence run button makes recorded-sequence simulation explicit");
            require(sequenceStartButton &&
                        sequenceStartButton->toolTip().contains("DAQ output disabled", Qt::CaseInsensitive),
                    "Sequence run button tooltip states DAQ output is disabled");
            require(sequenceStartButton && sequenceStartButton->property("daqOutputMode").toString() ==
                                               QStringLiteral("disabled-for-replay"),
                    "Sequence run button exposes the disabled-for-replay DAQ output guard");
            require(sequenceStartButton && !sequenceStartButton->isEnabled(),
                    "Sequence run button remains disabled until a sequence is loaded");
            require(runStateResetButton != nullptr, "RunStateResetCountersButton exists");
            require(runStateResetButton && runStateResetButton->objectName() == "RunStateResetCountersButton",
                    "RunStateResetCountersButton keeps a stable object name");
            require(runStateResetButton && runStateResetButton->isVisibleTo(this),
                    "RunStateResetCountersButton is visible in Live View");

            const QString initialRunStatusText = runStatusItem ? runStatusItem->text() : QString();
            if (realCameraVerifier) {
                auto stopCameraIfStreaming = [&]() {
                    if (cameraStartButton && appState.cameraStreaming) {
                        cameraStartButton->click();
                        return waitUntil(8000, [&]() { return !appState.cameraStreaming; });
                    }
                    return true;
                };
                auto waitForRealCameraBits = [&](int expectedBits, const QString& context) {
                    const bool frameWithBits = waitUntil(12000, [&]() {
                        const FrameMeta meta = cameraController->lastMeta();
                        return !cameraController->lastFrame().isNull() && meta.frameIndex > 0 &&
                               meta.bits == expectedBits;
                    });
                    const FrameMeta meta = cameraController->lastMeta();
                    require(frameWithBits,
                            QString("%1 real camera readback reached %2-bit depth (readback=%3)")
                                .arg(context)
                                .arg(expectedBits)
                                .arg(meta.bits));
                    require(meta.width > 0 && meta.height > 0 && meta.frameIndex > 0,
                            QString("%1 real camera frame metadata is populated (width=%2 height=%3 frame=%4 "
                                    "delivered=%5)")
                                .arg(context)
                                .arg(meta.width)
                                .arg(meta.height)
                                .arg(meta.frameIndex)
                                .arg(meta.delivered));
                    require(meta.readoutSpeed > DCAMPROP_READOUTSPEED__SLOWEST,
                            QString("%1 real camera readback accepted Fast readout (readout=%2 slowest=%3)")
                                .arg(context)
                                .arg(meta.readoutSpeed, 0, 'f', 0)
                                .arg(DCAMPROP_READOUTSPEED__SLOWEST));
                    qInfo().noquote()
                        << QString("VERIFY INFO: %1 real camera status=%2 frame=%3 size=%4x%5 delivered=%6 dropped=%7 "
                                   "bits=%8 readout=%9 slowest=%10")
                               .arg(context)
                               .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>"))
                               .arg(meta.frameIndex)
                               .arg(meta.width)
                               .arg(meta.height)
                               .arg(meta.delivered)
                               .arg(meta.dropped)
                               .arg(meta.bits)
                               .arg(meta.readoutSpeed, 0, 'f', 0)
                               .arg(DCAMPROP_READOUTSPEED__SLOWEST);
                    return meta;
                };
                const bool initSettled = waitUntil(12000, [&]() {
                    const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                    return cameraOpened || cameraStatus.contains("error", Qt::CaseInsensitive) ||
                           cameraStatus.contains("unavailable", Qt::CaseInsensitive);
                });
                require(initSettled, "Real camera initialization reached a terminal app-owned status");
                require(cameraOpened && cameraStatusItem && cameraStatusItem->text() == "Camera: connected",
                        QString("Real camera initialized and status chip is connected (status=%1)")
                            .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>")));
                if (cameraOpened && cameraStartButton) {
                    cameraStartButton->click();
                    const bool captureStarted = waitUntil(8000, [&]() {
                        const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                        return appState.cameraStreaming || cameraStatus.contains("acquiring", Qt::CaseInsensitive) ||
                               cameraStatus.contains("error", Qt::CaseInsensitive);
                    });
                    require(captureStarted, "Real camera capture start completed through CameraStartButton");
                    require(appState.cameraStreaming && cameraStatusItem &&
                                cameraStatusItem->text().contains("acquiring", Qt::CaseInsensitive),
                            QString("Real camera status chip reports acquiring (status=%1)")
                                .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>")));
                    const FrameMeta realMeta = waitForRealCameraBits(8, QStringLiteral("Default 8-bit"));
                    const int selectedBits = cameraBitsCombo ? cameraBitsCombo->currentText().toInt() : 0;
                    require(selectedBits == 8 && realMeta.bits == selectedBits,
                            QString("Real camera readback matches selected 8-bit depth (selected=%1 readback=%2)")
                                .arg(selectedBits)
                                .arg(realMeta.bits));
                    require(stopCameraIfStreaming(), "Real camera capture stops through CameraStartButton");

                    const int bit12Index = cameraBitsCombo ? cameraBitsCombo->findData(12) : -1;
                    require(bit12Index >= 0, "CameraBitsComboBox can select 12-bit mode by data");
                    if (bit12Index >= 0) {
                        cameraBitsCombo->setCurrentIndex(bit12Index);
                    }
                    qInfo().noquote()
                        << QString("VERIFY INFO: applying 12-bit camera mode with pixelType=%1")
                               .arg(DCAM_PIXELTYPE_MONO16);
                    if (cameraApplyButton) {
                        cameraApplyButton->click();
                    }
                    const bool apply12Started = waitUntil(8000, [&]() {
                        const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                        return appState.cameraStreaming || cameraStatus.contains("acquiring", Qt::CaseInsensitive) ||
                               cameraStatus.contains("error", Qt::CaseInsensitive);
                    });
                    require(apply12Started, "Applying 12-bit mode starts real camera capture");
                    const FrameMeta bit12Meta = waitForRealCameraBits(12, QStringLiteral("Applied 12-bit"));
                    require(bit12Meta.bits == 12,
                            QString("Real camera readback matches selected 12-bit depth (readback=%1)")
                                .arg(bit12Meta.bits));
                    require(stopCameraIfStreaming(), "Real camera capture stops after 12-bit verification");

                    const int bit8Index = cameraBitsCombo ? cameraBitsCombo->findData(8) : -1;
                    if (bit8Index >= 0) {
                        cameraBitsCombo->setCurrentIndex(bit8Index);
                        if (cameraApplyButton) {
                            cameraApplyButton->click();
                        }
                        const bool restore8Started = waitUntil(8000, [&]() {
                            const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                            return appState.cameraStreaming ||
                                   cameraStatus.contains("acquiring", Qt::CaseInsensitive) ||
                                   cameraStatus.contains("error", Qt::CaseInsensitive);
                        });
                        require(restore8Started, "Restoring 8-bit mode starts real camera capture");
                        waitForRealCameraBits(8, QStringLiteral("Restored 8-bit"));
                        require(stopCameraIfStreaming(), "Real camera capture stops after 8-bit restore");
                    }
                }
            }
            require(runStatusItem && runStatusItem->text() == initialRunStatusText,
                    "Real camera verifier leaves sorting run state unchanged");

            {
                StatsSnapshot seededSnap;
                {
                    QMutexLocker lock(&statsMutex);
                    stats.totalEvents = 9;
                    stats.classifiedHitCount = 4;
                    stats.classifiedWasteCount = 3;
                    stats.wentToHitCount = 2;
                    stats.wentToWasteCount = 7;
                    stats.classCounts.clear();
                    stats.classCounts.insert("Single", 4);
                    stats.classCounts.insert("Empty", 3);
                    stats.lastEventDir = "Hit";
                    stats.lastEventLabel = "Single";
                    stats.lastDecisionFrame = 42;
                    stats.lastDecisionEventId = 9;
                    stats.eventActive = false;
                    seededSnap = makeStatsSnapshot(stats);
                }
                applyStatsSnapshot(seededSnap);
                waitForUi(650);
                require(liveRunEventsMetricLabel && liveRunEventsMetricLabel->text() == "9",
                        "LiveRunEventsMetricLabel reflects seeded event count before reset");
                require(liveRunClassifiedHitMetricLabel && liveRunClassifiedHitMetricLabel->text() == "4",
                        "LiveRunClassifiedHitMetricLabel reflects seeded hit count before reset");
                require(liveRunClassifiedWasteMetricLabel && liveRunClassifiedWasteMetricLabel->text() == "3",
                        "LiveRunClassifiedWasteMetricLabel reflects seeded waste count before reset");
                require(liveRunWentToHitMetricLabel && liveRunWentToHitMetricLabel->text() == "2",
                        "LiveRunWentToHitMetricLabel reflects seeded went-to-hit count before reset");
                require(liveRunWentToWasteMetricLabel && liveRunWentToWasteMetricLabel->text() == "7", 
                        "LiveRunWentToWasteMetricLabel reflects seeded went-to-waste count before reset"); 
                require(statsClassTextLabel && statsClassTextLabel->text().contains("Single: 4"), 
                        "StatsClassCountsLabel reflects seeded class counts before reset"); 
                require(statsLastEventLabel && statsLastEventLabel->text().contains("Single (Sort)"), 
                        "StatsLastEventLabel reflects seeded last event before reset"); 

                if (runStateResetButton)
                    runStateResetButton->click();
                waitForUi(650);
                require(liveRunEventsMetricLabel && liveRunEventsMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets event count to zero");
                require(liveRunClassifiedHitMetricLabel && liveRunClassifiedHitMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets classified hit count to zero");
                require(liveRunClassifiedWasteMetricLabel && liveRunClassifiedWasteMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets classified waste count to zero");
                require(liveRunWentToHitMetricLabel && liveRunWentToHitMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets went-to-hit count to zero");
                require(liveRunWentToWasteMetricLabel && liveRunWentToWasteMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets went-to-waste count to zero");
                require(statsClassTextLabel && statsClassTextLabel->text() == "Classes:\n(none)",
                        "RunStateResetCountersButton clears class count text back to default");
                require(statsLastEventLabel && statsLastEventLabel->text() == "Last event: --",
                        "RunStateResetCountersButton clears last-event text");
                require(liveLastDecisionValueLabel && liveLastDecisionValueLabel->text() == "--",
                        "RunStateResetCountersButton clears the last decision summary");
            }

            const auto shellColors = desktop_app::theme::colors(currentThemeMode);
            const QString shellCss = shellColors.shellBackground.name(QColor::HexRgb);
            const QString appCss = shellColors.appBackground.name(QColor::HexRgb);
            const QString styleSheet = this->styleSheet();
            require(styleSheet.contains(shellCss, Qt::CaseInsensitive),
                    QString("shell stylesheet contains neutral shell color %1").arg(shellCss));
            require(shellCss.compare(QStringLiteral("#0B1F5E"), Qt::CaseInsensitive) != 0,
                    "shell color is no longer dark blue");
            require(shellCss.compare(appCss, Qt::CaseInsensitive) != 0,
                    "shell color remains distinct from app background");

            lutMinSpin->setValue(32);
            lutMaxSpin->setValue(180);
            app.processEvents();
            require(cameraController->lutMinValue() == 32, "LUT min runtime value updates from consolidated controls");
            require(cameraController->lutMaxValue() == 180, "LUT max runtime value updates from consolidated controls");

            QImage sample(320, 240, QImage::Format_Grayscale8);
            for (int y = 0; y < sample.height(); ++y) {
                uchar* row = sample.scanLine(y);
                for (int x = 0; x < sample.width(); ++x) {
                    row[x] = static_cast<uchar>((x + y) % 256);
                }
            }
            FrameMeta verifyMeta;
            verifyMeta.width = sample.width();
            verifyMeta.height = sample.height();
            verifyMeta.bits = 8;
            verifyMeta.binning = 1.0;
            verifyMeta.frameIndex = 42;
            verifyMeta.delivered = 42;
            verifyMeta.dropped = 0;
            verifyMeta.internalFps = 37.5;
            verifyMeta.readoutSpeed = 1.0;
            cameraController->applyFrameToPreviewWorkspaces(sample, verifyMeta, 37.5);
            app.processEvents();

            auto* liveImageLabel = imageView->findChild<QLabel*>("LiveImageLabel");
            const QPixmap livePixmap = liveImageLabel ? liveImageLabel->pixmap(Qt::ReturnByValue) : QPixmap();
            require(liveImageLabel && !livePixmap.isNull(), "LiveImageLabel received a rendered frame");
            require(!liveViewerEmpty->isVisible(), "LiveViewerEmptyState hides after frame update");
            require(liveHudResolution->text().contains("320 x 240"),
                    "LiveViewerHudResolutionLabel updated from frame data");
            require(liveHudFps->text().contains("42"), "LiveViewerHudFpsLabel updated from frame data");
            if (cameraLutAutoSetButton) {
                cameraLutAutoSetButton->click();
                app.processEvents();
            }
            require(cameraController->lutMaxValue() > cameraController->lutMinValue(),
                    "CameraLutAutoSetButton keeps LUT range ordered after direct Qt click");
            require(cameraLutMinSpin && cameraLutMinSpin->value() == cameraController->lutMinValue(),
                    "CameraLutAutoSetButton updates the visible black-level spin box");
            require(cameraLutMaxSpin && cameraLutMaxSpin->value() == cameraController->lutMaxValue(),
                    "CameraLutAutoSetButton updates the visible white-level spin box");
            require(cameraLutRangeBar &&
                        cameraLutRangeBar->property("lutMinimumValue").toInt() == cameraController->lutMinValue() &&
                        cameraLutRangeBar->property("lutMaximumValue").toInt() == cameraController->lutMaxValue(),
                    "CameraLutAutoSetButton updates CameraLutRangeBar Qt state");
            const double exposureBeforeAuto = exposureSpin->value();
            if (cameraAutoExposureButton) {
                cameraAutoExposureButton->click();
                waitForUi(350);
            }
            require(exposureSpin->value() >= exposureSpin->minimum() && exposureSpin->value() <= exposureSpin->maximum(),
                    QString("CameraAutoExposureButton leaves exposure within limits (before=%1 after=%2)")
                        .arg(exposureBeforeAuto, 0, 'f', 3)
                        .arg(exposureSpin->value(), 0, 'f', 3));

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (!failures.isEmpty()) {
                logMessage("Live camera consolidation verifier failed: " + failures.join("; "));
            } else {
                logMessage("Live camera consolidation verifier passed.");
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyDaqSettings) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };

            settingsController->refreshDaqDeviceOptions(true);
            workspaceStack->setCurrentWidget(settingsWorkspacePage);
            settingsNavButton->setChecked(true);
            headerTitleLabel->setText("/ Settings");
            headerStatusText->setText("Settings workspace");
            app.processEvents();
            waitForUi(350);

            auto* settingsHardwarePanel = this->findChild<QWidget*>("SettingsHardwarePanel");
            auto* deviceCombo = this->findChild<QComboBox*>("DaqDeviceComboBox");
            auto* channelEdit = this->findChild<QLineEdit*>("DaqChannelEdit");
            auto* amplitudeSpin = this->findChild<QDoubleSpinBox*>("DaqAmplitudeSpinBox");
            auto* frequencySpin = this->findChild<QDoubleSpinBox*>("DaqFrequencySpinBox");
            auto* durationSpin = this->findChild<QDoubleSpinBox*>("DaqDurationSpinBox");
            auto* delaySpin = this->findChild<QDoubleSpinBox*>("DaqDelaySpinBox");
            auto* reconnectButton = this->findChild<QPushButton*>("DaqReconnectButton");
            auto* manualTriggerButton = this->findChild<QPushButton*>("DaqManualTriggerButton");
            auto* statusIndicatorWidget = this->findChild<QLabel*>("DaqStatusTextLabel");
            auto* statusBarDaqWidget = this->findChild<QLabel*>("DaqStatusBarLabel");
            auto* shellDaqStatusWidget = this->findChild<QLabel*>("OpenDssShellDaqStatusLabel");
            auto* headerDaqChipWidget = this->findChild<QLabel*>("OpenDssHeaderDaqChip");
            auto* liveTriggerSafeButton = this->findChild<QPushButton*>("LiveTriggerSafeButton");
            auto* forceTriggerButton = this->findChild<QPushButton*>("LiveForceTriggerButton");
            auto* manualTriggerMenuAction = this->findChild<QAction*>("SortingForceTriggerAction");
            auto hasPanelLabelText = [](QWidget* root, const QString& text) {
                if (!root) {
                    return false;
                }
                for (auto* label : root->findChildren<QLabel*>()) {
                    if (label->text() == text) {
                        return true;
                    }
                }
                return false;
            };
            const QString daqStatusText = statusBarDaqWidget ? statusBarDaqWidget->text().trimmed().toLower() : QString();
            const bool manualTriggerReady =
                daqStatusText.contains("available") && !daqStatusText.contains("disabled") &&
                !daqStatusText.contains("unavailable");

            require(settingsHardwarePanel != nullptr, "Settings hardware panel exists");
            require(deviceCombo != nullptr, "DAQ device combo exists");
            require(channelEdit != nullptr, "DAQ channel edit exists");
            require(deviceCombo && deviceCombo->objectName() == "DaqDeviceComboBox",
                    "DAQ device combo uses the direct-lookup object name");
            require(deviceCombo && channelEdit &&
                        deviceCombo->mapTo(settingsHardwarePanel, QPoint(0, 0)).y() <
                            channelEdit->mapTo(settingsHardwarePanel, QPoint(0, 0)).y(),
                    "DAQ device combo is above the DAQ channel field in Settings > Hardware");
            require(reconnectButton && reconnectButton->text() == "Reconnect DAQ",
                    "DAQ reconnect button wording matches the current DAQ path");
            require(manualTriggerButton && !manualTriggerButton->isVisibleTo(this),
                    "Settings Manual Trigger is hidden from users");
            require(liveTriggerSafeButton == nullptr, "LiveTriggerSafeButton is absent from Live View");
            require(forceTriggerButton && forceTriggerButton->text() == "Manual Trigger",
                    "Live View uses Manual Trigger wording below the camera frame");
            require(manualTriggerMenuAction != nullptr, "Manual Trigger menu action exists");
            require(manualTriggerMenuAction && forceTriggerButton &&
                        manualTriggerMenuAction->isEnabled() == forceTriggerButton->isEnabled(),
                    "Manual Trigger menu action shares the Live View button gate");
            if (manualTriggerReady) {
                require(forceTriggerButton && forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger enables automatically when DAQ is available");
                require(manualTriggerMenuAction && manualTriggerMenuAction->isEnabled(),
                        "Manual Trigger menu action enables automatically when DAQ is available");
            } else {
                require(forceTriggerButton && !forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger stays disabled when DAQ is unavailable");
                require(manualTriggerMenuAction && !manualTriggerMenuAction->isEnabled(),
                        "Manual Trigger menu action stays disabled when DAQ is unavailable");
            }
            require(!hasPanelLabelText(settingsHardwarePanel, "Test mode preference"),
                    "Settings hardware no longer exposes a test mode preference");

            QStringList comboEntries;
            for (int i = 0; i < deviceCombo->count(); ++i) {
                comboEntries
                    << QStringLiteral("%1 => %2").arg(deviceCombo->itemText(i), deviceCombo->itemData(i).toString());
            }
            qInfo().noquote() << "VERIFY INFO: DAQ combo entries:" << comboEntries.join(" | ");
            qInfo().noquote() << "VERIFY INFO: Selected DAQ device:" << deviceCombo->currentData().toString();
            qInfo().noquote() << "VERIFY INFO: Selected DAQ channel:" << channelEdit->text().trimmed();
            qInfo().noquote() << "VERIFY INFO: Manual Trigger enabled:"
                              << (manualTriggerButton && manualTriggerButton->isEnabled());
            qInfo().noquote() << "VERIFY INFO: Discovered DAQ summary:"
                              << (settingsController->describeDiscoveredDaqDevices().isEmpty()
                                      ? QStringLiteral("<none>")
                                      : settingsController->describeDiscoveredDaqDevices());
            if (!settingsController->daqDiscoveryError().isEmpty()) {
                qInfo().noquote() << "VERIFY INFO: DAQ discovery status:" << settingsController->daqDiscoveryError();
            }

            if (!settingsController->discoveredDaqDevices().empty()) {
                require(deviceCombo->count() == static_cast<int>(settingsController->discoveredDaqDevices().size()),
                        "DAQ combo count matches the discovered device list");
            }

            const int compatibleCount = settingsController->discoveredCompatibleDeviceCount();
            QString onlyCompatibleDevice;
            for (const auto& device : settingsController->discoveredDaqDevices()) {
                if (device.isCompatible()) {
                    onlyCompatibleDevice = QString::fromStdString(device.name);
                    break;
                }
            }
            if (compatibleCount == 1) {
                require(deviceCombo->currentData().toString().compare(onlyCompatibleDevice, Qt::CaseInsensitive) == 0,
                        QStringLiteral("Single compatible DAQ auto-selects %1").arg(onlyCompatibleDevice));
            } else {
                qInfo().noquote() << "VERIFY INFO: Compatible DAQ count =" << compatibleCount;
            }

            const bool hasRealDiscoveredSelection =
                !settingsController->discoveredDaqDevices().empty() &&
                !deviceCombo->currentData().toString().trimmed().isEmpty();

            if (deviceCombo->count() > 1) {
                const int originalIndex = deviceCombo->currentIndex();
                const int nextIndex = (originalIndex + 1) % deviceCombo->count();
                deviceCombo->setCurrentIndex(nextIndex);
                waitForUi(350);
                QSettings settings;
                const QString selectedDevice = deviceCombo->currentData().toString().trimmed();
                require(settings.value("settings/daqSelectedDevice")
                                .toString()
                                .trimmed()
                                .compare(selectedDevice, Qt::CaseInsensitive) == 0,
                        "Changing the DAQ combo persists the selected device in QSettings");
                const DaqDeviceInfo* selectedInfo = nullptr;
                for (const auto& device : settingsController->discoveredDaqDevices()) {
                    if (QString::fromStdString(device.name).compare(selectedDevice, Qt::CaseInsensitive) == 0) {
                        selectedInfo = &device;
                        break;
                    }
                }
                if (selectedInfo && selectedInfo->isCompatible()) {
                    const QString channelText = channelEdit->text().trimmed();
                    const int slash = channelText.indexOf('/');
                    const QString channelDevice = slash > 0 ? channelText.left(slash) : channelText;
                    require(channelDevice.compare(selectedDevice, Qt::CaseInsensitive) == 0,
                            "Changing the DAQ combo updates the active DAQ channel device prefix");
                } else {
                    require(channelEdit->text().trimmed().isEmpty(),
                            "Selecting a DAQ without AO output clears the active channel");
                }
                if (reconnectButton) {
                    reconnectButton->click();
                    waitForUi(500);
                }
                require(statusIndicatorWidget != nullptr && !statusIndicatorWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the DAQ indicator populated");
                require(statusBarDaqWidget != nullptr && !statusBarDaqWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the DAQ status-bar label populated");
                require(headerDaqChipWidget != nullptr && !headerDaqChipWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the header DAQ chip populated");
                require(forceTriggerButton != nullptr && !forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger stays disabled during verification when DAQ is unavailable");
            } else if (deviceCombo->count() == 1 && hasRealDiscoveredSelection) {
                QSettings settings;
                require(settings.value("settings/daqSelectedDevice")
                                .toString()
                                .trimmed()
                                .compare(deviceCombo->currentData().toString().trimmed(), Qt::CaseInsensitive) == 0,
                        "Single discovered DAQ selection is persisted in QSettings");
                if (reconnectButton) {
                    reconnectButton->click();
                    waitForUi(500);
                }
            } else {
                require(!deviceCombo->isEnabled(), "DAQ combo disables when no devices are available");
                require(deviceCombo->currentData().toString().trimmed().isEmpty(),
                        "No-device DAQ combo placeholder does not expose a real device selection");
            }

            if (statusBarDaqWidget && shellDaqStatusWidget && headerDaqChipWidget) {
                require(statusBarDaqWidget->text() == shellDaqStatusWidget->text(),
                        "Shell DAQ status mirrors the DAQ status-bar label");
                const QString statusText = statusBarDaqWidget->text().toLower();
                const QString headerText = headerDaqChipWidget->text().toLower();
                qInfo().noquote() << "VERIFY INFO: DAQ status-bar text:" << statusBarDaqWidget->text();
                qInfo().noquote() << "VERIFY INFO: Header DAQ chip text:" << headerDaqChipWidget->text();
                if (statusText.contains("unavailable")) {
                    require(headerText.contains("unavailable"),
                            "Header DAQ chip reports unavailable when DAQ status is unavailable");
                } else if (statusText.contains("disabled")) {
                    require(headerText.contains("unavailable"),
                            "Header DAQ chip reports unavailable when DAQ status is disabled");
                } else if (statusText.contains("available")) {
                    require(headerText.contains("available"),
                            "Header DAQ chip reports available when DAQ status is available");
                } else {
                    require(headerText.contains("unavailable") || headerText.contains("unchecked"),
                            "Header DAQ chip remains coherent when DAQ status is unavailable");
                }
            }

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (!failures.isEmpty()) {
                logMessage("DAQ settings verifier failed: " + failures.join("; "));
            } else {
                logMessage("DAQ settings verifier passed.");
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyDirectDaqManualTrigger || options.verifyLiveViewManualTrigger) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            bool triggerInvoked = false;
            const bool verifyLiveViewTrigger = options.verifyLiveViewManualTrigger;
            const QString verifierPrefix = verifyLiveViewTrigger ? QStringLiteral("LIVE VIEW DAQ VERIFY")
                                                                 : QStringLiteral("DIRECT DAQ VERIFY");
            const QString triggerObjectName = verifyLiveViewTrigger ? QStringLiteral("LiveForceTriggerButton")
                                                                    : QStringLiteral("DaqManualTriggerButton");
            const QString triggerSource = verifyLiveViewTrigger ? QStringLiteral("LiveForceTriggerButton")
                                                                : QStringLiteral("DaqManualTriggerButton");
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << verifierPrefix << "FAIL:" << message;
                } else {
                    qInfo().noquote() << verifierPrefix << "PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto finish = [&](int exitCode) {
                qInfo().noquote() << verifierPrefix << "INFO: TriggerSource=" << triggerSource;
                qInfo().noquote() << verifierPrefix << "INFO: TriggerInvoked=" << (triggerInvoked ? 1 : 0);
                QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
            };
            auto nearlyEqual = [](double actual, double expected) {
                return std::abs(actual - expected) <= 0.0005;
            };

            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            workspaceStack->setCurrentWidget(settingsWorkspacePage);
            settingsNavButton->setChecked(true);
            headerTitleLabel->setText("/ Settings");
            headerStatusText->setText("Settings workspace");
            app.processEvents();
            waitForUi(350);

            auto* deviceCombo = this->findChild<QComboBox*>("DaqDeviceComboBox");
            auto* channelEdit = this->findChild<QLineEdit*>("DaqChannelEdit");
            auto* amplitudeSpin = this->findChild<QDoubleSpinBox*>("DaqAmplitudeSpinBox");
            auto* frequencySpin = this->findChild<QDoubleSpinBox*>("DaqFrequencySpinBox");
            auto* durationSpin = this->findChild<QDoubleSpinBox*>("DaqDurationSpinBox");
            auto* delaySpin = this->findChild<QDoubleSpinBox*>("DaqDelaySpinBox");
            auto* manualTriggerButton = this->findChild<QPushButton*>("DaqManualTriggerButton");
            auto* statusIndicatorWidget = this->findChild<QLabel*>("DaqStatusTextLabel");
            auto* statusBarDaqWidget = this->findChild<QLabel*>("DaqStatusBarLabel");
            auto* forceTriggerButton = this->findChild<QPushButton*>("LiveForceTriggerButton");
            auto* triggerButton = verifyLiveViewTrigger ? forceTriggerButton : manualTriggerButton;

            const QString selectedDevice = deviceCombo ? deviceCombo->currentData().toString().trimmed() : QString();
            const QString selectedChannel = channelEdit ? channelEdit->text().trimmed() : QString();
            const QString statusIndicatorText =
                statusIndicatorWidget ? statusIndicatorWidget->text().trimmed() : QString();
            const QString statusBarText = statusBarDaqWidget ? statusBarDaqWidget->text().trimmed() : QString();
            bool pipelineTriggerReady = false;
            {
                QMutexLocker lock(&pipelineMutex);
                pipelineTriggerReady = pipeline.isTriggerReady();
            }

            qInfo().noquote() << verifierPrefix << "INFO: SelectedDevice=" << selectedDevice;
            qInfo().noquote() << verifierPrefix << "INFO: SelectedChannel=" << selectedChannel;
            qInfo().noquote() << verifierPrefix << "INFO: AmplitudeV="
                              << (amplitudeSpin ? amplitudeSpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: FrequencyKHz="
                              << (frequencySpin ? frequencySpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: DurationMs="
                              << (durationSpin ? durationSpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: DelayMs="
                              << (delaySpin ? delaySpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: StatusIndicator=" << statusIndicatorText;
            qInfo().noquote() << verifierPrefix << "INFO: StatusBar=" << statusBarText;
            qInfo().noquote() << verifierPrefix << "INFO: PipelineTriggerReady=" << pipelineTriggerReady;
            qInfo().noquote() << verifierPrefix << "INFO: TriggerCount=1";

            require(!appState.daqDisabled, "DAQ state is not disabled");
            require(!appState.daqFault, "DAQ state is not faulted");
            require(appState.daqAvailable, "DAQ state is available");
            require(appState.daqStatusText.compare(QStringLiteral("DAQ: available"), Qt::CaseInsensitive) == 0,
                    "DAQ status text is available");
            require(deviceCombo != nullptr, "DaqDeviceComboBox exists");
            require(channelEdit != nullptr, "DaqChannelEdit exists");
            require(amplitudeSpin != nullptr, "DaqAmplitudeSpinBox exists");
            require(frequencySpin != nullptr, "DaqFrequencySpinBox exists");
            require(durationSpin != nullptr, "DaqDurationSpinBox exists");
            require(delaySpin != nullptr, "DaqDelaySpinBox exists");
            require(manualTriggerButton != nullptr, "DaqManualTriggerButton exists");
            require(manualTriggerButton && !manualTriggerButton->isVisibleTo(this),
                    "DaqManualTriggerButton is hidden from users");
            require(forceTriggerButton != nullptr, "LiveForceTriggerButton exists");
            require(forceTriggerButton && forceTriggerButton->text() == "Manual Trigger",
                    "Live View Manual Trigger button wording matches the direct DAQ path");
            require(forceTriggerButton && forceTriggerButton->isEnabled(), "LiveForceTriggerButton is enabled");
            require(triggerButton != nullptr, triggerObjectName + QStringLiteral(" exists"));
            require(triggerButton && triggerButton->isEnabled(), triggerObjectName + QStringLiteral(" is enabled"));
            require(selectedDevice == QStringLiteral("Dev1"), "Selected DAQ device is Dev1");
            require(selectedChannel == QStringLiteral("Dev1/ao0"), "Selected DAQ channel is Dev1/ao0");
            require(amplitudeSpin && nearlyEqual(amplitudeSpin->value(), 5.0), "Amplitude is 5.000 V");
            require(frequencySpin && nearlyEqual(frequencySpin->value(), 10.0), "Frequency is 10.000 kHz");
            require(durationSpin && nearlyEqual(durationSpin->value(), 5.0), "Duration is 5.000 ms");
            require(delaySpin && nearlyEqual(delaySpin->value(), 0.0), "Delay is 0.000 ms");
            require(!forceTriggerButton || !forceTriggerButton->isDown(),
                    "Live View Manual Trigger button is not active");

            if (!failures.isEmpty()) {
                const QString messagePrefix =
                    verifyLiveViewTrigger ? QStringLiteral("Live View manual trigger verifier")
                                          : QStringLiteral("Direct DAQ manual trigger verifier");
                logMessage(messagePrefix + " aborted before output: " + failures.join("; "));
                finish(2);
                return;
            }

            if (verifyLiveViewTrigger) {
                workspaceStack->setCurrentWidget(liveWorkspacePage);
                liveNavButton->setChecked(true);
                headerTitleLabel->setText("/ Live View");
                headerStatusText->setText("Live View workspace");
                app.processEvents();
                waitForUi(350);
                require(forceTriggerButton && forceTriggerButton->isVisibleTo(this),
                        "LiveForceTriggerButton is visible before the verifier click");
                if (!failures.isEmpty()) {
                    logMessage("Live View manual trigger verifier aborted before output: " + failures.join("; "));
                    finish(2);
                    return;
                }
            }

            qInfo().noquote() << verifierPrefix << "INFO: Invoking" << triggerSource << "exactly once.";
            triggerInvoked = true;
            triggerButton->click();
            waitForUi(1200);

            const QString resultText = statusLabel->text().trimmed();
            qInfo().noquote() << verifierPrefix << "INFO: ResultStatusLabel=" << resultText;
            require(resultText == QStringLiteral("DAQ trigger sent."), "Manual trigger reports DAQ trigger sent");
            require(appState.daqAvailable && !appState.daqDisabled && !appState.daqFault,
                    "DAQ remains available after manual trigger");

            if (!failures.isEmpty()) {
                const QString messagePrefix =
                    verifyLiveViewTrigger ? QStringLiteral("Live View manual trigger verifier")
                                          : QStringLiteral("Direct DAQ manual trigger verifier");
                logMessage(messagePrefix + " failed after one approved output attempt: " + failures.join("; "));
                finish(3);
                return;
            }

            if (verifyLiveViewTrigger) {
                logMessage("Live View manual trigger verifier sent one approved Dev1/ao0 output.");
            } else {
                logMessage("Direct DAQ manual trigger verifier sent one approved Dev1/ao0 output.");
            }
            finish(0);
        });
    }
    int rc = 0;
    try {
        rc = app.exec();
    } catch (const std::exception& e) {
        logMessage(QString("Fatal exception: %1").arg(e.what()));
        rc = 1;
    } catch (...) {
        logMessage("Fatal unknown exception");
        rc = 1;
    }
    logMessage(QString("Event loop exited with code %1").arg(rc));
    return rc;
}
