#pragma once
#include "core/IActivityRepository.h"
#include <filesystem>
#include <mutex>

struct sqlite3;
namespace pcat {
class ActivityRepository final : public IActivityRepository {
public:
    explicit ActivityRepository(std::filesystem::path dbPath);
    ~ActivityRepository();
    ActivityRepository(const ActivityRepository&)=delete;
    ActivityRepository& operator=(const ActivityRepository&)=delete;
    void initialize();
    void insert(const ActivityRecord& record) override;
    void commitClosedRecord(const ActivityRecord& record) override;
    std::vector<ActivityRecord> getRecords(std::chrono::system_clock::time_point start,
                                           std::chrono::system_clock::time_point end) override;
    int deleteByFilter(std::chrono::system_clock::time_point start,
                       std::chrono::system_clock::time_point end,
                       const std::string& process,
                       const std::string& category,
                       const std::string& title) override;
    void deleteAll() override;
    // Записи, для которых не удалось восстановить время ни строгим парсером, ни SQLite.
    // Они не попадают в запросы по периоду, поэтому о них нужно сообщать явно, а не терять молча.
    long long undatedRecordCount();
    void saveOpenCheckpoint(const ActivityRecord& record) override;
    void clearOpenCheckpoint() override;
    const std::filesystem::path& path() const { return dbPath_; }
private:
    void ensureOpen() const;
    void exec(const char* sql);
    sqlite3* db_{};
    std::filesystem::path dbPath_;
    std::mutex mutex_;
};
}
