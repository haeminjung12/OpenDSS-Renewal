#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

class QAction;
class QCheckBox;
class QComboBox;
class QDialog;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QWidget;

class DatasetLabelerDialog;

class DatasetWorkspaceController : public QObject {
    Q_OBJECT

  public:
    struct Dependencies {
        QWidget* window = nullptr;
        QString defaultTrainerDataset;
        QString defaultTrainerOutput;
        QLineEdit* trainerPythonEdit = nullptr;
        QLineEdit* trainerDatasetEdit = nullptr;
        QLineEdit* trainerOutputEdit = nullptr;
        QPushButton* trainerPythonBrowseBtn = nullptr;
        QPushButton* trainerDatasetBrowseBtn = nullptr;
        QPushButton* trainerOutputBrowseBtn = nullptr;
        QPushButton* trainerEnvCheckBtn = nullptr;
        QPushButton* trainerConfigurePathBtn = nullptr;
        QPushButton* trainerCancelBtn = nullptr;
        QPushButton* trainerStartTrainingBtn = nullptr;
        QPushButton* trainerDryRunBtn = nullptr;
        QLabel* trainerStatusLabel = nullptr;
        QPlainTextEdit* trainerResultText = nullptr;
        QProgressBar* trainerProgressBar = nullptr;
        const QJsonArray* trainerRegistryEntries = nullptr;
        QString trainerRegistryFilePath;
        QComboBox* trainerStartingModelCombo = nullptr;
        QComboBox* trainerTrainingModeCombo = nullptr;
        QLabel* trainerStartingModelHintLabel = nullptr;
        QComboBox* trainerArchitectureCombo = nullptr;
        QPushButton* trainerPretrainedImageNetBtn = nullptr;
        QPushButton* trainerPretrainedNoneBtn = nullptr;
        QSpinBox* trainerEpochsSpin = nullptr;
        QSpinBox* trainerBatchSpin = nullptr;
        QDoubleSpinBox* trainerLrSpin = nullptr;
        QPlainTextEdit* trainerHyperparameterJsonEdit = nullptr;
        QLabel* trainerSelectedArchitectureValue = nullptr;
        QCheckBox* trainerFlipCheck = nullptr;
        QCheckBox* trainerRotationCheck = nullptr;
        QCheckBox* trainerColorJitterCheck = nullptr;
        QCheckBox* trainerRandomCropCheck = nullptr;
        QComboBox* trainerSchedulerCombo = nullptr;
        QAction* datasetOpenAction = nullptr;
        QAction* datasetBuildAction = nullptr;
        QAction* datasetLabelDatasetAction = nullptr;
    };

    explicit DatasetWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

    void openDatasetLabelerPath(const QString& preferredPath);
    void openDatasetLabeler();
    void appendTrainerLog(const QString& text);
    void saveTrainerSettings() const;
    void setTrainerBusy(bool busy, bool trainerCommandWasTraining) const;
    QString trainingConfigPath() const;
    QStringList trainerTrainArgs(bool dryRun) const;
    QString trainerCommandPreview(const QString& program, const QStringList& args) const;
    QString trainerSummaryText(const QString& stateHeadline = QString(),
                               const QString& stateDetail = QString()) const;
    void refreshTrainerUi() const;

  private:
    struct TrainerDatasetCounts {
        int class0Count = -1;
        int class1Count = -1;
        int class2Count = -1;
        int totalCount = -1;
        bool available = false;
    };

    static QString quoteTrainerArg(QString arg);
    void populateTrainerModelOptions() const;
    void refreshTrainerModeHint() const;
    void refreshTrainerSummary() const;
    bool trainerSetupReady(QStringList* issues = nullptr) const;
    TrainerDatasetCounts collectTrainerDatasetCounts() const;
    void loadTrainerSettings() const;
    void wireDatasetActions();
    void wireTrainerPathButtons();
    void wireTrainerSettingsPersistence();
    void wireTrainerModelRenameAction();
    bool renameSelectedStartingModel() const;
    void runTrainerModelRenameVerifier() const;

    Dependencies deps_;
    QPointer<DatasetLabelerDialog> activeDatasetLabelerDialog_;
};
