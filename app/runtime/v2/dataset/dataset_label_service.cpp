#include "dataset_label_service.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

QString nextUpdatedAt(const QString& previous) {
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime prior = QDateTime::fromString(previous, Qt::ISODate);
    return (prior.isValid() && now <= prior ? prior.addMSecs(1) : now)
        .toString(Qt::ISODateWithMs);
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

bool isLinkOrReparse(const QFileInfo& entry) {
    if (entry.isSymLink())
        return true;
#ifdef Q_OS_WIN
    const QString nativePath = QDir::toNativeSeparators(entry.absoluteFilePath());
    const DWORD attributes = GetFileAttributesW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return false;
#endif
}

bool canonicalContained(const QString& canonicalRoot, const QFileInfo& entry,
                        QString& canonicalPath, QString* error) {
    if (isLinkOrReparse(entry))
        return fail(error, "Dataset folders must not contain symbolic links or reparse points.");
    canonicalPath = QDir::cleanPath(QDir::fromNativeSeparators(entry.canonicalFilePath()));
    if (canonicalPath.isEmpty() || !pathIsWithin(canonicalRoot, canonicalPath))
        return fail(error, "Dataset entry resolves outside the Dataset folder.");
    return true;
}

bool resolveCropForDisplay(const QString& datasetRoot, const DatasetRecord& record,
                           QString* error) {
    QString relative = QDir::fromNativeSeparators(record.cropPath);
    if (QFileInfo(relative).isAbsolute() || relative.contains(':'))
        return fail(error, "Crop path must be relative to dataset.json.");
    relative = QDir::cleanPath(relative);
    if (relative.isEmpty() || relative == "." || relative == ".." || relative.startsWith("../"))
        return fail(error, "Crop path escapes the Dataset folder.");

    const QString canonicalRoot =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(datasetRoot).canonicalFilePath()));
    if (canonicalRoot.isEmpty())
        return fail(error, "Could not resolve the Dataset folder.");

    const QFileInfo crop(QDir(datasetRoot).absoluteFilePath(relative));
    if (!crop.exists() || !crop.isFile() || !crop.isReadable())
        return fail(error, "Dataset crop is missing or unreadable: " + record.recordId);
    QString canonicalCrop;
    if (!canonicalContained(canonicalRoot, crop, canonicalCrop, error))
        return false;

    QFile cropFile(canonicalCrop);
    if (!cropFile.open(QIODevice::ReadOnly))
        return fail(error, "Dataset crop is missing or unreadable: " + record.recordId);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!cropFile.atEnd())
        hash.addData(cropFile.read(1024 * 1024));
    if (QString::fromLatin1(hash.result().toHex())
            .compare(record.cropSha256, Qt::CaseInsensitive) != 0)
        return fail(error, "Dataset crop SHA-256 does not match dataset.json: " +
                               record.recordId);

    QImageReader reader(canonicalCrop);
    if (!reader.canRead() || reader.read().isNull())
        return fail(error, "Dataset crop is not a decodable image: " + record.recordId);
    return true;
}

