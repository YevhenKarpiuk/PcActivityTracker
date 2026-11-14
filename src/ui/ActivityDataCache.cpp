#include "ui/ActivityDataCache.h"

#include "ui/UiCommon.h"

#include <algorithm>

namespace pcat {
namespace {
constexpr auto kReloadInterval = std::chrono::seconds(5);
}

bool ActivityDataCache::reloadDue() const {
    return dirty_.load() && std::chrono::steady_clock::now() - lastReload_ >= kReloadInterval;
}

void ActivityDataCache::setHistoryDays(int days) noexcept { historyDays_ = std::clamp(days, 1, kMaxHistoryDays); }

void ActivityDataCache::reload(ActivityMonitor& monitor) {
    dirty_.store(false);
    try {
        const auto now = std::chrono::system_clock::now();
        const auto start = now - std::chrono::hours(24LL * historyDays_);
        auto records = monitor.readSnapshot(start, now + std::chrono::seconds(1)).records;
        std::vector<ActivityRecord> today;
        const auto midnight = localMidnight(now);
        for (const auto& record : records)
            if (record.endTime > midnight) today.push_back(record);
        history_ = std::move(records);
        today_ = std::move(today);
        historyStart_ = start;
        lastReload_ = std::chrono::steady_clock::now();
        ++generation_;
    } catch (...) {
        dirty_.store(true);
        lastReload_ = std::chrono::steady_clock::now();
        throw;
    }
}
} // namespace pcat
