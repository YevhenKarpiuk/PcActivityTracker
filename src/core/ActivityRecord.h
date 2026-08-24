#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace pcat {
struct ActivityRecord {
    std::int64_t id{};
    std::chrono::system_clock::time_point startTime{};
    std::chrono::system_clock::time_point endTime{};
    std::int64_t durationSeconds{};
    std::string processName;
    std::string windowTitle;
    std::string exePath;
    std::string browserUrl;
    std::string browserDomain;
    bool isIdle{};
    std::string category{"Без категории"};
};
}
