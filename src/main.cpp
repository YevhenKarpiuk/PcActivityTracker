#include "ui/App.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace {
std::filesystem::path utf8Path(const char* value) {
    if (!value || !*value) return {};
    const auto* first = reinterpret_cast<const char8_t*>(value);
    return std::filesystem::path(std::u8string(first));
}

std::filesystem::path executablePath(int argc, char** argv) {
    std::filesystem::path argument;
    if (argc > 0 && argv) argument = utf8Path(argv[0]);

    std::error_code ec;
    if (!argument.empty() && argument.is_absolute()) {
        auto canonical = std::filesystem::weakly_canonical(argument, ec);
        return ec ? argument : canonical;
    }
    if (!argument.empty() && argument.has_parent_path()) {
        auto absolute = std::filesystem::absolute(argument, ec);
        if (!ec) return absolute;
    }

    // argv[0] can contain only a file name when the program was found through PATH. SDL knows the
    // directory that contains the running application on all supported desktop platforms.
    if (const char* base = SDL_GetBasePath(); base && *base) {
        auto result = utf8Path(base);
        if (!argument.empty()) result /= argument.filename();
#ifdef _WIN32
        else result /= "PcActivityTracker.exe";
#else
        else result /= "PcActivityTracker";
#endif
        return result;
    }

    auto fallback = std::filesystem::current_path();
    if (!argument.empty()) fallback /= argument.filename();
#ifdef _WIN32
    else fallback /= "PcActivityTracker.exe";
#else
    else fallback /= "PcActivityTracker";
#endif
    return fallback;
}

void showFatalError(const char* message) noexcept {
    if (!message) message = "Unknown fatal error";
    std::fprintf(stderr, "PcActivityTracker: %s\n", message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PcActivityTracker", message, nullptr);
}
} // namespace

int main(int argc, char** argv) {
    try {
        return pcat::runApp(executablePath(argc, argv));
    } catch (const std::exception& error) {
        showFatalError(error.what());
        return 1;
    } catch (...) {
        showFatalError("Unknown fatal error");
        return 1;
    }
}
