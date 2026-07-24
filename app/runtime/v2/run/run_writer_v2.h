#pragma once

#include "run_manifest_v2.h"

#include <QByteArray>
#include <QFile>

#include <memory>
#include <optional>

namespace desktop_app::v2::run {

struct RunWriterV2TestAccess;

class RunWriterV2 {
  public:
    static std::optional<RunWriterV2> start(const QString& runFolder,
                                            RunManifestData initialData,
                                            QString* error = nullptr);

    RunWriterV2(RunWriterV2&&) noexcept;
    RunWriterV2& operator=(RunWriterV2&&) noexcept;
    ~RunWriterV2();

    RunWriterV2(const RunWriterV2&) = delete;
    RunWriterV2& operator=(const RunWriterV2&) = delete;

    bool appendEvent(const RunEvent& event, const QByteArray& cropBytes,
                     QString* error = nullptr);
    bool flush(QString* error = nullptr);
    bool finalize(RunStatus status, const QString& endedAt, const QString& stopReason,
                  double achievedProcessingFps, QString* error = nullptr);

    const RunManifestData& data() const noexcept;

  private:
    friend struct RunWriterV2TestAccess;

    RunWriterV2(QString runFolder, RunManifestData data, std::unique_ptr<QFile> partialFile);

    QString runFolder_;
    RunManifestData data_;
    std::unique_ptr<QFile> partialFile_;
    bool finalized_ = false;
    bool failNextCsvAppendForTest_ = false;
};

} // namespace desktop_app::v2::run
