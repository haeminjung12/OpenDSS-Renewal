#pragma once

#include "model_test_service.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <atomic>
#include <thread>

namespace desktop_app::v2 {
class ModelLoadService;
class OperationCoordinator;
}

namespace desktop_app::v2::model_test {

class ModelTestController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl datasetManifestUrl READ datasetManifestUrl WRITE
                   setDatasetManifestUrl NOTIFY changed)
    Q_PROPERTY(QUrl outputFolderUrl READ outputFolderUrl WRITE setOutputFolderUrl
                   NOTIFY changed)
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(QString actionError READ actionError NOTIFY changed)
    Q_PROPERTY(bool canStart READ canStart NOTIFY changed)
    Q_PROPERTY(QString activeModelId READ activeModelId NOTIFY changed)
    Q_PROPERTY(QString activeModelName READ activeModelName NOTIFY changed)
    Q_PROPERTY(bool activeModelReady READ activeModelReady NOTIFY changed)
    Q_PROPERTY(QString plannedDeviceText READ plannedDeviceText NOTIFY changed)
    Q_PROPERTY(qint64 processedImages READ processedImages NOTIFY changed)
    Q_PROPERTY(qint64 eligibleImages READ eligibleImages NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QVariantMap resultSummary READ resultSummary NOTIFY changed)
    Q_PROPERTY(QUrl summaryUrl READ summaryUrl NOTIFY changed)
    Q_PROPERTY(QUrl predictionsCsvUrl READ predictionsCsvUrl NOTIFY changed)
    Q_PROPERTY(QUrl partialSummaryUrl READ partialSummaryUrl NOTIFY changed)
    Q_PROPERTY(QUrl partialPredictionsCsvUrl READ partialPredictionsCsvUrl NOTIFY
                   changed)
    Q_PROPERTY(QUrl artifactOutputFolderUrl READ artifactOutputFolderUrl NOTIFY
                   changed)

  public:
    ModelTestController(OperationCoordinator& operations,
                        ModelLoadService& modelLoader,
                        QString opendssVersion,
                        QObject* parent = nullptr);
    ~ModelTestController() override;

    QUrl datasetManifestUrl() const;
    void setDatasetManifestUrl(const QUrl& url);
    QUrl outputFolderUrl() const;
    void setOutputFolderUrl(const QUrl& url);

    QString presentation() const;
    QString errorMessage() const;
    QString actionError() const;
    bool canStart() const;
    QString activeModelId() const;
    QString activeModelName() const;
    bool activeModelReady() const;
    QString plannedDeviceText() const;
    qint64 processedImages() const;
    qint64 eligibleImages() const;
    double progress() const;
    QVariantMap resultSummary() const;
    QUrl summaryUrl() const;
    QUrl predictionsCsvUrl() const;
    QUrl partialSummaryUrl() const;
    QUrl partialPredictionsCsvUrl() const;
    QUrl artifactOutputFolderUrl() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool openSummary();
    Q_INVOKABLE bool openPredictions();
    Q_INVOKABLE bool openOutputFolder();
    Q_INVOKABLE bool openPartialSummary();
    Q_INVOKABLE bool openPartialPredictions();

  public slots:
    void refreshPreflight();

  signals:
    void changed();

  private:
    bool active() const;
    void clearOutcome();
    void updatePreflight();
    void postProgress(qint64 processed, qint64 eligible);
    void finishRun(bool succeeded, const QString& serviceError,
                   const QString& outputPath);
    bool openLocalArtifact(const QUrl& url, const QString& label);

    OperationCoordinator& operations_;
    ModelLoadService& modelLoader_;
    QString opendssVersion_;
    QUrl datasetManifestUrl_;
    QUrl outputFolderUrl_;
    QString presentation_ = QStringLiteral("empty");
    QString errorMessage_;
    QString actionError_;
    bool canStart_ = false;
    QString activeModelId_;
    QString activeModelName_;
    bool activeModelReady_ = false;
    QString plannedDeviceText_;
    qint64 processedImages_ = 0;
    qint64 eligibleImages_ = 0;
    QVariantMap resultSummary_;
    QUrl summaryUrl_;
    QUrl predictionsCsvUrl_;
    QUrl partialSummaryUrl_;
    QUrl partialPredictionsCsvUrl_;
    QUrl artifactOutputFolderUrl_;
    ModelTestService service_;
    std::atomic_bool stopRequested_{false};
    std::thread worker_;
};

} // namespace desktop_app::v2::model_test
