#pragma once

#include "training_service.h"
#include "../operation/operation_coordinator.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
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
    Q_PROPERTY(QString architecture READ architecture NOTIFY changed)
    Q_PROPERTY(QString modelName READ modelName NOTIFY changed)
    Q_PROPERTY(QString startingWeights READ startingWeights NOTIFY changed)
    Q_PROPERTY(QStringList libraryModelOptions READ libraryModelOptions NOTIFY changed)
    Q_PROPERTY(QVariantMap libraryModelCompatibility READ libraryModelCompatibility NOTIFY changed)
    Q_PROPERTY(int selectedLibraryModelIndex READ selectedLibraryModelIndex NOTIFY changed)
    Q_PROPERTY(QString selectedLibraryModelId READ selectedLibraryModelId NOTIFY changed)
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

public:
    TrainingController(OperationCoordinator &operations, ApplicationStateStore &stateStore,
                       ModelLoadService &modelLoadService, PipelineRunner &pipeline,
                       ModelLibraryController &modelLibraryController,
                       QString pythonExecutable, QString workingDirectory,
                       QObject *parent = nullptr);

    QUrl datasetManifestUrl() const;
    QString architecture() const;
    QString modelName() const;
    QString startingWeights() const;
    QStringList libraryModelOptions() const;
    QVariantMap libraryModelCompatibility() const;
    int selectedLibraryModelIndex() const;
    QString selectedLibraryModelId() const;
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

    void setDatasetManifestUrl(const QUrl &url);
    void setOutputDirectoryUrl(const QUrl &url);
    void setRequestedDevice(const QString &device);

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool retrySave();
    Q_INVOKABLE bool selectLibraryModel(int index);

signals:
    void changed();

private:
    struct WeightOption {
        QString label;
        QString path;
        QString initializationMode;
        QString id;
        QString architecture;
        QString startingWeights;
        QString packagePath;
        QString compatibilityReason;
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
    void refreshLibraryModels();
    QString selectedWeightPath() const;

    TrainingService service_;
    OperationCoordinator &operations_;
    ApplicationStateStore &stateStore_;
    ModelLoadService &modelLoadService_;
    PipelineRunner &pipeline_;
    ModelLibraryController &modelLibraryController_;
    QString pythonExecutable_;
    QString workingDirectory_;
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
    ModelLease selectedModelLease_;
};

} // namespace training
} // namespace desktop_app::v2
