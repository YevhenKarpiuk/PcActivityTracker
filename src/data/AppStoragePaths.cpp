#include "data/AppStoragePaths.h"

#include <cstdlib>

namespace pcat {
namespace {
bool hasPortableData(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "activity_tracker.db") || std::filesystem::exists(dir / "settings.json");
}

#ifdef _WIN32
std::filesystem::path environmentPath(const wchar_t* name) {
    if (const wchar_t* value = _wgetenv(name); value && *value) return std::filesystem::path(value);
    return {};
}
#else
std::filesystem::path environmentPath(const char* name) {
    if (const char* value = std::getenv(name); value && *value) return std::filesystem::path(value);
    return {};
}
#endif
} // namespace

std::filesystem::path appDataDirectory(const std::filesystem::path& executableDirectory) {
    const auto data = executableDirectory / "data";
    if (std::filesystem::exists(executableDirectory / "portable.txt") || hasPortableData(data)) return data;
#ifdef _WIN32
    if (auto value = environmentPath(L"LOCALAPPDATA"); !value.empty()) return value / "PcActivityTracker";
    if (auto value = environmentPath(L"USERPROFILE"); !value.empty()) return value / "AppData" / "Local" / "PcActivityTracker";
#elif __APPLE__
    if (auto value = environmentPath("HOME"); !value.empty()) return value / "Library" / "Application Support" / "PcActivityTracker";
#else
    if (auto value = environmentPath("XDG_DATA_HOME"); !value.empty()) return value / "PcActivityTracker";
    if (auto value = environmentPath("HOME"); !value.empty()) return value / ".local" / "share" / "PcActivityTracker";
#endif
    return data;
}
} // namespace pcat
