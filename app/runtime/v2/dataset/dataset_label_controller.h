#pragma once

#include "dataset_label_service.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace desktop_app::v2 {

class ApplicationStateStore;
class OperationCoordinator;

namespace dataset {

class DatasetLabelController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QUrl manifestUrl READ manifestUrl NOTIFY changed)
    Q_PROPERTY(QString datasetId READ datasetId NOTIFY changed)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY changed)
    Q_PROPERTY(int labeledCount READ labeledCount NOTIFY changed)
    Q_PROPERTY(int unreviewedCount READ unreviewedCount NOTIFY changed)
    Q_PROPERTY(int excludedCount READ excludedCount NOTIFY changed)
    Q_PROPERTY(int classCount READ classCount NOTIFY changed)
    Q_PROPERTY(int class0Count READ class0Count NOTIFY changed)
    Q_PROPERTY(int class1Count READ class1Count NOTIFY changed)
    Q_PROPERTY(int class2Count READ class2Count NOTIFY changed)
    Q_PROPERTY(QVariantList classNames READ classNames NOTIFY changed)
    Q_PROPERTY(bool class2Enabled READ class2Enabled NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(QVariantList records READ records NOTIFY changed)
    Q_PROPERTY(QVariantList filteredRecords READ filteredRecords NOTIFY changed)
    Q_PROPERTY(QString selectedRecordId READ selectedRecordId NOTIFY changed)
    Q_PROPERTY(QUrl selectedCropUrl READ selectedCropUrl NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QString filter READ filter NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)

public:
    DatasetLabelController(OperationCoordinator &operations, ApplicationStateStore &stateStore,
                           QObject *parent = nullptr);

    QString presentation() const;
    QUrl manifestUrl() const;
    QString datasetId() const;
    int totalCount() const;
    int labeledCount() const;
    int unreviewedCount() const;
    int excludedCount() const;
    int classCount() const;
    int class0Count() const;
    int class1Count() const;
    int class2Count() const;
    QVariantList classNames() const;
    bool class2Enabled() const;
    bool canUndo() const;
    QVariantList records() const;
    QVariantList filteredRecords() const;
    QString selectedRecordId() const;
    QUrl selectedCropUrl() const;
    int selectedIndex() const;
    QString filter() const;
    QString errorMessage() const;

    Q_INVOKABLE bool open(const QUrl &manifestUrl);
    Q_INVOKABLE bool configureClassCount(int classCount);
    Q_INVOKABLE bool renameClass(int index, const QString &name);
    Q_INVOKABLE bool assignClass(const QString &classId);
    Q_INVOKABLE bool exclude();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool select(const QString &recordId);
    Q_INVOKABLE bool setFilter(const QString &filter);
    Q_INVOKABLE bool saveAs(const QUrl &destinationFolderUrl);

signals:
    void changed();

private:
    bool finish(bool success, const QString &error, bool serviceChanged = false);
    void publishDatasetState();
    void refreshSnapshot();
    void refreshFilteredProjection();
    bool isMatchingRecord(const QString &recordId) const;

    DatasetLabelService service_;
    ApplicationStateStore &stateStore_;
    DatasetLabelSnapshot snapshot_;
    QVariantList records_;
    QVariantList filteredRecords_;
    QString selectedRecordId_;
    QUrl selectedCropUrl_;
    int selectedIndex_ = -1;
    QString filter_ = QStringLiteral("all");
    QString errorMessage_;
};

} // namespace dataset
} // namespace desktop_app::v2
