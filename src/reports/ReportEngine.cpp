#include "reports/ReportEngine.h"
#include "core/TextUtil.h"
#include "core/TimeUtil.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace pcat {

std::string groupFieldName(GroupField field) {
    switch (field) {
        case GroupField::Process: return "Программа";
        case GroupField::Category: return "Категория";
        case GroupField::BrowserDomain: return "Домен";
        case GroupField::WindowTitle: return "Окно";
        case GroupField::Status: return "Статус";
        case GroupField::Day: return "День";
        case GroupField::Weekday: return "День недели";
        case GroupField::Hour: return "Час";
    }
    return {};
}

std::string metricName(Metric metric) {
    switch (metric) {
        case Metric::TotalSeconds: return "Общее время";
        case Metric::ActiveSeconds: return "Активное время";
        case Metric::IdleSeconds: return "Простой";
        case Metric::RecordCount: return "Периоды";
        case Metric::AverageSeconds: return "Среднее";
        case Metric::MinSeconds: return "Минимум";
        case Metric::MaxSeconds: return "Максимум";
        case Metric::SharePercent: return "Доля, %";
    }
    return {};
}

namespace {
using Key = std::vector<std::string>;

struct Agg {
    long long total{};
    long long active{};
    long long idle{};

    // Contribution of every original activity record to this aggregate. Keeping it separately
    // from split pieces preserves period semantics for Average/Min/Max when a record crosses
    // a day/hour boundary.
    std::map<long long, long long> recordSeconds;
};

void add(Agg& aggregate, long long seconds, bool idle, long long recordId) {
    if (seconds <= 0) {
        return;
    }
    aggregate.total += seconds;
    if (idle) {
        aggregate.idle += seconds;
    } else {
        aggregate.active += seconds;
    }
    aggregate.recordSeconds[recordId] += seconds;
}

void merge(Agg& destination, const Agg& source) {
    destination.total += source.total;
    destination.active += source.active;
    destination.idle += source.idle;
    for (const auto& [recordId, seconds] : source.recordSeconds) {
        destination.recordSeconds[recordId] += seconds;
    }
}

ReportValues makeValues(const Agg& aggregate, long long grandTotalSeconds) {
    ReportValues values;
    values.totalSeconds = aggregate.total;
    values.activeSeconds = aggregate.active;
    values.idleSeconds = aggregate.idle;
    values.recordCount = static_cast<long long>(aggregate.recordSeconds.size());

    if (!aggregate.recordSeconds.empty()) {
        long long minimum = std::numeric_limits<long long>::max();
        long long maximum = 0;
        long long sum = 0;
        for (const auto& [recordId, seconds] : aggregate.recordSeconds) {
            (void)recordId;
            sum += seconds;
            minimum = std::min(minimum, seconds);
            maximum = std::max(maximum, seconds);
        }
        values.averageSeconds = static_cast<double>(sum) / static_cast<double>(aggregate.recordSeconds.size());
        values.minSeconds = minimum;
        values.maxSeconds = maximum;
    }

    values.sharePercent = grandTotalSeconds > 0
        ? static_cast<double>(aggregate.total) * 100.0 / static_cast<double>(grandTotalSeconds)
        : 0.0;
    return values;
}

double metricValue(const Agg& aggregate, Metric metric) {
    switch (metric) {
        case Metric::TotalSeconds: return static_cast<double>(aggregate.total);
        case Metric::ActiveSeconds: return static_cast<double>(aggregate.active);
        case Metric::IdleSeconds: return static_cast<double>(aggregate.idle);
        case Metric::RecordCount: return static_cast<double>(aggregate.recordSeconds.size());
        case Metric::AverageSeconds: {
            if (aggregate.recordSeconds.empty()) return 0.0;
            long long sum = 0;
            for (const auto& [recordId, seconds] : aggregate.recordSeconds) {
                (void)recordId;
                sum += seconds;
            }
            return static_cast<double>(sum) / static_cast<double>(aggregate.recordSeconds.size());
        }
        case Metric::MinSeconds: {
            if (aggregate.recordSeconds.empty()) return 0.0;
            long long minimum = std::numeric_limits<long long>::max();
            for (const auto& [recordId, seconds] : aggregate.recordSeconds) {
                (void)recordId;
                minimum = std::min(minimum, seconds);
            }
            return static_cast<double>(minimum);
        }
        case Metric::MaxSeconds: {
            long long maximum = 0;
            for (const auto& [recordId, seconds] : aggregate.recordSeconds) {
                (void)recordId;
                maximum = std::max(maximum, seconds);
            }
            return static_cast<double>(maximum);
        }
        case Metric::SharePercent:
            // The denominator is the same for every detail row, so total seconds gives identical ordering.
            return static_cast<double>(aggregate.total);
    }
    return 0.0;
}

std::string day(const std::tm& time) {
    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time);
    return buffer;
}

