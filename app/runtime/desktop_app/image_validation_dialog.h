#pragma once

#include <QDialog>
#include <QProcess>
#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class ImageValidationWidget : public QWidget {
  public:
    using SummaryChangedCallback = std::function<void(const QString&)>;
    enum class ObjectNameMode { Dialog, Workspace };

    ImageValidationWidget(QWidget* parent, const QString& initialPython, const QString& initialModel,
                          const QString& initialMetadata, const QString& initialDataset, const QString& initialOutput,
                          const QString& trainerPythonPath,
                          ObjectNameMode objectNameMode = ObjectNameMode::Dialog);
    ~ImageValidationWidget() override;

    void setSummaryChangedCallback(SummaryChangedCallback callback);

  private:
    void addPathRow(QGridLayout* layout, int row, const QString& label, QLineEdit* edit, bool directory,
                    const QString& dialogTitle);
    QStringList commandArguments() const;
    QString missingInputs() const;
    void updatePreviewAndGate();
    void loadSettings();
    void saveSettings() const;
    void startValidation();
    void cancelValidation();
    void stopProcess(int timeoutMs);
    void finishValidation(int exitCode, QProcess::ExitStatus exitStatus);
    void appendLog(const QString& text);
    bool loadSummaryArtifacts();

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
    QLabel* detailsLabel = nullptr;
    std::unique_ptr<QProcess> process;
    QString pythonPath;
    QString summaryPath;
    QString terminalStatus;
    bool canceled = false;
    bool workspaceMode = false;
    SummaryChangedCallback summaryChangedCallback;
};

class ImageValidationDialog : public QDialog {
  public:
    ImageValidationDialog(QWidget* parent, const QString& initialPython, const QString& initialModel,
                          const QString& initialMetadata, const QString& initialDataset, const QString& initialOutput,
                          const QString& trainerPythonPath);
    ~ImageValidationDialog() override;
};
