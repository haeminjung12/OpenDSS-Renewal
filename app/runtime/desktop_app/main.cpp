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
#include <QSemaphore>
#include <QScrollArea>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStandardPaths>
#include <windows.h>
#include <dbghelp.h>
#include <algorithm>
#include <array>
#include <functional>
#include <atomic>
#include <exception>
#include <csignal>
#include <cstdio>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <utility>
#include <chrono>
#include <cmath>
#include <string>
#include <iostream>
#include <opencv2/core.hpp>
#include "app_state.h"
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
#include "log_teebuf.h"
#include "frame_types.h"
#include "background_task_registry.h"
#include "camera_worker.h"
#include "../dataset_capture_session.h"
#include "../cli_runner.h"

#pragma comment(lib, "Dbghelp.lib")

namespace {
QMutex gLogMutex;
QFile gLogFile;
QString gLogPath;
std::atomic<bool> gCrashHandled(false);
void logMessage(const QString& msg);
void logMessageNoPrune(const QString& msg);
void installLogTees();

struct SequenceFrame {
    QImage image;
    QString path;
};

struct AppOptions {
    bool testMode = false;
    bool mockCamera = false;
    bool verifyCameraWorkspace = false;
    bool verifyDaqSettings = false;
    bool noDaq = false;
    bool noStartupPrompts = false;
    QString datasetBuilderReviewPath;
    QString initialWorkspace;
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
    explicit WheelEventForwarder(QWidget* target, QObject* parent = nullptr)
        : QObject(parent), target_(target) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Wheel && target_ && target_->isVisible()) {
            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            const QPointF targetPos = target_->mapFromGlobal(wheelEvent->globalPosition().toPoint());
            QWheelEvent forwardedEvent(targetPos,
                                       wheelEvent->globalPosition(),
                                       wheelEvent->pixelDelta(),
                                       wheelEvent->angleDelta(),
                                       wheelEvent->buttons(),
                                       wheelEvent->modifiers(),
                                       wheelEvent->phase(),
                                       wheelEvent->inverted(),
                                       wheelEvent->source(),
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

AppOptions parseAppOptions(int argc, char* argv[]) {
    AppOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg.find("--test-mode") != std::string::npos) {
            options.testMode = true;
            options.mockCamera = true;
            options.noDaq = true;
            options.noStartupPrompts = true;
        }
        if (arg.find("--mock-camera") != std::string::npos) {
            options.mockCamera = true;
        }
        if (arg.find("--verify-camera-workspace") != std::string::npos) {
            options.verifyCameraWorkspace = true;
        }
        if (arg.find("--verify-daq-settings") != std::string::npos) {
            options.verifyDaqSettings = true;
        }
        if (arg.find("--no-daq") != std::string::npos) {
            options.noDaq = true;
        }
        if (arg.find("--no-startup-prompts") != std::string::npos) {
            options.noStartupPrompts = true;
        }
        const std::string workspacePrefix = "--workspace=";
        if (arg == "--workspace" && i + 1 < argc) {
            options.initialWorkspace = QString::fromLocal8Bit(argv[++i]).trimmed().toLower();
        } else if (arg.rfind(workspacePrefix, 0) == 0) {
            options.initialWorkspace = QString::fromLocal8Bit(arg.substr(workspacePrefix.size()).c_str()).trimmed().toLower();
        }
        const std::string reviewPrefix = "--dataset-builder-review-manifest=";
        if (arg == "--dataset-builder-review-manifest" && i + 1 < argc) {
            options.datasetBuilderReviewPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.rfind(reviewPrefix, 0) == 0) {
            options.datasetBuilderReviewPath = QString::fromLocal8Bit(arg.substr(reviewPrefix.size()).c_str());
        }
    }
    return options;
}

QString findProjectRootFromApp() {
    QStringList starts;
    starts << QCoreApplication::applicationDirPath() << QDir::currentPath();
    for (const auto& start : starts) {
        QDir dir(start);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists() &&
                (QFileInfo(dir.filePath("app/runtime/models")).isDir() ||
                 QFileInfo(dir.filePath("internal-release/app/runtime/models")).isDir())) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) break;
        }
    }
    return QString();
}

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath) {
    if (projectRoot.isEmpty() || relativePath.trimmed().isEmpty()) return QString();
    const QString trimmed = relativePath.trimmed();
    const QStringList candidates = {
        QDir(projectRoot).absoluteFilePath(trimmed),
        QDir(projectRoot).absoluteFilePath("internal-release/" + trimmed),
        QDir(projectRoot).absoluteFilePath("internal-release/app/runtime/models/" + QFileInfo(trimmed).fileName()),
        QDir(projectRoot).absoluteFilePath("app/runtime/models/" + QFileInfo(trimmed).fileName())
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return QString();
}

QString modelRegistryPath() {
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString registry = runtimeModelArtifactPath(projectRoot, "app/runtime/models/model_registry.json");
        if (!registry.isEmpty()) return registry;
        return QDir(projectRoot).absoluteFilePath("app/runtime/models/model_registry.json");
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        QString candidate = dir.filePath("models/model_registry.json");
        if (QFileInfo(candidate).exists()) return candidate;
        if (!dir.cdUp()) break;
    }
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("models/model_registry.json");
}

QJsonObject temporaryStaticModelRegistry() {
    QJsonArray entries;

    QJsonObject promoted;
    promoted["registry_entry_id"] = "run_20260429_221500_wsl2_binary_linuxmirror_onnx";
    promoted["display_name"] = "Promoted/current binary runtime";
    promoted["state"] = "promoted_current";
    promoted["live_use_mode"] = "normal";
    promoted["selectable_for_normal_live_sorting"] = true;
    promoted["model_path"] = "app/runtime/models/squeezenet_final_new_condition.onnx";
    promoted["model_sha256"] = "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769";
    promoted["metadata_path"] = "app/runtime/models/metadata.json";
    promoted["metadata_sha256"] = "fa5321dfad900baec23fa6c239a29279e0e8c03fa2e78f0bd679dfb973888d2f";
    promoted["metadata_status"] = "Pass";
    promoted["validation_status"] = "Default hashes match / image pass / NI pass / sequence provisional";
    promoted["promotion_status"] = "Promoted/current";
    promoted["promotion_record_path"] = "docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    QJsonObject promotedDisplayLabels;
    promotedDisplayLabels["0"] = "Waste";
    promotedDisplayLabels["1"] = "Hits";
    promoted["classes"] = QJsonArray{"0", "1"};
    promoted["display_labels"] = promotedDisplayLabels;
    QJsonObject promotedTarget;
    promotedTarget["target_class_id"] = "1";
    promotedTarget["target_display_label"] = "Hits";
    promotedTarget["waste_class_id"] = "0";
    promoted["target_policy"] = promotedTarget;
    promoted["limitations"] = QJsonArray{"Sequence validation remains provisional by policy for public claims.", "Public model/data release approvals remain separate."};
    promoted["blockers"] = QJsonArray{};
    entries.append(promoted);

    QJsonObject backup;
    backup["registry_entry_id"] = "pre_binary_promotion_backup";
    backup["display_name"] = "Cell aggregate model V1 (2026-05-14)";
    backup["state"] = "promoted_current";
    backup["live_use_mode"] = "normal";
    backup["selectable_for_normal_live_sorting"] = true;
    backup["model_path"] = "app/runtime/models/pre_binary_promotion_backup.onnx";
    backup["model_sha256"] = "8b534dbec19d4f37e75803f6d01c9f32827f9d394c92a59c21c2ac6b23a2d1fd";
    backup["metadata_path"] = "app/runtime/models/pre_binary_promotion_backup_metadata.json";
    backup["metadata_sha256"] = "dd5499f4c96e3b5d9c812adc114262a20a5b56e927ef15ba06d69720d4cc9bac";
    backup["metadata_status"] = "Legacy schema";
    backup["validation_status"] = "Legacy backup packaged as active default";
    backup["promotion_status"] = "Packaged active default";
    backup["promotion_record_path"] = "open-visual-droplet-sorter-suite/docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    backup["classes"] = QJsonArray{"Empty", "Single", "MoreThanTwo"};
    QJsonObject backupDisplayLabels;
    backupDisplayLabels["Empty"] = "Empty";
    backupDisplayLabels["Single"] = "Single";
    backupDisplayLabels["MoreThanTwo"] = "More than two";
    backup["display_labels"] = backupDisplayLabels;
    QJsonObject backupTarget;
    backupTarget["target_class_id"] = "Single";
    backupTarget["target_display_label"] = "Single";
    backup["target_policy"] = backupTarget;
    backup["limitations"] = QJsonArray{"Legacy three-class runtime retained as the packaged active default for this internal release."};
    backup["blockers"] = QJsonArray{};
    entries.append(backup);

    QJsonObject registry;
    registry["schema_version"] = "model-registry-v1";
    registry["registry_id"] = "temporary-static-fallback";
    registry["source"] = "temporary_static_fallback_missing_or_invalid_file";
    registry["entries"] = entries;
    return registry;
}

QJsonObject loadModelRegistry(QString* loadedPath = nullptr, QString* loadWarning = nullptr) {
    const QString path = modelRegistryPath();
    if (loadedPath) *loadedPath = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (loadWarning) *loadWarning = "Model registry file not readable; using temporary static fallback: " + path;
        return temporaryStaticModelRegistry();
    }
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (loadWarning) *loadWarning = "Model registry file invalid; using temporary static fallback: " + parseError.errorString();
        return temporaryStaticModelRegistry();
    }
    QJsonObject registry = doc.object();
    if (registry.value("schema_version").toString() != "model-registry-v1" ||
        !registry.value("entries").isArray()) {
        if (loadWarning) *loadWarning = "Model registry schema missing/unsupported; using temporary static fallback.";
        return temporaryStaticModelRegistry();
    }
    if (loadWarning) loadWarning->clear();
    return registry;
}

QString registryString(const QJsonObject& entry, const QString& key) {
    return entry.value(key).toString();
}

QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key) {
    return entry.value(objectKey).toObject().value(key).toString();
}

QString runtimePathFromRegistryPath(const QString& path) {
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute()) return trimmed;
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        QString absolute = runtimeModelArtifactPath(projectRoot, trimmed);
        if (QFileInfo::exists(absolute)) {
            return QDir(QCoreApplication::applicationDirPath()).relativeFilePath(absolute);
        }
    }
    return trimmed;
}

QString registryEntrySummary(const QJsonObject& entry, const QString& registryPath, const QString& warning) {
    QStringList lines;
    if (!warning.isEmpty()) lines << "Registry warning: " + warning;
    lines << "Registry source: " + registryPath;
    lines << "Model: " + registryString(entry, "registry_entry_id");
    lines << "State: " + registryString(entry, "state");
    lines << "Live-use mode: " + registryString(entry, "live_use_mode");
    lines << "ONNX: " + registryString(entry, "model_path");
    lines << "ONNX SHA-256: " + registryString(entry, "model_sha256");
    lines << "Metadata: " + registryString(entry, "metadata_path");
    lines << "Metadata SHA-256: " + registryString(entry, "metadata_sha256");

    QStringList classLines;
    QJsonArray classes = entry.value("classes").toArray();
    QJsonObject displayLabels = entry.value("display_labels").toObject();
    for (const auto& value : classes) {
        QString classId = value.toString();
        QString display = displayLabels.value(classId).toString(classId);
        classLines << QString("%1 %2").arg(classId, display);
    }
    lines << "Classes: " + classLines.join(" / ");
    lines << "Target policy: canonical class id " +
        registryNestedString(entry, "target_policy", "target_class_id") +
        ", displayed as " + registryNestedString(entry, "target_policy", "target_display_label");
    lines << "Validation: " + registryString(entry, "validation_status");
    lines << "Promotion record: " + registryString(entry, "promotion_record_path");

    QStringList limitations;
    for (const auto& value : entry.value("limitations").toArray()) limitations << value.toString();
    if (!limitations.isEmpty()) lines << "Limitations: " + limitations.join("; ");

    QStringList blockerTexts;
    for (const auto& value : entry.value("blockers").toArray()) {
        QJsonObject blocker = value.toObject();
        QString text = blocker.value("blocker").toString();
        QString action = blocker.value("required_next_action").toString();
        if (!action.isEmpty()) text += " - " + action;
        if (!text.trimmed().isEmpty()) blockerTexts << text;
    }
    if (!blockerTexts.isEmpty()) lines << "Blockers: " + blockerTexts.join("; ");
    return lines.join("\n");
}

constexpr int kRuntimeSettingsSchemaVersion = 1;
constexpr const char* kRuntimeSettingsSchemaVersionKey = "runtime/v1/schemaVersion";

QString runOutputBaseForSettings(const QString& outputDir) {
    QString trimmed = outputDir.trimmed();
    if (trimmed.isEmpty()) return trimmed;
    QDir dir(trimmed);
    QString leaf = dir.dirName();
    if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
        dir.cdUp();
        return dir.absolutePath();
    }
    return trimmed;
}

struct DefaultWorkspacePaths {
    QString root;
    QString models;
    QString datasets;
    QString runs;
    QString activeModel;
    QString activeMetadata;
    QString preparedDataset;
};

QString defaultOpenDssRoot() {
    const QString userProfile = qEnvironmentVariable("USERPROFILE").trimmed();
    if (!userProfile.isEmpty()) {
        return QDir(userProfile).filePath("Documents/OpenDSS");
    }
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::home().filePath("Documents");
    return QDir(documents).filePath("OpenDSS");
}

QString resolvePackagedPathFromRegistryPath(const QString& registryPath) {
    const QString trimmed = registryPath.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute()) return trimmed;
    const QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString resolved = runtimeModelArtifactPath(projectRoot, trimmed);
        if (!resolved.isEmpty()) return resolved;
        const QString direct = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(direct)) return direct;
    }
    QDir probe(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = probe.absoluteFilePath(trimmed);
        if (QFileInfo::exists(candidate)) return candidate;
        const QString modelsCandidate = probe.absoluteFilePath("models/" + QFileInfo(trimmed).fileName());
        if (QFileInfo::exists(modelsCandidate)) return modelsCandidate;
        if (!probe.cdUp()) break;
    }
    return trimmed;
}

bool copyFileIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (!QFileInfo(sourcePath).isFile()) return false;
    if (QFileInfo(destinationPath).isFile()) return true;
    QDir().mkpath(QFileInfo(destinationPath).absolutePath());
    return QFile::copy(sourcePath, destinationPath);
}

bool copyDirectoryIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (sourcePath.trimmed().isEmpty()) return false;
    if (!QFileInfo(sourcePath).isDir()) return false;
    if (QFileInfo(destinationPath).isDir()) return true;
    QDir().mkpath(destinationPath);
    QDirIterator it(sourcePath, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    bool ok = true;
    while (it.hasNext()) {
        it.next();
        const QString relative = QDir(sourcePath).relativeFilePath(it.filePath());
        const QString destination = QDir(destinationPath).filePath(relative);
        if (it.fileInfo().isDir()) {
            ok = QDir().mkpath(destination) && ok;
        } else if (!QFileInfo(destination).isFile()) {
            QDir().mkpath(QFileInfo(destination).absolutePath());
            ok = QFile::copy(it.filePath(), destination) && ok;
        }
    }
    return ok;
}

QJsonObject activeRegistryEntry(const QJsonArray& entries) {
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value("selectable_for_normal_live_sorting").toBool(false) &&
            registryString(entry, "live_use_mode") != "blocked") {
            return entry;
        }
    }
    return entries.isEmpty() ? QJsonObject{} : entries.first().toObject();
}

DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries) {
    DefaultWorkspacePaths paths;
    paths.root = defaultOpenDssRoot();
    paths.models = QDir(paths.root).filePath("models");
    paths.datasets = QDir(paths.root).filePath("datasets");
    paths.runs = QDir(paths.root).filePath("runs");
    QDir().mkpath(paths.models);
    QDir().mkpath(paths.datasets);
    QDir().mkpath(paths.runs);

    const QJsonObject activeEntry = activeRegistryEntry(registryEntries);
    const QString sourceModel = resolvePackagedPathFromRegistryPath(registryString(activeEntry, "model_path"));
    const QString sourceMetadata = resolvePackagedPathFromRegistryPath(registryString(activeEntry, "metadata_path"));
    if (QFileInfo(sourceModel).isFile()) {
        paths.activeModel = QDir(paths.models).filePath(QFileInfo(sourceModel).fileName());
        copyFileIfMissing(sourceModel, paths.activeModel);
    }
    if (QFileInfo(sourceMetadata).isFile()) {
        paths.activeMetadata = QDir(paths.models).filePath(QFileInfo(sourceMetadata).fileName());
        copyFileIfMissing(sourceMetadata, paths.activeMetadata);
    }

    const QString projectRoot = findProjectRootFromApp();
    QString sourceDataset;
    if (!projectRoot.isEmpty()) {
        const QStringList candidates = {
            QDir(projectRoot).absoluteFilePath("internal-release/datasets/prepared/droplet_binary_2026-04-30"),
            QDir(projectRoot).absoluteFilePath("datasets/prepared/droplet_binary_2026-04-30")
        };
        for (const auto& candidate : candidates) {
            if (QFileInfo(candidate).isDir()) {
                sourceDataset = candidate;
                break;
            }
        }
    }
    paths.preparedDataset = QDir(paths.datasets).filePath("droplet_binary_2026-04-30");
    copyDirectoryIfMissing(sourceDataset, paths.preparedDataset);
    return paths;
}

void setComboTextIfPresent(QComboBox* combo, const QString& text) {
    if (!combo || text.isEmpty()) return;
    int index = combo->findText(text);
    if (index >= 0) combo->setCurrentIndex(index);
}

QJsonObject comboSnapshot(QComboBox* combo) {
    QJsonObject obj;
    if (!combo) return obj;
    obj["index"] = combo->currentIndex();
    obj["text"] = combo->currentText();
    return obj;
}

struct StatsTracker {
    QMap<QString, int> classCounts;
    int totalEvents = 0;
    int hitCount = 0;
    int wasteCount = 0;
    bool eventActive = false;
    int missCount = 0;
    int currentEventId = 0;
    cv::Point2f startCentroid = {0.0f, 0.0f};
    cv::Point2f lastCentroid = {0.0f, 0.0f};
    bool hasCentroid = false;
    double cumulativeDy = 0.0;
    double lastY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    int frameHeight = 0;
    QString currentLabel;
    QString lastEventDir = "Unknown";
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
};

struct StatsSnapshot {
    int totalEvents = 0;
    int hitCount = 0;
    int wasteCount = 0;
    bool eventActive = false;
    QString classText;
    QString lastText;
    QMap<QString, int> classCounts;
    QString lastEventDir;
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
};

QString normalizeEventLabel(const QString& label) {
    return label.isEmpty() ? "(unclassified)" : label;
}

QString decideEventDirection(double cumulativeDy, double lastY, int frameHeight, bool hasCentroid) {
    if (!hasCentroid) return "Unknown";
    double threshold = 2.0;
    if (frameHeight > 0) {
        threshold = std::max(threshold, frameHeight * 0.02);
    }
    bool movedUp = cumulativeDy < -threshold;
    bool movedDown = cumulativeDy > threshold;
    bool hasFrame = (frameHeight > 0);
    double mid = hasFrame ? frameHeight * 0.5 : 0.0;
    if (movedUp && (!hasFrame || lastY < mid)) {
        return "Waste";
    }
    if (movedDown && (!hasFrame || lastY >= mid)) {
        return "Hit";
    }
    if (hasFrame) {
        return (lastY < mid) ? "Waste" : "Hit";
    }
    return (cumulativeDy < 0.0) ? "Waste" : "Hit";
}

struct SequenceEventRecord {
    int eventId = 0;
    QString label;
    int startFrame = -1;
    int decisionFrame = -1;
    QString decisionDir;
    int firedFrame = -1;
    int framesTracked = 0;
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double cumulativeDy = 0.0;
    double pathLength = 0.0;
    int frameHeight = 0;
};

struct SequenceEventTracker {
    int resetFrames = 2;
    bool eventActive = false;
    int missCount = 0;
    int currentEventId = 0;
    cv::Point2f startCentroid = {0.0f, 0.0f};
    cv::Point2f lastCentroid = {0.0f, 0.0f};
    bool hasCentroid = false;
    double cumulativeDy = 0.0;
    double lastY = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double pathLength = 0.0;
    int frameHeight = 0;
    QString currentLabel;
    int startFrame = -1;
    int firedFrame = -1;
    int framesTracked = 0;
    QString lastEventDir = "Unknown";
    QString lastEventLabel;
    int lastDecisionFrame = -1;
    int lastDecisionEventId = 0;
    int lastFrameNumber = -1;
    std::vector<SequenceEventRecord> events;

    void reset(int resetFramesIn) {
        resetFrames = resetFramesIn;
        eventActive = false;
        missCount = 0;
        currentEventId = 0;
        hasCentroid = false;
        cumulativeDy = 0.0;
        lastY = 0.0;
        minY = 0.0;
        maxY = 0.0;
        pathLength = 0.0;
        frameHeight = 0;
        currentLabel.clear();
        startFrame = -1;
        firedFrame = -1;
        framesTracked = 0;
        lastEventDir = "Unknown";
        lastEventLabel.clear();
        lastDecisionFrame = -1;
        lastDecisionEventId = 0;
        lastFrameNumber = -1;
        events.clear();
    }

    void startEvent(const PipelineEvent& evt) {
        eventActive = true;
        missCount = 0;
        currentEventId++;
        startCentroid = evt.centroid;
        lastCentroid = evt.centroid;
        hasCentroid = true;
        cumulativeDy = 0.0;
        lastY = evt.centroid.y;
        minY = evt.centroid.y;
        maxY = evt.centroid.y;
        pathLength = 0.0;
        framesTracked = 1;
        if (evt.frameHeight > 0) frameHeight = evt.frameHeight;
        currentLabel = normalizeEventLabel(QString::fromStdString(evt.label));
        startFrame = static_cast<int>(evt.frameNumber);
        firedFrame = evt.fired ? static_cast<int>(evt.frameNumber) : -1;
    }

    void endEvent(int decisionFrame) {
        if (!eventActive) return;
        QString dir = decideEventDirection(cumulativeDy, lastY, frameHeight, hasCentroid);

        SequenceEventRecord rec;
        rec.eventId = currentEventId;
        rec.label = normalizeEventLabel(currentLabel);
        rec.startFrame = startFrame;
        rec.decisionFrame = decisionFrame;
        rec.decisionDir = dir;
        rec.firedFrame = firedFrame;
        rec.framesTracked = framesTracked;
        rec.startX = startCentroid.x;
        rec.startY = startCentroid.y;
        rec.endX = lastCentroid.x;
        rec.endY = lastCentroid.y;
        rec.minY = minY;
        rec.maxY = maxY;
        rec.cumulativeDy = cumulativeDy;
        rec.pathLength = pathLength;
        rec.frameHeight = frameHeight;
        events.push_back(rec);

        lastEventDir = dir;
        lastEventLabel = rec.label;
        lastDecisionFrame = decisionFrame;
        lastDecisionEventId = currentEventId;
        eventActive = false;
        hasCentroid = false;
        missCount = 0;
        currentLabel.clear();
        cumulativeDy = 0.0;
        pathLength = 0.0;
        framesTracked = 0;
        startFrame = -1;
        firedFrame = -1;
    }

    void update(const PipelineEvent& evt, bool processed) {
        if (!processed) return;
        lastFrameNumber = static_cast<int>(evt.frameNumber);
        if (evt.fired) {
            if (eventActive) {
                endEvent(static_cast<int>(evt.frameNumber));
            }
            startEvent(evt);
            return;
        }
        if (evt.detected) {
            if (!eventActive) {
                startEvent(evt);
            } else {
                double dx = static_cast<double>(evt.centroid.x - lastCentroid.x);
                double dy = static_cast<double>(evt.centroid.y - lastCentroid.y);
                pathLength += std::sqrt(dx * dx + dy * dy);
                cumulativeDy += dy;
                lastCentroid = evt.centroid;
                hasCentroid = true;
                lastY = evt.centroid.y;
                minY = std::min(minY, static_cast<double>(evt.centroid.y));
                maxY = std::max(maxY, static_cast<double>(evt.centroid.y));
                framesTracked++;
                if (evt.frameHeight > 0) frameHeight = evt.frameHeight;
                missCount = 0;
            }
        } else if (eventActive) {
            missCount++;
            if (missCount >= resetFrames) {
                endEvent(static_cast<int>(evt.frameNumber));
            }
        }
    }

    void finalize() {
        if (!eventActive) return;
        int decisionFrame = (lastFrameNumber >= 0) ? lastFrameNumber : startFrame;
        endEvent(decisionFrame);
    }
};

QMutex liveEventMutex;
SequenceEventTracker liveEventTracker;

struct LiveLogRecord {
    QString wallTime;
    qint64 elapsedMs = 0;
    qint64 frameIndex = 0;
    qint64 delivered = 0;
    qint64 dropped = 0;
    double fps = 0.0;
    double camFps = 0.0;
    double procMs = 0.0;
    bool processed = false;
    bool pipelineEnabled = false;
    bool pipelineReady = false;
    QString skipReason;
    bool detected = false;
    bool fired = false;
    double area = 0.0;
    int bboxX = 0;
    int bboxY = 0;
    int bboxW = 0;
    int bboxH = 0;
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    QString cropPath;
    QString label;
    double score = 0.0;
    bool triggered = false;
    bool triggerOk = false;
    int bgRemaining = 0;
    QString eventDir;
    int decisionFrame = -1;
    int decisionEventId = 0;
    int hitCount = 0;
    int wasteCount = 0;
};

class ZoomImageView : public QScrollArea {
public:
    ZoomImageView(QWidget* parent=nullptr)
        : QScrollArea(parent), label(new QLabel), scale(1.0), hasImage(false), zoomSteps(0), effectiveScale(1.0) {
        label->setBackgroundRole(QPalette::Base);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        label->setScaledContents(true); // paint-time scaling instead of allocating huge pixmaps
        setWidget(label);
        setAlignment(Qt::AlignCenter);
        setWidgetResizable(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setMouseTracking(true);
    }

    void setZoomChanged(const std::function<void(double)>& cb) { onZoomChanged = cb; }
    void setImageLabelObjectName(const char* name) { nameWidget(label, name); }

    void setImage(const QImage& img) {
        if (img.isNull()) return;
        if (!hasImage) {
            scale = 1.0;
            effectiveScale = 1.0;
            hasImage = true;
            zoomSteps = 0;
            zoomReadyTimer.restart();
            if (onZoomChanged) onZoomChanged(effectiveScale);
            if (horizontalScrollBar()) horizontalScrollBar()->setValue(0);
            if (verticalScrollBar()) verticalScrollBar()->setValue(0);
        }
        // Make a deep copy so the buffer is stable while frames keep streaming.
        lastImage = img.copy();
        basePixmap = QPixmap::fromImage(lastImage);
        updatePixmap();
    }

    void resetScale() {
        scale = 1.0;
        effectiveScale = 1.0;
        zoomSteps = 0;
        if (onZoomChanged) onZoomChanged(effectiveScale);
        hasImage = !lastImage.isNull();
        updatePixmap();
    }

    void fitToView() {
        if (lastImage.isNull()) {
            resetScale();
            return;
        }
        const QSize available = viewport() ? viewport()->contentsRect().size() : QSize();
        if (available.width() <= 0 || available.height() <= 0) {
            resetScale();
            return;
        }
        const double sx = static_cast<double>(available.width()) / static_cast<double>(lastImage.width());
        const double sy = static_cast<double>(available.height()) / static_cast<double>(lastImage.height());
        scale = std::clamp(std::min(sx, sy), 0.05, computeMaxScale());
        zoomSteps = static_cast<int>(std::lround(std::log(scale) / std::log(1.25)));
        updatePixmap();
        if (horizontalScrollBar()) horizontalScrollBar()->setValue(0);
        if (verticalScrollBar()) verticalScrollBar()->setValue(0);
    }

    void zoomBySteps(int steps) {
        if (lastImage.isNull()) return;
        zoomSteps = std::clamp(zoomSteps + steps, -50, 50);
        double desiredScale = std::pow(1.25, zoomSteps);
        scale = std::clamp(desiredScale, 0.05, computeMaxScale());
        zoomSteps = static_cast<int>(std::lround(std::log(scale) / std::log(1.25)));
        updatePixmap();
    }

protected:
    void wheelEvent(QWheelEvent* ev) override {
        try {
            if (lastImage.isNull()) {
                QScrollArea::wheelEvent(ev);
                return;
            }
            if (zoomReadyTimer.isValid() && zoomReadyTimer.elapsed() < 1000) {
                ev->accept();
                return;
            }
            if (!horizontalScrollBar() || !verticalScrollBar()) {
                QScrollArea::wheelEvent(ev);
                return;
            }
            // Normalize to wheel ticks (120 per detent)
            double ticks = ev->angleDelta().y() / 120.0;
            double oldScale = scale;
            int newSteps = std::clamp(zoomSteps + static_cast<int>(std::round(ticks)), -50, 50);
            double desiredScale = std::pow(1.25, newSteps); // ~1.25x per tick
            double maxScale = computeMaxScale();
            double newScale = std::clamp(desiredScale, 0.05, maxScale); // avoid zero/negative and clamp max
            if (qFuzzyCompare(newScale, scale)) {
                ev->accept();
                return;
            }
            // Keep steps consistent with the clamped scale to avoid runaway values.
            zoomSteps = static_cast<int>(std::lround(std::log(newScale) / std::log(1.25)));

            QPointF vpPos = ev->position();
            QPointF contentPos = (vpPos + QPointF(horizontalScrollBar()->value(),
                                                  verticalScrollBar()->value())) / oldScale;

            scale = newScale;
            updatePixmap();

            horizontalScrollBar()->setValue(int(contentPos.x() * scale - vpPos.x()));
            verticalScrollBar()->setValue(int(contentPos.y() * scale - vpPos.y()));
            ev->accept();
        } catch (const std::exception& e) {
            Q_UNUSED(e);
        } catch (...) {
        }
    }

private:
    double computeMaxScale() const {
        if (basePixmap.isNull()) return 1.56;
        int w = basePixmap.width();
        int h = basePixmap.height();
        int maxDim = (std::min(w, h) <= 256) ? 8192 : 4096;
        double dimCap = static_cast<double>(maxDim) / static_cast<double>(std::max(w, h));
        // Allow more zoom for small dimensions but cap to a sane upper bound.
        return std::clamp(std::max(1.56, dimCap * 2.0), 0.1, 8.0);
    }

    void updatePixmap() {
        try {
            if (basePixmap.isNull() || scale <= 0.0) return;
            if (updatingPixmap.test_and_set()) {
                // Skip re-entrant calls that can happen when zooming rapidly during streaming.
                return;
            }
            int baseW = basePixmap.width();
            int baseH = basePixmap.height();
            QSize targetSize = (scale == 1.0)
                ? basePixmap.size()
                : QSize(std::max(1, int(std::lround(baseW * scale))),
                        std::max(1, int(std::lround(baseH * scale))));

            int maxDim = (std::min(baseW, baseH) <= 256) ? 8192 : 4096;
            if (targetSize.width() > maxDim || targetSize.height() > maxDim) {
                double factor = static_cast<double>(maxDim) / static_cast<double>(std::max(targetSize.width(), targetSize.height()));
                targetSize.setWidth(std::max(1, int(std::lround(targetSize.width() * factor))));
                targetSize.setHeight(std::max(1, int(std::lround(targetSize.height() * factor))));
            }

            label->setPixmap(basePixmap);
            label->resize(targetSize);
            label->setAlignment(Qt::AlignCenter);
            effectiveScale = static_cast<double>(targetSize.width()) / static_cast<double>(baseW);
            if (onZoomChanged) onZoomChanged(effectiveScale);
            updatingPixmap.clear();
        } catch (const std::exception& e) {
            Q_UNUSED(e);
            updatingPixmap.clear();
        } catch (...) {
            updatingPixmap.clear();
        }
    }

    QLabel* label;
    QImage lastImage;
    QPixmap basePixmap;
    double scale;
    double effectiveScale;
    bool hasImage;
    int zoomSteps;
    QElapsedTimer zoomReadyTimer;
    std::atomic_flag updatingPixmap = ATOMIC_FLAG_INIT;
    std::function<void(double)> onZoomChanged;
};

static QString formatTimeSeconds(double seconds) {
    if (seconds < 0) seconds = 0;
    int totalMs = static_cast<int>(std::lround(seconds * 1000.0));
    int ms = totalMs % 1000;
    int totalSec = totalMs / 1000;
    int s = totalSec % 60;
    int totalMin = totalSec / 60;
    int m = totalMin % 60;
    int h = totalMin / 60;
    if (h > 0) {
        return QString("%1:%2:%3.%4")
            .arg(h,2,10,QChar('0'))
            .arg(m,2,10,QChar('0'))
            .arg(s,2,10,QChar('0'))
            .arg(ms,3,10,QChar('0'));
    }
    return QString("%1:%2.%3")
        .arg(m,2,10,QChar('0'))
        .arg(s,2,10,QChar('0'))
        .arg(ms,3,10,QChar('0'));
}

static QImage renderPieChart(const QString& title,
                             const QVector<QString>& labels,
                             const QVector<double>& values,
                             const QVector<QColor>& colors,
                             const QSize& size = QSize(520, 420)) {
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont titleFont = p.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRect(0, 0, size.width(), 30), Qt::AlignCenter, title);

    double total = 0.0;
    for (double v : values) total += v;
    QRect pieRect(20, 40, size.height() - 60, size.height() - 60);
    if (total <= 0.0) {
        p.setFont(QFont(p.font().family(), 10));
        p.drawText(pieRect, Qt::AlignCenter, "No data");
        return img;
    }

    int startAngle = 0;
    for (int i = 0; i < values.size(); ++i) {
        double fraction = values[i] / total;
        int span = static_cast<int>(std::round(fraction * 360.0 * 16.0));
        p.setBrush(colors.value(i, Qt::gray));
        p.setPen(Qt::NoPen);
        p.drawPie(pieRect, startAngle, span);
        startAngle += span;
    }

    QRect legendRect(pieRect.right() + 20, pieRect.top(), size.width() - pieRect.right() - 30, pieRect.height());
    p.setPen(Qt::black);
    QFont legendFont = p.font();
    legendFont.setPointSize(9);
    legendFont.setBold(false);
    p.setFont(legendFont);
    int y = legendRect.top();
    for (int i = 0; i < labels.size(); ++i) {
        double percent = values[i] / total * 100.0;
        QRect colorBox(legendRect.left(), y + 4, 12, 12);
        p.fillRect(colorBox, colors.value(i, Qt::gray));
        p.drawRect(colorBox);
        QString text = QString("%1  %2% (%3)")
            .arg(labels[i])
            .arg(percent, 0, 'f', 1)
            .arg(static_cast<int>(values[i]));
        p.drawText(legendRect.left() + 18, y + 14, text);
        y += 20;
    }

    return img;
}

class StatsFigureWindow : public QDialog {
public:
    explicit StatsFigureWindow(QWidget* parent=nullptr)
        : QDialog(parent) {
        nameWidget(this, "StatsFigureWindow");
        setWindowTitle("Pipeline Figures");
        resize(1100, 600);
        auto layout = new QVBoxLayout;
        auto row = new QHBoxLayout;
        hitWasteLabel = new QLabel;
        classLabel = new QLabel;
        nameWidget(hitWasteLabel, "StatsHitWasteImageLabel");
        nameWidget(classLabel, "StatsClassDistributionImageLabel");
        hitWasteLabel->setAlignment(Qt::AlignCenter);
        classLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(hitWasteLabel, 1);
        row->addWidget(classLabel, 1);
        layout->addLayout(row);
        saveBtn = new QPushButton("Save Figures...");
        nameWidget(saveBtn, "StatsSaveFiguresButton");
        layout->addWidget(saveBtn, 0, Qt::AlignRight);
        setLayout(layout);
    }

    void setImages(const QImage& hitWaste, const QImage& classImg) {
        hitWasteImg = hitWaste;
        classImg_ = classImg;
        hitWasteLabel->setPixmap(QPixmap::fromImage(hitWasteImg));
        classLabel->setPixmap(QPixmap::fromImage(classImg_));
    }

    QPushButton* saveButton() const { return saveBtn; }

    bool saveImages(const QString& dir, const QString& prefix) const {
        if (dir.isEmpty()) return false;
        QDir out(dir);
        out.mkpath(".");
        QString hitPath = out.filePath(prefix + "_hit_waste.png");
        QString clsPath = out.filePath(prefix + "_class_dist.png");
        bool ok1 = hitWasteImg.isNull() ? false : hitWasteImg.save(hitPath);
        bool ok2 = classImg_.isNull() ? false : classImg_.save(clsPath);
        return ok1 && ok2;
    }

private:
    QLabel* hitWasteLabel = nullptr;
    QLabel* classLabel = nullptr;
    QPushButton* saveBtn = nullptr;
    QImage hitWasteImg;
    QImage classImg_;
};

class ViewerWindow : public QWidget {
public:
    ViewerWindow(QWidget* parent=nullptr)
        : QWidget(parent), fps(0.0) {
        nameWidget(this, "CaptureViewerWindow");
        setWindowFlags(Qt::Window);
        setWindowTitle("Capture Viewer");
        resize(1100, 800);
        setMinimumSize(800, 600);

        imageView = new ZoomImageView;
        nameWidget(imageView, "ViewerImageView");
        imageView->setMinimumSize(640, 480);
        imageView->setStyleSheet("background:#000;");
        imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        frameLabel = new QLabel("Frame: -- / --");
        timeLabel = new QLabel("Time: -- / --");
        nameWidget(frameLabel, "ViewerFrameLabel");
        nameWidget(timeLabel, "ViewerTimeLabel");
        frameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

        folderEdit = new QLineEdit;
        nameWidget(folderEdit, "ViewerFolderEdit");
        folderEdit->setPlaceholderText("Select capture folder...");
        auto browseBtn = new QPushButton("...");
        auto loadBtn = new QPushButton("Load");
        recentCombo = new QComboBox;
        nameWidget(browseBtn, "ViewerBrowseButton");
        nameWidget(loadBtn, "ViewerLoadButton");
        nameWidget(recentCombo, "ViewerRecentComboBox");
        recentCombo->setMinimumWidth(200);

        slider = new QSlider(Qt::Horizontal);
        nameWidget(slider, "ViewerFrameSlider");
        slider->setRange(0, 0);
        slider->setEnabled(false);

        prevBtn = new QPushButton("<");
        nextBtn = new QPushButton(">");
        nameWidget(prevBtn, "ViewerPreviousFrameButton");
        nameWidget(nextBtn, "ViewerNextFrameButton");
        prevBtn->setEnabled(false);
        nextBtn->setEnabled(false);

        auto folderRow = new QHBoxLayout;
        folderRow->addWidget(new QLabel("Folder"));
        folderRow->addWidget(folderEdit, 1);
        folderRow->addWidget(browseBtn);
        folderRow->addWidget(loadBtn);

        auto recentRow = new QHBoxLayout;
        recentRow->addWidget(new QLabel("Recent"));
        recentRow->addWidget(recentCombo, 1);

        auto navRow = new QHBoxLayout;
        navRow->addWidget(prevBtn);
        navRow->addWidget(nextBtn);
        navRow->addWidget(frameLabel, 1);

        auto infoCol = new QVBoxLayout;
        infoCol->addLayout(folderRow);
        infoCol->addLayout(recentRow);
        infoCol->addWidget(timeLabel);
        infoCol->addLayout(navRow);
        infoCol->addWidget(slider);
        infoCol->addStretch(1);

        auto rightPane = new QWidget;
        rightPane->setLayout(infoCol);
        rightPane->setMinimumWidth(320);

        auto layout = new QHBoxLayout;
        layout->addWidget(imageView, 3);
        layout->addWidget(rightPane, 1);
        setLayout(layout);

        imageView->setZoomChanged(nullptr);

        QObject::connect(browseBtn, &QPushButton::clicked, [this](){
            QString dir = QFileDialog::getExistingDirectory(this, "Select capture folder", folderEdit->text());
            if (!dir.isEmpty()) folderEdit->setText(dir);
        });
        QObject::connect(loadBtn, &QPushButton::clicked, [this](){
            loadFolder(folderEdit->text());
        });
        QObject::connect(recentCombo, &QComboBox::activated, [this](int idx){
            if (idx < 0) return;
            QString dir = recentCombo->itemText(idx);
            if (!dir.isEmpty()) {
                folderEdit->setText(dir);
                loadFolder(dir);
            }
        });
        QObject::connect(slider, &QSlider::valueChanged, [this](int v){
            loadFrame(v);
        });
        QObject::connect(prevBtn, &QPushButton::clicked, [this](){
            if (frameFiles.isEmpty()) return;
            int v = std::max(0, slider->value() - 1);
            slider->setValue(v);
        });
        QObject::connect(nextBtn, &QPushButton::clicked, [this](){
            if (frameFiles.isEmpty()) return;
            int v = std::min(slider->maximum(), slider->value() + 1);
            slider->setValue(v);
        });

        auto leftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
        auto rightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
        auto ctrlLeftShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
        auto ctrlRightShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this);
        auto pageUpShortcut = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
        auto pageDownShortcut = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
        QObject::connect(leftShortcut, &QShortcut::activated, [this](){
            stepFrames(-1);
        });
        QObject::connect(rightShortcut, &QShortcut::activated, [this](){
            stepFrames(1);
        });
        QObject::connect(ctrlLeftShortcut, &QShortcut::activated, [this](){
            stepFrames(-5);
        });
        QObject::connect(ctrlRightShortcut, &QShortcut::activated, [this](){
            stepFrames(5);
        });
        QObject::connect(pageUpShortcut, &QShortcut::activated, [this](){
            stepFrames(-10);
        });
        QObject::connect(pageDownShortcut, &QShortcut::activated, [this](){
            stepFrames(10);
        });

        loadRecentFolders();
    }

private:
    void stepFrames(int delta) {
        if (frameFiles.isEmpty()) return;
        int v = std::clamp(slider->value() + delta, 0, slider->maximum());
        slider->setValue(v);
    }

    void loadRecentFolders() {
        QSettings settings;
        QStringList recent = settings.value("viewer/recentFolders").toStringList();
        recentCombo->clear();
        for (const QString& path : recent) {
            recentCombo->addItem(path);
        }
    }

    void updateRecentFolders(const QString& dirPath) {
        QSettings settings;
        QStringList recent = settings.value("viewer/recentFolders").toStringList();
        recent.removeAll(dirPath);
        recent.prepend(dirPath);
        while (recent.size() > 10) recent.removeLast();
        settings.setValue("viewer/recentFolders", recent);
        recentCombo->clear();
        for (const QString& path : recent) {
            recentCombo->addItem(path);
        }
    }

    void loadFolder(const QString& dirPath) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            QMessageBox::warning(this, "Folder not found", "The selected folder does not exist.");
            return;
        }
        QStringList filters;
        filters << "*.tif" << "*.tiff" << "*.TIF" << "*.TIFF";
        frameFiles = dir.entryList(filters, QDir::Files, QDir::Name);
        for (QString& f : frameFiles) {
            f = dir.absoluteFilePath(f);
        }
        fps = readFpsFromInfo(dir.absoluteFilePath("capture_info.txt"));
        slider->setEnabled(!frameFiles.isEmpty());
        prevBtn->setEnabled(!frameFiles.isEmpty());
        nextBtn->setEnabled(!frameFiles.isEmpty());
        int count = static_cast<int>(frameFiles.size());
        slider->setRange(0, std::max(0, count - 1));
        slider->setValue(0);
        updateTimeLabel(0);
        if (frameFiles.isEmpty()) {
            frameLabel->setText("Frame: -- / --");
        } else {
            frameLabel->setText(QString("Frame: %1 / %2").arg(1).arg(count));
            updateRecentFolders(dir.absolutePath());
        }
    }

    double readFpsFromInfo(const QString& infoPath) const {
        QFile f(infoPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0.0;
        QTextStream ts(&f);
        double foundFps = 0.0;
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("Internal FPS:", Qt::CaseInsensitive) ||
                line.startsWith("FPS:", Qt::CaseInsensitive)) {
                QStringList parts = line.split(":");
                if (parts.size() >= 2) {
                    bool ok = false;
                    double val = parts.last().trimmed().toDouble(&ok);
                    if (ok) foundFps = val;
                }
            }
        }
        return foundFps;
    }

    void loadFrame(int index) {
        if (frameFiles.isEmpty()) return;
        int count = static_cast<int>(frameFiles.size());
        index = std::clamp(index, 0, count - 1);
        QImageReader reader(frameFiles.at(index));
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull()) {
            QMessageBox::warning(this, "Read error", "Failed to load image:\n" + reader.errorString());
            return;
        }
        imageView->setImage(img);
        frameLabel->setText(QString("Frame: %1 / %2").arg(index + 1).arg(count));
        updateTimeLabel(index);
    }

    void updateTimeLabel(int index) {
        int count = static_cast<int>(frameFiles.size());
        if (fps <= 0.0 || count == 0) {
            timeLabel->setText("Time: -- / --");
            return;
        }
        double totalSec = static_cast<double>(count) / fps;
        double currentSec = static_cast<double>(index) / fps;
        timeLabel->setText(QString("Time: %1 / %2").arg(formatTimeSeconds(currentSec)).arg(formatTimeSeconds(totalSec)));
    }

    ZoomImageView* imageView;
    QLabel* frameLabel;
    QLabel* timeLabel;
    QLineEdit* folderEdit;
    QComboBox* recentCombo;
    QSlider* slider;
    QPushButton* prevBtn;
    QPushButton* nextBtn;
    QStringList frameFiles;
    double fps;
};

class DatasetLabelerDialog : public QDialog {
public:
    explicit DatasetLabelerDialog(QWidget* parent = nullptr, const QString& initialPath = QString())
        : QDialog(parent) {
        setWindowTitle("Dataset Builder Review");
        resize(1280, 780);
        setMinimumSize(980, 620);
        nameWidget(this, "datasetLabelerWorkspace");

        auto* openFolderButton = new QPushButton("Open Dataset Folder");
        auto* openManifestButton = new QPushButton("Open Manifest");
        hitButton = new QPushButton("Hit");
        wasteButton = new QPushButton("Waste");
        excludeButton = new QPushButton("Exclude");
        acceptButton = new QPushButton("Accept Auto-label");
        undoButton = new QPushButton("Undo");
        saveButton = new QPushButton("Save Review");
        excludeReasonCombo = new QComboBox;
        excludeReasonCombo->addItems({"edge_case", "artifact", "ambiguous", "partial_droplet", "bad_crop", "not_for_training", "other"});
        notesEdit = new QPlainTextEdit;
        notesEdit->setPlaceholderText("Review notes");
        notesEdit->setMaximumHeight(74);
        for (auto* button : {hitButton, wasteButton, excludeButton, acceptButton, undoButton, saveButton}) {
            button->setEnabled(false);
        }
        excludeReasonCombo->setEnabled(false);
        notesEdit->setEnabled(false);

        pathLabel = new QLabel("No dataset selected.");
        pathLabel->setWordWrap(true);
        pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        bannerLabel = new QLabel("Open a Dataset Builder manifest to review crops. Auto-labels remain suggestions until a reviewed label is saved.");
        bannerLabel->setWordWrap(true);
        bannerLabel->setStyleSheet("color:#6b4f00;");
        loadStatusLabel = new QLabel("Dataset Builder review load status: no manifest loaded");
        loadStatusLabel->setWordWrap(true);
        loadStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        loadStatusEdit = new QLineEdit("Dataset Builder review load status: no manifest loaded");
        loadStatusEdit->setReadOnly(true);
        loadStatusEdit->setFrame(false);
        loadStatusEdit->setFocusPolicy(Qt::NoFocus);
        filterCombo = new QComboBox;
        filterCombo->addItems({"All crops", "Unreviewed", "Reviewed", "Excluded", "Auto hit", "Auto waste", "Low confidence", "Warnings"});
        searchEdit = new QLineEdit;
        searchEdit->setPlaceholderText("Filter by image id, path, auto-label, reviewed label, state, or warning");
        prevButton = new QPushButton("Previous");
        nextButton = new QPushButton("Next");
        prevButton->setEnabled(false);
        nextButton->setEnabled(false);
        browserTable = new QTableWidget(0, 7);
        browserTable->setHorizontalHeaderLabels({"Image ID", "Crop", "Auto", "Reviewed", "State", "Eligible", "Warnings"});
        browserTable->horizontalHeader()->setStretchLastSection(true);
        browserTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        browserTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        browserTable->setSelectionMode(QAbstractItemView::SingleSelection);
        classBalanceTable = new QTableWidget(0, 6);
        classBalanceTable->setHorizontalHeaderLabels({"Label", "Reviewed", "Trainer Eligible", "Auto", "Warning", "Policy"});
        classBalanceTable->horizontalHeader()->setStretchLastSection(true);
        classBalanceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        previewLabel = new QLabel("Select a dataset to inspect manifest entries and available summary artifacts.");
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setMinimumSize(320, 220);
        previewLabel->setStyleSheet("background:#111;color:#ddd;border:1px solid #555;");
        previewDetailsLabel = new QLabel("No crop selected.");
        previewDetailsLabel->setWordWrap(true);
        previewDetailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        outputText = new QPlainTextEdit;
        outputText->setReadOnly(true);
        outputText->setPlainText("Accepted inputs: Dataset Builder metadata/dataset_manifest.json, older dataset manifests, metadata/labels.csv, or metadata/crops.csv.");

        nameWidget(openFolderButton, "datasetLabelerOpenDatasetAction");
        nameWidget(openManifestButton, "datasetLabelerOpenManifestButton");
        nameWidget(pathLabel, "datasetLabelerDatasetPathLabel");
        nameWidget(filterCombo, "datasetLabelerCropFilterCombo");
        nameWidget(searchEdit, "datasetLabelerCropSearchEdit");
        nameWidget(prevButton, "datasetLabelerPreviousCropButton");
        nameWidget(nextButton, "datasetLabelerNextCropButton");
        nameWidget(browserTable, "datasetLabelerCropBrowser");
        nameWidget(previewLabel, "datasetLabelerCropPreview");
        nameWidget(previewDetailsLabel, "datasetLabelerCropDetailsLabel");
        nameWidget(hitButton, "datasetBuilderReviewHitButton");
        nameWidget(wasteButton, "datasetBuilderReviewWasteButton");
        nameWidget(excludeButton, "datasetBuilderReviewExcludeButton");
        nameWidget(acceptButton, "datasetBuilderReviewAcceptAutoButton");
        nameWidget(undoButton, "datasetLabelerUndoButton");
        nameWidget(saveButton, "datasetBuilderSaveReviewButton");
        nameWidget(excludeReasonCombo, "datasetBuilderExcludeReasonCombo");
        nameWidget(notesEdit, "datasetBuilderReviewNotesEdit");
        nameWidget(classBalanceTable, "datasetLabelerClassBalanceTable");
        nameWidget(bannerLabel, "datasetLabelerReadinessBanner");
        nameWidget(loadStatusLabel, "DatasetBuilderReviewLoadStatusLabel");
        nameObject(loadStatusEdit, "DatasetBuilderReviewLoadStatusText");
        nameWidget(outputText, "datasetLabelerBackendOutputText");

        auto* topButtons = new QHBoxLayout;
        topButtons->addWidget(openFolderButton);
        topButtons->addWidget(openManifestButton);
        topButtons->addStretch(1);

        auto* actionsLayout = new QGridLayout;
        actionsLayout->addWidget(hitButton, 0, 0);
        actionsLayout->addWidget(wasteButton, 0, 1);
        actionsLayout->addWidget(excludeButton, 0, 2);
        actionsLayout->addWidget(acceptButton, 1, 0, 1, 3);
        actionsLayout->addWidget(new QLabel("Exclude reason"), 2, 0);
        actionsLayout->addWidget(excludeReasonCombo, 2, 1, 1, 2);
        actionsLayout->addWidget(notesEdit, 3, 0, 1, 3);
        actionsLayout->addWidget(undoButton, 4, 0);
        actionsLayout->addWidget(saveButton, 4, 1, 1, 2);
        auto* actionsGroup = new QGroupBox("Manual Review");
        actionsGroup->setLayout(actionsLayout);

        auto* leftLayout = new QVBoxLayout;
        leftLayout->addLayout(topButtons);
        leftLayout->addWidget(pathLabel);
        auto* filterLayout = new QGridLayout;
        filterLayout->addWidget(new QLabel("View"), 0, 0);
        filterLayout->addWidget(filterCombo, 0, 1);
        filterLayout->addWidget(new QLabel("Search"), 1, 0);
        filterLayout->addWidget(searchEdit, 1, 1);
        leftLayout->addLayout(filterLayout);
        leftLayout->addWidget(browserTable, 1);
        auto* navLayout = new QHBoxLayout;
        navLayout->addWidget(prevButton);
        navLayout->addWidget(nextButton);
        leftLayout->addLayout(navLayout);

        auto* rightLayout = new QVBoxLayout;
        rightLayout->addWidget(previewLabel, 1);
        rightLayout->addWidget(previewDetailsLabel);
        rightLayout->addWidget(actionsGroup);
        rightLayout->addWidget(new QLabel("Class Balance"));
        rightLayout->addWidget(classBalanceTable, 1);

        auto* splitter = new QSplitter;
        auto* leftWidget = new QWidget;
        leftWidget->setLayout(leftLayout);
        auto* rightWidget = new QWidget;
        rightWidget->setLayout(rightLayout);
        splitter->addWidget(leftWidget);
        splitter->addWidget(rightWidget);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        auto* layout = new QVBoxLayout;
        layout->addWidget(bannerLabel);
        layout->addWidget(loadStatusLabel);
        layout->addWidget(loadStatusEdit);
        layout->addWidget(splitter, 1);
        layout->addWidget(new QLabel("Inspection Output"));
        layout->addWidget(outputText, 1);
        setLayout(layout);

        QObject::connect(openFolderButton, &QPushButton::clicked, [this]() {
            const QString selected = QFileDialog::getExistingDirectory(this, "Select dataset folder", currentDatasetPath);
            if (!selected.isEmpty()) loadDatasetPath(selected);
        });
        QObject::connect(openManifestButton, &QPushButton::clicked, [this]() {
            const QString selected = QFileDialog::getOpenFileName(this, "Select dataset manifest", currentDatasetPath, "JSON manifest (*.json);;All files (*.*)");
            if (!selected.isEmpty()) loadDatasetPath(selected);
        });
        QObject::connect(filterCombo, &QComboBox::currentTextChanged, [this]() { applyBrowserFilter(); });
        QObject::connect(searchEdit, &QLineEdit::textChanged, [this]() { applyBrowserFilter(); });
        QObject::connect(prevButton, &QPushButton::clicked, [this]() { selectRelativeRow(-1); });
        QObject::connect(nextButton, &QPushButton::clicked, [this]() { selectRelativeRow(1); });
        QObject::connect(browserTable, &QTableWidget::itemSelectionChanged, [this]() { updatePreviewFromSelection(); });
        QObject::connect(hitButton, &QPushButton::clicked, [this]() { applyReviewLabel("hit", true); });
        QObject::connect(wasteButton, &QPushButton::clicked, [this]() { applyReviewLabel("waste", true); });
        QObject::connect(excludeButton, &QPushButton::clicked, [this]() { applyReviewLabel("exclude", true); });
        QObject::connect(acceptButton, &QPushButton::clicked, [this]() { acceptAutoLabel(); });
        QObject::connect(saveButton, &QPushButton::clicked, [this]() { saveManifestAndLabels(); });
        QObject::connect(undoButton, &QPushButton::clicked, [this]() { undoLastReviewEdit(); });
        auto* prevShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
        auto* nextShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
        auto* hitShortcut = new QShortcut(QKeySequence(Qt::Key_H), this);
        auto* wasteShortcut = new QShortcut(QKeySequence(Qt::Key_W), this);
        auto* excludeShortcut = new QShortcut(QKeySequence(Qt::Key_E), this);
        auto* acceptShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
        auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
        auto* saveShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
        QObject::connect(prevShortcut, &QShortcut::activated, [this]() { selectRelativeRow(-1); });
        QObject::connect(nextShortcut, &QShortcut::activated, [this]() { selectRelativeRow(1); });
        QObject::connect(hitShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) applyReviewLabel("hit", true); });
        QObject::connect(wasteShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) applyReviewLabel("waste", true); });
        QObject::connect(excludeShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) applyReviewLabel("exclude", true); });
        QObject::connect(acceptShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) acceptAutoLabel(); });
        QObject::connect(undoShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) undoLastReviewEdit(); });
        QObject::connect(saveShortcut, &QShortcut::activated, [this]() { if (reviewShortcutAllowed()) saveManifestAndLabels(); });

        if (!initialPath.isEmpty()) loadDatasetPath(initialPath);
    }

private:
    struct BrowserRow {
        int manifestIndex = -1;
        QString imageId;
        QString cropPath;
        QString autoLabel;
        QString autoSource;
        QString reviewedLabel;
        QString reviewState;
        QString eligible;
        QString warnings;
        QString confidence;
        QString sourceFramePath;
        QString notes;
        QString excludeReason;
    };

    struct ReviewUndo {
        int manifestIndex = -1;
        QJsonObject previousItem;
    };

    void loadDatasetPath(const QString& selectedPath) {
        currentDatasetPath = QFileInfo(selectedPath).absoluteFilePath();
        browserRows.clear();
        undoStack.clear();
        manifestDoc = QJsonDocument();
        manifestPath.clear();
        isBuilderManifest = false;
        browserTable->setRowCount(0);
        classBalanceTable->setRowCount(0);
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText("No crop selected.");
        previewDetailsLabel->setText("No crop selected.");
        setLoadStatusText("Dataset Builder review load status: loading " + QDir::toNativeSeparators(currentDatasetPath));

        QFileInfo info(currentDatasetPath);
        const bool isManifest = info.isFile();
        datasetRoot = isManifest ? info.dir().absolutePath() : info.absoluteFilePath();
        if (isManifest && info.fileName() == "dataset_manifest.json" && info.dir().dirName() == "metadata") {
            QDir root(info.dir());
            root.cdUp();
            datasetRoot = root.absolutePath();
        }
        pathLabel->setText("Dataset: " + QDir::toNativeSeparators(datasetRoot));

        QStringList report;
        report << "Load: " + currentDatasetPath;
        bool loaded = false;
        if (isManifest) {
            loaded = loadManifest(info.absoluteFilePath(), report);
        } else {
            const QString manifest = QDir(datasetRoot).filePath("metadata/dataset_manifest.json");
            if (QFileInfo::exists(manifest)) loaded = loadManifest(manifest, report);
            else report << "No dataset_manifest.json found.";
        }
        loadSummaryArtifacts(report);
        if (!loaded) loadCropsCsv(report);
        applyBrowserFilter();
        updateReviewControls();
        if (browserRows.isEmpty()) report << "No crop/item rows were available for browsing.";
        else report << QString("Browser rows available: %1; visible after filter: %2").arg(browserRows.size()).arg(browserTable->rowCount());
        updateLoadStatus();
        outputText->setPlainText(report.join("\n"));
    }

    bool loadManifest(const QString& manifestPath, QStringList& report) {
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report << "Manifest unreadable: " + manifestPath;
            return false;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            report << QString("Manifest parse failed at offset %1: %2").arg(err.offset).arg(err.errorString());
            return false;
        }
        QJsonObject root = doc.object();
        manifestDoc = doc;
        this->manifestPath = QFileInfo(manifestPath).absoluteFilePath();
        isBuilderManifest = isDatasetBuilderManifest(root);
        report << "Manifest: " + manifestPath;
        report << "Dataset id: " + root.value("dataset_id").toString("--");
        report << (isBuilderManifest ? "Mode: Dataset Builder review; reviewed_label is editable." : "Mode: read-only legacy dataset inspection.");

        if (isBuilderManifest) {
            loadBuilderManifest(root, report);
            return true;
        }

        QMap<QString, QString> displayByClass;
        QJsonArray classes = root.value("classes").toArray();
        if (classes.isEmpty()) classes = root.value("schema").toObject().value("classes").toArray();
        for (const QJsonValue& value : classes) {
            QJsonObject cls = value.toObject();
            const QString id = cls.value("id").toVariant().toString();
            if (!id.isEmpty()) displayByClass[id] = cls.value("display_name").toString(id);
        }
        QMap<QString, int> includedCounts;
        QMap<QString, int> statusCounts;
        int seedCount = 0;
        int demoCount = 0;
        int unknownRedistribution = 0;
        const QJsonArray items = root.value("items").toArray();
        const int maxRows = std::min(static_cast<int>(items.size()), 500);
        for (int i = 0; i < items.size(); ++i) {
            const QJsonObject item = items.at(i).toObject();
            const QString path = item.value("path").toString(item.value("source_relative_path").toString());
            const QString label = item.value("label").toVariant().toString();
            const QString status = item.value("status").toString("--");
            const QJsonObject provenance = item.value("provenance").toObject();
            const QString origin = provenance.value("origin").toString(provenance.value("source").toString("--"));
            const bool seed = provenance.value("seed_image").toBool(false);
            const bool demo = provenance.value("demo_image").toBool(false);
            const QString redistribution = provenance.value("redistribution_status").toString();
            statusCounts[status]++;
            if (status == "included" && !label.isEmpty()) includedCounts[label]++;
            if (seed) seedCount++;
            if (demo) demoCount++;
            if (redistribution == "unknown") unknownRedistribution++;
            if (i < maxRows) addLegacyBrowserRow(path, label, status, origin, seed ? "yes" : (demo ? "demo" : "no"));
        }
        report << QString("Manifest items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
        if (!statusCounts.isEmpty()) {
            QStringList parts;
            for (auto it = statusCounts.begin(); it != statusCounts.end(); ++it) parts << QString("%1=%2").arg(it.key()).arg(it.value());
            report << "Statuses: " + parts.join(", ");
        }
        if (seedCount || demoCount || unknownRedistribution) {
            report << QString("Provenance: seed=%1, demo=%2, unknown redistribution=%3").arg(seedCount).arg(demoCount).arg(unknownRedistribution);
        }
        populateCountsFromMap(includedCounts, displayByClass);
        return true;
    }

    bool isDatasetBuilderManifest(const QJsonObject& root) const {
        if (root.value("schema_version").toString() == "dataset-builder-manifest-v1") return true;
        const QJsonArray items = root.value("items").toArray();
        if (items.isEmpty()) return false;
        const QJsonObject first = items.first().toObject();
        return first.contains("crop_path") || first.contains("auto_label") || first.contains("reviewed_label") || first.contains("trainer_eligible");
    }

    void loadBuilderManifest(const QJsonObject& root, QStringList& report) {
        QMap<QString, int> autoCounts;
        QMap<QString, int> reviewedCounts;
        QMap<QString, int> eligibleCounts;
        int lowConfidenceCount = 0;
        const QJsonArray items = root.value("items").toArray();
        const int maxRows = std::min(static_cast<int>(items.size()), 1000);
        for (int i = 0; i < items.size(); ++i) {
            const QJsonObject item = items.at(i).toObject();
            const QString autoLabel = normalizedLabel(item.value("auto_label").toString("unknown"));
            const QString reviewedLabel = normalizedLabel(item.value("reviewed_label").toString());
            const QString reviewState = item.value("review_state").toString("unreviewed");
            const bool eligible = item.value("trainer_eligible").toBool(false);
            const double confidence = item.value("auto_label_confidence").toDouble(-1.0);
            QStringList warnings;
            if (reviewState == "unreviewed") warnings << "needs review";
            if (reviewedLabel == "exclude" && item.value("exclude_reason").toString().isEmpty()) warnings << "missing exclude reason";
            if (eligible && reviewedLabel != "hit" && reviewedLabel != "waste") warnings << "eligible label invalid";
            if ((reviewState == "confirmed" || reviewState == "relabeled") && !eligible) warnings << "reviewed class not trainer-eligible";
            if (confidence >= 0.0 && confidence < 0.80) {
                warnings << "low confidence";
                lowConfidenceCount++;
            }
            autoCounts[autoLabel]++;
            if (reviewState == "unreviewed") reviewedCounts["unreviewed"]++;
            else reviewedCounts[reviewedLabel.isEmpty() ? "--" : reviewedLabel]++;
            if (eligible) eligibleCounts[reviewedLabel]++;
            if (i < maxRows) {
                browserRows.push_back({
                    i,
                    item.value("image_id").toString(QString("item_%1").arg(i + 1)),
                    item.value("crop_path").toString(item.value("path").toString()),
                    autoLabel,
                    item.value("auto_label_source").toString("--"),
                    reviewedLabel,
                    reviewState,
                    eligible ? "yes" : "no",
                    warnings.join("; "),
                    confidence >= 0.0 ? QString::number(confidence, 'f', 3) : QString("--"),
                    item.value("source_frame_path").toString(),
                    item.value("notes").toString(),
                    item.value("exclude_reason").toString()
                });
            }
        }
        report << QString("Builder manifest items: %1; displayed rows: %2").arg(items.size()).arg(maxRows);
        report << QString("Trainer eligible reviewed hit=%1 waste=%2; exclude=%3; unreviewed=%4")
            .arg(eligibleCounts.value("hit"))
            .arg(eligibleCounts.value("waste"))
            .arg(reviewedCounts.value("exclude"))
            .arg(reviewedCounts.value("unreviewed"));
        if (lowConfidenceCount) report << QString("Low-confidence auto-label suggestions: %1").arg(lowConfidenceCount);
        populateBuilderBalanceTable(autoCounts, reviewedCounts, eligibleCounts, items.size());
        updateBannerFromBuilderCounts(reviewedCounts, eligibleCounts, items.size());
    }

    void loadSummaryArtifacts(QStringList& report) {
        QDir metadataDir(QDir(datasetRoot).filePath("metadata"));
        const QString summaryPath = metadataDir.filePath("dataset_summary.json");
        if (QFileInfo::exists(summaryPath)) {
            QFile file(summaryPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
                if (err.error == QJsonParseError::NoError && doc.isObject()) {
                    QJsonObject root = doc.object();
                    report << "Summary artifact: " + summaryPath;
                    report << QString("Samples included=%1 excluded=%2")
                        .arg(root.value("samples_included").toVariant().toString(),
                             root.value("samples_excluded").toVariant().toString());
                    QJsonArray warnings = root.value("warnings").toArray();
                    for (const QJsonValue& warning : warnings) report << "Warning: " + warning.toString();
                }
            }
        }
        const QString balancePath = metadataDir.filePath("class_balance.csv");
        if (QFileInfo::exists(balancePath)) {
            report << "Class balance artifact: " + balancePath;
            loadClassBalanceCsv(balancePath);
        }
    }

    void loadClassBalanceCsv(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QTextStream ts(&file);
        ts.readLine();
        classBalanceTable->setRowCount(0);
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            if (line.trimmed().isEmpty()) continue;
            const QStringList cells = line.split(',');
            const int row = classBalanceTable->rowCount();
            classBalanceTable->insertRow(row);
            for (int col = 0; col < 6; ++col) {
                classBalanceTable->setItem(row, col, new QTableWidgetItem(col < cells.size() ? cells.at(col).trimmed() : QString()));
            }
        }
    }

    bool loadCropsCsv(QStringList& report) {
        QString csvPath = QDir(datasetRoot).filePath("metadata/labels.csv");
        if (!QFileInfo::exists(csvPath)) csvPath = QDir(datasetRoot).filePath("metadata/crops.csv");
        QFile file(csvPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QTextStream ts(&file);
        const QStringList columns = ts.readLine().split(',');
        const int pathCol = std::max(columns.indexOf("path"), columns.indexOf("crop_path"));
        int labelCol = columns.indexOf("reviewed_label");
        if (labelCol < 0) labelCol = columns.indexOf("label");
        int statusCol = columns.indexOf("review_state");
        if (statusCol < 0) statusCol = columns.indexOf("status");
        const int autoCol = columns.indexOf("auto_label");
        int count = 0;
        while (!ts.atEnd() && count < 500) {
            const QStringList cells = ts.readLine().split(',');
            auto cell = [&](int index) { return (index >= 0 && index < cells.size()) ? cells.at(index).trimmed() : QString("--"); };
            browserRows.push_back({-1, cell(columns.indexOf("image_id")), cell(pathCol), cell(autoCol), "--", cell(labelCol), cell(statusCol), "no", "--", "--", "--", "--", "--"});
            count++;
        }
        report << QString("Loaded CSV rows for browsing: %1").arg(count);
        return count > 0;
    }

    void populateCountsFromMap(const QMap<QString, int>& counts, const QMap<QString, QString>& displayByClass) {
        if (counts.isEmpty() || classBalanceTable->rowCount() > 0) return;
        for (auto it = counts.begin(); it != counts.end(); ++it) {
            const int row = classBalanceTable->rowCount();
            classBalanceTable->insertRow(row);
            classBalanceTable->setItem(row, 0, new QTableWidgetItem(it.key()));
            classBalanceTable->setItem(row, 1, new QTableWidgetItem(displayByClass.value(it.key(), it.key())));
            classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(it.value())));
            for (int col = 3; col < 6; ++col) classBalanceTable->setItem(row, col, new QTableWidgetItem("--"));
        }
    }

    void populateBuilderBalanceTable(const QMap<QString, int>& autoCounts,
                                     const QMap<QString, int>& reviewedCounts,
                                     const QMap<QString, int>& eligibleCounts,
                                     int totalItems) {
        classBalanceTable->setRowCount(0);
        const QStringList labels = {"hit", "waste", "exclude", "unreviewed"};
        for (const QString& label : labels) {
            const int row = classBalanceTable->rowCount();
            classBalanceTable->insertRow(row);
            const bool trainingLabel = label == "hit" || label == "waste";
            const int reviewed = reviewedCounts.value(label);
            const int eligible = eligibleCounts.value(label);
            QString warning;
            if (trainingLabel && eligible == 0) warning = "blocks handoff";
            if (label == "exclude" && totalItems > 0 && reviewed * 4 > totalItems) warning = ">25% excluded";
            classBalanceTable->setItem(row, 0, new QTableWidgetItem(label));
            classBalanceTable->setItem(row, 1, new QTableWidgetItem(QString::number(reviewed)));
            classBalanceTable->setItem(row, 2, new QTableWidgetItem(QString::number(eligible)));
            classBalanceTable->setItem(row, 3, new QTableWidgetItem(QString::number(autoCounts.value(label))));
            classBalanceTable->setItem(row, 4, new QTableWidgetItem(warning));
            classBalanceTable->setItem(row, 5, new QTableWidgetItem(trainingLabel ? "trainer class" : "not trainer-eligible"));
        }
    }

    void updateBannerFromBuilderCounts(const QMap<QString, int>& reviewedCounts,
                                       const QMap<QString, int>& eligibleCounts,
                                       int totalItems) {
        const int hit = eligibleCounts.value("hit");
        const int waste = eligibleCounts.value("waste");
        const int exclude = reviewedCounts.value("exclude");
        const int unreviewed = reviewedCounts.value("unreviewed");
        QStringList warnings;
        if (unreviewed > 0) warnings << QString("%1 crops still need manual review").arg(unreviewed);
        if (hit == 0 || waste == 0) warnings << "trainer handoff blocked until reviewed hit and waste both exist";
        const int minority = std::min(hit, waste);
        const int majority = std::max(hit, waste);
        if (minority > 0 && majority > 3 * minority) warnings << QString("class imbalance %1:%2").arg(majority).arg(minority);
        if (totalItems > 0 && exclude * 4 > totalItems) warnings << "more than 25% excluded";
        if (warnings.isEmpty()) bannerLabel->setText("Review status: reviewed hit/waste items are trainer-eligible; exclude and unreviewed items are retained but not handed to training.");
        else bannerLabel->setText("Review warnings: " + warnings.join("; "));
    }

    void addLegacyBrowserRow(const QString& path, const QString& label, const QString& status, const QString& origin, const QString& seed) {
        browserRows.push_back({-1, QFileInfo(path).fileName(), path, "--", origin, label, status, "no", seed, "--", "--", "--", "--"});
    }

    bool rowMatchesFilter(const BrowserRow& row) const {
        const QString mode = filterCombo ? filterCombo->currentText() : QString("All crops");
        const QString stateLower = row.reviewState.toLower();
        if (mode == "Unreviewed" && stateLower != "unreviewed") return false;
        if (mode == "Reviewed" && stateLower == "unreviewed") return false;
        if (mode == "Excluded" && row.reviewedLabel != "exclude" && stateLower != "excluded") return false;
        if (mode == "Auto hit" && row.autoLabel != "hit") return false;
        if (mode == "Auto waste" && row.autoLabel != "waste") return false;
        if (mode == "Low confidence" && !row.warnings.contains("low confidence", Qt::CaseInsensitive)) return false;
        if (mode == "Warnings" && row.warnings.trimmed().isEmpty()) return false;

        const QString needle = searchEdit ? searchEdit->text().trimmed().toLower() : QString();
        if (needle.isEmpty()) return true;
        const QString haystack = QStringList{row.imageId, row.cropPath, row.autoLabel, row.reviewedLabel, row.reviewState, row.eligible, row.warnings}.join(" ").toLower();
        return haystack.contains(needle);
    }

    void applyBrowserFilter() {
        const QString previousPath = selectedPath();
        browserTable->setRowCount(0);
        int restoreRow = -1;
        for (const BrowserRow& rowData : browserRows) {
            if (!rowMatchesFilter(rowData)) continue;
            const int row = browserTable->rowCount();
            browserTable->insertRow(row);
            const QStringList values = {rowData.imageId, rowData.cropPath, rowData.autoLabel, rowData.reviewedLabel, rowData.reviewState, rowData.eligible, rowData.warnings};
            for (int col = 0; col < values.size(); ++col) browserTable->setItem(row, col, new QTableWidgetItem(values.at(col)));
            browserTable->item(row, 0)->setData(Qt::UserRole, rowData.manifestIndex);
            if (!previousPath.isEmpty() && rowData.cropPath == previousPath) restoreRow = row;
        }
        browserTable->resizeColumnsToContents();
        if (restoreRow >= 0) {
            browserTable->selectRow(restoreRow);
        } else if (browserTable->rowCount() > 0) {
            browserTable->selectRow(0);
        } else {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText("No crop matches the current filter.");
            previewDetailsLabel->setText("No crop selected.");
            updateNavigationButtons();
        }
        updateLoadStatus();
    }

    void updateLoadStatus() {
        if (manifestPath.isEmpty()) {
            setLoadStatusText("Dataset Builder review load status: no manifest loaded");
            return;
        }
        QString datasetId = "--";
        if (manifestDoc.isObject()) {
            datasetId = manifestDoc.object().value("dataset_id").toString("--");
        }
        setLoadStatusText(QString("Dataset Builder manifest loaded: dataset_id=%1; items=%2; visible=%3; manifest=%4")
            .arg(datasetId)
            .arg(browserRows.size())
            .arg(browserTable ? browserTable->rowCount() : 0)
            .arg(QDir::toNativeSeparators(manifestPath)));
    }

    void setLoadStatusText(const QString& text) {
        if (loadStatusLabel) loadStatusLabel->setText(text);
        if (loadStatusEdit) loadStatusEdit->setText(text);
    }

    void updatePreviewFromSelection() {
        const QList<QTableWidgetItem*> selected = browserTable->selectedItems();
        if (selected.isEmpty()) {
            updateNavigationButtons();
            return;
        }
        const int row = selected.first()->row();
        const QString relPath = browserTable->item(row, 1) ? browserTable->item(row, 1)->text() : QString();
        const QString imageId = browserTable->item(row, 0) ? browserTable->item(row, 0)->text() : QString("--");
        const QString autoLabel = browserTable->item(row, 2) ? browserTable->item(row, 2)->text() : QString("--");
        const QString reviewedLabel = browserTable->item(row, 3) ? browserTable->item(row, 3)->text() : QString("--");
        const QString state = browserTable->item(row, 4) ? browserTable->item(row, 4)->text() : QString("--");
        const QString eligible = browserTable->item(row, 5) ? browserTable->item(row, 5)->text() : QString("--");
        const QString warnings = browserTable->item(row, 6) ? browserTable->item(row, 6)->text() : QString("--");
        const QString imagePath = QFileInfo(relPath).isAbsolute() ? relPath : QDir(datasetRoot).filePath(relPath);
        const BrowserRow data = rowDataForVisibleRow(row);
        previewDetailsLabel->setText(QString("Image ID: %1\nCrop: %2\nAuto-label: %3 (%4, confidence %5)\nReviewed label: %6\nReview state: %7\nTrainer eligible: %8\nWarnings: %9")
            .arg(imageId,
                 QDir::toNativeSeparators(relPath),
                 autoLabel,
                 data.autoSource,
                 data.confidence,
                 reviewedLabel.isEmpty() ? "--" : reviewedLabel,
                 state,
                 eligible,
                 warnings.isEmpty() ? "--" : warnings));
        if (notesEdit && !notesEdit->hasFocus()) notesEdit->setPlainText(data.notes);
        if (excludeReasonCombo && !data.excludeReason.isEmpty()) setComboTextIfPresent(excludeReasonCombo, data.excludeReason);
        QImageReader reader(imagePath);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull()) {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText("Preview unavailable\n" + relPath);
            updateNavigationButtons();
            return;
        }
        previewLabel->setText(QString());
        previewLabel->setPixmap(QPixmap::fromImage(img).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updateNavigationButtons();
        updateReviewControls();
    }

    QString selectedPath() const {
        const QList<QTableWidgetItem*> selected = browserTable ? browserTable->selectedItems() : QList<QTableWidgetItem*>();
        if (selected.isEmpty()) return QString();
        const int row = selected.first()->row();
        return browserTable->item(row, 1) ? browserTable->item(row, 1)->text() : QString();
    }

    void selectRelativeRow(int delta) {
        const int rows = browserTable->rowCount();
        if (rows <= 0) return;
        int row = browserTable->currentRow();
        if (row < 0) row = 0;
        row = std::clamp(row + delta, 0, rows - 1);
        browserTable->selectRow(row);
        browserTable->scrollToItem(browserTable->item(row, 0), QAbstractItemView::PositionAtCenter);
    }

    void updateNavigationButtons() {
        const int rows = browserTable ? browserTable->rowCount() : 0;
        const int row = browserTable ? browserTable->currentRow() : -1;
        if (prevButton) prevButton->setEnabled(rows > 0 && row > 0);
        if (nextButton) nextButton->setEnabled(rows > 0 && row >= 0 && row < rows - 1);
    }

    bool reviewShortcutAllowed() const {
        QWidget* focus = QApplication::focusWidget();
        if (!focus) return true;
        if (qobject_cast<QLineEdit*>(focus)) return false;
        if (qobject_cast<QPlainTextEdit*>(focus)) return false;
        if (qobject_cast<QTextEdit*>(focus)) return false;
        if (qobject_cast<QComboBox*>(focus)) return false;
        return true;
    }

    int selectedManifestIndex() const {
        const int row = browserTable ? browserTable->currentRow() : -1;
        if (row < 0 || !browserTable->item(row, 0)) return -1;
        return browserTable->item(row, 0)->data(Qt::UserRole).toInt();
    }

    BrowserRow rowDataForVisibleRow(int visibleRow) const {
        if (visibleRow < 0 || !browserTable || !browserTable->item(visibleRow, 0)) return {};
        const int manifestIndex = browserTable->item(visibleRow, 0)->data(Qt::UserRole).toInt();
        for (const BrowserRow& row : browserRows) {
            if (row.manifestIndex == manifestIndex && manifestIndex >= 0) return row;
        }
        return {};
    }

    void updateReviewControls() {
        const bool canReview = isBuilderManifest && selectedManifestIndex() >= 0;
        for (auto* button : {hitButton, wasteButton, excludeButton, acceptButton, saveButton}) {
            if (button) button->setEnabled(canReview);
        }
        if (undoButton) undoButton->setEnabled(canReview && !undoStack.isEmpty());
        if (excludeReasonCombo) excludeReasonCombo->setEnabled(canReview);
        if (notesEdit) notesEdit->setEnabled(canReview);
        if (!canReview && isBuilderManifest) {
            bannerLabel->setText("Select a crop row to review. Auto-labels are suggestions; only reviewed hit/waste rows are trainer-eligible.");
        } else if (!isBuilderManifest && !manifestDoc.isNull()) {
            bannerLabel->setText("Read-only legacy manifest inspection. Open a Dataset Builder manifest to edit reviewed_label.");
        }
    }

    void acceptAutoLabel() {
        const int index = selectedManifestIndex();
        if (index < 0) return;
        const QJsonArray items = manifestDoc.object().value("items").toArray();
        if (index >= items.size()) return;
        const QString autoLabel = normalizedLabel(items.at(index).toObject().value("auto_label").toString());
        if (autoLabel == "hit" || autoLabel == "waste") applyReviewLabel(autoLabel, true);
    }

    void applyReviewLabel(const QString& label, bool advance) {
        if (!isBuilderManifest) return;
        const int index = selectedManifestIndex();
        if (index < 0) return;
        QJsonObject root = manifestDoc.object();
        QJsonArray items = root.value("items").toArray();
        if (index >= items.size()) return;
        QJsonObject item = items.at(index).toObject();
        undoStack.push_back({index, item});
        const QString normalized = normalizedLabel(label);
        const QString autoLabel = normalizedLabel(item.value("auto_label").toString());
        item["reviewed_label"] = normalized;
        item["reviewed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        item["review_state"] = normalized == "exclude" ? "excluded" : (normalized == autoLabel ? "confirmed" : "relabeled");
        item["trainer_eligible"] = (normalized == "hit" || normalized == "waste");
        item["notes"] = notesEdit ? notesEdit->toPlainText().trimmed() : QString();
        if (normalized == "exclude") {
            item["exclude_reason"] = excludeReasonCombo ? excludeReasonCombo->currentText() : QString("other");
        } else {
            item.remove("exclude_reason");
        }
        items.replace(index, item);
        root["items"] = items;
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifestDoc = QJsonDocument(root);
        rebuildRowsFromCurrentManifest();
        applyBrowserFilter();
        saveManifestAndLabels(false);
        if (advance) selectRelativeRow(1);
    }

    void undoLastReviewEdit() {
        if (undoStack.isEmpty()) return;
        ReviewUndo undo = undoStack.takeLast();
        QJsonObject root = manifestDoc.object();
        QJsonArray items = root.value("items").toArray();
        if (undo.manifestIndex < 0 || undo.manifestIndex >= items.size()) return;
        items.replace(undo.manifestIndex, undo.previousItem);
        root["items"] = items;
        root["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifestDoc = QJsonDocument(root);
        rebuildRowsFromCurrentManifest();
        applyBrowserFilter();
        saveManifestAndLabels(false);
    }

    void rebuildRowsFromCurrentManifest() {
        QStringList report;
        browserRows.clear();
        classBalanceTable->setRowCount(0);
        loadBuilderManifest(manifestDoc.object(), report);
    }

    bool saveManifestAndLabels(bool showMessage = true) {
        if (!isBuilderManifest || manifestPath.isEmpty()) return false;
        QFile file(manifestPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            if (showMessage) QMessageBox::warning(this, "Save failed", "Could not write manifest:\n" + manifestPath);
            return false;
        }
        file.write(manifestDoc.toJson(QJsonDocument::Indented));
        file.close();
        writeLabelsCsv();
        if (showMessage) outputText->setPlainText("Saved Dataset Builder review manifest and labels.csv:\n" + manifestPath);
        return true;
    }

    void writeLabelsCsv() {
        QDir metadataDir(QDir(datasetRoot).filePath("metadata"));
        metadataDir.mkpath(".");
        QFile file(metadataDir.filePath("labels.csv"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return;
        QTextStream ts(&file);
        ts << "image_id,crop_path,source_frame_path,source_frame_id,timestamp,crop_x,crop_y,crop_w,crop_h,collection_mode,batch_index,auto_label,auto_label_source,auto_label_confidence,auto_label_model_id,review_state,reviewed_label,exclude_reason,trainer_eligible,hash_sha256\n";
        const QJsonArray items = manifestDoc.object().value("items").toArray();
        for (const QJsonValue& value : items) {
            const QJsonObject item = value.toObject();
            const QJsonArray rect = item.value("crop_rect").toArray();
            QStringList cols;
            cols << item.value("image_id").toString()
                 << item.value("crop_path").toString()
                 << item.value("source_frame_path").toString()
                 << item.value("source_frame_id").toVariant().toString()
                 << item.value("timestamp").toString()
                 << (rect.size() > 0 ? rect.at(0).toVariant().toString() : QString())
                 << (rect.size() > 1 ? rect.at(1).toVariant().toString() : QString())
                 << (rect.size() > 2 ? rect.at(2).toVariant().toString() : QString())
                 << (rect.size() > 3 ? rect.at(3).toVariant().toString() : QString())
                 << item.value("collection_mode").toString()
                 << item.value("batch_index").toVariant().toString()
                 << item.value("auto_label").toString()
                 << item.value("auto_label_source").toString()
                 << item.value("auto_label_confidence").toVariant().toString()
                 << item.value("auto_label_model_id").toString()
                 << item.value("review_state").toString("unreviewed")
                 << item.value("reviewed_label").toString()
                 << item.value("exclude_reason").toString()
                 << (item.value("trainer_eligible").toBool(false) ? "true" : "false")
                 << item.value("hash_sha256").toString();
            for (int i = 0; i < cols.size(); ++i) cols[i] = csvEscape(cols.at(i));
            ts << cols.join(',') << "\n";
        }
    }

    QString normalizedLabel(const QString& label) const {
        const QString lower = label.trimmed().toLower();
        if (lower == "hits" || lower == "hit" || lower == "1") return "hit";
        if (lower == "waste" || lower == "empty" || lower == "0") return "waste";
        if (lower == "exclude" || lower == "excluded" || lower == "reject" || lower == "rejected") return "exclude";
        if (lower.isEmpty()) return QString();
        return lower;
    }

    QString csvEscape(QString text) const {
        const bool quote = text.contains(',') || text.contains('"') || text.contains('\n') || text.contains('\r');
        text.replace("\"", "\"\"");
        return quote ? "\"" + text + "\"" : text;
    }

    QLabel* pathLabel = nullptr;
    QLabel* bannerLabel = nullptr;
    QComboBox* filterCombo = nullptr;
    QLineEdit* searchEdit = nullptr;
    QPushButton* hitButton = nullptr;
    QPushButton* wasteButton = nullptr;
    QPushButton* excludeButton = nullptr;
    QPushButton* acceptButton = nullptr;
    QPushButton* undoButton = nullptr;
    QPushButton* saveButton = nullptr;
    QComboBox* excludeReasonCombo = nullptr;
    QPlainTextEdit* notesEdit = nullptr;
    QPushButton* prevButton = nullptr;
    QPushButton* nextButton = nullptr;
    QTableWidget* browserTable = nullptr;
    QTableWidget* classBalanceTable = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* previewDetailsLabel = nullptr;
    QLabel* loadStatusLabel = nullptr;
    QLineEdit* loadStatusEdit = nullptr;
    QPlainTextEdit* outputText = nullptr;
    QVector<BrowserRow> browserRows;
    QVector<ReviewUndo> undoStack;
    QJsonDocument manifestDoc;
    QString currentDatasetPath;
    QString manifestPath;
    QString datasetRoot;
    bool isBuilderManifest = false;
};

class ImageValidationDialog : public QDialog {
public:
    ImageValidationDialog(QWidget* parent,
                          const QString& initialPython,
                          const QString& initialModel,
                          const QString& initialMetadata,
                          const QString& initialDataset,
                          const QString& initialOutput,
                          const QString& trainerPythonPath)
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

        auto* note = new QLabel("Sequence validation remains unavailable here: runner-wrapped replay is not implemented, and existing artifact comparison is internal/provisional only.");
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

        auto update = [this](){
            terminalStatus.clear();
            updatePreviewAndGate();
        };
        for (auto* edit : {pythonEdit, modelEdit, metadataEdit, datasetEdit, outputEdit}) {
            QObject::connect(edit, &QLineEdit::textChanged, update);
        }
        QObject::connect(classesEdit, &QLineEdit::textChanged, update);
        QObject::connect(deviceCombo, &QComboBox::currentTextChanged, update);
        QObject::connect(schemaCombo, &QComboBox::currentTextChanged, [this, update](){
            classesEdit->setEnabled(schemaCombo->currentIndex() == 2);
            update();
        });
        QObject::connect(startButton, &QPushButton::clicked, [this](){ startValidation(); });
        QObject::connect(cancelButton, &QPushButton::clicked, [this](){ cancelValidation(); });
        QObject::connect(openSummaryButton, &QPushButton::clicked, [this](){
            if (!summaryPath.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(summaryPath));
        });
        QObject::connect(openOutputButton, &QPushButton::clicked, [this](){
            QString path = outputEdit->text().trimmed();
            if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
        });

        loadSettings();
        classesEdit->setEnabled(schemaCombo->currentIndex() == 2);
        updatePreviewAndGate();
    }

    ~ImageValidationDialog() override {
        stopProcess(1000);
    }

private:
    void addPathRow(QGridLayout* layout, int row, const QString& label, QLineEdit* edit, bool directory, const QString& dialogTitle) {
        auto* browse = new QPushButton("Browse");
        layout->addWidget(new QLabel(label), row, 0);
        layout->addWidget(edit, row, 1);
        layout->addWidget(browse, row, 2);
        QObject::connect(browse, &QPushButton::clicked, this, [this, edit, directory, dialogTitle](){
            QString current = edit->text().trimmed();
            QString selected;
            if (directory) {
                selected = QFileDialog::getExistingDirectory(this, dialogTitle, current);
            } else {
                selected = QFileDialog::getOpenFileName(this, dialogTitle, current);
            }
            if (!selected.isEmpty()) edit->setText(QDir::toNativeSeparators(selected));
        });
    }

    QStringList commandArguments() const {
        QStringList args = {
            "-m", "droplet_trainer",
            "validate-images",
            "--model", modelEdit->text().trimmed(),
            "--metadata", metadataEdit->text().trimmed(),
            "--dataset", datasetEdit->text().trimmed(),
            "--output", outputEdit->text().trimmed(),
            "--device", deviceCombo->currentText(),
            "--json"
        };
        if (schemaCombo->currentIndex() == 1) {
            args << "--legacy-schema";
        } else if (schemaCombo->currentIndex() == 2) {
            args << "--classes" << classesEdit->text().trimmed();
        }
        return args;
    }

    QString missingInputs() const {
        QStringList missing;
        if (pythonEdit->text().trimmed().isEmpty()) missing << "Python executable";
        if (!QFileInfo(modelEdit->text().trimmed()).isFile()) missing << "model file";
        if (!QFileInfo(metadataEdit->text().trimmed()).isFile()) missing << "metadata file";
        if (!QFileInfo(datasetEdit->text().trimmed()).isDir()) missing << "dataset folder";
        if (outputEdit->text().trimmed().isEmpty()) missing << "output folder";
        if (schemaCombo->currentIndex() == 2 && classesEdit->text().trimmed().isEmpty()) missing << "custom class list";
        if (!pythonPath.isEmpty() && !QFileInfo(pythonPath).isDir()) missing << "training/python module path";
        return missing.join(", ");
    }

    void updatePreviewAndGate() {
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

    void loadSettings() {
        QSettings settings;
        pythonEdit->setText(settings.value("validator/pythonExecutable", pythonEdit->text()).toString());
        datasetEdit->setText(settings.value("validator/imageDataset", datasetEdit->text()).toString());
        outputEdit->setText(settings.value("validator/outputFolder", outputEdit->text()).toString());
        const QString device = settings.value("validator/device", deviceCombo->currentText()).toString();
        int index = deviceCombo->findText(device);
        if (index >= 0) deviceCombo->setCurrentIndex(index);
        schemaCombo->setCurrentIndex(settings.value("validator/schemaMode", 0).toInt());
        classesEdit->setText(settings.value("validator/classes", classesEdit->text()).toString());
    }

    void saveSettings() const {
        QSettings settings;
        settings.setValue("validator/pythonExecutable", pythonEdit->text().trimmed());
        settings.setValue("validator/imageDataset", datasetEdit->text().trimmed());
        settings.setValue("validator/outputFolder", outputEdit->text().trimmed());
        settings.setValue("validator/device", deviceCombo->currentText());
        settings.setValue("validator/schemaMode", schemaCombo->currentIndex());
        settings.setValue("validator/classes", classesEdit->text().trimmed());
    }

    void startValidation() {
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
        QObject::connect(process.get(), &QProcess::readyReadStandardOutput, this, [this](){
            appendLog(QString::fromUtf8(process->readAllStandardOutput()));
        });
        QObject::connect(process.get(), &QProcess::readyReadStandardError, this, [this](){
            appendLog(QString::fromUtf8(process->readAllStandardError()));
        });
        QObject::connect(process.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error){
            Q_UNUSED(error);
            terminalStatus = "Failed to start validator: " + process->errorString();
            statusLabel->setText(terminalStatus);
            appendLog("PROCESS ERROR: " + process->errorString() + "\n");
            updatePreviewAndGate();
        });
        QObject::connect(process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         this, [this](int exitCode, QProcess::ExitStatus exitStatus){
            finishValidation(exitCode, exitStatus);
        });
        canceled = false;
        process->start(pythonEdit->text().trimmed(), commandArguments());
        updatePreviewAndGate();
    }

    void cancelValidation() {
        if (!process || process->state() == QProcess::NotRunning) return;
        canceled = true;
        statusLabel->setText("Canceling image validation...");
        stopProcess(2500);
    }

    void stopProcess(int timeoutMs) {
        if (!process || process->state() == QProcess::NotRunning) return;
        process->terminate();
        if (!process->waitForFinished(timeoutMs)) {
            process->kill();
            process->waitForFinished(1000);
        }
    }

    void finishValidation(int exitCode, QProcess::ExitStatus exitStatus) {
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

    void appendLog(const QString& text) {
        if (text.isEmpty()) return;
        logText->moveCursor(QTextCursor::End);
        logText->insertPlainText(text);
        logText->moveCursor(QTextCursor::End);
    }

    bool loadSummaryArtifacts() {
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
        artifactLabel->setText(QString("Artifacts: %1\nSummary status: %2%3\nExpected CSVs: predictions.csv, confusion_matrix.csv, class_metrics.csv, failure_cases.csv")
            .arg(summaryPath, status, metrics));
        openSummaryButton->setEnabled(true);
        openOutputButton->setEnabled(true);
        return true;
    }

    QLineEdit* pythonEdit = nullptr;
    QLineEdit* modelEdit = nullptr;
    QLineEdit* metadataEdit = nullptr;
    QLineEdit* datasetEdit = nullptr;
    QLineEdit* outputEdit = nullptr;
    QComboBox* deviceCombo = nullptr;
    QComboBox* schemaCombo = nullptr;
    QLineEdit* classesEdit = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* artifactLabel = nullptr;
    QPlainTextEdit* commandPreview = nullptr;
    QPlainTextEdit* logText = nullptr;
    QPushButton* startButton = nullptr;
    QPushButton* cancelButton = nullptr;
    QPushButton* openSummaryButton = nullptr;
    QPushButton* openOutputButton = nullptr;
    std::unique_ptr<QProcess> process;
    QString pythonPath;
    QString summaryPath;
    QString terminalStatus;
    bool canceled = false;
};

class MainWindowCloseFilter : public QObject {
public:
    explicit MainWindowCloseFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Close) {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (!widget || widget == watched || !widget->isVisible()) continue;
                if (qobject_cast<QDialog*>(widget)) {
                    widget->close();
                }
            }
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
        return QObject::eventFilter(watched, event);
    }
};

void pruneLogs() {
    QFileInfo fi(gLogFile);
    QString baseDir = fi.dir().absolutePath();
    QString baseName = "session_log";
    QStringList files = QDir(baseDir).entryList(QStringList() << (baseName + "*.txt"), QDir::Files, QDir::Time);
    for (int i = 50; i < files.size(); ++i) {
        QString path = baseDir + "/" + files[i];
        QFile file(path);
        if (!file.remove()) {
            logMessageNoPrune(QString("Failed to remove log file %1: %2").arg(path, file.errorString()));
        }
    }
}

void logMessage(const QString& msg) {
    QMutexLocker locker(&gLogMutex);
    if (!gLogFile.isOpen()) {
        if (!gLogFile.open(QIODevice::Append | QIODevice::Text)) return;
    }
    QTextStream ts(&gLogFile);
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + " " + msg;
    ts << line << "\n";
    ts.flush();
    gLogFile.close();
    pruneLogs();
}

void logMessageNoPrune(const QString& msg) {
    QMutexLocker locker(&gLogMutex);
    if (!gLogFile.isOpen()) {
        if (!gLogFile.open(QIODevice::Append | QIODevice::Text)) return;
    }
    QTextStream ts(&gLogFile);
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + " " + msg;
    ts << line << "\n";
    ts.flush();
    gLogFile.close();
}

void qtLogHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    Q_UNUSED(ctx);
    QString level;
    switch (type) {
        case QtDebugMsg: level="DEBUG"; break;
        case QtInfoMsg: level="INFO"; break;
        case QtWarningMsg: level="WARN"; break;
        case QtCriticalMsg: level="CRIT"; break;
        case QtFatalMsg: level="FATAL"; break;
    }
    logMessage(QString("[%1] %2").arg(level, msg));
}

void termHandler() {
    logMessage("std::terminate called");
    std::_Exit(1);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info) {
    if (gCrashHandled.exchange(true)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void* addr = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    logMessage(QString("Unhandled exception: code=0x%1 addr=0x%2")
        .arg(code, 8, 16, QChar('0'))
        .arg(reinterpret_cast<quintptr>(addr), sizeof(quintptr) * 2, 16, QChar('0')));

    wchar_t basePath[MAX_PATH] = {};
    DWORD len = GetTempPathW(MAX_PATH, basePath);
    if (len == 0 || len >= MAX_PATH) {
        DWORD mlen = GetModuleFileNameW(nullptr, basePath, MAX_PATH);
        if (mlen > 0 && mlen < MAX_PATH) {
            for (DWORD i = mlen; i > 0; --i) {
                if (basePath[i] == L'\\' || basePath[i] == L'/') {
                    basePath[i + 1] = L'\0';
                    break;
                }
            }
        } else {
            basePath[0] = L'.';
            basePath[1] = L'\\';
            basePath[2] = L'\0';
        }
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t fileName[128] = {};
    swprintf(fileName, 128, L"droplet_crash_%04d%02d%02d_%02d%02d%02d.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    wchar_t dumpPath[MAX_PATH] = {};
    swprintf(dumpPath, MAX_PATH, L"%s%s", basePath, fileName);

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpNormal, info ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(hFile);
        logMessage(QString("Crash dump saved: %1").arg(QString::fromWCharArray(dumpPath)));
    } else {
        DWORD err = GetLastError();
        logMessage(QString("Failed to create crash dump file. err=%1").arg(err));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void installLogTees() {
    static LogTeeBuf loggerBuf(std::cout.rdbuf(), [](const QString& m){ logMessage(m); });
    static std::ostream loggerStream(&loggerBuf);
    std::cout.rdbuf(loggerStream.rdbuf());
    std::cerr.rdbuf(loggerStream.rdbuf());
}
} // namespace


int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cli") {
#ifdef _WIN32
            if (GetConsoleWindow() == nullptr) {
                AllocConsole();
                FILE* out = nullptr;
                FILE* err = nullptr;
                freopen_s(&out, "CONOUT$", "w", stdout);
                freopen_s(&err, "CONOUT$", "w", stderr);
            }
#endif
            return run_cli(argc, argv);
        }
    }
    AppOptions options = parseAppOptions(argc, argv);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Hamamatsu");
    QCoreApplication::setApplicationName("OpenVisualDropletSorter");
    QSettings runtimeSettings;
    desktop_app::AppState appState;
    appState.targetClassId = runtimeSettings.value("runtime/v1/model/targetClassId", QStringLiteral("1")).toString().trimmed();
    if (appState.targetClassId.isEmpty()) appState.targetClassId = QStringLiteral("1");
    appState.testMode = options.testMode || runtimeSettings.value("settings/testMode", false).toBool();
    appState.daqDisabled = options.noDaq;
#ifdef HAVE_NIDAQMX
    constexpr bool kDaqBuildEnabled = true;
#else
    constexpr bool kDaqBuildEnabled = false;
#endif
    const QString initialDaqStatusText = options.noDaq ? QStringLiteral("DAQ: disabled")
        : (kDaqBuildEnabled ? QStringLiteral("DAQ: unchecked") : QStringLiteral("DAQ: unavailable"));
    appState.daqFault = !options.noDaq && !kDaqBuildEnabled;
    appState.daqStatusText = initialDaqStatusText;
    QString registryFilePath;
    QString registryLoadWarning;
    QJsonObject modelRegistry = loadModelRegistry(&registryFilePath, &registryLoadWarning);
    QJsonArray registryEntries = modelRegistry.value("entries").toArray();
    if (registryEntries.isEmpty()) {
        modelRegistry = temporaryStaticModelRegistry();
        registryEntries = modelRegistry.value("entries").toArray();
        registryLoadWarning = "Model registry had no rows; using temporary static fallback.";
    }
    const DefaultWorkspacePaths defaultWorkspacePaths = ensureDefaultWorkspaceAssets(registryEntries);

    QPixmap splashPixmap(560, 340);
    splashPixmap.fill(QColor("#0B1F5E"));
    {
        QPainter painter(&splashPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPixmap icon(":/branding/opendss-icon-512.png");
        if (!icon.isNull()) {
            painter.drawPixmap(QRect(42, 38, 76, 76), icon, icon.rect());
        }
        painter.setPen(QColor("#FFFFFF"));
        QFont titleFont("Inter", 26, QFont::Bold);
        painter.setFont(titleFont);
        painter.drawText(QRect(136, 46, 360, 38), Qt::AlignLeft | Qt::AlignVCenter, "OpenDSS");
        QFont descriptorFont("Inter", 13, QFont::Medium);
        painter.setFont(descriptorFont);
        painter.setPen(QColor("#E5E7EB"));
        painter.drawText(QRect(138, 86, 360, 26), Qt::AlignLeft | Qt::AlignVCenter,
                         "Open Visual Droplet Sorter Suite");
        QPen flowPen(QColor("#7DD3FC"));
        flowPen.setWidth(2);
        painter.setPen(flowPen);
        painter.drawLine(QPointF(54, 250), QPointF(156, 220));
        painter.drawLine(QPointF(156, 220), QPointF(264, 238));
        painter.drawLine(QPointF(264, 238), QPointF(386, 200));
        painter.setBrush(QColor("#2563EB"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(54, 250), 5, 5);
        painter.drawEllipse(QPointF(156, 220), 6, 6);
        painter.setBrush(QColor("#14B8A6"));
        painter.drawEllipse(QPointF(264, 238), 6, 6);
        painter.drawEllipse(QPointF(386, 200), 5, 5);
        painter.setBrush(QColor("#2563EB"));
        painter.drawRoundedRect(QRect(42, 286, 210, 5), 2, 2);
        painter.setBrush(QColor("#14B8A6"));
        painter.drawRoundedRect(QRect(252, 286, 126, 5), 2, 2);
        painter.setFont(QFont("Inter", 11, QFont::Medium));
        painter.setPen(QColor("#FFFFFF"));
        painter.drawText(QRect(42, 304, 360, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         "Loading instrument modules...");
    }
    QSplashScreen splash(splashPixmap);
    splash.setObjectName("OpenDssSplashScreen");
    QElapsedTimer splashTimer;
    splashTimer.start();
    splash.show();
    app.processEvents();

    gLogPath = QCoreApplication::applicationDirPath() + "/session_log.txt";
    gLogFile.setFileName(gLogPath);
    if (QFile::exists(gLogPath)) QFile::remove(gLogPath);
    pruneLogs();
    qInstallMessageHandler(qtLogHandler);
    std::set_terminate(termHandler);
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    installLogTees();
    logMessage(QString("Log file: %1").arg(gLogPath));

    QMainWindow window;
    nameWidget(&window, "MainWindow");
    window.setWindowTitle("Open Visual Droplet Sorter Suite");
    window.setWindowIcon(QIcon(":/branding/opendss-icon-512.png"));
    auto* mainWindowCloseFilter = new MainWindowCloseFilter(&window);
    window.installEventFilter(mainWindowCloseFilter);
    window.resize(1280, 720);
    window.setMinimumSize(1100, 650);
    bool viewerOnly = false;
    const bool hardwareFreeMode = options.testMode || options.mockCamera;
    auto currentThemeMode =
        runtimeSettings.value("shell/theme", "dark").toString().compare("light", Qt::CaseInsensitive) == 0
            ? desktop_app::theme::ThemeMode::Light
            : desktop_app::theme::ThemeMode::Dark;
    auto applyShellTheme = [&]() {
        app.setPalette(desktop_app::theme::palette(currentThemeMode));
        window.setStyleSheet(desktop_app::theme::shellStyleSheet(currentThemeMode));
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
    auto stopBtn = new QPushButton("Stop");
    auto pipelineStartBtn = new QPushButton("Start Sorting");
    auto pipelineStopBtn = new QPushButton("Stop Sorting");
    pipelineStopBtn->setEnabled(false);
    auto reconnectBtn = new QPushButton("Reconnect");
    auto applyBtn = new QPushButton("Apply Camera Settings");
    auto viewerBtn = new QPushButton("Viewer");
    nameWidget(startBtn, "CameraStartButton");
    nameWidget(stopBtn, "CameraStopButton");
    nameWidget(pipelineStartBtn, "PipelineStartButton");
    nameWidget(pipelineStopBtn, "PipelineStopButton");
    nameWidget(reconnectBtn, "CameraReconnectButton");
    nameWidget(applyBtn, "CameraApplySettingsButton");
    nameWidget(viewerBtn, "OpenViewerButton");

    auto addDisabledAction = [](QMenu* menu, const QString& text, const char* objectName, const QString& statusTip = QString()) {
        QAction* action = menu->addAction(text);
        nameAction(action, objectName);
        action->setEnabled(false);
        if (!statusTip.isEmpty()) action->setStatusTip(statusTip);
        return action;
    };

    auto fileMenu = window.menuBar()->addMenu("&File");
    auto openViewerAction = fileMenu->addAction("Open &Viewer");
    auto openOutputAction = fileMenu->addAction("Open Current &Output Folder");
    fileMenu->addSeparator();
    addDisabledAction(fileMenu, "New Project", "FileNewProjectAction", "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Open Project", "FileOpenProjectAction", "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Recent Projects", "FileRecentProjectsAction", "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Save Session", "FileSaveSessionAction", "Session packaging is a future workflow.");
    addDisabledAction(fileMenu, "Export Support Bundle", "FileExportSupportBundleAction", "Support bundle export is a future workflow.");
    fileMenu->addSeparator();
    auto exitAction = fileMenu->addAction("E&xit");

    auto cameraMenu = window.menuBar()->addMenu("&Camera");
    auto reconnectAction = cameraMenu->addAction("&Reconnect");
    auto startPreviewAction = cameraMenu->addAction("Start Preview");
    auto stopPreviewAction = cameraMenu->addAction("Stop Preview");
    auto captureStillAction = cameraMenu->addAction("Capture Still");
    addDisabledAction(cameraMenu, "Camera Presets", "CameraPresetsAction", "Camera preset management is not wired in this shell step.");

    auto datasetMenu = window.menuBar()->addMenu("&Dataset");
    addDisabledAction(datasetMenu, "New Dataset", "DatasetNewDatasetAction", "Dataset workflows are placeholder-only in this shell step.");
    auto datasetOpenAction = datasetMenu->addAction("Open Dataset");
    datasetOpenAction->setStatusTip("Open the Dataset Builder review workspace.");
    auto datasetBuildAction = datasetMenu->addAction("Build Dataset");
    datasetBuildAction->setStatusTip("Open collected crops for Dataset Builder manual review.");
    addDisabledAction(datasetMenu, "Import Images", "DatasetImportImagesAction", "Dataset workflows are placeholder-only in this shell step.");
    auto datasetCaptureFromCameraAction = datasetMenu->addAction("Capture From Camera");
    nameAction(datasetCaptureFromCameraAction, "DatasetCaptureFromCameraAction");
    datasetCaptureFromCameraAction->setStatusTip("Start a live Dataset Builder capture session from the camera stream.");
    auto datasetLabelDatasetAction = datasetMenu->addAction("Label Dataset");
    datasetLabelDatasetAction->setStatusTip("Open the Dataset Builder review workspace. Builder manifests can save reviewed labels.");
    auto datasetReadinessAction = datasetMenu->addAction("Readiness Check");

    auto trainingMenu = window.menuBar()->addMenu("&Training");
    auto trainingEnvironmentSettingsAction = trainingMenu->addAction("Training Environment Settings");
    auto trainingValidateEnvironmentAction = trainingMenu->addAction("Validate Environment");
    auto trainingNewRunAction = addDisabledAction(trainingMenu, "New Training Run", "TrainingNewRunAction", "Full GUI-launched training is intentionally unavailable in this readiness prototype.");
    addDisabledAction(trainingMenu, "Open Training Output", "TrainingOpenOutputAction", "Trainer outputs are not wired in this shell step.");

    auto validationMenu = window.menuBar()->addMenu("&Validation");
    auto imageValidationAction = validationMenu->addAction("Image Validation");
    imageValidationAction->setStatusTip("Launch image-level ONNX validation through the external Python validator.");
    auto modelManagerAction = validationMenu->addAction("Model Manager");
    modelManagerAction->setStatusTip("Open the read-only model registry and promotion gate view.");
    auto sequenceValidationAction = addDisabledAction(validationMenu, "Sequence Validation", "ValidationSequenceValidationAction", "Runner-wrapped sequence validation is not available; artifact comparison remains internal/provisional.");
    addDisabledAction(validationMenu, "Compare Models", "ValidationCompareModelsAction", "Model comparison is not wired in this shell step.");
    addDisabledAction(validationMenu, "Export Validation Report", "ValidationExportReportAction", "Validation reports are not wired in this shell step.");

    auto sortingMenu = window.menuBar()->addMenu("&Sorting");
    auto startSortingAction = sortingMenu->addAction("Start Sorting");
    auto stopSortingAction = sortingMenu->addAction("Stop Sorting");
    auto triggerDisabledAction = addDisabledAction(sortingMenu, "Trigger Disabled", "SortingTriggerDisabledAction", "DAQ trigger output is disabled until manually armed.");
    auto armTriggerAction = addDisabledAction(sortingMenu, "Arm Trigger", "SortingArmTriggerAction", "Trigger arming is not introduced in this declutter pass.");
    auto manualTriggerAction = sortingMenu->addAction("Manual Trigger");
    auto openRunFolderAction = sortingMenu->addAction("Open Run Folder");

    auto viewMenu = window.menuBar()->addMenu("&View");
    auto showLogsAction = viewMenu->addAction("Show Logs Dock");
    auto showDiagnosticsAction = viewMenu->addAction("Show Diagnostics Dock");
    viewMenu->addSeparator();
    auto resetLayoutAction = viewMenu->addAction("Reset Layout");

    auto settingsMenu = window.menuBar()->addMenu("&Settings");
    addDisabledAction(settingsMenu, "Preferences", "SettingsPreferencesAction", "Preferences are placeholder-only in this shell step.");
    addDisabledAction(settingsMenu, "Paths", "SettingsPathsAction", "Path settings are still controlled by the existing runtime fields.");
    addDisabledAction(settingsMenu, "Hardware Configuration", "SettingsHardwareConfigurationAction", "Hardware settings are still controlled by the existing runtime fields.");

    auto toolsMenu = window.menuBar()->addMenu("&Tools");
    auto systemDiagnosticsAction = toolsMenu->addAction("System Diagnostics");
    addDisabledAction(toolsMenu, "Model Artifact Verification", "ToolsModelArtifactVerificationAction", "Model verification is not wired in this shell step.");
    addDisabledAction(toolsMenu, "Dataset Manifest Verification", "ToolsDatasetManifestVerificationAction", "Dataset verification is not wired in this shell step.");

    auto helpMenu = window.menuBar()->addMenu("&Help");
    auto aboutAction = helpMenu->addAction("&About");

    nameWidget(window.menuBar(), "MainMenuBar");
    nameObject(fileMenu, "FileMenu");
    nameObject(cameraMenu, "CameraMenu");
    nameObject(datasetMenu, "DatasetMenu");
    nameObject(trainingMenu, "TrainingMenu");
    nameObject(validationMenu, "ValidationMenu");
    nameAction(imageValidationAction, "ValidationImageValidationAction");
    nameAction(modelManagerAction, "ValidationModelManagerAction");
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

    auto commandStrip = new QToolBar("Command Strip", &window);
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
    window.addToolBar(Qt::TopToolBarArea, commandStrip);

    auto displayStrip = new QToolBar("Display Tools", &window);
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
    auto overlayAction = displayStrip->addAction("Overlay");
    overlayAction->setCheckable(true);
    overlayAction->setChecked(true);
    auto crosshairAction = displayStrip->addAction("Crosshair");
    crosshairAction->setCheckable(true);
    displayStrip->addSeparator();
    auto calibrationAction = displayStrip->addAction("Calibration");
    auto clearOverlayAction = displayStrip->addAction("Clear");
    nameAction(copyFrameAction, "DisplayCopyFrameAction");
    nameAction(copyDocumentAction, "DisplayCopyDocumentAction");
    nameAction(fitAction, "DisplayFitAction");
    nameAction(oneToOneAction, "DisplayOneToOneAction");
    nameAction(zoomInAction, "DisplayZoomInAction");
    nameAction(zoomOutAction, "DisplayZoomOutAction");
    nameAction(imageRegionAction, "DisplayImageRegionAction");
    nameAction(overlayAction, "DisplayOverlayAction");
    nameAction(crosshairAction, "DisplayCrosshairAction");
    nameAction(calibrationAction, "DisplayCalibrationAction");
    nameAction(clearOverlayAction, "DisplayClearOverlayAction");
    for (auto* action : {copyFrameAction, copyDocumentAction, fitAction, oneToOneAction,
                         zoomInAction, zoomOutAction, imageRegionAction, overlayAction,
                         crosshairAction, calibrationAction, clearOverlayAction}) {
        action->setStatusTip("Display shell control; runtime behavior is unchanged in this alignment step.");
    }
    fitAction->setStatusTip("Fit the live image inside the available viewer area.");
    overlayAction->setStatusTip("Show or hide the Live detection overlay.");
    crosshairAction->setStatusTip("Show or hide the Live center crosshair.");
    window.addToolBarBreak(Qt::TopToolBarArea);
    window.addToolBar(Qt::TopToolBarArea, displayStrip);
    commandStrip->hide();
    displayStrip->hide();

    // Settings controls
    auto presetCombo = new QComboBox;
    presetCombo->addItem("2304 x 2304", QVariant::fromValue(QSize(2304,2304)));
    presetCombo->addItem("2304 x 1152", QVariant::fromValue(QSize(2304,1152)));
    presetCombo->addItem("2304 x 576", QVariant::fromValue(QSize(2304,576)));
    presetCombo->addItem("2304 x 288", QVariant::fromValue(QSize(2304,288)));
    presetCombo->addItem("2304 x 144", QVariant::fromValue(QSize(2304,144)));
    presetCombo->addItem("2304 x 72", QVariant::fromValue(QSize(2304,72)));
    presetCombo->addItem("2304 x 36", QVariant::fromValue(QSize(2304,36)));
    presetCombo->addItem("2304 x 16", QVariant::fromValue(QSize(2304,16)));
    presetCombo->addItem("2304 x 8", QVariant::fromValue(QSize(2304,8)));
    presetCombo->addItem("2304 x 4", QVariant::fromValue(QSize(2304,4)));
    presetCombo->addItem("1152 x 1152", QVariant::fromValue(QSize(1152,1152)));
    presetCombo->addItem("1152 x 576", QVariant::fromValue(QSize(1152,576)));
    presetCombo->addItem("1152 x 288", QVariant::fromValue(QSize(1152,288)));
    presetCombo->addItem("1152 x 144", QVariant::fromValue(QSize(1152,144)));
    presetCombo->addItem("576 x 576", QVariant::fromValue(QSize(576,576)));
    presetCombo->addItem("576 x 288", QVariant::fromValue(QSize(576,288)));
    presetCombo->addItem("576 x 144", QVariant::fromValue(QSize(576,144)));
    presetCombo->addItem("288 x 288", QVariant::fromValue(QSize(288,288)));
    presetCombo->addItem("288 x 144", QVariant::fromValue(QSize(288,144)));
    presetCombo->addItem("144 x 144", QVariant::fromValue(QSize(144,144)));
    presetCombo->addItem("Custom", QVariant::fromValue(QSize(-1,-1)));

    auto customWidthSpin = new QSpinBox;
    customWidthSpin->setRange(1, 4096);
    customWidthSpin->setValue(2304);
    auto customHeightSpin = new QSpinBox;
    customHeightSpin->setRange(1, 4096);
    customHeightSpin->setValue(2304);
    presetCombo->addItem("512 x 128", QVariant::fromValue(QSize(512,128)));
    presetCombo->addItem("512 x 64", QVariant::fromValue(QSize(512,64)));
    presetCombo->addItem("256 x 64", QVariant::fromValue(QSize(256,64)));
    presetCombo->addItem("256 x 32", QVariant::fromValue(QSize(256,32)));

    auto binCombo = new QComboBox;
    binCombo->addItems({"1","2","4"});
    binCombo->setCurrentIndex(0);

    auto bitsCombo = new QComboBox;
    bitsCombo->addItems({"8","12","16"});
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

    auto readoutCombo = new QComboBox;
    readoutCombo->addItem("Fastest", DCAMPROP_READOUTSPEED__FASTEST);
    readoutCombo->addItem("Slowest", DCAMPROP_READOUTSPEED__SLOWEST);
    readoutCombo->setCurrentIndex(0);

    auto logCheck = new QCheckBox("Enable logging (session_log.txt)");
    logCheck->setChecked(true);

    // Save controls
    QString defaultSaveDir = defaultWorkspacePaths.root;
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
    grid->addWidget(new QLabel("Preset"),0,0);
    grid->addWidget(presetCombo,0,1);
    grid->addWidget(new QLabel("Custom W/H"),1,0);
    auto customLayout = new QHBoxLayout;
    customLayout->addWidget(customWidthSpin);
    customLayout->addWidget(customHeightSpin);
    grid->addLayout(customLayout,1,1);
    grid->addWidget(new QLabel("Binning"),2,0);
    grid->addWidget(binCombo,2,1);
    grid->addWidget(new QLabel("Bits"),5,0);
    grid->addWidget(bitsCombo,5,1);
    grid->addWidget(new QLabel("Exposure (ms)"),6,0);
    grid->addWidget(exposureSpin,6,1);
    grid->addWidget(new QLabel("Readout speed"),7,0);
    grid->addWidget(readoutCombo,7,1);
    grid->addWidget(new QLabel("Display every Nth frame"),8,0);
    grid->addWidget(displayEverySpin,8,1);
    grid->addWidget(logCheck,9,0,1,2);
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
    auto saveOverlayCheck = new QCheckBox("Save overlays");
    auto liveConfigureSettingsBtn = new QPushButton("Configure");
    nameWidget(liveConfigureSettingsBtn, "LiveConfigureSettingsButton");
    auto datasetCaptureModeCombo = new QComboBox;
    datasetCaptureModeCombo->addItems({"mixed", "hit-only", "waste-only"});
    auto datasetBatchTargetSpin = new QSpinBox;
    datasetBatchTargetSpin->setRange(1, 100000);
    datasetBatchTargetSpin->setValue(100);
    auto datasetStartCaptureBtn = new QPushButton("Start Dataset Capture");
    auto datasetStopCaptureBtn = new QPushButton("Stop and Review");
    datasetStopCaptureBtn->setEnabled(false);
    auto datasetCaptureStatusLabel = new QLabel("Dataset Builder capture: idle");
    datasetCaptureStatusLabel->setWordWrap(true);
    auto loadPipelineBtn = new QPushButton("Load Pipeline");

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
    pipelineLayout->addWidget(new QLabel("Model provenance"), row, 0);
    pipelineLayout->addWidget(liveModelSummaryText, row++, 1, 1, 3);
    pipelineLayout->addWidget(new QLabel("ONNX path"), row, 0);
    pipelineLayout->addWidget(onnxEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(onnxBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Metadata path"), row, 0);
    pipelineLayout->addWidget(metaEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(metaBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Output dir"), row, 0);
    pipelineLayout->addWidget(outputEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(outputBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Target class"), row, 0);
    pipelineLayout->addWidget(targetClassCombo, row++, 1, 1, 3);
    pipelineLayout->addWidget(saveCropCheck, row, 0);
    pipelineLayout->addWidget(saveOverlayCheck, row++, 1, 1, 2);
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
    labviewStatusDot->setStyleSheet("background:#666;border-radius:7px;border:1px solid #333;");
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
    labviewLayout->addWidget(labviewTestBtn, labRow++, 0, 1, 2);
    auto labviewReconnectBtn = new QPushButton("Reconnect LabVIEW");
    labviewLayout->addWidget(labviewReconnectBtn, labRow++, 0, 1, 2);

    auto labviewWidget = new QWidget;
    labviewWidget->setLayout(labviewLayout);

    auto bgFramesSpin = new QSpinBox;
    bgFramesSpin->setRange(1, 10000);
    bgFramesSpin->setValue(pipelineDetectCfg.bgFrames);
    auto bgUpdateSpin = new QSpinBox;
    bgUpdateSpin->setRange(0, 10000);
    bgUpdateSpin->setValue(pipelineDetectCfg.bgUpdateFrames);
    auto resetFramesSpin = new QSpinBox;
    resetFramesSpin->setRange(1, 1000);
    resetFramesSpin->setValue(pipelineDetectCfg.resetFrames);
    auto minAreaSpin = new QDoubleSpinBox;
    minAreaSpin->setDecimals(1);
    minAreaSpin->setRange(-1.0, 1e9);
    minAreaSpin->setValue(pipelineDetectCfg.minArea);
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
    auto minBboxSpin = new QSpinBox;
    minBboxSpin->setRange(1, 10000);
    minBboxSpin->setValue(pipelineDetectCfg.minBbox);
    auto marginSpin = new QSpinBox;
    marginSpin->setRange(0, 10000);
    marginSpin->setValue(pipelineDetectCfg.margin);
    auto diffThreshSpin = new QSpinBox;
    diffThreshSpin->setRange(0, 255);
    diffThreshSpin->setValue(pipelineDetectCfg.diffThresh);
    auto blurRadiusSpin = new QSpinBox;
    blurRadiusSpin->setRange(0, 25);
    blurRadiusSpin->setValue(pipelineDetectCfg.blurRadius);
    auto morphRadiusSpin = new QSpinBox;
    morphRadiusSpin->setRange(0, 25);
    morphRadiusSpin->setValue(pipelineDetectCfg.morphRadius);
    auto scaleSpin = new QDoubleSpinBox;
    scaleSpin->setDecimals(3);
    scaleSpin->setRange(0.05, 1.0);
    scaleSpin->setSingleStep(0.05);
    scaleSpin->setValue(pipelineDetectCfg.scale);
    auto gapFireSpin = new QSpinBox;
    gapFireSpin->setRange(0, 10000);
    gapFireSpin->setValue(pipelineDetectCfg.gapFireShift);

    auto detectLayout = new QGridLayout;
    int detRow = 0;
    detectLayout->addWidget(new QLabel("Background frames"), detRow, 0);
    detectLayout->addWidget(bgFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("BG update frames"), detRow, 0);
    detectLayout->addWidget(bgUpdateSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Reset frames"), detRow, 0);
    detectLayout->addWidget(resetFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area (-1=auto)"), detRow, 0);
    detectLayout->addWidget(minAreaSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area frac"), detRow, 0);
    detectLayout->addWidget(minAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Max area frac"), detRow, 0);
    detectLayout->addWidget(maxAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min bbox"), detRow, 0);
    detectLayout->addWidget(minBboxSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Margin"), detRow, 0);
    detectLayout->addWidget(marginSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Diff thresh"), detRow, 0);
    detectLayout->addWidget(diffThreshSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Blur radius"), detRow, 0);
    detectLayout->addWidget(blurRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Morph radius"), detRow, 0);
    detectLayout->addWidget(morphRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Scale"), detRow, 0);
    detectLayout->addWidget(scaleSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Gap fire shift"), detRow, 0);
    detectLayout->addWidget(gapFireSpin, detRow++, 1);
    auto detectWidget = new QWidget;
    detectWidget->setLayout(detectLayout);
    std::function<void()> scheduleDetectorApply = [](){};

    auto statsEventsLabel = new QLabel("Events: 0");
    auto statsClassLabel = new QLabel("Classes:\n(none)");
    auto statsHitLabel = new QLabel("Hits: 0\nWastes: 0");
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
    seqFolderEdit->setPlaceholderText("Select sequence folder...");
    auto seqBrowseBtn = new QPushButton("...");
    auto seqLoadBtn = new QPushButton("Load into memory");
    auto seqStartBtn = new QPushButton("Start Test");
    auto seqStopBtn = new QPushButton("Stop");
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
    auto seqWidget = new QWidget;
    seqWidget->setLayout(seqLayout);

    auto trainerPythonEdit = new QLineEdit("python");
    auto trainerPythonBrowseBtn = new QPushButton("Browse");
    auto trainerDatasetEdit = new QLineEdit;
    trainerDatasetEdit->setPlaceholderText("Select prepared dataset or labeled dataset root...");
    auto trainerDatasetBrowseBtn = new QPushButton("Browse");
    auto trainerOutputEdit = new QLineEdit;
    trainerOutputEdit->setPlaceholderText("Select training output directory...");
    auto trainerOutputBrowseBtn = new QPushButton("Browse");
    for (auto* edit : {trainerPythonEdit, trainerDatasetEdit, trainerOutputEdit}) {
        edit->setMinimumWidth(0);
        edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    auto trainerEnvCheckBtn = new QPushButton("Validate Environment");
    auto trainerConfigurePathBtn = new QPushButton("Configure Path");
    trainerConfigurePathBtn->setFlat(true);
    trainerConfigurePathBtn->setCursor(Qt::PointingHandCursor);
    auto trainerCancelBtn = new QPushButton("Cancel");
    trainerCancelBtn->setEnabled(false);
    auto trainerStartTrainingBtn = new QPushButton("Start Training");
    auto trainerDryRunBtn = new QPushButton("Dry Run");
    auto trainerStatusLabel = new QLabel("Trainer idle. Validate the Python environment or run a dry run before training.");
    trainerStatusLabel->setWordWrap(true);
    trainerStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto trainerResultText = new QPlainTextEdit;
    trainerResultText->setReadOnly(true);
    trainerResultText->setMinimumHeight(210);
    trainerResultText->setPlainText("Trainer process output appears here.");
    auto trainerProgressBar = new QProgressBar;
    trainerProgressBar->setRange(0, 100);
    trainerProgressBar->setValue(0);
    trainerProgressBar->setTextVisible(true);
    trainerProgressBar->setFormat("Idle");

    auto trainerPathsLayout = new QGridLayout;
    trainerPathsLayout->setColumnStretch(0, 0);
    trainerPathsLayout->setColumnStretch(1, 1);
    trainerPathsLayout->setColumnStretch(2, 0);
    trainerPathsLayout->setColumnStretch(3, 0);
    int trainerRow = 0;
    trainerPathsLayout->addWidget(new QLabel("Python"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerPythonEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerPythonBrowseBtn, trainerRow++, 3);
    trainerPathsLayout->addWidget(new QLabel("Dataset"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerDatasetEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerDatasetBrowseBtn, trainerRow++, 3);
    trainerPathsLayout->addWidget(new QLabel("Output"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerOutputEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerOutputBrowseBtn, trainerRow++, 3);
    auto trainerPathsGroup = new QGroupBox("Paths");
    trainerPathsGroup->setMinimumWidth(0);
    trainerPathsGroup->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    trainerPathsGroup->setLayout(trainerPathsLayout);

    auto trainerActionsLayout = new QHBoxLayout;
    trainerActionsLayout->addWidget(trainerEnvCheckBtn);
    trainerActionsLayout->addStretch(1);

    auto trainerEnvironmentPanel = new QFrame;
    trainerEnvironmentPanel->setProperty("panel", true);
    auto trainerEnvironmentLayout = new QVBoxLayout;
    trainerEnvironmentLayout->setContentsMargins(12, 12, 12, 12);
    trainerEnvironmentLayout->setSpacing(10);
    auto trainerEnvironmentTitle = new QLabel("ENVIRONMENT");
    trainerEnvironmentTitle->setProperty("panelTitle", true);
    auto trainerEnvironmentSubtitle = new QLabel("External Python trainer - not bundled");
    trainerEnvironmentSubtitle->setProperty("mutedText", true);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentTitle);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentSubtitle);
    const QVector<QPair<QString, QString>> trainerCheckRows = {
        {"Python executable", "Configured by TrainerPythonPathEdit"},
        {"Trainer package", "Validated by module import"},
        {"PyTorch / CUDA", "Validated by env-check"},
        {"Dataset manifest", "Checked by dry run or training command"},
        {"Training output", "Written by external trainer process"},
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
        auto* value = new QLabel(row.second);
        value->setProperty("mutedText", true);
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

    auto trainerFormPanel = new QFrame;
    trainerFormPanel->setProperty("panel", true);
    auto trainerFormLayout = new QVBoxLayout;
    trainerFormLayout->setContentsMargins(12, 12, 12, 12);
    trainerFormLayout->setSpacing(10);
    auto trainerFormTitle = new QLabel("RUN TRAINING");
    trainerFormTitle->setProperty("panelTitle", true);
    trainerFormLayout->addWidget(trainerFormTitle);
    trainerFormLayout->addWidget(trainerPathsGroup);
    auto trainerHyperGrid = new QGridLayout;
    trainerHyperGrid->setHorizontalSpacing(10);
    trainerHyperGrid->setVerticalSpacing(8);
    auto trainerArchitectureCombo = new QComboBox;
    trainerArchitectureCombo->addItem("SqueezeNet", "squeezenet1_1");
    trainerArchitectureCombo->addItem("ResNet-18", "resnet18");
    trainerArchitectureCombo->addItem("ResNet-34", "resnet34");
    auto trainerPretrainedGroup = new QButtonGroup(&window);
    trainerPretrainedGroup->setExclusive(true);
    auto trainerPretrainedImageNetBtn = new QPushButton("ImageNet");
    auto trainerPretrainedNoneBtn = new QPushButton("None");
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
    trainerBatchSpin->setValue(32);
    auto trainerLrSpin = new QDoubleSpinBox;
    trainerLrSpin->setDecimals(5);
    trainerLrSpin->setRange(0.0001, 1.0);
    trainerLrSpin->setValue(0.001);
    auto addTrainerFormCell = [&](int row, int column, const QString& labelText, QWidget* editor) {
        auto* label = new QLabel(labelText);
        label->setProperty("metricLabel", true);
        trainerHyperGrid->addWidget(label, row, column);
        trainerHyperGrid->addWidget(editor, row, column + 1);
    };
    addTrainerFormCell(0, 0, "Architecture", trainerArchitectureCombo);
    addTrainerFormCell(0, 2, "Pretrained", trainerPretrainedSegment);
    auto* trainerEpochsLabel = new QLabel("Epochs");
    trainerEpochsLabel->setProperty("metricLabel", true);
    auto* trainerBatchLabel = new QLabel("Batch size");
    trainerBatchLabel->setProperty("metricLabel", true);
    auto* trainerLrLabel = new QLabel("Learning rate");
    trainerLrLabel->setProperty("metricLabel", true);
    trainerHyperGrid->addWidget(trainerEpochsLabel, 1, 0);
    trainerHyperGrid->addWidget(trainerEpochsSpin, 1, 1);
    trainerHyperGrid->addWidget(trainerBatchLabel, 1, 2);
    trainerHyperGrid->addWidget(trainerBatchSpin, 1, 3);
    trainerHyperGrid->addWidget(trainerLrLabel, 2, 0);
    trainerHyperGrid->addWidget(trainerLrSpin, 2, 1);
    trainerFormLayout->addLayout(trainerHyperGrid);
    auto trainerLaunchRow = new QHBoxLayout;
    trainerLaunchRow->addWidget(trainerStartTrainingBtn);
    trainerLaunchRow->addWidget(trainerDryRunBtn);
    trainerLaunchRow->addWidget(trainerCancelBtn);
    trainerLaunchRow->addStretch(1);
    trainerFormLayout->addLayout(trainerLaunchRow);
    trainerFormPanel->setLayout(trainerFormLayout);

    auto trainerLogPanel = new QFrame;
    trainerLogPanel->setProperty("panel", true);
    auto trainerLogLayout = new QVBoxLayout;
    trainerLogLayout->setContentsMargins(12, 12, 12, 12);
    trainerLogLayout->setSpacing(10);
    auto trainerLogTitle = new QLabel("PROGRESS / LOG");
    trainerLogTitle->setProperty("panelTitle", true);
    trainerLogLayout->addWidget(trainerLogTitle);
    trainerLogLayout->addWidget(trainerStatusLabel);
    trainerLogLayout->addWidget(trainerProgressBar);
    trainerLogLayout->addWidget(trainerResultText, 1);
    trainerLogPanel->setLayout(trainerLogLayout);

    auto trainerRecentRunsPanel = new QFrame;
    trainerRecentRunsPanel->setProperty("panel", true);
    auto trainerRecentRunsLayout = new QVBoxLayout;
    trainerRecentRunsLayout->setContentsMargins(12, 12, 12, 12);
    trainerRecentRunsLayout->setSpacing(10);
    auto trainerRecentRunsTitle = new QLabel("RECENT RUNS");
    trainerRecentRunsTitle->setProperty("panelTitle", true);
    trainerRecentRunsLayout->addWidget(trainerRecentRunsTitle);
    auto trainerRecentRunsTable = new QTableWidget(3, 3);
    trainerRecentRunsTable->setObjectName("TrainerRecentRunsTable");
    trainerRecentRunsTable->setHorizontalHeaderLabels({"Run", "Acc", "State"});
    trainerRecentRunsTable->verticalHeader()->setVisible(false);
    trainerRecentRunsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trainerRecentRunsTable->setSelectionMode(QAbstractItemView::NoSelection);
    trainerRecentRunsTable->setMinimumHeight(140);
    const QVector<std::array<QString, 3>> trainerRuns = {
        {QString("run_2026-04-29"), QString("96.4%"), QString("complete")},
        {QString("run_2026-04-22"), QString("94.1%"), QString("complete")},
        {QString("run_2026-04-15"), QString("--"), QString("failed")},
    };
    for (int row = 0; row < trainerRuns.size(); ++row) {
        for (int col = 0; col < 3; ++col) {
            auto* item = new QTableWidgetItem(trainerRuns.at(row).at(col));
            item->setToolTip(trainerRuns.at(row).at(col));
            trainerRecentRunsTable->setItem(row, col, item);
        }
    }
    trainerRecentRunsTable->horizontalHeader()->setStretchLastSection(true);
    trainerRecentRunsLayout->addWidget(trainerRecentRunsTable);
    trainerRecentRunsPanel->setLayout(trainerRecentRunsLayout);

    auto trainerAdvancedPanel = new QGroupBox("Advanced - augmentations & schedulers");
    auto trainerAdvancedLayout = new QVBoxLayout;
    auto trainerFlipCheck = new QCheckBox("Random horizontal flip");
    trainerFlipCheck->setChecked(true);
    auto trainerRotationCheck = new QCheckBox("Random rotation +/-15 deg");
    trainerRotationCheck->setChecked(true);
    auto trainerColorJitterCheck = new QCheckBox("Color jitter");
    auto trainerRandomCropCheck = new QCheckBox("Random crop");
    trainerRandomCropCheck->setChecked(true);
    auto trainerSchedulerCombo = new QComboBox;
    trainerSchedulerCombo->addItems({"StepLR", "CosineAnnealing", "None"});
    trainerAdvancedLayout->addWidget(trainerFlipCheck);
    trainerAdvancedLayout->addWidget(trainerRotationCheck);
    trainerAdvancedLayout->addWidget(trainerColorJitterCheck);
    trainerAdvancedLayout->addWidget(trainerRandomCropCheck);
    trainerAdvancedLayout->addWidget(new QLabel("Scheduler"));
    trainerAdvancedLayout->addWidget(trainerSchedulerCombo);
    trainerAdvancedPanel->setLayout(trainerAdvancedLayout);

    auto trainerWidget = new QWidget;
    auto trainerLayout = new QHBoxLayout;
    trainerLayout->setContentsMargins(16, 16, 16, 16);
    trainerLayout->setSpacing(12);
    auto trainerLeftScroll = new QScrollArea;
    trainerLeftScroll->setObjectName("TrainerWorkspaceLeftScrollArea");
    trainerLeftScroll->setWidgetResizable(true);
    trainerLeftScroll->setFrameShape(QFrame::NoFrame);
    trainerLeftScroll->setMinimumWidth(0);
    trainerLeftScroll->setFixedWidth(520);
    trainerLeftScroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto trainerLeftContent = new QWidget;
    trainerLeftContent->setMinimumWidth(0);
    trainerLeftContent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto trainerLeftLayout = new QVBoxLayout;
    trainerLeftLayout->setContentsMargins(0, 0, 0, 0);
    trainerLeftLayout->setSpacing(12);
    trainerLeftLayout->addWidget(trainerEnvironmentPanel);
    trainerLeftLayout->addWidget(trainerFormPanel);
    trainerLeftLayout->addWidget(trainerLogPanel, 1);
    trainerLeftContent->setLayout(trainerLeftLayout);
    trainerLeftScroll->setWidget(trainerLeftContent);
    auto trainerRightScroll = new QScrollArea;
    trainerRightScroll->setObjectName("TrainerWorkspaceRightScrollArea");
    trainerRightScroll->setWidgetResizable(true);
    trainerRightScroll->setFrameShape(QFrame::NoFrame);
    trainerRightScroll->setFixedWidth(300);
    auto trainerRightContent = new QWidget;
    trainerRightContent->setObjectName("TrainerWorkspaceRightStack");
    auto trainerRightLayout = new QVBoxLayout;
    trainerRightLayout->setContentsMargins(0, 0, 0, 0);
    trainerRightLayout->setSpacing(12);
    trainerRightLayout->addWidget(trainerRecentRunsPanel);
    trainerRightLayout->addWidget(trainerAdvancedPanel);
    trainerRightLayout->addStretch(1);
    trainerRightContent->setLayout(trainerRightLayout);
    trainerRightScroll->setWidget(trainerRightContent);
    trainerLayout->addWidget(trainerLeftScroll, 0);
    trainerLayout->addWidget(trainerRightScroll, 0);
    trainerLayout->addStretch(1);
    trainerWidget->setLayout(trainerLayout);

    auto trainerDockProxy = new QWidget;
    auto trainerDockProxyLayout = new QVBoxLayout;
    trainerDockProxyLayout->setContentsMargins(12, 12, 12, 12);
    auto trainerDockProxyLabel = new QLabel("Trainer readiness now opens in the main Trainer workspace. Training launch remains disabled.");
    trainerDockProxyLabel->setWordWrap(true);
    auto trainerDockProxyButton = new QPushButton("Open Trainer Workspace");
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
    nameWidget(readoutCombo, "CameraReadoutSpeedComboBox");
    nameWidget(displayEverySpin, "CameraDisplayEverySpinBox");
    nameWidget(lutMinSpin, "CameraLutMinSpinBox");
    nameWidget(lutMaxSpin, "CameraLutMaxSpinBox");
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
    nameWidget(saveOverlayCheck, "PipelineSaveOverlaysCheckBox");
    nameWidget(datasetCaptureModeCombo, "DatasetCaptureModeComboBox");
    nameWidget(datasetBatchTargetSpin, "DatasetCaptureBatchTargetSpinBox");
    nameWidget(datasetStartCaptureBtn, "DatasetCaptureStartButton");
    nameWidget(datasetStopCaptureBtn, "DatasetCaptureStopReviewButton");
    nameWidget(datasetCaptureStatusLabel, "DatasetCaptureStatusLabel");
    nameWidget(loadPipelineBtn, "PipelineLoadButton");
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
    nameWidget(trainerEnvironmentPanel, "TrainerEnvironmentPanel");
    nameWidget(trainerFormPanel, "TrainerRunTrainingPanel");
    nameWidget(trainerLogPanel, "TrainerProgressLogPanel");
    nameWidget(trainerArchitectureCombo, "TrainerArchitectureCombo");
    nameWidget(trainerPretrainedSegment, "TrainerPretrainedSegmentedControl");
    nameWidget(trainerPretrainedImageNetBtn, "TrainerPretrainedImageNetButton");
    nameWidget(trainerPretrainedNoneBtn, "TrainerPretrainedNoneButton");
    nameWidget(trainerEpochsSpin, "TrainerEpochsSpinBox");
    nameWidget(trainerBatchSpin, "TrainerBatchSizeSpinBox");
    nameWidget(trainerLrSpin, "TrainerLearningRateSpinBox");
    nameWidget(trainerRecentRunsPanel, "TrainerRecentRunsPanel");
    nameWidget(trainerAdvancedPanel, "TrainerAdvancedAugmentationSchedulerGroup");
    nameWidget(trainerFlipCheck, "TrainerRandomHorizontalFlipCheckBox");
    nameWidget(trainerRotationCheck, "TrainerRandomRotationCheckBox");
    nameWidget(trainerColorJitterCheck, "TrainerColorJitterCheckBox");
    nameWidget(trainerRandomCropCheck, "TrainerRandomCropCheckBox");
    nameWidget(trainerSchedulerCombo, "TrainerSchedulerCombo");
    if (options.noDaq) {
        daqDeviceCombo->setEnabled(false);
        daqChannelEdit->clear();
        labviewStatusText->setText("Disabled");
        labviewOutputLabel->setText("Output: disabled");
    }

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
    auto blockersLabel = new QLabel("Camera, model, and DAQ readiness appear here when startup or run actions are blocked.");
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
    auto liveOverlayTool = addViewerTool("Overlay: show or hide the detection overlay.", "overlay", true);
    auto liveCrosshairTool = addViewerTool("Crosshair: show or hide the center reticle.", "crosshair", false);
    liveCrosshairTool->setCheckable(true);
    nameWidget(liveFitTool, "LiveViewerFitButton");
    nameWidget(liveOverlayTool, "LiveViewerOverlayToggle");
    nameWidget(liveCrosshairTool, "LiveViewerCrosshairToggle");
    liveFitTool->setAccessibleName("Live Fit to View");
    liveOverlayTool->setAccessibleName("Live Overlay");
    liveCrosshairTool->setAccessibleName("Live Crosshair");
    QObject::connect(liveFitTool, &QToolButton::clicked, fitAction, &QAction::trigger);
    QObject::connect(liveOverlayTool, &QToolButton::toggled, overlayAction, &QAction::setChecked);
    QObject::connect(overlayAction, &QAction::toggled, liveOverlayTool, &QToolButton::setChecked);
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

    auto liveDetectionOverlay = new QWidget;
    nameWidget(liveDetectionOverlay, "LiveViewerDetectionOverlay");
    liveDetectionOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    liveDetectionOverlay->setVisible(liveOverlayTool->isChecked());
    auto liveDetectionOverlayLayout = new QGridLayout;
    liveDetectionOverlayLayout->setContentsMargins(0, 0, 0, 0);
    liveDetectionOverlayLayout->setRowStretch(0, 3);
    liveDetectionOverlayLayout->setRowStretch(2, 4);
    liveDetectionOverlayLayout->setColumnStretch(0, 3);
    liveDetectionOverlayLayout->setColumnStretch(2, 4);
    auto liveDetectionBox = new QFrame;
    nameWidget(liveDetectionBox, "LiveViewerDetectionBox");
    liveDetectionBox->setFixedSize(92, 92);
    liveDetectionBox->setStyleSheet("QFrame#LiveViewerDetectionBox{border:2px solid rgba(20,184,166,0.95);background:rgba(20,184,166,0.07);border-radius:2px;}");
    auto liveDetectionBoxLayout = new QVBoxLayout;
    liveDetectionBoxLayout->setContentsMargins(4, 4, 4, 4);
    auto liveDetectionLabel = new QLabel("SINGLE 0.94");
    nameWidget(liveDetectionLabel, "LiveViewerDetectionLabel");
    liveDetectionLabel->setAlignment(Qt::AlignCenter);
    liveDetectionLabel->setStyleSheet("background:rgba(2,6,23,0.82);color:#CCFBF1;font-size:10px;font-weight:700;padding:2px 4px;border-radius:2px;");
    liveDetectionBoxLayout->addWidget(liveDetectionLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
    liveDetectionBoxLayout->addStretch(1);
    liveDetectionBox->setLayout(liveDetectionBoxLayout);
    liveDetectionOverlayLayout->addWidget(liveDetectionBox, 1, 1, Qt::AlignCenter);
    liveDetectionOverlay->setLayout(liveDetectionOverlayLayout);
    liveViewerStackLayout->addWidget(liveDetectionOverlay);
    QObject::connect(overlayAction, &QAction::toggled, liveDetectionOverlay, &QWidget::setVisible);

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
    auto liveDetectorTitle = new QLabel("Detector tuning");
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
        liveDetectorGrid->addWidget(labelWidget, liveDetectorRow, 0);
        liveDetectorGrid->addWidget(spin, liveDetectorRow++, 1);
    };
    auto makeLinkedIntSpin = [&](QSpinBox* source, int min, int max) {
        auto* spin = new QSpinBox;
        spin->setRange(min, max);
        spin->setValue(source->value());
        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [=, &scheduleDetectorApply](int value) {
            if (source->value() != value) source->setValue(value);
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
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), [=, &scheduleDetectorApply](double value) {
            if (!qFuzzyCompare(source->value() + 1.0, value + 1.0)) source->setValue(value);
            scheduleDetectorApply();
        });
        QObject::connect(source, qOverload<double>(&QDoubleSpinBox::valueChanged), spin, &QDoubleSpinBox::setValue);
        return spin;
    };
    auto makeUnavailableSpin = [](int value, const QString& tooltip) {
        auto* spin = new QSpinBox;
        spin->setRange(0, 1000000);
        spin->setValue(value);
        spin->setEnabled(false);
        spin->setToolTip(tooltip);
        return spin;
    };
    addLiveDetectorSpin("BG frames", makeLinkedIntSpin(bgFramesSpin, 1, 10000), "LiveDetectorBgFramesSpinBox");
    addLiveDetectorSpin("Diff threshold", makeLinkedIntSpin(diffThreshSpin, 0, 255), "LiveDetectorDiffThresholdSpinBox");
    addLiveDetectorSpin("Min area", makeLinkedDoubleSpin(minAreaSpin, -1.0, 1e9, 1, 1.0), "LiveDetectorMinAreaSpinBox");
    addLiveDetectorSpin("Max area", makeLinkedDoubleSpin(maxAreaFracSpin, 0.0, 1.0, 4, 0.001), "LiveDetectorMaxAreaSpinBox");
    addLiveDetectorSpin("Blur radius", makeLinkedIntSpin(blurRadiusSpin, 0, 25), "LiveDetectorBlurRadiusSpinBox");
    addLiveDetectorSpin("Erode iterations", makeUnavailableSpin(0, "Current runtime exposes one morph radius, not separate erode iterations."), "LiveDetectorErodeIterationsSpinBox");
    addLiveDetectorSpin("Dilate iterations", makeUnavailableSpin(morphRadiusSpin->value(), "Current runtime exposes one morph radius, not separate dilate iterations."), "LiveDetectorDilateIterationsSpinBox");
    addLiveDetectorSpin("Min contour points", makeLinkedIntSpin(minBboxSpin, 1, 10000), "LiveDetectorMinContourPointsSpinBox");
    addLiveDetectorSpin("Aspect ratio min", makeUnavailableSpin(0, "Aspect-ratio filtering is not exposed by the current detector config."), "LiveDetectorAspectRatioMinSpinBox");
    addLiveDetectorSpin("Aspect ratio max", makeUnavailableSpin(0, "Aspect-ratio filtering is not exposed by the current detector config."), "LiveDetectorAspectRatioMaxSpinBox");
    addLiveDetectorSpin("Velocity min", makeUnavailableSpin(0, "Velocity filtering is not exposed by the current detector config."), "LiveDetectorVelocityMinSpinBox");
    addLiveDetectorSpin("Velocity max", makeUnavailableSpin(0, "Velocity filtering is not exposed by the current detector config."), "LiveDetectorVelocityMaxSpinBox");
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
    pipelineStopBtn->setText("Stop Sorting");
    auto triggerSafeBtn = new QPushButton(options.noDaq ? "Trigger Safe" : "Trigger Ready");
    nameWidget(triggerSafeBtn, "LiveTriggerSafeButton");
    triggerSafeBtn->setCheckable(true);
    triggerSafeBtn->setChecked(appState.triggerArmed);
    auto liveForceTriggerBtn = new QPushButton("Force Trigger");
    nameWidget(liveForceTriggerBtn, "LiveForceTriggerButton");
    liveForceTriggerBtn->setEnabled(false);
    auto liveSnapshotBtn = new QPushButton("Snapshot");
    nameWidget(liveSnapshotBtn, "LiveSnapshotButton");
    auto liveOpenRunBtn = new QPushButton("Open Run");
    nameWidget(liveOpenRunBtn, "LiveOpenRunButton");
    liveOpenRunBtn->setEnabled(false);
    openRunFolderAction->setEnabled(false);
    auto liveDetectorTuningBtn = new QPushButton("Tuning");
    nameWidget(liveDetectorTuningBtn, "LiveDetectorTuningButton");
    liveDetectorTuningBtn->setToolTip("Open detector tuning controls.");
    for (auto* button : {pipelineStartBtn, pipelineStopBtn, triggerSafeBtn, liveForceTriggerBtn,
                         liveSnapshotBtn, liveOpenRunBtn, liveDetectorTuningBtn}) {
        button->setMaximumHeight(36);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    pipelineStartBtn->setMaximumWidth(150);
    pipelineStopBtn->setMaximumWidth(138);
    triggerSafeBtn->setMaximumWidth(128);
    liveForceTriggerBtn->setMaximumWidth(124);
    liveSnapshotBtn->setMaximumWidth(110);
    liveOpenRunBtn->setMaximumWidth(108);
    liveDetectorTuningBtn->setMaximumWidth(96);
    liveRunLayout->addWidget(pipelineStartBtn);
    liveRunLayout->addWidget(pipelineStopBtn);
    liveRunLayout->addSpacing(4);
    liveRunLayout->addWidget(triggerSafeBtn);
    liveRunLayout->addWidget(liveForceTriggerBtn);
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

    std::function<void()> updateForceTriggerState = [](){};
    updateForceTriggerState = [&]() {
        const bool waveformValid = !daqChannelEdit->text().trimmed().isEmpty()
            && amplitudeSpin->value() > 0.0
            && freqSpin->value() > 0.0
            && durationSpin->value() > 0.0;
        appState.daqWaveformValid = waveformValid;
        const bool canFire = appState.triggerArmed
            && appState.daqAvailable
            && !appState.daqDisabled
            && !appState.daqFault
            && waveformValid;
        QStringList blockers;
        if (!appState.triggerArmed) blockers << "trigger is safe";
        if (!appState.daqAvailable || appState.daqDisabled) blockers << "DAQ is not available";
        if (appState.daqFault) blockers << (appState.daqFaultText.isEmpty() ? QStringLiteral("DAQ fault is active") : appState.daqFaultText);
        if (!waveformValid) blockers << "waveform settings are incomplete";
        const QString tip = canFire
            ? QStringLiteral("Fire the full configured waveform from Settings.")
            : QStringLiteral("Force Trigger disabled: %1.").arg(blockers.join("; "));
        liveForceTriggerBtn->setEnabled(canFire);
        liveForceTriggerBtn->setToolTip(tip);
        manualTriggerAction->setEnabled(canFire);
        manualTriggerAction->setStatusTip(tip);
        {
            QSignalBlocker blocker(triggerSafeBtn);
            triggerSafeBtn->setChecked(appState.triggerArmed);
        }
        triggerSafeBtn->setText(appState.triggerArmed ? QStringLiteral("Trigger Armed") : QStringLiteral("Trigger Safe"));
        triggerSafeBtn->setToolTip(appState.triggerArmed
            ? QStringLiteral("Click to safe the trigger.")
            : QStringLiteral("Click to arm the trigger. Force Trigger still requires valid DAQ and waveform settings."));
    };
    updateForceTriggerState();

    auto eventsMetricLabel = new QLabel("0");
    auto hitsMetricLabel = new QLabel("0");
    auto wasteMetricLabel = new QLabel("0");
    auto trigMetricLabel = new QLabel("--");
    nameWidget(eventsMetricLabel, "LiveRunEventsMetricLabel");
    nameWidget(hitsMetricLabel, "LiveRunHitsMetricLabel");
    nameWidget(wasteMetricLabel, "LiveRunWasteMetricLabel");
    nameWidget(trigMetricLabel, "LiveRunTriggerRateMetricLabel");

    auto runPanel = makePanel("Run");
    runPanel->setObjectName("LiveRunPanel");
    auto runBody = makePanelBody(runPanel);
    auto metricGrid = new QGridLayout;
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setSpacing(1);
    metricGrid->addWidget(makeMetric("Events", eventsMetricLabel), 0, 0);
    metricGrid->addWidget(makeMetric("Hits", hitsMetricLabel), 0, 1);
    metricGrid->addWidget(makeMetric("Waste", wasteMetricLabel), 1, 0);
    metricGrid->addWidget(makeMetric("Trig/s", trigMetricLabel), 1, 1);
    runBody->addLayout(metricGrid);
    auto lastDecisionCard = new QFrame;
    nameWidget(lastDecisionCard, "LiveLastDecisionCard");
    auto lastDecisionLayout = new QHBoxLayout;
    lastDecisionLayout->setContentsMargins(10, 8, 10, 8);
    lastDecisionLayout->setSpacing(10);
    auto lastDecisionThumb = new QLabel("64x64");
    nameWidget(lastDecisionThumb, "LiveLastDecisionThumbnail");
    lastDecisionThumb->setAlignment(Qt::AlignCenter);
    lastDecisionThumb->setFixedSize(54, 42);
    lastDecisionThumb->setStyleSheet("background:#0A0A0A;color:#94A3B8;border-radius:3px;font-size:10px;");
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
    pipelineGrid->addWidget(new QLabel("Target"), 0, 0);
    pipelineGrid->addWidget(targetClassCombo, 0, 1);
    pipelineGrid->addWidget(new QLabel("Model"), 1, 0);
    pipelineGrid->addWidget(liveModelCombo, 1, 1);
    pipelineGrid->addWidget(openLiveModelManagerBtn, 1, 2);
    pipelineGrid->addWidget(new QLabel("Output"), 2, 0);
    pipelineGrid->addWidget(outputEdit, 2, 1, 1, 2);
    pipelineGrid->addWidget(saveCropCheck, 3, 0);
    pipelineGrid->addWidget(saveOverlayCheck, 3, 1);
    pipelineGrid->addWidget(loadPipelineBtn, 3, 2);
    pipelineGrid->addWidget(liveConfigureSettingsBtn, 4, 2);
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
    rightScroll->setMinimumWidth(330);
    rightScroll->setMaximumWidth(360);
    rightScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto rightStack = new QWidget;
    nameWidget(rightStack, "LiveRightMetricsStack");
    rightStack->setMinimumWidth(310);
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
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setCollapsible(1, false);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setSizes({760, 340});

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

    auto leftSequenceTab = new QWidget;
    nameWidget(leftSequenceTab, "OperationalSequenceTab");
    auto leftSequenceLayout = new QVBoxLayout;
    leftSequenceLayout->setContentsMargins(8, 8, 8, 8);
    leftSequenceLayout->addWidget(seqWidget);
    leftSequenceLayout->addStretch(1);
    leftSequenceTab->setLayout(leftSequenceLayout);

    auto leftAnalysisTab = new QWidget;
    nameWidget(leftAnalysisTab, "OperationalAnalysisTab");
    auto leftAnalysisLayout = new QVBoxLayout;
    leftAnalysisLayout->setContentsMargins(8, 8, 8, 8);
    auto leftOverlayCheck = new QCheckBox("View Overlay");
    nameWidget(leftOverlayCheck, "AnalysisOverlayCheckBox");
    leftOverlayCheck->setChecked(true);
    leftAnalysisLayout->addWidget(pipelineWidget);
    leftAnalysisLayout->addWidget(leftOverlayCheck);
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

    auto modelManagerWidget = new QWidget;
    nameWidget(modelManagerWidget, "modelManagerScreen");
    auto modelManagerLayout = new QVBoxLayout;
    modelManagerLayout->setContentsMargins(8, 8, 8, 8);
    modelManagerLayout->setSpacing(8);

    auto modelRegistryTable = new QTableWidget(registryEntries.size(), 6);
    nameWidget(modelRegistryTable, "modelRegistryTable");
    modelRegistryTable->setHorizontalHeaderLabels({"Model", "State", "Metadata", "Validation", "Promotion", "Path"});
    modelRegistryTable->verticalHeader()->setVisible(false);
    modelRegistryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modelRegistryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    modelRegistryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    modelRegistryTable->setAlternatingRowColors(true);
    modelRegistryTable->setMinimumHeight(118);
    modelRegistryTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto setRegistryCell = [&](int row, int column, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        modelRegistryTable->setItem(row, column, item);
    };
    for (int i = 0; i < registryEntries.size(); ++i) {
        QJsonObject entry = registryEntries.at(i).toObject();
        setRegistryCell(i, 0, registryString(entry, "registry_entry_id"));
        setRegistryCell(i, 1, registryString(entry, "state") + " / " + registryString(entry, "live_use_mode"));
        setRegistryCell(i, 2, registryString(entry, "metadata_status"));
        setRegistryCell(i, 3, registryString(entry, "validation_status"));
        setRegistryCell(i, 4, registryString(entry, "promotion_status"));
        setRegistryCell(i, 5, registryString(entry, "model_path"));
        if (!entry.value("selectable_for_normal_live_sorting").toBool(false)) {
            for (int column = 0; column < modelRegistryTable->columnCount(); ++column) {
                if (auto* item = modelRegistryTable->item(i, column)) {
                    item->setForeground(QBrush(QColor(Qt::gray)));
                }
            }
        }
    }
    modelRegistryTable->resizeColumnsToContents();
    modelRegistryTable->horizontalHeader()->setStretchLastSection(true);
    modelManagerLayout->addWidget(modelRegistryTable);

    auto modelManagerSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(modelManagerSplitter, "modelManagerSplitter");

    auto modelDetailsPanel = new QGroupBox("Selected Model Details");
    nameWidget(modelDetailsPanel, "modelDetailsPanel");
    auto modelDetailsLayout = new QVBoxLayout;
    auto modelDetailsText = new QTextEdit;
    nameWidget(modelDetailsText, "modelDetailsText");
    modelDetailsText->setReadOnly(true);
    modelDetailsText->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    modelDetailsText->setPlainText(registryEntrySummary(registryEntries.first().toObject(), registryFilePath, registryLoadWarning));
    modelDetailsLayout->addWidget(modelDetailsText);
    modelDetailsPanel->setLayout(modelDetailsLayout);
    QObject::connect(modelRegistryTable, &QTableWidget::currentCellChanged,
        [=](int currentRow, int, int, int) {
            if (currentRow < 0 || currentRow >= registryEntries.size()) return;
            modelDetailsText->setPlainText(registryEntrySummary(registryEntries.at(currentRow).toObject(), registryFilePath, registryLoadWarning));
        });
    modelRegistryTable->selectRow(0);

    auto modelGatePanel = new QWidget;
    nameWidget(modelGatePanel, "modelGatePanel");
    auto modelGateLayout = new QVBoxLayout;
    modelGateLayout->setContentsMargins(0, 0, 0, 0);

    auto modelGateChecklist = new QTableWidget(8, 3);
    nameWidget(modelGateChecklist, "modelGateChecklist");
    modelGateChecklist->setHorizontalHeaderLabels({"Gate", "Status", "Evidence"});
    modelGateChecklist->verticalHeader()->setVisible(false);
    modelGateChecklist->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modelGateChecklist->setSelectionMode(QAbstractItemView::NoSelection);
    modelGateChecklist->setAlternatingRowColors(true);
    auto setGateCell = [&](int row, int column, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        modelGateChecklist->setItem(row, column, item);
    };
    setGateCell(0, 0, "Default runtime install");
    setGateCell(0, 1, "Pass - hashes match promoted candidate");
    setGateCell(0, 2, "docs/worker-reports/2026-04-30-actual-model-promotion-execution.md");
    setGateCell(1, 0, "Metadata");
    setGateCell(1, 1, "Pass for current helper");
    setGateCell(1, 2, "app/runtime/models/metadata.json; SHA-256 fa5321dfad900baec23fa6c239a29279e0e8c03fa2e78f0bd679dfb973888d2f");
    setGateCell(2, 0, "Sidecar");
    setGateCell(2, 1, "Pass");
    setGateCell(2, 2, "app/runtime/models/model.onnx.data; SHA-256 2e3727b593fee4f155caf67eb18a7b3a2b73ebb3655a1a6ab33b74c25a02ebd4");
    setGateCell(3, 0, "Image validation");
    setGateCell(3, 1, "Internal pass");
    setGateCell(3, 2, "docs/worker-reports/2026-04-30-post-promotion-validation-execution.md");
    setGateCell(4, 0, "Sequence validation");
    setGateCell(4, 1, "Provisional");
    setGateCell(4, 2, "Policy keeps sequence validation provisional until separately promoted");
    setGateCell(5, 0, "Hardware trigger");
    setGateCell(5, 1, "Pass - Wave 19/20");
    setGateCell(5, 2, "docs/worker-reports/2026-04-30-ni-class-driven-trigger-validation.md; docs/worker-reports/2026-04-30-ni-scope-observation-confirmation.md");
    setGateCell(6, 0, "Internal promotion");
    setGateCell(6, 1, "Completed - Wave 23");
    setGateCell(6, 2, "Installed default artifacts match authorized promoted candidate hashes");
    setGateCell(7, 0, "Public release policy");
    setGateCell(7, 1, "Separate blocker");
    setGateCell(7, 2, "Public release/data approvals remain separate from internal promoted runtime state");
    modelGateChecklist->resizeColumnsToContents();
    modelGateChecklist->horizontalHeader()->setStretchLastSection(true);
    modelGateLayout->addWidget(modelGateChecklist);

    auto promotionBlockersList = new QTableWidget(4, 4);
    nameWidget(promotionBlockersList, "promotionBlockersList");
    promotionBlockersList->setHorizontalHeaderLabels({"Blocker", "Owner/type", "Evidence", "Required next action"});
    promotionBlockersList->verticalHeader()->setVisible(false);
    promotionBlockersList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    promotionBlockersList->setSelectionMode(QAbstractItemView::NoSelection);
    promotionBlockersList->setAlternatingRowColors(true);
    auto setBlockerCell = [&](int row, int column, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        promotionBlockersList->setItem(row, column, item);
    };
    setBlockerCell(0, 0, "Sequence validation remains provisional");
    setBlockerCell(0, 1, "Human domain review");
    setBlockerCell(0, 2, "docs/validation/internal-fixtures/first-sequence-candidate-20260226_163405_BEST_SO_FAR/sequence_ground_truth_user_review_support_wave12.md");
    setBlockerCell(0, 3, "Keep sequence claims provisional until a separate policy decision promotes them");
    setBlockerCell(1, 0, "Runtime trigger-target policy");
    setBlockerCell(1, 1, "Runtime/product decision");
    setBlockerCell(1, 2, "docs/contracts/runtime-trigger-target-policy.md");
    setBlockerCell(1, 3, "Continue using canonical class id 1 for Hits");
    setBlockerCell(2, 0, "Public model/data release policy");
    setBlockerCell(2, 1, "Release/data policy");
    setBlockerCell(2, 2, "docs/worker-reports/2026-04-30-public-release-gap-audit.md");
    setBlockerCell(2, 3, "Resolve redistribution and public-claim approvals before public release");
    setBlockerCell(3, 0, "Public claim wording");
    setBlockerCell(3, 1, "Release communications");
    setBlockerCell(3, 2, "docs/qa/candidate-promotion-readiness-checklist.md");
    setBlockerCell(3, 3, "Keep public release claims separate from internal promoted runtime state");
    promotionBlockersList->resizeColumnsToContents();
    promotionBlockersList->horizontalHeader()->setStretchLastSection(true);
    modelGateLayout->addWidget(promotionBlockersList);

    auto modelActionLayout = new QHBoxLayout;
    auto verifyModelArtifactsButton = new QPushButton("Verify Artifacts");
    auto openModelMetadataButton = new QPushButton("Open Metadata");
    auto compareModelsButton = new QPushButton("Compare Models");
    auto promoteModelButton = new QPushButton("Promote Model");
    nameWidget(verifyModelArtifactsButton, "verifyModelArtifactsButton");
    nameWidget(openModelMetadataButton, "openModelMetadataButton");
    nameWidget(compareModelsButton, "compareModelsButton");
    nameWidget(promoteModelButton, "promoteModelButton");
    verifyModelArtifactsButton->setEnabled(false);
    openModelMetadataButton->setEnabled(false);
    compareModelsButton->setEnabled(false);
    promoteModelButton->setEnabled(false);
    promoteModelButton->setToolTip("Disabled: the default runtime already matches the promoted candidate; this read-only view does not support destructive re-promotion. Sequence and public release items remain separately tracked.");
    modelActionLayout->addWidget(verifyModelArtifactsButton);
    modelActionLayout->addWidget(openModelMetadataButton);
    modelActionLayout->addWidget(compareModelsButton);
    modelActionLayout->addWidget(promoteModelButton);
    modelGateLayout->addLayout(modelActionLayout);
    modelGatePanel->setLayout(modelGateLayout);

    modelManagerSplitter->addWidget(modelDetailsPanel);
    modelManagerSplitter->addWidget(modelGatePanel);
    modelManagerSplitter->setStretchFactor(0, 1);
    modelManagerSplitter->setStretchFactor(1, 2);
    modelManagerLayout->addWidget(modelManagerSplitter, 1);
    modelManagerWidget->setLayout(modelManagerLayout);

    auto operationalTabs = new QTabWidget;
    operationalTabs->setObjectName("OperationalTabs");
    operationalTabs->setAccessibleName("OperationalTabs");
    operationalTabs->addTab(leftCaptureTab, "Capture");
    operationalTabs->addTab(leftDevicesTab, "Devices");
    operationalTabs->addTab(leftSequenceTab, "Sequence");
    operationalTabs->addTab(leftAnalysisTab, "Analysis");
    operationalTabs->addTab(trainerDockProxy, "Trainer");
    operationalTabs->addTab(modelManagerWidget, "Model Manager");

    auto operationDock = new QDockWidget("Capture", &window);
    operationDock->setObjectName("OperationalDock");
    operationDock->setAccessibleName("OperationalDock");
    operationDock->setWidget(operationalTabs);
    operationDock->setMinimumWidth(260);
    window.addDockWidget(Qt::LeftDockWidgetArea, operationDock);
    operationDock->hide();

    desktop_app::workspace::CameraWorkspaceControls cameraWorkspaceControls;
    cameraWorkspaceControls.presetCombo = presetCombo;
    cameraWorkspaceControls.bitsCombo = bitsCombo;
    cameraWorkspaceControls.customWidthSpin = customWidthSpin;
    cameraWorkspaceControls.customHeightSpin = customHeightSpin;
    cameraWorkspaceControls.exposureSpin = exposureSpin;
    cameraWorkspaceControls.readoutCombo = readoutCombo;
    cameraWorkspaceControls.binCombo = binCombo;
    cameraWorkspaceControls.lutMinSpin = lutMinSpin;
    cameraWorkspaceControls.lutMaxSpin = lutMaxSpin;
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
    auto cameraControlsStack = desktop_app::workspace::buildCameraControlsStack(cameraWorkspaceControls);
    rightStackLayout->insertWidget(2, cameraControlsStack);

    desktop_app::workspace::ModelWorkspaceControls modelWorkspaceControls;
    modelWorkspaceControls.registryEntries = registryEntries;
    modelWorkspaceControls.registryFilePath = registryFilePath;
    modelWorkspaceControls.registryLoadWarning = registryLoadWarning;
    modelWorkspaceControls.targetClassCombo = targetClassCombo;
    modelWorkspaceControls.modelManagerAction = modelManagerAction;
    modelWorkspaceControls.imageValidationAction = imageValidationAction;
    modelWorkspaceControls.appState = &appState;
    auto modelWorkspacePage = desktop_app::workspace::buildModelWorkspace(modelWorkspaceControls);

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

    desktop_app::workspace::ValidatorWorkspaceControls validatorWorkspaceControls;
    validatorWorkspaceControls.modelPath = onnxEdit->text().trimmed();
    validatorWorkspaceControls.metadataPath = metaEdit->text().trimmed();
    validatorWorkspaceControls.imageValidationAction = imageValidationAction;
    auto validatorWorkspacePage = desktop_app::workspace::buildValidatorWorkspace(validatorWorkspaceControls);

    desktop_app::workspace::ReportsWorkspaceControls reportsWorkspaceControls;
    reportsWorkspaceControls.logPath = gLogPath;
    reportsWorkspaceControls.hardwareFreeMode = hardwareFreeMode;
    reportsWorkspaceControls.viewerOnly = viewerOnly;
    reportsWorkspaceControls.noDaq = options.noDaq;
    reportsWorkspaceControls.showLogsAction = showLogsAction;
    reportsWorkspaceControls.showDiagnosticsAction = showDiagnosticsAction;
    reportsWorkspaceControls.openRunFolderAction = openRunFolderAction;
    auto reportsWorkspacePage = desktop_app::workspace::buildReportsWorkspace(reportsWorkspaceControls);

    desktop_app::workspace::SettingsWorkspaceControls settingsWorkspaceControls;
    settingsWorkspaceControls.outputRoot = defaultWorkspacePaths.runs;
    settingsWorkspaceControls.modelPath = defaultWorkspacePaths.models;
    settingsWorkspaceControls.metadataPath = metaEdit->text().trimmed();
    settingsWorkspaceControls.datasetsRoot = defaultWorkspacePaths.datasets;
    settingsWorkspaceControls.logPath = gLogPath;
    settingsWorkspaceControls.hardwareFreeMode = hardwareFreeMode;
    settingsWorkspaceControls.cameraSavePathEdit = savePathEdit;
    settingsWorkspaceControls.cameraPresetCombo = presetCombo;
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
    settingsWorkspaceControls.operationalTabs = operationalTabs;
    settingsWorkspaceControls.analysisTab = leftAnalysisTab;
    settingsWorkspaceControls.devicesTab = leftDevicesTab;
    settingsWorkspaceControls.appState = &appState;
    auto settingsWorkspacePage = desktop_app::workspace::buildSettingsWorkspace(settingsWorkspaceControls);

    auto workspaceStack = new QStackedWidget;
    nameWidget(workspaceStack, "OpenDssWorkspaceStack");
    workspaceStack->addWidget(liveWorkspacePage);
    workspaceStack->addWidget(modelWorkspacePage);
    workspaceStack->addWidget(datasetWorkspacePage);
    workspaceStack->addWidget(trainerWorkspacePage);
    workspaceStack->addWidget(validatorWorkspacePage);
    workspaceStack->addWidget(reportsWorkspacePage);
    workspaceStack->addWidget(settingsWorkspacePage);
    workspaceStack->setCurrentWidget(liveWorkspacePage);
    auto liveModelMenu = new QMenu(openLiveModelManagerBtn);
    auto* liveSelectModelAction = liveModelMenu->addAction("Select model");
    QObject::connect(liveSelectModelAction, &QAction::triggered, modelManagerAction, &QAction::trigger);
    liveModelMenu->addAction("Open Model workspace", [=]() {
        workspaceStack->setCurrentWidget(modelWorkspacePage);
    });
    openLiveModelManagerBtn->setMenu(liveModelMenu);
    QObject::connect(liveConfigureSettingsBtn, &QPushButton::clicked, [=]() {
        workspaceStack->setCurrentWidget(settingsWorkspacePage);
    });

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
    auto headerCameraChip = new QLabel(options.mockCamera ? "Camera mock" : "Camera startup");
    auto headerModelChip = new QLabel("Model not loaded");
    auto headerDaqChip = new QLabel((options.noDaq || !kDaqBuildEnabled) ? "DAQ unavailable" : "DAQ unchecked");
    auto headerTriggerChip = new QLabel(options.noDaq ? "Trigger safe" : "Trigger unchecked");
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
    headerDaqChip->setProperty("chipTone", options.noDaq ? "disabled" : "warn");
    headerTriggerChip->setProperty("chipTone", options.noDaq ? "running" : "warn");
    auto shellMenuButton = new QToolButton;
    nameWidget(shellMenuButton, "OpenDssHeaderMenuButton");
    shellMenuButton->setProperty("headerIcon", true);
    shellMenuButton->setIcon(makeBrandIcon("menu", QColor("#FFFFFF"), QColor("#7DD3FC")));
    shellMenuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    shellMenuButton->setToolTip("Menu");
    shellMenuButton->setAccessibleName("OpenDssHeaderMenuButton");
    shellMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto shellMenu = new QMenu(shellMenuButton);
    for (auto* action : window.menuBar()->actions()) {
        shellMenu->addAction(action);
    }
    shellMenuButton->setMenu(shellMenu);
    auto diagnosticsHeaderButton = new QToolButton;
    nameWidget(diagnosticsHeaderButton, "OpenDssHeaderDiagnosticsButton");
    diagnosticsHeaderButton->setProperty("headerIcon", true);
    diagnosticsHeaderButton->setIcon(makeBrandIcon("info", QColor("#FFFFFF"), QColor("#7DD3FC")));
    diagnosticsHeaderButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    diagnosticsHeaderButton->setToolTip("Diagnostics");
    diagnosticsHeaderButton->setAccessibleName("OpenDssHeaderDiagnosticsButton");
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
        currentThemeMode = themeToggleButton->isChecked()
            ? desktop_app::theme::ThemeMode::Light
            : desktop_app::theme::ThemeMode::Dark;
        runtimeSettings.setValue("shell/theme", currentThemeMode == desktop_app::theme::ThemeMode::Light ? "light" : "dark");
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
    headerLogoLabel->setPixmap(QPixmap(":/branding/opendss-icon-512.png").scaled(
        QSize(24, 24), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    shellHeaderLayout->addWidget(shellMenuButton);
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
    auto shellLogPathStatus = new QLabel("Log: " + QFileInfo(gLogPath).fileName());
    nameWidget(shellLogPathStatus, "OpenDssShellLogPathLabel");
    shellLogPathStatus->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    shellLogPathStatus->setToolTip(gLogPath);
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
    railLogo->setPixmap(QPixmap(":/branding/opendss-icon-512.png").scaled(
        QSize(26, 26), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    navLayout->addWidget(railLogo, 0, Qt::AlignHCenter);
    navLayout->addSpacing(8);
    auto navGroup = new QButtonGroup(&window);
    navGroup->setExclusive(true);
    auto addNavButton = [&](const QString& text, const QString& iconKey, QWidget* page, const char* objectName) {
        auto* button = new QPushButton;
        nameWidget(button, objectName);
        button->setCheckable(true);
        button->setProperty("railButton", true);
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
    auto liveNavButton = addNavButton("Live View", "play", liveWorkspacePage, "NavLiveButton");
    auto modelNavButton = addNavButton("Model", "model", modelWorkspacePage, "NavModelButton");
    auto datasetNavButton = addNavButton("Dataset", "dataset", datasetWorkspacePage, "NavDatasetButton");
    auto trainerNavButton = addNavButton("Trainer", "trainer", trainerWorkspacePage, "NavTrainerButton");
    auto validatorNavButton = addNavButton("Validator", "validator", validatorWorkspacePage, "NavValidatorButton");
    auto reportsNavButton = addNavButton("Reports", "reports", reportsWorkspacePage, "NavReportsButton");
    navLayout->addStretch(1);
    auto settingsNavButton = addNavButton("Settings", "settings", settingsWorkspacePage, "NavSettingsButton");
    auto wireHeaderChipNavigation = [&](QLabel* chip, QPushButton* destination, const QString& tooltip) {
        chip->setCursor(Qt::PointingHandCursor);
        chip->setToolTip(tooltip);
        chip->installEventFilter(new HeaderChipClickFilter([destination]() {
            if (destination) destination->click();
        }, chip));
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
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
    } else if (options.initialWorkspace == "validator") {
        workspaceStack->setCurrentWidget(validatorWorkspacePage);
        headerTitleLabel->setText("/ Validator");
        headerStatusText->setText("Validator workspace");
        validatorNavButton->setChecked(true);
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
    shellLayout->addWidget(shellContent, 1);
    centralWidget->setLayout(shellLayout);
    window.setCentralWidget(centralWidget);

    QObject::connect(trainerDockProxyButton, &QPushButton::clicked, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    auto logDock = new QDockWidget("Logs", &window);
    logDock->setObjectName("LogsDock");
    auto logDockText = new QPlainTextEdit;
    nameWidget(logDockText, "LogsTextEdit");
    logDockText->setObjectName("LogsText");
    logDockText->setReadOnly(true);
    logDockText->setPlainText("Session log: " + gLogPath + "\n\nLive log streaming remains handled by session_log.txt in this shell step.");
    logDock->setWidget(logDockText);
    logDock->setMinimumHeight(36);
    window.addDockWidget(Qt::BottomDockWidgetArea, logDock);
    window.resizeDocks({logDock}, {72}, Qt::Vertical);
    logDock->hide();

    auto diagnosticsDock = new QDockWidget("System Diagnostics", &window);
    diagnosticsDock->setObjectName("SystemDiagnosticsDock");
    auto diagnosticsLabel = new QLabel(
        "Application: shell loaded\n"
        "Camera/DCAM: checked by existing startup path\n"
        "Model: loaded through existing Pipeline controls\n"
        "DAQ: configured in Devices > DAQ / Trigger\n"
        "Python trainer: readiness checks available in Trainer tab\n"
        "Image validator: launch available in Validation > Image Validation\n"
        "Training launch, runner-wrapped sequence validation, and Model Promotion: disabled placeholders");
    nameWidget(diagnosticsLabel, "SystemDiagnosticsLabel");
    diagnosticsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    diagnosticsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    diagnosticsLabel->setWordWrap(true);
    diagnosticsDock->setWidget(diagnosticsLabel);
    window.addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
    diagnosticsDock->hide();
    QObject::connect(diagnosticsHeaderButton, &QToolButton::clicked, diagnosticsDock, &QDockWidget::show);

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
    nameWidget(window.statusBar(), "StatusBar");
    for (auto* item : {cameraStatusItem, modelStatusItem, daqStatusItem, pythonStatusItem, runStatusItem}) {
        item->setFrameStyle(QFrame::NoFrame);
        window.statusBar()->addPermanentWidget(item);
    }
    window.statusBar()->showMessage("Shell ready");
    window.menuBar()->hide();
    window.statusBar()->hide();

    auto setHeaderChipText = [](QLabel* label, const QString& text, int maximumWidth = 220) {
        if (!label) return;
        const int horizontalPadding = 28;
        const int textWidth = qMax(24, maximumWidth - horizontalPadding);
        const QString visibleText = label->fontMetrics().elidedText(text, Qt::ElideRight, textWidth);
        const int targetWidth = qMin(maximumWidth, label->fontMetrics().horizontalAdvance(visibleText) + horizontalPadding);
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
        if (text.isEmpty()) return text;
        text[0] = text[0].toUpper();
        return text;
    };
    auto runHeaderText = [&](const QString& runText, const QString& statusText) {
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("live view")) return QStringLiteral("Live View");
        if (run.contains("capture")) return QStringLiteral("Camera capture");
        if (run.contains("viewer-only")) return QStringLiteral("Camera viewer-only");
        if (run == QStringLiteral("idle")) return QStringLiteral("Idle");
        const QString simplifiedStatus = statusText.simplified();
        return simplifiedStatus.isEmpty() ? QStringLiteral("Idle") : simplifiedStatus;
    };
    auto cameraHeaderText = [&](const QString& cameraText, const QString& runText) {
        const QString camera = statusValue(cameraText, "Camera").toLower();
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("viewer-only") || camera.contains("unavailable")) return QStringLiteral("Camera viewer-only");
        if (camera.contains("mock acquiring")) return QStringLiteral("Camera mock acquiring");
        if (camera.contains("acquiring")) return QStringLiteral("Camera acquiring");
        if (camera.contains("mock")) return QStringLiteral("Camera mock");
        if (camera.contains("connected")) return QStringLiteral("Camera connected");
        if (camera.contains("error")) return QStringLiteral("Camera error");
        return QStringLiteral("Camera startup");
    };
    auto modelHeaderText = [&](const QString& modelText) {
        const QString model = statusValue(modelText, "Model").toLower();
        if (model.contains("loaded")) return QStringLiteral("Model SqueezeNet");
        if (model.contains("error")) return QStringLiteral("Model error");
        return QStringLiteral("Model not loaded");
    };
    auto daqHeaderText = [&](const QString& daqText) {
        const QString daq = statusValue(daqText, "DAQ").toLower();
        if (daq.contains("disabled") || daq.contains("unavailable")) return QStringLiteral("DAQ unavailable");
        if (daq.contains("available")) return QStringLiteral("DAQ available");
        return QStringLiteral("DAQ unchecked");
    };
    auto triggerHeaderText = [&](const QString& triggerText) {
        const QString trigger = triggerText.simplified().toLower();
        if (options.noDaq || trigger.contains("disabled") || trigger.contains("safe")) return QStringLiteral("Trigger safe");
        if (trigger.contains("queued")) return QStringLiteral("Trigger queued");
        if (trigger.contains("sent")) return QStringLiteral("Trigger sent");
        if (trigger.contains("failed")) return QStringLiteral("Trigger failed");
        return titleCaseStatus(triggerText);
    };

    auto shellStatusMirrorTimer = new QTimer(&window);
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
        setHeaderChipText(headerTriggerChip, appState.triggerArmed ? QStringLiteral("Trigger armed") : QStringLiteral("Trigger safe"), 145);
        shellHeaderLayout->invalidate();
        headerDaqChip->setProperty("chipTone", (daqStatusItem->text().contains("disabled") || daqStatusItem->text().contains("unavailable")) ? "disabled" :
            (daqStatusItem->text().contains("available") ? "running" : "warn"));
        headerModelChip->setProperty("chipTone", modelStatusItem->text().contains("loaded") ? "running" :
            (modelStatusItem->text().contains("error") ? "error" : "warn"));
        headerCameraChip->setProperty("chipTone", cameraStatusItem->text().contains("connected") || cameraStatusItem->text().contains("acquiring") ? "running" :
            (cameraStatusItem->text().contains("error") ? "error" : "warn"));
        headerTriggerChip->setProperty("chipTone", appState.triggerArmed ? "warn" : "running");
        headerCameraChip->style()->unpolish(headerCameraChip);
        headerCameraChip->style()->polish(headerCameraChip);
        headerModelChip->style()->unpolish(headerModelChip);
        headerModelChip->style()->polish(headerModelChip);
        headerDaqChip->style()->unpolish(headerDaqChip);
        headerDaqChip->style()->polish(headerDaqChip);
        headerTriggerChip->style()->unpolish(headerTriggerChip);
        headerTriggerChip->style()->polish(headerTriggerChip);

        const QRegularExpression intRe("(\\d+)");
        auto firstNumber = [&](const QString& text, const QString& fallback) {
            auto match = intRe.match(text);
            return match.hasMatch() ? match.captured(1) : fallback;
        };
        eventsMetricLabel->setText(firstNumber(statsEventsLabel->text(), "0"));
        hitsMetricLabel->setText(firstNumber(statsHitLabel->text(), "0"));
        wasteMetricLabel->setText(statsHitLabel->text().contains("Waste")
            ? firstNumber(statsHitLabel->text().section("Waste", 1), "0")
            : "0");
        trigMetricLabel->setText(pipelineEnableCheck->isChecked() ? "live" : "--");
        lastDecisionValue->setText(statsLastLabel->text().contains("--") ? "--" : statsLastLabel->text().simplified());
    });
    shellStatusMirrorTimer->start();

    QObject::connect(fitAction, &QAction::triggered, [&](){
        imageView->fitToView();
        cameraImageView->fitToView();
        window.statusBar()->showMessage("Preview images fit to view");
    });
    QObject::connect(oneToOneAction, &QAction::triggered, [=](){
        imageView->resetScale();
        cameraImageView->resetScale();
    });
    QObject::connect(zoomInAction, &QAction::triggered, [=](){
        imageView->zoomBySteps(1);
        cameraImageView->zoomBySteps(1);
    });
    QObject::connect(zoomOutAction, &QAction::triggered, [=](){
        imageView->zoomBySteps(-1);
        cameraImageView->zoomBySteps(-1);
    });
    QObject::connect(overlayAction, &QAction::toggled, leftOverlayCheck, &QCheckBox::setChecked);
    QObject::connect(leftOverlayCheck, &QCheckBox::toggled, overlayAction, &QAction::setChecked);
    QObject::connect(leftLoadBtn, &QPushButton::clicked, viewerBtn, &QPushButton::click);
    QObject::connect(leftReconnectBtn, &QPushButton::clicked, reconnectBtn, &QPushButton::click);
    QObject::connect(openViewerAction, &QAction::triggered, viewerBtn, &QPushButton::click);
    QObject::connect(reconnectAction, &QAction::triggered, reconnectBtn, &QPushButton::click);
    QObject::connect(startPreviewAction, &QAction::triggered, startBtn, &QPushButton::click);
    QObject::connect(stopPreviewAction, &QAction::triggered, stopBtn, &QPushButton::click);
    QObject::connect(captureStillAction, &QAction::triggered, captureBtn, &QPushButton::click);
    QObject::connect(startSortingAction, &QAction::triggered, pipelineStartBtn, &QPushButton::click);
    QObject::connect(stopSortingAction, &QAction::triggered, pipelineStopBtn, &QPushButton::click);
    QObject::connect(triggerSafeBtn, &QPushButton::toggled, [&](bool armed) {
        appState.triggerArmed = armed;
        updateForceTriggerState();
    });
    QObject::connect(manualTriggerAction, &QAction::triggered, liveForceTriggerBtn, &QPushButton::click);
    QObject::connect(liveForceTriggerBtn, &QPushButton::clicked, [&]() {
        updateForceTriggerState();
        if (!liveForceTriggerBtn->isEnabled()) return;
        labviewTestBtn->click();
    });
    QObject::connect(liveSnapshotBtn, &QPushButton::clicked, captureBtn, &QPushButton::click);
    QObject::connect(liveDetectorTuningBtn, &QPushButton::clicked, [&]() {
        liveDetectorDrawerOverlay->setVisible(!liveDetectorDrawerOverlay->isVisible());
        if (liveDetectorDrawerOverlay->isVisible()) liveDetectorDrawerOverlay->raise();
    });
    QObject::connect(liveDetectorClose, &QToolButton::clicked, liveDetectorDrawerOverlay, &QWidget::hide);
    QString currentRunDir;
    auto updateOpenRunAvailability = [&]() {
        const bool hasRun = !currentRunDir.trimmed().isEmpty() && QFileInfo(currentRunDir).isDir();
        liveOpenRunBtn->setEnabled(hasRun);
        openRunFolderAction->setEnabled(hasRun);
    };
    auto openPipelineOutputFolder = [&](){
        if (currentRunDir.trimmed().isEmpty() || !QFileInfo(currentRunDir).isDir()) {
            statusLabel->setText("No valid run folder is available yet.");
            window.statusBar()->showMessage("Open Run blocked: no valid run");
            logMessage("Open Run blocked: no valid run folder.");
            updateOpenRunAvailability();
            return;
        }
        QString dir = currentRunDir;
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    };
    QObject::connect(openRunFolderAction, &QAction::triggered, openPipelineOutputFolder);
    QObject::connect(openOutputAction, &QAction::triggered, openPipelineOutputFolder);
    QObject::connect(liveOpenRunBtn, &QPushButton::clicked, openPipelineOutputFolder);
    QObject::connect(showLogsAction, &QAction::triggered, logDock, &QDockWidget::show);
    QObject::connect(showDiagnosticsAction, &QAction::triggered, diagnosticsDock, &QDockWidget::show);
    QObject::connect(systemDiagnosticsAction, &QAction::triggered, diagnosticsDock, &QDockWidget::show);
    QObject::connect(modelManagerAction, &QAction::triggered, [=](){
        operationDock->show();
        operationalTabs->setCurrentWidget(modelManagerWidget);
    });
    QObject::connect(resetLayoutAction, &QAction::triggered, [&](){
        window.addDockWidget(Qt::LeftDockWidgetArea, operationDock);
        window.addDockWidget(Qt::BottomDockWidgetArea, logDock);
        window.addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
        operationDock->show();
        logDock->show();
        diagnosticsDock->hide();
        if (imageSubWindow) {
            imageSubWindow->show();
            imageSubWindow->resize(760, 620);
            imageSubWindow->move(16, 16);
        }
    });
    QObject::connect(exitAction, &QAction::triggered, &window, &QWidget::close);
    QObject::connect(aboutAction, &QAction::triggered, [&](){
        QMessageBox::about(&window,
                           "About Open Visual Droplet Sorter Suite",
                           "Open Visual Droplet Sorter Suite\n\n"
                           "Qt shell preview around the existing Hamamatsu live-view and droplet pipeline controls.\n\n"
                           "Image validation can be launched through the external Python validator. Trainer launch, runner-wrapped sequence validation, and model promotion remain disabled placeholders.");
    });

    // Logging helper
    auto logLine = [&](const QString& msg) {
        if (!logCheck->isChecked()) return;
        logMessage(msg);
    };

    auto csvQuote = [](const QString& s)->QString {
        QString out = s;
        out.replace("\"", "\"\"");
        return "\"" + out + "\"";
    };

    auto buildRunOutputDir = [&](const QString& prefix)->QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty()) base = QCoreApplication::applicationDirPath();
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
    updateOpenRunAvailability();

    auto buildDatasetBuilderDir = [&](QString* datasetIdOut)->QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty()) base = QCoreApplication::applicationDirPath();
        QDir baseDir(base);
        QString leaf = baseDir.dirName();
        if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
            baseDir.cdUp();
        }
        QString shortId = QUuid::createUuid().toString(QUuid::Id128).left(6).toLower();
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString datasetId = QString("builder_%1_live_%2").arg(stamp, shortId);
        if (datasetIdOut) *datasetIdOut = datasetId;
        QDir root(baseDir.filePath("datasets/builder"));
        root.mkpath(".");
        root.mkpath(datasetId);
        return root.filePath(datasetId);
    };

    auto pickExistingPath = [](const QStringList& candidates)->QString {
        for (const auto& c : candidates) {
            if (QFileInfo::exists(c)) return c;
        }
        return candidates.isEmpty() ? QString() : candidates.first();
    };

    QString appDir = QCoreApplication::applicationDirPath();
    auto findModelUpwards = [&](const QString& filename)->QString {
        QDir dir(appDir);
        for (int i = 0; i < 6; ++i) {
            QString candidate = dir.filePath("models/" + filename);
            if (QFileInfo::exists(candidate)) return candidate;
            if (!dir.cdUp()) break;
        }
        const QString projectRoot = findProjectRootFromApp();
        if (!projectRoot.isEmpty()) {
            const QString promotedArtifact = runtimeModelArtifactPath(projectRoot, "app/runtime/models/" + filename);
            if (!promotedArtifact.isEmpty()) return promotedArtifact;
        }
        return QString();
    };
    auto findOutputsRootUpwards = [&]()->QString {
        QDir dir(appDir);
        for (int i = 0; i < 8; ++i) {
            QString outputsDir = dir.filePath("outputs");
            if (QFileInfo(outputsDir).isDir()) {
                return QDir(outputsDir).filePath("pipeline_output");
            }
            if (!dir.cdUp()) break;
        }
        return QString();
    };
    auto findProjectRootUpwards = [&]()->QString {
        QDir dir(appDir);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) break;
        }
        QDir cwd(QDir::currentPath());
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(cwd.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return cwd.absolutePath();
            }
            if (!cwd.cdUp()) break;
        }
        return QString();
    };
    QString projectRoot = findProjectRootUpwards();
    auto projectPath = [&](const QString& rel)->QString {
        return projectRoot.isEmpty() ? QString() : QDir(projectRoot).absoluteFilePath(rel);
    };
    QString defaultTrainerOutput = defaultWorkspacePaths.runs;
    QString defaultTrainerDataset = defaultWorkspacePaths.preparedDataset;
    {
        QSettings settings;
        trainerPythonEdit->setText(settings.value("settings/pythonTrainer", trainerPythonEdit->text()).toString());
        if (QFileInfo(defaultTrainerDataset).isDir()) {
            trainerDatasetEdit->setText(settings.value("settings/datasetsRoot", QDir::toNativeSeparators(defaultTrainerDataset)).toString());
        } else {
            trainerDatasetEdit->setText(settings.value("settings/datasetsRoot", trainerDatasetEdit->text()).toString());
        }
        trainerOutputEdit->setText(settings.value("trainer/outputDir", QDir::toNativeSeparators(defaultTrainerOutput)).toString());
        const QString arch = settings.value("trainer/architecture", trainerArchitectureCombo->currentData().toString()).toString();
        int archIndex = trainerArchitectureCombo->findData(arch);
        if (archIndex >= 0) trainerArchitectureCombo->setCurrentIndex(archIndex);
        const bool pretrained = settings.value("trainer/pretrained", true).toBool();
        trainerPretrainedImageNetBtn->setChecked(pretrained);
        trainerPretrainedNoneBtn->setChecked(!pretrained);
        trainerEpochsSpin->setValue(settings.value("trainer/epochs", trainerEpochsSpin->value()).toInt());
        trainerBatchSpin->setValue(settings.value("trainer/batchSize", trainerBatchSpin->value()).toInt());
        trainerLrSpin->setValue(settings.value("trainer/learningRate", trainerLrSpin->value()).toDouble());
        trainerFlipCheck->setChecked(settings.value("trainer/augment/randomFlip", trainerFlipCheck->isChecked()).toBool());
        trainerRotationCheck->setChecked(settings.value("trainer/augment/randomRotation", trainerRotationCheck->isChecked()).toBool());
        trainerColorJitterCheck->setChecked(settings.value("trainer/augment/colorJitter", trainerColorJitterCheck->isChecked()).toBool());
        trainerRandomCropCheck->setChecked(settings.value("trainer/augment/randomCrop", trainerRandomCropCheck->isChecked()).toBool());
        const QString scheduler = settings.value("trainer/scheduler", trainerSchedulerCombo->currentText()).toString();
        int schedulerIndex = trainerSchedulerCombo->findText(scheduler);
        if (schedulerIndex >= 0) trainerSchedulerCombo->setCurrentIndex(schedulerIndex);
    }
    QPointer<DatasetLabelerDialog> activeDatasetLabelerDialog;
    auto openDatasetLabelerPath = [&](const QString& preferredPath) {
        QString initialDataset = trainerDatasetEdit->text().trimmed();
        if (!preferredPath.trimmed().isEmpty()) {
            initialDataset = preferredPath.trimmed();
        }
        if (initialDataset.isEmpty() || !QFileInfo(initialDataset).exists()) {
            initialDataset = defaultTrainerDataset;
        }
        if (!activeDatasetLabelerDialog.isNull()) {
            activeDatasetLabelerDialog->close();
        }
        auto* dialog = new DatasetLabelerDialog(&window, QFileInfo(initialDataset).exists() ? initialDataset : QString());
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        activeDatasetLabelerDialog = dialog;
        QObject::connect(&app, &QCoreApplication::aboutToQuit, dialog, &QDialog::close);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    };
    auto openDatasetLabeler = [&]() {
        openDatasetLabelerPath(QString());
    };

    QProcess* trainerProcess = nullptr;
    bool trainerCommandWasTraining = false;
    bool trainerCommandWasDryRun = false;
    auto appendTrainerLog = [&](const QString& text) {
        if (text.isEmpty()) return;
        trainerResultText->moveCursor(QTextCursor::End);
        trainerResultText->insertPlainText(text);
        trainerResultText->moveCursor(QTextCursor::End);
    };
    auto quoteTrainerArg = [](QString arg) {
        arg.replace("\"", "\\\"");
        return arg.contains(' ') ? "\"" + arg + "\"" : arg;
    };
    auto trainerCommandPreview = [&](const QString& program, const QStringList& args) {
        QStringList pieces{quoteTrainerArg(program)};
        for (const QString& arg : args) pieces << quoteTrainerArg(arg);
        return pieces.join(" ");
    };
    auto saveTrainerSettings = [&]() {
        QSettings settings;
        settings.setValue("settings/pythonTrainer", trainerPythonEdit->text().trimmed());
        settings.setValue("settings/datasetsRoot", trainerDatasetEdit->text().trimmed());
        settings.setValue("trainer/outputDir", trainerOutputEdit->text().trimmed());
        settings.setValue("trainer/architecture", trainerArchitectureCombo->currentData().toString());
        settings.setValue("trainer/pretrained", trainerPretrainedImageNetBtn->isChecked());
        settings.setValue("trainer/epochs", trainerEpochsSpin->value());
        settings.setValue("trainer/batchSize", trainerBatchSpin->value());
        settings.setValue("trainer/learningRate", trainerLrSpin->value());
        settings.setValue("trainer/augment/randomFlip", trainerFlipCheck->isChecked());
        settings.setValue("trainer/augment/randomRotation", trainerRotationCheck->isChecked());
        settings.setValue("trainer/augment/colorJitter", trainerColorJitterCheck->isChecked());
        settings.setValue("trainer/augment/randomCrop", trainerRandomCropCheck->isChecked());
        settings.setValue("trainer/scheduler", trainerSchedulerCombo->currentText());
    };
    auto setTrainerBusy = [&](bool busy) {
        trainerEnvCheckBtn->setEnabled(!busy);
        trainerConfigurePathBtn->setEnabled(!busy);
        trainerPythonBrowseBtn->setEnabled(!busy);
        trainerDatasetBrowseBtn->setEnabled(!busy);
        trainerOutputBrowseBtn->setEnabled(!busy);
        trainerStartTrainingBtn->setEnabled(!busy);
        trainerDryRunBtn->setEnabled(!busy);
        trainerStartTrainingBtn->setText(busy && trainerCommandWasTraining ? "Training..." : "Start Training");
        trainerCancelBtn->setEnabled(busy);
        trainerProgressBar->setRange(busy ? 0 : 0, busy ? 0 : 100);
        trainerProgressBar->setValue(0);
        trainerProgressBar->setFormat(busy ? "Running..." : "Idle");
    };
    auto trainingConfigPath = [&]()->QString {
        const QString outputDir = trainerOutputEdit->text().trimmed();
        if (outputDir.isEmpty()) return QString();
        QDir().mkpath(outputDir);
        QJsonObject config;
        config["schema_version"] = 1;
        config["architecture"] = trainerArchitectureCombo->currentData().toString();
        config["batch_size"] = trainerBatchSpin->value();
        config["epochs"] = trainerEpochsSpin->value();
        config["pretrained"] = trainerPretrainedImageNetBtn->isChecked();
        QJsonArray stages;
        QJsonObject stage;
        stage["name"] = "gui";
        stage["epochs"] = trainerEpochsSpin->value();
        stage["learning_rate"] = trainerLrSpin->value();
        stage["trainable"] = "fine_tune";
        stages.append(stage);
        config["stages"] = stages;
        QJsonObject augmentation;
        augmentation["random_flip"] = trainerFlipCheck->isChecked();
        augmentation["random_rotation"] = trainerRotationCheck->isChecked();
        augmentation["color_jitter"] = trainerColorJitterCheck->isChecked();
        augmentation["random_crop"] = trainerRandomCropCheck->isChecked();
        config["augmentation"] = augmentation;
        config["scheduler"] = trainerSchedulerCombo->currentText();
        const QString path = QDir(outputDir).absoluteFilePath("trainer_gui_config.json");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            trainerStatusLabel->setText("Unable to write trainer config: " + file.errorString());
            return QString();
        }
        file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
        file.close();
        return path;
    };
    auto trainerTrainArgs = [&](bool dryRun)->QStringList {
        QStringList args = {
            "-m", "droplet_trainer",
            "train",
            "--dataset", trainerDatasetEdit->text().trimmed(),
            "--output", trainerOutputEdit->text().trimmed(),
            "--config", trainingConfigPath(),
            "--jsonl"
        };
        if (dryRun) args << "--dry-run";
        return args;
    };
    auto startTrainerCommand = [&](const QString& label, const QStringList& args, bool isTraining, bool isDryRun) {
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            trainerStatusLabel->setText("A trainer command is already running.");
            return;
        }
        QString python = trainerPythonEdit->text().trimmed();
        if (python.isEmpty()) {
            trainerStatusLabel->setText("Select a Python executable before running trainer commands.");
            return;
        }
        if ((isTraining || isDryRun) && (trainerDatasetEdit->text().trimmed().isEmpty() || trainerOutputEdit->text().trimmed().isEmpty() || args.contains(QString()))) {
            trainerStatusLabel->setText("Dataset, output directory, and generated config are required before training.");
            return;
        }
        saveTrainerSettings();
        trainerCommandWasTraining = isTraining;
        trainerCommandWasDryRun = isDryRun;
        trainerResultText->clear();
        appendTrainerLog(QString("Running %1\n%2\n\n").arg(label, trainerCommandPreview(python, args)));
        trainerStatusLabel->setText(QString("%1 running...").arg(label));
        pythonStatusItem->setText("Python: checking");
        setTrainerBusy(true);

        auto* process = new QProcess(&window);
        trainerProcess = process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString trainingPython = projectPath("training/python");
        if (QFileInfo(trainingPython).isDir()) {
            QString existing = env.value("PYTHONPATH");
            env.insert("PYTHONPATH", existing.isEmpty() ? trainingPython : trainingPython + QDir::listSeparator() + existing);
        }
        process->setProcessEnvironment(env);
        if (!projectRoot.isEmpty()) {
            process->setWorkingDirectory(projectRoot);
        }
        QObject::connect(process, &QProcess::readyReadStandardOutput, &window, [&, process]() {
            if (trainerProcess != process) return;
            const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
            appendTrainerLog(chunk);
            int progressIndex = chunk.indexOf("\"percent\"");
            if (progressIndex >= 0) {
                QRegularExpression rx("\"percent\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
                auto match = rx.match(chunk, progressIndex);
                if (match.hasMatch()) {
                    trainerProgressBar->setRange(0, 100);
                    trainerProgressBar->setValue(qBound(0, static_cast<int>(match.captured(1).toDouble()), 100));
                    trainerProgressBar->setFormat(QString("%1%").arg(trainerProgressBar->value()));
                }
            }
        });
        QObject::connect(process, &QProcess::readyReadStandardError, &window, [&, process]() {
            if (trainerProcess != process) return;
            appendTrainerLog(QString::fromLocal8Bit(process->readAllStandardError()));
        });
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &window, [&, process](int exitCode, QProcess::ExitStatus exitStatus) {
            if (trainerProcess != process) return;
            const bool crashed = exitStatus == QProcess::CrashExit;
            appendTrainerLog(QString("\nProcess finished: exit=%1%2\n").arg(exitCode).arg(crashed ? " crashed" : ""));
            const bool ok = exitCode == 0 && !crashed;
            if (trainerCommandWasDryRun) {
                trainerStatusLabel->setText(ok ? "Dry run completed." : "Dry run failed. Review log output.");
            } else if (trainerCommandWasTraining) {
                trainerStatusLabel->setText(ok ? "Training completed." : "Training failed. Review log output.");
            } else {
                trainerStatusLabel->setText(ok ? "Python environment validated." : "Environment validation failed.");
            }
            pythonStatusItem->setText(ok ? "Python: ready" : "Python: issue");
            setTrainerBusy(false);
            trainerProcess = nullptr;
            process->deleteLater();
        });
        QObject::connect(process, &QProcess::errorOccurred, &window, [&, process](QProcess::ProcessError error) {
            if (trainerProcess != process) return;
            Q_UNUSED(error);
            trainerStatusLabel->setText("Failed to start trainer subprocess.");
            appendTrainerLog("Process error: " + process->errorString() + "\n");
            pythonStatusItem->setText("Python: start failed");
            setTrainerBusy(false);
            trainerProcess = nullptr;
            process->deleteLater();
        });
        process->start(python, args);
    };
    QObject::connect(trainerPythonBrowseBtn, &QPushButton::clicked, [&]() {
        QString file = QFileDialog::getOpenFileName(&window, "Select Python executable", trainerPythonEdit->text(),
                                                    "Python executable (python.exe python);;All files (*.*)");
        if (!file.isEmpty()) trainerPythonEdit->setText(QDir::toNativeSeparators(file));
    });
    QObject::connect(trainerDatasetBrowseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&window, "Select dataset directory", trainerDatasetEdit->text());
        if (!dir.isEmpty()) trainerDatasetEdit->setText(QDir::toNativeSeparators(dir));
    });
    QObject::connect(trainerOutputBrowseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&window, "Select training output directory", trainerOutputEdit->text());
        if (!dir.isEmpty()) trainerOutputEdit->setText(QDir::toNativeSeparators(dir));
    });
    QObject::connect(trainerEnvCheckBtn, &QPushButton::clicked, [&]() {
        const QString code = "import sys, droplet_trainer; print('Python ' + sys.version.split()[0] + ' -- trainer available')";
        startTrainerCommand("Environment validation", {"-c", code}, false, false);
    });
    QObject::connect(trainerDryRunBtn, &QPushButton::clicked, [&]() {
        startTrainerCommand("Training dry run", trainerTrainArgs(true), false, true);
    });
    QObject::connect(trainerStartTrainingBtn, &QPushButton::clicked, [&]() {
        startTrainerCommand("Training", trainerTrainArgs(false), true, false);
    });
    QObject::connect(trainerCancelBtn, &QPushButton::clicked, [&]() {
        if (!trainerProcess || trainerProcess->state() == QProcess::NotRunning) return;
        trainerStatusLabel->setText("Canceling trainer subprocess...");
        trainerProcess->terminate();
        QPointer<QProcess> processPtr(trainerProcess);
        QTimer::singleShot(3000, &window, [processPtr]() {
            if (!processPtr.isNull() && processPtr->state() != QProcess::NotRunning) {
                processPtr->kill();
            }
        });
    });
    for (auto* edit : {trainerPythonEdit, trainerDatasetEdit, trainerOutputEdit}) {
        QObject::connect(edit, &QLineEdit::textChanged, saveTrainerSettings);
    }
    QObject::connect(trainerArchitectureCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveTrainerSettings);
    QObject::connect(trainerPretrainedImageNetBtn, &QPushButton::toggled, saveTrainerSettings);
    QObject::connect(trainerEpochsSpin, qOverload<int>(&QSpinBox::valueChanged), saveTrainerSettings);
    QObject::connect(trainerBatchSpin, qOverload<int>(&QSpinBox::valueChanged), saveTrainerSettings);
    QObject::connect(trainerLrSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), saveTrainerSettings);
    QObject::connect(trainerFlipCheck, &QCheckBox::toggled, saveTrainerSettings);
    QObject::connect(trainerRotationCheck, &QCheckBox::toggled, saveTrainerSettings);
    QObject::connect(trainerColorJitterCheck, &QCheckBox::toggled, saveTrainerSettings);
    QObject::connect(trainerRandomCropCheck, &QCheckBox::toggled, saveTrainerSettings);
    QObject::connect(trainerSchedulerCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveTrainerSettings);
    QObject::connect(datasetOpenAction, &QAction::triggered, openDatasetLabeler);
    QObject::connect(datasetBuildAction, &QAction::triggered, openDatasetLabeler);
    QObject::connect(datasetLabelDatasetAction, &QAction::triggered, openDatasetLabeler);
    QObject::connect(datasetReadinessAction, &QAction::triggered, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
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
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    QObject::connect(trainingValidateEnvironmentAction, &QAction::triggered, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerEnvCheckBtn->click();
    });

    QString defaultOnnxRel = "../../../models/pre_binary_promotion_backup.onnx";
    QString defaultMetaRel = "../../../models/pre_binary_promotion_backup_metadata.json";
    QString defaultOnnxAbs = QDir(appDir).absoluteFilePath(defaultOnnxRel);
    QString defaultMetaAbs = QDir(appDir).absoluteFilePath(defaultMetaRel);
    QString defaultOnnxFromProject = findModelUpwards("pre_binary_promotion_backup.onnx");
    QString defaultMetaFromProject = findModelUpwards("pre_binary_promotion_backup_metadata.json");
    QStringList onnxCandidates = {
        defaultWorkspacePaths.activeModel,
        defaultOnnxFromProject,
        defaultOnnxAbs,
        appDir + "/pre_binary_promotion_backup.onnx",
        appDir + "/models/pre_binary_promotion_backup.onnx",
        appDir + "/../models/pre_binary_promotion_backup.onnx",
        appDir + "/../../models/pre_binary_promotion_backup.onnx"
    };
    QStringList metaCandidates = {
        defaultWorkspacePaths.activeMetadata,
        defaultMetaFromProject,
        defaultMetaAbs,
        appDir + "/pre_binary_promotion_backup_metadata.json",
        appDir + "/models/pre_binary_promotion_backup_metadata.json",
        appDir + "/../models/pre_binary_promotion_backup_metadata.json",
        appDir + "/../../models/pre_binary_promotion_backup_metadata.json"
    };
    QString onnxPicked = pickExistingPath(onnxCandidates);
    if (onnxPicked.isEmpty()) {
        onnxPicked = defaultOnnxRel;
    } else {
        onnxPicked = QDir(appDir).relativeFilePath(onnxPicked);
    }
    QString metaPicked = pickExistingPath(metaCandidates);
    if (metaPicked.isEmpty()) {
        metaPicked = defaultMetaRel;
    } else {
        metaPicked = QDir(appDir).relativeFilePath(metaPicked);
    }
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

    auto addLiveModelRow = [&](const QString& label,
                               const QString& id,
                               const QString& onnxPath,
                               const QString& metadataPath,
                               const QString& state,
                               const QString& mode,
                               const QString& targetClassId,
                               const QString& summary,
                               const QString& onnxSha256,
                               const QString& metadataSha256,
                               bool selectable) {
        liveModelCombo->addItem(label);
        const int index = liveModelCombo->count() - 1;
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
        if (label.isEmpty()) label = registryString(entry, "registry_entry_id");
        label += " - " + registryString(entry, "state");
        if (!targetId.isEmpty()) {
            label += " - " + (targetDisplay.isEmpty() ? targetId : QString("%1 (%2)").arg(targetDisplay, targetId));
        }
        const bool selectable = entry.value("selectable_for_normal_live_sorting").toBool(false) &&
            registryString(entry, "live_use_mode") != "blocked";
        addLiveModelRow(
            label,
            registryString(entry, "registry_entry_id"),
            runtimePathFromRegistryPath(registryString(entry, "model_path")),
            runtimePathFromRegistryPath(registryString(entry, "metadata_path")),
            registryString(entry, "state"),
            selectable ? registryString(entry, "live_use_mode") : "blocked",
            targetId,
            registryEntrySummary(entry, registryFilePath, registryLoadWarning),
            registryString(entry, "model_sha256"),
            registryString(entry, "metadata_sha256"),
            selectable);
    }
    if (liveModelCombo->count() == 0) {
        addLiveModelRow(
            "Temporary static fallback - promoted/current binary runtime - Hits (1)",
            "run_20260429_221500_wsl2_binary_linuxmirror_onnx",
            onnxPicked,
            metaPicked,
            "promoted_current",
            "normal",
            "1",
            "Temporary static fallback row. Registry file was empty or unavailable.",
            "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769",
            "fa5321dfad900baec23fa6c239a29279e0e8c03fa2e78f0bd679dfb973888d2f",
            true);
    }
    int activeLiveModelIndex = 0;
    for (int i = 0; i < liveModelCombo->count(); ++i) {
        if (liveModelCombo->itemData(i, kLiveModelModeRole).toString() != "blocked") {
            activeLiveModelIndex = i;
            break;
        }
    }
    liveModelCombo->setCurrentIndex(activeLiveModelIndex);
    appState.activeModelId = liveModelCombo->currentData(kLiveModelIdRole).toString();
    liveModelSummaryText->setPlainText(liveModelCombo->currentData(kLiveModelSummaryRole).toString());

    QString pendingTargetClassId = appState.targetClassId;
    auto selectedTargetClassId = [&]()->QString {
        if (!targetClassCombo) return QString();
        QVariant data = targetClassCombo->currentData();
        QString classId = data.isValid() ? data.toString().trimmed() : QString();
        if (!classId.isEmpty()) return classId;
        classId = targetClassCombo->currentText().trimmed();
        return classId.isEmpty() ? appState.targetClassId : classId;
    };

    auto setSelectedTargetClassId = [&](const QString& classId) {
        pendingTargetClassId = classId.trimmed();
        if (pendingTargetClassId.isEmpty()) return;
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
        runtimeSettings.setValue("runtime/v1/model/targetClassId", appState.targetClassId);
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
        if (!runtimeSettings.contains(kRuntimeSettingsSchemaVersionKey)) return;
        const int schemaVersion = runtimeSettings.value(kRuntimeSettingsSchemaVersionKey, 0).toInt();
        if (schemaVersion < 1 || schemaVersion > kRuntimeSettingsSchemaVersion) return;

        onnxEdit->setText(runtimeSettings.value("runtime/v1/model/path", onnxEdit->text()).toString());
        metaEdit->setText(runtimeSettings.value("runtime/v1/model/metadataPath", metaEdit->text()).toString());
        setSelectedTargetClassId(runtimeSettings.value("runtime/v1/model/targetClassId", pendingTargetClassId).toString());
        outputEdit->setText(runtimeSettings.value("runtime/v1/output/baseDir", outputEdit->text()).toString());
        saveCropCheck->setChecked(runtimeSettings.value("runtime/v1/output/saveCrops", saveCropCheck->isChecked()).toBool());
        saveOverlayCheck->setChecked(runtimeSettings.value("runtime/v1/output/saveOverlays", saveOverlayCheck->isChecked()).toBool());

        setComboTextIfPresent(presetCombo, runtimeSettings.value("runtime/v1/camera/presetText").toString());
        customWidthSpin->setValue(runtimeSettings.value("runtime/v1/camera/customWidth", customWidthSpin->value()).toInt());
        customHeightSpin->setValue(runtimeSettings.value("runtime/v1/camera/customHeight", customHeightSpin->value()).toInt());
        setComboTextIfPresent(binCombo, runtimeSettings.value("runtime/v1/camera/binning").toString());
        setComboTextIfPresent(bitsCombo, runtimeSettings.value("runtime/v1/camera/bits").toString());
        exposureSpin->setValue(runtimeSettings.value("runtime/v1/camera/exposureMs", exposureSpin->value()).toDouble());
        setComboTextIfPresent(readoutCombo, runtimeSettings.value("runtime/v1/camera/readoutSpeed").toString());
        displayEverySpin->setValue(runtimeSettings.value("runtime/v1/camera/displayEvery", displayEverySpin->value()).toInt());
        lutMinSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMin", lutMinSpin->value()).toInt());
        lutMaxSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMax", lutMaxSpin->value()).toInt());
        savePathEdit->setText(runtimeSettings.value("runtime/v1/camera/savePath", savePathEdit->text()).toString());

        frameSkipSpin->setValue(runtimeSettings.value("runtime/v1/detector/frameSkip", frameSkipSpin->value()).toInt());
        bgFramesSpin->setValue(runtimeSettings.value("runtime/v1/detector/bgFrames", bgFramesSpin->value()).toInt());
        bgUpdateSpin->setValue(runtimeSettings.value("runtime/v1/detector/bgUpdateFrames", bgUpdateSpin->value()).toInt());
        resetFramesSpin->setValue(runtimeSettings.value("runtime/v1/detector/resetFrames", resetFramesSpin->value()).toInt());
        minAreaSpin->setValue(runtimeSettings.value("runtime/v1/detector/minArea", minAreaSpin->value()).toDouble());
        minAreaFracSpin->setValue(runtimeSettings.value("runtime/v1/detector/minAreaFrac", minAreaFracSpin->value()).toDouble());
        maxAreaFracSpin->setValue(runtimeSettings.value("runtime/v1/detector/maxAreaFrac", maxAreaFracSpin->value()).toDouble());
        minBboxSpin->setValue(runtimeSettings.value("runtime/v1/detector/minBbox", minBboxSpin->value()).toInt());
        marginSpin->setValue(runtimeSettings.value("runtime/v1/detector/margin", marginSpin->value()).toInt());
        diffThreshSpin->setValue(runtimeSettings.value("runtime/v1/detector/diffThresh", diffThreshSpin->value()).toInt());
        blurRadiusSpin->setValue(runtimeSettings.value("runtime/v1/detector/blurRadius", blurRadiusSpin->value()).toInt());
        morphRadiusSpin->setValue(runtimeSettings.value("runtime/v1/detector/morphRadius", morphRadiusSpin->value()).toInt());
        scaleSpin->setValue(runtimeSettings.value("runtime/v1/detector/scale", scaleSpin->value()).toDouble());
        gapFireSpin->setValue(runtimeSettings.value("runtime/v1/detector/gapFireShift", gapFireSpin->value()).toInt());
    };

    auto runtimeSettingsSnapshot = [&](const QString& runMode)->QJsonObject {
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
        if (runDir.trimmed().isEmpty()) return;
        QDir dir(runDir);
        dir.mkpath(".");
        QFile file(dir.filePath("runtime_settings_snapshot.json"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            logMessage(QString("Failed to write runtime settings snapshot: %1").arg(file.errorString()));
            return;
        }
        file.write(QJsonDocument(runtimeSettingsSnapshot(runMode)).toJson(QJsonDocument::Indented));
    };

    auto resolveAppRelative = [&](const QString& path)->QString {
        if (path.isEmpty()) return path;
        QFileInfo info(path);
        if (info.isAbsolute()) return info.absoluteFilePath();
        QString abs = QDir(appDir).absoluteFilePath(path);
        if (QFileInfo::exists(abs)) return abs;
        QString fallback = findModelUpwards(QFileInfo(path).fileName());
        if (!fallback.isEmpty()) return fallback;
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
            targetClassCombo->addItem(fallbackId, fallbackId);
            pendingTargetClassId = fallbackId;
            logMessage(QString("Target selector using legacy fallback class id '%1': %2")
                .arg(fallbackId, QString::fromStdString(err)));
            return;
        }

        for (const std::string& classIdStd : metadata.classes) {
            const std::string displayLabel = DisplayLabelForClassId(metadata, classIdStd);
            const QString classId = QString::fromStdString(classIdStd);
            const QString displayText = QString::fromStdString(FormatClassForDisplay(classIdStd, displayLabel));
            targetClassCombo->addItem(displayText, classId);
        }

        std::string resolvedClassId;
        std::string resolvedDisplayLabel;
        std::string resolveErr;
        if (!ResolveTargetClassId(metadata,
                                  requestedClassId.toStdString(),
                                  std::string(),
                                  resolvedClassId,
                                  resolvedDisplayLabel,
                                  resolveErr)) {
            ResolveTargetClassId(metadata,
                                 std::string(),
                                 std::string(),
                                 resolvedClassId,
                                 resolvedDisplayLabel,
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
            pipelineStatusLabel->setText("Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            return;
        }
        const QString onnxPath = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString metadataPath = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        const QJsonObject packagedActiveEntry = activeRegistryEntry(registryEntries);
        const bool selectedPackagedActive = appState.activeModelId == registryString(packagedActiveEntry, "registry_entry_id");
        if (selectedPackagedActive && !defaultWorkspacePaths.activeModel.isEmpty()) {
            onnxEdit->setText(defaultWorkspacePaths.activeModel);
        } else if (!onnxPath.isEmpty()) {
            onnxEdit->setText(onnxPath);
        }
        if (selectedPackagedActive && !defaultWorkspacePaths.activeMetadata.isEmpty()) {
            metaEdit->setText(defaultWorkspacePaths.activeMetadata);
        } else if (!metadataPath.isEmpty()) {
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

    auto findPathUpwards = [&](const QString& relativePath)->QString {
        QDir dir(appDir);
        for (int i = 0; i < 10; ++i) {
            QString candidate = dir.filePath(relativePath);
            if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
            if (!dir.cdUp()) break;
        }
        return QString();
    };

    auto defaultValidationOutput = [&]()->QString {
        QString runName = "validation_gui_image_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        return QDir(defaultWorkspacePaths.runs).filePath(runName);
    };

    QObject::connect(imageValidationAction, &QAction::triggered, [&](){
        QSettings settings;
        QString trainerPythonPath = findPathUpwards("training/python");
        QString datasetPath = settings.value("validator/imageDataset", defaultWorkspacePaths.preparedDataset).toString();
        QString validationOutput = settings.value("validator/outputFolder", defaultValidationOutput()).toString();
        ImageValidationDialog dialog(&window,
                                     settings.value("validator/pythonExecutable", "python").toString(),
                                     resolveAppRelative(onnxEdit->text().trimmed()),
                                     resolveAppRelative(metaEdit->text().trimmed()),
                                     datasetPath,
                                     validationOutput,
                                     trainerPythonPath);
        dialog.exec();
        pythonStatusItem->setText("Python: validator configured");
    });

    QTimer labviewApplyTimer;
    QTimer detectorTuningApplyTimer;
    bool autoApplyLabview = true;
    std::function<void()> scheduleLabviewApply = [](){};
    auto setLabviewStatus = [&](const QString& text, const QString& color){
        labviewStatusText->setText(text);
        labviewStatusDot->setStyleSheet(QString("background:%1;border-radius:7px;border:1px solid #333;").arg(color));
    };
    struct DaqAvailabilityState {
        bool available = false;
        bool disabled = false;
        bool fault = false;
        QString statusText;
        QString faultText;
        QString indicatorText;
        QString indicatorColor;
    };
    std::vector<DaqDeviceInfo> discoveredDaqDevices;
    QString daqDiscoveryError;
    auto sameText = [](const QString& left, const QString& right) {
        return left.compare(right, Qt::CaseInsensitive) == 0;
    };
    auto channelDeviceName = [&](const QString& channel) {
        const QString trimmed = channel.trimmed();
        const int slash = trimmed.indexOf('/');
        return slash > 0 ? trimmed.left(slash) : trimmed;
    };
    auto channelSuffix = [&](const QString& channel) {
        const QString trimmed = channel.trimmed();
        const int slash = trimmed.indexOf('/');
        return (slash >= 0 && slash + 1 < trimmed.size()) ? trimmed.mid(slash + 1) : QString();
    };
    auto parseSimulatedDaqDevices = [&]() {
        std::vector<DaqDeviceInfo> devices;
        const QString raw = qEnvironmentVariable("OVDS_DAQ_SIMULATED_DEVICES").trimmed();
        if (raw.isEmpty()) return devices;

        const QStringList deviceSpecs = raw.split(';', Qt::SkipEmptyParts);
        for (const QString& deviceSpec : deviceSpecs) {
            const QStringList parts = deviceSpec.split('|');
            if (parts.isEmpty()) continue;

            DaqDeviceInfo info;
            info.name = parts.value(0).trimmed().toStdString();
            info.productType = parts.value(1).trimmed().toStdString();
            const QStringList channels = parts.value(2).split(',', Qt::SkipEmptyParts);
            for (const QString& channel : channels) {
                const QString trimmedChannel = channel.trimmed();
                if (!trimmedChannel.isEmpty()) info.aoChannels.push_back(trimmedChannel.toStdString());
            }
            if (!info.name.empty()) devices.push_back(std::move(info));
        }
        std::sort(devices.begin(), devices.end(), [](const DaqDeviceInfo& left, const DaqDeviceInfo& right) {
            return left.name < right.name;
        });
        return devices;
    };
    auto findDiscoveredDaqDevice = [&](const QString& deviceName) -> const DaqDeviceInfo* {
        for (const auto& device : discoveredDaqDevices) {
            if (sameText(QString::fromStdString(device.name), deviceName.trimmed())) return &device;
        }
        return nullptr;
    };
    auto discoveredCompatibleDeviceCount = [&]() {
        int count = 0;
        for (const auto& device : discoveredDaqDevices) {
            if (device.isCompatible()) ++count;
        }
        return count;
    };
    auto formatDaqDeviceLabel = [&](const DaqDeviceInfo& device) {
        QString label = QString::fromStdString(device.name);
        const QString product = QString::fromStdString(device.productType).trimmed();
        if (!product.isEmpty()) label += QStringLiteral(" - %1").arg(product);
        if (!device.isCompatible()) label += QStringLiteral(" (no AO output)");
        return label;
    };
    auto describeDiscoveredDaqDevices = [&]() {
        QStringList parts;
        for (const auto& device : discoveredDaqDevices) {
            QString part = formatDaqDeviceLabel(device);
            if (device.isCompatible()) {
                QStringList channels;
                for (const std::string& channel : device.aoChannels) channels << QString::fromStdString(channel);
                if (!channels.isEmpty()) part += QStringLiteral(" [%1]").arg(channels.join(", "));
            }
            parts << part;
        }
        return parts.join("; ");
    };
    auto chooseChannelForDevice = [&](const DaqDeviceInfo& device, const QString& currentChannel, const QString& previousDeviceName) {
        if (!device.isCompatible()) return QString();

        const QString trimmedChannel = currentChannel.trimmed();
        const QString selectedDeviceName = QString::fromStdString(device.name);
        auto resolveExactChannel = [&](const QString& candidate) {
            for (const std::string& channel : device.aoChannels) {
                const QString discoveredChannel = QString::fromStdString(channel);
                if (sameText(discoveredChannel, candidate)) return discoveredChannel;
            }
            return QString();
        };

        if (sameText(channelDeviceName(trimmedChannel), selectedDeviceName)) {
            const QString exactCurrent = resolveExactChannel(trimmedChannel);
            if (!exactCurrent.isEmpty()) return exactCurrent;
        }

        const QString suffix = channelSuffix(trimmedChannel);
        if (!suffix.isEmpty() && (trimmedChannel.isEmpty() || sameText(channelDeviceName(trimmedChannel), previousDeviceName) || sameText(channelDeviceName(trimmedChannel), selectedDeviceName))) {
            const QString directCandidate = resolveExactChannel(selectedDeviceName + "/" + suffix);
            if (!directCandidate.isEmpty()) return directCandidate;
            for (const std::string& channel : device.aoChannels) {
                const QString discoveredChannel = QString::fromStdString(channel);
                if (sameText(channelSuffix(discoveredChannel), suffix)) return discoveredChannel;
            }
        }

        return QString::fromStdString(device.preferredChannel());
    };
    auto syncDaqDeviceComboToChannel = [&]() {
        const QString deviceName = channelDeviceName(daqChannelEdit->text());
        if (deviceName.isEmpty()) return;
        for (int i = 0; i < daqDeviceCombo->count(); ++i) {
            if (sameText(daqDeviceCombo->itemData(i).toString(), deviceName)) {
                QSignalBlocker blocker(daqDeviceCombo);
                daqDeviceCombo->setCurrentIndex(i);
                break;
            }
        }
    };
    std::function<void(bool)> refreshDaqDeviceOptions = [](bool){};
    refreshDaqDeviceOptions = [&](bool allowChannelRewrite) {
        if (options.noDaq) {
            discoveredDaqDevices.clear();
            daqDiscoveryError.clear();
            QSignalBlocker blocker(daqDeviceCombo);
            daqDeviceCombo->clear();
            daqDeviceCombo->addItem(QStringLiteral("Disabled by launch option"), QString());
            daqDeviceCombo->setCurrentIndex(0);
            daqDeviceCombo->setEnabled(false);
            return;
        }

        const std::vector<DaqDeviceInfo> simulatedDevices = parseSimulatedDaqDevices();
        std::string discoveryErr;
        discoveredDaqDevices = simulatedDevices.empty() ? discoverDaqDevices(discoveryErr) : simulatedDevices;
        daqDiscoveryError = QString::fromStdString(discoveryErr).trimmed();
        if (!simulatedDevices.empty()) {
            daqDiscoveryError = QStringLiteral("Simulated DAQ discovery override active");
        }

        QSettings settings;
        const QString savedDevice = settings.value("settings/daqSelectedDevice").toString().trimmed();
        const QString currentChannelText = daqChannelEdit->text().trimmed();
        const QString currentDevice = channelDeviceName(currentChannelText);

        QSignalBlocker blocker(daqDeviceCombo);
        daqDeviceCombo->clear();
        for (const auto& device : discoveredDaqDevices) {
            daqDeviceCombo->addItem(formatDaqDeviceLabel(device), QString::fromStdString(device.name));
        }

        QString chosenDevice;
        if (discoveredCompatibleDeviceCount() == 1) {
            for (const auto& device : discoveredDaqDevices) {
                if (device.isCompatible()) {
                    chosenDevice = QString::fromStdString(device.name);
                    break;
                }
            }
        } else if (discoveredCompatibleDeviceCount() > 1) {
            if (const DaqDeviceInfo* saved = findDiscoveredDaqDevice(savedDevice); saved && saved->isCompatible()) {
                chosenDevice = savedDevice;
            } else if (const DaqDeviceInfo* current = findDiscoveredDaqDevice(currentDevice); current && current->isCompatible()) {
                chosenDevice = currentDevice;
            } else {
                for (const auto& device : discoveredDaqDevices) {
                    if (device.isCompatible()) {
                        chosenDevice = QString::fromStdString(device.name);
                        break;
                    }
                }
            }
        } else if (!discoveredDaqDevices.empty()) {
            if (findDiscoveredDaqDevice(savedDevice)) {
                chosenDevice = savedDevice;
            } else if (findDiscoveredDaqDevice(currentDevice)) {
                chosenDevice = currentDevice;
            } else {
                chosenDevice = QString::fromStdString(discoveredDaqDevices.front().name);
            }
        }

        if (discoveredDaqDevices.empty()) {
            const QString emptyText = daqDiscoveryError.isEmpty()
                ? QStringLiteral("No NI-DAQmx devices detected")
                : QStringLiteral("DAQ discovery unavailable");
            daqDeviceCombo->addItem(emptyText, QString());
            daqDeviceCombo->setCurrentIndex(0);
            daqDeviceCombo->setEnabled(false);
            if (allowChannelRewrite) daqChannelEdit->clear();
            settings.setValue("settings/daqSelectedDevice", QString());
            settings.sync();
            return;
        }

        int chosenIndex = -1;
        for (int i = 0; i < daqDeviceCombo->count(); ++i) {
            if (sameText(daqDeviceCombo->itemData(i).toString(), chosenDevice)) {
                chosenIndex = i;
                break;
            }
        }
        if (chosenIndex < 0) chosenIndex = 0;
        daqDeviceCombo->setCurrentIndex(chosenIndex);
        daqDeviceCombo->setEnabled(daqDeviceCombo->count() > 0);

        const QString selectedDevice = daqDeviceCombo->currentData().toString().trimmed();
        settings.setValue("settings/daqSelectedDevice", selectedDevice);
        settings.sync();

        if (!allowChannelRewrite) return;

        if (const DaqDeviceInfo* selectedInfo = findDiscoveredDaqDevice(selectedDevice)) {
            const QString nextChannel = chooseChannelForDevice(*selectedInfo, currentChannelText, currentDevice);
            if (nextChannel != currentChannelText) daqChannelEdit->setText(nextChannel);
        } else if (!selectedDevice.isEmpty()) {
            daqChannelEdit->clear();
        }
    };
    auto currentDaqConfig = [&]() {
        DaqConfig cfg;
        cfg.channel = daqChannelEdit->text().trimmed().toStdString();
        cfg.rangeMin = -10.0;
        cfg.rangeMax = 10.0;
        cfg.amplitude = amplitudeSpin->value();
        cfg.frequencyHz = freqSpin->value() * 1000.0;
        cfg.durationMs = durationSpin->value();
        cfg.delayMs = delaySpin->value();
        return cfg;
    };
    auto probeDaqAvailability = [&]() {
        DaqAvailabilityState state;
        if (options.noDaq) {
            state.disabled = true;
            state.statusText = QStringLiteral("DAQ: disabled");
            state.indicatorText = QStringLiteral("Disabled");
            state.indicatorColor = QStringLiteral("#666");
            return state;
        }

        const DaqConfig cfg = currentDaqConfig();
        if (cfg.channel.empty()) {
            state.fault = true;
            state.statusText = QStringLiteral("DAQ: unavailable");
            if (discoveredDaqDevices.empty()) {
                state.faultText = daqDiscoveryError.isEmpty()
                    ? QStringLiteral("No NI-DAQmx devices detected.")
                    : daqDiscoveryError;
            } else if (const DaqDeviceInfo* selectedInfo = findDiscoveredDaqDevice(daqDeviceCombo->currentData().toString()); selectedInfo && !selectedInfo->isCompatible()) {
                state.faultText = QStringLiteral("Selected device %1 does not report analog output channels in NI-DAQmx.")
                    .arg(QString::fromStdString(selectedInfo->name));
            } else {
                state.faultText = QStringLiteral("No DAQ output channel is configured.");
            }
            state.indicatorText = QStringLiteral("Disabled");
            state.indicatorColor = QStringLiteral("#c0392b");
            return state;
        }

        DaqTrigger probeTrigger;
        std::string probeErr;
        if (probeTrigger.init(cfg, probeErr)) {
            state.available = true;
            state.statusText = QStringLiteral("DAQ: available");
            state.indicatorText = QStringLiteral("Connected");
            state.indicatorColor = QStringLiteral("#2ecc71");
            return state;
        }

        state.fault = true;
        state.statusText = QStringLiteral("DAQ: unavailable");
        state.faultText = QString::fromStdString(probeErr);
        state.indicatorText = QStringLiteral("Disconnected");
        state.indicatorColor = QStringLiteral("#c0392b");
        return state;
    };
    auto applyDaqAvailability = [&](const DaqAvailabilityState& state) {
        setLabviewStatus(state.indicatorText, state.indicatorColor);
        daqStatusItem->setText(state.statusText);
        appState.daqAvailable = state.available;
        appState.daqDisabled = state.disabled;
        appState.daqFault = state.fault;
        appState.daqStatusText = state.statusText;
        appState.daqFaultText = state.faultText;
    };
    labviewApplyTimer.setSingleShot(true);
    labviewApplyTimer.setInterval(300);
    detectorTuningApplyTimer.setSingleShot(true);
    detectorTuningApplyTimer.setInterval(250);
    auto updateLabviewOutput = [&](){
        QString channel = daqChannelEdit->text().trimmed();
        if (channel.isEmpty()) {
            labviewOutputLabel->setText("Output: (disabled)");
            return;
        }
        labviewOutputLabel->setText(QString("Output: %1 | amp=%2 V freq=%3 kHz dur=%4 ms delay=%5 ms")
            .arg(channel)
            .arg(amplitudeSpin->value(), 0, 'f', 2)
            .arg(freqSpin->value(), 0, 'f', 3)
            .arg(durationSpin->value(), 0, 'f', 2)
            .arg(delaySpin->value(), 0, 'f', 2));
    };
    refreshDaqDeviceOptions(false);
    updateLabviewOutput();
    setLabviewStatus("Disconnected", "#666");

    QObject::connect(daqDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&]() {
        if (options.noDaq) return;
        const QString selectedDevice = daqDeviceCombo->currentData().toString().trimmed();
        QSettings settings;
        settings.setValue("settings/daqSelectedDevice", selectedDevice);
        settings.sync();
        if (const DaqDeviceInfo* device = findDiscoveredDaqDevice(selectedDevice)) {
            const QString nextChannel = chooseChannelForDevice(*device,
                                                               daqChannelEdit->text().trimmed(),
                                                               channelDeviceName(daqChannelEdit->text()));
            if (nextChannel != daqChannelEdit->text().trimmed()) {
                daqChannelEdit->setText(nextChannel);
            } else {
                updateLabviewOutput();
                scheduleLabviewApply();
            }
        } else {
            daqChannelEdit->clear();
        }
    });
    QObject::connect(daqChannelEdit, &QLineEdit::textChanged, [&](){
        QSettings settings;
        settings.setValue("settings/daqChannel", daqChannelEdit->text().trimmed());
        settings.sync();
        syncDaqDeviceComboToChannel();
        updateLabviewOutput();
        scheduleLabviewApply();
    });
    QObject::connect(amplitudeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [&](){
        updateLabviewOutput();
        scheduleLabviewApply();
    });
    QObject::connect(freqSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [&](){
        updateLabviewOutput();
        scheduleLabviewApply();
    });
    QObject::connect(durationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [&](){
        updateLabviewOutput();
        scheduleLabviewApply();
    });
    QObject::connect(delaySpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [&](){
        updateLabviewOutput();
        scheduleLabviewApply();
    });

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
    QMutex datasetCaptureMutex;
    DatasetCaptureSession datasetCaptureSession;
    std::atomic<bool> datasetCaptureActive(false);
    QString datasetCaptureDir;
    QString datasetCaptureManifestPath;

    imageView->setZoomChanged([=](double zoom){
        zoomStatusLabel->setText(QString("%1%").arg(static_cast<int>(std::lround(zoom * 100.0))));
        scaleStatusLabel->setText(QString("SF: %1 Px").arg(zoom, 0, 'f', 3));
    });
    cameraImageView->setZoomChanged([=](double zoom){
        Q_UNUSED(zoom);
    });

    QThread cameraThread;
    cameraThread.setObjectName("CameraWorkerThread");
    auto* cameraWorker = new CameraWorker();
    bool cameraOpened = false;
    QImage lastFrame;
    FrameMeta lastMeta{};
    PipelineRunner pipeline;
    QMutex pipelineMutex;
    std::atomic<bool> pipelineEnabled(false);
    bool labviewTriggerReady = false;
    std::array<unsigned char, 256> lutTable{};
    QMutex lutMutex;
    std::atomic<int> lutMinValue{0};
    std::atomic<int> lutMaxValue{255};
    std::atomic<int> lutRangeMax{255};
    std::atomic<bool> lutEnabled{false};

    auto rebuildLut = [&](){
        int rangeMax = lutRangeMax.load();
        if (rangeMax <= 0) rangeMax = 255;
        int minVal = std::clamp(lutMinValue.load(), 0, rangeMax);
        int maxVal = std::clamp(lutMaxValue.load(), 0, rangeMax);
        bool enabled = (minVal > 0 || maxVal < rangeMax);
        std::array<unsigned char, 256> temp{};
        if (!enabled || maxVal <= minVal) {
            for (int i = 0; i < 256; ++i) {
                temp[i] = static_cast<unsigned char>(i);
            }
            enabled = false;
        } else {
            double scale = 255.0 / static_cast<double>(rangeMax);
            int min8 = std::clamp(static_cast<int>(std::lround(minVal * scale)), 0, 255);
            int max8 = std::clamp(static_cast<int>(std::lround(maxVal * scale)), 0, 255);
            if (max8 <= min8) {
                for (int i = 0; i < 256; ++i) {
                    temp[i] = static_cast<unsigned char>(i);
                }
                enabled = false;
            } else {
                for (int i = 0; i < 256; ++i) {
                    if (i <= min8) {
                        temp[i] = 0;
                    } else if (i >= max8) {
                        temp[i] = 255;
                    } else {
                        temp[i] = static_cast<unsigned char>(
                            (i - min8) * 255 / (max8 - min8));
                    }
                }
            }
        }
        {
            QMutexLocker lock(&lutMutex);
            lutTable = temp;
        }
        lutEnabled.store(enabled);
    };

    auto applyLutToImage = [&](const QImage& img)->QImage {
        if (img.isNull() || !lutEnabled.load()) return img;
        QImage out = img;
        if (out.format() != QImage::Format_Grayscale8) {
            out = out.convertToFormat(QImage::Format_Grayscale8);
        } else {
            out.detach();
        }
        std::array<unsigned char, 256> tableCopy;
        {
            QMutexLocker lock(&lutMutex);
            tableCopy = lutTable;
        }
        const int w = out.width();
        const int h = out.height();
        for (int y = 0; y < h; ++y) {
            unsigned char* row = out.scanLine(y);
            for (int x = 0; x < w; ++x) {
                row[x] = tableCopy[row[x]];
            }
        }
        return out;
    };

    auto currentCameraBits = [&]() {
        if (bitsCombo->currentData().isValid()) {
            bool ok = false;
            const int bits = bitsCombo->currentData().toInt(&ok);
            if (ok && bits > 0) {
                return bits;
            }
        }
        return bitsCombo->currentText().toInt();
    };
    auto currentCameraPixelType = [&]() {
        const int bits = currentCameraBits();
        return (bits > 8) ? DCAM_PIXELTYPE_MONO16 : DCAM_PIXELTYPE_MONO8;
    };

    auto updateLutRange = [&](int bits){
        int prevRange = lutRangeMax.load();
        int maxVal = 255;
        if (bits >= 1 && bits <= 16) {
            maxVal = (1 << bits) - 1;
        }
        lutRangeMax.store(maxVal);
        lutRangeLabel->setText(QString("Scale: 0 - %1").arg(maxVal));
        {
            QSignalBlocker b1(lutMinSpin);
            QSignalBlocker b2(lutMaxSpin);
            QSignalBlocker b3(lutMinSlider);
            QSignalBlocker b4(lutMaxSlider);
            lutMinSpin->setRange(0, maxVal);
            lutMaxSpin->setRange(0, maxVal);
            lutMinSlider->setRange(0, maxVal);
            lutMaxSlider->setRange(0, maxVal);
        }
        int minVal = std::clamp(lutMinValue.load(), 0, maxVal);
        int maxValCur = std::clamp(lutMaxValue.load(), 0, maxVal);
        if (minVal == 0 && maxValCur == prevRange) {
            maxValCur = maxVal;
        }
        if (maxValCur < minVal) maxValCur = minVal;
        {
            QSignalBlocker b1(lutMinSpin);
            QSignalBlocker b2(lutMaxSpin);
            QSignalBlocker b3(lutMinSlider);
            QSignalBlocker b4(lutMaxSlider);
            lutMinSpin->setValue(minVal);
            lutMaxSpin->setValue(maxValCur);
            lutMinSlider->setValue(minVal);
            lutMaxSlider->setValue(maxValCur);
        }
        lutMinValue.store(minVal);
        lutMaxValue.store(maxValCur);
        rebuildLut();
    };

    auto setLutMin = [&](int v){
        int rangeMax = lutRangeMax.load();
        v = std::clamp(v, 0, rangeMax);
        int maxVal = lutMaxValue.load();
        if (v > maxVal) {
            maxVal = v;
            QSignalBlocker b1(lutMaxSpin);
            QSignalBlocker b2(lutMaxSlider);
            lutMaxSpin->setValue(maxVal);
            lutMaxSlider->setValue(maxVal);
            lutMaxValue.store(maxVal);
        }
        {
            QSignalBlocker b1(lutMinSpin);
            QSignalBlocker b2(lutMinSlider);
            lutMinSpin->setValue(v);
            lutMinSlider->setValue(v);
        }
        lutMinValue.store(v);
        rebuildLut();
    };

    auto setLutMax = [&](int v){
        int rangeMax = lutRangeMax.load();
        v = std::clamp(v, 0, rangeMax);
        int minVal = lutMinValue.load();
        if (v < minVal) {
            minVal = v;
            QSignalBlocker b1(lutMinSpin);
            QSignalBlocker b2(lutMinSlider);
            lutMinSpin->setValue(minVal);
            lutMinSlider->setValue(minVal);
            lutMinValue.store(minVal);
        }
        {
            QSignalBlocker b1(lutMaxSpin);
            QSignalBlocker b2(lutMaxSlider);
            lutMaxSpin->setValue(v);
            lutMaxSlider->setValue(v);
        }
        lutMaxValue.store(v);
        rebuildLut();
    };

    updateLutRange(currentCameraBits());
    restoreRuntimeSettings();
    auto repairRuntimeModelPaths = [&]() {
        auto rawPathExists = [&](const QString& path) {
            const QString trimmed = path.trimmed();
            if (trimmed.isEmpty()) return false;
            QFileInfo info(trimmed);
            if (info.isAbsolute()) return info.exists();
            return QFileInfo(QDir(appDir).absoluteFilePath(trimmed)).exists();
        };
        if (rawPathExists(onnxEdit->text()) && rawPathExists(metaEdit->text())) {
            return;
        }
        const QString registryOnnx = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString registryMeta = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        if (!registryOnnx.isEmpty()) onnxEdit->setText(registryOnnx);
        if (!registryMeta.isEmpty()) metaEdit->setText(registryMeta);
        logMessage(QString("Runtime model paths repaired from selected registry entry: onnx=%1 meta=%2")
            .arg(onnxEdit->text(), metaEdit->text()));
    };
    repairRuntimeModelPaths();
    populateTargetClassSelector();
    updateLutRange(currentCameraBits());

    QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](int){
        bool isCustom = presetCombo->currentData().toSize().width() < 0;
        customWidthSpin->setEnabled(isCustom);
        customHeightSpin->setEnabled(isCustom);
    });
    // Initialize state
    bool restoredCustom = presetCombo->currentData().toSize().width() < 0;
    customWidthSpin->setEnabled(restoredCustom);
    customHeightSpin->setEnabled(restoredCustom);

    auto applySettings = [&](){
        QSize preset = presetCombo->currentData().toSize();
        bool isCustom = preset.width() < 0 || preset.height() < 0;
        int bin = binCombo->currentText().toInt();
        int bits = currentCameraBits();
        int pixel = (bits > 8) ? DCAM_PIXELTYPE_MONO16 : DCAM_PIXELTYPE_MONO8;
        double exp_ms = exposureSpin->value();
        double exp_s = exp_ms / 1000.0;
        int readout = readoutCombo->currentData().toInt();
        ApplySettings s;
        s.width = isCustom ? customWidthSpin->value() : preset.width();
        s.height = isCustom ? customHeightSpin->value() : preset.height();
        s.binning = bin;
        s.binningIndependent = false;
        s.binH = bin;
        s.binV = bin;
        s.bits = bits;
        s.pixelType = pixel;
        s.exposure_s = exp_s;
        s.readoutSpeed = readout;
        s.bundleEnabled = false;
        s.bundleCount = 0;
        logLine(QString("Apply: preset=%1x%2 bin=%3 binH=%4 binV=%5 bits=%6 pixType=%7 exp_ms=%8 readout=%9")
            .arg(s.width).arg(s.height).arg(s.binning).arg(s.binH).arg(s.binV)
            .arg(s.bits).arg(s.pixelType).arg(exp_ms,0,'f',3).arg(readout));
        statusLabel->setText("Applying camera settings...");
        QMetaObject::invokeMethod(cameraWorker, [cameraWorker, s, displayEvery = displayEverySpin->value()]() {
            cameraWorker->setDisplayEvery(displayEvery);
            cameraWorker->applySettings(s);
        }, Qt::QueuedConnection);
        if (pipeline.isReady()) {
            QMutexLocker lock(&pipelineMutex);
            pipeline.reset();
            pipelineStatusLabel->setText("Pipeline: warming (settings changed)");
        }
    };

    QTimer applyTimer;
    applyTimer.setSingleShot(true);
    applyTimer.setInterval(250);
    bool autoApplyCamera = false;
    QObject::connect(&applyTimer, &QTimer::timeout, [&](){
        if (viewerOnly) return;
        applySettings();
    });
    auto scheduleApplySettings = [&](){
        if (!autoApplyCamera || viewerOnly) return;
        if (!cameraOpened) return;
        applyTimer.start();
    };

    auto setViewerOnly = [&](){
        viewerOnly = true;
        appState.cameraStreaming = false;
        statusLabel->setText("Viewer-only mode (camera init failed).");
        cameraStatusItem->setText("Camera: unavailable");
        runStatusItem->setText("Run: viewer-only");
        window.statusBar()->showMessage("Viewer-only mode");
        startBtn->setEnabled(false);
        stopBtn->setEnabled(false);
        reconnectBtn->setEnabled(false);
        applyBtn->setEnabled(false);
        operationalTabs->setEnabled(false);
    };

    auto setHardwareFreeMode = [&](){
        appState.testMode = true;
        appState.cameraStreaming = false;
        appState.daqAvailable = false;
        appState.daqDisabled = options.noDaq;
        appState.daqFault = !options.noDaq && !kDaqBuildEnabled;
        appState.daqStatusText = initialDaqStatusText;
        statusLabel->setText(options.mockCamera ? "Test mode: mock camera active." : "Test mode: camera startup skipped.");
        cameraStatusItem->setText(options.mockCamera ? "Camera: mock" : "Camera: unavailable");
        cameraOpened = false;
        modelStatusItem->setText("Model: not loaded");
        daqStatusItem->setText(initialDaqStatusText);
        runStatusItem->setText("Run: idle");
        window.statusBar()->showMessage("Hardware-free test mode");
        logLine("Hardware-free GUI test mode active; startup camera prompts suppressed.");
    };

    QObject::connect(cameraWorker, &CameraWorker::exposureLimitsReady, &window,
                     [exposureSpin](double minimumMs, double maximumMs, double currentMs) {
        exposureSpin->setMinimum(minimumMs);
        exposureSpin->setMaximum(maximumMs);
        exposureSpin->setValue(currentMs);
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::formatOptionsReady, &window,
                     [&](const QVariantMap& options) {
        const QString summary = desktop_app::workspace::refreshCameraFormatOptions(presetCombo,
                                                                                   bitsCombo,
                                                                                   readoutCombo,
                                                                                   customWidthSpin,
                                                                                   customHeightSpin,
                                                                                   exposureSpin,
                                                                                   options);
        updateLutRange(currentCameraBits());
        logLine(summary);
    },
                     Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::readbackReady, &window,
                     [logLine](const QString& text) {
        logLine(text);
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::initCompleted, &window,
                     [&](const QString& err) {
        if (!err.isEmpty()) {
            cameraOpened = false;
            statusLabel->setText("Init error: " + err);
            cameraStatusItem->setText("Camera: error");
            window.statusBar()->showMessage("Camera initialization failed");
            auto choice = QMessageBox::question(
                &window,
                "Init failed",
                "Camera init failed:\n" + err + "\n\nLaunch viewer-only mode?",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (choice == QMessageBox::Yes) {
                logLine("Init failed; switching to viewer-only mode.");
                setViewerOnly();
                return;
            }
            QMetaObject::invokeMethod(&app, "quit", Qt::QueuedConnection);
            return;
        }
        cameraOpened = true;
        statusLabel->setText("Initialized.");
        cameraStatusItem->setText("Camera: connected");
        window.statusBar()->showMessage("Camera initialized");
        exposureSpin->setValue(10.0);
        autoApplyCamera = true;
        logLine("Init success");
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::reconnectCompleted, &window,
                     [&](const QString& err) {
        if (!err.isEmpty()) {
            cameraOpened = false;
            statusLabel->setText("Reconnect error: " + err);
            cameraStatusItem->setText("Camera: error");
            window.statusBar()->showMessage("Camera reconnect failed");
            return;
        }
        cameraOpened = true;
        statusLabel->setText("Reconnected.");
        cameraStatusItem->setText("Camera: connected");
        window.statusBar()->showMessage("Camera reconnected");
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::startCompleted, &window,
                     [&](const QString& err) {
        if (!err.isEmpty()) {
            appState.cameraStreaming = false;
            statusLabel->setText("Start error: " + err);
            cameraStatusItem->setText("Camera: error");
            window.statusBar()->showMessage("Capture start failed");
            return;
        }
        appState.cameraStreaming = true;
        statusLabel->setText("Capture started.");
        cameraStatusItem->setText("Camera: acquiring");
        runStatusItem->setText("Run: capture");
        window.statusBar()->showMessage("Capture started");
        if (pipeline.isReady()) {
            QMutexLocker lock(&pipelineMutex);
            pipeline.reset();
            pipelineStatusLabel->setText("Pipeline: warming (capture start)");
        }
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::stopCompleted, &window,
                     [&]() {
        appState.cameraStreaming = false;
        statusLabel->setText("Capture stopped.");
        cameraStatusItem->setText(cameraOpened ? "Camera: connected" : "Camera: unavailable");
        if (!pipelineEnabled.load()) {
            runStatusItem->setText("Run: idle");
        }
        window.statusBar()->showMessage("Capture stopped");
        if (pipelineEnabled.load()) {
            pipelineStatusLabel->setText("Pipeline: paused");
        }
    }, Qt::QueuedConnection);

    QObject::connect(cameraWorker, &CameraWorker::applyCompleted, &window,
                     [&](const QString& err) {
        if (!err.isEmpty()) {
            if (err.startsWith("WARN:")) {
                appState.cameraStreaming = true;
                statusLabel->setText("Applied with warnings: " + err.mid(5));
                cameraStatusItem->setText("Camera: acquiring");
            } else {
                appState.cameraStreaming = false;
                statusLabel->setText("Apply error: " + err);
            }
        } else {
            appState.cameraStreaming = true;
            statusLabel->setText("Applied. Streaming");
            cameraStatusItem->setText("Camera: acquiring");
        }
    }, Qt::QueuedConnection);

    auto doInit = [&]()->bool{
        if (hardwareFreeMode || options.noStartupPrompts) {
            setHardwareFreeMode();
            return true;
        }
        statusLabel->setText("Initializing camera...");
        cameraStatusItem->setText("Camera: startup pending");
        QMetaObject::invokeMethod(cameraWorker, [cameraWorker, bits = currentCameraBits(), pixel = currentCameraPixelType()]() {
            cameraWorker->initAndOpen(bits, pixel);
        }, Qt::QueuedConnection);
        return true;
    };

    QObject::connect(reconnectBtn, &QPushButton::clicked, [&](){
        if (hardwareFreeMode) {
            statusLabel->setText("Mock camera reconnected.");
            cameraStatusItem->setText("Camera: mock");
            window.statusBar()->showMessage("Mock camera reconnected");
            logLine("Mock camera reconnect requested.");
            return;
        }
        statusLabel->setText("Reconnecting camera...");
        QMetaObject::invokeMethod(cameraWorker, [cameraWorker, bits = currentCameraBits(), pixel = currentCameraPixelType()]() {
            cameraWorker->reconnect(bits, pixel);
        }, Qt::QueuedConnection);
    });

    QObject::connect(startBtn, &QPushButton::clicked, [&](){
        if (viewerOnly) return;
        if (hardwareFreeMode) {
            appState.cameraStreaming = true;
            statusLabel->setText("Mock preview started.");
            cameraStatusItem->setText("Camera: mock acquiring");
            runStatusItem->setText("Run: capture");
            window.statusBar()->showMessage("Mock preview started");
            logLine("Mock preview started.");
            return;
        }
        statusLabel->setText("Starting capture...");
        QMetaObject::invokeMethod(cameraWorker, [cameraWorker, bits = currentCameraBits(), pixel = currentCameraPixelType(), displayEvery = displayEverySpin->value()]() {
            cameraWorker->setDisplayEvery(displayEvery);
            cameraWorker->startCapture(bits, pixel);
        }, Qt::QueuedConnection);
    });

    QObject::connect(stopBtn, &QPushButton::clicked, [&](){
        if (viewerOnly) return;
        if (hardwareFreeMode) {
            appState.cameraStreaming = false;
            statusLabel->setText("Mock preview stopped.");
            cameraStatusItem->setText("Camera: mock");
            if (!pipelineEnabled.load()) {
                runStatusItem->setText("Run: idle");
            }
            window.statusBar()->showMessage("Mock preview stopped");
            logLine("Mock preview stopped.");
            return;
        }
        statusLabel->setText("Stopping capture...");
        QMetaObject::invokeMethod(cameraWorker, [cameraWorker]() {
            cameraWorker->stopCapture();
        }, Qt::QueuedConnection);
    });

    QObject::connect(applyBtn, &QPushButton::clicked, [&](){
        if (viewerOnly) return;
        if (hardwareFreeMode) {
            statusLabel->setText("Mock camera settings applied.");
            cameraStatusItem->setText("Camera: mock");
            window.statusBar()->showMessage("Mock camera settings applied");
            logLine("Mock camera settings applied.");
            return;
        }
        applySettings();
    });

    QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
        scheduleApplySettings();
    });
    QObject::connect(customWidthSpin, qOverload<int>(&QSpinBox::valueChanged), [&](){
        scheduleApplySettings();
    });
    QObject::connect(customHeightSpin, qOverload<int>(&QSpinBox::valueChanged), [&](){
        scheduleApplySettings();
    });
    QObject::connect(binCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
        scheduleApplySettings();
    });
    QObject::connect(bitsCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
        updateLutRange(currentCameraBits());
        scheduleApplySettings();
    });
    QObject::connect(lutMinSpin, qOverload<int>(&QSpinBox::valueChanged), [&](int v){
        setLutMin(v);
    });
    QObject::connect(lutMaxSpin, qOverload<int>(&QSpinBox::valueChanged), [&](int v){
        setLutMax(v);
    });
    QObject::connect(lutMinSlider, &QSlider::valueChanged, [&](int v){
        setLutMin(v);
    });
    QObject::connect(lutMaxSlider, &QSlider::valueChanged, [&](int v){
        setLutMax(v);
    });
    QObject::connect(exposureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), [&](){
        scheduleApplySettings();
    });
    QObject::connect(readoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
        scheduleApplySettings();
    });

    QPointer<ViewerWindow> viewerWindow;
    QPointer<StatsFigureWindow> statsFigureWindow;
    QObject::connect(viewerBtn, &QPushButton::clicked, [&](){
        if (viewerWindow) {
            viewerWindow->raise();
            viewerWindow->activateWindow();
            return;
        }
        viewerWindow = new ViewerWindow(nullptr);
        viewerWindow->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(viewerWindow, &QObject::destroyed, [&](){ viewerWindow = nullptr; });
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

    QObject::connect(saveBrowseBtn, &QPushButton::clicked, [&](){
        QString dir = QFileDialog::getExistingDirectory(&window, "Select save directory", savePathEdit->text());
        if (!dir.isEmpty()) savePathEdit->setText(dir);
    });
    QObject::connect(saveOpenBtn, &QPushButton::clicked, [&](){
        QString dir = savePathEdit->text();
        if (dir.isEmpty()) dir = QCoreApplication::applicationDirPath();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    QObject::connect(onnxBrowseBtn, &QPushButton::clicked, [&](){
        QString file = QFileDialog::getOpenFileName(&window, "Select ONNX model", onnxEdit->text(),
                                                    "ONNX Model (*.onnx)");
        if (!file.isEmpty()) {
            onnxEdit->setText(file);
            saveRuntimeSettings();
        }
    });
    QObject::connect(metaBrowseBtn, &QPushButton::clicked, [&](){
        QString file = QFileDialog::getOpenFileName(&window, "Select metadata JSON", metaEdit->text(),
                                                    "JSON (*.json)");
        if (!file.isEmpty()) {
            metaEdit->setText(file);
            populateTargetClassSelector();
            saveRuntimeSettings();
        }
    });
    QObject::connect(outputBrowseBtn, &QPushButton::clicked, [&](){
        QString dir = QFileDialog::getExistingDirectory(&window, "Select output directory", outputEdit->text());
        if (!dir.isEmpty()) {
            outputEdit->setText(dir);
            saveRuntimeSettings();
        }
    });
    QObject::connect(liveModelCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
        applyLiveModelSelection();
    });
    QObject::connect(openLiveModelManagerBtn, &QPushButton::clicked, [&](){
        operationalTabs->setCurrentWidget(modelManagerWidget);
    });
    QObject::connect(refreshLiveModelsBtn, &QPushButton::clicked, [&](){
        applyLiveModelSelection();
    });

    auto connectRuntimeSettingsPersistence = [&]() {
        QObject::connect(onnxEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(metaEdit, &QLineEdit::editingFinished, [&](){
            populateTargetClassSelector();
            saveRuntimeSettings();
        });
        QObject::connect(outputEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(targetClassCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&](){
            pendingTargetClassId = selectedTargetClassId();
            appState.targetClassId = pendingTargetClassId;
            saveRuntimeSettings();
        });
        QObject::connect(savePathEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(saveCropCheck, &QCheckBox::toggled, saveRuntimeSettings);
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
        QObject::connect(minAreaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(minAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(maxAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(minBboxSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(marginSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(diffThreshSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(blurRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(morphRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(scaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(gapFireSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, saveRuntimeSettings);
    };
    connectRuntimeSettingsPersistence();
    saveRuntimeSettings();

    QObject::connect(pipelineEnableCheck, &QCheckBox::toggled, [&](bool enabled){
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
            setLabviewStatus("Disabled", "#666");
            if (!sequenceRunning.load() && !sequenceStarting.load()) {
                stopLiveLogging();
            }
        } else if (daqChannelEdit->text().trimmed().isEmpty()) {
            refreshDaqDeviceOptions(true);
            applyDaqAvailability(probeDaqAvailability());
        } else {
            refreshDaqDeviceOptions(true);
            applyDaqAvailability(probeDaqAvailability());
        }
        if (enabled && !sequenceRunning.load() && !sequenceStarting.load()) {
            if (ready && !liveLogging.load()) {
                startLiveLogging();
            }
        }
    });

    bool daqStartupStateLogged = false;
    auto logDaqStartupState = [&](const QString& stateText) {
        if (daqStartupStateLogged) return;
        daqStartupStateLogged = true;
        logMessage("DAQ startup state: " + stateText);
    };

    auto loadPipeline = [&](bool enableAfter){
        logMessage("Pipeline init requested");
        refreshDaqDeviceOptions(true);
        if (liveModelCombo->currentData(kLiveModelModeRole).toString() == "blocked") {
            pipelineStatusLabel->setText("Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            logMessage("Pipeline init blocked by live model selection gate: " + liveModelCombo->currentText());
            return;
        }
        PipelineConfig cfg;
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        cfg.onnxPath = onnxResolved.toStdString();
        cfg.metadataPath = metaResolved.toStdString();
        appState.targetClassId = selectedTargetClassId();
        cfg.targetClassId = appState.targetClassId.toStdString();
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

        logMessage(QString("Pipeline init paths: onnx=%1 meta=%2").arg(onnxEdit->text(), metaEdit->text()));
        logMessage(QString("Pipeline init resolved paths: onnx=%1 meta=%2").arg(onnxResolved, metaResolved));
        logMessage(QString("DAQ config: channel=%1 range=[-10,10] amp=%2V freq=%3Hz duration=%4ms delay=%5ms")
            .arg(daqChannelEdit->text().trimmed())
            .arg(amplitudeSpin->value(), 0, 'f', 3)
            .arg(freqSpin->value() * 1000.0, 0, 'f', 1)
            .arg(durationSpin->value(), 0, 'f', 3)
            .arg(delaySpin->value(), 0, 'f', 3));
        if (!discoveredDaqDevices.empty()) {
            logMessage(QString("DAQ discovery: %1").arg(describeDiscoveredDaqDevices()));
        } else if (!daqDiscoveryError.isEmpty()) {
            logMessage(QString("DAQ discovery: %1").arg(daqDiscoveryError));
        } else {
            logMessage("DAQ discovery: no NI-DAQmx devices detected");
        }

        std::string err;
        QString resolvedTargetText;
        {
            QMutexLocker locker(&pipelineMutex);
            try {
                if (!pipeline.init(cfg, err)) {
                    pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(QString::fromStdString(err)));
                    modelStatusItem->setText("Model: error");
                    window.statusBar()->showMessage("Pipeline initialization failed");
                    pipelineEnabled.store(false);
                    pipelineEnableCheck->setChecked(false);
                    pipelineStartBtn->setEnabled(false);
                    labviewTriggerReady = false;
                    applyDaqAvailability(probeDaqAvailability());
                    updateForceTriggerState();
                    logMessage(QString("Pipeline init failed: %1").arg(QString::fromStdString(err)));
                    return;
                }
                pipeline.reset();
                labviewTriggerReady = pipeline.isTriggerReady();
                resolvedTargetText = QString::fromStdString(pipeline.targetDisplayText());
                cfg.targetClassId = pipeline.targetClassId();
            } catch (const std::exception& e) {
                const QString exceptionText = QString::fromLocal8Bit(e.what());
                pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(exceptionText));
                modelStatusItem->setText("Model: error");
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QString("DAQ startup exception: %1").arg(exceptionText);
                window.statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                applyDaqAvailability(probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage(QString("Pipeline init threw exception: %1").arg(exceptionText));
                return;
            } catch (...) {
                pipelineStatusLabel->setText("Pipeline error: unknown startup exception");
                modelStatusItem->setText("Model: error");
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QStringLiteral("DAQ startup exception: unknown");
                window.statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                applyDaqAvailability(probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage("Pipeline init threw unknown exception");
                return;
            }
        }

        if (!err.empty()) {
            pipelineStatusLabel->setText(QString("Pipeline ready (DAQ off), target %1: %2").arg(resolvedTargetText, QString::fromStdString(err)));
            modelStatusItem->setText("Model: loaded");
            daqStatusItem->setText("DAQ: unavailable");
            appState.daqAvailable = false;
            appState.daqFault = true;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText = QString::fromStdString(err);
            logDaqStartupState(daqStatusItem->text() + ": " + QString::fromStdString(err));
            window.statusBar()->showMessage("Pipeline ready with DAQ warning");
            logMessage(QString("Pipeline init warning: %1").arg(QString::fromStdString(err)));
        } else {
            pipelineStatusLabel->setText(QString("Pipeline ready, target %1").arg(resolvedTargetText));
            modelStatusItem->setText("Model: loaded");
            daqStatusItem->setText("DAQ: available");
            appState.daqAvailable = true;
            appState.daqFault = false;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText.clear();
            window.statusBar()->showMessage("Pipeline ready");
            logMessage("Pipeline init success");
        }
        setSelectedTargetClassId(QString::fromStdString(cfg.targetClassId));
        appState.targetClassId = QString::fromStdString(cfg.targetClassId);
        saveRuntimeSettings();
        pipelineStartBtn->setEnabled(!enableAfter && !sequenceRunning.load());
        pipelineEnabled.store(enableAfter);
        if (enableAfter) {
            pipelineEnableCheck->setChecked(true);
        }
        if (enableAfter && !sequenceRunning.load() && !sequenceStarting.load() && !liveLogging.load()) {
            startLiveLogging();
        }

        if (cfg.daq.channel.empty()) {
            applyDaqAvailability(probeDaqAvailability());
            logDaqStartupState(daqStatusItem->text());
        } else {
            applyDaqAvailability(probeDaqAvailability());
            if (!appState.daqAvailable) {
                logDaqStartupState(daqStatusItem->text() + (appState.daqFaultText.isEmpty() ? QString() : QStringLiteral(": ") + appState.daqFaultText));
            }
        }
        appState.daqWaveformValid = !cfg.daq.channel.empty() && cfg.daq.amplitude > 0.0 && cfg.daq.frequencyHz > 0.0 && cfg.daq.durationMs > 0.0;
        appState.daqStatusText = daqStatusItem->text();
        updateLabviewOutput();
        updateForceTriggerState();
    };

    QObject::connect(&labviewApplyTimer, &QTimer::timeout, [&](){
        if (viewerOnly) return;
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter);
        updateForceTriggerState();
    });
    QObject::connect(&detectorTuningApplyTimer, &QTimer::timeout, [&](){
        if (viewerOnly) return;
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter);
        updateForceTriggerState();
    });
    scheduleLabviewApply = [&](){
        if (!autoApplyLabview) return;
        labviewApplyTimer.start();
    };
    scheduleDetectorApply = [&](){
        detectorTuningApplyTimer.start();
    };

    QObject::connect(loadPipelineBtn, &QPushButton::clicked, [&](){
        loadPipeline(false);
    });

    QObject::connect(pipelineStartBtn, &QPushButton::clicked, [&](){
        if (sequenceRunning.load()) return;
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            loadPipeline(false);
            {
                QMutexLocker lock(&pipelineMutex);
                ready = pipeline.isReady();
            }
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: load a valid pipeline first.");
            runStatusItem->setText("Run: idle");
            window.statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked because pipeline is not ready.");
            updateOpenRunAvailability();
            return;
        }
        QString runDir = buildRunOutputDir("live");
        if (runDir.isEmpty()) {
            statusLabel->setText("Start Sorting blocked: failed to create run folder.");
            window.statusBar()->showMessage("Start Sorting blocked: no run folder");
            logMessage("Start Sorting blocked because run folder creation failed.");
            updateOpenRunAvailability();
            return;
        }
        outputEdit->setText(runDir);
        writeRuntimeSettingsSnapshot(runDir, "live");
        loadPipeline(true);
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: pipeline failed after run setup.");
            runStatusItem->setText("Run: idle");
            window.statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked after run setup because pipeline is not ready.");
            updateOpenRunAvailability();
            return;
        }
        currentRunDir = runDir;
        updateOpenRunAvailability();
        statusLabel->setText("Pipeline started.");
        updateForceTriggerState();
        runStatusItem->setText("Run: Live View");
        window.statusBar()->showMessage("Live View started");
    });

    QObject::connect(pipelineStopBtn, &QPushButton::clicked, [&](){
        if (sequenceRunning.load()) return;
        pipelineEnableCheck->setChecked(false);
        updateForceTriggerState();
        statusLabel->setText("Pipeline stopped.");
        runStatusItem->setText("Run: idle");
        updateOpenRunAvailability();
        window.statusBar()->showMessage("Live sorting stopped");
    });

    auto stopDatasetCapture = [&](const QString& reason, bool openReview) {
        QString reviewPath;
        std::string err;
        {
            QMutexLocker lock(&datasetCaptureMutex);
            if (!datasetCaptureActive.load()) return;
            datasetCaptureSession.setStopReason(reason.toStdString());
            if (!datasetCaptureSession.finalize(err)) {
                logMessage(QString("Dataset Builder capture finalize failed: %1").arg(QString::fromStdString(err)));
            }
            reviewPath = datasetCaptureManifestPath;
            datasetCaptureActive.store(false);
        }
        datasetStartCaptureBtn->setEnabled(true);
        datasetStopCaptureBtn->setEnabled(false);
        datasetCaptureStatusLabel->setText(QString("Dataset Builder capture stopped: %1\nManifest: %2").arg(reason, reviewPath));
        statusLabel->setText("Dataset Builder capture stopped. Review required before trainer handoff.");
        trainerDatasetEdit->setText(datasetCaptureDir);
        if (openReview && QFileInfo::exists(reviewPath)) {
            openDatasetLabelerPath(reviewPath);
        }
    };

    auto startDatasetCapture = [&]() {
        if (datasetCaptureActive.load()) return;
        DatasetCollectionMode mode = DatasetCollectionMode::Mixed;
        std::string modeText = datasetCaptureModeCombo->currentText().toStdString();
        std::string err;
        if (!DatasetCaptureSession::parseCollectionMode(modeText, mode)) {
            datasetCaptureStatusLabel->setText("Invalid Dataset Builder collection mode.");
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
                datasetCaptureStatusLabel->setText("Dataset Builder capture failed: " + QString::fromStdString(err));
                return;
            }
            datasetCaptureDir = sessionDir;
            datasetCaptureManifestPath = QDir(sessionDir).filePath("metadata/dataset_manifest.json");
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
            loadPipeline(true);
        }
        datasetStartCaptureBtn->setEnabled(false);
        datasetStopCaptureBtn->setEnabled(true);
        datasetCaptureStatusLabel->setText(QString("Dataset Builder capture active: 0 / %1 crops\n%2")
            .arg(datasetBatchTargetSpin->value())
            .arg(sessionDir));
        trainerDatasetEdit->setText(sessionDir);
        statusLabel->setText("Dataset Builder capture active. Crops remain unreviewed until manual review.");
        logMessage("Dataset Builder live capture started: " + sessionDir);
    };

    QObject::connect(datasetStartCaptureBtn, &QPushButton::clicked, startDatasetCapture);
    QObject::connect(datasetCaptureFromCameraAction, &QAction::triggered, startDatasetCapture);
    QObject::connect(datasetStopCaptureBtn, &QPushButton::clicked, [&]() {
        stopDatasetCapture("cancelled", true);
    });

    QObject::connect(labviewReconnectBtn, &QPushButton::clicked, [&](){
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter);
    });

    QObject::connect(labviewTestBtn, &QPushButton::clicked, [&](){
        if (options.noDaq) {
            statusLabel->setText("DAQ disabled for test mode.");
            daqStatusItem->setText("DAQ: disabled");
            appState.daqAvailable = false;
            appState.daqDisabled = true;
            appState.daqFault = false;
            appState.triggerArmed = false;
            appState.daqStatusText = daqStatusItem->text();
            setLabviewStatus("Disabled", "#666");
            updateForceTriggerState();
            window.statusBar()->showMessage("DAQ disabled");
            logMessage("DAQ force trigger skipped because --no-daq is active.");
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

        bool canUsePipeline = false;
        {
            QMutexLocker lock(&pipelineMutex);
            canUsePipeline = pipeline.isTriggerReady();
        }

        statusLabel->setText("DAQ trigger queued...");
        QPointer<QWidget> windowPtr(&window);
        QPointer<QLabel> statusLabelPtr(statusLabel);
        backgroundTasks.launch("daq-force-trigger", [&, cfg, canUsePipeline, windowPtr, statusLabelPtr](const BackgroundTaskRegistry::StopFlag& stop){
            std::string trigErr;
            bool ok = false;
            bool usedPipeline = false;
            if (canUsePipeline && !stop->load()) {
                QMutexLocker lock(&pipelineMutex);
                if (!stop->load()) {
                    ok = pipeline.fireTrigger(trigErr);
                    usedPipeline = true;
                }
            }
            if (!usedPipeline && !stop->load()) {
                DaqTrigger manualTrigger;
                if (!manualTrigger.init(cfg, trigErr)) {
                    ok = false;
                } else {
                    ok = manualTrigger.fire(trigErr);
                }
            }
            if (stop->load() || windowPtr.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(windowPtr, [&, ok, trigErr, statusLabelPtr](){
                if (statusLabelPtr.isNull()) return;
                if (ok) {
                    statusLabelPtr->setText("DAQ trigger sent.");
                    appState.daqAvailable = true;
                    appState.daqDisabled = false;
                    appState.daqFault = false;
                    appState.daqStatusText = "DAQ: available";
                    setLabviewStatus("Connected", "#2ecc71");
                    updateForceTriggerState();
                } else {
                    statusLabelPtr->setText("DAQ trigger failed: " + QString::fromStdString(trigErr));
                    appState.daqAvailable = false;
                    appState.daqFault = true;
                    appState.daqStatusText = "DAQ: unavailable";
                    appState.daqFaultText = QString::fromStdString(trigErr);
                    setLabviewStatus("Disconnected", "#c0392b");
                    updateForceTriggerState();
                    logMessage(QString("DAQ force trigger failed: %1").arg(QString::fromStdString(trigErr)));
                }
            }, Qt::QueuedConnection);
        });
    });

    QObject::connect(captureBtn, &QPushButton::clicked, [&](){
        if (lastFrame.isNull()) {
            statusLabel->setText("No frame to capture");
            return;
        }
        QString baseDir = savePathEdit->text();
        if (baseDir.isEmpty()) baseDir = QCoreApplication::applicationDirPath();
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

    auto startSaving = [&](){
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

    auto stopSaving = [&](){
        if (!recording.load()) return;
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
        if (baseDir.isEmpty()) baseDir = QCoreApplication::applicationDirPath();
        QDir dir(baseDir);
        dir.mkpath(".");
        QString sub = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString outDir = dir.filePath(sub);
        dir.mkpath(outDir);

        saving = true;
        statusLabel->setText("Saving to disk...");
        logLine(QString("Saving %1 frames to %2").arg(frames->size()).arg(outDir));
        if (!savingDialog) {
            savingDialog = new QDialog(&window);
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

        FrameMeta metaCopy = lastMeta;
        double expMsCopy = exposureSpin->value();
        QString recordStartStr = recordStartTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
        QPointer<QLabel> statusLabelPtr(statusLabel);
        QPointer<QDialog> savingDialogPtr(savingDialog);
        QPointer<QProgressBar> savingProgressPtr(savingProgress);

        backgroundTasks.launch("capture-save-export",
            [frames, outDir, logLine, statusLabelPtr, savingDialogPtr, savingProgressPtr, totalFrames, metaCopy, expMsCopy, recordStartStr, &saving]
            (const BackgroundTaskRegistry::StopFlag& stop) {
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
                    QMetaObject::invokeMethod(savingProgressPtr, [savingProgressPtr, v](){
                        if (!savingProgressPtr.isNull()) {
                            savingProgressPtr->setValue(v);
                        }
                    }, Qt::QueuedConnection);
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
            logLine(canceled
                ? QString("Save canceled after partial export to %1").arg(outDir)
                : QString("Saved %1 frames to %2").arg(frames->size()).arg(outDir));
            if (!statusLabelPtr.isNull()) {
                QMetaObject::invokeMethod(statusLabelPtr, [statusLabelPtr, canceled](){
                    if (!statusLabelPtr.isNull()) {
                        statusLabelPtr->setText(canceled ? "Save canceled" : "Save complete");
                    }
                }, Qt::QueuedConnection);
            }
            if (!savingDialogPtr.isNull()) {
                QMetaObject::invokeMethod(savingDialogPtr, [savingDialogPtr](){
                    if (!savingDialogPtr.isNull()) {
                        savingDialogPtr->hide();
                    }
                }, Qt::QueuedConnection);
            }
            saving = false;
        });
    };

    QObject::connect(saveStartBtn, &QPushButton::clicked, startSaving);
    QObject::connect(saveStopBtn, &QPushButton::clicked, stopSaving);

    QObject::connect(&saveInfoTimer, &QTimer::timeout, [&](){
        if (!recording.load()) return;
        double elapsed = recordTimer.isValid() ? recordTimer.elapsed() / 1000.0 : 0.0;
        saveInfoLabel->setText(QString("Elapsed: %1 s\nFrames: %2")
            .arg(elapsed,0,'f',1).arg(recordedFrames.load()));
    });

    auto updatePipelineStatus = [&](const PipelineEvent& evt, int bgRemaining, bool pipelineReady){
        QMetaObject::invokeMethod(pipelineStatusLabel, [pipelineStatusLabel, &pipelineEnabled, evt, bgRemaining, pipelineReady](){
            if (!pipelineEnabled.load()) {
                pipelineStatusLabel->setText("Pipeline: paused");
                return;
            }
            if (!pipelineReady) {
                pipelineStatusLabel->setText("Pipeline: not loaded");
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
        }, Qt::QueuedConnection);
    };

    auto buildClassText = [&](const QMap<QString, int>& counts)->QString {
        if (counts.isEmpty()) return "Classes:\n(none)";
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
            if (used.contains(it.key())) continue;
            text += QString("\n%1: %2").arg(it.key()).arg(it.value());
        }
        return text;
    };

    auto makeStatsSnapshot = [&](const StatsTracker& s)->StatsSnapshot {
        StatsSnapshot snap;
        snap.totalEvents = s.totalEvents;
        snap.hitCount = s.hitCount;
        snap.wasteCount = s.wasteCount;
        snap.eventActive = s.eventActive;
        snap.classText = buildClassText(s.classCounts);
        snap.classCounts = s.classCounts;
        snap.lastEventDir = s.lastEventDir;
        snap.lastEventLabel = s.lastEventLabel;
        snap.lastDecisionFrame = s.lastDecisionFrame;
        snap.lastDecisionEventId = s.lastDecisionEventId;
        if (!s.lastEventLabel.isEmpty()) {
            snap.lastText = QString("Last event: %1 (%2)").arg(s.lastEventLabel, s.lastEventDir);
        } else {
            snap.lastText = QString("Last event: --");
        }
        return snap;
    };

    auto getStatsSnapshot = [&]()->StatsSnapshot {
        QMutexLocker lock(&statsMutex);
        return makeStatsSnapshot(stats);
    };

    auto buildStatsFigures = [&](const StatsSnapshot& snap){
        int hit = snap.hitCount;
        int waste = snap.wasteCount;
        QImage hitWaste = renderPieChart("Hit vs Waste",
                                         {"Hit", "Waste"},
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
        QImage classImg = renderPieChart("Class Distribution",
                                         {"0", "1", ">2"},
                                         {static_cast<double>(empty),
                                          static_cast<double>(single),
                                          static_cast<double>(more)},
                                         {QColor(52, 152, 219), QColor(241, 196, 15), QColor(155, 89, 182)});
        return std::pair<QImage, QImage>(hitWaste, classImg);
    };

    auto saveStatsFigures = [&](const QString& outDir, const QString& prefix, const StatsSnapshot& snap)->bool {
        if (outDir.isEmpty()) return false;
        auto figures = buildStatsFigures(snap);
        QDir out(outDir);
        out.mkpath(".");
        QString hitPath = out.filePath(prefix + "_hit_waste.png");
        QString clsPath = out.filePath(prefix + "_class_dist.png");
        bool ok1 = !figures.first.isNull() && figures.first.save(hitPath);
        bool ok2 = !figures.second.isNull() && figures.second.save(clsPath);
        return ok1 && ok2;
    };

    auto updateStatsFigureWindow = [&](const StatsSnapshot& snap){
        if (!statsFigureWindow) return;
        auto figures = buildStatsFigures(snap);
        statsFigureWindow->setImages(figures.first, figures.second);
    };

    auto applyStatsSnapshot = [&](const StatsSnapshot& snap){
        QMetaObject::invokeMethod(statsEventsLabel, [=](){
            statsEventsLabel->setText(QString("Events: %1  Active: %2")
                .arg(snap.totalEvents)
                .arg(snap.eventActive ? "Yes" : "No"));
            statsHitLabel->setText(QString("Hits: %1\nWastes: %2")
                .arg(snap.hitCount)
                .arg(snap.wasteCount));
            statsClassLabel->setText(snap.classText);
            statsLastLabel->setText(snap.lastText);
        }, Qt::QueuedConnection);
    };

    auto resetStats = [&](){
        StatsSnapshot snap;
        {
            QMutexLocker lock(&statsMutex);
            stats = StatsTracker{};
            snap = makeStatsSnapshot(stats);
        }
        applyStatsSnapshot(snap);
    };

    auto showStatsFigures = [&](){
        StatsSnapshot snap = getStatsSnapshot();
        auto figures = buildStatsFigures(snap);
        if (!statsFigureWindow) {
            statsFigureWindow = new StatsFigureWindow(&window);
            statsFigureWindow->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(statsFigureWindow, &QObject::destroyed, [&](){ statsFigureWindow = nullptr; });
            QObject::connect(statsFigureWindow->saveButton(), &QPushButton::clicked, [&](){
                QString outDir = outputEdit->text().trimmed();
                if (outDir.isEmpty()) outDir = QCoreApplication::applicationDirPath();
                QString dir = QFileDialog::getExistingDirectory(statsFigureWindow, "Select output directory", outDir);
                if (dir.isEmpty()) return;
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

    auto endEventLocked = [&](StatsTracker& s, int decisionFrame){
        if (!s.eventActive) return;
        QString dir = "Unknown";
        if (s.hasCentroid) {
            double dy = s.cumulativeDy;
            double threshold = 2.0;
            if (s.frameHeight > 0) {
                threshold = std::max(threshold, s.frameHeight * 0.02);
            }
            bool movedUp = dy < -threshold;
            bool movedDown = dy > threshold;
            bool hasFrame = (s.frameHeight > 0);
            double mid = hasFrame ? s.frameHeight * 0.5 : 0.0;

            if (movedUp && (!hasFrame || s.lastY < mid)) {
                s.wasteCount++;
                dir = "Waste";
            } else if (movedDown && (!hasFrame || s.lastY >= mid)) {
                s.hitCount++;
                dir = "Hit";
            } else if (hasFrame) {
                if (s.lastY < mid) {
                    s.wasteCount++;
                    dir = "Waste";
                } else {
                    s.hitCount++;
                    dir = "Hit";
                }
            } else if (dy < 0.0) {
                s.wasteCount++;
                dir = "Waste";
            } else {
                s.hitCount++;
                dir = "Hit";
            }
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

    auto updateStatsFromEvent = [&](const PipelineEvent& evt, bool processed){
        if (!processed) return;
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
                if (evt.frameHeight > 0) stats.frameHeight = evt.frameHeight;
                stats.totalEvents++;
                QString label = QString::fromStdString(evt.label);
                if (label.isEmpty()) label = "(unclassified)";
                stats.currentLabel = label;
                stats.classCounts[label] = stats.classCounts.value(label) + 1;
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
                    if (evt.frameHeight > 0) stats.frameHeight = evt.frameHeight;
                    stats.totalEvents++;
                    QString label = QString::fromStdString(evt.label);
                    if (label.isEmpty()) label = "(unclassified)";
                    stats.currentLabel = label;
                    stats.classCounts[label] = stats.classCounts.value(label) + 1;
                } else {
                    stats.cumulativeDy += static_cast<double>(evt.centroid.y - stats.lastCentroid.y);
                    stats.lastCentroid = evt.centroid;
                    stats.hasCentroid = true;
                    stats.lastY = evt.centroid.y;
                    stats.minY = std::min(stats.minY, static_cast<double>(evt.centroid.y));
                    stats.maxY = std::max(stats.maxY, static_cast<double>(evt.centroid.y));
                    if (evt.frameHeight > 0) stats.frameHeight = evt.frameHeight;
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

    auto processPipelineFrame = [&](const QImage& img,
                                    PipelineEvent& evt,
                                    int& bgRemaining,
                                    bool& pipelineReady,
                                    double* procMsOut)->bool {
        bgRemaining = 0;
        pipelineReady = false;
        if (!pipelineEnabled.load() || img.isNull()) return false;

        QImage lutImg = applyLutToImage(img);
        cv::Mat gray(lutImg.height(), lutImg.width(), CV_8UC1,
                     const_cast<uchar*>(lutImg.bits()), lutImg.bytesPerLine());
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

    auto writeLiveLogCsv = [&](const QString& outDir,
                               const QString& prefix,
                               const std::vector<LiveLogRecord>& records)->QString {
        if (outDir.isEmpty()) return QString();
        QDir out(outDir);
        out.mkpath(".");
        QString path = out.filePath(prefix + "_live_log.csv");
        QFile logFile(path);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream ts(&logFile);
        ts << "wall_time,frame_index,delivered,dropped,fps,cam_fps,proc_ms,processed,pipeline_enabled,pipeline_ready,bg_remaining,skip_reason,"
              "detected,fired,area,bbox_x,bbox_y,bbox_w,bbox_h,crop_x,crop_y,crop_w,crop_h,crop_path,label,score,triggered,trigger_ok,"
              "event_dir,decision_frame,decision_event_id,hit_count,waste_count\n";
        for (const auto& rec : records) {
            ts << csvQuote(rec.wallTime) << ","
               << rec.frameIndex << ","
               << rec.delivered << ","
               << rec.dropped << ","
               << QString::number(rec.fps, 'f', 2) << ","
               << QString::number(rec.camFps, 'f', 2) << ","
               << QString::number(rec.procMs, 'f', 3) << ","
               << (rec.processed ? "1" : "0") << ","
               << (rec.pipelineEnabled ? "1" : "0") << ","
               << (rec.pipelineReady ? "1" : "0") << ","
               << rec.bgRemaining << ","
               << csvQuote(rec.skipReason) << ","
               << (rec.detected ? "1" : "0") << ","
               << (rec.fired ? "1" : "0") << ","
               << QString::number(rec.area, 'f', 1) << ","
               << rec.bboxX << "," << rec.bboxY << "," << rec.bboxW << "," << rec.bboxH << ","
               << rec.cropX << "," << rec.cropY << "," << rec.cropW << "," << rec.cropH << ","
               << csvQuote(rec.cropPath) << ","
               << csvQuote(rec.label) << ","
               << QString::number(rec.score, 'f', 4) << ","
               << (rec.triggered ? "1" : "0") << ","
               << (rec.triggerOk ? "1" : "0") << ","
               << csvQuote(rec.eventDir) << ","
               << rec.decisionFrame << ","
               << rec.decisionEventId << ","
               << rec.hitCount << ","
               << rec.wasteCount
               << "\n";
        }
        ts.flush();
        logFile.close();
        return path;
    };

    auto writeLiveSequenceLog = [&](const QString& outDir,
                                    const QString& timestamp,
                                    const std::vector<LiveLogRecord>& records,
                                    const QString& onnxResolved,
                                    const QString& metaResolved,
                                    const QString& targetLabel,
                                    const QString& daqChannel,
                                    double daqAmp,
                                    double daqFreqHz,
                                    double daqDuration,
                                    double daqDelay,
                                    int frameSkip,
                                    int bgFrames,
                                    int bgUpdate,
                                    int resetFrames,
                                    double minArea,
                                    double minAreaFrac,
                                    double maxAreaFrac,
                                    int minBbox,
                                    int margin,
                                    int diffThresh,
                                    int blurRadius,
                                    int morphRadius,
                                    double scale,
                                    int gapFireShift,
                                    int displayEvery)->QString {
        if (outDir.isEmpty()) return QString();
        QDir out(outDir);
        out.mkpath(".");
        QString logPath = out.filePath("sequence_test_log_live_" + timestamp + ".csv");
        QFile logFile(logPath);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
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
        int pipelineEnabledBefore = (!records.empty() && records.front().pipelineEnabled) ? 1 : 0;

        QTextStream ts(&logFile);
        ts << "# sequence_folder=live\n";
        ts << "# fps=" << QString::number(avgFps, 'f', 2) << "\n";
        ts << "# frames=" << records.size() << "\n";
        ts << "# display_every=" << displayEvery << "\n";
        ts << "# output_dir=" << outDir << "\n";
        ts << "# onnx=" << onnxResolved << "\n";
        ts << "# metadata=" << metaResolved << "\n";
        ts << "# target_label=" << targetLabel << "\n";
        ts << "# target_class_id=" << targetLabel << "\n";
        ts << "# model_registry_entry_id=" << liveModelCombo->currentData(kLiveModelIdRole).toString() << "\n";
        ts << "# model_state_at_start=" << liveModelCombo->currentData(kLiveModelStateRole).toString() << "\n";
        ts << "# live_use_mode=" << liveModelCombo->currentData(kLiveModelModeRole).toString() << "\n";
        ts << "# model_sha256=" << liveModelCombo->currentData(kLiveModelOnnxHashRole).toString() << "\n";
        ts << "# metadata_sha256=" << liveModelCombo->currentData(kLiveModelMetadataHashRole).toString() << "\n";
        ts << "# pipeline_enabled_before=" << pipelineEnabledBefore << "\n";
        ts << "# pipeline_forced=0\n";
        ts << "# frame_skip=" << frameSkip << "\n";
        ts << "# detect_bg_frames=" << bgFrames << "\n";
        ts << "# detect_bg_update=" << bgUpdate << "\n";
        ts << "# detect_reset_frames=" << resetFrames << "\n";
        ts << "# detect_min_area=" << QString::number(minArea, 'f', 3) << "\n";
        ts << "# detect_min_area_frac=" << QString::number(minAreaFrac, 'f', 6) << "\n";
        ts << "# detect_max_area_frac=" << QString::number(maxAreaFrac, 'f', 6) << "\n";
        ts << "# detect_min_bbox=" << minBbox << "\n";
        ts << "# detect_margin=" << margin << "\n";
        ts << "# detect_diff_thresh=" << diffThresh << "\n";
        ts << "# detect_blur_radius=" << blurRadius << "\n";
        ts << "# detect_morph_radius=" << morphRadius << "\n";
        ts << "# detect_scale=" << QString::number(scale, 'f', 3) << "\n";
        ts << "# detect_gap_fire_shift=" << gapFireShift << "\n";
        ts << "# daq_channel=" << daqChannel << "\n";
        ts << "# daq_range_min=-10\n";
        ts << "# daq_range_max=10\n";
        ts << "# daq_amplitude_v=" << QString::number(daqAmp, 'f', 3) << "\n";
        ts << "# daq_frequency_hz=" << QString::number(daqFreqHz, 'f', 1) << "\n";
        ts << "# daq_duration_ms=" << QString::number(daqDuration, 'f', 3) << "\n";
        ts << "# daq_delay_ms=" << QString::number(daqDelay, 'f', 3) << "\n";
        ts << "index,filename,scheduled_ms,actual_ms,jitter_ms,wall_time,proc_ms,processed,pipeline_enabled,pipeline_ready,bg_remaining,skip_reason,"
              "detected,fired,area,bbox_x,bbox_y,bbox_w,bbox_h,crop_x,crop_y,crop_w,crop_h,crop_path,label,score,triggered,trigger_ok,frame_number,"
              "event_dir,decision_frame,decision_event_id\n";

        for (int i = 0; i < static_cast<int>(records.size()); ++i) {
            const auto& rec = records[i];
            double scheduledMs = (avgFps > 0.0) ? (static_cast<double>(i) * 1000.0 / avgFps) : 0.0;
            double actualMs = static_cast<double>(rec.elapsedMs);
            double jitterMs = actualMs - scheduledMs;
            QString filename = QString("live_frame_%1").arg(rec.frameIndex);

            ts << i << ","
               << csvQuote(filename) << ","
               << QString::number(scheduledMs,'f',3) << ","
               << QString::number(actualMs,'f',3) << ","
               << QString::number(jitterMs,'f',3) << ","
               << csvQuote(rec.wallTime) << ","
               << QString::number(rec.procMs,'f',3) << ","
               << (rec.processed ? "1" : "0") << ","
               << (rec.pipelineEnabled ? "1" : "0") << ","
               << (rec.pipelineReady ? "1" : "0") << ","
               << rec.bgRemaining << ","
               << csvQuote(rec.skipReason) << ","
               << (rec.detected ? "1" : "0") << ","
               << (rec.fired ? "1" : "0") << ","
               << QString::number(rec.area,'f',1) << ","
               << rec.bboxX << "," << rec.bboxY << "," << rec.bboxW << "," << rec.bboxH << ","
               << rec.cropX << "," << rec.cropY << "," << rec.cropW << "," << rec.cropH << ","
               << csvQuote(rec.cropPath) << ","
               << csvQuote(rec.label) << ","
               << QString::number(rec.score,'f',4) << ","
               << (rec.triggered ? "1" : "0") << ","
               << (rec.triggerOk ? "1" : "0") << ","
               << rec.frameIndex << ","
               << csvQuote(rec.eventDir) << ","
               << rec.decisionFrame << ","
               << rec.decisionEventId
               << "\n";
        }
        ts.flush();
        logFile.close();
        return logPath;
    };

    auto writeEventTrajectoryCsv = [&](const QString& outDir,
                                       const QString& filename,
                                       const std::vector<SequenceEventRecord>& events)->QString {
        if (outDir.isEmpty()) return QString();
        QDir out(outDir);
        out.mkpath(".");
        QString path = out.filePath(filename);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream ts(&file);
        auto csvQuote = [](const QString& s)->QString {
            QString out = s;
            out.replace("\"", "\"\"");
            return "\"" + out + "\"";
        };
        ts << "event_id,label,detected_frame,decision_frame,decision_dir,fired_frame,frames_tracked,"
              "start_x,start_y,end_x,end_y,min_y,max_y,cumulative_dy,path_length,frame_height\n";
        for (const auto& rec : events) {
            ts << rec.eventId << ","
               << csvQuote(rec.label) << ","
               << rec.startFrame << ","
               << rec.decisionFrame << ","
               << csvQuote(rec.decisionDir) << ","
               << rec.firedFrame << ","
               << rec.framesTracked << ","
               << QString::number(rec.startX, 'f', 3) << ","
               << QString::number(rec.startY, 'f', 3) << ","
               << QString::number(rec.endX, 'f', 3) << ","
               << QString::number(rec.endY, 'f', 3) << ","
               << QString::number(rec.minY, 'f', 3) << ","
               << QString::number(rec.maxY, 'f', 3) << ","
               << QString::number(rec.cumulativeDy, 'f', 3) << ","
               << QString::number(rec.pathLength, 'f', 3) << ","
               << rec.frameHeight
               << "\n";
        }
        ts.flush();
        file.close();
        return path;
    };

    auto writeSequenceSummaryCsv = [&](const QString& outDir,
                                       const QString& filename,
                                       const std::vector<SequenceEventRecord>& events,
                                       const QString& targetLabel,
                                       int totalFrames,
                                       double fps,
                                       const QString& sequenceFolder,
                                       const QString& outputDir,
                                       const QString& onnxResolved,
                                       const QString& metaResolved)->QString {
        if (outDir.isEmpty()) return QString();
        QDir out(outDir);
        out.mkpath(".");
        QString path = out.filePath(filename);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream ts(&file);
        auto csvQuote = [](const QString& s)->QString {
            QString out = s;
            out.replace("\"", "\"\"");
            return "\"" + out + "\"";
        };
        QString target = targetLabel.trimmed();
        if (target.isEmpty()) {
            target = "Single";
        }
        QString targetLower = target.toLower();
        const QString unclassifiedLower = normalizeEventLabel(QString()).toLower();
        QMap<QString, int> classCounts;
        int hitCount = 0;
        int wasteCount = 0;
        int truePositive = 0;
        int trueNegative = 0;
        int falsePositive = 0;
        int falseNegative = 0;
        int consideredEvents = 0;
        for (const auto& rec : events) {
            QString label = normalizeEventLabel(rec.label);
            QString labelLower = label.toLower();
            bool isClassified = (labelLower != unclassifiedLower);
            bool eligible = (rec.firedFrame >= 0) && isClassified;
            if (!eligible) {
                continue;
            }
            consideredEvents++;
            classCounts[label] = classCounts.value(label) + 1;
            bool isTarget = (labelLower == targetLower);
            bool isHit = (rec.decisionDir == "Hit");
            bool isWaste = (rec.decisionDir == "Waste");
            if (isHit) hitCount++;
            if (isWaste) wasteCount++;
            if (isTarget) {
                if (isHit) {
                    truePositive++;
                } else if (isWaste) {
                    falseNegative++;
                }
            } else {
                if (isWaste) {
                    trueNegative++;
                } else if (isHit) {
                    falsePositive++;
                }
            }
        }
        int totalDecisions = consideredEvents;
        double efficiency = totalDecisions > 0 ? static_cast<double>(truePositive + trueNegative) / totalDecisions : 0.0;
        double precision = (truePositive + falsePositive) > 0
            ? static_cast<double>(truePositive) / (truePositive + falsePositive)
            : 0.0;
        double recall = (truePositive + falseNegative) > 0
            ? static_cast<double>(truePositive) / (truePositive + falseNegative)
            : 0.0;

        ts << "metric,value\n";
        ts << "summary_schema,sequence_summary.motion_alignment.v2\n";
        ts << "summary_note," << csvQuote("Counts compare the runtime target label to motion Hit/Waste decisions; this is not sequence accuracy.") << "\n";
        ts << "sequence_folder," << csvQuote(sequenceFolder) << "\n";
        ts << "output_dir," << csvQuote(outputDir) << "\n";
        ts << "onnx," << csvQuote(onnxResolved) << "\n";
        ts << "metadata," << csvQuote(metaResolved) << "\n";
        ts << "target_label," << csvQuote(target) << "\n";
        ts << "target_class_id," << csvQuote(target) << "\n";
        ts << "model_registry_entry_id," << csvQuote(liveModelCombo->currentData(kLiveModelIdRole).toString()) << "\n";
        ts << "model_state_at_start," << csvQuote(liveModelCombo->currentData(kLiveModelStateRole).toString()) << "\n";
        ts << "live_use_mode," << csvQuote(liveModelCombo->currentData(kLiveModelModeRole).toString()) << "\n";
        ts << "model_sha256," << csvQuote(liveModelCombo->currentData(kLiveModelOnnxHashRole).toString()) << "\n";
        ts << "metadata_sha256," << csvQuote(liveModelCombo->currentData(kLiveModelMetadataHashRole).toString()) << "\n";
        ts << "fps," << QString::number(fps, 'f', 2) << "\n";
        ts << "frames_total," << totalFrames << "\n";
        ts << "events_detected," << events.size() << "\n";
        ts << "events_considered_fired_classified," << consideredEvents << "\n";
        ts << "motion_hit_count," << hitCount << "\n";
        ts << "motion_waste_count," << wasteCount << "\n";
        ts << "target_motion_hit_count," << truePositive << "\n";
        ts << "target_motion_waste_count," << falseNegative << "\n";
        ts << "non_target_motion_waste_count," << trueNegative << "\n";
        ts << "non_target_motion_hit_count," << falsePositive << "\n";
        ts << "target_vs_motion_alignment_rate," << QString::number(efficiency, 'f', 4) << "\n";
        ts << "target_motion_hit_precision," << QString::number(precision, 'f', 4) << "\n";
        ts << "target_motion_hit_recall," << QString::number(recall, 'f', 4) << "\n";

        ts << "\nclass,label,count\n";
        for (auto it = classCounts.begin(); it != classCounts.end(); ++it) {
            ts << "class," << csvQuote(it.key()) << "," << it.value() << "\n";
        }

        ts << "\ntarget_event_id,detected_frame,decision_frame,decision_dir,fired_frame,frames_tracked,start_y,end_y,cumulative_dy,path_length\n";
        for (const auto& rec : events) {
            QString labelLower = normalizeEventLabel(rec.label).toLower();
            bool isClassified = (labelLower != unclassifiedLower);
            if (rec.firedFrame < 0 || !isClassified) continue;
            if (labelLower != targetLower) continue;
            ts << rec.eventId << ","
               << rec.startFrame << ","
               << rec.decisionFrame << ","
               << csvQuote(rec.decisionDir) << ","
               << rec.firedFrame << ","
               << rec.framesTracked << ","
               << QString::number(rec.startY, 'f', 3) << ","
               << QString::number(rec.endY, 'f', 3) << ","
               << QString::number(rec.cumulativeDy, 'f', 3) << ","
               << QString::number(rec.pathLength, 'f', 3)
               << "\n";
        }
        ts.flush();
        file.close();
        return path;
    };

    startLiveLogging = [&](){
        QMutexLocker lock(&liveLogMutex);
        liveLog.clear();
        liveLogStart = QDateTime::currentDateTime();
        {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.reset(resetFramesSpin->value());
        }
        liveLogging.store(true);
    };

    stopLiveLogging = [&](){
        if (!liveLogging.exchange(false)) return;
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
        if (outDir.isEmpty()) outDir = QCoreApplication::applicationDirPath();
        QString timestamp = liveLogStart.isValid()
            ? liveLogStart.toString("yyyyMMdd_hhmmss")
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
        QString seqLogPath = writeLiveSequenceLog(outDir, timestamp, records,
                                                 onnxResolved, metaResolved, targetLabel,
                                                 daqChannel, daqAmp, daqFreqHz, daqDuration, daqDelay,
                                                 frameSkip, bgFrames, bgUpdate, resetFrames, minArea, minAreaFrac, maxAreaFrac,
                                                 minBbox, margin, diffThresh, blurRadius, morphRadius, scale, gapFireShift,
                                                 displayEvery);
        QString trajPath = writeEventTrajectoryCsv(outDir,
                                                   "sequence_event_trajectory_live_" + timestamp + ".csv",
                                                   liveEvents);
        QString summaryPath = writeSequenceSummaryCsv(outDir,
                                                      "sequence_summary_live_" + timestamp + ".csv",
                                                      liveEvents,
                                                      targetLabel,
                                                      static_cast<int>(records.size()),
                                                      avgFps,
                                                      QString(),
                                                      outDir,
                                                      onnxResolved,
                                                      metaResolved);
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

    auto formatBytes = [](size_t bytes)->QString {
        const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return QString("%1 MB").arg(mb, 0, 'f', 1);
    };

    auto setSequenceUiRunning = [&](bool running){
        seqStartBtn->setEnabled(!running);
        seqStopBtn->setEnabled(running);
        seqLoadBtn->setEnabled(!running);
        pipelineWidget->setEnabled(!running);
        labviewWidget->setEnabled(!running);
        detectWidget->setEnabled(!running);
        pipelineStartBtn->setEnabled(!running && !pipelineEnabled.load());
        pipelineStopBtn->setEnabled(!running && pipelineEnabled.load());
        if (!viewerOnly) {
            startBtn->setEnabled(!running);
            stopBtn->setEnabled(!running);
            reconnectBtn->setEnabled(!running);
            applyBtn->setEnabled(!running);
        }
    };

    auto updateSequenceStatus = [&](const QString& text){
        QMetaObject::invokeMethod(seqStatusLabel, [seqStatusLabel, text](){
            seqStatusLabel->setText(text);
        }, Qt::QueuedConnection);
    };

    auto collectSequenceFiles = [&](const QString& dirPath)->QStringList {
        QDir dir(dirPath);
        QStringList filters;
        filters << "*.tif" << "*.tiff" << "*.TIF" << "*.TIFF"
                << "*.png" << "*.PNG" << "*.jpg" << "*.JPG"
                << "*.jpeg" << "*.JPEG" << "*.bmp" << "*.BMP";
        return dir.entryList(filters, QDir::Files, QDir::Name);
    };

    QObject::connect(seqBrowseBtn, &QPushButton::clicked, [&](){
        QString dir = QFileDialog::getExistingDirectory(&window, "Select sequence folder", seqFolderEdit->text());
        if (!dir.isEmpty()) seqFolderEdit->setText(dir);
    });

    QObject::connect(seqLoadBtn, &QPushButton::clicked, [&](){
        if (sequenceRunning.load() || sequenceLoading.load()) return;
        sequenceStop.store(false);
        QString dirPath = seqFolderEdit->text().trimmed();
        QDir dir(dirPath);
        if (!dir.exists()) {
            seqStatusLabel->setText("Sequence folder not found.");
            return;
        }
        QStringList files = collectSequenceFiles(dirPath);
        if (files.isEmpty()) {
            seqStatusLabel->setText("No images found in folder.");
            return;
        }

        seqLoadBtn->setEnabled(false);
        seqStartBtn->setEnabled(false);
        updateSequenceStatus(QString("Loading %1 frames...").arg(files.size()));
        sequenceLoading.store(true);
        QPointer<QLabel> seqStatusLabelPtr(seqStatusLabel);
        QPointer<QPushButton> seqLoadBtnPtr(seqLoadBtn);
        QPointer<QPushButton> seqStartBtnPtr(seqStartBtn);

        backgroundTasks.launch("sequence-load",
            [&, dirPath, files, seqStatusLabelPtr, seqLoadBtnPtr, seqStartBtnPtr](const BackgroundTaskRegistry::StopFlag& stop) {
            auto frames = std::make_shared<std::vector<SequenceFrame>>();
            frames->reserve(files.size());
            size_t totalBytes = 0;
            int loaded = 0;
            for (const QString& rel : files) {
                if (sequenceStop.load() || stop->load()) break;
                QString absPath = QDir(dirPath).absoluteFilePath(rel);
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
                    QMetaObject::invokeMethod(seqStatusLabelPtr, [seqStatusLabelPtr, progress](){
                        if (!seqStatusLabelPtr.isNull()) {
                            seqStatusLabelPtr->setText(progress);
                        }
                    }, Qt::QueuedConnection);
                }
            }
            const bool canceled = sequenceStop.load() || stop->load();
            if (!canceled) {
                QMutexLocker lock(&sequenceMutex);
                sequenceFrames = frames;
            }
            const QString status = canceled
                ? QString("Sequence load canceled.")
                : QString("Loaded %1 frames (%2).").arg(frames->size()).arg(formatBytes(totalBytes));
            sequenceLoading.store(false);
            if (!seqStatusLabelPtr.isNull()) {
                QMetaObject::invokeMethod(seqStatusLabelPtr, [seqStatusLabelPtr, status](){
                    if (!seqStatusLabelPtr.isNull()) {
                        seqStatusLabelPtr->setText(status);
                    }
                }, Qt::QueuedConnection);
            }
            if (!seqLoadBtnPtr.isNull()) {
                QMetaObject::invokeMethod(seqLoadBtnPtr, [seqLoadBtnPtr, seqStartBtnPtr, canceled](){
                    if (!seqLoadBtnPtr.isNull()) seqLoadBtnPtr->setEnabled(true);
                    if (!seqStartBtnPtr.isNull()) seqStartBtnPtr->setEnabled(!canceled);
                }, Qt::QueuedConnection);
            }
        });
    });

    auto stopSequenceTest = [&](){
        sequenceStop.store(true);
        if (sequenceThread.joinable()) {
            sequenceThread.join();
        }
        if (sequenceRunning.load()) {
            sequenceRunning.store(false);
            setSequenceUiRunning(false);
            seqStatusLabel->setText("Sequence stopped.");
            statusLabel->setText("Sequence test stopped.");
        }
    };

    QObject::connect(seqStopBtn, &QPushButton::clicked, stopSequenceTest);

    QObject::connect(statsResetBtn, &QPushButton::clicked, resetStats);
    QObject::connect(statsShowBtn, &QPushButton::clicked, showStatsFigures);

    QObject::connect(seqStartBtn, &QPushButton::clicked, [&](){
        if (sequenceRunning.load()) return;
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
            currentRunDir = runDir;
            updateOpenRunAvailability();
        }
        sequencePrevPipelineChecked = pipelineEnableCheck->isChecked();
        sequenceStarting.store(true);
        loadPipeline(true);
        bool pipelineReady = false;
        {
            QMutexLocker lock(&pipelineMutex);
            pipelineReady = pipeline.isReady();
        }
        sequenceStarting.store(false);
        if (!pipelineReady) {
            seqStatusLabel->setText("Pipeline not ready. Fix settings and load pipeline.");
            return;
        }
        if (sequenceThread.joinable()) {
            sequenceThread.join();
        }
        sequenceStop.store(false);
        sequenceRunning.store(true);
        setSequenceUiRunning(true);

        if (!viewerOnly) {
            QMetaObject::invokeMethod(cameraWorker, [cameraWorker]() {
                cameraWorker->stopCapture();
            }, Qt::BlockingQueuedConnection);
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
        seqStatusLabel->setText(QString("Running %1 frames at %2 fps...")
            .arg(frames->size()).arg(fps,0,'f',2));

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

        sequenceThread = std::thread([&, frames, fps, displayEvery, logPath, outDir, onnxResolved, metaResolved, targetLabel, seqFolder,
                                      daqChannel, daqAmp, daqFreqHz, daqDuration, daqDelay,
                                      frameSkip, bgFrames, bgUpdate, resetFrames, minArea, minAreaFrac, maxAreaFrac,
                                      minBbox, margin, diffThresh, blurRadius, morphRadius, scale, gapFireShift](){
            QFile logFile(logPath);
            if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                updateSequenceStatus("Failed to open sequence log.");
                sequenceRunning.store(false);
                QMetaObject::invokeMethod(&window, [&, logPath](){
                    setSequenceUiRunning(false);
                    statusLabel->setText("Sequence test failed (log open).");
                    seqLogLabel->setText("Log: " + logPath);
                }, Qt::QueuedConnection);
                return;
            }
            QTextStream ts(&logFile);
            ts << "# sequence_folder=" << seqFolder << "\n";
            ts << "# fps=" << QString::number(fps, 'f', 2) << "\n";
            ts << "# frames=" << frames->size() << "\n";
            ts << "# display_every=" << displayEvery << "\n";
            ts << "# output_dir=" << outDir << "\n";
            ts << "# onnx=" << onnxResolved << "\n";
            ts << "# metadata=" << metaResolved << "\n";
            ts << "# target_label=" << targetLabel << "\n";
            ts << "# pipeline_enabled_before=" << (sequencePrevPipelineChecked ? 1 : 0) << "\n";
            ts << "# pipeline_forced=" << (sequencePrevPipelineChecked ? 0 : 1) << "\n";
            ts << "# frame_skip=" << frameSkip << "\n";
            ts << "# detect_bg_frames=" << bgFrames << "\n";
            ts << "# detect_bg_update=" << bgUpdate << "\n";
            ts << "# detect_reset_frames=" << resetFrames << "\n";
            ts << "# detect_min_area=" << QString::number(minArea, 'f', 3) << "\n";
            ts << "# detect_min_area_frac=" << QString::number(minAreaFrac, 'f', 6) << "\n";
            ts << "# detect_max_area_frac=" << QString::number(maxAreaFrac, 'f', 6) << "\n";
            ts << "# detect_min_bbox=" << minBbox << "\n";
            ts << "# detect_margin=" << margin << "\n";
            ts << "# detect_diff_thresh=" << diffThresh << "\n";
            ts << "# detect_blur_radius=" << blurRadius << "\n";
            ts << "# detect_morph_radius=" << morphRadius << "\n";
            ts << "# detect_scale=" << QString::number(scale, 'f', 3) << "\n";
            ts << "# detect_gap_fire_shift=" << gapFireShift << "\n";
            ts << "# daq_channel=" << daqChannel << "\n";
            ts << "# daq_range_min=-10\n";
            ts << "# daq_range_max=10\n";
            ts << "# daq_amplitude_v=" << QString::number(daqAmp, 'f', 3) << "\n";
            ts << "# daq_frequency_hz=" << QString::number(daqFreqHz, 'f', 1) << "\n";
            ts << "# daq_duration_ms=" << QString::number(daqDuration, 'f', 3) << "\n";
            ts << "# daq_delay_ms=" << QString::number(daqDelay, 'f', 3) << "\n";
            ts << "index,filename,scheduled_ms,actual_ms,jitter_ms,wall_time,proc_ms,processed,pipeline_enabled,pipeline_ready,bg_remaining,skip_reason,"
                  "detected,fired,area,bbox_x,bbox_y,bbox_w,bbox_h,crop_x,crop_y,crop_w,crop_h,crop_path,label,score,triggered,trigger_ok,frame_number,"
                  "event_dir,decision_frame,decision_event_id\n";
            ts.flush();

            SequenceEventTracker tracker;
            tracker.reset(resetFrames);

            auto csvQuote = [](const QString& s)->QString {
                QString out = s;
                out.replace("\"", "\"\"");
                return "\"" + out + "\"";
            };

            using clock = std::chrono::steady_clock;
            auto start = clock::now();
            std::chrono::duration<double> period(1.0 / fps);

            for (size_t i = 0; i < frames->size(); ++i) {
                if (sequenceStop.load()) break;
                auto target = start + period * static_cast<double>(i);
                while (!sequenceStop.load()) {
                    auto now = clock::now();
                    if (now >= target) break;
                    auto remaining = target - now;
                    if (remaining > std::chrono::milliseconds(2)) {
                        std::this_thread::sleep_for(remaining - std::chrono::milliseconds(1));
                    } else {
                        std::this_thread::yield();
                    }
                }
                if (sequenceStop.load()) break;

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
                    QImage imgCopy = applyLutToImage(frame.image);
                    QMetaObject::invokeMethod(&window, [&, imgCopy, meta, i, fps, frames](){
                        imageView->setImage(imgCopy);
                        lastFrame = imgCopy;
                        lastMeta = meta;
                        statsLabel->setText(QString("Source: Sequence\nResolution: %1 x %2\nBits: %3\nFPS: %4\nFrame: %5 / %6")
                            .arg(meta.width).arg(meta.height).arg(meta.bits)
                            .arg(fps,0,'f',2).arg(i + 1).arg(frames->size()));
                    }, Qt::QueuedConnection);
                }

                QString cropPath = QString::fromStdString(evt.cropPath);
                QString label = QString::fromStdString(evt.label);
                ts << i << ","
                   << csvQuote(QFileInfo(frame.path).fileName()) << ","
                   << QString::number(scheduledMs,'f',3) << ","
                   << QString::number(actualMs,'f',3) << ","
                   << QString::number(jitterMs,'f',3) << ","
                   << csvQuote(wallTime) << ","
                   << QString::number(procMs,'f',3) << ","
                   << (processed ? "1" : "0") << ","
                   << (enabledNow ? "1" : "0") << ","
                   << (pipelineReady ? "1" : "0") << ","
                   << bgRemaining << ","
                   << csvQuote(skipReason) << ","
                   << (evt.detected ? "1" : "0") << ","
                   << (evt.fired ? "1" : "0") << ","
                   << QString::number(evt.area,'f',1) << ","
                   << evt.bbox.x << "," << evt.bbox.y << "," << evt.bbox.width << "," << evt.bbox.height << ","
                   << evt.cropRect.x << "," << evt.cropRect.y << "," << evt.cropRect.width << "," << evt.cropRect.height << ","
                   << csvQuote(cropPath) << ","
                   << csvQuote(label) << ","
                   << QString::number(evt.score,'f',4) << ","
                   << (evt.triggered ? "1" : "0") << ","
                   << (evt.triggerOk ? "1" : "0") << ","
                   << evt.frameNumber << ","
                   << csvQuote(tracker.lastEventDir) << ","
                   << tracker.lastDecisionFrame << ","
                   << tracker.lastDecisionEventId
                   << "\n";
                if (i % 50 == 0) {
                    ts.flush();
                }
            }

            tracker.finalize();
            QString trajPath = writeEventTrajectoryCsv(outDir,
                                                       "sequence_event_trajectory_" + timestamp + ".csv",
                                                       tracker.events);
            QString summaryPath = writeSequenceSummaryCsv(outDir,
                                                          "sequence_summary_" + timestamp + ".csv",
                                                          tracker.events,
                                                          targetLabel,
                                                          static_cast<int>(frames->size()),
                                                          fps,
                                                          seqFolder,
                                                          outDir,
                                                          onnxResolved,
                                                          metaResolved);

            ts.flush();
            logFile.close();

            sequenceRunning.store(false);
            QMetaObject::invokeMethod(&window, [&, logPath, trajPath, summaryPath](){
                setSequenceUiRunning(false);
                seqStatusLabel->setText("Sequence finished.");
                statusLabel->setText("Sequence test finished.");
                QString logText = "Log: " + logPath;
                if (!trajPath.isEmpty()) {
                    logText += "\nTrajectory: " + trajPath;
                }
                if (!summaryPath.isEmpty()) {
                    logText += "\nSummary: " + summaryPath;
                }
                seqLogLabel->setText(logText);
            }, Qt::QueuedConnection);
        });
    });

    cameraWorker->setRecordHook([saveMutex, saveBuffer, &recording, &recordedFrames,
                           &pipelineEnabled, &sequenceRunning, &processPipelineFrame,
                           &liveLogging, &liveLogMutex, &liveLog, &getStatsSnapshot, &liveLogStart,
                           &datasetCaptureActive, &datasetCaptureMutex, &datasetCaptureSession,
                           &datasetCaptureDir, &datasetCaptureManifestPath, datasetStartCaptureBtn,
                           datasetStopCaptureBtn, datasetCaptureStatusLabel, statusLabel,
                           trainerDatasetEdit, &openDatasetLabelerPath, &window](const QImage& img, const FrameMeta& meta, double fps){
        if (recording.load()) {
            QMutexLocker lk(saveMutex.get());
            saveBuffer->push_back(img.copy());
            recordedFrames++;
        }

        if (sequenceRunning.load()) return;

        PipelineEvent evt;
        int bgRemaining = 0;
        bool pipelineReady = false;
        double procMs = 0.0;
        bool processed = processPipelineFrame(img, evt, bgRemaining, pipelineReady, &procMs);

        if (datasetCaptureActive.load() && processed && evt.fired && evt.classified && !evt.cropPath.empty()) {
            bool reachedTarget = false;
            std::size_t collected = 0;
            std::size_t target = 0;
            QString addError;
            {
                QMutexLocker captureLock(&datasetCaptureMutex);
                if (datasetCaptureActive.load()) {
                    DatasetCropCandidate candidate;
                    candidate.sourceType = "live_stream";
                    candidate.sourceSequenceId = "live_camera";
                    candidate.sourceFrameIndex = static_cast<int>(meta.frameIndex);
                    candidate.eventId = liveEventTracker.currentEventId;
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
                        datasetCaptureSession.finalize(err);
                        datasetCaptureActive.store(false);
                    } else {
                        reachedTarget = datasetCaptureSession.targetReached();
                        collected = datasetCaptureSession.collectedCount();
                        target = datasetCaptureSession.currentBatchTarget();
                    }
                }
            }
            if (!addError.isEmpty()) {
                QMetaObject::invokeMethod(&window, [&, addError]() {
                    datasetStartCaptureBtn->setEnabled(true);
                    datasetStopCaptureBtn->setEnabled(false);
                    datasetCaptureStatusLabel->setText("Dataset Builder capture stopped after error: " + addError);
                    statusLabel->setText("Dataset Builder capture stopped after error.");
                }, Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(&window, [&, collected, target]() {
                    datasetCaptureStatusLabel->setText(QString("Dataset Builder capture active: %1 / %2 crops\n%3")
                        .arg(static_cast<qulonglong>(collected))
                        .arg(static_cast<qulonglong>(target))
                        .arg(datasetCaptureDir));
                }, Qt::QueuedConnection);
            }
            if (reachedTarget) {
                QMetaObject::invokeMethod(&window, [&, collected]() {
                    QMessageBox prompt(&window);
                    prompt.setWindowTitle("Dataset Builder Batch Target Reached");
                    prompt.setText(QString("Dataset Builder collected %1 crops. Continue collecting or stop and review?").arg(static_cast<qulonglong>(collected)));
                    QPushButton* continueButton = prompt.addButton("Continue Collecting", QMessageBox::AcceptRole);
                    QPushButton* reviewButton = prompt.addButton("Stop and Review", QMessageBox::RejectRole);
                    prompt.exec();
                    bool continueCollecting = (prompt.clickedButton() == continueButton);
                    {
                        QMutexLocker captureLock(&datasetCaptureMutex);
                        if (datasetCaptureActive.load()) {
                            if (continueCollecting) {
                                datasetCaptureSession.extendBatchTarget();
                            } else {
                                datasetCaptureSession.recordBatchPrompt("stop_for_review");
                                datasetCaptureSession.setStopReason("user_stop_after_batch_prompt");
                                std::string err;
                                datasetCaptureSession.finalize(err);
                                datasetCaptureActive.store(false);
                            }
                        }
                    }
                    if (continueCollecting) {
                        datasetCaptureStatusLabel->setText(QString("Dataset Builder capture continuing to %1 crops\n%2")
                            .arg(static_cast<qulonglong>(datasetCaptureSession.currentBatchTarget()))
                            .arg(datasetCaptureDir));
                    } else {
                        datasetStartCaptureBtn->setEnabled(true);
                        datasetStopCaptureBtn->setEnabled(false);
                        datasetCaptureStatusLabel->setText("Dataset Builder capture stopped for review.\nManifest: " + datasetCaptureManifestPath);
                        trainerDatasetEdit->setText(datasetCaptureDir);
                        if (QFileInfo::exists(datasetCaptureManifestPath)) {
                            openDatasetLabelerPath(datasetCaptureManifestPath);
                        }
                    }
                }, Qt::BlockingQueuedConnection);
            }
        }

        if (liveLogging.load()) {
            QString lastEventDir;
            int lastDecisionFrame = -1;
            int lastDecisionEventId = 0;
            {
                QMutexLocker eventLock(&liveEventMutex);
                liveEventTracker.update(evt, processed);
                lastEventDir = liveEventTracker.lastEventDir;
                lastDecisionFrame = liveEventTracker.lastDecisionFrame;
                lastDecisionEventId = liveEventTracker.lastDecisionEventId;
            }
            bool enabledNow = pipelineEnabled.load();
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
            rec.elapsedMs = liveLogStart.isValid()
                ? liveLogStart.msecsTo(QDateTime::currentDateTime())
                : 0;
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
            rec.hitCount = snap.hitCount;
            rec.wasteCount = snap.wasteCount;
            QMutexLocker lk(&liveLogMutex);
            liveLog.push_back(rec);
        }
    });

    auto applyFrameToPreviewWorkspaces =
        [&](const QImage& img, FrameMeta meta, double fps) {
            if (!img.isNull()) {
                QImage viewImg = applyLutToImage(img);
                imageView->setImage(viewImg);
                cameraImageView->setImage(viewImg);
                lastFrame = viewImg;
                liveViewerEmpty->hide();
                cameraViewerEmpty->hide();
            }
            lastMeta = meta;
            const QString resolutionText = QString("RES %1 x %2\nCAM %3")
                .arg(meta.width).arg(meta.height)
                .arg(meta.delivered > 0 ? "LIVE" : "IDLE");
            const QString frameTimeText = QString("EXP %1 ms\nPROC -- ms").arg(exposureSpin->value(), 0, 'f', 3);
            const QString fpsText = QString("FPS %1\nFRAME %2\nDROP %3")
                .arg(fps, 0, 'f', 1).arg(meta.frameIndex).arg(meta.dropped);
            liveHudResolution->setText(resolutionText);
            cameraHudResolution->setText(resolutionText);
            liveHudFrameTime->setText(frameTimeText);
            cameraHudFrameTime->setText(frameTimeText);
            liveHudFps->setText(fpsText);
            cameraHudFps->setText(fpsText);
            statsLabel->setText(QString("Resolution: %1 x %2\nBinning: %3\nBits: %4\nFPS: %5 (Cam: %6)\nFrame: %7\nDelivered: %8 Dropped: %9\nReadout: %10")
                .arg(meta.width).arg(meta.height).arg(meta.binning,0,'f',1).arg(meta.bits)
                .arg(fps,0,'f',1).arg(meta.internalFps,0,'f',1).arg(meta.frameIndex).arg(meta.delivered).arg(meta.dropped).arg(meta.readoutSpeed,0,'f',0));
            if (logCheck->isChecked() && (meta.frameIndex % 100 == 0)) {
                logLine(QString("Frame=%1 FPS=%2 camfps=%3 delivered=%4 dropped=%5")
                    .arg(meta.frameIndex).arg(fps,0,'f',1).arg(meta.internalFps,0,'f',1).arg(meta.delivered).arg(meta.dropped));
            }
        };

    QObject::connect(cameraWorker, &CameraWorker::frameReady, &window,
                     [&](const QImage& img, FrameMeta meta, double fps){
        applyFrameToPreviewWorkspaces(img, meta, fps);
    }, Qt::QueuedConnection);

    auto shutdownCameraThread = [&]() {
        if (!cameraThread.isRunning()) {
            return;
        }

        auto shutdownAck = std::make_shared<QSemaphore>();
        const bool queued = QMetaObject::invokeMethod(cameraWorker, [cameraWorker, shutdownAck]() {
            cameraWorker->shutdown();
            shutdownAck->release();
        }, Qt::QueuedConnection);
        if (queued && !shutdownAck->tryAcquire(1, 5000)) {
            logMessage("Timed out waiting for camera worker shutdown acknowledgement.");
        } else if (!queued) {
            logMessage("Failed to queue camera worker shutdown.");
        }

        cameraThread.quit();
        if (!cameraThread.wait(5000)) {
            logMessage("Timed out waiting for camera worker thread to stop.");
        }
    };

    QObject::connect(&app, &QApplication::aboutToQuit, [&](){
        backgroundTasks.requestStop();
        sequenceStop.store(true);
        recording.store(false);
        if (datasetCaptureActive.load()) {
            QMutexLocker lock(&datasetCaptureMutex);
            datasetCaptureSession.setStopReason("cancelled");
            std::string err;
            datasetCaptureSession.finalize(err);
            datasetCaptureActive.store(false);
        }
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            trainerProcess->terminate();
            if (!trainerProcess->waitForFinished(2500)) {
                trainerProcess->kill();
                trainerProcess->waitForFinished(1000);
            }
        }
        stopSequenceTest();
        backgroundTasks.waitAll();
        stopLiveLogging();
        shutdownCameraThread();
        logMessage("Exiting application");
    });

    cameraWorker->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished, cameraWorker, &QObject::deleteLater);
    cameraThread.start();

    Q_UNUSED(splashTimer);
    app.processEvents();
    QThread::msleep(700);
    app.processEvents();
    window.showMaximized();
    splash.finish(&window);
    doInit();
    if (options.noDaq) {
        logDaqStartupState("DAQ disabled by launch option.");
    }
    if (!hardwareFreeMode) {
        QTimer::singleShot(0, [&](){
            loadPipeline(false);
        });
    }
    if (!options.datasetBuilderReviewPath.trimmed().isEmpty()) {
        QTimer::singleShot(0, [&](){
            logMessage("Opening Dataset Builder review manifest from command line: " + options.datasetBuilderReviewPath);
            openDatasetLabelerPath(options.datasetBuilderReviewPath);
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

            workspaceStack->setCurrentWidget(liveWorkspacePage);
            liveNavButton->setChecked(true);
            headerTitleLabel->setText("/ Live View");
            headerStatusText->setText("Live View workspace");
            rightScroll->setMinimumWidth(336);
            rightScroll->setMaximumWidth(336);
            mainSplitter->setSizes({qMax(760, window.width() - 336), 336});
            app.processEvents();

            require(workspaceStack->currentWidget() == liveWorkspacePage, "Live View workspace opens as the active page");
            require(liveNavButton && liveNavButton->toolTip() == "Live View", "Live navigation tooltip reads Live View");
            require(headerTitleLabel && headerTitleLabel->text() == "/ Live View", "Header title reads / Live View");
            require(headerStatusText && headerStatusText->text() == "Live View workspace", "Header status reads Live View workspace");
            require(!liveNavButton->text().contains("Live Sorting"), "Live navigation label no longer shows Live Sorting");
            require(!headerTitleLabel->text().contains("Live Sorting"), "Header title no longer shows Live Sorting");
            require(!headerStatusText->text().contains("Live Sorting"), "Header status no longer shows Live Sorting");

            auto* cameraWorkspace = window.findChild<QWidget*>("CameraWorkspace");
            auto* cameraControlsStack = window.findChild<QWidget*>("CameraControlsStack");
            auto* cameraFormatPanel = window.findChild<QWidget*>("CameraFormatSpeedPanel");
            auto* cameraFormatPanelFrame = window.findChild<QWidget*>("CameraFormatSpeedPanelFrame");
            auto* cameraLutPanel = window.findChild<QWidget*>("CameraLutDisplayPanel");
            auto* cameraLutPanelFrame = window.findChild<QWidget*>("CameraLutDisplayPanelFrame");
            auto* cameraRecordingPanel = window.findChild<QWidget*>("CameraRecordingPanel");
            auto* cameraRecordingPanelFrame = window.findChild<QWidget*>("CameraRecordingPanelFrame");
            auto* cameraAdvancedPanel = window.findChild<QWidget*>("CameraAdvancedFrameStatsPanel");
            auto* cameraLutRangeBar = window.findChild<QWidget*>("CameraLutRangeBar");
            auto* cameraNavButton = window.findChild<QPushButton*>("NavCameraButton");
            auto* liveHardwarePanel = window.findChild<QWidget*>("LiveHardwarePanel");
            auto* liveClassDistributionPanel = window.findChild<QWidget*>("LiveClassDistributionPanel");
            auto* cameraIndependentBinningCheck = window.findChild<QCheckBox*>("CameraIndependentBinningCheckBox");
            auto* cameraBinHSpin = window.findChild<QSpinBox*>("CameraBinHSpinBox");
            auto* cameraBinVSpin = window.findChild<QSpinBox*>("CameraBinVSpinBox");
            auto* cameraLutMinSlider = window.findChild<QSlider*>("CameraLutMinSlider");
            auto* cameraLutMaxSlider = window.findChild<QSlider*>("CameraLutMaxSlider");
            auto* cameraPresetCombo = window.findChild<QComboBox*>("CameraPresetComboBox");
            auto* cameraBitsCombo = window.findChild<QComboBox*>("CameraBitsComboBox");
            auto* cameraWidthSpin = window.findChild<QSpinBox*>("CameraCustomWidthSpinBox");
            auto* cameraHeightSpin = window.findChild<QSpinBox*>("CameraCustomHeightSpinBox");
            auto* cameraExposureSpin = window.findChild<QDoubleSpinBox*>("CameraExposureSpinBox");
            auto* cameraReadoutCombo = window.findChild<QComboBox*>("CameraReadoutSpeedComboBox");
            auto* cameraBinningCombo = window.findChild<QComboBox*>("CameraBinningComboBox");
            auto* cameraLutModeControl = window.findChild<QWidget*>("CameraLutModeSegmentedControl");
            auto* cameraLutMinSpin = window.findChild<QSpinBox*>("CameraLutMinSpinBox");
            auto* cameraLutMaxSpin = window.findChild<QSpinBox*>("CameraLutMaxSpinBox");
            auto* cameraDisplayEverySpin = window.findChild<QSpinBox*>("CameraDisplayEverySpinBox");
            auto* savePathLineEdit = window.findChild<QLineEdit*>("SavePathEdit");
            auto* saveBrowseButton = window.findChild<QPushButton*>("SaveBrowseButton");
            auto* saveOpenFolderButton = window.findChild<QPushButton*>("SaveOpenFolderButton");
            auto* cameraRecordingFormatControl = window.findChild<QWidget*>("CameraRecordingFormatSegmentedControl");
            auto* saveStartButton = window.findChild<QPushButton*>("SaveStartButton");
            auto* saveStopButton = window.findChild<QPushButton*>("SaveStopButton");
            auto* navRailFrame = window.findChild<QFrame*>("OpenDssNavigationRail");
            auto* headerFrame = window.findChild<QFrame*>("OpenDssHeader");
            auto* statusStripFrame = window.findChild<QFrame*>("OpenDssStatusStrip");
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

            require(cameraNavButton == nullptr, "NavCameraButton is absent");
            require(cameraWorkspace == nullptr, "CameraWorkspace is absent");
            require(workspaceStack->currentWidget() == liveWorkspacePage, "LiveWorkspace is selected");
            require(rightViewport != nullptr, "LiveRightMetricsScrollArea viewport exists");
            require(cameraControlsStack != nullptr, "CameraControlsStack exists");
            require(cameraFormatPanel != nullptr, "CameraFormatSpeedPanel exists");
            require(cameraFormatPanel && cameraFormatPanel->isVisibleTo(&window), "CameraFormatSpeedPanel is visible");
            require(cameraFormatPanelFrame != nullptr, "CameraFormatSpeedPanelFrame exists");
            require(cameraLutPanel != nullptr, "CameraLutDisplayPanel exists");
            require(cameraLutPanel && cameraLutPanel->isVisibleTo(&window), "CameraLutDisplayPanel is visible");
            require(cameraLutPanelFrame != nullptr, "CameraLutDisplayPanelFrame exists");
            require(cameraRecordingPanel != nullptr, "CameraRecordingPanel exists");
            require(cameraRecordingPanel && cameraRecordingPanel->isVisibleTo(&window), "CameraRecordingPanel is visible");
            require(cameraRecordingPanelFrame != nullptr, "CameraRecordingPanelFrame exists");
            require(cameraLutRangeBar != nullptr, "CameraLutRangeBar exists");
            require(cameraLutRangeBar && cameraLutRangeBar->isVisibleTo(&window), "CameraLutRangeBar is visible");
            require(cameraAdvancedPanel == nullptr, "CameraAdvancedFrameStatsPanel is absent");
            require(liveHardwarePanel == nullptr, "LiveHardwarePanel is absent");
            require(liveClassDistributionPanel == nullptr, "LiveClassDistributionPanel is absent");
            require(cameraIndependentBinningCheck == nullptr, "CameraIndependentBinningCheckBox is absent");
            require(cameraBinHSpin == nullptr, "CameraBinHSpinBox is absent");
            require(cameraBinVSpin == nullptr, "CameraBinVSpinBox is absent");
            require(cameraLutMinSlider == nullptr, "CameraLutMinSlider is absent from the visible workspace tree");
            require(cameraLutMaxSlider == nullptr, "CameraLutMaxSlider is absent from the visible workspace tree");
            require(navRailFrame != nullptr, "OpenDssNavigationRail exists");
            require(headerFrame != nullptr, "OpenDssHeader exists");
            require(statusStripFrame != nullptr, "OpenDssStatusStrip exists");
            require(cameraControlsStack && rightViewport && cameraControlsStack->width() <= rightViewport->width(),
                    "CameraControlsStack width does not exceed the Live View right-panel viewport");
            require(rightScroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
                    "Live View right-panel horizontal scrollbar remains disabled");

            requireHorizontallyContained(cameraFormatPanelFrame, rightViewport, "CameraFormatSpeedPanelFrame fits within the viewport width");
            requireHorizontallyContained(cameraLutPanelFrame, rightViewport, "CameraLutDisplayPanelFrame fits within the viewport width");
            requireHorizontallyContained(cameraRecordingPanelFrame, rightViewport, "CameraRecordingPanelFrame fits within the viewport width");
            requireContained(cameraPresetCombo, cameraFormatPanelFrame, "CameraPresetComboBox fits within Format & Speed");
            requireContained(cameraBitsCombo, cameraFormatPanelFrame, "CameraBitsComboBox fits within Format & Speed");
            requireContained(cameraWidthSpin, cameraFormatPanelFrame, "CameraCustomWidthSpinBox fits within Format & Speed");
            requireContained(cameraHeightSpin, cameraFormatPanelFrame, "CameraCustomHeightSpinBox fits within Format & Speed");
            requireContained(cameraExposureSpin, cameraFormatPanelFrame, "CameraExposureSpinBox fits within Format & Speed");
            requireContained(cameraReadoutCombo, cameraFormatPanelFrame, "CameraReadoutSpeedComboBox fits within Format & Speed");
            requireContained(cameraBinningCombo, cameraFormatPanelFrame, "CameraBinningComboBox fits within Format & Speed");
            require(cameraPresetCombo && cameraPresetCombo->width() >= cameraPresetCombo->sizeHint().width(),
                    "CameraPresetComboBox expands enough for the active preset text");
            require(cameraReadoutCombo && cameraReadoutCombo->width() >= cameraReadoutCombo->sizeHint().width(),
                    "CameraReadoutSpeedComboBox expands enough for the active readout text");
            requireContained(cameraLutModeControl, cameraLutPanelFrame, "CameraLutModeSegmentedControl fits within LUT & Display");
            requireContained(cameraLutMinSpin, cameraLutPanelFrame, "CameraLutMinSpinBox fits within LUT & Display");
            requireContained(cameraLutMaxSpin, cameraLutPanelFrame, "CameraLutMaxSpinBox fits within LUT & Display");
            requireContained(cameraLutRangeBar, cameraLutPanelFrame, "CameraLutRangeBar fits within LUT & Display");
            requireContained(cameraDisplayEverySpin, cameraLutPanelFrame, "CameraDisplayEverySpinBox fits within LUT & Display");
            requireContained(savePathLineEdit, cameraRecordingPanelFrame, "SavePathEdit fits within Recording");
            requireContained(saveBrowseButton, cameraRecordingPanelFrame, "SaveBrowseButton fits within Recording");
            requireContained(saveOpenFolderButton, cameraRecordingPanelFrame, "SaveOpenFolderButton fits within Recording");
            requireContained(cameraRecordingFormatControl, cameraRecordingPanelFrame, "CameraRecordingFormatSegmentedControl fits within Recording");
            requireContained(saveStartButton, cameraRecordingPanelFrame, "SaveStartButton fits within Recording");
            requireContained(saveStopButton, cameraRecordingPanelFrame, "SaveStopButton fits within Recording");

            const auto shellColors = desktop_app::theme::colors(currentThemeMode);
            const QString shellCss = shellColors.shellBackground.name(QColor::HexRgb);
            const QString appCss = shellColors.appBackground.name(QColor::HexRgb);
            const QString styleSheet = window.styleSheet();
            require(styleSheet.contains(shellCss, Qt::CaseInsensitive), QString("shell stylesheet contains neutral shell color %1").arg(shellCss));
            require(shellCss.compare(QStringLiteral("#0B1F5E"), Qt::CaseInsensitive) != 0, "shell color is no longer dark blue");
            require(shellCss.compare(appCss, Qt::CaseInsensitive) != 0, "shell color remains distinct from app background");

            lutMinSpin->setValue(32);
            lutMaxSpin->setValue(180);
            app.processEvents();
            require(lutMinValue.load() == 32, "LUT min runtime value updates from consolidated controls");
            require(lutMaxValue.load() == 180, "LUT max runtime value updates from consolidated controls");

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
            applyFrameToPreviewWorkspaces(sample, verifyMeta, 37.5);
            app.processEvents();

            auto* liveImageLabel = imageView->findChild<QLabel*>("LiveImageLabel");
            const QPixmap livePixmap = liveImageLabel ? liveImageLabel->pixmap(Qt::ReturnByValue) : QPixmap();
            require(liveImageLabel && !livePixmap.isNull(), "LiveImageLabel received a rendered frame");
            require(!liveViewerEmpty->isVisible(), "LiveViewerEmptyState hides after frame update");
            require(liveHudResolution->text().contains("320 x 240"), "LiveViewerHudResolutionLabel updated from frame data");
            require(liveHudFps->text().contains("42"), "LiveViewerHudFpsLabel updated from frame data");

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

            refreshDaqDeviceOptions(true);
            workspaceStack->setCurrentWidget(settingsWorkspacePage);
            settingsNavButton->setChecked(true);
            headerTitleLabel->setText("/ Settings");
            headerStatusText->setText("Settings workspace");
            app.processEvents();
            waitForUi(350);

            auto* settingsHardwarePanel = window.findChild<QWidget*>("SettingsHardwarePanel");
            auto* deviceCombo = window.findChild<QComboBox*>("DaqDeviceComboBox");
            auto* channelEdit = window.findChild<QLineEdit*>("DaqChannelEdit");
            auto* reconnectButton = window.findChild<QPushButton*>("DaqReconnectButton");
            auto* statusIndicatorWidget = window.findChild<QLabel*>("DaqStatusTextLabel");
            auto* statusBarDaqWidget = window.findChild<QLabel*>("DaqStatusBarLabel");
            auto* shellDaqStatusWidget = window.findChild<QLabel*>("OpenDssShellDaqStatusLabel");
            auto* headerDaqChipWidget = window.findChild<QLabel*>("OpenDssHeaderDaqChip");
            auto* forceTriggerButton = window.findChild<QPushButton*>("LiveForceTriggerButton");

            require(settingsHardwarePanel != nullptr, "Settings hardware panel exists");
            require(deviceCombo != nullptr, "DAQ device combo exists");
            require(channelEdit != nullptr, "DAQ channel edit exists");
            require(deviceCombo && deviceCombo->objectName() == "DaqDeviceComboBox", "DAQ device combo uses the direct-lookup object name");
            require(deviceCombo && channelEdit
                    && deviceCombo->mapTo(settingsHardwarePanel, QPoint(0, 0)).y() < channelEdit->mapTo(settingsHardwarePanel, QPoint(0, 0)).y(),
                    "DAQ device combo is above the DAQ channel field in Settings > Hardware");

            QStringList comboEntries;
            for (int i = 0; i < deviceCombo->count(); ++i) {
                comboEntries << QStringLiteral("%1 => %2").arg(deviceCombo->itemText(i), deviceCombo->itemData(i).toString());
            }
            qInfo().noquote() << "VERIFY INFO: DAQ combo entries:" << comboEntries.join(" | ");
            qInfo().noquote() << "VERIFY INFO: Selected DAQ device:" << deviceCombo->currentData().toString();
            qInfo().noquote() << "VERIFY INFO: Selected DAQ channel:" << channelEdit->text().trimmed();
            qInfo().noquote() << "VERIFY INFO: Discovered DAQ summary:" << (describeDiscoveredDaqDevices().isEmpty() ? QStringLiteral("<none>") : describeDiscoveredDaqDevices());
            if (!daqDiscoveryError.isEmpty()) {
                qInfo().noquote() << "VERIFY INFO: DAQ discovery status:" << daqDiscoveryError;
            }

            if (!discoveredDaqDevices.empty()) {
                require(deviceCombo->count() == static_cast<int>(discoveredDaqDevices.size()),
                        "DAQ combo count matches the discovered device list");
            }

            const int compatibleCount = discoveredCompatibleDeviceCount();
            QString onlyCompatibleDevice;
            for (const auto& device : discoveredDaqDevices) {
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

            if (deviceCombo->count() > 1) {
                const int originalIndex = deviceCombo->currentIndex();
                const int nextIndex = (originalIndex + 1) % deviceCombo->count();
                deviceCombo->setCurrentIndex(nextIndex);
                waitForUi(350);
                QSettings settings;
                const QString selectedDevice = deviceCombo->currentData().toString().trimmed();
                require(settings.value("settings/daqSelectedDevice").toString().trimmed().compare(selectedDevice, Qt::CaseInsensitive) == 0,
                        "Changing the DAQ combo persists the selected device in QSettings");
                const DaqDeviceInfo* selectedInfo = findDiscoveredDaqDevice(selectedDevice);
                if (selectedInfo && selectedInfo->isCompatible()) {
                    require(channelDeviceName(channelEdit->text()).compare(selectedDevice, Qt::CaseInsensitive) == 0,
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
                        "Force Trigger stays gated during verification without arming or firing");
            } else if (deviceCombo->count() == 1) {
                QSettings settings;
                require(settings.value("settings/daqSelectedDevice").toString().trimmed().compare(deviceCombo->currentData().toString().trimmed(), Qt::CaseInsensitive) == 0,
                        "Single discovered DAQ selection is persisted in QSettings");
                if (reconnectButton) {
                    reconnectButton->click();
                    waitForUi(500);
                }
            } else {
                require(!deviceCombo->isEnabled(), "DAQ combo disables when no devices are available");
            }

            if (statusBarDaqWidget && shellDaqStatusWidget && headerDaqChipWidget) {
                require(statusBarDaqWidget->text() == shellDaqStatusWidget->text(),
                        "Shell DAQ status mirrors the DAQ status-bar label");
                const QString statusText = statusBarDaqWidget->text().toLower();
                const QString headerText = headerDaqChipWidget->text().toLower();
                qInfo().noquote() << "VERIFY INFO: DAQ status-bar text:" << statusBarDaqWidget->text();
                qInfo().noquote() << "VERIFY INFO: Header DAQ chip text:" << headerDaqChipWidget->text();
                if (statusText.contains("unavailable")) {
                    require(headerText.contains("unavailable"), "Header DAQ chip reports unavailable when DAQ status is unavailable");
                } else if (statusText.contains("disabled")) {
                    require(headerText.contains("unavailable"), "Header DAQ chip reports unavailable when DAQ status is disabled");
                } else if (statusText.contains("available")) {
                    require(headerText.contains("available"), "Header DAQ chip reports available when DAQ status is available");
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
