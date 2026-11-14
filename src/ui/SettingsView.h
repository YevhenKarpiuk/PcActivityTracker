#pragma once
#include "core/ActivityMonitor.h"
#include "core/AppSettings.h"
#include "data/SettingsService.h"

#include <filesystem>
#include <string>

namespace pcat {
// Вкладка «Настройки»: черновик редактируется отдельно и применяется только кнопкой «Сохранить».
class SettingsView {
public:
    struct Context {
        AppSettings& settings; // применённые настройки
        SettingsService& service;
        ActivityMonitor& monitor;
        const std::filesystem::path& executablePath;
        std::string& status;
    };
    explicit SettingsView(const AppSettings& settings);
    void draw(Context& context);

private:
    AppSettings draft_;
    std::string excludedProcesses_;
    std::string excludedTitles_;
    std::string excludedPatterns_;
    std::string excludedDomains_;
    std::string categories_;
    bool appliedAutostart_{false};

    void save(Context& context);
};
} // namespace pcat
