#pragma once
#include <chrono>
#include <string>
#include <vector>

namespace pcat {
enum class GroupField { Process, Category, BrowserDomain, WindowTitle, Status, Day, Weekday, Hour };
enum class Metric { TotalSeconds, ActiveSeconds, IdleSeconds, RecordCount, AverageSeconds, MinSeconds, MaxSeconds, SharePercent };
enum class RowKind { Detail, Subtotal, GrandTotal };

struct ReportFilter {
    std::string processContains;
    std::string categoryContains;
    std::string domainContains;
    std::string titleContains;
    bool includeIdle{true};
};
struct ReportDefinition {
    std::chrono::system_clock::time_point start{};
    std::chrono::system_clock::time_point end{};
    std::vector<GroupField> groups{GroupField::Process};
    std::vector<Metric> metrics{Metric::TotalSeconds, Metric::SharePercent};
    ReportFilter filter;
    bool showSubtotals{true};
    bool showGrandTotal{true};
    int topN{0};
    Metric sortMetric{Metric::TotalSeconds};
    bool sortDescending{true};
};
struct ReportValues {
    long long totalSeconds{};
    long long activeSeconds{};
    long long idleSeconds{};
    long long recordCount{};
    double averageSeconds{};
    long long minSeconds{};
    long long maxSeconds{};
    double sharePercent{};
};
struct ReportRow {
    RowKind kind{RowKind::Detail};
    int level{};
    std::vector<std::string> keys;
    ReportValues values;
};
struct ReportResult {
    ReportDefinition definition;
    std::vector<ReportRow> rows;
    ReportValues grandTotal;
};
std::string groupFieldName(GroupField f);
std::string metricName(Metric m);
}
