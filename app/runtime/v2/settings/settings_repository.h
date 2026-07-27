#pragma once

#include "../state/domain_state.h"

#include <QString>

#include <array>
#include <cstddef>

namespace desktop_app::v2 {

class ApplicationStateStore;

enum class OutputRootSelector : std::size_t {
    CaptureSingle,
    CaptureSequence,
    CaptureDataset,
    Train,
    ModelTest,
    Live,
    SequenceTest,
    LibraryCreate,
    LibraryExport
};

class SettingsRepository final
{
public:
    SettingsRepository(QString preferencesFilePath, ApplicationStateStore &stateStore);

    bool load(QString *error = nullptr);
    bool setStorageRoot(const QString &storageRoot, QString *error = nullptr);
    bool setTextSizePercent(int textSizePercent, QString *error = nullptr);
    QString outputRoot(OutputRootSelector selector) const;
    bool outputRootFellBack(OutputRootSelector selector) const;
    QString outputRootFallbackReason(OutputRootSelector selector) const;
    bool setOutputRoot(OutputRootSelector selector, const QString &outputRoot,
                       QString *error = nullptr);

private:
    static constexpr std::size_t OutputRootCount = 9;
    using OutputRoots = std::array<QString, OutputRootCount>;

    bool save(const PreferencesState &preferences, const OutputRoots &outputRoots,
              QString *error) const;

    QString preferencesFilePath_;
    ApplicationStateStore &stateStore_;
    OutputRoots outputRoots_;
    OutputRoots loadFallbackReasons_;
};

} // namespace desktop_app::v2
