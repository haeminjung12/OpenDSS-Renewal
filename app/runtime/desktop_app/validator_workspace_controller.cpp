#include "validator_workspace_controller.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtGui/QAction>
#include <QtGui/QImageReader>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include "background_task_registry.h"
#include "image_validation_dialog.h"

ValidatorWorkspaceController::ValidatorWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    wireValidatorAction();
    wireSequenceControls();
}

void ValidatorWorkspaceController::setSequenceUiRunning(bool running) const {
    if (deps_.seqStartBtn) deps_.seqStartBtn->setEnabled(!running);
    if (deps_.seqStopBtn) deps_.seqStopBtn->setEnabled(running);
    if (deps_.seqLoadBtn) deps_.seqLoadBtn->setEnabled(!running);
    if (deps_.pipelineWidget) deps_.pipelineWidget->setEnabled(!running);
    if (deps_.labviewWidget) deps_.labviewWidget->setEnabled(!running);
    if (deps_.detectWidget) deps_.detectWidget->setEnabled(!running);
    if (deps_.pipelineStartBtn && deps_.pipelineEnabled) {
        deps_.pipelineStartBtn->setEnabled(!running && !deps_.pipelineEnabled->load());
    }
    if (deps_.pipelineStopBtn && deps_.pipelineEnabled) {
        deps_.pipelineStopBtn->setEnabled(!running && deps_.pipelineEnabled->load());
    }
    if (deps_.viewerOnly && !*deps_.viewerOnly) {
        if (deps_.startBtn) deps_.startBtn->setEnabled(!running);
        if (deps_.stopBtn) deps_.stopBtn->setEnabled(!running);
        if (deps_.reconnectBtn) deps_.reconnectBtn->setEnabled(!running);
        if (deps_.applyBtn) deps_.applyBtn->setEnabled(!running);
    }
}

void ValidatorWorkspaceController::updateSequenceStatus(const QString& text) const {
    if (!deps_.seqStatusLabel) return;
    QPointer<QLabel> label(deps_.seqStatusLabel);
    QMetaObject::invokeMethod(
        deps_.seqStatusLabel,
        [label, text]() {
            if (!label.isNull()) {
                label->setText(text);
            }
        },
        Qt::QueuedConnection);
}

void ValidatorWorkspaceController::stopSequenceTest() {
    if (!deps_.sequenceStop || !deps_.sequenceThread || !deps_.sequenceRunning) return;
    deps_.sequenceStop->store(true);
    if (deps_.sequenceThread->joinable()) {
        deps_.sequenceThread->join();
    }
    if (deps_.sequenceRunning->load()) {
        deps_.sequenceRunning->store(false);
        setSequenceUiRunning(false);
        if (deps_.seqStatusLabel) deps_.seqStatusLabel->setText("Sequence stopped.");
        if (deps_.statusLabel) deps_.statusLabel->setText("Sequence test stopped.");
    }
}

QString ValidatorWorkspaceController::formatBytes(size_t bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QString("%1 MB").arg(mb, 0, 'f', 1);
}

QStringList ValidatorWorkspaceController::collectSequenceFiles(const QString& dirPath) {
    QDir dir(dirPath);
    QStringList filters;
    filters << "*.tif" << "*.tiff" << "*.TIF" << "*.TIFF"
            << "*.png" << "*.PNG" << "*.jpg" << "*.JPG"
            << "*.jpeg" << "*.JPEG" << "*.bmp" << "*.BMP";
    return dir.entryList(filters, QDir::Files, QDir::Name);
}

QString ValidatorWorkspaceController::defaultValidationOutput() const {
    const QString runName = "validation_gui_image_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return QDir(deps_.validationRunsRoot).filePath(runName);
}

QString ValidatorWorkspaceController::findPathUpwards(const QString& relativePath) const {
    QDir dir(deps_.appDir);
    for (int i = 0; i < 10; ++i) {
        const QString candidate = dir.filePath(relativePath);
        if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
        if (!dir.cdUp()) break;
    }
    return QString();
}

void ValidatorWorkspaceController::openImageValidationDialog() {
    if (!deps_.parentWindow || !deps_.onnxEdit || !deps_.metaEdit || !deps_.resolveAppRelative) return;

    QSettings settings;
    const QString trainerPythonPath = findPathUpwards("training/python");
    const QString datasetPath =
        settings.value("validator/imageDataset", deps_.preparedDatasetPath).toString();
    const QString validationOutput =
        settings.value("validator/outputFolder", defaultValidationOutput()).toString();

    ImageValidationDialog dialog(deps_.parentWindow,
                                 settings.value("validator/pythonExecutable", "python").toString(),
                                 deps_.resolveAppRelative(deps_.onnxEdit->text().trimmed()),
                                 deps_.resolveAppRelative(deps_.metaEdit->text().trimmed()),
                                 datasetPath,
                                 validationOutput,
                                 trainerPythonPath);
    dialog.exec();
    if (deps_.pythonStatusItem) {
        deps_.pythonStatusItem->setText("Python: validator configured");
    }
}

void ValidatorWorkspaceController::wireValidatorAction() {
    if (!deps_.imageValidationAction) return;
    connect(deps_.imageValidationAction, &QAction::triggered, this, [this]() {
        openImageValidationDialog();
    });
}

