#pragma once
#include "core/ActivityMonitor.h"
#include "core/ActivityRecord.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

namespace pcat {
// Снимок истории для UI. Читается из базы вне кадра рендеринга и только по необходимости:
// каждый закрытый интервал помечает данные грязными, но перечитывать за это всю историю
// (до 3650 дней) на каждое событие нельзя, поэтому фоновые обновления коалесцируются.
class ActivityDataCache {
public:
    static constexpr int kMaxHistoryDays = 3650;

    // Можно вызывать из потока мониторинга.
    void markDirty() noexcept { dirty_.store(true); }
    // Пора ли фоновое перечитывание: данные грязные и прошло не меньше интервала коалесценции.
    bool reloadDue() const;
    // Перечитывает немедленно. Бросает исключение при ошибке базы; данные тогда остаются грязными.
    void reload(ActivityMonitor& monitor);

    void setHistoryDays(int days) noexcept;
    int historyDays() const noexcept { return historyDays_; }
    std::chrono::system_clock::time_point historyStart() const noexcept { return historyStart_; }
    const std::vector<ActivityRecord>& history() const noexcept { return history_; }
    const std::vector<ActivityRecord>& today() const noexcept { return today_; }
    // Меняется при каждом перечитывании: представления по нему понимают, что пора пересчитать своё.
    std::uint64_t generation() const noexcept { return generation_; }

private:
    std::atomic_bool dirty_{true};
    int historyDays_{30};
    std::chrono::system_clock::time_point historyStart_{};
    std::chrono::steady_clock::time_point lastReload_{};
    std::vector<ActivityRecord> history_;
    std::vector<ActivityRecord> today_;
    std::uint64_t generation_{0};
};
} // namespace pcat
