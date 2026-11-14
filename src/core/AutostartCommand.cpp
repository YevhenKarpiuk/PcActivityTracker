#include "core/AutostartCommand.h"

#include <cctype>

namespace pcat {
std::string autostartCommandLine(std::string_view executableUtf8) {
    std::string result;
    result.reserve(executableUtf8.size() + kAutostartArgument.size() + 3);
    result += '"';
    result += executableUtf8;
    result += "\" ";
    result += kAutostartArgument;
    return result;
}

std::string executableFromCommandLine(std::string_view commandLine) {
    auto isSpace = [](char c) { return c == ' ' || c == '\t'; };
    std::size_t begin = 0;
    while (begin < commandLine.size() && isSpace(commandLine[begin])) ++begin;
    if (begin == commandLine.size()) return {};
    if (commandLine[begin] == '"') {
        const auto end = commandLine.find('"', begin + 1);
        if (end == std::string_view::npos || end == begin + 1) return {};
        return std::string(commandLine.substr(begin + 1, end - begin - 1));
    }
    // Без кавычек Windows сама ищет границу по ".exe": путь может содержать пробелы.
    std::string lower(commandLine.substr(begin));
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (const auto exe = lower.find(".exe"); exe != std::string::npos) {
        const auto end = exe + 4;
        if (end == lower.size() || isSpace(lower[end])) return std::string(commandLine.substr(begin, end));
    }
    auto end = begin;
    while (end < commandLine.size() && !isSpace(commandLine[end])) ++end;
    return std::string(commandLine.substr(begin, end - begin));
}

bool hasAutostartArgument(int argc, const char* const* argv) {
    if (!argv) return false;
    for (int i = 1; i < argc; ++i)
        if (argv[i] && kAutostartArgument == argv[i]) return true;
    return false;
}
}