void ValidatorWorkspaceController::wireSequenceControls() {
    if (deps_.seqBrowseBtn && deps_.seqFolderEdit) {
        connect(deps_.seqBrowseBtn, &QPushButton::clicked, this, [this]() {
            const QString dir =
                QFileDialog::getExistingDirectory(deps_.parentWindow, "Select sequence folder", deps_.seqFolderEdit->text());
            if (!dir.isEmpty()) deps_.seqFolderEdit->setText(dir);
        });
    }

    if (deps_.seqLoadBtn && deps_.seqFolderEdit && deps_.seqStatusLabel && deps_.sequenceRunning &&
        deps_.sequenceLoading && deps_.sequenceStop && deps_.sequenceMutex && deps_.sequenceFrames &&
        deps_.backgroundTasks) {
        connect(deps_.seqLoadBtn, &QPushButton::clicked, this, [this]() {
            if (deps_.sequenceRunning->load() || deps_.sequenceLoading->load()) return;
            deps_.sequenceStop->store(false);

            const QString dirPath = deps_.seqFolderEdit->text().trimmed();
            const QDir dir(dirPath);
            if (!dir.exists()) {
                deps_.seqStatusLabel->setText("Sequence folder not found.");
                return;
            }

            const QStringList files = collectSequenceFiles(dirPath);
            if (files.isEmpty()) {
                deps_.seqStatusLabel->setText("No images found in folder.");
                return;
            }

            deps_.seqLoadBtn->setEnabled(false);
            if (deps_.seqStartBtn) deps_.seqStartBtn->setEnabled(false);
            updateSequenceStatus(QString("Loading %1 frames...").arg(files.size()));
            deps_.sequenceLoading->store(true);

            QPointer<QLabel> seqStatusLabelPtr(deps_.seqStatusLabel);
            QPointer<QPushButton> seqLoadBtnPtr(deps_.seqLoadBtn);
            QPointer<QPushButton> seqStartBtnPtr(deps_.seqStartBtn);

            deps_.backgroundTasks->launch(
                "sequence-load",
                [this, dirPath, files, seqStatusLabelPtr, seqLoadBtnPtr, seqStartBtnPtr](
                    const BackgroundTaskRegistry::StopFlag& stop) {
                    auto frames = std::make_shared<std::vector<SequenceFrame>>();
                    frames->reserve(files.size());
                    size_t totalBytes = 0;
                    int loaded = 0;
                    for (const QString& rel : files) {
                        if (deps_.sequenceStop->load() || stop->load()) break;
                        const QString absPath = QDir(dirPath).absoluteFilePath(rel);
                        QImageReader reader(absPath);
                        reader.setAutoTransform(true);
                        QImage img = reader.read();
                        if (img.isNull()) {
                            continue;
                        }
                        if (img.format() != QImage::Format_Grayscale8) {
                            img = img.convertToFormat(QImage::Format_Grayscale8);
                        }
                        totalBytes += static_cast<size_t>(img.sizeInBytes());
                        frames->push_back({img, absPath});
                        loaded++;
                        if (loaded % 100 == 0 && !seqStatusLabelPtr.isNull()) {
                            const QString progress = QString("Loaded %1 / %2 frames...").arg(loaded).arg(files.size());
                            QMetaObject::invokeMethod(
                                seqStatusLabelPtr,
                                [seqStatusLabelPtr, progress]() {
                                    if (!seqStatusLabelPtr.isNull()) {
                                        seqStatusLabelPtr->setText(progress);
                                    }
                                },
                                Qt::QueuedConnection);
                        }
                    }

                    const bool canceled = deps_.sequenceStop->load() || stop->load();
                    if (!canceled) {
                        QMutexLocker lock(deps_.sequenceMutex);
                        *deps_.sequenceFrames = frames;
                    }

                    const QString status = canceled
                                               ? QString("Sequence load canceled.")
                                               : QString("Loaded %1 frames (%2).")
                                                     .arg(frames->size())
                                                     .arg(formatBytes(totalBytes));
                    deps_.sequenceLoading->store(false);
                    if (!seqStatusLabelPtr.isNull()) {
                        QMetaObject::invokeMethod(
                            seqStatusLabelPtr,
                            [seqStatusLabelPtr, status]() {
                                if (!seqStatusLabelPtr.isNull()) {
                                    seqStatusLabelPtr->setText(status);
                                }
                            },
                            Qt::QueuedConnection);
                    }
                    if (!seqLoadBtnPtr.isNull()) {
                        QMetaObject::invokeMethod(
                            seqLoadBtnPtr,
                            [seqLoadBtnPtr, seqStartBtnPtr, canceled]() {
                                if (!seqLoadBtnPtr.isNull()) seqLoadBtnPtr->setEnabled(true);
                                if (!seqStartBtnPtr.isNull()) seqStartBtnPtr->setEnabled(!canceled);
                            },
                            Qt::QueuedConnection);
                    }
                });
        });
    }

    if (deps_.seqStopBtn) {
        connect(deps_.seqStopBtn, &QPushButton::clicked, this, [this]() {
            stopSequenceTest();
        });
    }
}
