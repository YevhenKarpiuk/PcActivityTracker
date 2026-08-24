#pragma once
#include "core/ActivityRecord.h"
#include "core/IActivityRepository.h"
#include "reports/ReportTypes.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace pcat {
class ReportView {
public:
    void draw(IActivityRepository& repository,const std::optional<ActivityRecord>& liveRecord);
private:
    enum class ChartType { Bars, Line, Pie };
    ReportDefinition def_{};
    ReportResult result_{};
    std::vector<ActivityRecord> reportRecords_;
    bool initialized_{};
    GroupField groupOrder_[8]{GroupField::Day,GroupField::Process,GroupField::Category,GroupField::BrowserDomain,GroupField::WindowTitle,GroupField::Status,GroupField::Weekday,GroupField::Hour};
    Metric metricOrder_[8]{Metric::TotalSeconds,Metric::ActiveSeconds,Metric::IdleSeconds,Metric::RecordCount,Metric::AverageSeconds,Metric::MinSeconds,Metric::MaxSeconds,Metric::SharePercent};
    std::array<char,16> startDate_{};
    std::array<char,16> endDate_{};
    std::array<char,128> processFilter_{};
    std::array<char,128> categoryFilter_{};
    std::array<char,128> domainFilter_{};
    std::array<char,256> titleFilter_{};
    Metric chartMetric_{Metric::TotalSeconds};
    ChartType chartType_{ChartType::Bars};
    std::string status_;
    void initialize();
    void rebuild(IActivityRepository& repository,const std::optional<ActivityRecord>& liveRecord);
};
}
