#include "settings_repository.h"

#include "../state/application_state_store.h"
#include "../../desktop_app/json_persistence.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <cmath>
#include <utility>

namespace desktop_app::v2 {
namespace {

constexpr int kSchemaVersion = 1;
constexpr int kMinimumTextSizePercent = 80;
constexpr int kMaximumTextSizePercent = 200;

PreferencesState defaultPreferences()
{
    return {QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                .filePath(QStringLiteral("OpenDropletSortingSuite")),
            100};
}

bool validateStorageRoot(const QString &storageRoot, QString *error)
{
    const QFileInfo info(storageRoot);
    if (storageRoot.isEmpty() || !info.isAbsolute() || !info.exists() || !info.isDir()) {
        if (error)
            *error = QStringLiteral("Storage root must be an existing absolute directory.");
        return false;
    }

    const QString probePath = QDir(storageRoot).filePath(
        QStringLiteral(".opendss-settings-probe-%1.tmp").arg(QUuid::createUuid().toString(QUuid::Id128)));
    QSaveFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly) || probe.write("probe") != 5 || !probe.commit()) {
        if (error)
            *error = QStringLiteral("Storage root is not writable: %1").arg(QDir::toNativeSeparators(storageRoot));
        return false;
    }
    if (!QFile::remove(probePath)) {
        if (error)
            *error = QStringLiteral("Storage root probe could not be removed: %1")
                         .arg(QDir::toNativeSeparators(storageRoot));
        return false;
    }
    return true;
}

bool validatePreferences(const PreferencesState &preferences, QString *error)
{
    if (!validateStorageRoot(preferences.storageRoot, error))
        return false;
    if (preferences.textSizePercent < kMinimumTextSizePercent
        || preferences.textSizePercent > kMaximumTextSizePercent) {
        if (error)
            *error = QStringLiteral("Text size percent must be between 80 and 200.");
        return false;
    }
    return true;
}

bool parsePreferences(const QJsonObject &document, PreferencesState *preferences, QString *error)
{
    if (document.size() != 3 || !document.contains(QStringLiteral("schema_version"))
        || !document.contains(QStringLiteral("storage_root"))
        || !document.contains(QStringLiteral("text_size_percent"))) {
        if (error)
            *error = QStringLiteral("Preferences document must contain exactly the supported keys.");
        return false;
    }

    const QJsonValue schemaVersion = document.value(QStringLiteral("schema_version"));
    const QJsonValue storageRoot = document.value(QStringLiteral("storage_root"));
    const QJsonValue textSizePercent = document.value(QStringLiteral("text_size_percent"));
    const double textSize = textSizePercent.toDouble();
    if (!schemaVersion.isDouble() || !storageRoot.isString() || !textSizePercent.isDouble()
        || std::floor(schemaVersion.toDouble()) != schemaVersion.toDouble()
        || std::floor(textSize) != textSize || schemaVersion.toDouble() != kSchemaVersion
        || textSize < kMinimumTextSizePercent || textSize > kMaximumTextSizePercent) {
        if (error)
            *error = QStringLiteral("Preferences document has unsupported field values.");
        return false;
    }

    const PreferencesState candidate{storageRoot.toString(), static_cast<int>(textSize)};
    if (!validatePreferences(candidate, error))
        return false;
    *preferences = candidate;
    return true;
}

} // namespace

SettingsRepository::SettingsRepository(QString preferencesFilePath, ApplicationStateStore &stateStore)
    : preferencesFilePath_(std::move(preferencesFilePath))
    , stateStore_(stateStore)
{
}

bool SettingsRepository::load(QString *error)
{
    if (error)
        error->clear();

    if (!QFileInfo::exists(preferencesFilePath_)) {
        stateStore_.publishPreferences(defaultPreferences());
        return true;
    }

    QFile file(preferencesFilePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Unable to read preferences file: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Preferences file is not a JSON object.");
        return false;
    }

    PreferencesState candidate;
    if (!parsePreferences(document.object(), &candidate, error))
        return false;
    stateStore_.publishPreferences(candidate);
    return true;
}

bool SettingsRepository::save(const PreferencesState &preferences, QString *error) const
{
    if (!validatePreferences(preferences, error))
        return false;
    return desktop_app::writeJsonObjectAtomically(
        preferencesFilePath_,
        {{QStringLiteral("schema_version"), kSchemaVersion},
         {QStringLiteral("storage_root"), preferences.storageRoot},
         {QStringLiteral("text_size_percent"), preferences.textSizePercent}},
        error);
}

bool SettingsRepository::setStorageRoot(const QString &storageRoot, QString *error)
{
    PreferencesState candidate = stateStore_.snapshot().preferences;
    candidate.storageRoot = storageRoot;
    if (!save(candidate, error))
        return false;
    stateStore_.publishPreferences(candidate);
    return true;
}

bool SettingsRepository::setTextSizePercent(int textSizePercent, QString *error)
{
    PreferencesState candidate = stateStore_.snapshot().preferences;
    candidate.textSizePercent = textSizePercent;
    if (!save(candidate, error))
        return false;
    stateStore_.publishPreferences(candidate);
    return true;
}

} // namespace desktop_app::v2
