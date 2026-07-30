#pragma once

#include <QImage>

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "frame_types.h"

class LiveFrameDispatcher {
  public:
    struct Range {
        std::uint64_t first = 0;
        std::uint64_t last = 0;
    };

    struct Integrity {
        std::uint64_t handoffAccepted = 0;
        std::uint64_t sourceGapCount = 0;
        std::uint64_t queueRejectedCount = 0;
        std::uint64_t consumerFailureCount = 0;
        std::vector<Range> sourceGaps;
        std::vector<Range> queueRejected;
        std::vector<Range> consumerFailures;
    };

    struct OfferResult {
        bool accepted = false;
        std::uint64_t handoffId = 0;
        Integrity delta;
    };

    struct Membership {
        bool recording = false;
        bool sequenceRunning = false;
        bool pipelineEnabled = false;
        bool collection = false;
        bool datasetCapture = false;
        bool liveLogging = false;
    };

    using Consumer = std::function<void(const QImage&, const FrameMeta&, double, std::uint64_t, Membership)>;

    explicit LiveFrameDispatcher(Consumer consumer) : consumer_(std::move(consumer)), worker_([this] { run(); }) {}
    ~LiveFrameDispatcher() {
        stopAndDrain();
    }

    LiveFrameDispatcher(const LiveFrameDispatcher&) = delete;
    LiveFrameDispatcher& operator=(const LiveFrameDispatcher&) = delete;

    OfferResult offer(const QImage& image, const FrameMeta& meta, double fps, Membership membership) {
        std::lock_guard<std::mutex> lock(mutex_);
        OfferResult result;
        result.handoffId = nextId_++;
        membership.collection = membership.collection && collectionOpen_;
        membership.datasetCapture = membership.datasetCapture && datasetOpen_;
        auto deliveryDelta = [&](bool member, bool& haveDelivered, std::uint64_t& lastDelivered) {
            Integrity delta;
            if (!member)
                return delta;
            if (haveDelivered && meta.delivered > lastDelivered + 1) {
                addRange(delta.sourceGaps, lastDelivered + 1, meta.delivered - 1);
                delta.sourceGapCount = meta.delivered - lastDelivered - 1;
            }
            lastDelivered = meta.delivered;
            haveDelivered = true;
            return delta;
        };
        Integrity collectionDelta =
            deliveryDelta(membership.collection, collectionHaveDelivered_, collectionLastDelivered_);
        Integrity datasetDelta =
            deliveryDelta(membership.datasetCapture, datasetHaveDelivered_, datasetLastDelivered_);
        if (!accepting_ || queue_.size() == kCapacity) {
            if (membership.collection) {
                collectionDelta.queueRejectedCount = 1;
                addRange(collectionDelta.queueRejected, result.handoffId, result.handoffId);
            }
            if (membership.datasetCapture) {
                datasetDelta.queueRejectedCount = 1;
                addRange(datasetDelta.queueRejected, result.handoffId, result.handoffId);
            }
            merge(collectionSession_, collectionDelta);
            merge(datasetSession_, datasetDelta);
            merge(result.delta, membership.collection ? collectionDelta : datasetDelta);
            return result;
        }
        result.accepted = true;
        lastAcceptedId_ = result.handoffId;
        if (membership.collection) {
            ++collectionSession_.handoffAccepted;
            lastCollectionAcceptedId_ = result.handoffId;
        }
        if (membership.datasetCapture) {
            ++datasetSession_.handoffAccepted;
            lastDatasetAcceptedId_ = result.handoffId;
        }
        merge(collectionSession_, collectionDelta);
        merge(datasetSession_, datasetDelta);
        merge(result.delta, membership.collection ? collectionDelta : datasetDelta);
        queue_.push_back({image.copy(), meta, fps, result.handoffId, membership});
        ready_.notify_one();
        return result;
    }

    std::uint64_t closeCollectionBoundary() {
        std::lock_guard<std::mutex> lock(mutex_);
        collectionOpen_ = false;
        return lastCollectionAcceptedId_;
    }

    void openCollectionBoundary() {
        std::lock_guard<std::mutex> lock(mutex_);
        collectionOpen_ = true;
        collectionSession_ = {};
        collectionHaveDelivered_ = false;
        lastCollectionAcceptedId_ = 0;
    }

    std::uint64_t closeDatasetBoundary() {
        std::lock_guard<std::mutex> lock(mutex_);
        datasetOpen_ = false;
        return lastDatasetAcceptedId_;
    }

    void openDatasetBoundary() {
        std::lock_guard<std::mutex> lock(mutex_);
        datasetOpen_ = true;
        datasetSession_ = {};
        datasetHaveDelivered_ = false;
        lastDatasetAcceptedId_ = 0;
    }

