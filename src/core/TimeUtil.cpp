#include "core/TimeUtil.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string_view>

namespace pcat::timeutil {
namespace {
[[noreturn]] void invalidTimestamp(const std::string& value) {
    throw std::runtime_error("Invalid ISO-8601 timestamp: " + value);
}

int parseDigits(std::string_view text, std::size_t position, std::size_t count, const std::string& original) {
    if (position + count > text.size()) invalidTimestamp(original);
    int value = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const auto c = static_cast<unsigned char>(text[position + index]);
        if (!std::isdigit(c)) invalidTimestamp(original);
        value = value * 10 + static_cast<int>(c - static_cast<unsigned char>('0'));
    }
    return value;
}
} // namespace

TimePoint parseIso8601(const std::string& value) {
    if (value.size() < 19
        || value[4] != '-'
        || value[7] != '-'
        || (value[10] != 'T' && value[10] != 't' && value[10] != ' ')
        || value[13] != ':'
        || value[16] != ':') {
        invalidTimestamp(value);
    }

    const int year = parseDigits(value, 0, 4, value);
    const int month = parseDigits(value, 5, 2, value);
    const int day = parseDigits(value, 8, 2, value);
    const int hour = parseDigits(value, 11, 2, value);
    const int minute = parseDigits(value, 14, 2, value);
    const int second = parseDigits(value, 17, 2, value);

    const std::chrono::year_month_day date{
        std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok() || hour > 23 || minute > 59 || second > 60) invalidTimestamp(value);

    std::size_t position = 19;
    long long nanoseconds = 0;
    if (position < value.size() && value[position] == '.') {
        ++position;
        int digits = 0;
        while (position < value.size() && std::isdigit(static_cast<unsigned char>(value[position]))) {
            if (digits < 9) {
                nanoseconds = nanoseconds * 10 + (value[position] - '0');
                ++digits;
            }
            ++position;
        }
        if (digits == 0) invalidTimestamp(value);
        while (digits < 9) {
            nanoseconds *= 10;
            ++digits;
        }
    }

    int offsetSeconds = 0;
    if (position < value.size()) {
        if ((value[position] == 'Z' || value[position] == 'z') && position + 1 == value.size()) {
            ++position;
        } else if (value[position] == '+' || value[position] == '-') {
            const int sign = value[position] == '+' ? 1 : -1;
            if (position + 6 != value.size() || value[position + 3] != ':') invalidTimestamp(value);
            const int offsetHours = parseDigits(value, position + 1, 2, value);
            const int offsetMinutes = parseDigits(value, position + 4, 2, value);
            if (offsetHours > 14 || offsetMinutes > 59 || (offsetHours == 14 && offsetMinutes != 0)) invalidTimestamp(value);
            offsetSeconds = sign * (offsetHours * 3600 + offsetMinutes * 60);
            position += 6;
        } else {
            invalidTimestamp(value);
        }
    }
    if (position != value.size()) invalidTimestamp(value);

    // Build UTC directly with C++20 calendar types instead of timegm/_mkgmtime. Apart from being
    // timezone-independent, this also avoids the narrower pre-1970 range of some Windows CRT paths.
    auto utc = std::chrono::sys_days{date}
        + std::chrono::hours(hour)
        + std::chrono::minutes(minute)
        + std::chrono::seconds(std::min(second, 59))
        + std::chrono::nanoseconds(nanoseconds)
        - std::chrono::seconds(offsetSeconds);
    if (second == 60) utc += std::chrono::seconds(1);
    return TimePoint{std::chrono::duration_cast<TimePoint::duration>(utc.time_since_epoch())};
}

std::string toIso8601Utc(TimePoint point) {
    // floor(), unlike time_point_cast(), keeps the fractional part non-negative for timestamps
    // before the Unix epoch as well. This makes the serializer a true round-trip for all supported times.
    const auto wholeSeconds = std::chrono::floor<std::chrono::seconds>(point);
    const auto dayPoint = std::chrono::floor<std::chrono::days>(wholeSeconds);
    const std::chrono::year_month_day date{dayPoint};
    const std::chrono::hh_mm_ss timeOfDay{wholeSeconds - dayPoint};
    const auto fraction = std::chrono::duration_cast<std::chrono::nanoseconds>(point - wholeSeconds).count();
    const auto ticks100ns = fraction / 100;

    char buffer[48]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02lld:%02lld:%02lld.%07lldZ",
                  static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                  static_cast<long long>(timeOfDay.hours().count()),
                  static_cast<long long>(timeOfDay.minutes().count()),
                  static_cast<long long>(timeOfDay.seconds().count()), static_cast<long long>(ticks100ns));
    return buffer;
}

std::string formatDuration(long long seconds) {
    if (seconds < 0) seconds = 0;
    const auto days = seconds / 86400;
    seconds %= 86400;
    const auto hours = seconds / 3600;
    seconds %= 3600;
    const auto minutes = seconds / 60;
    const auto remainingSeconds = seconds % 60;
    char buffer[64]{};
    if (days > 0) {
        std::snprintf(buffer, sizeof(buffer), "%lld д %02lld:%02lld:%02lld", days, hours, minutes, remainingSeconds);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", hours, minutes, remainingSeconds);
    }
    return buffer;
}

std::tm localTm(TimePoint point) {
    const auto raw = std::chrono::system_clock::to_time_t(point);
    std::tm time{};
#ifdef _WIN32
    if (localtime_s(&time, &raw) != 0) throw std::runtime_error("Cannot convert timestamp to local time");
#else
    if (!localtime_r(&raw, &time)) throw std::runtime_error("Cannot convert timestamp to local time");
#endif
    return time;
}

TimePoint fromLocalTm(std::tm time) {
    time.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(std::mktime(&time));
}

} // namespace pcat::timeutil
