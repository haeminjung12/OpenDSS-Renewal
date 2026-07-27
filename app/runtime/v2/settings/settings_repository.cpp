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

#include <array>
#include <cmath>
#include <utility>

namespace desktop_app::v2 {
namespace {

constexpr int kSchemaVersion = 1;
constexpr std::size_t kOutputRootCount = 9;

using OutputRoots = std::array<QString, kOutputRootCount>;

struct OutputRootDefinition {
    OutputRootSelector selector;
    const char *key;
    const char *fallbackFolder;
};

constexpr std::array<OutputRootDefinition, kOutputRootCount> kOutputRootDefinitions{{
    {OutputRootSelector::CaptureSingle, "capture_single_output_root", "datasets"},
    {OutputRootSelector::CaptureSequence, "capture_sequence_output_root", "datasets"},
    {OutputRootSelector::CaptureDataset, "capture_dataset_output_root", "datasets"},
    {OutputRootSelector::Train, "train_output_root", "models"},
    {OutputRootSelector::ModelTest, "model_test_output_root", "reports"},
    {OutputRootSelector::Live, "live_output_root", "runs"},
    {OutputRootSelector::SequenceTest, "sequence_test_output_root", "runs"},
    {OutputRootSelector::LibraryCreate, "library_create_output_root", "models"},
    {OutputRootSelector::LibraryExport, "library_export_output_root", "models"},
}};

std::size_t selectorIndex(OutputRootSelector selector)
{
    return static_cast<std::size_t>(selector);
}

const OutputRootDefinition &definition(OutputRootSelector selector)
{
    return kOutputRootDefinitions.at(selectorIndex(selector));
}

QString standardRoot(OutputRootSelector selector)
{
    return QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                    .filePath(QStringLiteral("OpenDropletSortingSuite")))
        .filePath(QString::fromLatin1(definition(selector).fallbackFolder));
}

bool isKnownPreferenceKey(const QString &key)
{
    if (key == QStringLiteral("schema_version") || key == QStringLiteral("storage_root")
        || key == QStringLiteral("text_size_percent")) {
        return true;
    }
    for (const OutputRootDefinition &item : kOutputRootDefinitions) {
        if (key == QString::fromLatin1(item.key))
            return true;
    }
    return false;
}

bool normalizeTextSizePercent(double value, int *normalized)
{
    if (value == 80.0 || value == 100.0 || value == 125.0) {
        *normalized = static_cast<int>(value);
        return true;
    }
    if (value == 90.0) {
        *normalized = 100;
        return true;
    }
    if (value == 150.0 || value == 175.0 || value == 200.0) {
        *normalized = 125;
        return true;
    }
    return false;
}

bool isSupportedTextSizePercent(int value)
{
    return value == 80 || value == 100 || value == 125;
}

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

bool validateOutputRoot(const QString &outputRoot, QString *error)
{
    const QFileInfo info(outputRoot);
    if (outputRoot.isEmpty() || !info.isAbsolute() || !info.exists() || !info.isDir()) {
        if (error)
            *error = QStringLiteral("Output location must be an existing absolute directory.");
        return false;
    }

    const QString probePath = QDir(outputRoot).filePath(
        QStringLiteral(".opendss-output-probe-%1.tmp").arg(QUuid::createUuid().toString(QUuid::Id128)));
    QSaveFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly) || probe.write("probe") != 5 || !probe.commit()) {
        if (error)
            *error = QStringLiteral("Output location is not writable: %1")
                         .arg(QDir::toNativeSeparators(outputRoot));
        return false;
    }
    if (!QFile::remove(probePath)) {
        if (error)
            *error = QStringLiteral("Output location probe could not be removed: %1")
                         .arg(QDir::toNativeSeparators(outputRoot));
        return false;
    }
    return true;
}

bool validatePreferences(const PreferencesState &preferences, QString *error)
{
    if (!validateStorageRoot(preferences.storageRoot, error))
        return false;
    if (!isSupportedTextSizePercent(preferences.textSizePercent)) {
        if (error)
            *error = QStringLiteral("Text size percent must be 80, 100, or 125.");
        return false;
    }
    return true;
}

