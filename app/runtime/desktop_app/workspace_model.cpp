#include "workspace_model.h"

#include <QtCore>
#include <QtWidgets>

#include "app_state.h"
#include "model_registry_service.h"
#include "object_names.h"
#include "theme.h"
#include "widget_helpers.h"

#include <QDesktopServices>
#include <QUrl>

#include <initializer_list>
#include <memory>
#include <utility>

namespace desktop_app::workspace {
namespace {

constexpr auto kVerifyAddButtonsEnv = "OVDS_VERIFY_MODEL_WORKSPACE_ADD_BUTTONS";
constexpr auto kVerifyListManagementEnv = "OVDS_VERIFY_MODEL_WORKSPACE_LIST_MANAGEMENT";
constexpr auto kVerifyActiveSimplificationEnv = "OVDS_VERIFY_MODEL_ACTIVE_SIMPLIFICATION";

QString metadataArchitectureSummary(const QJsonObject& metadataDoc);
bool entryIsActive(const QJsonObject& entry);
bool entryIsBlockedFromLiveSorting(const QJsonObject& entry);
bool statusLooksLikeStarter(const QJsonObject& entry, const QJsonObject& metadataDoc);

QString findProjectRootFromApp() {
    QStringList starts;
    starts << QCoreApplication::applicationDirPath() << QDir::currentPath();
    for (const auto& start : starts) {
        QDir dir(start);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists() &&
                (QFileInfo(dir.filePath("app/runtime/models")).isDir() ||
                 QFileInfo(dir.filePath("internal-release/app/runtime/models")).isDir())) {
                return dir.absolutePath();
            }
            if (!dir.cdUp())
                break;
        }
    }
    return QString();
}

QString absoluteRegistryPath(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return trimmed;
    return resolvePackagedPathFromRegistryPath(trimmed);
}

QString fileSizeSummary(const QString& path) {
    QFileInfo info(absoluteRegistryPath(path));
    if (!info.isFile())
        return QString();
    const qint64 bytes = info.size();
    if (bytes >= 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    if (bytes >= 1024) {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    return QString::number(bytes) + " B";
}

QLabel* makeModelFieldLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setProperty("mutedText", true);
    return label;
}

QLabel* makeModelValue(const QString& objectName, bool selectable = false) {
    auto* label = new QLabel("--");
    nameWidget(label, objectName.toUtf8().constData());
    label->setTextInteractionFlags(selectable ? (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)
                                              : Qt::NoTextInteraction);
    if (selectable)
        label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}

QString wrapTechnicalText(QString text) {
    if (text.isEmpty())
        return QString();
    QString escaped = text.toHtmlEscaped();
    escaped.replace("/", "/<br>");
    escaped.replace("\\", "\\<br>");
    escaped.replace("_", "_<br>");
    escaped.replace("-", "-<wbr>");
    QString wrapped;
    int runLength = 0;
    for (int i = 0; i < escaped.size(); ++i) {
        wrapped += escaped.at(i);
        const bool tag = escaped.at(i) == '<';
        if (tag) {
            const int tagEnd = escaped.indexOf('>', i);
            if (tagEnd >= i) {
                wrapped += escaped.mid(i + 1, tagEnd - i);
                i = tagEnd;
                runLength = 0;
                continue;
            }
        }
        if (escaped.at(i).isLetterOrNumber()) {
            ++runLength;
            if (runLength >= 24) {
                wrapped += "<br>";
                runLength = 0;
            }
        } else {
            runLength = 0;
        }
    }
    return wrapped;
}

void addField(QGridLayout* grid, int row, int column, const QString& label, QLabel* value, int columnSpan = 1) {
    grid->addWidget(makeModelFieldLabel(label), row, column * 2);
    grid->addWidget(value, row, column * 2 + 1, 1, columnSpan * 2 - 1);
}

QStringList jsonStringList(const QJsonArray& values) {
    QStringList result;
    for (const auto& value : values)
        result << value.toVariant().toString();
    return result;
}

QString jsonCompact(const QJsonValue& value) {
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return value.toVariant().toString();
}

QString jsonPathSummary(const QJsonValue& value) {
    if (value.isUndefined() || value.isNull())
        return "(none)";
    if (value.isObject()) {
        QStringList lines;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isArray()) {
                const QStringList values = jsonStringList(it.value().toArray());
                lines << QString("%1: %2").arg(it.key(), values.isEmpty() ? QString("(none)") : values.join("; "));
            } else {
                lines << QString("%1: %2").arg(it.key(), jsonCompact(it.value()));
            }
        }
        return lines.isEmpty() ? QString("(none)") : lines.join("\n");
    }
    return jsonCompact(value);
}

QStringList classIdsForEntry(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    const QJsonArray metadataClasses = metadataDoc.value("classes").toArray();
    if (!metadataClasses.isEmpty())
        return jsonStringList(metadataClasses);
    return jsonStringList(entry.value("classes").toArray());
}

QString defaultDisplayLabelForClassId(const QStringList& classIds, const QString& classId) {
    if (classIds == QStringList{"0", "1"}) {
        if (classId == "0")
            return "Non-target";
        if (classId == "1")
            return "Target";
    }
    if (classIds == QStringList{"0", "1", "2"}) {
        if (classId == "0")
            return "Non-target A";
        if (classId == "1")
            return "Target";
        if (classId == "2")
            return "Non-target B";
    }
    return classId;
}

QJsonObject displayLabelsForEntry(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    QJsonObject result = entry.value("display_labels").toObject();
    const QJsonObject metadataLabels = metadataDoc.value("display_labels").toObject();
    for (auto it = metadataLabels.constBegin(); it != metadataLabels.constEnd(); ++it)
        result[it.key()] = it.value();
    return result;
}

QString displayLabelForClassId(const QStringList& classIds, const QJsonObject& displayLabels, const QString& classId) {
    const QString display = displayLabels.value(classId).toString().trimmed();
    return display.isEmpty() ? defaultDisplayLabelForClassId(classIds, classId) : display;
}

QString classesSummary(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    QStringList classLines;
    const QStringList classIds = classIdsForEntry(entry, metadataDoc);
    const QJsonObject displayLabels = displayLabelsForEntry(entry, metadataDoc);
    for (const QString& classId : classIds) {
        const QString displayLabel = displayLabelForClassId(classIds, displayLabels, classId);
        classLines << (displayLabel == classId ? classId : QString("%1 (%2)").arg(displayLabel, classId));
    }
    return classLines.isEmpty() ? QString("--") : classLines.join(", ");
}

QString titleCaseToken(QString value) {
    value = value.trimmed();
    if (value.isEmpty())
        return QString();
    value.replace('_', ' ');
    value.replace('-', ' ');
    value = value.simplified();
    QStringList words = value.split(' ', Qt::SkipEmptyParts);
    for (QString& word : words) {
        if (word.compare("onnx", Qt::CaseInsensitive) == 0) {
            word = "ONNX";
            continue;
        }
        word = word.toLower();
        word[0] = word.at(0).toUpper();
    }
    return words.join(' ');
}

bool entryIsTemplate(const QJsonObject& entry, const QJsonObject& metadataDoc = {}) {
    return registryString(entry, "promotion_status").contains("template", Qt::CaseInsensitive) ||
           registryString(entry, "metadata_status").contains("template", Qt::CaseInsensitive) ||
           metadataDoc.value("status").toString().contains("blank_untrained_template", Qt::CaseInsensitive);
}

QString userFacingModelStatus(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    return statusLooksLikeStarter(entry, metadataDoc) ? QString("Untrained") : QString("Trained");
}

QString userFacingModelListSummary(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    QStringList parts;
    parts << userFacingModelStatus(entry, metadataDoc);
    if (entryIsActive(entry))
        parts << "In use now";
    const QString classifier = metadataArchitectureSummary(metadataDoc);
    if (!classifier.isEmpty())
        parts << classifier;
    return parts.join("  -  ");
}

QString userFacingValidationSummary(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    if (entryIsTemplate(entry, metadataDoc)) {
        return "No validation has been run. This blank template must be trained and validated before live sorting.";
    }
    const QJsonObject validationSummary = metadataDoc.value("validation_summary").toObject();
    if (validationSummary.isEmpty()) {
        const QString fallback = registryString(entry, "validation_status").trimmed();
        return fallback.isEmpty() ? QString("Validation information is not available yet.") : fallback;
    }
    const QJsonObject imageValidation = validationSummary.value("image_validation").toObject();
    const QJsonObject sequenceValidation = validationSummary.value("sequence_validation").toObject();
    QStringList lines;
    const QString imageStatus = imageValidation.value("status").toString().trimmed();
    if (imageStatus.isEmpty() || imageStatus.compare("not_run", Qt::CaseInsensitive) == 0) {
        lines << "Image validation has not been run yet.";
    } else {
        QString imageLine = "Image validation completed";
        QStringList metrics;
        if (imageValidation.value("accuracy").isDouble()) {
            metrics << QString::number(imageValidation.value("accuracy").toDouble() * 100.0, 'f', 1) + "% accuracy";
        }
        if (imageValidation.value("macro_f1").isDouble()) {
            metrics << "macro F1 " + QString::number(imageValidation.value("macro_f1").toDouble(), 'f', 3);
        }
        if (!metrics.isEmpty())
            imageLine += " with " + metrics.join(" and ");
        imageLine += ".";
        const QString readableImageStatus = titleCaseToken(imageStatus);
        if (!readableImageStatus.isEmpty())
            imageLine += " Status: " + readableImageStatus + ".";
        lines << imageLine;
    }
    const QString sequenceStatus = sequenceValidation.value("status").toString().trimmed();
    if (sequenceStatus.compare("not_run", Qt::CaseInsensitive) == 0) {
        lines << "Sequence validation has not been run yet.";
    } else if (sequenceStatus.compare("not_available", Qt::CaseInsensitive) == 0) {
        lines << "Sequence validation is not available for this model yet.";
    } else if (!sequenceStatus.isEmpty()) {
        lines << "Sequence validation status: " + titleCaseToken(sequenceStatus) + ".";
    }
    if (lines.isEmpty()) {
        const QString fallback = registryString(entry, "validation_status").trimmed();
        return fallback.isEmpty() ? QString("Validation information is not available yet.") : fallback;
    }
    return lines.join(" ");
}

