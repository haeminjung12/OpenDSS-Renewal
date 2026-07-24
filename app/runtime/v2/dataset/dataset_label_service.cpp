#include "dataset_label_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

using desktop_app::v2::dataset::DatasetClass;
using desktop_app::v2::dataset::DatasetLabelState;
using desktop_app::v2::dataset::DatasetRecord;
using desktop_app::v2::dataset::UserLabelRecord;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

bool samePath(const QString& first, const QString& second) {
#ifdef Q_OS_WIN
    return QDir::cleanPath(QDir::fromNativeSeparators(first))
               .compare(QDir::cleanPath(QDir::fromNativeSeparators(second)),
                        Qt::CaseInsensitive) == 0;
#else
    return QDir::cleanPath(first) == QDir::cleanPath(second);
#endif
}

bool pathIsWithin(const QString& parent, const QString& candidate) {
    const QString relative = QDir(parent).relativeFilePath(candidate);
    return relative != ".." && !relative.startsWith("../") && !QFileInfo(relative).isAbsolute();
}

bool copyFolder(const QString& source, const QString& destination, QString* error) {
    if (!QDir().mkpath(destination))
        return fail(error, "Could not create Dataset staging folder.");

    const QDir sourceDir(source);
    const QFileInfoList entries =
        sourceDir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString destinationPath = QDir(destination).filePath(entry.fileName());
        if (entry.isDir() && !entry.isSymLink()) {
            if (!copyFolder(entry.absoluteFilePath(), destinationPath, error))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), destinationPath)) {
            return fail(error, "Could not copy Dataset file: " + entry.fileName());
        }
    }
    return true;
}

DatasetLabelState stateForLabel(const UserLabelRecord* label) {
    if (!label)
        return DatasetLabelState::Unlabeled;
    if (label->excluded)
        return DatasetLabelState::Excluded;
    if (label->classId == "0")
        return DatasetLabelState::Class0;
    if (label->classId == "1")
        return DatasetLabelState::Class1;
    return DatasetLabelState::Class2;
}

} // namespace

