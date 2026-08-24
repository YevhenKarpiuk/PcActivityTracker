#pragma once
#include <chrono>
#include <string>

namespace pcat::timeutil {
using TimePoint = std::chrono::system_clock::time_point;
TimePoint parseIso8601(const std::string& text);
std::string toIso8601Utc(TimePoint tp);
std::string formatDuration(long long seconds);
std::tm localTm(TimePoint tp);
TimePoint fromLocalTm(std::tm tm);
}