QJsonObject loadMetadataDoc(const QJsonObject& entry) {
    const QString absolutePath = absoluteRegistryPath(registryString(entry, "metadata_path"));
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

QString inputSizeSummary(const QJsonObject& metadataDoc) {
    const QString value = jsonStringList(metadataDoc.value("input_size").toArray()).join(" x ");
    return value.isEmpty() ? "--" : value;
}

QString metadataArchitectureSummary(const QJsonObject& metadataDoc) {
    const QJsonObject architecture = metadataDoc.value("architecture").toObject();
    QStringList parts;
    if (!architecture.value("family").toString().isEmpty())
        parts << architecture.value("family").toString();
    if (!architecture.value("variant").toString().isEmpty())
        parts << architecture.value("variant").toString();
    const QString format = metadataDoc.value("export").toObject().value("format").toString();
    if (!format.isEmpty())
        parts << format.toUpper();
    return parts.isEmpty() ? QString("ONNX classifier") : parts.join(" / ");
}

bool entryIsActive(const QJsonObject& entry) {
    return entry.value("selectable_for_normal_live_sorting").toBool(false) ||
           registryString(entry, "state").contains("promoted", Qt::CaseInsensitive) ||
           registryString(entry, "promotion_status").contains("current", Qt::CaseInsensitive);
}

QString displayNameForEntry(const QJsonObject& entry) {
    const QString displayName = registryString(entry, "display_name");
    return displayName.isEmpty() ? registryString(entry, "registry_entry_id") : displayName;
}

QString simpleListTitleForEntry(const QJsonObject& entry) {
    QString title = displayNameForEntry(entry).trimmed();
    if (title.endsWith(".onnx", Qt::CaseInsensitive) && !title.contains('/') && !title.contains('\\'))
        title = QFileInfo(title).completeBaseName();
    return title.isEmpty() ? QString("Untitled model") : title;
}

bool stringContainsAny(QString value, std::initializer_list<const char*> needles) {
    value = value.trimmed();
    for (const char* needle : needles) {
        if (value.contains(QString::fromUtf8(needle), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool statusLooksLikeStarter(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    const QString metadataStatus = metadataDoc.value("status").toString().trimmed();
    const QString registryValidation = registryString(entry, "validation_status").trimmed();
    const QString metadataSummary = registryString(entry, "metadata_status").trimmed();
    const QString blockers = jsonCompact(entry.value("blockers"));
    return entryIsTemplate(entry, metadataDoc) || entryIsBlockedFromLiveSorting(entry) ||
           stringContainsAny(metadataStatus, {"transfer_start", "untrained", "template"}) ||
           stringContainsAny(registryValidation, {"starter"}) || stringContainsAny(metadataSummary, {"starter"}) ||
           stringContainsAny(blockers, {"training starter"});
}

bool validationSummaryLooksPositive(const QJsonObject& summary) {
    const QString status = summary.value("status").toString().trimmed();
    if (status.isEmpty())
        return false;
    if (stringContainsAny(status, {"not_run", "not_available", "missing", "failed", "error"}))
        return false;
    if (summary.value("accuracy").isDouble() || summary.value("macro_f1").isDouble())
        return true;
    return stringContainsAny(status, {"pass", "validated", "complete", "completed", "provisional", "approved"});
}

bool entryHasValidationPass(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    const QString validationStatus = registryString(entry, "validation_status").trimmed();
    if (!validationStatus.isEmpty()) {
        if (stringContainsAny(validationStatus, {"not validated", "no validation", "missing", "starter only"})) {
            return false;
        }
        if (stringContainsAny(validationStatus, {"pass", "validated", "provisional", "approved"}))
            return true;
    }

    const QJsonObject validationSummary = metadataDoc.value("validation_summary").toObject();
    if (validationSummaryLooksPositive(validationSummary.value("image_validation").toObject()))
        return true;
    if (validationSummaryLooksPositive(validationSummary.value("sequence_validation").toObject()))
        return true;
    return !entry.value("validation_evidence").toObject().isEmpty() &&
           stringContainsAny(validationStatus, {"hash", "pass", "validated", "provisional"});
}

QString modelListStatusLabel(const QJsonObject& entry, const QJsonObject& metadataDoc) {
    if (statusLooksLikeStarter(entry, metadataDoc))
        return "Untrained";
    return "Trained";
}

QString formattedUserFacingDate(const QString& rawValue) {
    const QString value = rawValue.trimmed();
    if (value.isEmpty())
        return QString();

    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid())
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (dateTime.isValid())
        return dateTime.date().toString("MMM d, yyyy");

    const QDate date = QDate::fromString(value, Qt::ISODate);
    if (date.isValid())
        return date.toString("MMM d, yyyy");

    return QString();
}

QString firstFormattedDateCandidate(const QJsonObject& object, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const QString formatted = formattedUserFacingDate(object.value(QString::fromUtf8(key)).toString());
        if (!formatted.isEmpty())
            return formatted;
    }
    return QString();
}

QString registryUpdatedAtLabel(const QString& registryFilePath) {
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return QString();
    return formattedUserFacingDate(doc.object().value("updated_at").toString());
}

QString modelListDateLabel(const QJsonObject& entry, const QJsonObject& metadataDoc, const QString& registryFilePath) {
    const QJsonObject validationSummary = metadataDoc.value("validation_summary").toObject();
    const QString validationDate =
        firstFormattedDateCandidate(validationSummary.value("image_validation").toObject(),
                                    {"validated_at", "completed_at", "finished_at", "run_at", "timestamp", "date"});
    if (!validationDate.isEmpty())
        return validationDate;

    const QString sequenceValidationDate =
        firstFormattedDateCandidate(validationSummary.value("sequence_validation").toObject(),
                                    {"validated_at", "completed_at", "finished_at", "run_at", "timestamp", "date"});
    if (!sequenceValidationDate.isEmpty())
        return sequenceValidationDate;

    const QString trainingDate =
        firstFormattedDateCandidate(metadataDoc.value("training_summary").toObject(),
                                    {"trained_at", "completed_at", "finished_at", "run_at", "timestamp", "date"});
    if (!trainingDate.isEmpty())
        return trainingDate;

    const QString metadataDate =
        firstFormattedDateCandidate(metadataDoc, {"created_at", "trained_at", "updated_at", "date", "version"});
    if (!metadataDate.isEmpty())
        return metadataDate;

    const QString entryDate = firstFormattedDateCandidate(entry, {"created_at", "updated_at", "date"});
    if (!entryDate.isEmpty())
        return entryDate;

    const QString registryDate = registryUpdatedAtLabel(registryFilePath);
    return registryDate.isEmpty() ? QString("No date") : registryDate;
}

QString modelListRowText(const QJsonObject& entry, const QJsonObject& metadataDoc, const QString& registryFilePath) {
    return simpleListTitleForEntry(entry) + "\n" + modelListStatusLabel(entry, metadataDoc) + "  -  " +
           modelListDateLabel(entry, metadataDoc, registryFilePath);
}

bool envFlagEnabled(const char* name) {
    const QString value = qEnvironmentVariable(name).trimmed();
    if (value.isEmpty())
        return false;
    return value.compare("0", Qt::CaseInsensitive) != 0 && value.compare("false", Qt::CaseInsensitive) != 0 &&
           value.compare("no", Qt::CaseInsensitive) != 0;
}

QJsonArray loadRegistryEntriesFromPath(const QString& registryFilePath, QString* warning) {
    if (warning)
        warning->clear();
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warning)
            *warning = "Registry file not readable: " + registryFilePath;
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (warning)
            *warning = "Registry parse failed: " + parseError.errorString();
        return {};
    }
    return doc.object().value("entries").toArray();
}

bool saveRegistryEntriesToPath(const QString& registryFilePath, const QJsonArray& entries, QString* error) {
    if (error)
        error->clear();
    if (registryFilePath.trimmed().isEmpty()) {
        if (error)
            *error = "No registry file path is available.";
        return false;
    }
    QJsonObject registry;
    QFile existing(registryFilePath);
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll());
        if (doc.isObject())
            registry = doc.object();
    }
    if (registry.value("schema_version").toString().isEmpty())
        registry["schema_version"] = "model-registry-v1";
    registry["entries"] = entries;
    QFile file(registryFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error)
            *error = "Registry file not writable: " + registryFilePath;
        return false;
    }
    file.write(QJsonDocument(registry).toJson(QJsonDocument::Indented));
    return true;
}