std::string hour(const std::tm& time) {
    char buffer[8]{};
    std::strftime(buffer, sizeof(buffer), "%H:00", &time);
    return buffer;
}

std::string weekday(const std::tm& time) {
    static constexpr const char* names[] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
    const auto index = std::clamp(time.tm_wday, 0, 6);
    return names[index];
}

bool needsLocalTime(GroupField field) {
    return field == GroupField::Day || field == GroupField::Weekday || field == GroupField::Hour;
}

// localTm() выполняет localtime_s с блокировкой и чтением timezone, поэтому вызывать его
// на каждое поле группировки нельзя: он нужен только временным группировкам и считается
// один раз на кусок интервала.
std::string keyFor(GroupField field, const ActivityRecord& record, const std::tm& local) {
    switch (field) {
        case GroupField::Process: return record.processName.empty() ? "—" : record.processName;
        case GroupField::Category: return record.category.empty() ? "Без категории" : record.category;
        case GroupField::BrowserDomain: return record.browserDomain.empty() ? "—" : record.browserDomain;
        case GroupField::WindowTitle: return record.windowTitle.empty() ? "—" : record.windowTitle;
        case GroupField::Status: return record.isIdle ? "Простой" : "Активность";
        case GroupField::Day: return day(local);
        case GroupField::Weekday: return weekday(local);
        case GroupField::Hour: return hour(local);
    }
    return "—";
}

enum class SplitMode { None, Day, Hour };

SplitMode splitMode(const ReportDefinition& definition) {
    if (std::find(definition.groups.begin(), definition.groups.end(), GroupField::Hour) != definition.groups.end()) {
        return SplitMode::Hour;
    }
    if (std::find(definition.groups.begin(), definition.groups.end(), GroupField::Day) != definition.groups.end()
        || std::find(definition.groups.begin(), definition.groups.end(), GroupField::Weekday) != definition.groups.end()) {
        return SplitMode::Day;
    }
    return SplitMode::None;
}

struct Piece {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
    long long seconds{};
};

std::vector<Piece> splitRecord(
    const ActivityRecord& record,
    std::chrono::system_clock::time_point begin,
    std::chrono::system_clock::time_point end,
    SplitMode mode) {

    auto start = std::max(record.startTime, begin);
    const auto finish = std::min(record.endTime, end);
    const auto clippedStart = start;
    std::vector<Piece> result;
    if (finish <= start) return result;

    if (mode == SplitMode::None) {
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(finish - start).count();
        if (seconds > 0) result.push_back({start, finish, seconds});
        return result;
    }

    // Convert cumulative duration to whole seconds and subtract what has already been emitted.
    // Flooring every piece independently can lose a second at each hour/day boundary when timestamps
    // contain fractions (e.g. 08:59:59.800 -> 09:00:00 -> 09:00:01.800).
    long long accountedSeconds = 0;
    while (start < finish) {
        auto local = timeutil::localTm(start);
        local.tm_sec = 0;
        if (mode == SplitMode::Hour) {
            local.tm_min = 0;
            ++local.tm_hour;
        } else {
            local.tm_min = 0;
            local.tm_hour = 0;
            ++local.tm_mday;
        }

        auto next = timeutil::fromLocalTm(local);
        // This fallback also guarantees progress around unusual DST transitions.
        if (next <= start) {
            next = start + (mode == SplitMode::Hour ? std::chrono::hours(1) : std::chrono::hours(24));
        }
        const auto pieceEnd = std::min(finish, next);
        const auto cumulativeSeconds = std::chrono::duration_cast<std::chrono::seconds>(pieceEnd - clippedStart).count();
        const auto seconds = cumulativeSeconds - accountedSeconds;
        accountedSeconds = cumulativeSeconds;
        if (seconds > 0) result.push_back({start, pieceEnd, seconds});
        start = pieceEnd;
    }
    return result;
}

