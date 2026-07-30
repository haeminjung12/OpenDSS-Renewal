#include "single_image_capture_service.h"

#include "frame_conversion.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QRegularExpression>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <unistd.h>
#endif

namespace desktop_app::v2 {
namespace {

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString captureName(const QString &requested)
{
    QString leafPath = requested.trimmed();
    leafPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString stem = QFileInfo(leafPath).fileName();
    static const QRegularExpression tifSuffix(QStringLiteral(R"((?:\.tiff?)+$)"),
                                               QRegularExpression::CaseInsensitiveOption);
    stem.remove(tifSuffix);
    stem.replace(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N} _.-])")), QStringLiteral("_"));
    stem.remove(QRegularExpression(QStringLiteral(R"(^[ ._]+|[ ._]+$)")));
    if (stem.isEmpty()) {
        stem = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    }
    static const QRegularExpression reservedWindowsName(
        QStringLiteral(R"(^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (reservedWindowsName.match(stem).hasMatch()) {
        stem.prepend(QLatin1Char('_'));
    }
    return stem + QStringLiteral(".tif");
}

enum class PublishResult {
    Published,
    TargetExists,
    Failed,
};

PublishResult publishWithoutReplace(const QString &temporaryPath,
                                    const QString &targetPath,
                                    QString *detail)
{
#ifdef Q_OS_WIN
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()),
                    reinterpret_cast<LPCWSTR>(targetPath.utf16()),
                    MOVEFILE_WRITE_THROUGH)) {
        return PublishResult::Published;
    }
    const DWORD code = GetLastError();
    if (code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS) {
        return PublishResult::TargetExists;
    }
    if (detail) {
        *detail = QStringLiteral("Windows error %1").arg(code);
    }
    return PublishResult::Failed;
#else
    const QByteArray temporaryNative = QFile::encodeName(temporaryPath);
    const QByteArray targetNative = QFile::encodeName(targetPath);
    if (::link(temporaryNative.constData(), targetNative.constData()) == 0) {
        if (::unlink(temporaryNative.constData()) == 0) {
            return PublishResult::Published;
        }
        const int unlinkError = errno;
        if (detail) {
            *detail =
                QStringLiteral("The final target was published, but the temporary file at %1 "
                               "could not be removed: %2")
                    .arg(QDir::toNativeSeparators(temporaryPath),
                         QString::fromLocal8Bit(std::strerror(unlinkError)));
        }
        return PublishResult::Failed;
    }
    if (errno == EEXIST) {
        return PublishResult::TargetExists;
    }
    if (detail) {
        *detail = QString::fromLocal8Bit(std::strerror(errno));
    }
    return PublishResult::Failed;
#endif
}

void cleanFailedPublicationTemporary(const QString &temporaryPath, QString *detail)
{
    QFile temporary(temporaryPath);
    if (!temporary.exists() || temporary.remove()) {
        return;
    }

    const QString retained =
        QStringLiteral(" The temporary file was retained at %1: %2")
            .arg(QDir::toNativeSeparators(temporaryPath), temporary.errorString());
    if (detail) {
        detail->append(retained);
    }
}

} // namespace

bool SingleImageCaptureService::save(const CameraFrame &frame,
                                     const QString &saveDirectory,
                                     const QString &requestedFileName,
                                     QString *savedPath,
                                     QString *error) const
{
    if (savedPath) {
        savedPath->clear();
    }
    setError(error, {});

    const QFileInfo directoryInfo(saveDirectory);
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        setError(error, QStringLiteral("The selected save location is not a directory."));
        return false;
    }
    if (!directoryInfo.isWritable()) {
        setError(error, QStringLiteral("The selected save location is not writable."));
        return false;
    }

    QString conversionError;
    const QImage image = convertCameraFrame(frame, &conversionError);
    if (image.isNull()) {
        setError(error, conversionError);
        return false;
    }

    const QString target =
        QDir(saveDirectory).absoluteFilePath(captureName(requestedFileName));
    if (QFileInfo::exists(target)) {
        setError(error, QStringLiteral("A file already exists at the requested capture path: %1")
                            .arg(QDir::toNativeSeparators(target)));
        return false;
    }

    QString temporaryPath;
    {
        QTemporaryFile file(
            QDir(saveDirectory).absoluteFilePath(QStringLiteral(".opendss-capture-XXXXXX.tmp")));
        file.setAutoRemove(true);
        if (!file.open()) {
            setError(error,
                     QStringLiteral("A temporary capture file could not be opened: %1")
                         .arg(file.errorString()));
            return false;
        }

        QImageWriter writer(&file, "tiff");
        if (!writer.write(image)) {
            const QString writerError = writer.errorString();
            setError(error,
                     QStringLiteral("The TIFF image could not be written: %1").arg(writerError));
            return false;
        }
        if (!file.flush()) {
            setError(error,
                     QStringLiteral("The temporary capture file could not be flushed: %1")
                         .arg(file.errorString()));
            return false;
        }
        temporaryPath = file.fileName();
        file.close();
        file.setAutoRemove(false);
    }

    QString publishDetail;
    const PublishResult publishResult =
        publishWithoutReplace(temporaryPath, target, &publishDetail);
    if (publishResult == PublishResult::TargetExists) {
        cleanFailedPublicationTemporary(temporaryPath, &publishDetail);
        setError(error, QStringLiteral("A file already exists at the requested capture path: %1")
                            .arg(QDir::toNativeSeparators(target))
                            + publishDetail);
        return false;
    }
    if (publishResult == PublishResult::Failed) {
        cleanFailedPublicationTemporary(temporaryPath, &publishDetail);
        setError(error,
                 QStringLiteral("The completed capture could not be published: %1")
                     .arg(publishDetail));
        return false;
    }
    if (savedPath) {
        *savedPath = QFileInfo(target).absoluteFilePath();
    }
    return true;
}

} // namespace desktop_app::v2
