#pragma once
#include "core/AppSettings.h"
#include "core/CategoryResolver.h"
#include "core/IActivityProvider.h"
#include "core/IActivityRepository.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace pcat {
class ActivityMonitor {
public:
    using RecordCallback = std::function<void(const ActivityRecord&)>;
    ActivityMonitor(IActivityProvider& provider, IActivityRepository& repository, AppSettings settings);
    ~ActivityMonitor() noexcept;
    void start();
    void stop() noexcept;
    void setPaused(bool value);
    // Pauses capture, closes the current interval and verifies that all queued records are durable.
    // Returns false if persistence failed; the monitor remains paused so destructive history operations
    // cannot be followed by a late retry that resurrects deleted data.
    bool pauseAndFlushForMaintenance() noexcept;
    bool isPaused() const { return paused_.load(); }
    bool isRunning() const { return running_.load(); }
    void setSettings(const AppSettings& settings);
    void setRecordCallback(RecordCallback cb);
    std::optional<ActivityRecord> currentRecordPreview(std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
    std::string lastError() const;
private:
    void loop(std::stop_token token);
    void process();
    void closeCurrent(std::chrono::system_clock::time_point end) noexcept;
    bool different(const ActivitySnapshot& a,const ActivitySnapshot& b,const std::string& ca,const std::string& cb) const;
    ActivityRecord makeRecord(const ActivitySnapshot& snapshot,const std::string& category,std::chrono::system_clock::time_point start,std::chrono::system_clock::time_point end) const;
    bool flushPending();
    void checkpointCurrent(std::chrono::system_clock::time_point now);
    void setError(std::string message);

    IActivityProvider& provider_;
    IActivityRepository& repository_;
    mutable std::mutex mutex_;
    std::mutex processMutex_;
    std::mutex persistenceMutex_;
    AppSettings settings_;
    CategoryIndex categoryIndex_;
    std::optional<ActivitySnapshot> current_;
    std::string currentCategory_;
    std::chrono::system_clock::time_point currentStart_{};
    std::chrono::system_clock::time_point lastCheckpoint_{};
    std::deque<ActivityRecord> pending_;
    RecordCallback callback_;
    std::string lastError_;
    std::jthread thread_;
    std::atomic_bool running_{false};
    std::atomic_bool paused_{false};
};
}
