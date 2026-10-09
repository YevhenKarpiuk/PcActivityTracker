#pragma once
#include "core/ActivityRecord.h"
#include "reports/ReportTypes.h"
#include <vector>
namespace pcat {
ReportResult buildReport(const std::vector<ActivityRecord>& records,const ReportDefinition& definition);
struct TodaySummary {
    ReportValues values;
    std::string topProcess;
    long long topActiveSeconds{};
};
TodaySummary buildTodaySummary(const std::vector<ActivityRecord>& records,
                              std::chrono::system_clock::time_point start,
                              std::chrono::system_clock::time_point end);
}
