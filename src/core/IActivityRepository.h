#pragma once
#include "core/ActivityRecord.h"
#include <chrono>
#include <vector>

namespace pcat {
class IActivityRepository {
public:
    virtual ~IActivityRepository() = default;
    virtual void insert(const ActivityRecord& record) = 0;
    // Atomically persist a closed interval and retire any crash-recovery checkpoint that
    // belongs to the interval being closed. Persistent repositories should override this.
    virtual void commitClosedRecord(const ActivityRecord& record) {
        insert(record);
        clearOpenCheckpoint();
    }
    virtual std::vector<ActivityRecord> getRecords(std::chrono::system_clock::time_point start,
                                                   std::chrono::system_clock::time_point end) = 0;
    virtual int deleteByFilter(std::chrono::system_clock::time_point start,
                               std::chrono::system_clock::time_point end,
                               const std::string& process,
                               const std::string& category,
                               const std::string& title) = 0;
    virtual void deleteAll() = 0;

    // A lightweight crash-recovery checkpoint for the currently open interval.
    // Repositories that do not persist data may keep the default no-op implementation.
    virtual void saveOpenCheckpoint(const ActivityRecord&) {}
    virtual void clearOpenCheckpoint() {}
};
}
