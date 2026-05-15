#pragma once

#include <QString>

#include <vector>

#include "sequence_summary_writer.h"

QString writeLiveLogCsv(const QString& outDir,
                        const QString& prefix,
                        const std::vector<LiveLogRecord>& records);

QString writeLiveSequenceLog(const QString& outDir,
                             const QString& timestamp,
                             const std::vector<LiveLogRecord>& records,
                             const SequenceLogMetadata& metadata);
