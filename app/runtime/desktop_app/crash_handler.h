#pragma once

#include <QString>

void initializeCrashAndLogHandling(const QString& logPath);
void logMessage(const QString& msg);
void logMessageNoPrune(const QString& msg);
void installLogTees();
