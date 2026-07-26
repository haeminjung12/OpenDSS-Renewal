#include "sequence_test_controller.h"

#include "../sequence/sequence_manifest_v2.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <utility>

namespace {

using namespace desktop_app::v2;
using namespace desktop_app::v2::sequence_test;

QString localManifestPath(const QUrl& url, QString* error) {
    if (!url.isValid() || !url.isLocalFile() || url.hasQuery() ||
        url.hasFragment()) {
        *error = QStringLiteral("Sequence manifest must be a local URL.");
        return {};
    }
    const QString path = url.toLocalFile();
    if (path.trimmed().isEmpty()) {
        *error = QStringLiteral("Sequence manifest must contain a local path.");
        return {};
    }
    const QFileInfo info(path);
    if (info.fileName() != QStringLiteral("sequence.json")) {
        *error = QStringLiteral("Select an OpenDSS Image Sequence sequence.json.");
        return {};
    }
    if (!info.isFile() || !info.isReadable()) {
        *error = QStringLiteral("sequence.json is not a readable file.");
        return {};
    }
    return info.canonicalFilePath();
}

QString framePath(const QString& sequenceRoot,
                  const QString& pattern,
                  qint64 oneBasedFrame) {
    if (pattern == QStringLiteral("frames/frame_%08d.tif")) {
        return QDir(sequenceRoot)
            .filePath(QStringLiteral("frames/frame_%1.tif")
                          .arg(oneBasedFrame, 8, 10, QLatin1Char('0')));
    }
    if (pattern == QStringLiteral("%06d.tiff")) {
        return QDir(sequenceRoot)
            .filePath(QStringLiteral("%1.tiff")
                          .arg(oneBasedFrame - 1, 6, 10, QLatin1Char('0')));
    }
    return {};
}

QByteArray readFileBytes(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Could not read sequence.json.");
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
        *error = QStringLiteral("sequence.json is empty.");
    return bytes;
}

bool calculateBufferBytes(qint64 frameCount,
                          int width,
                          int height,
                          qulonglong* bytes,
                          QString* error) {
    if (frameCount <= 0 || width <= 0 || height <= 0) {
        *error = QStringLiteral("Sequence frame facts are invalid.");
        return false;
    }
    const qulonglong unsignedWidth = static_cast<qulonglong>(width);
    if (unsignedWidth > (std::numeric_limits<qulonglong>::max)() - 3) {
        *error = QStringLiteral("Sequence buffer size is too large.");
        return false;
    }
    const qulonglong rowBytes = (unsignedWidth + 3) & ~qulonglong{3};
    const qulonglong unsignedHeight = static_cast<qulonglong>(height);
    const qulonglong unsignedFrames = static_cast<qulonglong>(frameCount);
    const qulonglong maximum = (std::numeric_limits<qulonglong>::max)();
    if (rowBytes > maximum / unsignedHeight ||
        rowBytes * unsignedHeight > maximum / unsignedFrames) {
        *error = QStringLiteral("Sequence buffer size is too large.");
        return false;
    }
    *bytes = rowBytes * unsignedHeight * unsignedFrames;
    return true;
}

bool validActiveModel(const run::ModelSnapshot& model) {
    if (model.id.trimmed().isEmpty() || model.name.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$"))
             .match(model.sha256)
             .hasMatch() ||
        (model.classes.size() != 2 && model.classes.size() != 3)) {
        return false;
    }
    QSet<QString> ids;
    for (const auto& value : model.classes) {
        if (value.id.trimmed().isEmpty() || value.name.trimmed().isEmpty() ||
            ids.contains(value.id)) {
            return false;
        }
        ids.insert(value.id);
    }
    return true;
}

QString writableOutputRoot(const QUrl& url, QString* error) {
    if (!url.isValid() || !url.isLocalFile() || url.hasQuery() ||
        url.hasFragment() || url.toLocalFile().trimmed().isEmpty()) {
        *error = QStringLiteral("Select a local output folder.");
        return {};
    }
    const QFileInfo info(url.toLocalFile());
    if (!info.isDir() || !info.isWritable()) {
        *error = QStringLiteral("Output folder is not a writable directory.");
        return {};
    }
    QTemporaryFile probe(
        QDir(info.absoluteFilePath())
            .filePath(QStringLiteral(".opendss-sequence-test-write-XXXXXX")));
    if (!probe.open()) {
        *error = QStringLiteral("Output folder is not writable.");
        return {};
    }
    return info.canonicalFilePath();
}

} // namespace

