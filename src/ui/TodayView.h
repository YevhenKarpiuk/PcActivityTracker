#pragma once
#include "reports/ReportEngine.h"
#include "ui/ActivityDataCache.h"

#include <chrono>
#include <cstdint>

namespace pcat {
// Вкладка «Сегодня»: итоги дня считает ReportEngine, здесь только отображение.
class TodayView {
public:
    void draw(const ActivityMonitor& monitor, const ActivityDataCache& data);

private:
    TodaySummary summary_;
    std::uint64_t generation_{UINT64_MAX};
    std::chrono::steady_clock::time_point lastSummary_{};
};
} // namespace pcat
