#include "data/AppStoragePaths.h"

#include <cstdlib>
#include <fstream>

namespace pcat {
namespace {
bool hasPortableData(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "activity_tracker.db") || std::filesystem::exists(dir / "settings.json");
}

// Проверяет, что каталог реально доступен на запись: наличия прав в ACL недостаточно, мешать могут
// read-only носитель, сетевой ресурс или Windows-виртуализация каталога программы.
bool directoryIsWritable(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (!std::filesystem::is_directory(dir, ec)) return false;

    const auto probe = dir / ".pcat-write-test";
    {
        std::ofstream stream(probe, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << 'x';
        stream.flush();
        if (!stream) return false;
    }
    std::filesystem::remove(probe, ec);
    return true;
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

// Каталог в профиле пользователя. Используется только как запасной вариант, когда рядом
// с программой писать нельзя.
std::filesystem::path perUserDirectory() {
#ifdef _WIN32
    if (auto value = environmentPath(L"LOCALAPPDATA"); !value.empty()) return value / "PcActivityTracker";
    if (auto value = environmentPath(L"USERPROFILE"); !value.empty()) return value / "AppData" / "Local" / "PcActivityTracker";
#elif __APPLE__
    if (auto value = environmentPath("HOME"); !value.empty()) return value / "Library" / "Application Support" / "PcActivityTracker";
#else
    if (auto value = environmentPath("XDG_DATA_HOME"); !value.empty()) return value / "PcActivityTracker";
    if (auto value = environmentPath("HOME"); !value.empty()) return value / ".local" / "share" / "PcActivityTracker";
#endif
    return {};
}
} // namespace

std::filesystem::path appDataDirectory(const std::filesystem::path& executableDirectory) {
    const auto data = executableDirectory / "data";

    // 1. Явный portable.txt и уже существующие данные рядом с программой имеют безусловный приоритет:
    //    пользователь не должен внезапно потерять историю из-за смены логики выбора каталога.
    if (std::filesystem::exists(executableDirectory / "portable.txt") || hasPortableData(data)) return data;

    // 2. По умолчанию боевая база и настройки лежат в каталоге самой программы.
    if (directoryIsWritable(data)) return data;

    // 3. Профиль пользователя — только если писать рядом с программой физически нельзя
    //    (установка в Program Files, read-only носитель, сетевой ресурс). Иначе приложение
    //    не смогло бы работать вообще.
    if (auto perUser = perUserDirectory(); !perUser.empty()) return perUser;

    return data;
}
} // namespace pcat