namespace desktop_app::v2::sequence_test {

SequenceTestController::SequenceTestController(
    SequenceTestService& service,
    ActiveModelSnapshotProvider activeModelProvider,
    ResultsRefreshCallback resultsRefresh,
    StorageRootProvider storageRootProvider,
    AvailableMemoryProvider availableMemoryProvider,
    DaqReadinessGate daqReadinessProvider,
    QJsonObject detectorSettings,
    QJsonObject cropSettings,
    QJsonObject timingSettings,
    QString opendssVersion,
    QObject* parent)
    : QObject(parent),
      service_(service),
      activeModelProvider_(std::move(activeModelProvider)),
      resultsRefresh_(std::move(resultsRefresh)),
      availableMemoryProvider_(std::move(availableMemoryProvider)),
      daqReadinessProvider_(std::move(daqReadinessProvider)),
      detectorSettings_(std::move(detectorSettings)),
      cropSettings_(std::move(cropSettings)),
      timingSettings_(std::move(timingSettings)),
      opendssVersion_(std::move(opendssVersion)) {
    try {
        if (storageRootProvider) {
            const QString root = storageRootProvider().trimmed();
            if (!root.isEmpty())
                outputFolderUrl_ = QUrl::fromLocalFile(QDir::cleanPath(root));
        }
    } catch (const std::exception& exception) {
        errorMessage_ =
            QStringLiteral("Default output folder is unavailable: %1")
                .arg(exception.what());
    } catch (...) {
        errorMessage_ = QStringLiteral("Default output folder is unavailable.");
    }
    refreshAvailableMemory();
    refreshActiveModel();
    updatePreflight();
}

SequenceTestController::~SequenceTestController() {
    shuttingDown_.store(true, std::memory_order_release);
    cancelLoad_.store(true, std::memory_order_release);
    stopRequested_.store(true, std::memory_order_release);
    if (stopWorker_.joinable())
        stopWorker_.join();
    service_.requestStop();
    if (loadWorker_.joinable())
        loadWorker_.join();
    if (runWorker_.joinable())
        runWorker_.join();
}

QString SequenceTestController::presentation() const {
    return presentation_;
}

QString SequenceTestController::errorMessage() const {
    return errorMessage_;
}

bool SequenceTestController::canLoadToMemory() const {
    return canLoadToMemory_;
}

bool SequenceTestController::canStart() const {
    return canStart_;
}

QString SequenceTestController::activeModelName() const {
    return activeModelName_;
}

bool SequenceTestController::activeModelReady() const {
    return activeModelReady_;
}

QUrl SequenceTestController::sourceManifestUrl() const {
    return sourceManifestUrl_;
}

QString SequenceTestController::sequenceName() const {
    return sequenceName_;
}

QUrl SequenceTestController::sequenceFolderUrl() const {
    return sequenceFolderUrl_;
}

QString SequenceTestController::sequencePath() const {
    return sequencePath_;
}

qint64 SequenceTestController::frameCount() const {
    return frameCount_;
}

double SequenceTestController::recordedFps() const {
    return recordedFps_;
}

QUrl SequenceTestController::previewUrl() const {
    return previewUrl_;
}

QString SequenceTestController::sequenceValidation() const {
    return sequenceValidation_;
}

qulonglong SequenceTestController::availableMemoryBytes() const {
    return availableMemoryBytes_;
}

qulonglong SequenceTestController::bufferBytes() const {
    return bufferBytes_;
}

bool SequenceTestController::memoryReady() const {
    return memoryReady_;
}

QString SequenceTestController::loadStatus() const {
    return loadStatus_;
}

double SequenceTestController::requestedProcessingFps() const {
    return requestedProcessingFps_;
}

void SequenceTestController::setRequestedProcessingFps(double value) {
    if (operationActive() || requestedProcessingFps_ == value)
        return;
    requestedProcessingFps_ = value;
    updatePreflight();
    emit changed();
}

double SequenceTestController::achievedProcessingFps() const {
    return achievedProcessingFps_;
}

qint64 SequenceTestController::processedFrames() const {
    return processedFrames_;
}

qint64 SequenceTestController::totalFrames() const {
    return totalFrames_;
}

double SequenceTestController::progress() const {
    return totalFrames_ > 0
               ? static_cast<double>(processedFrames_) /
                     static_cast<double>(totalFrames_)
               : 0.0;
}

QString SequenceTestController::outputStatus() const {
    return outputStatus_;
}

bool SequenceTestController::triggerEveryDroplet() const {
    return triggerEveryDroplet_;
}

void SequenceTestController::setTriggerEveryDroplet(bool value) {
    if (operationActive() || triggerEveryDroplet_ == value)
        return;
    triggerEveryDroplet_ = value;
    updatePreflight();
    emit changed();
}

QVariantList SequenceTestController::hitClassModel() const {
    return hitClassModel_;
}

QString SequenceTestController::selectedHitClassId() const {
    return selectedHitClassId_;
}

void SequenceTestController::setSelectedHitClassId(const QString& value) {
    const QString id = value.trimmed();
    if (operationActive() || selectedHitClassId_ == id)
        return;
    selectedHitClassId_ = id;
    updatePreflight();
    emit changed();
}

bool SequenceTestController::physicalDaqOutputEnabled() const {
    return physicalDaqOutputEnabled_;
}

void SequenceTestController::setPhysicalDaqOutputEnabled(bool value) {
    if (operationActive() || physicalDaqOutputEnabled_ == value)
        return;
    physicalDaqOutputEnabled_ = value;
    physicalDaqWarning_.clear();
    updatePreflight();
    emit changed();
}

QUrl SequenceTestController::outputFolderUrl() const {
    return outputFolderUrl_;
}

void SequenceTestController::setOutputFolderUrl(const QUrl& value) {
    if (operationActive() || outputFolderUrl_ == value)
        return;
    outputFolderUrl_ = value;
    updatePreflight();
    emit changed();
}

QUrl SequenceTestController::runFolderUrl() const { return runFolderUrl_; }

QUrl SequenceTestController::runSummaryUrl() const { return runSummaryUrl_; }

bool SequenceTestController::selectSequence(const QUrl& sequenceJson) {
    if (inputLocked())
        return false;

    ++selectionGeneration_;
    clearSelectedSequence();
    sourceManifestUrl_ = sequenceJson;

    QString error;
    const QString manifestPath = localManifestPath(sequenceJson, &error);
    const QByteArray manifestBytes =
        error.isEmpty() ? readFileBytes(manifestPath, &error) : QByteArray{};
    auto manifest = error.isEmpty()
                        ? sequence::SequenceManifestV2::load(manifestPath, &error)
                        : std::nullopt;
    if (manifest) {
        const QByteArray confirmation = readFileBytes(manifestPath, &error);
        if (!error.isEmpty() || confirmation != manifestBytes) {
            error =
                QStringLiteral("sequence.json changed while it was being selected.");
            manifest.reset();
        }
    }
    if (manifest && manifest->data().status != QStringLiteral("completed")) {
        error = QStringLiteral("Sequence status must be completed.");
        manifest.reset();
    }
    if (manifest && manifest->data().frameCount <= 0) {
        error = QStringLiteral("Sequence has no frames.");
        manifest.reset();
    }

    qulonglong requiredBytes = 0;
    if (manifest &&
        !calculateBufferBytes(manifest->data().frameCount,
                              manifest->data().imageWidth,
                              manifest->data().imageHeight,
                              &requiredBytes,
                              &error)) {
        manifest.reset();
    }

    QString firstFramePath;
    QImage firstFrame;
    if (manifest) {
        const QString root = QFileInfo(manifestPath).absolutePath();
        const QFileInfo firstFrameInfo(
            framePath(root, manifest->data().frameFilenamePattern, 1));
        if (!firstFrameInfo.isFile() || !firstFrameInfo.isReadable()) {
            error =
                QStringLiteral("The first Sequence frame is missing or unreadable.");
            manifest.reset();
        } else {
            firstFramePath = firstFrameInfo.canonicalFilePath();
        }
    }
    if (manifest) {
        QImageReader reader(firstFramePath);
        firstFrame = reader.read();
        if (firstFrame.isNull()) {
            error = QStringLiteral("The first Sequence frame could not be decoded.");
            manifest.reset();
        } else if (firstFrame.width() != manifest->data().imageWidth ||
                   firstFrame.height() != manifest->data().imageHeight) {
            error =
                QStringLiteral("The first Sequence frame dimensions do not match sequence.json.");
            manifest.reset();
        }
    }

    if (!manifest) {
        sequenceValidation_ =
            error.isEmpty() ? QStringLiteral("Unsupported OpenDSS v2 sequence")
                            : error;
        errorMessage_ = sequenceValidation_;
        loadStatus_ = QStringLiteral("Not loaded");
        presentation_ = QStringLiteral("error");
        updatePreflight(true);
        emit changed();
        return false;
    }

    const auto& data = manifest->data();
    sourceManifestPath_ = manifestPath;
    selectedManifestBytes_ = manifestBytes;
    sourceManifestUrl_ = QUrl::fromLocalFile(manifestPath);
    sequenceName_ = data.name;
    sequenceId_ = data.sequenceId;
    experimentType_ = data.experimentType;
    notes_ = data.notes;
    const QString root = QFileInfo(manifestPath).absolutePath();
    sequenceFolderUrl_ = QUrl::fromLocalFile(root);
    sequencePath_ = QDir::cleanPath(manifestPath);
    frameCount_ = data.frameCount;
    frameFilenamePattern_ = data.frameFilenamePattern;
    imageWidth_ = data.imageWidth;
    imageHeight_ = data.imageHeight;
    hitBoundary_ = {imageHeight_ / 2.0, run::HitSide::PositiveY,
                    imageWidth_, imageHeight_};
    recordedFps_ = data.nominalFps;
    requestedProcessingFps_ = data.nominalFps;
    cameraSettings_ = data.cameraSettings;
    previewUrl_ = QUrl::fromLocalFile(firstFramePath);
    sequenceValidation_ = QStringLiteral("Valid");
    bufferBytes_ = requiredBytes;
    loadStatus_ = QStringLiteral("Not loaded");
    presentation_ = QStringLiteral("unavailable");
    errorMessage_.clear();
    refreshAvailableMemory();
    refreshActiveModel();
    updatePreflight();
    emit changed();
    return true;
}

bool SequenceTestController::loadToMemory() {
    if (inputLocked())
        return false;
    if (loadWorker_.joinable())
        loadWorker_.join();

    refreshAvailableMemory();
    updatePreflight();
    if (!canLoadToMemory_) {
        presentation_ = QStringLiteral("error");
        loadStatus_ = QStringLiteral("Error");
        emit changed();
        return false;
    }

    const quint64 generation = selectionGeneration_;
    const QString manifestPath = sourceManifestPath_;
    const QString sequenceId = sequenceId_;
    const QString root = QFileInfo(manifestPath).absolutePath();
    const qint64 frameCount = frameCount_;
    const QString frameFilenamePattern = frameFilenamePattern_;
    const int width = imageWidth_;
    const int height = imageHeight_;
    const QByteArray selectedManifestBytes = selectedManifestBytes_;

    clearLoadedSequence();
    cancelLoad_.store(false, std::memory_order_release);
    loadStatus_ = QStringLiteral("Loading");
    presentation_ = QStringLiteral("unavailable");
    errorMessage_.clear();
    canLoadToMemory_ = false;
    emit changed();

    try {
        loadWorker_ = std::thread(
            [this, generation, manifestPath, sequenceId, root, frameCount,
             frameFilenamePattern, width, height, selectedManifestBytes] {
                std::shared_ptr<const LoadedSequence> result;
                qulonglong actualBytes = 0;
                QByteArray manifestBytes;
                QString error;
                try {
                    manifestBytes = readFileBytes(manifestPath, &error);
                    QString manifestError;
                    const auto manifest =
                        error.isEmpty()
                            ? sequence::SequenceManifestV2::load(
                                  manifestPath, &manifestError)
                            : std::nullopt;
                    const QByteArray confirmedManifestBytes =
                        manifest ? readFileBytes(manifestPath, &manifestError)
                                 : QByteArray{};
                    if (!manifest || !manifestError.isEmpty() ||
                        manifestBytes != selectedManifestBytes ||
                        confirmedManifestBytes != selectedManifestBytes) {
                        error =
                            QStringLiteral("sequence.json changed after selection. Select the Sequence again.");
                    } else if (manifest->data().sequenceId != sequenceId ||
                               manifest->data().frameCount != frameCount ||
                               manifest->data().frameFilenamePattern !=
                                   frameFilenamePattern ||
                               manifest->data().imageWidth != width ||
                               manifest->data().imageHeight != height) {
                        error =
                            QStringLiteral("sequence.json facts no longer match the selected Sequence.");
                    }
                    std::shared_ptr<LoadedSequence> loaded;
                    if (error.isEmpty()) {
                        loaded = std::make_shared<LoadedSequence>();
                        loaded->sourceSequenceJson =
                            QFileInfo(manifestPath).canonicalFilePath();
                        loaded->sequenceId = sequenceId;
                        loaded->frames.reserve(
                            static_cast<qsizetype>(frameCount));
                    }
                    for (qint64 index = 1;
                         error.isEmpty() && index <= frameCount;
                         ++index) {
                        if (cancelLoad_.load(std::memory_order_acquire) ||
                            shuttingDown_.load(std::memory_order_acquire)) {
                            return;
                        }
                        QImageReader reader(
                            framePath(root, frameFilenamePattern, index));
                        QImage image = reader.read();
                        if (image.isNull()) {
                            error =
                                QStringLiteral("Sequence frame %1 could not be decoded.")
                                    .arg(index);
                            break;
                        }
                        if (image.width() != width || image.height() != height) {
                            error =
                                QStringLiteral("Sequence frame %1 dimensions do not match sequence.json.")
                                    .arg(index);
                            break;
                        }
                        image = image.convertToFormat(QImage::Format_Grayscale8);
                        if (image.isNull()) {
                            error =
                                QStringLiteral("Sequence frame %1 could not be prepared.")
                                    .arg(index);
                            break;
                        }
                        const qulonglong size =
                            static_cast<qulonglong>(image.sizeInBytes());
                        if (actualBytes >
                            (std::numeric_limits<qulonglong>::max)() - size) {
                            error =
                                QStringLiteral("Sequence buffer size is too large.");
                            break;
                        }
                        actualBytes += size;
                        loaded->frames.push_back({index, std::move(image)});
                    }
                    if (error.isEmpty() &&
                        readFileBytes(manifestPath, &error) !=
                            selectedManifestBytes) {
                        error =
                            QStringLiteral("sequence.json changed while the Sequence was loading.");
                    }
                    if (error.isEmpty() &&
                        loaded && loaded->frames.size() == frameCount) {
                        result = std::move(loaded);
                    }
                } catch (const std::exception& exception) {
                    error =
                        QStringLiteral("Sequence memory loading failed: %1")
                            .arg(exception.what());
                } catch (...) {
                    error = QStringLiteral("Sequence memory loading failed.");
                }
                if (shuttingDown_.load(std::memory_order_acquire))
                    return;
                QMetaObject::invokeMethod(
                    this,
                    [this, generation, result = std::move(result), actualBytes,
                     manifestBytes, error] {
                        finishLoad(generation, result, actualBytes,
                                   manifestBytes, error);
                    },
                    Qt::QueuedConnection);
            });
    } catch (const std::exception& exception) {
        loadStatus_ = QStringLiteral("Error");
        presentation_ = QStringLiteral("error");
        errorMessage_ =
            QStringLiteral("Sequence memory loading could not start: %1")
                .arg(exception.what());
        updatePreflight(true);
        emit changed();
        return false;
    }
    return true;
}

bool SequenceTestController::start() {
    if (inputLocked())
        return false;
    if (runWorker_.joinable())
        runWorker_.join();
    if (stopWorker_.joinable())
        stopWorker_.join();

    refreshActiveModel();
    refreshAvailableMemory();
    updatePreflight();
    if (!canStart_) {
        presentation_ = QStringLiteral("error");
        emit changed();
        return false;
    }

    QString outputError;
    const QString outputRoot =
        writableOutputRoot(outputFolderUrl_, &outputError);
    if (outputRoot.isEmpty()) {
        errorMessage_ = outputError;
        presentation_ = QStringLiteral("error");
        updatePreflight(true);
        emit changed();
        return false;
    }

    SequenceTestRequest request;
    request.sequenceJson = sourceManifestPath_;
    request.frozenManifestBytes = frozenManifestBytes_;
    request.loadedSequence = loadedSequence_;
    request.outputRoot = outputRoot;
    request.runName =
        QStringLiteral("Sequence Test - %1").arg(sequenceName_);
    request.experimentType = experimentType_;
    request.notes = notes_;
    request.triggerMode = triggerEveryDroplet_
                              ? run::TriggerMode::EveryDroplet
                              : run::TriggerMode::ClassBased;
    if (!triggerEveryDroplet_)
        request.hitClassId = selectedHitClassId_;
    request.hitBoundary = hitBoundary_;
    request.physicalDaqOutputEnabled = physicalDaqOutputEnabled_;
    request.requestedProcessingFps = requestedProcessingFps_;
    request.useActiveModel = activeModelReady_;
    request.opendssVersion = opendssVersion_;
    request.detectorSettings = detectorSettings_;
    request.cropSettings = cropSettings_;
    request.timingSettings = timingSettings_;
    request.cameraSettings = cameraSettings_;
    request.daqSettings = {
        {QStringLiteral("physical_output_enabled"),
         physicalDaqOutputEnabled_}};

    const quint64 generation = ++runGeneration_;
    request.progressCallback =
        [this, generation](const SequenceTestProgress& value) {
            if (stopRequested_.load(std::memory_order_acquire))
                service_.requestStop();
            postProgress(generation, value);
        };
    processedFrames_ = 0;
    totalFrames_ = frameCount_;
    achievedProcessingFps_ = 0.0;
    runFolderUrl_.clear();
    runSummaryUrl_.clear();
    outputStatus_ = QStringLiteral("Starting");
    presentation_ = QStringLiteral("starting");
    errorMessage_.clear();
    canStart_ = false;
    stopRequested_.store(false, std::memory_order_release);
    emit changed();

    try {
        runWorker_ = std::thread(
            [this, generation, request = std::move(request)] {
                QString error;
                QString runFolder;
                const bool succeeded =
                    service_.run(request, &error, &runFolder);
                if (shuttingDown_.load(std::memory_order_acquire))
                    return;
                QMetaObject::invokeMethod(
                    this,
                    [this, generation, succeeded, error, runFolder] {
                        finishRun(generation, succeeded, error, runFolder);
                    },
                    Qt::QueuedConnection);
            });
    } catch (const std::exception& exception) {
        presentation_ = QStringLiteral("error");
        outputStatus_ = QStringLiteral("Failed");
        errorMessage_ =
            QStringLiteral("Sequence Test could not start: %1")
                .arg(exception.what());
        updatePreflight(true);
        emit changed();
        return false;
    }
    return true;
}

bool SequenceTestController::stop() {
    if (!operationActive() ||
        presentation_ == QStringLiteral("stopping")) {
        return false;
    }
    if (stopWorker_.joinable())
        stopWorker_.join();

    stopRequested_.store(true, std::memory_order_release);
    presentation_ = QStringLiteral("stopping");
    outputStatus_ = QStringLiteral("Stopping");
    emit changed();
    try {
        stopWorker_ = std::thread([this] {
            service_.requestStop();
            if (shuttingDown_.load(std::memory_order_acquire))
                return;
            QMetaObject::invokeMethod(
                this,
                [this] { emit stopRequestAccepted(); },
                Qt::QueuedConnection);
        });
    } catch (const std::exception& exception) {
        presentation_ = QStringLiteral("error");
        outputStatus_ = QStringLiteral("Failed");
        errorMessage_ =
            QStringLiteral("Sequence Test stop could not be requested: %1")
                .arg(exception.what());
        emit changed();
        return false;
    }
    return true;
}

void SequenceTestController::refreshPreflight() {
    if (inputLocked())
        return;
    refreshAvailableMemory();
    refreshActiveModel();
    updatePreflight(presentation_ == QStringLiteral("error") &&
                    outputStatus_ == QStringLiteral("Failed"));
    emit changed();
}

void SequenceTestController::startAnotherTest() {
    if (operationActive())
        return;
    runFolderUrl_.clear();
    runSummaryUrl_.clear();
    processedFrames_ = 0;
    achievedProcessingFps_ = 0.0;
    outputStatus_ = QStringLiteral("Not started");
    errorMessage_.clear();
    presentation_ = memoryReady_ ? QStringLiteral("unavailable")
                                 : QStringLiteral("selected");
    updatePreflight();
    emit changed();
}

bool SequenceTestController::openRunFolder() {
    if (!runFolderUrl_.isLocalFile() || runFolderUrl_.hasQuery() ||
        runFolderUrl_.hasFragment() ||
        !QFileInfo(runFolderUrl_.toLocalFile()).isDir()) {
        errorMessage_ = QStringLiteral("Sequence Test Run folder is unavailable.");
        emit changed();
        return false;
    }
    emit openRunFolderRequested(runFolderUrl_);
    return true;
}

bool SequenceTestController::operationActive() const {
    return presentation_ == QStringLiteral("starting") ||
           presentation_ == QStringLiteral("running") ||
           presentation_ == QStringLiteral("stopping");
}

bool SequenceTestController::inputLocked() const {
    return operationActive() || loadStatus_ == QStringLiteral("Loading");
}

void SequenceTestController::clearSelectedSequence() {
    clearLoadedSequence();
    sourceManifestUrl_.clear();
    sourceManifestPath_.clear();
    selectedManifestBytes_.clear();
    sequenceId_.clear();
    sequenceName_.clear();
    experimentType_.clear();
    notes_.clear();
    sequenceFolderUrl_.clear();
    sequencePath_.clear();
    frameCount_ = 0;
    frameFilenamePattern_.clear();
    imageWidth_ = 0;
    imageHeight_ = 0;
    hitBoundary_ = {};
    recordedFps_ = 0.0;
    requestedProcessingFps_ = 0.0;
    cameraSettings_ = {};
    previewUrl_.clear();
    sequenceValidation_ = QStringLiteral("Not selected");
    bufferBytes_ = 0;
    loadStatus_ = QStringLiteral("Not loaded");
    canLoadToMemory_ = false;
    canStart_ = false;
    processedFrames_ = 0;
    totalFrames_ = 0;
    achievedProcessingFps_ = 0.0;
    outputStatus_ = QStringLiteral("Not started");
}

void SequenceTestController::clearLoadedSequence() {
    cancelLoad_.store(true, std::memory_order_release);
    loadedSequence_.reset();
    frozenManifestBytes_.clear();
    memoryReady_ = false;
}

void SequenceTestController::refreshAvailableMemory() {
    availableMemoryBytes_ = 0;
    if (!availableMemoryProvider_)
        return;
    try {
        availableMemoryBytes_ = availableMemoryProvider_();
    } catch (const std::exception& exception) {
        qWarning().noquote()
            << "Sequence Test available-memory provider failed:"
            << exception.what();
    } catch (...) {
        qWarning().noquote()
            << "Sequence Test available-memory provider failed.";
    }
}

void SequenceTestController::refreshActiveModel() {
    activeModel_.reset();
    activeModelName_.clear();
    activeModelReady_ = false;
    hitClassModel_.clear();
    if (!activeModelProvider_)
        return;

    QString error;
    try {
        auto model = activeModelProvider_(&error);
        if (model && validActiveModel(*model))
            activeModel_ = std::move(model);
    } catch (const std::exception& exception) {
        error =
            QStringLiteral("Active Model inspection failed: %1")
                .arg(exception.what());
    } catch (...) {
        error = QStringLiteral("Active Model inspection failed.");
    }
    if (!activeModel_)
        return;

    activeModelName_ = activeModel_->name;
    activeModelReady_ = true;
    for (const auto& value : activeModel_->classes) {
        hitClassModel_.push_back(
            QVariantMap{{QStringLiteral("id"), value.id},
                        {QStringLiteral("name"), value.name}});
    }
    const bool selectionExists =
        std::any_of(activeModel_->classes.cbegin(),
                    activeModel_->classes.cend(),
                    [&](const run::RunClassSnapshot& value) {
                        return value.id == selectedHitClassId_;
                    });
    if (!selectionExists)
        selectedHitClassId_.clear();
}

void SequenceTestController::updatePreflight(bool preserveFailure) {
    canLoadToMemory_ =
        !sourceManifestPath_.isEmpty() && !memoryReady_ &&
        loadStatus_ != QStringLiteral("Loading") && bufferBytes_ > 0 &&
        availableMemoryBytes_ >= bufferBytes_;
    canStart_ = false;

    QString blocker;
    if (sourceManifestPath_.isEmpty()) {
        blocker = sequenceValidation_ == QStringLiteral("Not selected")
                      ? QStringLiteral("Select an OpenDSS Image Sequence.")
                      : sequenceValidation_;
    } else if (!memoryReady_) {
        if (loadStatus_ == QStringLiteral("Error") && preserveFailure)
            blocker = errorMessage_;
        else if (availableMemoryBytes_ < bufferBytes_) {
            blocker =
                QStringLiteral("Sequence requires %1 bytes, but only %2 bytes are available.")
                    .arg(bufferBytes_)
                    .arg(availableMemoryBytes_);
        } else {
            blocker = QStringLiteral("Load the Sequence to memory.");
        }
    } else if (!std::isfinite(requestedProcessingFps_) ||
               requestedProcessingFps_ <= 0.0) {
        blocker =
            QStringLiteral("Processing FPS must be finite and positive.");
    } else if (hitBoundary_.imageWidth != imageWidth_ ||
               hitBoundary_.imageHeight != imageHeight_ ||
               !std::isfinite(hitBoundary_.boundaryY) ||
               hitBoundary_.boundaryY < 0.0 ||
               hitBoundary_.boundaryY >= hitBoundary_.imageHeight) {
        blocker =
            QStringLiteral("Hit boundary calibration does not match the Sequence.");
    } else if (!triggerEveryDroplet_ && !activeModelReady_) {
        blocker =
            QStringLiteral("Class-Based Sorting requires the Active Model.");
    } else if (!triggerEveryDroplet_ && selectedHitClassId_.isEmpty()) {
        blocker = QStringLiteral("Select a Hit Class.");
    } else if (!triggerEveryDroplet_ &&
               std::none_of(activeModel_->classes.cbegin(),
                            activeModel_->classes.cend(),
                            [&](const run::RunClassSnapshot& value) {
                                return value.id == selectedHitClassId_;
                            })) {
        blocker =
            QStringLiteral("Hit Class is not present in the Active Model.");
    } else if (physicalDaqOutputEnabled_) {
        QString daqError;
        bool daqReady = false;
        try {
            daqReady =
                daqReadinessProvider_ &&
                daqReadinessProvider_(&daqError);
        } catch (const std::exception& exception) {
            daqError =
                QStringLiteral("DAQ readiness check failed: %1")
                    .arg(exception.what());
        } catch (...) {
            daqError = QStringLiteral("DAQ readiness check failed.");
        }
        if (!daqReady) {
            physicalDaqOutputEnabled_ = false;
            const QString reason =
                daqError.trimmed().isEmpty()
                    ? QStringLiteral("DAQ is not ready.")
                    : daqError.trimmed();
            physicalDaqWarning_ =
                QStringLiteral("Physical DAQ output was disabled because: %1 "
                               "Processing-only Sequence Test remains available.")
                    .arg(reason);
        } else {
            physicalDaqWarning_.clear();
        }
    }
    if (blocker.isEmpty()) {
        QString outputError;
        if (writableOutputRoot(outputFolderUrl_, &outputError).isEmpty())
            blocker = outputError;
    }

    canStart_ = blocker.isEmpty() && !operationActive();
    if (!preserveFailure || errorMessage_.isEmpty())
        errorMessage_ =
            blocker.isEmpty() ? physicalDaqWarning_ : blocker;
    if (!inputLocked() && presentation_ != QStringLiteral("completed") &&
        presentation_ != QStringLiteral("interrupted") &&
        !(preserveFailure && presentation_ == QStringLiteral("error"))) {
        presentation_ =
            sourceManifestPath_.isEmpty()
                ? QStringLiteral("empty")
                : canStart_ ? QStringLiteral("ready")
                            : QStringLiteral("unavailable");
    }
}

void SequenceTestController::postProgress(
    quint64 generation,
    const SequenceTestProgress& value) {
    QMetaObject::invokeMethod(
        this,
        [this, generation, value] {
            if (generation != runGeneration_ ||
                shuttingDown_.load(std::memory_order_acquire)) {
                return;
            }
            processedFrames_ = value.processedFrames;
            totalFrames_ = value.totalFrames;
            achievedProcessingFps_ = value.achievedProcessingFps;
            if (presentation_ == QStringLiteral("starting")) {
                presentation_ = QStringLiteral("running");
                outputStatus_ = QStringLiteral("Running");
            }
            emit changed();
        },
        Qt::QueuedConnection);
}

void SequenceTestController::finishLoad(
    quint64 generation,
    std::shared_ptr<const LoadedSequence> loaded,
    qulonglong actualBytes,
    const QByteArray& manifestBytes,
    const QString& error) {
    if (generation != selectionGeneration_ ||
        shuttingDown_.load(std::memory_order_acquire)) {
        return;
    }
    if (!loaded) {
        clearLoadedSequence();
        loadStatus_ = QStringLiteral("Error");
        presentation_ = QStringLiteral("error");
        errorMessage_ =
            error.isEmpty() ? QStringLiteral("Sequence memory loading failed.")
                            : error;
        updatePreflight(true);
        emit changed();
        return;
    }

    loadedSequence_ = std::move(loaded);
    frozenManifestBytes_ = manifestBytes;
    memoryReady_ = true;
    bufferBytes_ = actualBytes;
    loadStatus_ = QStringLiteral("Ready");
    presentation_ = QStringLiteral("unavailable");
    errorMessage_.clear();
    updatePreflight();
    emit changed();
}

void SequenceTestController::finishRun(quint64 generation,
                                       bool succeeded,
                                       const QString& error,
                                       const QString& runFolder) {
    if (generation != runGeneration_ ||
        shuttingDown_.load(std::memory_order_acquire)) {
        return;
    }
    const bool stopped =
        stopRequested_.exchange(false, std::memory_order_acq_rel);
    runFolderUrl_ =
        runFolder.isEmpty() ? QUrl{} : QUrl::fromLocalFile(runFolder);
    const QString summaryPath =
        QDir(runFolder).filePath(QStringLiteral("run_summary.json"));
    runSummaryUrl_ = QFileInfo(summaryPath).isFile()
                         ? QUrl::fromLocalFile(summaryPath)
                         : QUrl{};
    if (succeeded) {
        presentation_ = stopped ? QStringLiteral("interrupted")
                                : QStringLiteral("completed");
        outputStatus_ =
            stopped ? QStringLiteral("Interrupted")
                    : QStringLiteral("Completed");
        errorMessage_.clear();
    } else {
        presentation_ = QStringLiteral("error");
        outputStatus_ = QStringLiteral("Failed");
        errorMessage_ =
            error.isEmpty() ? QStringLiteral("Sequence Test failed.") : error;
    }
    if (resultsRefresh_) {
        try {
            resultsRefresh_();
        } catch (const std::exception& exception) {
            qWarning().noquote()
                << "Sequence Test Results refresh failed:"
                << exception.what();
        } catch (...) {
            qWarning().noquote()
                << "Sequence Test Results refresh failed.";
        }
    }
    canStart_ = false;
    emit changed();
}

} // namespace desktop_app::v2::sequence_test
