#include "data/AppLog.h"

#include "core/TimeUtil.h"

#include <chrono>
#include <fstream>
#include <mutex>

namespace pcat::applog {
namespace {
std::mutex& logMutex() { static std::mutex mutex; return mutex; }
std::ofstream& stream() { static std::ofstream file; return file; }

const char* levelName(Level level) {
    switch (level) {
    case Level::Info: return "INFO ";
    case Level::Warning: return "WARN ";
    case Level::Error: return "ERROR";
    }
    return "?    ";
}
} // namespace

void open(const std::filesystem::path& file, std::uintmax_t maxBytes) {
    std::scoped_lock lock(logMutex());
    auto& out = stream();
    if (out.is_open()) out.close();
    std::error_code ec;
    if (std::filesystem::exists(file, ec) && std::filesystem::file_size(file, ec) > maxBytes && !ec) {
        auto rotated = file;
        rotated += ".1";
        std::filesystem::remove(rotated, ec);
        std::filesystem::rename(file, rotated, ec);
    }
    out.open(file, std::ios::binary | std::ios::app);
}

void close() noexcept {
    std::scoped_lock lock(logMutex());
    stream().close();
}

bool isOpen() noexcept {
    std::scoped_lock lock(logMutex());
    return stream().is_open();
}

void write(Level level, std::string_view message) noexcept {
    try {
        std::scoped_lock lock(logMutex());
        auto& out = stream();
        if (!out.is_open()) return;
        out << timeutil::formatLocalDateTime(std::chrono::system_clock::now()) << ' ' << levelName(level) << ' ' << message << '\n';
        out.flush();
    } catch (...) {
    }
}
} // namespace pcat::applog
