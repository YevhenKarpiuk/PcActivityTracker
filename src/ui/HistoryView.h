#pragma once
#include "core/IActivityRepository.h"
#include "ui/ActivityDataCache.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pcat {
// Вкладка «История»: фильтры, экспорт и удаление записей.
class HistoryView {
public:
    struct Context {
        ActivityMonitor& monitor;
        IActivityRepository& repository;
        ActivityDataCache& data;
        const std::filesystem::path& exportDirectory; // каталог данных, а не CWD процесса
        std::string& status;
    };
    void draw(Context& context);

private:
    std::array<char, 128> process_{};
    std::array<char, 128> category_{};
    std::array<char, 256> title_{};
    std::vector<std::size_t> filtered_;
    std::uint64_t generation_{UINT64_MAX};
    bool filterDirty_{true};

    bool matches(const ActivityRecord& record) const;
    void reload(Context& context);
    void drawToolbar(Context& context);
    void drawDeletePopups(Context& context);
    void drawTable(const ActivityDataCache& data, const std::optional<ActivityRecord>& live);
};
} // namespace pcat
