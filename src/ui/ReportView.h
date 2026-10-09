#pragma once
#include "core/ActivityRecord.h"
#include "core/ActivityMonitor.h"
#include "reports/ReportTypes.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pcat {
class ReportView {
public:
    // Каталог для CSV/JSON выгрузок. Без него экспорт ушёл бы в текущий каталог процесса,
    // который у GUI-приложения не совпадает с каталогом программы.
    void setExportDirectory(std::filesystem::path directory){exportDirectory_=std::move(directory);}
    void draw(ActivityMonitor& monitor);
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
    std::filesystem::path exportDirectory_;
    std::string status_;
    void initialize();
    void rebuild(ActivityMonitor& monitor);
};
}