namespace desktop_app::v2::dataset {

bool DatasetLabelService::open(const QString& manifestPath, QString* error) {
    if (error)
        error->clear();
    const QString absolutePath = QFileInfo(manifestPath).absoluteFilePath();
    const auto manifest = DatasetManifestV2::load(absolutePath, error);
    if (!manifest)
        return false;

    manifestPath_ = absolutePath;
    datasetId_ = manifest->datasetId();
    classes_ = manifest->classes();
    records_ = manifest->records();
    labels_ = manifest->labels();
    undo_.reset();
    return true;
}

bool DatasetLabelService::isOpen(QString* error) const {
    if (manifestPath_.isEmpty())
        return fail(error, "No Dataset is open.");
    return true;
}

bool DatasetLabelService::saveMutation(const QVector<DatasetClass>& classes,
                                       const QVector<UserLabelRecord>& labels, QString* error) {
    if (!DatasetManifestV2::save(manifestPath_, datasetId_, classes, records_, labels, error))
        return false;

    undo_ = UndoState{classes_, labels_};
    classes_ = classes;
    labels_ = labels;
    return true;
}

bool DatasetLabelService::configureClassCount(int classCount, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    if (classCount != 2 && classCount != 3)
        return fail(error, "A Dataset must configure two or three classes.");
    if (classes_.size() == classCount)
        return true;
    if (!classes_.isEmpty() && classes_.size() != 2 && classes_.size() != 3)
        return fail(error, "The current Dataset class schema is invalid.");

    QVector<DatasetClass> next = classes_;
    if (next.isEmpty()) {
        for (int index = 0; index < classCount; ++index)
            next.push_back(DatasetClass{QString::number(index), "Class " + QString::number(index)});
    } else if (next.size() == 2 && classCount == 3) {
        next.push_back(DatasetClass{"2", "Class 2"});
    } else if (next.size() == 3 && classCount == 2) {
        const bool hasClassTwo = std::any_of(labels_.cbegin(), labels_.cend(), [](const UserLabelRecord& label) {
            return !label.excluded && label.classId == "2";
        });
        if (hasClassTwo) {
            return fail(error,
                        "Class 2 labels must be relabeled or excluded before selecting two classes.");
        }
        next.removeLast();
    }
    return saveMutation(next, labels_, error);
}

bool DatasetLabelService::renameClass(const QString& classId, const QString& name, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
        return fail(error, "Class name must not be blank.");

    QVector<DatasetClass> next = classes_;
    const auto current = std::find_if(next.begin(), next.end(), [&](const DatasetClass& value) {
        return value.id == classId;
    });
    if (current == next.end())
        return fail(error, "Unknown Class ID: " + classId);
    const bool duplicate = std::any_of(next.cbegin(), next.cend(), [&](const DatasetClass& value) {
        return value.id != classId && value.name.trimmed().compare(trimmedName, Qt::CaseInsensitive) == 0;
    });
    if (duplicate)
        return fail(error, "Class names must be unique.");
    if (current->name == trimmedName)
        return true;
    current->name = trimmedName;
    return saveMutation(next, labels_, error);
}

bool DatasetLabelService::assignClass(const QString& recordId, const QString& classId,
                                      QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const bool recordExists = std::any_of(records_.cbegin(), records_.cend(), [&](const DatasetRecord& record) {
        return record.recordId == recordId;
    });
    if (!recordExists)
        return fail(error, "Unknown record ID: " + recordId);
    const bool classExists = std::any_of(classes_.cbegin(), classes_.cend(), [&](const DatasetClass& value) {
        return value.id == classId;
    });
    if (!classExists)
        return fail(error, "Class ID is not configured: " + classId);

    QVector<UserLabelRecord> next = labels_;
    const auto current = std::find_if(next.begin(), next.end(), [&](const UserLabelRecord& label) {
        return label.recordId == recordId;
    });
    if (current == next.end()) {
        next.push_back(UserLabelRecord{QUuid::createUuid().toString(QUuid::WithoutBraces),
                                       recordId, classId, false});
    } else {
        if (!current->excluded && current->classId == classId)
            return true;
        current->classId = classId;
        current->excluded = false;
    }
    return saveMutation(classes_, next, error);
}

bool DatasetLabelService::exclude(const QString& recordId, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const bool recordExists = std::any_of(records_.cbegin(), records_.cend(), [&](const DatasetRecord& record) {
        return record.recordId == recordId;
    });
    if (!recordExists)
        return fail(error, "Unknown record ID: " + recordId);

    QVector<UserLabelRecord> next = labels_;
    const auto current = std::find_if(next.begin(), next.end(), [&](const UserLabelRecord& label) {
        return label.recordId == recordId;
    });
    if (current == next.end()) {
        next.push_back(UserLabelRecord{QUuid::createUuid().toString(QUuid::WithoutBraces),
                                       recordId, {}, true});
    } else {
        if (current->excluded)
            return true;
        current->classId.clear();
        current->excluded = true;
    }
    return saveMutation(classes_, next, error);
}

bool DatasetLabelService::undo(QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    if (!undo_)
        return fail(error, "There is no label change to undo.");

    const UndoState previous = *undo_;
    if (!DatasetManifestV2::save(manifestPath_, datasetId_, previous.classes, records_,
                                 previous.labels, error)) {
        return false;
    }
    classes_ = previous.classes;
    labels_ = previous.labels;
    undo_.reset();
    return true;
}

bool DatasetLabelService::saveAs(const QString& destinationFolder, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;

    const QString destination = QFileInfo(destinationFolder).absoluteFilePath();
    if (QFileInfo::exists(destination))
        return fail(error, "Save As destination already exists.");

    const QString source = QFileInfo(manifestPath_).absolutePath();
    if (samePath(source, destination) || pathIsWithin(source, destination))
        return fail(error, "Save As destination must not be inside the current Dataset folder.");

    const QFileInfo destinationInfo(destination);
    const QString parent = destinationInfo.absolutePath();
    if (!QDir().mkpath(parent))
        return fail(error, "Could not create Save As parent folder.");

    const QString staging =
        QDir(parent).filePath("." + destinationInfo.fileName() + ".staging-" +
                              QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir stagingDir(staging);
    auto cleanup = [&] { stagingDir.removeRecursively(); };

    if (!copyFolder(source, staging, error)) {
        cleanup();
        return false;
    }

    const QString newDatasetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagedManifest = QDir(staging).filePath("dataset.json");
    if (!DatasetManifestV2::save(stagedManifest, newDatasetId, classes_, records_, labels_, error)) {
        cleanup();
        return false;
    }
    if (!QDir(parent).rename(staging, destination)) {
        cleanup();
        return fail(error, "Could not publish the new Dataset folder.");
    }

    manifestPath_ = QDir(destination).filePath("dataset.json");
    datasetId_ = newDatasetId;
    undo_.reset();
    return true;
}

DatasetLabelSnapshot DatasetLabelService::snapshot() const {
    DatasetLabelSnapshot value;
    value.manifestPath = manifestPath_;
    value.datasetId = datasetId_;
    value.classes = classes_;
    value.counts.classCounts.fill(0, classes_.size());
    value.canUndo = undo_.has_value();

    for (const DatasetRecord& record : records_) {
        const auto label = std::find_if(labels_.cbegin(), labels_.cend(), [&](const UserLabelRecord& candidate) {
            return candidate.recordId == record.recordId;
        });
        const UserLabelRecord* labelPointer = label == labels_.cend() ? nullptr : &*label;
        const DatasetLabelState state = stateForLabel(labelPointer);
        value.records.push_back(DatasetLabelRecordState{record.recordId, record.cropPath, state});
        if (state == DatasetLabelState::Unlabeled) {
            ++value.counts.unreviewed;
        } else if (state == DatasetLabelState::Excluded) {
            ++value.counts.excluded;
        } else {
            const int classIndex = labelPointer->classId.toInt();
            if (classIndex >= 0 && classIndex < value.counts.classCounts.size())
                ++value.counts.classCounts[classIndex];
        }
    }
    return value;
}

} // namespace desktop_app::v2::dataset
