#include "json_persistence.h"

#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QThread>

namespace desktop_app {

bool writeJsonDocumentAtomically(const QString& path, const QJsonDocument& document, QString* error,
                                 QJsonDocument::JsonFormat format) {
    if (error)
        error->clear();

    const QFileInfo targetInfo(path);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        if (error)
            *error = QString("Unable to create JSON output directory: %1")
                         .arg(QDir::toNativeSeparators(targetInfo.absolutePath()));
        return false;
    }

    const QByteArray bytes = document.toJson(format);
    QString lastError;
    for (int attempt = 0; attempt < 3; ++attempt) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            lastError = QString("Unable to open JSON file for writing: %1 (%2)")
                            .arg(QDir::toNativeSeparators(path), file.errorString());
        } else {
            const qint64 bytesWritten = file.write(bytes);
            if (bytesWritten != bytes.size()) {
                lastError = QString("Incomplete JSON write to %1: wrote %2 of %3 bytes (%4)")
                                .arg(QDir::toNativeSeparators(path))
                                .arg(bytesWritten)
                                .arg(bytes.size())
                                .arg(file.errorString());
                file.cancelWriting();
            } else if (file.commit()) {
                return true;
            } else {
                lastError = QString("Unable to commit JSON file: %1 (%2)")
                                .arg(QDir::toNativeSeparators(path), file.errorString());
            }
        }
        if (attempt < 2)
            QThread::msleep(50 * (attempt + 1));
    }
    if (error)
        *error = lastError;
    return false;
}

bool writeJsonObjectAtomically(const QString& path, const QJsonObject& object, QString* error,
                               QJsonDocument::JsonFormat format) {
    return writeJsonDocumentAtomically(path, QJsonDocument(object), error, format);
}

} // namespace desktop_app
