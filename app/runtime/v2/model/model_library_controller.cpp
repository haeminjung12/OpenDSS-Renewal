#include "model_library_controller.h"

#include "../../desktop_app/model_registry_service.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <utility>

namespace desktop_app::v2 {
namespace {

QString entryId(const QJsonObject &entry)
{
    return registryString(entry, QStringLiteral("registry_entry_id")).trimmed();
}

QString entryName(const QJsonObject &entry)
{
    const QString name = registryString(entry, QStringLiteral("display_name")).trimmed();
    return name.isEmpty() ? entryId(entry) : name;
}

QJsonObject projectionEntry(const QJsonObject &entry,
                            const ModelPackageInspection &inspection)
{
    const QString id = entryId(entry);
    QString architectureId;
    QString origin;
    if (id == QStringLiteral("opendss_blank_mobilenet_v3_small")) {
        architectureId = QStringLiteral("mobilenet_v3_small");
        origin = QStringLiteral("blank");
    } else if (id == QStringLiteral("opendss_pretrained_mobilenet_v3_small")) {
        architectureId = QStringLiteral("mobilenet_v3_small");
        origin = QStringLiteral("pretrained");
    } else if (id == QStringLiteral("opendss_blank_efficientnet_b0")) {
        architectureId = QStringLiteral("efficientnet_b0");
        origin = QStringLiteral("blank");
    } else if (id == QStringLiteral("opendss_pretrained_efficientnet_b0")) {
        architectureId = QStringLiteral("efficientnet_b0");
        origin = QStringLiteral("pretrained");
    }

    QJsonObject projected = entry;
    if (!architectureId.isEmpty()) {
        projected = packagedModernModelRegistryEntry(architectureId, origin);
        for (const QString &key :
             {QStringLiteral("registry_entry_id"), QStringLiteral("display_name"),
              QStringLiteral("package_path"), QStringLiteral("active")}) {
            projected.insert(key, entry.value(key));
        }
    }

    QFile metadataFile(inspection.metadataPath);
    if (metadataFile.open(QIODevice::ReadOnly)) {
        const QJsonObject metadata =
            QJsonDocument::fromJson(metadataFile.readAll()).object();
        const QJsonArray classes =
            metadata.value(QStringLiteral("classes"))
                .toArray(metadata.value(QStringLiteral("class_ids")).toArray());
        if (!classes.isEmpty())
            projected.insert(QStringLiteral("classes"), classes);
        const QJsonObject displayLabels =
            metadata.value(QStringLiteral("display_labels")).toObject();
        if (!displayLabels.isEmpty())
            projected.insert(QStringLiteral("display_labels"), displayLabels);
        const QString createdAt =
            metadata.value(QStringLiteral("created_at")).toString();
        if (!createdAt.isEmpty())
            projected.insert(QStringLiteral("created_at"), createdAt);
    }
    return projected;
}

QString performanceLabel(const QJsonObject &entry, const QString &architectureId)
{
    Q_UNUSED(entry);
    if (architectureId == QStringLiteral("mobilenet_v3_small"))
        return QStringLiteral("Faster");
    if (architectureId == QStringLiteral("efficientnet_b0"))
        return QStringLiteral("More Accurate");
    return {};
}

QString classSummary(const QJsonObject &entry)
{
    QStringList labels;
    const QJsonObject displayLabels = entry.value(QStringLiteral("display_labels")).toObject();
    for (const QJsonValue &value : entry.value(QStringLiteral("classes")).toArray()) {
        const QString id = value.toString();
        labels.append(displayLabels.value(id).toString(id));
    }
    return labels.join(QStringLiteral(", "));
}

QVariantMap rowMap(const QJsonObject &entry)
{
    const ModelPackageInspection inspection = inspectModelPackage(entry);
    const QJsonObject projected = projectionEntry(entry, inspection);
    return {{QStringLiteral("id"), entryId(entry)},
            {QStringLiteral("name"), entryName(entry)},
            {QStringLiteral("architecture"), inspection.architectureId},
            {QStringLiteral("userFacingLabel"),
             registryString(projected, QStringLiteral("user_facing_label")).trimmed()},
            {QStringLiteral("performanceLabel"),
             performanceLabel(projected, inspection.architectureId)},
            {QStringLiteral("classSummary"), classSummary(projected)},
            {QStringLiteral("active"), entry.value(QStringLiteral("active")).toBool(false)},
            {QStringLiteral("status"), inspection.status},
            {QStringLiteral("message"), inspection.message},
            {QStringLiteral("canActivate"), inspection.canActivate}};
}

} // namespace

ModelLibraryController::ModelLibraryController(QString registryFilePath, QObject *parent)
    : QObject(parent)
    , registryFilePath_(std::move(registryFilePath))
{
}

QVariantList ModelLibraryController::modelRows() const
{
    QVariantList rows;
    rows.reserve(entries_.size());
    for (const QJsonValue &value : entries_) {
        if (value.isObject())
            rows.append(rowMap(value.toObject()));
    }
    return rows;
}

int ModelLibraryController::selectedIndex() const
{
    return selectedIndex_;
}

QString ModelLibraryController::selectedId() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= entries_.size())
        return {};
    return entryId(entries_.at(selectedIndex_).toObject());
}

