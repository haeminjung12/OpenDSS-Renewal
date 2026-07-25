#pragma once

#include "training_service.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace desktop_app::v2 {

class ApplicationStateStore;
class OperationCoordinator;

namespace training {

class TrainingController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl datasetManifestUrl READ datasetManifestUrl WRITE setDatasetManifestUrl NOTIFY changed)
    Q_PROPERTY(QString architecture READ architecture WRITE setArchitecture NOTIFY changed)
    Q_PROPERTY(QString modelName READ modelName WRITE setModelName NOTIFY changed)
    Q_PROPERTY(QUrl outputDirectoryUrl READ outputDirectoryUrl WRITE setOutputDirectoryUrl NOTIFY changed)
    Q_PROPERTY(QString requestedDevice READ requestedDevice WRITE setRequestedDevice NOTIFY changed)
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(int stageEpochs READ stageEpochs NOTIFY changed)
    Q_PROPERTY(int epoch READ epoch NOTIFY changed)
    Q_PROPERTY(int globalEpoch READ globalEpoch NOTIFY changed)
    Q_PROPERTY(QUrl resultDirectoryUrl READ resultDirectoryUrl NOTIFY changed)
    Q_PROPERTY(QUrl modelOnnxUrl READ modelOnnxUrl NOTIFY changed)
    Q_PROPERTY(QUrl metadataUrl READ metadataUrl NOTIFY changed)

public:
    TrainingController(OperationCoordinator &operations, ApplicationStateStore &stateStore,
                       QString pythonExecutable, QString repositoryRoot,
                       QObject *parent = nullptr);

    QUrl datasetManifestUrl() const;
    QString architecture() const;
    QString modelName() const;
    QUrl outputDirectoryUrl() const;
    QString requestedDevice() const;
    QString presentation() const;
    QString errorMessage() const;
    QString stage() const;
    int stageEpochs() const;
    int epoch() const;
    int globalEpoch() const;
    QUrl resultDirectoryUrl() const;
    QUrl modelOnnxUrl() const;
    QUrl metadataUrl() const;

    void setDatasetManifestUrl(const QUrl &url);
    void setArchitecture(const QString &architecture);
    void setModelName(const QString &name);
    void setOutputDirectoryUrl(const QUrl &url);
    void setRequestedDevice(const QString &device);

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

signals:
    void changed();

private:
    bool selectionsLocked() const;
    QString inputError() const;
    void publishTrainingState();

    TrainingService service_;
    ApplicationStateStore &stateStore_;
    QString pythonExecutable_;
    QString repositoryRoot_;
    QUrl datasetManifestUrl_;
    QString architecture_ = QStringLiteral("mobilenet");
    QString modelName_;
    QUrl outputDirectoryUrl_;
    QString requestedDevice_ = QStringLiteral("gpu");
    QString controllerError_;
};

} // namespace training
} // namespace desktop_app::v2
