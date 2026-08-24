#include "reports/ReportExporter.h"

#include <fstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <stdexcept>

namespace pcat {
namespace {
std::string csvEscape(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

std::string jsonEscape(const std::string& text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const char raw : text) {
        const auto c = static_cast<unsigned char>(raw);
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20U) {
                    out += "\\u00";
                    out += hex[c >> 4U];
                    out += hex[c & 15U];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string displayKey(const ReportRow& row, std::size_t column, std::size_t groupCount) {
    if (row.kind == RowKind::GrandTotal) return column == 0 ? "ИТОГО" : "";
    if (column < row.keys.size()) return row.keys[column];
    if (row.kind == RowKind::Subtotal && column == row.keys.size() && column < groupCount) return "Итого";
    return {};
}

const char* rowKindName(RowKind kind) {
    switch (kind) {
        case RowKind::Detail: return "detail";
        case RowKind::Subtotal: return "subtotal";
        case RowKind::GrandTotal: return "grand_total";
    }
    return "unknown";
}

void writeMetric(std::ostream& stream, const ReportValues& values, Metric metric) {
    switch (metric) {
        case Metric::TotalSeconds: stream << values.totalSeconds; break;
        case Metric::ActiveSeconds: stream << values.activeSeconds; break;
        case Metric::IdleSeconds: stream << values.idleSeconds; break;
        case Metric::RecordCount: stream << values.recordCount; break;
        case Metric::AverageSeconds: stream << std::fixed << std::setprecision(2) << values.averageSeconds; break;
        case Metric::MinSeconds: stream << values.minSeconds; break;
        case Metric::MaxSeconds: stream << values.maxSeconds; break;
        case Metric::SharePercent: stream << std::fixed << std::setprecision(2) << values.sharePercent; break;
    }
}

void writeJsonMetric(std::ostream& stream, const ReportValues& values, Metric metric) {
    switch (metric) {
        case Metric::TotalSeconds: stream << values.totalSeconds; break;
        case Metric::ActiveSeconds: stream << values.activeSeconds; break;
        case Metric::IdleSeconds: stream << values.idleSeconds; break;
        case Metric::RecordCount: stream << values.recordCount; break;
        case Metric::AverageSeconds: stream << values.averageSeconds; break;
        case Metric::MinSeconds: stream << values.minSeconds; break;
        case Metric::MaxSeconds: stream << values.maxSeconds; break;
        case Metric::SharePercent: stream << values.sharePercent; break;
    }
}
} // namespace

void exportReportCsv(const ReportResult& report, const std::filesystem::path& path) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot create report CSV");
    stream.imbue(std::locale::classic());
    stream << "\xEF\xBB\xBF";

    for (const auto group : report.definition.groups) {
        stream << csvEscape(groupFieldName(group)) << ';';
    }
    for (const auto metric : report.definition.metrics) {
        stream << csvEscape(metricName(metric)) << ';';
    }
    stream << "Тип строки\n";

    for (const auto& row : report.rows) {
        for (std::size_t index = 0; index < report.definition.groups.size(); ++index) {
            stream << csvEscape(displayKey(row, index, report.definition.groups.size())) << ';';
        }
        for (const auto metric : report.definition.metrics) {
            writeMetric(stream, row.values, metric);
            stream << ';';
        }
        stream << rowKindName(row.kind) << '\n';
    }
    if (!stream) throw std::runtime_error("Failed to write report CSV");
}

void exportReportJson(const ReportResult& report, const std::filesystem::path& path) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot create report JSON");
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10);

    stream << "{\n  \"groups\": [";
    for (std::size_t index = 0; index < report.definition.groups.size(); ++index) {
        if (index != 0) stream << ", ";
        stream << '"' << jsonEscape(groupFieldName(report.definition.groups[index])) << '"';
    }
    stream << "],\n  \"metrics\": [";
    for (std::size_t index = 0; index < report.definition.metrics.size(); ++index) {
        if (index != 0) stream << ", ";
        stream << '"' << jsonEscape(metricName(report.definition.metrics[index])) << '"';
    }
    stream << "],\n  \"rows\": [\n";

    for (std::size_t rowIndex = 0; rowIndex < report.rows.size(); ++rowIndex) {
        const auto& row = report.rows[rowIndex];
        stream << "    {\"kind\": \"" << rowKindName(row.kind) << "\", \"keys\": [";
        for (std::size_t index = 0; index < report.definition.groups.size(); ++index) {
            if (index != 0) stream << ',';
            stream << '"' << jsonEscape(displayKey(row, index, report.definition.groups.size())) << '"';
        }
        stream << "], \"values\": {";
        for (std::size_t index = 0; index < report.definition.metrics.size(); ++index) {
            if (index != 0) stream << ", ";
            const auto metric = report.definition.metrics[index];
            stream << '"' << jsonEscape(metricName(metric)) << "\": ";
            writeJsonMetric(stream, row.values, metric);
        }
        stream << "}}" << (rowIndex + 1 < report.rows.size() ? "," : "") << '\n';
    }
    stream << "  ]\n}\n";
    if (!stream) throw std::runtime_error("Failed to write report JSON");
}

} // namespace pcat
