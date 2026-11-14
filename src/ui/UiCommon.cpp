#include "ui/UiCommon.h"

#include "core/TextUtil.h"
#include "core/TimeUtil.h"

#include <sstream>

namespace pcat {
std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::string joinLines(const std::vector<std::string>& values) {
    std::string out;
    for (const auto& value : values) {
        out += value;
        out += '\n';
    }
    return out;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        line = textutil::trim(line);
        if (!line.empty()) out.push_back(std::move(line));
    }
    return out;
}

std::string joinCategories(const CategoryMap& values) {
    std::string out;
    for (const auto& [key, value] : values) out += key + "=" + value + "\n";
    return out;
}

CategoryMap parseCategories(const std::string& text) {
    CategoryMap result;
    std::istringstream stream(text);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        line = textutil::trim(line);
        if (line.empty()) continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos)
            throw std::runtime_error("Категории: строка " + std::to_string(lineNumber) + " должна иметь формат приложение=категория");
        auto key = textutil::trim(line.substr(0, pos));
        auto value = textutil::trim(line.substr(pos + 1));
        if (key.empty() || value.empty())
            throw std::runtime_error("Категории: пустое приложение или категория в строке " + std::to_string(lineNumber));
        result[key] = value;
    }
    return result;
}

std::chrono::system_clock::time_point localMidnight(std::chrono::system_clock::time_point now) {
    auto tm = timeutil::localTm(now);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    return timeutil::fromLocalTm(tm);
}
} // namespace pcat