std::map<Key, Agg> applyTopN(const std::map<Key, Agg>& source, int topN, Metric metric) {
    if (topN <= 0 || source.empty()) return source;
    const auto keySize = source.begin()->first.size();
    if (keySize == 0) return source;

    std::map<Key, std::vector<std::pair<Key, Agg>>> parents;
    for (const auto& item : source) {
        Key parent = item.first;
        parent.pop_back();
        parents[parent].push_back(item);
    }

    std::map<Key, Agg> result;
    for (auto& [parent, items] : parents) {
        std::stable_sort(items.begin(), items.end(), [metric](const auto& left, const auto& right) {
            const auto leftValue = metricValue(left.second, metric);
            const auto rightValue = metricValue(right.second, metric);
            if (leftValue != rightValue) return leftValue > rightValue;
            return left.first < right.first;
        });

        Agg other;
        int kept = 0;
        for (const auto& item : items) {
            if (kept < topN) {
                result.emplace(item);
                ++kept;
            } else {
                merge(other, item.second);
            }
        }
        if (other.total > 0) {
            // Реальная группа может называться «Остальные». Без отдельной метки технический
            // остаток перезаписал бы её агрегат и часть времени просто исчезла бы из отчёта.
            const std::string baseLabel = "Остальные (Top N)";
            std::string label = baseLabel;
            const auto clashes = [&items](const std::string& candidate) {
                return std::any_of(items.begin(), items.end(), [&candidate](const auto& item) {
                    return !item.first.empty() && item.first.back() == candidate;
                });
            };
            for(int suffix=2;clashes(label);++suffix)label=baseLabel+" #"+std::to_string(suffix);

            Key otherKey = parent;
            otherKey.push_back(std::move(label));
            merge(result[std::move(otherKey)], other);
        }
    }
    return result;
}

using Detail = std::pair<Key, Agg>;

// Агрегаты всех префиксов группировок, длины 1..size. Длина size — это сама детальная строка.
// Используются и для промежуточных итогов, и для сортировки верхних уровней.
std::map<Key, Agg> buildLevelAggregates(const std::map<Key, Agg>& details) {
    std::map<Key, Agg> levels;
    for (const auto& [key, aggregate] : details) {
        for (std::size_t count = 1; count <= key.size(); ++count) {
            Key prefix(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(count));
            merge(levels[prefix], aggregate);
        }
    }
    return levels;
}

std::vector<Detail> orderDetails(const std::map<Key, Agg>& details,
                                 const std::map<Key, Agg>& levels,
                                 Metric metric,
                                 bool descending) {
    struct Sortable {
        const Key* key{};
        const Agg* aggregate{};
        std::vector<double> levelValues;
    };

    std::vector<Sortable> sortable;
    sortable.reserve(details.size());
    for (const auto& [key, aggregate] : details) {
        Sortable item{&key, &aggregate, {}};
        item.levelValues.reserve(key.size());
        for (std::size_t count = 1; count <= key.size(); ++count) {
            Key prefix(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(count));
            const auto found = levels.find(prefix);
            item.levelValues.push_back(found != levels.end() ? metricValue(found->second, metric) : 0.0);
        }
        sortable.push_back(std::move(item));
    }

    std::stable_sort(sortable.begin(), sortable.end(), [descending](const Sortable& left, const Sortable& right) {
        const auto& leftKey = *left.key;
        const auto& rightKey = *right.key;
        const auto common = std::min(leftKey.size(), rightKey.size());
        for (std::size_t index = 0; index < common; ++index) {
            if (leftKey[index] == rightKey[index]) continue;
            // Порядок задаёт первый различающийся уровень, и сравниваются агрегаты префиксов.
            // Поэтому выбранный показатель управляет сортировкой на всех уровнях, а не только
            // на самом глубоком, а строки с общим родителем остаются соседними — от этого
            // зависит корректная расстановка промежуточных итогов.
            const auto leftValue = left.levelValues[index];
            const auto rightValue = right.levelValues[index];
            if (leftValue != rightValue) return descending ? leftValue > rightValue : leftValue < rightValue;
            return leftKey[index] < rightKey[index];
        }
        return leftKey.size() < rightKey.size();
    });

    std::vector<Detail> ordered;
    ordered.reserve(sortable.size());
    for (const auto& item : sortable) ordered.emplace_back(*item.key, *item.aggregate);
    return ordered;
}