    void resumeDatasetBoundary() {
        std::lock_guard<std::mutex> lock(mutex_);
        datasetOpen_ = true;
    }

    void waitThrough(std::uint64_t checkpoint) {
        std::unique_lock<std::mutex> lock(mutex_);
        drained_.wait(lock, [this, checkpoint] {
            return completedId_ >= checkpoint || (!accepting_ && queue_.empty() && !consuming_);
        });
    }

    void stopAndDrain() {
        std::lock_guard<std::mutex> stopLock(stopMutex_);
        std::uint64_t checkpoint = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            accepting_ = false;
            checkpoint = lastAcceptedId_;
            ready_.notify_all();
        }
        waitThrough(checkpoint);
        if (worker_.joinable())
            worker_.join();
    }

    Integrity integrity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return collectionSession_;
    }

    Integrity datasetIntegrity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return datasetSession_;
    }

    bool faulted() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return faulted_;
    }

    static constexpr std::size_t capacity() {
        return kCapacity;
    }

  private:
    struct Item {
        QImage image;
        FrameMeta meta;
        double fps = 0.0;
        std::uint64_t id = 0;
        Membership membership;
    };

    static constexpr std::size_t kCapacity = 16;

    static void addRange(std::vector<Range>& ranges, std::uint64_t first, std::uint64_t last) {
        if (!ranges.empty() && first <= ranges.back().last + 1) {
            ranges.back().last = (std::max)(ranges.back().last, last);
        } else {
            ranges.push_back({first, last});
        }
    }

    static void merge(Integrity& target, const Integrity& value) {
        target.sourceGapCount += value.sourceGapCount;
        target.queueRejectedCount += value.queueRejectedCount;
        target.consumerFailureCount += value.consumerFailureCount;
        for (const auto& range : value.sourceGaps)
            addRange(target.sourceGaps, range.first, range.last);
        for (const auto& range : value.queueRejected)
            addRange(target.queueRejected, range.first, range.last);
        for (const auto& range : value.consumerFailures)
            addRange(target.consumerFailures, range.first, range.last);
    }

    void recordConsumerFailureLocked(const Item& failedItem) {
        if (failedItem.membership.collection) {
            ++collectionSession_.consumerFailureCount;
            addRange(collectionSession_.consumerFailures, failedItem.id, failedItem.id);
        }
        if (failedItem.membership.datasetCapture) {
            ++datasetSession_.consumerFailureCount;
            addRange(datasetSession_.consumerFailures, failedItem.id, failedItem.id);
        }
        for (const auto& pending : queue_) {
            if (pending.membership.collection) {
                ++collectionSession_.consumerFailureCount;
                addRange(collectionSession_.consumerFailures, pending.id, pending.id);
            }
            if (pending.membership.datasetCapture) {
                ++datasetSession_.consumerFailureCount;
                addRange(datasetSession_.consumerFailures, pending.id, pending.id);
            }
        }
        queue_.clear();
        accepting_ = false;
        faulted_ = true;
        completedId_ = lastAcceptedId_;
    }

    void run() {
        for (;;) {
            Item item;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return !queue_.empty() || !accepting_; });
                if (queue_.empty() && !accepting_)
                    break;
                item = std::move(queue_.front());
                queue_.pop_front();
                consuming_ = true;
            }

            bool failed = false;
            try {
                consumer_(item.image, item.meta, item.fps, item.id, item.membership);
            } catch (...) {
                failed = true;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (failed) {
                    recordConsumerFailureLocked(item);
                } else {
                    completedId_ = (std::max)(completedId_, item.id);
                }
                consuming_ = false;
                drained_.notify_all();
                ready_.notify_all();
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        drained_.notify_all();
    }

    Consumer consumer_;
    mutable std::mutex mutex_;
    std::mutex stopMutex_;
    std::condition_variable ready_, drained_;
    std::deque<Item> queue_;
    std::thread worker_;
    std::uint64_t nextId_ = 1;
    std::uint64_t lastAcceptedId_ = 0;
    std::uint64_t lastCollectionAcceptedId_ = 0;
    std::uint64_t lastDatasetAcceptedId_ = 0;
    std::uint64_t completedId_ = 0;
    std::uint64_t collectionLastDelivered_ = 0;
    std::uint64_t datasetLastDelivered_ = 0;
    bool accepting_ = true;
    bool consuming_ = false;
    bool collectionOpen_ = true;
    bool datasetOpen_ = true;
    bool collectionHaveDelivered_ = false;
    bool datasetHaveDelivered_ = false;
    bool faulted_ = false;
    Integrity collectionSession_;
    Integrity datasetSession_;
};
