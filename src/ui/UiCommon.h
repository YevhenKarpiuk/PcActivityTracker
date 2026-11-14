#pragma once
// Мелкие помощники UI-слоя, общие для нескольких вкладок. Агрегаты здесь не считаются (см. reports).
#include "core/ActivityMonitor.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pcat {
using CategoryMap = std::map<std::string, std::string, std::less<>>;

std::string pathUtf8(const std::filesystem::path& path);
std::string joinLines(const std::vector<std::string>& values);
std::vector<std::string> splitLines(const std::string& text);
std::string joinCategories(const CategoryMap& values);
// Бросает std::runtime_error с номером строки при неверном формате «приложение=категория».
CategoryMap parseCategories(const std::string& text);
std::chrono::system_clock::time_point localMidnight(std::chrono::system_clock::time_point now);

// Удаление истории только после сброса текущего интервала в базу; мониторинг возвращается
// в прежнее состояние и при ошибке операции.
template<class Fn>
void mutateHistorySafely(ActivityMonitor& monitor, Fn&& operation) {
    const bool wasPaused = monitor.isPaused();
    if (!monitor.pauseAndFlushForMaintenance())
        throw std::runtime_error("Не удалось сохранить текущую активность. Мониторинг оставлен на паузе; удаление отменено.");
    try {
        operation();
    } catch (...) {
        if (!wasPaused) monitor.setPaused(false);
        throw;
    }
    if (!wasPaused) monitor.setPaused(false);
}
} // namespace pcat
