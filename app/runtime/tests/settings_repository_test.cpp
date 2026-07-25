#include "../v2/settings/settings_repository.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <iostream>
#include <initializer_list>
#include <utility>

namespace {

using namespace desktop_app::v2;

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

bool writeDocument(const QString &path, const QJsonObject &document)
{
    const QByteArray bytes = QJsonDocument(document).toJson();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool written = file.write(bytes) == bytes.size();
    file.close();
    return written;
}

QJsonObject validDocument(const QString &root, int textSizePercent = 100)
{
    return {{QStringLiteral("schema_version"), 1},
            {QStringLiteral("storage_root"), root},
            {QStringLiteral("text_size_percent"), textSizePercent}};
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
        return fail(1, "Unable to create temporary directory.");

    const QString preferencesPath = temporaryDirectory.filePath(QStringLiteral("preferences.json"));
    const QString root = temporaryDirectory.path();
    ApplicationStateStore store;
    int changedCount = 0;
    QObject::connect(&store, &ApplicationStateStore::changed, &store, [&changedCount] { ++changedCount; });
    SettingsRepository repository(preferencesPath, store);

    if (!repository.load())
        return fail(2, "Missing preferences did not load defaults.");
    const QString expectedDefaultRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                            .filePath(QStringLiteral("OpenDropletSortingSuite"));
    if (store.snapshot().preferences.storageRoot != expectedDefaultRoot
        || store.snapshot().preferences.textSizePercent != 100 || QFile::exists(preferencesPath)) {
        return fail(3, "Missing preferences did not publish unwritten defaults.");
    }

    if (!repository.setStorageRoot(root) || !repository.setTextSizePercent(125))
        return fail(4, "Valid preferences did not save.");
    QFile savedFile(preferencesPath);
    if (!savedFile.open(QIODevice::ReadOnly))
        return fail(5, "Unable to read saved preferences.");
    const QJsonDocument savedDocument = QJsonDocument::fromJson(savedFile.readAll());
    savedFile.close();
    if (!savedDocument.isObject() || savedDocument.object().size() != 3
        || !savedDocument.object().contains(QStringLiteral("schema_version"))
        || !savedDocument.object().contains(QStringLiteral("storage_root"))
        || !savedDocument.object().contains(QStringLiteral("text_size_percent"))) {
        return fail(5, "Saved preferences did not use exactly the required keys.");
    }

    ApplicationStateStore roundTripStore;
    SettingsRepository roundTrip(preferencesPath, roundTripStore);
    if (!roundTrip.load() || roundTripStore.snapshot().preferences.storageRoot != root
        || roundTripStore.snapshot().preferences.textSizePercent != 125) {
        return fail(6, "Valid preferences did not round trip.");
    }
    for (const int supportedTextSize : {80, 100, 125}) {
        if (!repository.setTextSizePercent(supportedTextSize)
            || store.snapshot().preferences.textSizePercent != supportedTextSize) {
            return fail(7, "Supported text sizes did not save.");
        }
    }
    if (!repository.setTextSizePercent(125))
        return fail(7, "Unable to restore the supported text size.");
    if (repository.setTextSizePercent(79) || repository.setTextSizePercent(110)
        || repository.setTextSizePercent(126) || repository.setTextSizePercent(201)
        || store.snapshot().preferences.textSizePercent != 125) {
        return fail(7, "Out-of-range text sizes changed preferences.");
    }
    for (const auto &[legacyValue, expectedValue] : std::initializer_list<std::pair<int, int>>{
             {90, 100}, {150, 125}, {175, 125}, {200, 125}}) {
        if (!writeDocument(preferencesPath, validDocument(root, legacyValue)))
            return fail(8, "Unable to write legacy preferences.");
        QString legacyError;
        if (!repository.load(&legacyError)) {
            std::cerr << legacyError.toStdString() << '\n';
            return fail(8, "Legacy preferences did not normalize.");
        }
        if (store.snapshot().preferences.textSizePercent != expectedValue) {
            return fail(8, "Legacy preferences did not normalize.");
        }
        QFile normalizedFile(preferencesPath);
        const QJsonDocument normalizedDocument = normalizedFile.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(normalizedFile.readAll())
            : QJsonDocument();
        if (!normalizedDocument.isObject()
            || normalizedDocument.object().value(QStringLiteral("text_size_percent")).toInt()
                != expectedValue) {
            return fail(8, "Legacy preferences were published before normalization persisted.");
        }
    }
    if (!repository.setTextSizePercent(125))
        return fail(8, "Unable to restore normalized preferences.");
    {
        QFile malformed(preferencesPath);
        if (!malformed.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || malformed.write("not json") != 8) {
            return fail(8, "Unable to write malformed preferences.");
        }
        malformed.close();
        if (repository.load() || store.snapshot().preferences.textSizePercent != 125) {
            return fail(8, "Malformed preferences replaced the last valid state.");
        }
    }

    const QList<QJsonObject> invalidDocuments{
        {{QStringLiteral("schema_version"), 2}, {QStringLiteral("storage_root"), root}, {QStringLiteral("text_size_percent"), 125}},
        {{QStringLiteral("schema_version"), QStringLiteral("1")}, {QStringLiteral("storage_root"), root}, {QStringLiteral("text_size_percent"), 125}},
        {{QStringLiteral("schema_version"), 1}, {QStringLiteral("storage_root"), 7}, {QStringLiteral("text_size_percent"), 125}},
        {{QStringLiteral("schema_version"), 1}, {QStringLiteral("storage_root"), root}, {QStringLiteral("text_size_percent"), QStringLiteral("125")}},
        {{QStringLiteral("schema_version"), 1}, {QStringLiteral("storage_root"), root}, {QStringLiteral("text_size_percent"), 125.5}},
        {{QStringLiteral("schema_version"), 1}, {QStringLiteral("storage_root"), root}},
        {{QStringLiteral("schema_version"), 1}, {QStringLiteral("storage_root"), root}, {QStringLiteral("text_size_percent"), 125}, {QStringLiteral("extra"), true}},
        validDocument(QStringLiteral("relative-root"), 125),
        validDocument(temporaryDirectory.filePath(QStringLiteral("missing-root")), 125)};
    for (const QJsonObject &invalid : invalidDocuments) {
        if (!writeDocument(preferencesPath, invalid) || repository.load()
            || store.snapshot().preferences.storageRoot != root
            || store.snapshot().preferences.textSizePercent != 125) {
            return fail(9, "Invalid preferences replaced the last valid state.");
        }
    }
    if (!writeDocument(preferencesPath, validDocument(root, 125)))
        return fail(10, "Unable to restore valid preferences.");
    const QString ordinaryFilePath = temporaryDirectory.filePath(QStringLiteral("not-a-directory"));
    QFile ordinaryFile(ordinaryFilePath);
    if (!ordinaryFile.open(QIODevice::WriteOnly) || ordinaryFile.write("file") != 4)
        return fail(10, "Unable to prepare a non-directory storage root.");
    ordinaryFile.close();
    if (repository.setStorageRoot(QString()) || repository.setStorageRoot(QStringLiteral("relative-root"))
        || repository.setStorageRoot(temporaryDirectory.filePath(QStringLiteral("missing-root")))
        || repository.setStorageRoot(ordinaryFilePath)
        || store.snapshot().preferences.storageRoot != root) {
        return fail(11, "Invalid roots changed preferences.");
    }

    const int changesBeforeFailedSave = changedCount;
    if (!QFile::remove(preferencesPath) || !QDir().mkdir(preferencesPath) || repository.setTextSizePercent(125)
        || store.snapshot().preferences.textSizePercent != 125 || changedCount != changesBeforeFailedSave) {
        return fail(12, "Failed atomic save published a candidate state.");
    }

    return 0;
}
