#pragma once

#include "dataset_manifest_v2.h"
#include "../operation/operation_coordinator.h"

#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2::dataset {

enum class DatasetLabelState {
    Unlabeled,
    Class0,
    Class1,
    Class2,
    Excluded,
};

struct DatasetLabelRecordState {
    QString recordId;
    QString cropPath;
    DatasetLabelState state = DatasetLabelState::Unlabeled;
};

struct DatasetLabelCounts {
    QVector<int> classCounts;
    int unreviewed = 0;
    int excluded = 0;
};

struct DatasetLabelSnapshot {
    QString manifestPath;
    QString datasetId;
    QVector<DatasetClass> classes;
    QVector<DatasetLabelRecordState> records;
    DatasetLabelCounts counts;
    bool canUndo = false;
};

class DatasetLabelService {
  public:
    explicit DatasetLabelService(OperationCoordinator& operations);

    bool open(const QString& manifestPath, QString* error = nullptr);

    bool configureClassCount(int classCount, QString* error = nullptr);
    bool renameClass(const QString& classId, const QString& name, QString* error = nullptr);
    bool assignClass(const QString& recordId, const QString& classId, QString* error = nullptr);
    bool exclude(const QString& recordId, QString* error = nullptr);
    bool undo(QString* error = nullptr);
    bool saveAs(const QString& destinationFolder, QString* error = nullptr);

    DatasetLabelSnapshot snapshot() const;

  private:
    struct UndoState {
        QVector<DatasetClass> classes;
        QVector<UserLabelRecord> labels;
    };

    bool saveMutation(const QVector<DatasetClass>& classes,
                      const QVector<UserLabelRecord>& labels, QString* error);
    bool isOpen(QString* error) const;

    OperationCoordinator& operations_;
    QString manifestPath_;
    DatasetManifestData data_;
    std::optional<UndoState> undo_;
};

} // namespace desktop_app::v2::dataset