bool parsePreferences(const QJsonObject &document, PreferencesState *preferences,
                      OutputRoots *outputRoots, OutputRoots *fallbackReasons,
                      bool *needsNormalization, QString *error)
{
    if (!document.contains(QStringLiteral("schema_version"))
        || !document.contains(QStringLiteral("storage_root"))
        || !document.contains(QStringLiteral("text_size_percent"))) {
        if (error)
            *error = QStringLiteral("Preferences document is missing a required key.");
        return false;
    }
    for (auto iterator = document.constBegin(); iterator != document.constEnd(); ++iterator) {
        if (!isKnownPreferenceKey(iterator.key())) {
            if (error)
                *error = QStringLiteral("Preferences document contains an unsupported key.");
            return false;
        }
    }

    const QJsonValue schemaVersion = document.value(QStringLiteral("schema_version"));
    const QJsonValue storageRoot = document.value(QStringLiteral("storage_root"));
    const QJsonValue textSizePercent = document.value(QStringLiteral("text_size_percent"));
    const double textSize = textSizePercent.toDouble();
    if (!schemaVersion.isDouble() || !storageRoot.isString() || !textSizePercent.isDouble()
        || std::floor(schemaVersion.toDouble()) != schemaVersion.toDouble()
        || !std::isfinite(textSize) || std::floor(textSize) != textSize
        || schemaVersion.toDouble() != kSchemaVersion) {
        if (error)
            *error = QStringLiteral("Preferences document has unsupported field values.");
        return false;
    }

    int normalizedTextSizePercent = 0;
    if (!normalizeTextSizePercent(textSize, &normalizedTextSizePercent)) {
        if (error)
            *error = QStringLiteral("Preferences document has unsupported field values.");
        return false;
    }

    const PreferencesState candidate{storageRoot.toString(), normalizedTextSizePercent};
    if (!validatePreferences(candidate, error))
        return false;

    outputRoots->fill({});
    fallbackReasons->fill({});
    for (const OutputRootDefinition &item : kOutputRootDefinitions) {
        const QString key = QString::fromLatin1(item.key);
        if (!document.contains(key))
            continue;
        const QJsonValue value = document.value(key);
        if (!value.isString()) {
            if (error)
                *error = QStringLiteral("Preferences document has unsupported field values.");
            return false;
        }

        const QString selectedRoot = value.toString();
        QString validationError;
        const std::size_t index = selectorIndex(item.selector);
        if (validateOutputRoot(selectedRoot, &validationError)) {
            outputRoots->at(index) = selectedRoot;
        } else {
            fallbackReasons->at(index) =
                QStringLiteral("%1 Using the standard location %2.")
                    .arg(validationError,
                         QDir::toNativeSeparators(standardRoot(item.selector)));
            *needsNormalization = true;
        }
    }

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
        outputRoots_.fill({});
        loadFallbackReasons_.fill({});
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
    const QByteArray bytes = file.readAll();
    file.close();
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Preferences file is not a JSON object.");
        return false;
    }

    PreferencesState candidate;
    OutputRoots outputRoots;
    OutputRoots fallbackReasons;
    bool needsNormalization = false;
    if (!parsePreferences(document.object(), &candidate, &outputRoots, &fallbackReasons,
                          &needsNormalization, error)) {
        return false;
    }
    if (document.object().value(QStringLiteral("text_size_percent")).toInt()
            != candidate.textSizePercent) {
        needsNormalization = true;
    }
    if (needsNormalization && !save(candidate, outputRoots, error))
        return false;
    outputRoots_ = outputRoots;
    loadFallbackReasons_ = fallbackReasons;
    stateStore_.publishPreferences(candidate);
    return true;
}

bool SettingsRepository::save(const PreferencesState &preferences,
                              const OutputRoots &outputRoots, QString *error) const
{
    if (!validatePreferences(preferences, error))
        return false;

    QJsonObject document{
        {QStringLiteral("schema_version"), kSchemaVersion},
        {QStringLiteral("storage_root"), preferences.storageRoot},
        {QStringLiteral("text_size_percent"), preferences.textSizePercent}};
    for (const OutputRootDefinition &item : kOutputRootDefinitions) {
        const QString &outputRoot = outputRoots.at(selectorIndex(item.selector));
        if (!outputRoot.isEmpty() && validateOutputRoot(outputRoot, nullptr))
            document.insert(QString::fromLatin1(item.key), outputRoot);
    }
    return desktop_app::writeJsonObjectAtomically(preferencesFilePath_, document, error);
}

bool SettingsRepository::setStorageRoot(const QString &storageRoot, QString *error)
{
    PreferencesState candidate = stateStore_.snapshot().preferences;
    candidate.storageRoot = storageRoot;
    if (!isSupportedTextSizePercent(candidate.textSizePercent)) {
        if (error)
            *error = QStringLiteral("Current text size percent is unsupported.");
        return false;
    }
    if (!save(candidate, outputRoots_, error))
        return false;
    stateStore_.publishPreferences(candidate);
    return true;
}

bool SettingsRepository::setTextSizePercent(int textSizePercent, QString *error)
{
    PreferencesState candidate = stateStore_.snapshot().preferences;
    if (!normalizeTextSizePercent(textSizePercent, &candidate.textSizePercent)) {
        if (error)
            *error = QStringLiteral("Text size percent must be 80, 100, or 125.");
        return false;
    }
    if (!save(candidate, outputRoots_, error))
        return false;
    stateStore_.publishPreferences(candidate);
    return true;
}

QString SettingsRepository::outputRoot(OutputRootSelector selector) const
{
    const std::size_t index = selectorIndex(selector);
    QString validationError;
    if (loadFallbackReasons_.at(index).isEmpty()
        && !outputRoots_.at(index).isEmpty()
        && validateOutputRoot(outputRoots_.at(index), &validationError)) {
        return outputRoots_.at(index);
    }
    return standardRoot(selector);
}

bool SettingsRepository::outputRootFellBack(OutputRootSelector selector) const
{
    return !outputRootFallbackReason(selector).isEmpty();
}

QString SettingsRepository::outputRootFallbackReason(OutputRootSelector selector) const
{
    const std::size_t index = selectorIndex(selector);
    if (!loadFallbackReasons_.at(index).isEmpty())
        return loadFallbackReasons_.at(index);
    if (outputRoots_.at(index).isEmpty())
        return {};

    QString validationError;
    if (validateOutputRoot(outputRoots_.at(index), &validationError))
        return {};
    return QStringLiteral("%1 Using the standard location %2.")
        .arg(validationError, QDir::toNativeSeparators(standardRoot(selector)));
}

bool SettingsRepository::setOutputRoot(OutputRootSelector selector,
                                       const QString &outputRoot, QString *error)
{
    if (!validateOutputRoot(outputRoot, error))
        return false;

    OutputRoots candidate = outputRoots_;
    const std::size_t index = selectorIndex(selector);
    candidate.at(index) = outputRoot;
    if (!save(stateStore_.snapshot().preferences, candidate, error))
        return false;
    outputRoots_ = candidate;
    loadFallbackReasons_.at(index).clear();
    return true;
}

} // namespace desktop_app::v2