bool copyFolder(const QString& canonicalSourceRoot, const QString& source,
                const QString& destination, QString* error) {
    if (!QDir().mkpath(destination))
        return fail(error, "Could not create Dataset staging folder.");

    const QDir sourceDir(source);
    const QFileInfoList entries =
        sourceDir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        QString canonicalEntry;
        if (!canonicalContained(canonicalSourceRoot, entry, canonicalEntry, error))
            return false;
        const QString destinationPath = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyFolder(canonicalSourceRoot, canonicalEntry, destinationPath, error))
                return false;
        } else if (!entry.isFile()) {
            return fail(error, "Dataset contains an unsupported filesystem entry: " +
                                   entry.fileName());
        } else if (!QFile::copy(canonicalEntry, destinationPath)) {
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

DatasetLabelService::DatasetLabelService(OperationCoordinator& operations)
    : operations_(operations) {}

bool DatasetLabelService::open(const QString& manifestPath, QString* error) {
    if (error)
        error->clear();
    const QString absolutePath = QFileInfo(manifestPath).absoluteFilePath();
    auto access = operations_.acquireDataset(absolutePath, DatasetAccess::Read);
    if (!access.acquired())
        return fail(error, access.fault ? access.fault->reason : "Dataset is in use.");
    const auto manifest = DatasetManifestV2::load(absolutePath, error);
    if (!manifest)
        return false;

    QSet<QString> excludedRecordIds;
    for (const UserLabelRecord& label : manifest->labels()) {
        if (label.excluded)
            excludedRecordIds.insert(label.recordId);
    }
    const QString datasetRoot = QFileInfo(absolutePath).absolutePath();
    for (const DatasetRecord& record : manifest->records()) {
        if (manifest->data().provenance.provenanceMode ==
                QStringLiteral("legacy_crop_only") &&
            excludedRecordIds.contains(record.recordId) &&
            record.cropPath.isEmpty() && record.cropSha256.isEmpty())
            continue;
        if (!resolveCropForDisplay(datasetRoot, record, error))
            return false;
    }

    manifestPath_ = absolutePath;
    data_ = manifest->data();
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
    auto access = operations_.acquireDataset(manifestPath_, DatasetAccess::Write);
    if (!access.acquired())
        return fail(error, access.fault ? access.fault->reason : "Dataset is in use.");
    DatasetManifestData next = data_;
    next.classes = classes;
    next.labels = labels;
    next.provenance.updatedAt = nextUpdatedAt(data_.provenance.updatedAt);
    if (!DatasetManifestV2::save(manifestPath_, next, error))
        return false;

    undo_ = UndoState{data_.classes, data_.labels};
    data_ = std::move(next);
    return true;
}

bool DatasetLabelService::configureClassCount(int classCount, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    if (classCount != 2 && classCount != 3)
        return fail(error, "A Dataset must configure two or three classes.");
    if (data_.classes.size() == classCount)
        return true;
    if (!data_.classes.isEmpty() && data_.classes.size() != 2 && data_.classes.size() != 3)
        return fail(error, "The current Dataset class schema is invalid.");

    QVector<DatasetClass> next = data_.classes;
    if (next.isEmpty()) {
        for (int index = 0; index < classCount; ++index)
            next.push_back(DatasetClass{QString::number(index), "Class " + QString::number(index)});
    } else if (next.size() == 2 && classCount == 3) {
        next.push_back(DatasetClass{"2", "Class 2"});
    } else if (next.size() == 3 && classCount == 2) {
        const bool hasClassTwo = std::any_of(data_.labels.cbegin(), data_.labels.cend(), [](const UserLabelRecord& label) {
            return !label.excluded && label.classId == "2";
        });
        if (hasClassTwo) {
            return fail(error,
                        "Class 2 labels must be relabeled or excluded before selecting two classes.");
        }
        next.removeLast();
    }
    return saveMutation(next, data_.labels, error);
}

bool DatasetLabelService::renameClass(const QString& classId, const QString& name, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
        return fail(error, "Class name must not be blank.");

    QVector<DatasetClass> next = data_.classes;
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
    return saveMutation(next, data_.labels, error);
}

bool DatasetLabelService::assignClass(const QString& recordId, const QString& classId,
                                      QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const bool recordExists = std::any_of(data_.records.cbegin(), data_.records.cend(), [&](const DatasetRecord& record) {
        return record.recordId == recordId;
    });
    if (!recordExists)
        return fail(error, "Unknown record ID: " + recordId);
    const bool classExists = std::any_of(data_.classes.cbegin(), data_.classes.cend(), [&](const DatasetClass& value) {
        return value.id == classId;
    });
    if (!classExists)
        return fail(error, "Class ID is not configured: " + classId);

    QVector<UserLabelRecord> next = data_.labels;
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
    return saveMutation(data_.classes, next, error);
}

bool DatasetLabelService::exclude(const QString& recordId, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    const bool recordExists = std::any_of(data_.records.cbegin(), data_.records.cend(), [&](const DatasetRecord& record) {
        return record.recordId == recordId;
    });
    if (!recordExists)
        return fail(error, "Unknown record ID: " + recordId);

    QVector<UserLabelRecord> next = data_.labels;
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
    return saveMutation(data_.classes, next, error);
}

bool DatasetLabelService::undo(QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;
    if (!undo_)
        return fail(error, "There is no label change to undo.");

    auto access = operations_.acquireDataset(manifestPath_, DatasetAccess::Write);
    if (!access.acquired())
        return fail(error, access.fault ? access.fault->reason : "Dataset is in use.");
    const UndoState previous = *undo_;
    DatasetManifestData next = data_;
    next.classes = previous.classes;
    next.labels = previous.labels;
    next.provenance.updatedAt = nextUpdatedAt(data_.provenance.updatedAt);
    if (!DatasetManifestV2::save(manifestPath_, next, error)) {
        return false;
    }
    data_ = std::move(next);
    undo_.reset();
    return true;
}

bool DatasetLabelService::saveAs(const QString& destinationFolder, QString* error) {
    if (error)
        error->clear();
    if (!isOpen(error))
        return false;

    auto access = operations_.acquireDataset(manifestPath_, DatasetAccess::Write);
    if (!access.acquired())
        return fail(error, access.fault ? access.fault->reason : "Dataset is in use.");
    const QString destination = QFileInfo(destinationFolder).absoluteFilePath();
    if (QFileInfo::exists(destination))
        return fail(error, "Save As destination already exists.");

    const QString source = QFileInfo(manifestPath_).absolutePath();
    const QString canonicalSource =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(source).canonicalFilePath()));
    if (canonicalSource.isEmpty())
        return fail(error, "Could not resolve the current Dataset folder.");
    if (isLinkOrReparse(QFileInfo(source)))
        return fail(error, "The current Dataset folder must not be a symbolic link or reparse point.");

    if (samePath(source, destination) || pathIsWithin(source, destination))
        return fail(error, "Save As destination must not be inside the current Dataset folder.");

    const QFileInfo destinationInfo(destination);
    const QString parent = destinationInfo.absolutePath();
    if (!QDir().mkpath(parent))
        return fail(error, "Could not create Save As parent folder.");
    const QString canonicalParent =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(parent).canonicalFilePath()));
    if (canonicalParent.isEmpty())
        return fail(error, "Could not resolve the Save As parent folder.");
    if (samePath(canonicalSource, canonicalParent) ||
        pathIsWithin(canonicalSource, canonicalParent)) {
        return fail(error, "Save As destination must not resolve inside the current Dataset folder.");
    }

    const QString staging =
        QDir(canonicalParent).filePath("." + destinationInfo.fileName() + ".staging-" +
                                       QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir stagingDir(staging);
    auto cleanup = [&] { stagingDir.removeRecursively(); };

    if (!copyFolder(canonicalSource, canonicalSource, staging, error)) {
        cleanup();
        return false;
    }

    const QString newDatasetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagedManifest = QDir(staging).filePath("dataset.json");
    DatasetManifestData copiedData = data_;
    copiedData.datasetId = newDatasetId;
    copiedData.provenance.updatedAt = nextUpdatedAt(data_.provenance.updatedAt);
    if (!DatasetManifestV2::save(stagedManifest, copiedData, error)) {
        cleanup();
        return false;
    }
    const QString canonicalDestination =
        QDir(canonicalParent).filePath(destinationInfo.fileName());
    if (!QDir().rename(staging, canonicalDestination)) {
        cleanup();
        return fail(error, "Could not publish the new Dataset folder.");
    }

    manifestPath_ = QDir(canonicalDestination).filePath("dataset.json");
    data_ = std::move(copiedData);
    undo_.reset();
    return true;
}

DatasetLabelSnapshot DatasetLabelService::snapshot() const {
    DatasetLabelSnapshot value;
    value.manifestPath = manifestPath_;
    value.datasetId = data_.datasetId;
    value.classes = data_.classes;
    value.counts.classCounts.fill(0, data_.classes.size());
    value.canUndo = undo_.has_value();

    for (const DatasetRecord& record : data_.records) {
        const auto label = std::find_if(data_.labels.cbegin(), data_.labels.cend(), [&](const UserLabelRecord& candidate) {
            return candidate.recordId == record.recordId;
        });
        const UserLabelRecord* labelPointer = label == data_.labels.cend() ? nullptr : &*label;
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