bool recordMatches(const ActivityRecord& record, const ReportFilter& filter) {
    if (!filter.includeIdle && record.isIdle) return false;
    return textutil::containsCaseInsensitive(record.processName, filter.processContains)
        && textutil::containsCaseInsensitive(record.category, filter.categoryContains)
        && textutil::containsCaseInsensitive(record.browserDomain, filter.domainContains)
        && textutil::containsCaseInsensitive(record.windowTitle, filter.titleContains);
}
} // namespace

ReportResult buildReport(const std::vector<ActivityRecord>& records, const ReportDefinition& definition) {
    ReportResult result;
    result.definition = definition;

    std::map<Key, Agg> fullDetails;
    Agg grand;
    long long syntheticId = 1;
    const auto mode = splitMode(definition);

    const bool anyLocalTimeGroup = std::any_of(definition.groups.begin(), definition.groups.end(), needsLocalTime);

    for (const auto& record : records) {
        if (!recordMatches(record, definition.filter)) continue;
        const auto recordId = record.id != 0 ? record.id : -(syntheticId++);
        for (const auto& piece : splitRecord(record, definition.start, definition.end, mode)) {
            std::tm local{};
            if (anyLocalTimeGroup) local = timeutil::localTm(piece.start);
            Key key;
            key.reserve(definition.groups.size());
            for (const auto group : definition.groups) {
                key.push_back(keyFor(group, record, local));
            }
            add(fullDetails[key], piece.seconds, record.isIdle, recordId);
            add(grand, piece.seconds, record.isIdle, recordId);
        }
    }

    result.grandTotal = makeValues(grand, grand.total);

    // A report without groupings is a totals-only report, not a duplicate detail + total pair.
    if (definition.groups.empty()) {
        if (definition.showGrandTotal) {
            result.rows.push_back({RowKind::GrandTotal, 0, {"ИТОГО"}, result.grandTotal});
        }
        return result;
    }

    const auto details = applyTopN(fullDetails, definition.topN, definition.sortMetric);
    const auto prefixes = buildLevelAggregates(details);
    const auto ordered = orderDetails(details, prefixes, definition.sortMetric, definition.sortDescending);

    Key previous;
    const auto emitClosed = [&](const Key& previousKey, const Key& nextKey) {
        if (!definition.showSubtotals || previousKey.empty()) return;
        for (int level = static_cast<int>(previousKey.size()) - 1; level >= 1; --level) {
            const auto size = static_cast<std::size_t>(level);
            const bool closes = nextKey.size() < size
                || !std::equal(previousKey.begin(), previousKey.begin() + level, nextKey.begin());
            if (!closes) continue;
            Key prefix(previousKey.begin(), previousKey.begin() + level);
            const auto found = prefixes.find(prefix);
            if (found != prefixes.end()) {
                result.rows.push_back({RowKind::Subtotal, level, prefix, makeValues(found->second, grand.total)});
            }
        }
    };

    for (const auto& [key, aggregate] : ordered) {
        if (!previous.empty()) emitClosed(previous, key);
        result.rows.push_back({RowKind::Detail, static_cast<int>(key.size()), key, makeValues(aggregate, grand.total)});
        previous = key;
    }
    emitClosed(previous, {});

    if (definition.showGrandTotal) {
        result.rows.push_back({RowKind::GrandTotal, 0, {"ИТОГО"}, result.grandTotal});
    }
    return result;
}

TodaySummary buildTodaySummary(const std::vector<ActivityRecord>& records,
                              std::chrono::system_clock::time_point start,
                              std::chrono::system_clock::time_point end) {
    ReportDefinition definition;
    definition.start=start;
    definition.end=end;
    definition.groups={GroupField::Process};
    definition.sortMetric=Metric::ActiveSeconds;
    definition.sortDescending=true;
    definition.showGrandTotal=false;
    definition.showSubtotals=false;
    const auto report=buildReport(records,definition);
    TodaySummary summary;
    summary.values=report.grandTotal;
    if(!report.rows.empty()&&report.rows.front().values.activeSeconds>0) {
        summary.topProcess=report.rows.front().keys.front();
        summary.topActiveSeconds=report.rows.front().values.activeSeconds;
    }
    return summary;
}

} // namespace pcat
