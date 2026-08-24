#pragma once
#include <chrono>
#include <string>

namespace pcat {
struct ActivitySnapshot {
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    std::string processName;
    std::string windowTitle;
    std::string exePath;
    std::string browserUrl;
    std::string browserDomain;
    int idleSeconds{};
    bool isIdle{};
};
}
