#include "background_task_registry.h"

#include <utility>

BackgroundTaskRegistry::~BackgroundTaskRegistry() {
    requestStop();
    waitAll();
}

BackgroundTaskRegistry::StopFlag BackgroundTaskRegistry::launch(QString name, TaskBody body) {
    auto stop = std::make_shared<std::atomic_bool>(false);
    auto complete = std::make_shared<std::atomic_bool>(false);
    std::thread thread([stop, complete, body = std::move(body)]() mutable {
        body(stop);
        complete->store(true);
    });
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(Task{std::move(name), stop, complete, std::move(thread)});
    reapCompletedLocked();
    return stop;
}

void BackgroundTaskRegistry::requestStop() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& task : tasks_) {
        task.stop->store(true);
    }
}

void BackgroundTaskRegistry::waitAll() {
    std::vector<std::thread> toJoin;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& task : tasks_) {
            if (task.thread.joinable()) {
                toJoin.push_back(std::move(task.thread));
            }
        }
        tasks_.clear();
    }
    for (auto& thread : toJoin) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void BackgroundTaskRegistry::reapCompleted() {
    std::lock_guard<std::mutex> lock(mutex_);
    reapCompletedLocked();
}

void BackgroundTaskRegistry::reapCompletedLocked() {
    for (auto it = tasks_.begin(); it != tasks_.end();) {
        if (it->complete->load()) {
            if (it->thread.joinable()) {
                it->thread.join();
            }
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}
