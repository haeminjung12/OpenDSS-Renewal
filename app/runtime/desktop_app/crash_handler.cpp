#include "crash_handler.h"

#include "log_teebuf.h"

#include <QtCore>

#include <atomic>
#include <exception>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

namespace {
QMutex gLogMutex;
QFile gLogFile;
std::atomic<bool> gCrashHandled(false);

void pruneLogs() {
    QFileInfo fi(gLogFile);
    QString baseDir = fi.dir().absolutePath();
    QString baseName = "session_log";
    QStringList files = QDir(baseDir).entryList(QStringList() << (baseName + "*.txt"), QDir::Files, QDir::Time);
    for (int i = 50; i < files.size(); ++i) {
        QString path = baseDir + "/" + files[i];
        QFile file(path);
        if (!file.remove()) {
            logMessageNoPrune(QString("Failed to remove log file %1: %2").arg(path, file.errorString()));
        }
    }
}

void qtLogHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    Q_UNUSED(ctx);
    QString level;
    switch (type) {
        case QtDebugMsg: level="DEBUG"; break;
        case QtInfoMsg: level="INFO"; break;
        case QtWarningMsg: level="WARN"; break;
        case QtCriticalMsg: level="CRIT"; break;
        case QtFatalMsg: level="FATAL"; break;
    }
    logMessage(QString("[%1] %2").arg(level, msg));
}

void termHandler() {
    logMessage("std::terminate called");
    std::_Exit(1);
}

#ifdef _WIN32
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info) {
    if (gCrashHandled.exchange(true)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void* addr = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    logMessage(QString("Unhandled exception: code=0x%1 addr=0x%2")
        .arg(code, 8, 16, QChar('0'))
        .arg(reinterpret_cast<quintptr>(addr), sizeof(quintptr) * 2, 16, QChar('0')));

    wchar_t basePath[MAX_PATH] = {};
    DWORD len = GetTempPathW(MAX_PATH, basePath);
    if (len == 0 || len >= MAX_PATH) {
        DWORD mlen = GetModuleFileNameW(nullptr, basePath, MAX_PATH);
        if (mlen > 0 && mlen < MAX_PATH) {
            for (DWORD i = mlen; i > 0; --i) {
                if (basePath[i] == L'\\' || basePath[i] == L'/') {
                    basePath[i + 1] = L'\0';
                    break;
                }
            }
        } else {
            basePath[0] = L'.';
            basePath[1] = L'\\';
            basePath[2] = L'\0';
        }
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t fileName[128] = {};
    swprintf(fileName, 128, L"droplet_crash_%04d%02d%02d_%02d%02d%02d.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    wchar_t dumpPath[MAX_PATH] = {};
    swprintf(dumpPath, MAX_PATH, L"%s%s", basePath, fileName);

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpNormal, info ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(hFile);
        logMessage(QString("Crash dump saved: %1").arg(QString::fromWCharArray(dumpPath)));
    } else {
        DWORD err = GetLastError();
        logMessage(QString("Failed to create crash dump file. err=%1").arg(err));
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif
} // namespace

void logMessage(const QString& msg) {
    QMutexLocker locker(&gLogMutex);
    if (!gLogFile.isOpen()) {
        if (!gLogFile.open(QIODevice::Append | QIODevice::Text)) return;
    }
    QTextStream ts(&gLogFile);
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + " " + msg;
    ts << line << "\n";
    ts.flush();
    gLogFile.close();
    pruneLogs();
}

void logMessageNoPrune(const QString& msg) {
    QMutexLocker locker(&gLogMutex);
    if (!gLogFile.isOpen()) {
        if (!gLogFile.open(QIODevice::Append | QIODevice::Text)) return;
    }
    QTextStream ts(&gLogFile);
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + " " + msg;
    ts << line << "\n";
    ts.flush();
    gLogFile.close();
}

void installLogTees() {
    static LogTeeBuf loggerBuf(std::cout.rdbuf(), [](const QString& m){ logMessage(m); });
    static std::ostream loggerStream(&loggerBuf);
    std::cout.rdbuf(loggerStream.rdbuf());
    std::cerr.rdbuf(loggerStream.rdbuf());
}

void initializeCrashAndLogHandling(const QString& logPath) {
    gLogFile.setFileName(logPath);
    if (QFile::exists(logPath)) QFile::remove(logPath);
    pruneLogs();
    qInstallMessageHandler(qtLogHandler);
    std::set_terminate(termHandler);
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
    installLogTees();
}
