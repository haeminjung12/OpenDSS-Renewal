#pragma once

#include "training_service.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

class PipelineRunner;

namespace desktop_app::v2 {

class ApplicationStateStore;
class ModelLibraryController;
class ModelLoadService;
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
    Q_PROPERTY(QUrl registeredPackageUrl READ registeredPackageUrl NOTIFY changed)
    Q_PROPERTY(bool retrySaveAvailable READ retrySaveAvailable NOTIFY changed)
    Q_PROPERTY(QStringList weightOptions READ weightOptions NOTIFY changed)
    Q_PROPERTY(int selectedWeightIndex READ selectedWeightIndex NOTIFY changed)
    Q_PROPERTY(QString selectedWeightPath READ selectedWeightPath NOTIFY changed)

public:
    TrainingController(OperationCoordinator &operations, ApplicationStateStore &stateStore,
                       ModelLoadService &modelLoadService, PipelineRunner &pipeline,
                       ModelLibraryController &modelLibraryController,
                       QString pythonExecutable, QString repositoryRoot,
                       QString modelsRoot,
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
    QUrl registeredPackageUrl() const;
    bool retrySaveAvailable() const;
    QStringList weightOptions() const;
    int selectedWeightIndex() const;
    QString selectedWeightPath() const;

    void setDatasetManifestUrl(const QUrl &url);
    void setArchitecture(const QString &architecture);
    void setModelName(const QString &name);
    void setOutputDirectoryUrl(const QUrl &url);
    void setRequestedDevice(const QString &device);

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool retrySave();
    Q_INVOKABLE bool loadWeights(int index);

signals:
    void changed();

private:
    struct WeightOption {
        QString label;
        QString path;
        QString initializationMode;
    };

    enum class RegistrationState {
        NotStarted,
        Saving,
        SaveFailed,
        Completed,
    };

    bool selectionsLocked() const;
    QString inputError() const;
    bool saveCompletedTraining();
    void handleServiceChanged();
    void publishTrainingState();
    void refreshWeightOptions();

    TrainingService service_;
    ApplicationStateStore &stateStore_;
    ModelLoadService &modelLoadService_;
    PipelineRunner &pipeline_;
    ModelLibraryController &modelLibraryController_;
    QString pythonExecutable_;
    QString repositoryRoot_;
    QString modelsRoot_;
    QVector<WeightOption> weightOptions_;
    int selectedWeightIndex_ = -1;
    QUrl datasetManifestUrl_;
    QString architecture_ = QStringLiteral("mobilenet");
    QString modelName_;
    QUrl outputDirectoryUrl_;
    QString requestedDevice_ = QStringLiteral("gpu");
    QString controllerError_;
    QString registrationError_;
    QUrl registeredPackageUrl_;
    RegistrationState registrationState_ = RegistrationState::NotStarted;
};

} // namespace training
} // namespace desktop_app::v2
