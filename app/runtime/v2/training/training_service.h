#pragma once

#include "../operation/operation_coordinator.h"

#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QTimer>

namespace desktop_app::v2::training {

enum class TrainingProfile {
    Faster,
    MoreAccurate,
};

enum class TrainingState {
    Ready,
    Running,
    Completed,
    Failed,
    Interrupted,
};

struct TrainingRequest {
    QString datasetJsonPath;
    TrainingProfile profile = TrainingProfile::Faster;
    QString modelName;
    QString outputDirectory;
    QString pythonExecutable;
    QString device;
    QString workingDirectory;
    QString initializationMode;
    QString initializationPath;
};

struct TrainingProgress {
    QString stage;
    int stageEpochs = 0;
    int epoch = 0;
    int globalEpoch = 0;
};

struct TrainingResult {
    QString runDirectory;
    QString modelOnnx;
    QString metadataJson;
};

class TrainingService final : public QObject
{
    Q_OBJECT

public:
    explicit TrainingService(OperationCoordinator &operations, QObject *parent = nullptr);

    TrainingState state() const;
    const TrainingProgress &progress() const;
    const QString &lastError() const;
    const QString &standardError() const;
    const TrainingResult &result() const;
    const QString &preparedManifestPath() const;
    const QString &configPath() const;

    bool start(const TrainingRequest &request, QString *error = nullptr);
    void cancel();

signals:
    void changed();

private:
    void setState(TrainingState state);
    void consumeStandardOutput();
    void consumeStandardError();
    void processOutputLine(const QByteArray &line);
    void finish(int exitCode, QProcess::ExitStatus exitStatus);
    void failToStart(const QString &message);

    QProcess process_;
    QTimer killTimer_;
    OperationCoordinator &operations_;
    OperationLease operationLease_;
    TrainingState state_ = TrainingState::Ready;
    TrainingProgress progress_;
    TrainingResult result_;
    QString lastError_;
    QString standardError_;
    QString preparedManifestPath_;
    QString configPath_;
    QString activeOutputRoot_;
    QByteArray outputBuffer_;
    QString protocolError_;
    QString trainerError_;
    QJsonObject runFinished_;
    bool cancelRequested_ = false;
};

} // namespace desktop_app::v2::training
