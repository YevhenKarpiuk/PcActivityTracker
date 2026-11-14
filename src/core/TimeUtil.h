#pragma once
#include <chrono>
#include <string>

namespace pcat::timeutil {
using TimePoint = std::chrono::system_clock::time_point;
TimePoint parseIso8601(const std::string& text);
std::string toIso8601Utc(TimePoint tp);
std::string formatDuration(long long seconds);
std::tm localTm(TimePoint tp);
// Местное время для показа человеку: "2026-08-24 10:00:05". Хранение и экспорт остаются в UTC ISO-8601.
std::string formatLocalDateTime(TimePoint tp);
TimePoint fromLocalTm(std::tm tm);
}