bool registryEntriesMatchPackagedEntry(const QJsonObject& existingEntry, const QJsonObject& packagedEntry) {
    const QString existingRegistryId = registryString(existingEntry, "registry_entry_id").trimmed();
    const QString packagedRegistryId = registryString(packagedEntry, "registry_entry_id").trimmed();
    if (!existingRegistryId.isEmpty() && existingRegistryId.compare(packagedRegistryId, Qt::CaseInsensitive) == 0)
        return true;

    const QString existingModelId = registryString(existingEntry, "model_id").trimmed();
    const QString packagedModelId = registryString(packagedEntry, "model_id").trimmed();
    if (!existingModelId.isEmpty() && !packagedModelId.isEmpty() &&
        existingModelId.compare(packagedModelId, Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString existingModelPath = QDir::cleanPath(absoluteRegistryPath(registryString(existingEntry, "model_path")));
    const QString packagedModelPath = QDir::cleanPath(absoluteRegistryPath(registryString(packagedEntry, "model_path")));
    if (!existingModelPath.isEmpty() && !packagedModelPath.isEmpty() &&
        existingModelPath.compare(packagedModelPath, Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString existingMetadataPath =
        QDir::cleanPath(absoluteRegistryPath(registryString(existingEntry, "metadata_path")));
    const QString packagedMetadataPath =
        QDir::cleanPath(absoluteRegistryPath(registryString(packagedEntry, "metadata_path")));
    return !existingMetadataPath.isEmpty() && !packagedMetadataPath.isEmpty() &&
           existingMetadataPath.compare(packagedMetadataPath, Qt::CaseInsensitive) == 0;
}

int findRegistryEntryRow(const QJsonArray& entries, const QJsonObject& packagedEntry) {
    for (int i = 0; i < entries.size(); ++i) {
        if (registryEntriesMatchPackagedEntry(entries.at(i).toObject(), packagedEntry))
            return i;
    }
    return -1;
}

QString packagedEntryAvailabilityError(const QJsonObject& entry) {
    const QString modelPath = absoluteRegistryPath(registryString(entry, "model_path"));
    if (!QFileInfo(modelPath).isFile()) {
        return QString("Packaged model asset is missing: %1").arg(QDir::toNativeSeparators(modelPath));
    }

    const QString metadataPath = absoluteRegistryPath(registryString(entry, "metadata_path"));
    if (!metadataPath.trimmed().isEmpty() && !QFileInfo(metadataPath).isFile()) {
        return QString("Packaged model metadata is missing: %1").arg(QDir::toNativeSeparators(metadataPath));
    }

    return QString();
}

QString registryEntryId(const QJsonObject& entry) {
    return registryString(entry, "registry_entry_id").trimmed();
}

int findRegistryEntryRowById(const QJsonArray& entries, const QString& entryId) {
    if (entryId.trimmed().isEmpty())
        return -1;
    for (int i = 0; i < entries.size(); ++i) {
        if (registryEntryId(entries.at(i).toObject()).compare(entryId, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

bool entryIsBlockedFromLiveSorting(const QJsonObject& entry) {
    return registryString(entry, "live_use_mode").contains("blocked", Qt::CaseInsensitive);
}

QString sourceRegistryEntryId(const QJsonObject& entry) {
    const QString sourceId = registryString(entry, "source_registry_entry_id").trimmed();
    return sourceId.isEmpty() ? registryEntryId(entry) : sourceId;
}

int nextUserAddedOrdinal(const QJsonArray& entries, const QJsonObject& sourceEntry) {
    const QString sourceId = registryEntryId(sourceEntry);
    int maxOrdinal = 0;
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (sourceRegistryEntryId(entry).compare(sourceId, Qt::CaseInsensitive) != 0)
            continue;
        const int ordinal = entry.value("user_added_ordinal").toInt();
        if (ordinal > maxOrdinal)
            maxOrdinal = ordinal;
    }
    return maxOrdinal + 1;
}

QJsonObject makeUserAddedRegistryEntry(const QJsonObject& sourceEntry, const QJsonArray& existingEntries) {
    const int ordinal = nextUserAddedOrdinal(existingEntries, sourceEntry);
    QJsonObject entry = sourceEntry;
    const QString sourceId = registryEntryId(sourceEntry);
    const QString displayName = displayNameForEntry(sourceEntry);
    entry["registry_entry_id"] = QString("%1__user_%2").arg(sourceId).arg(ordinal, 3, 10, QChar('0'));
    entry["display_name"] = QString("%1 (added %2)").arg(displayName).arg(ordinal);
    entry["source_registry_entry_id"] = sourceId;
    entry["user_added"] = true;
    entry["user_added_ordinal"] = ordinal;
    entry["selectable_for_normal_live_sorting"] = false;
    entry["state"] = "available";
    if (!entryIsBlockedFromLiveSorting(entry))
        entry["promotion_status"] = "Available";
    return entry;
}

QString removalBlockedReason(const QJsonObject& entry, int registryCount) {
    if (registryCount <= 1)
        return "At least one model must remain in the Model workspace list.";
    if (entryIsActive(entry))
        return "The promoted/current live model cannot be removed while it is active.";
    return QString();
}

QString firstExistingEvidencePath(const QJsonObject& entry) {
    const QJsonValue evidenceValue = entry.value("validation_evidence");
    QStringList candidates;
    if (evidenceValue.isObject()) {
        const QJsonObject evidence = evidenceValue.toObject();
        for (auto it = evidence.constBegin(); it != evidence.constEnd(); ++it) {
            if (it.value().isString())
                candidates << it.value().toString();
            if (it.value().isArray()) {
                for (const auto& value : it.value().toArray()) {
                    if (value.isString())
                        candidates << value.toString();
                }
            }
        }
    } else if (evidenceValue.isString()) {
        candidates << evidenceValue.toString();
    }
    const QString promotion = registryString(entry, "promotion_record_path");
    if (!promotion.isEmpty())
        candidates << promotion;
    for (const auto& candidate : candidates) {
        const QString absolute = absoluteRegistryPath(candidate);
        if (QFileInfo(absolute).exists())
            return absolute;
    }
    return {};
}

void openPathOrWarn(QWidget* parent, const QString& path, const QString& label) {
    const QString absolute = absoluteRegistryPath(path);
    if (!absolute.isEmpty() && QFileInfo(absolute).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
        return;
    }
    QMessageBox::information(parent, label, label + " is not available for the selected model.");
}

QPushButton* makeSmallButton(const QString& text, const char* objectName) {
    auto* button = new QPushButton(text);
    nameWidget(button, objectName);
    button->setMinimumHeight(26);
    return button;
}

QString canonicalTargetClassId(QString value) {
    value = value.trimmed();
    if (value.compare("Empty", Qt::CaseInsensitive) == 0)
        return "0";
    if (value.compare("Single", Qt::CaseInsensitive) == 0)
        return "1";
    if (value.compare("MoreThanTwo", Qt::CaseInsensitive) == 0 ||
        value.compare("More than two", Qt::CaseInsensitive) == 0) {
        return "2";
    }
    return value.isEmpty() ? QString("1") : value;
}

} // namespace

QWidget* buildModelWorkspace(const ModelWorkspaceControls& controls) {
    using desktop_app::ui::makeMetric;
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;

    const QJsonObject packagedBlankEntry = packagedBlankModelRegistryEntry();
    const QJsonObject packagedPretrainedEntry = packagedPretrainedModelRegistryEntry();
    auto registryEntries = std::make_shared<QJsonArray>(controls.registryEntries);
    auto modelWorkspacePage = new QWidget;
    nameWidget(modelWorkspacePage, "ModelWorkspace");
    auto modelWorkspaceLayout = new QHBoxLayout;
    modelWorkspaceLayout->setContentsMargins(10, 10, 10, 10);
    modelWorkspaceLayout->setSpacing(12);

    auto modelWorkspaceRegistryPanel = makePanel("Models");
    modelWorkspaceRegistryPanel->setObjectName("ModelRegistryPanel");
    modelWorkspaceRegistryPanel->setMinimumWidth(300);
    auto modelRegistryBody = makePanelBody(modelWorkspaceRegistryPanel, 0, 0, 0, 0);
    auto* registryHeaderActions = new QWidget;
    auto* registryHeaderLayout = new QGridLayout;
    registryHeaderLayout->setContentsMargins(12, 10, 12, 8);
    registryHeaderLayout->setSpacing(8);
    auto* addBlankModelButton = makeSmallButton("Add blank model", "ModelWorkspaceAddBlankModelButton");
    auto* addPretrainedModelButton = makeSmallButton("Add pre-trained model", "ModelWorkspaceAddPretrainedModelButton");
    auto* addModelButton = makeSmallButton("Import ONNX", "ModelWorkspaceAddModelButton");
    auto* removeModelButton = makeSmallButton("Remove model", "ModelWorkspaceRemoveModelButton");
    auto* refreshModelsButton = makeSmallButton("Refresh", "ModelWorkspaceRefreshButton");
    registryHeaderLayout->addWidget(addBlankModelButton, 0, 0);
    registryHeaderLayout->addWidget(addPretrainedModelButton, 0, 1);
    registryHeaderLayout->addWidget(addModelButton, 1, 0);
    registryHeaderLayout->addWidget(refreshModelsButton, 1, 1);
    registryHeaderLayout->addWidget(removeModelButton, 2, 0, 1, 2);
    registryHeaderLayout->setColumnStretch(0, 1);
    registryHeaderLayout->setColumnStretch(1, 1);
    registryHeaderActions->setLayout(registryHeaderLayout);
    modelRegistryBody->addWidget(registryHeaderActions);

    auto modelRegistryList = new QTableWidget(registryEntries->size(), 1);
    nameWidget(modelRegistryList, "ModelWorkspaceRegistryTable");
    modelRegistryList->setHorizontalHeaderLabels({"Model"});
    modelRegistryList->horizontalHeader()->setVisible(false);
    modelRegistryList->horizontalHeader()->setStretchLastSection(true);
    modelRegistryList->verticalHeader()->setVisible(false);
    modelRegistryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modelRegistryList->setSelectionBehavior(QAbstractItemView::SelectRows);
    modelRegistryList->setSelectionMode(QAbstractItemView::SingleSelection);
    modelRegistryList->setAlternatingRowColors(false);
    modelRegistryList->setShowGrid(false);
    modelRegistryList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto populateRegistryList = [=]() {
        modelRegistryList->setRowCount(registryEntries->size());
        for (int i = 0; i < registryEntries->size(); ++i) {
            const QJsonObject entry = registryEntries->at(i).toObject();
            const QJsonObject metadataDoc = loadMetadataDoc(entry);
            const QString rowText = modelListRowText(entry, metadataDoc, controls.registryFilePath);
            auto* item = new QTableWidgetItem(rowText);
            item->setToolTip(rowText);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            if (entryIsTemplate(entry, metadataDoc) ||
                registryString(entry, "live_use_mode").contains("blocked", Qt::CaseInsensitive)) {
                item->setForeground(QBrush(QColor(Qt::gray)));
            }
            modelRegistryList->setItem(i, 0, item);
            modelRegistryList->setRowHeight(i, 64);
        }
    };
    populateRegistryList();
    modelRegistryBody->addWidget(modelRegistryList, 1);
    auto modelDetailScroll = new QScrollArea;
    nameWidget(modelDetailScroll, "ModelWorkspaceDetailScrollArea");
    modelDetailScroll->setWidgetResizable(true);
    modelDetailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto modelDetailStack = new QWidget;
    nameWidget(modelDetailStack, "ModelWorkspaceDetailStack");
    auto modelDetailLayout = new QVBoxLayout;
    modelDetailLayout->setContentsMargins(0, 0, 2, 0);
    modelDetailLayout->setSpacing(12);

    auto modelOverviewPanel = makePanel("Model detail", "ONNX classifier");
    modelOverviewPanel->setObjectName("ModelOverviewPanel");
    auto modelOverviewBody = makePanelBody(modelOverviewPanel);
    auto modelOverviewGrid = new QGridLayout;
    modelOverviewGrid->setContentsMargins(0, 0, 0, 0);
    modelOverviewGrid->setHorizontalSpacing(10);
    modelOverviewGrid->setVerticalSpacing(8);
    auto modelNameValue = makeModelValue("ModelWorkspaceNameValue");
    auto architectureValue = makeModelValue("ModelWorkspaceArchitectureValue");
    auto modelStateValue = makeModelValue("ModelWorkspaceStateValue");
    auto modelClassesValue = makeModelValue("ModelWorkspaceClassesValue");
    addField(modelOverviewGrid, 0, 0, "Model", modelNameValue);
    addField(modelOverviewGrid, 1, 0, "Classifier", architectureValue);
    addField(modelOverviewGrid, 2, 0, "Status", modelStateValue);
    addField(modelOverviewGrid, 3, 0, "Output classes", modelClassesValue);
    modelOverviewGrid->setColumnStretch(1, 1);
    modelOverviewGrid->setColumnStretch(3, 1);
    modelOverviewBody->addLayout(modelOverviewGrid);
    auto* overviewActions = new QHBoxLayout;
    overviewActions->setContentsMargins(0, 0, 0, 0);
    overviewActions->setSpacing(8);
    auto* setActiveButton = makeSmallButton("Set Active", "ModelWorkspaceSetActiveButton");
    auto* modelActionsButton = makeSmallButton("More", "ModelWorkspaceActionsButton");
    modelActionsButton->setToolTip("More model actions");
    overviewActions->addWidget(setActiveButton, 0);
    overviewActions->addWidget(modelActionsButton, 0);
    overviewActions->addStretch(1);
    modelOverviewBody->addLayout(overviewActions);

    auto modelTechnicalPanel = makePanel("Technical details");
    modelTechnicalPanel->setObjectName("ModelTechnicalDetailsPanel");
    auto modelTechnicalBody = makePanelBody(modelTechnicalPanel);
    auto modelTechnicalGrid = new QGridLayout;
    modelTechnicalGrid->setContentsMargins(0, 0, 0, 0);
    modelTechnicalGrid->setHorizontalSpacing(10);
    modelTechnicalGrid->setVerticalSpacing(8);
    auto registryIdValue = makeModelValue("ModelWorkspaceRegistryIdValue", true);
    auto modelFileSizeValue = makeModelValue("ModelWorkspaceFileSizeValue");
    auto modelPathValue = makeModelValue("ModelWorkspacePathValue", true);
    auto modelShaValue = makeModelValue("ModelWorkspaceShaValue", true);
    auto metadataPathValue = makeModelValue("ModelWorkspaceMetadataPathValue", true);
    auto metadataShaValue = makeModelValue("ModelWorkspaceMetadataShaValue", true);
    addField(modelTechnicalGrid, 0, 0, "Registry ID", registryIdValue);
    addField(modelTechnicalGrid, 1, 0, "File size", modelFileSizeValue);
    addField(modelTechnicalGrid, 2, 0, "Model path", modelPathValue);
    addField(modelTechnicalGrid, 3, 0, "ONNX SHA-256", modelShaValue);
    addField(modelTechnicalGrid, 4, 0, "Metadata path", metadataPathValue);
    addField(modelTechnicalGrid, 5, 0, "Metadata SHA-256", metadataShaValue);
    modelTechnicalGrid->setColumnStretch(1, 1);
    modelTechnicalBody->addLayout(modelTechnicalGrid);

    auto modelMetadataPanel = makePanel("Advanced metadata");
    modelMetadataPanel->setObjectName("ModelMetadataNormalizationPanel");
    auto modelMetadataBody = makePanelBody(modelMetadataPanel);
    auto modelMetadataGrid = new QGridLayout;
    modelMetadataGrid->setContentsMargins(0, 0, 0, 0);
    modelMetadataGrid->setHorizontalSpacing(10);
    modelMetadataGrid->setVerticalSpacing(8);
    auto metadataSchemaValue = makeModelValue("ModelWorkspaceMetadataSchemaValue");
    auto labelSchemaValue = makeModelValue("ModelWorkspaceLabelSchemaValue");
    auto inputSizeValue = makeModelValue("ModelWorkspaceInputSizeValue");
    auto normalizationMeanValue = makeModelValue("ModelWorkspaceNormalizationMeanValue");
    auto normalizationStdValue = makeModelValue("ModelWorkspaceNormalizationStdValue");
    addField(modelMetadataGrid, 0, 0, "Metadata status", metadataSchemaValue);
    addField(modelMetadataGrid, 1, 0, "Label schema", labelSchemaValue);
    addField(modelMetadataGrid, 2, 0, "Input size", inputSizeValue);
    addField(modelMetadataGrid, 3, 0, "Mean", normalizationMeanValue);
    addField(modelMetadataGrid, 4, 0, "Std", normalizationStdValue);
    modelMetadataGrid->setColumnStretch(1, 1);
    modelMetadataGrid->setColumnStretch(3, 1);
    modelMetadataBody->addLayout(modelMetadataGrid);
    auto* metadataActions = new QHBoxLayout;
    metadataActions->setContentsMargins(0, 0, 0, 0);
    metadataActions->setSpacing(8);
    auto* openMetadataButton = makeSmallButton("Open Metadata", "ModelWorkspaceOpenMetadataButton");
    auto* reloadMetadataButton = makeSmallButton("Reload Metadata", "ModelWorkspaceReloadMetadataButton");
    metadataActions->addWidget(openMetadataButton, 0);
    metadataActions->addWidget(reloadMetadataButton, 0);
    metadataActions->addStretch(1);
    modelMetadataBody->addLayout(metadataActions);

    auto modelValidationPanel = makePanel("Validation result");
    modelValidationPanel->setObjectName("ModelValidationPanel");
    auto modelValidationBody = makePanelBody(modelValidationPanel);
    auto validationStatusValue = makeModelValue("ModelWorkspaceValidationStatusValue");
    modelValidationBody->addWidget(validationStatusValue);
    auto validationMetricsGrid = new QGridLayout;
    validationMetricsGrid->setContentsMargins(0, 0, 0, 0);
    validationMetricsGrid->setHorizontalSpacing(1);
    validationMetricsGrid->setVerticalSpacing(1);
    auto accuracyValue = new QLabel("--");
    auto macroF1Value = new QLabel("--");
    auto sequenceValue = new QLabel("--");
    auto lossValue = new QLabel("--");
    validationMetricsGrid->addWidget(makeMetric("Accuracy", accuracyValue), 0, 0);
    validationMetricsGrid->addWidget(makeMetric("Macro F1", macroF1Value), 0, 1);
    validationMetricsGrid->addWidget(makeMetric("Sequence", sequenceValue), 0, 2);
    validationMetricsGrid->addWidget(makeMetric("Loss", lossValue), 0, 3);
    modelValidationBody->addLayout(validationMetricsGrid);
    auto modelPromotionPanel = makePanel("Notes and history");
    modelPromotionPanel->setObjectName("ModelPromotionPolicyPanel");
    auto modelPromotionBody = makePanelBody(modelPromotionPanel);
    auto validationEvidenceValue = makeModelValue("ModelWorkspaceValidationEvidenceValue", true);
    validationEvidenceValue->setProperty("mutedText", true);
    validationEvidenceValue->setMaximumHeight(84);
    auto promotionStatusValue = makeModelValue("ModelWorkspacePromotionStatusValue");
    auto promotionRecordValue = makeModelValue("ModelWorkspacePromotionRecordValue", true);
    auto limitationsValue = makeModelValue("ModelWorkspaceLimitationsValue");
    auto blockersValue = makeModelValue("ModelWorkspaceBlockersValue", true);
    modelPromotionBody->addWidget(validationEvidenceValue);
    modelPromotionBody->addWidget(promotionStatusValue);
    modelPromotionBody->addWidget(promotionRecordValue);
    modelPromotionBody->addWidget(limitationsValue);
    modelPromotionBody->addWidget(blockersValue);

    auto* benchmarkBody = new QWidget;
    auto* benchmarkLayout = new QGridLayout;
    benchmarkLayout->setContentsMargins(0, 0, 0, 0);
    benchmarkLayout->setHorizontalSpacing(1);
    benchmarkLayout->setVerticalSpacing(1);
    auto* meanLatencyValue = new QLabel("--");
    auto* p99LatencyValue = new QLabel("--");
    auto* throughputValue = new QLabel("--");
    auto* benchmarkAccuracyValue = new QLabel("--");
    benchmarkLayout->addWidget(makeMetric("Mean latency", meanLatencyValue), 0, 0);
    benchmarkLayout->addWidget(makeMetric("P99", p99LatencyValue), 0, 1);
    benchmarkLayout->addWidget(makeMetric("Throughput", throughputValue), 0, 2);
    benchmarkLayout->addWidget(makeMetric("Top-1 accuracy", benchmarkAccuracyValue), 0, 3);
    benchmarkBody->setLayout(benchmarkLayout);
    auto* benchmarkDisclosure = desktop_app::ui::makeCollapsedGroup("Inference benchmark", benchmarkBody);
    benchmarkDisclosure->setObjectName("ModelBenchmarkDisclosure");

    auto* advancedDetailsBody = new QWidget;
    auto* advancedDetailsLayout = new QVBoxLayout;
    advancedDetailsLayout->setContentsMargins(0, 0, 0, 0);
    advancedDetailsLayout->setSpacing(12);
    advancedDetailsLayout->addWidget(modelTechnicalPanel);
    advancedDetailsLayout->addWidget(modelMetadataPanel);
    advancedDetailsLayout->addWidget(modelPromotionPanel);
    advancedDetailsLayout->addWidget(benchmarkDisclosure);
    advancedDetailsBody->setLayout(advancedDetailsLayout);
    auto* advancedDetailsDisclosure =
        desktop_app::ui::makeCollapsedGroup("Advanced details", advancedDetailsBody);
    advancedDetailsDisclosure->setObjectName("ModelAdvancedDetailsDisclosure");

    modelDetailLayout->addWidget(modelOverviewPanel);
    modelDetailLayout->addWidget(modelValidationPanel);
    modelDetailLayout->addWidget(advancedDetailsDisclosure);
    modelDetailLayout->addStretch(1);
    modelDetailStack->setLayout(modelDetailLayout);
    modelDetailScroll->setWidget(modelDetailStack);
    modelDetailScroll->setMinimumWidth(520);

    auto selectedEntry = [=]() -> QJsonObject {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            row = 0;
        if (row < 0 || row >= registryEntries->size())
            return {};
        return registryEntries->at(row).toObject();
    };

    auto selectRegistryRow = [=](const QString& entryId, int fallbackRow = 0) {
        int row = findRegistryEntryRowById(*registryEntries, entryId);
        if (row < 0 && !registryEntries->isEmpty())
            row = qBound(0, fallbackRow, registryEntries->size() - 1);
        if (row >= 0)
            modelRegistryList->selectRow(row);
    };

    auto updateModelWorkspaceDetails = [=]() {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            row = 0;
        if (row < 0 || row >= registryEntries->size())
            return;
        const QJsonObject entry = registryEntries->at(row).toObject();
        const QJsonObject metadataDoc = loadMetadataDoc(entry);
        const QString modelName = registryString(entry, "display_name").isEmpty()
                                      ? registryString(entry, "registry_entry_id")
                                      : registryString(entry, "display_name");
        modelNameValue->setText(modelName);
        registryIdValue->setText(wrapTechnicalText(registryString(entry, "registry_entry_id")));
        architectureValue->setText(metadataArchitectureSummary(metadataDoc));
        modelStateValue->setText(userFacingModelStatus(entry, metadataDoc));
        const QString modelFileSize = fileSizeSummary(registryString(entry, "model_path"));
        modelFileSizeValue->setText(modelFileSize.isEmpty() ? "(unavailable)" : modelFileSize);
        modelPathValue->setText(wrapTechnicalText(registryString(entry, "model_path")));
        const QString modelSha =
            registryString(entry, "model_sha256").isEmpty() ? "(not recorded)" : registryString(entry, "model_sha256");
        modelShaValue->setText(wrapTechnicalText(modelSha));
        metadataPathValue->setText(wrapTechnicalText(registryString(entry, "metadata_path")));
        const QString metadataSha = registryString(entry, "metadata_sha256").isEmpty()
                                        ? "(not recorded)"
                                        : registryString(entry, "metadata_sha256");
        metadataShaValue->setText(wrapTechnicalText(metadataSha));
        modelClassesValue->setText(classesSummary(entry, metadataDoc));
        const QString metadataStatus = registryString(entry, "metadata_status");
        metadataSchemaValue->setText(metadataStatus.isEmpty()
                                         ? "(unknown)"
                                         : metadataStatus + " / " + registryString(entry, "metadata_schema_version"));
        labelSchemaValue->setText(registryString(entry, "label_schema_version").isEmpty()
                                      ? "(unknown)"
                                      : registryString(entry, "label_schema_version"));
        inputSizeValue->setText(inputSizeSummary(metadataDoc));
        const QJsonObject normalization = metadataDoc.value("normalization").toObject();
        const QString mean = jsonStringList(normalization.value("mean").toArray()).join(", ");
        const QString std = jsonStringList(normalization.value("std").toArray()).join(", ");
        normalizationMeanValue->setText(mean.isEmpty() ? "--" : mean);
        normalizationStdValue->setText(std.isEmpty() ? "--" : std);
        const bool blockedFromLiveSorting = entryIsBlockedFromLiveSorting(entry);
        const ActiveModelReadiness activationReadiness = evaluateActiveModelReadiness(entry);
        setActiveButton->setEnabled(!entryIsActive(entry) && !blockedFromLiveSorting);
        if (entryIsActive(entry)) {
            setActiveButton->setToolTip("Selected model is already active.");
        } else if (blockedFromLiveSorting) {
            setActiveButton->setToolTip("This starter model is blocked from live sorting until it is trained.");
        } else if (!activationReadiness.ready) {
            setActiveButton->setToolTip(activationReadiness.message);
        } else {
            setActiveButton->setToolTip("Mark the selected model as active.");
        }
        const QString removeReason = removalBlockedReason(entry, registryEntries->size());
        removeModelButton->setEnabled(removeReason.isEmpty());
        removeModelButton->setToolTip(removeReason.isEmpty() ? "Remove the selected model row from the workspace list."
                                                             : removeReason);
        validationStatusValue->setText(userFacingValidationSummary(entry, metadataDoc));
        const QJsonObject validationSummary = metadataDoc.value("validation_summary").toObject();
        const QJsonObject imageValidation = validationSummary.value("image_validation").toObject();
        const QJsonObject sequenceValidation = validationSummary.value("sequence_validation").toObject();
        const auto percent = [](const QJsonValue& value) {
            return value.isDouble() ? QString::number(value.toDouble() * 100.0, 'f', 1) + "%" : QString("--");
        };
        const auto decimal = [](const QJsonValue& value) {
            return value.isDouble() ? QString::number(value.toDouble(), 'f', 3) : QString("--");
        };
        accuracyValue->setText(percent(imageValidation.value("accuracy")));
        macroF1Value->setText(decimal(imageValidation.value("macro_f1")));
        const QString sequenceStatus = titleCaseToken(sequenceValidation.value("status").toString());
        sequenceValue->setText(sequenceStatus.isEmpty() ? QString("--") : sequenceStatus);
        lossValue->setText(decimal(imageValidation.value("loss")));
        const QJsonObject benchmark = metadataDoc.value("benchmark").toObject();
        const QJsonObject inferenceBenchmark = metadataDoc.value("inference_benchmark").toObject();
        meanLatencyValue->setText(decimal(benchmark.value("mean_latency_ms").isUndefined()
                                              ? inferenceBenchmark.value("mean_latency_ms")
                                              : benchmark.value("mean_latency_ms")));
        p99LatencyValue->setText(decimal(benchmark.value("p99_latency_ms").isUndefined()
                                             ? imageValidation.value("p99_latency_ms")
                                             : benchmark.value("p99_latency_ms")));
        const QJsonValue throughput = benchmark.value("throughput_fps").isUndefined()
                                          ? inferenceBenchmark.value("throughput_fps")
                                          : benchmark.value("throughput_fps");
        throughputValue->setText(throughput.isDouble() ? QString::number(throughput.toDouble(), 'f', 0) + " fps"
                                                       : "--");
        benchmarkAccuracyValue->setText(percent(imageValidation.value("accuracy")));
        validationEvidenceValue->setText(
            wrapTechnicalText("Validation files: " + jsonPathSummary(entry.value("validation_evidence"))));
        promotionStatusValue->setText("Workspace role: " + (entryIsActive(entry) ? QString("Active model")
                                                                                  : QString("Available model")));
        promotionRecordValue->setText(wrapTechnicalText(
            "History record: " + (registryString(entry, "promotion_record_path").isEmpty()
                                      ? QString("(none)")
                                      : registryString(entry, "promotion_record_path"))));
        limitationsValue->setText("Limitations: " + jsonStringList(entry.value("limitations").toArray()).join("; "));
        blockersValue->setText(wrapTechnicalText("Blockers: " + (entry.value("blockers").toArray().isEmpty()
                                                                     ? "(none)"
                                                                     : jsonPathSummary(entry.value("blockers")))));
        if (controls.validatorWorkspace) {
            if (auto* modelEdit = controls.validatorWorkspace->findChild<QLineEdit*>("ValidatorWorkspaceModelEdit")) {
                modelEdit->setText(absoluteRegistryPath(registryString(entry, "model_path")));
            }
            if (auto* metadataEdit = controls.validatorWorkspace->findChild<QLineEdit*>("ValidatorWorkspaceMetadataEdit")) {
                metadataEdit->setText(absoluteRegistryPath(registryString(entry, "metadata_path")));
            }
        }
    };
    QObject::connect(modelRegistryList, &QTableWidget::currentCellChanged,
                     [=](int, int, int, int) { updateModelWorkspaceDetails(); });
    QObject::connect(openMetadataButton, &QPushButton::clicked, [=]() {
        openPathOrWarn(modelWorkspacePage, selectedEntry().value("metadata_path").toString(), "Open Metadata");
    });
    QObject::connect(reloadMetadataButton, &QPushButton::clicked, [=]() { updateModelWorkspaceDetails(); });
    auto persistRegistryEntries = [=](const QJsonArray& updatedEntries, const QString& selectedId, int fallbackRow,
                                      QString* error) -> bool {
        if (error)
            error->clear();
        if (!saveRegistryEntriesToPath(controls.registryFilePath, updatedEntries, error))
            return false;
        *registryEntries = updatedEntries;
        populateRegistryList();
        selectRegistryRow(selectedId, fallbackRow);
        updateModelWorkspaceDetails();
        return true;
    };
    auto addPackagedEntry = [=](const QJsonObject& packagedEntry) -> QString {
        const QString availabilityError = packagedEntryAvailabilityError(packagedEntry);
        if (!availabilityError.isEmpty())
            return availabilityError;

        QJsonArray updatedEntries = *registryEntries;
        const QJsonObject addedEntry = makeUserAddedRegistryEntry(packagedEntry, updatedEntries);
        updatedEntries.append(addedEntry);
        QString error;
        if (!persistRegistryEntries(updatedEntries, registryEntryId(addedEntry), updatedEntries.size() - 1, &error))
            return error;
        return QString();
    };
    auto removeSelectedModel = [=](bool requireConfirmation) -> QString {
        const int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            return "No model is selected.";
        const QJsonObject entry = registryEntries->at(row).toObject();
        const QString blockReason = removalBlockedReason(entry, registryEntries->size());
        if (!blockReason.isEmpty())
            return blockReason;
        if (requireConfirmation) {
            const auto reply =
                QMessageBox::question(modelWorkspacePage, "Remove model",
                                      QString("Remove \"%1\" from the Model workspace list?\n\n"
                                              "This removes only the registry row. It does not delete bundled or imported model files.")
                                          .arg(displayNameForEntry(entry)),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes)
                return QString();
        }

        QJsonArray updatedEntries = *registryEntries;
        updatedEntries.removeAt(row);
        const int fallbackRow = qMin(row, updatedEntries.size() - 1);
        const QString nextSelectionId =
            fallbackRow >= 0 ? registryEntryId(updatedEntries.at(fallbackRow).toObject()) : QString();
        QString error;
        if (!persistRegistryEntries(updatedEntries, nextSelectionId, fallbackRow, &error))
            return error;
        return QString();
    };
    auto renameSelectedModel = [=](const QString& requestedName = QString(), bool promptUser = true) -> QString {
        const int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            return "No model is selected.";

        const QJsonObject entry = registryEntries->at(row).toObject();
        const QString selectedId = registryEntryId(entry);
        QString newName = requestedName.trimmed();
        if (promptUser) {
            bool ok = false;
            newName = QInputDialog::getText(modelWorkspacePage, "Rename model", "Model name:", QLineEdit::Normal,
                                            displayNameForEntry(entry), &ok)
                          .trimmed();
            if (!ok)
                return QString();
        }
        if (newName.isEmpty())
            return "Model name cannot be empty.";

        QString error;
        if (!renameRegistryEntryDisplayName(controls.registryFilePath, selectedId, newName, &error))
            return error;

        QJsonArray refreshed = loadRegistryEntriesFromPath(controls.registryFilePath, &error);
        if (refreshed.isEmpty())
            return error.isEmpty() ? "Registry refresh found no model entries after rename." : error;
        *registryEntries = refreshed;
        populateRegistryList();
        selectRegistryRow(selectedId, row);
        updateModelWorkspaceDetails();
        return QString();
    };
    QObject::connect(modelActionsButton, &QPushButton::clicked, [=]() {
        QMenu menu(modelActionsButton);
        menu.addAction("Open Metadata", [=]() {
            openPathOrWarn(modelWorkspacePage, selectedEntry().value("metadata_path").toString(), "Open Metadata");
        });
        menu.addAction("Rename model", [=]() {
            const QString error = renameSelectedModel();
            if (!error.isEmpty())
                QMessageBox::warning(modelWorkspacePage, "Rename model", error);
        });
        menu.addAction("Remove model", [=]() {
            const QString error = removeSelectedModel(true);
            if (!error.isEmpty())
                QMessageBox::warning(modelWorkspacePage, "Remove model", error);
        });
        menu.exec(modelActionsButton->mapToGlobal(QPoint(0, modelActionsButton->height())));
    });
    QObject::connect(setActiveButton, &QPushButton::clicked, [=]() {
        int row = modelRegistryList->currentRow();
        if (row < 0 || row >= registryEntries->size())
            return;
        QJsonObject selected = registryEntries->at(row).toObject();
        if (entryIsBlockedFromLiveSorting(selected)) {
            QMessageBox::information(modelWorkspacePage, "Set Active",
                                     "This starter model is blocked from live sorting until it is trained.");
            return;
        }
        const ActiveModelReadiness readiness = evaluateActiveModelReadiness(selected);
        if (!readiness.ready) {
            QMessageBox::warning(modelWorkspacePage, "Set Active", readiness.message);
            return;
        }
        const QString selectedId = registryEntryId(selected);
        QString error;
        if (!activateModelRegistryEntry(controls.registryFilePath, selectedId, &error)) {
            QMessageBox::warning(modelWorkspacePage, "Set Active", error);
            return;
        }
        QString warning;
        const QJsonArray refreshed = loadRegistryEntriesFromPath(controls.registryFilePath, &warning);
        if (refreshed.isEmpty()) {
            QMessageBox::warning(modelWorkspacePage, "Set Active",
                                 warning.isEmpty() ? "The model was marked active, but the registry could not be reloaded."
                                                   : warning);
            return;
        }
        *registryEntries = refreshed;
        populateRegistryList();
        selectRegistryRow(selectedId, row);
        updateModelWorkspaceDetails();
    });
    QObject::connect(addBlankModelButton, &QPushButton::clicked, [=]() {
        const QString error = addPackagedEntry(packagedBlankEntry);
        if (!error.isEmpty())
            QMessageBox::warning(modelWorkspacePage, "Add blank model", error);
    });
    QObject::connect(addPretrainedModelButton, &QPushButton::clicked, [=]() {
        const QString error = addPackagedEntry(packagedPretrainedEntry);
        if (!error.isEmpty())
            QMessageBox::warning(modelWorkspacePage, "Add pre-trained model", error);
    });
    QObject::connect(removeModelButton, &QPushButton::clicked, [=]() {
        const QString error = removeSelectedModel(true);
        if (!error.isEmpty())
            QMessageBox::warning(modelWorkspacePage, "Remove model", error);
    });
    QObject::connect(addModelButton, &QPushButton::clicked, [=]() {
        const QString startDir = chooseOpenFileDialogPath(QString(), defaultOpenDssModelsPath(), findPackagedAppPath("models"));
        const QString path = QFileDialog::getOpenFileName(modelWorkspacePage, "Import ONNX Model", startDir,
                                                          "ONNX models (*.onnx);;All files (*.*)");
        if (path.isEmpty())
            return;
        QJsonObject entry;
        const QFileInfo info(path);
        entry["registry_entry_id"] = info.completeBaseName();
        entry["display_name"] = info.fileName();
        entry["state"] = "available";
        entry["live_use_mode"] = "normal";
        entry["selectable_for_normal_live_sorting"] = false;
        entry["model_path"] = path;
        entry["metadata_status"] = "Missing";
        entry["validation_status"] = "No validation on file for this model";
        entry["promotion_status"] = "Available";
        QJsonObject labels;
        labels["0"] = "Non-target";
        labels["1"] = "Target";
        entry["display_labels"] = labels;
        entry["classes"] = QJsonArray{"0", "1"};
        QJsonObject targetPolicy;
        targetPolicy["target_class_id"] = "1";
        targetPolicy["target_display_label"] = "Target";
        entry["target_policy"] = targetPolicy;
        QJsonArray updatedEntries = *registryEntries;
        updatedEntries.append(entry);
        QString error;
        if (!persistRegistryEntries(updatedEntries, registryEntryId(entry), updatedEntries.size() - 1, &error)) {
            QMessageBox::warning(modelWorkspacePage, "Add Model", error);
        }
    });
    QObject::connect(refreshModelsButton, &QPushButton::clicked, [=]() {
        const QString selectedId = registryEntryId(selectedEntry());
        QString warning;
        QJsonArray refreshed = loadRegistryEntriesFromPath(controls.registryFilePath, &warning);
        if (refreshed.isEmpty()) {
            QMessageBox::information(modelWorkspacePage, "Refresh Models",
                                     warning.isEmpty() ? "Registry refresh found no model entries." : warning);
            return;
        }
        *registryEntries = refreshed;
        populateRegistryList();
        selectRegistryRow(selectedId, 0);
        updateModelWorkspaceDetails();
    });
    modelRegistryList->selectRow(0);
    updateModelWorkspaceDetails();
    QTimer::singleShot(2000, modelWorkspacePage, updateModelWorkspaceDetails);
    const bool verifyAddButtons = envFlagEnabled(kVerifyAddButtonsEnv);
    const bool verifyListManagement = envFlagEnabled(kVerifyListManagementEnv);
    const bool verifyActiveSimplification = envFlagEnabled(kVerifyActiveSimplificationEnv);
    if (verifyAddButtons || verifyListManagement || verifyActiveSimplification) {
        QTimer::singleShot(0, modelWorkspacePage, [=]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition)
                    failures.push_back(message);
            };
            require(modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceValidateButton") == nullptr,
                    "Model workspace hides the top-level testing button");
            require(modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceRevalidateButton") == nullptr,
                    "Model workspace hides the re-validate button");
            require(modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceOpenReportButton") == nullptr,
                    "Model workspace hides the validation report button");
            require(modelWorkspacePage->findChild<QPushButton*>("ModelWorkspaceRunValidationButton") == nullptr,
                    "Model workspace hides the run-validation button");
            auto matchingCount = [&](const QJsonArray& entries, const QJsonObject& packagedEntry) {
                int count = 0;
                for (const auto& value : entries) {
                    if (registryEntriesMatchPackagedEntry(value.toObject(), packagedEntry))
                        ++count;
                }
                return count;
            };
            auto persistedEntries = [&]() {
                QString warning;
                const QJsonArray persisted = loadRegistryEntriesFromPath(controls.registryFilePath, &warning);
                require(warning.isEmpty(), QString("Verifier registry warning absent: %1").arg(warning));
                return persisted;
            };
            auto currentRowEntry = [&]() -> QJsonObject {
                const int row = modelRegistryList->currentRow();
                if (row < 0 || row >= registryEntries->size())
                    return {};
                return registryEntries->at(row).toObject();
            };
            auto persistedEntryById = [&](const QJsonArray& entries, const QString& entryId) -> QJsonObject {
                const int row = findRegistryEntryRowById(entries, entryId);
                return row >= 0 ? entries.at(row).toObject() : QJsonObject{};
            };
            auto verifySimpleListRow = [&](const QString& entryId, const QString& context) {
                const int row = findRegistryEntryRowById(*registryEntries, entryId);
                require(row >= 0, context + " row exists");
                if (row < 0)
                    return;

                auto* item = modelRegistryList->item(row, 0);
                require(item != nullptr, context + " table item exists");
                if (!item)
                    return;

                const QJsonObject entry = registryEntries->at(row).toObject();
                const QJsonObject metadataDoc = loadMetadataDoc(entry);
                const QString title = simpleListTitleForEntry(entry);
                const QString status = modelListStatusLabel(entry, metadataDoc);
                const QString date = modelListDateLabel(entry, metadataDoc, controls.registryFilePath);
                const QStringList lines = item->text().split('\n');

                require(lines.size() == 2, context + " row uses a simple two-line layout");
                require(lines.value(0) == title, context + " row title stays user-facing");
                require(status == "Untrained" || status == "Trained",
                        context + " row status is one of the allowed labels");
                require(lines.value(1).contains(status), context + " row shows the mapped status");
                require(!lines.value(1).contains("validated", Qt::CaseInsensitive),
                        context + " row never shows validation-derived status labels");
                require(lines.value(1).contains(date), context + " row shows the best available date or fallback");
                require(item->toolTip() == item->text(), context + " tooltip matches the simplified row text");
                if (context.contains("blank starter", Qt::CaseInsensitive))
                    require(status == "Untrained", context + " row maps starter models to Untrained");
                if (context.contains("pre-trained", Qt::CaseInsensitive) ||
                    context.contains("Promoted/current", Qt::CaseInsensitive))
                    require(status == "Trained", context + " row maps usable models to Trained");

                const QString itemText = item->text();
                require(!registryEntryId(entry).isEmpty() ? !itemText.contains(registryEntryId(entry), Qt::CaseInsensitive)
                                                          : true,
                        context + " row hides registry ids");
                require(!registryString(entry, "model_path").isEmpty()
                            ? !itemText.contains(registryString(entry, "model_path"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides model paths");
                require(!registryString(entry, "metadata_path").isEmpty()
                            ? !itemText.contains(registryString(entry, "metadata_path"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides metadata paths");
                require(!registryString(entry, "model_sha256").isEmpty()
                            ? !itemText.contains(registryString(entry, "model_sha256"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides model hashes");
                require(!registryString(entry, "metadata_sha256").isEmpty()
                            ? !itemText.contains(registryString(entry, "metadata_sha256"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides metadata hashes");
                require(!registryString(entry, "metadata_schema_version").isEmpty()
                            ? !itemText.contains(registryString(entry, "metadata_schema_version"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides metadata schema names");
                require(!registryString(entry, "label_schema_version").isEmpty()
                            ? !itemText.contains(registryString(entry, "label_schema_version"), Qt::CaseInsensitive)
                            : true,
                        context + " row hides label schema names");
            };
            auto verifyReleaseModelsFallback = [&]() {
                const QString syntheticBaseName =
                    QString("__ovds_verify_release_model_resolution_%1").arg(QCoreApplication::applicationPid());
                const QString syntheticModelRelative = QString("app/runtime/models/%1.onnx").arg(syntheticBaseName);
                const QString syntheticMetadataRelative =
                    QString("app/runtime/models/%1_metadata.json").arg(syntheticBaseName);
                const QString releaseModelsDir = QDir(QCoreApplication::applicationDirPath()).filePath("models");
                const QString expectedModelPath = QDir(releaseModelsDir).filePath(syntheticBaseName + ".onnx");
                const QString expectedMetadataPath = QDir(releaseModelsDir).filePath(syntheticBaseName + "_metadata.json");
                require(QDir().mkpath(releaseModelsDir),
                        QString("Verifier can create sibling release models folder: %1")
                            .arg(QDir::toNativeSeparators(releaseModelsDir)));

                QFile syntheticModel(expectedModelPath);
                require(syntheticModel.open(QIODevice::WriteOnly | QIODevice::Truncate),
                        QString("Verifier can create sibling release model asset: %1")
                            .arg(QDir::toNativeSeparators(expectedModelPath)));
                if (syntheticModel.isOpen())
                    syntheticModel.close();

                QFile syntheticMetadata(expectedMetadataPath);
                require(syntheticMetadata.open(QIODevice::WriteOnly | QIODevice::Truncate),
                        QString("Verifier can create sibling release model metadata: %1")
                            .arg(QDir::toNativeSeparators(expectedMetadataPath)));
                if (syntheticMetadata.isOpen()) {
                    syntheticMetadata.write("{}");
                    syntheticMetadata.close();
                }

                const QJsonObject syntheticEntry = {{"model_path", syntheticModelRelative},
                                                    {"metadata_path", syntheticMetadataRelative}};
                require(packagedEntryAvailabilityError(syntheticEntry).isEmpty(),
                        "Packaged availability accepts sibling Release/models fallback assets");
                require(QDir::cleanPath(absoluteRegistryPath(syntheticModelRelative)) == QDir::cleanPath(expectedModelPath),
                        "Packaged model resolver targets sibling Release/models asset");
                require(QDir::cleanPath(absoluteRegistryPath(syntheticMetadataRelative)) ==
                            QDir::cleanPath(expectedMetadataPath),
                        "Packaged metadata resolver targets sibling Release/models asset");

                QFile::remove(expectedModelPath);
                QFile::remove(expectedMetadataPath);
            };
            auto verifyAddedPackagedEntry = [&](const QString& buttonText, QPushButton* button,
                                               const QJsonObject& packagedEntry, bool shouldBeBlocked) {
                require(button != nullptr, QString("%1 button exists").arg(buttonText));
                require(button && button->text() == buttonText, QString("%1 button text is exact").arg(buttonText));
                if (!button)
                    return QJsonObject{};

                const int previousRowCount = modelRegistryList->rowCount();
                const int previousMatchCount = matchingCount(*registryEntries, packagedEntry);
                button->click();
                QCoreApplication::processEvents();

                require(modelRegistryList->rowCount() == previousRowCount + 1,
                        QString("%1 increases the visible model row count").arg(buttonText));
                require(matchingCount(*registryEntries, packagedEntry) == previousMatchCount + 1,
                        QString("%1 adds another registry row for the packaged asset").arg(buttonText));

                const QJsonObject storedEntry = currentRowEntry();
                require(!storedEntry.isEmpty(), QString("%1 selects the newly added row").arg(buttonText));
                require(registryString(storedEntry, "source_registry_entry_id") == registryEntryId(packagedEntry),
                        QString("%1 records the packaged source registry id").arg(buttonText));
                require(registryEntryId(storedEntry) != registryEntryId(packagedEntry),
                        QString("%1 gives the added row a unique registry id").arg(buttonText));
                require(registryString(storedEntry, "display_name").contains("(added "),
                        QString("%1 gives the added row a display-name suffix").arg(buttonText));
                require(registryString(storedEntry, "model_path") == registryString(packagedEntry, "model_path"),
                        QString("%1 keeps the packaged model path").arg(buttonText));
                require(registryString(storedEntry, "metadata_path") == registryString(packagedEntry, "metadata_path"),
                        QString("%1 keeps the packaged metadata path").arg(buttonText));
                require(QDir::cleanPath(absoluteRegistryPath(registryString(storedEntry, "model_path"))) ==
                                QDir::cleanPath(
                                    resolvePackagedPathFromRegistryPath(registryString(storedEntry, "model_path"))),
                        QString("%1 uses the shared packaged model resolver").arg(buttonText));
                require(QDir::cleanPath(absoluteRegistryPath(registryString(storedEntry, "metadata_path"))) ==
                                QDir::cleanPath(
                                    resolvePackagedPathFromRegistryPath(registryString(storedEntry, "metadata_path"))),
                        QString("%1 uses the shared packaged metadata resolver").arg(buttonText));

                const QJsonArray persisted = persistedEntries();
                require(findRegistryEntryRowById(persisted, registryEntryId(storedEntry)) >= 0,
                        QString("%1 persists the added registry row").arg(buttonText));
                require(matchingCount(persisted, packagedEntry) == previousMatchCount + 1,
                        QString("%1 persists the extra packaged-asset row").arg(buttonText));

                if (shouldBeBlocked) {
                    require(entryIsBlockedFromLiveSorting(storedEntry),
                            QString("%1 keeps the added blank model blocked from live sorting").arg(buttonText));
                    require(!storedEntry.value("selectable_for_normal_live_sorting").toBool(false),
                            QString("%1 keeps the added blank model non-selectable").arg(buttonText));
                    require(!setActiveButton->isEnabled(),
                            QString("%1 leaves Set Active disabled for the blocked blank starter").arg(buttonText));
                }
                return storedEntry;
            };
            auto writeTextFile = [&](const QString& path, const QByteArray& bytes, const QString& context) {
                require(QDir().mkpath(QFileInfo(path).absolutePath()), context + " parent folder exists");
                QFile file(path);
                require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), context + " file can be written");
                if (file.isOpen()) {
                    file.write(bytes);
                    file.close();
                }
            };
            auto writeJsonFile = [&](const QString& path, const QJsonObject& object, const QString& context) {
                writeTextFile(path, QJsonDocument(object).toJson(QJsonDocument::Indented), context);
            };
            auto appendVerifierEntry = [&](const QJsonObject& entry, const QString& context) -> QJsonObject {
                QJsonArray updatedEntries = *registryEntries;
                updatedEntries.append(entry);
                QString error;
                require(persistRegistryEntries(updatedEntries, registryEntryId(entry), updatedEntries.size() - 1, &error),
                        context + ": " + error);
                QCoreApplication::processEvents();
                return currentRowEntry();
            };
            auto verifyActiveModelSimplificationCases = [&]() {
                QTemporaryDir tempDir(QDir::tempPath() + "/ovds_model_active_verify_XXXXXX");
                require(tempDir.isValid(), "Active-model verifier temp directory is available");
                if (!tempDir.isValid())
                    return;

                auto jsonArrayFromStrings = [](const QStringList& values) {
                    QJsonArray array;
                    for (const QString& value : values)
                        array.append(value);
                    return array;
                };
                auto validationSummary = []() {
                    return QJsonObject{{"image_validation",
                                        QJsonObject{{"status", "completed"}, {"accuracy", 0.98}, {"macro_f1", 0.97}}},
                                       {"sequence_validation", QJsonObject{{"status", "not_run"}}}};
                };
                auto metadataObject = [&](const QStringList& classIds, const QJsonObject& labels, bool includeValidation) {
                    QJsonObject metadata{{"schema_version", "model-metadata-v1"},
                                         {"model_id", "verify_model"},
                                         {"model_name", "Verifier model"},
                                         {"classes", jsonArrayFromStrings(classIds)},
                                         {"display_labels", labels}};
                    if (includeValidation)
                        metadata["validation_summary"] = validationSummary();
                    return metadata;
                };
                auto entryObject = [&](const QString& entryId, const QString& displayName, const QString& modelPath,
                                       const QString& metadataPath, const QJsonObject& extra = QJsonObject{}) {
                    QJsonObject entry{{"registry_entry_id", entryId},
                                      {"display_name", displayName},
                                      {"state", "available"},
                                      {"live_use_mode", "normal"},
                                      {"selectable_for_normal_live_sorting", false},
                                      {"model_path", modelPath},
                                      {"metadata_path", metadataPath},
                                      {"promotion_status", "Available"}};
                    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
                        entry[it.key()] = it.value();
                    return entry;
                };

                const QString promotedDirA = QDir(tempDir.path()).filePath("promoted_a");
                const QString promotedDirB = QDir(tempDir.path()).filePath("promoted_b");
                const QString promotedModelA = QDir(promotedDirA).filePath("model.onnx");
                const QString promotedModelB = QDir(promotedDirB).filePath("model.onnx");
                const QString promotedMetadataA = QDir(promotedDirA).filePath("metadata.json");
                const QString promotedMetadataB = QDir(promotedDirB).filePath("metadata.json");
                writeTextFile(promotedModelA, "model-a", "Ready model A");
                writeTextFile(promotedModelB, "model-b", "Ready model B");
                writeJsonFile(promotedMetadataA,
                              metadataObject({"0", "1"},
                                             QJsonObject{{"0", "Non-target"}, {"1", "Target"}}, true),
                              "Ready model A metadata");
                writeJsonFile(promotedMetadataB,
                              metadataObject({"0", "1"},
                                             QJsonObject{{"0", "Non-target"}, {"1", "Target"}}, true),
                              "Ready model B metadata");
                const QString ternaryDir = QDir(tempDir.path()).filePath("ternary_ready");
                const QString ternaryModel = QDir(ternaryDir).filePath("model.onnx");
                const QString ternaryMetadata = QDir(ternaryDir).filePath("metadata.json");
                writeTextFile(ternaryModel, "model-ternary", "Ready 3-class model");
                QJsonObject ternaryPolicy{{"mode", "trigger_on_target_class"},
                                          {"target_class_id", "1"},
                                          {"target_display_label", "Single"},
                                          {"non_target_class_ids", QJsonArray{"0", "2"}},
                                          {"trigger_rule", "trigger_on_target_class"},
                                          {"waste_class_id", QJsonValue::Null},
                                          {"waste_display_label", QJsonValue::Null}};
                QJsonObject ternaryMetadataObject =
                    metadataObject({"0", "1", "2"},
                                   QJsonObject{{"0", "Empty"}, {"1", "Single"}, {"2", "MoreThanOne"}}, true);
                ternaryMetadataObject["sorting_policy"] = ternaryPolicy;
                writeJsonFile(ternaryMetadata, ternaryMetadataObject, "Ready 3-class model metadata");

                const QString readyEntryIdA = QString("verify_ready_a_%1").arg(QCoreApplication::applicationPid());
                const QString readyEntryIdB = QString("verify_ready_b_%1").arg(QCoreApplication::applicationPid());
                const QJsonObject readyEntryA =
                    appendVerifierEntry(entryObject(readyEntryIdA, "Verifier ready A", promotedModelA, promotedMetadataA),
                                        "Ready model A entry persists");
                const QJsonObject readyEntryB =
                    appendVerifierEntry(entryObject(readyEntryIdB, "Verifier ready B", promotedModelB, promotedMetadataB),
                                        "Ready model B entry persists");
                const QString ternaryEntryId = QString("verify_ternary_ready_%1").arg(QCoreApplication::applicationPid());
                const QJsonObject ternaryEntry =
                    appendVerifierEntry(
                        entryObject(ternaryEntryId, "Verifier ready 3-class", ternaryModel, ternaryMetadata),
                        "Ready 3-class model entry persists");

                selectRegistryRow(registryEntryId(readyEntryA), 0);
                updateModelWorkspaceDetails();
                require(modelStateValue->text() == "Trained", "Ready model detail status is Trained");
                require(setActiveButton->isEnabled(), "Ready promoted model keeps Set Active enabled");
                require(setActiveButton->toolTip() == "Mark the selected model as active.",
                        "Ready promoted model keeps the Set Active tooltip");
                setActiveButton->click();
                QCoreApplication::processEvents();
                const QJsonArray afterReadyA = persistedEntries();
                const QJsonObject persistedReadyA = persistedEntryById(afterReadyA, registryEntryId(readyEntryA));
                require(persistedReadyA.value("selectable_for_normal_live_sorting").toBool(false),
                        "Ready promoted model becomes selectable after Set Active");
                require(registryString(persistedReadyA, "state") == "promoted_current",
                        "Ready promoted model becomes promoted_current after Set Active");

                selectRegistryRow(registryEntryId(ternaryEntry), 0);
                updateModelWorkspaceDetails();
                const ActiveModelReadiness ternaryReadiness = evaluateActiveModelReadiness(currentRowEntry());
                require(ternaryReadiness.ready,
                        "3-class target/non-target policy with non_target_class_ids is ready for activation");
                require(setActiveButton->isEnabled(), "3-class ready model keeps Set Active enabled");

                const QByteArray originalUserProfile = qgetenv("USERPROFILE");
                const QString syntheticUserProfile = QDir(tempDir.path()).filePath("profile_root");
                require(QDir().mkpath(syntheticUserProfile), "Active-model verifier synthetic profile root exists");
                qputenv("USERPROFILE", syntheticUserProfile.toUtf8());
                const DefaultWorkspacePaths syncedPathsA = ensureDefaultWorkspaceAssets(afterReadyA);
                QFile activeModelA(syncedPathsA.activeModel);
                require(activeModelA.open(QIODevice::ReadOnly), "Ready model A sync writes the active ONNX file");
                const QByteArray syncedModelA = activeModelA.isOpen() ? activeModelA.readAll() : QByteArray();
                if (activeModelA.isOpen())
                    activeModelA.close();
                require(syncedModelA == QByteArray("model-a"),
                        "Ready model A sync writes the selected active ONNX contents");

                selectRegistryRow(registryEntryId(readyEntryB), 0);
                updateModelWorkspaceDetails();
                require(setActiveButton->isEnabled(), "Second ready promoted model keeps Set Active enabled");
                setActiveButton->click();
                QCoreApplication::processEvents();
                const QJsonArray afterReadyB = persistedEntries();
                const QJsonObject persistedReadyB = persistedEntryById(afterReadyB, registryEntryId(readyEntryB));
                require(persistedReadyB.value("selectable_for_normal_live_sorting").toBool(false),
                        "Second ready promoted model becomes selectable after Set Active");
                const DefaultWorkspacePaths syncedPathsB = ensureDefaultWorkspaceAssets(afterReadyB);
                QFile activeModelB(syncedPathsB.activeModel);
                require(activeModelB.open(QIODevice::ReadOnly), "Second ready model sync keeps the active ONNX file readable");
                const QByteArray syncedModelB = activeModelB.isOpen() ? activeModelB.readAll() : QByteArray();
                if (activeModelB.isOpen())
                    activeModelB.close();
                require(syncedModelB == QByteArray("model-b"),
                        "Second ready model sync replaces the active ONNX contents when filenames match");
                if (originalUserProfile.isEmpty())
                    qunsetenv("USERPROFILE");
                else
                    qputenv("USERPROFILE", originalUserProfile);

                auto verifyBlockedCase = [&](const QString& caseSuffix, const QString& displayName, const QString& modelPath,
                                             const QString& metadataPath, const QJsonObject& metadata,
                                             const QString& expectedMissingItem,
                                             const QString& expectedMessageFragment,
                                             const QJsonObject& extra = QJsonObject{}) {
                    if (!metadataPath.isEmpty() && !metadata.isEmpty())
                        writeJsonFile(metadataPath, metadata, displayName + " metadata");
                    const QString entryId =
                        QString("verify_%1_%2").arg(caseSuffix).arg(QCoreApplication::applicationPid());
                    const QJsonObject storedEntry =
                        appendVerifierEntry(entryObject(entryId, displayName, modelPath, metadataPath, extra),
                                            displayName + " entry persists");
                    selectRegistryRow(registryEntryId(storedEntry), 0);
                    updateModelWorkspaceDetails();
                    const ActiveModelReadiness readiness = evaluateActiveModelReadiness(currentRowEntry());
                    require(!readiness.ready, displayName + " is blocked from activation");
                    require(readiness.missingItem.compare(expectedMissingItem, Qt::CaseInsensitive) == 0,
                            displayName + " reports the exact missing item");
                    require(readiness.message.contains(expectedMessageFragment, Qt::CaseInsensitive),
                            displayName + " reports the expected user-facing diagnostic");
                    require(setActiveButton->toolTip().contains(expectedMessageFragment, Qt::CaseInsensitive),
                            displayName + " surfaces the exact blocker in the Set Active tooltip");
                };

                const QString missingModelMetadata = QDir(tempDir.path()).filePath("missing_model_metadata.json");
                writeJsonFile(missingModelMetadata,
                              metadataObject({"0", "1"},
                                             QJsonObject{{"0", "Non-target"}, {"1", "Target"}}, true),
                              "Missing model metadata");
                verifyBlockedCase("missing_model", "Verifier missing model",
                                  QDir(tempDir.path()).filePath("missing_model.onnx"), missingModelMetadata,
                                  QJsonObject{}, "model.onnx", "ONNX model file is missing");

                const QString missingMetadataModel = QDir(tempDir.path()).filePath("missing_metadata.onnx");
                writeTextFile(missingMetadataModel, "missing-metadata-model", "Missing metadata model");
                verifyBlockedCase("missing_metadata", "Verifier missing metadata", missingMetadataModel,
                                  QDir(tempDir.path()).filePath("missing_metadata.json"), QJsonObject{},
                                  "metadata.json", "metadata file is missing");

                const QString missingLabelsModel = QDir(tempDir.path()).filePath("missing_labels.onnx");
                const QString missingLabelsMetadata = QDir(tempDir.path()).filePath("missing_labels.json");
                writeTextFile(missingLabelsModel, "missing-labels-model", "Missing labels model");
                verifyBlockedCase("missing_labels", "Verifier missing labels", missingLabelsModel, missingLabelsMetadata,
                                  metadataObject({"alpha", "beta"}, QJsonObject{}, true), "classes/labels",
                                  "class labels could not be read");

                const QString missingPolicyModel = QDir(tempDir.path()).filePath("missing_policy.onnx");
                const QString missingPolicyMetadata = QDir(tempDir.path()).filePath("missing_policy.json");
                writeTextFile(missingPolicyModel, "missing-policy-model", "Missing policy model");
                verifyBlockedCase("missing_policy", "Verifier missing policy", missingPolicyModel, missingPolicyMetadata,
                                  metadataObject({"alpha", "beta"},
                                                 QJsonObject{{"alpha", "Target-like"}, {"beta", "Non-target-like"}}, true),
                                  "target/non-target policy", "target/non-target sorting policy could not be read");

                const QString unvalidatedModel = QDir(tempDir.path()).filePath("unvalidated_model.onnx");
                const QString unvalidatedMetadata = QDir(tempDir.path()).filePath("unvalidated_metadata.json");
                writeTextFile(unvalidatedModel, "unvalidated-model", "Unvalidated model");
                writeJsonFile(unvalidatedMetadata,
                              metadataObject({"0", "1"},
                                             QJsonObject{{"0", "Non-target"}, {"1", "Target"}}, false),
                              "Unvalidated model metadata");
                const QJsonObject unvalidatedEntry =
                    appendVerifierEntry(entryObject(QString("verify_unvalidated_%1").arg(QCoreApplication::applicationPid()),
                                                    "Verifier unvalidated trained model", unvalidatedModel,
                                                    unvalidatedMetadata),
                                        "Unvalidated trained model entry persists");
                selectRegistryRow(registryEntryId(unvalidatedEntry), 0);
                updateModelWorkspaceDetails();
                const ActiveModelReadiness unvalidatedReadiness = evaluateActiveModelReadiness(currentRowEntry());
                require(unvalidatedReadiness.ready,
                        "Model without validation summary remains activatable when files, labels, and policy are usable");
                require(modelStateValue->text() == "Trained", "Model without validation summary still shows Trained");
                require(setActiveButton->isEnabled(),
                        "Model without validation summary keeps Set Active enabled after practical checks pass");
            };

            if (verifyAddButtons || verifyListManagement) {
                verifyReleaseModelsFallback();
                require(removeModelButton != nullptr, "Remove model button exists");
                require(removeModelButton && removeModelButton->text() == "Remove model",
                        "Remove model button text is exact");
                const int initialRowCount = modelRegistryList->rowCount();
                const int initialBlankCount = matchingCount(*registryEntries, packagedBlankEntry);
                const int initialPretrainedCount = matchingCount(*registryEntries, packagedPretrainedEntry);
                require(initialRowCount >= 3, "Verifier temp registry starts with packaged model rows present");
                require(initialBlankCount >= 1, "Verifier temp registry starts with the packaged blank starter row");
                require(initialPretrainedCount >= 1, "Verifier temp registry starts with the packaged pre-trained row");

                const QJsonObject addedBlankEntry =
                    verifyAddedPackagedEntry("Add blank model", addBlankModelButton, packagedBlankEntry, true);
                const QJsonObject addedPretrainedEntry = verifyAddedPackagedEntry("Add pre-trained model",
                                                                                  addPretrainedModelButton,
                                                                                  packagedPretrainedEntry, false);
                verifySimpleListRow(registryEntryId(addedBlankEntry), "Added blank starter");
                verifySimpleListRow(registryEntryId(addedPretrainedEntry), "Added pre-trained model");

                int activeRow = -1;
                for (int i = 0; i < registryEntries->size(); ++i) {
                    if (entryIsActive(registryEntries->at(i).toObject())) {
                        activeRow = i;
                        break;
                    }
                }
                require(activeRow >= 0, "Registry still has a promoted/current row");
                if (activeRow >= 0) {
                    modelRegistryList->selectRow(activeRow);
                    updateModelWorkspaceDetails();
                    require(!removeModelButton->isEnabled(), "Remove model is blocked for the promoted/current row");
                    verifySimpleListRow(registryEntryId(registryEntries->at(activeRow).toObject()),
                                        "Promoted/current row");
                }

                selectRegistryRow(registryEntryId(addedPretrainedEntry), 0);
                updateModelWorkspaceDetails();
                require(removeModelButton->isEnabled(), "Remove model is enabled for an added non-active row");
                const QString removeError = removeSelectedModel(false);
                require(removeError.isEmpty(),
                        QString("Remove model succeeds for added pre-trained row: %1").arg(removeError));
                QCoreApplication::processEvents();
                require(modelRegistryList->rowCount() == initialRowCount + 1,
                        "Remove model decreases the visible model row count");
                const QJsonArray afterRemovePersisted = persistedEntries();
                require(findRegistryEntryRowById(afterRemovePersisted, registryEntryId(addedPretrainedEntry)) < 0,
                        "Remove model persists deletion of the selected added row");
                require(matchingCount(afterRemovePersisted, packagedPretrainedEntry) == initialPretrainedCount,
                        "Remove model restores the packaged pre-trained asset count after deletion");

                selectRegistryRow(registryEntryId(addedBlankEntry), 0);
                refreshModelsButton->click();
                QCoreApplication::processEvents();
                require(modelRegistryList->rowCount() == afterRemovePersisted.size(),
                        "Refresh reloads the persisted model registry row count");
                require(findRegistryEntryRowById(*registryEntries, registryEntryId(addedBlankEntry)) >= 0,
                        "Refresh reload preserves the added blank model row");
                require(matchingCount(*registryEntries, packagedBlankEntry) == initialBlankCount + 1,
                        "Refresh reload preserves the extra blank-model row");

                selectRegistryRow(registryEntryId(addedBlankEntry), 0);
                updateModelWorkspaceDetails();
                const QJsonObject reloadedBlankEntry = currentRowEntry();
                require(registryEntryId(reloadedBlankEntry) == registryEntryId(addedBlankEntry),
                        "Refresh re-selects the added blank model row");
                require(modelStateValue->text() == "Untrained", "Reloaded blank starter detail status is Untrained");
                require(entryIsBlockedFromLiveSorting(reloadedBlankEntry),
                        "Reloaded blank starter row remains blocked from live sorting");
                require(!setActiveButton->isEnabled(), "Reloaded blank starter row still disables Set Active");
                verifySimpleListRow(registryEntryId(reloadedBlankEntry), "Reloaded blank starter row");

                const QString renamedBlank =
                    QString("Verifier renamed blank starter %1").arg(QCoreApplication::applicationPid());
                const QString renameError = renameSelectedModel(renamedBlank, false);
                require(renameError.isEmpty(), QString("Rename model succeeds for selected row: %1").arg(renameError));
                const QJsonObject renamedEntry = currentRowEntry();
                require(registryString(renamedEntry, "display_name") == renamedBlank,
                        "Rename model updates the in-memory selected entry display name");
                verifySimpleListRow(registryEntryId(renamedEntry), "Renamed blank starter row");
                const QJsonArray afterRenamePersisted = persistedEntries();
                const int renamedPersistedRow =
                    findRegistryEntryRowById(afterRenamePersisted, registryEntryId(renamedEntry));
                require(renamedPersistedRow >= 0, "Rename model preserves the selected registry row");
                if (renamedPersistedRow >= 0) {
                    const QJsonObject persistedRenamedEntry = afterRenamePersisted.at(renamedPersistedRow).toObject();
                    require(registryString(persistedRenamedEntry, "display_name") == renamedBlank,
                            "Rename model persists display_name to the registry");
                    require(registryString(persistedRenamedEntry, "model_path") ==
                                registryString(addedBlankEntry, "model_path"),
                            "Rename model leaves model_path unchanged");
                    require(registryString(persistedRenamedEntry, "metadata_path") ==
                                registryString(addedBlankEntry, "metadata_path"),
                            "Rename model leaves metadata_path unchanged");
                }
                const QString emptyRenameError = renameSelectedModel("   ", false);
                require(emptyRenameError.contains("empty", Qt::CaseInsensitive),
                        "Rename model rejects empty display names");
            }

            if (verifyActiveSimplification)
                verifyActiveModelSimplificationCases();

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (failures.isEmpty()) {
                qInfo().noquote() << "Model workspace verifier passed.";
            } else {
                qWarning().noquote() << "Model workspace verifier failed:" << failures.join("; ");
            }
            QCoreApplication::exit(exitCode);
        });
    }

    auto* modelWorkspaceSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(modelWorkspaceSplitter, "ModelWorkspaceSplitter");
    modelWorkspaceSplitter->addWidget(modelWorkspaceRegistryPanel);
    modelWorkspaceSplitter->addWidget(modelDetailScroll);
    modelWorkspaceSplitter->setStretchFactor(0, 0);
    modelWorkspaceSplitter->setStretchFactor(1, 1);
    desktop_app::ui::configureWorkspaceSplitter(modelWorkspaceSplitter, "workspace/model/splitter", {360, 860},
                                                {300, 520});
    modelWorkspaceLayout->addWidget(modelWorkspaceSplitter, 1);
    modelWorkspacePage->setLayout(modelWorkspaceLayout);
    return modelWorkspacePage;
}

} // namespace desktop_app::workspace
