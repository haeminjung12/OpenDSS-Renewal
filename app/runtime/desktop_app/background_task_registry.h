#pragma once

#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class BackgroundTaskRegistry {
public:
    using StopFlag = std::shared_ptr<std::atomic_bool>;
    using TaskBody = std::function<void(const StopFlag&)>;

    BackgroundTaskRegistry() = default;
    ~BackgroundTaskRegistry();

    BackgroundTaskRegistry(const BackgroundTaskRegistry&) = delete;
    BackgroundTaskRegistry& operator=(const BackgroundTaskRegistry&) = delete;

    StopFlag launch(QString name, TaskBody body);
    void requestStop();
    void waitAll();
    void reapCompleted();

private:
    struct Task {
        QString name;
        StopFlag stop;
        std::shared_ptr<std::atomic_bool> complete;
        std::thread thread;
    };

    void reapCompletedLocked();

    std::mutex mutex_;
    std::vector<Task> tasks_;
};