QVariantMap ModelLibraryController::selectedDetail() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= entries_.size())
        return {};

    const QJsonObject entry = entries_.at(selectedIndex_).toObject();
    const ModelPackageInspection inspection = inspectModelPackage(entry);
    const QJsonObject projected = projectionEntry(entry, inspection);
    return {{QStringLiteral("id"), entryId(entry)},
            {QStringLiteral("name"), entryName(entry)},
            {QStringLiteral("active"), entry.value(QStringLiteral("active")).toBool(false)},
            {QStringLiteral("architecture"), inspection.architectureId},
            {QStringLiteral("userFacingLabel"),
             registryString(projected, QStringLiteral("user_facing_label")).trimmed()},
            {QStringLiteral("performanceLabel"),
             performanceLabel(projected, inspection.architectureId)},
            {QStringLiteral("classCount"), inspection.classCount},
            {QStringLiteral("classSummary"), classSummary(projected)},
            {QStringLiteral("createdAt"),
             projected.value(QStringLiteral("created_at")).toString()},
            {QStringLiteral("packageLocation"), inspection.packagePath},
            {QStringLiteral("status"), inspection.status},
            {QStringLiteral("message"), inspection.message},
            {QStringLiteral("canActivate"), inspection.canActivate}};
}

QString ModelLibraryController::activeId() const
{
    for (const QJsonValue &value : entries_) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("active")).toBool(false))
            return entryId(entry);
    }
    return {};
}

QString ModelLibraryController::presentation() const
{
    if (!errorMessage_.isEmpty())
        return QStringLiteral("error");
    return entries_.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
}

QString ModelLibraryController::errorMessage() const
{
    return errorMessage_;
}

bool ModelLibraryController::refresh()
{
    const QString priorSelection = selectedId();
    QString warning;
    QJsonArray refreshed = readModelRegistryEntriesFromPath(registryFilePath_, &warning);
    entries_ = std::move(refreshed);
    selectedIndex_ = -1;
    for (int index = 0; index < entries_.size(); ++index) {
        if (!priorSelection.isEmpty()
            && entryId(entries_.at(index).toObject()).compare(
                   priorSelection, Qt::CaseInsensitive) == 0) {
            selectedIndex_ = index;
            break;
        }
    }
    errorMessage_ = warning;
    emit changed();
    return warning.isEmpty();
}

bool ModelLibraryController::select(int index)
{
    if (index < 0 || index >= entries_.size())
        return fail(QStringLiteral("Selected model is unavailable."));

    selectedIndex_ = index;
    errorMessage_.clear();
    emit changed();
    return true;
}

bool ModelLibraryController::setActive()
{
    const QString id = selectedId();
    if (id.isEmpty())
        return fail(QStringLiteral("No model is selected."));
    if (id.compare(activeId(), Qt::CaseInsensitive) == 0)
        return fail(QStringLiteral("Selected model is already Active."));

    QString error;
    if (!activateModelRegistryEntry(registryFilePath_, id, &error))
        return fail(error);
    return refresh();
}

bool ModelLibraryController::renameSelected(const QString &displayName)
{
    const QString id = selectedId();
    if (id.isEmpty())
        return fail(QStringLiteral("No model is selected."));

    QString error;
    if (!renameRegistryEntryDisplayName(registryFilePath_, id, displayName, &error))
        return fail(error);
    return refresh();
}

bool ModelLibraryController::fail(const QString &message)
{
    errorMessage_ = message;
    emit changed();
    return false;
}

} // namespace desktop_app::v2
