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
    Q_PROPERTY(qint64 processedImages READ processedImages NOTIFY changed)
    Q_PROPERTY(qint64 eligibleImages READ eligibleImages NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QVariantMap resultSummary READ resultSummary NOTIFY changed)
    Q_PROPERTY(QUrl summaryUrl READ summaryUrl NOTIFY changed)
    Q_PROPERTY(QUrl predictionsCsvUrl READ predictionsCsvUrl NOTIFY changed)
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
    qint64 processedImages() const;
    qint64 eligibleImages() const;
    double progress() const;
    QVariantMap resultSummary() const;
    QUrl summaryUrl() const;
    QUrl predictionsCsvUrl() const;
    QUrl artifactOutputFolderUrl() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();

  signals:
    void changed();

  private:
    bool active() const;
    void clearOutcome();
    void updateReadyPresentation();
    void postProgress(qint64 processed, qint64 eligible);
    void finishRun(bool succeeded, const QString& serviceError,
                   const QString& outputPath);

    QString opendssVersion_;
    QUrl datasetManifestUrl_;
    QUrl outputFolderUrl_;
    QString presentation_ = QStringLiteral("empty");
    QString errorMessage_;
    qint64 processedImages_ = 0;
    qint64 eligibleImages_ = 0;
    QVariantMap resultSummary_;
    QUrl summaryUrl_;
    QUrl predictionsCsvUrl_;
    QUrl artifactOutputFolderUrl_;
    ModelTestService service_;
    std::atomic_bool stopRequested_{false};
    std::thread worker_;
};

} // namespace desktop_app::v2::model_test
