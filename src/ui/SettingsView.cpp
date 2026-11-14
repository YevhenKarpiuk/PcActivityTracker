#include "ui/SettingsView.h"

#include "data/AppLog.h"
#include "platform/Autostart.h"
#include "ui/UiCommon.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace pcat {
SettingsView::SettingsView(const AppSettings& settings)
    : draft_(settings),
      excludedProcesses_(joinLines(settings.excludedProcesses)),
      excludedTitles_(joinLines(settings.excludedWindowTitles)),
      excludedPatterns_(joinLines(settings.excludedWindowTitlePatterns)),
      excludedDomains_(joinLines(settings.excludedBrowserDomains)),
      categories_(joinCategories(settings.categories)),
      appliedAutostart_(settings.startWithSystem) {}

void SettingsView::draw(Context& context) {
    int idle = draft_.idleThresholdSeconds;
    int poll = draft_.pollingIntervalSeconds;
    if (ImGui::InputInt("Порог простоя, сек", &idle)) draft_.idleThresholdSeconds = std::max(10, idle);
    if (ImGui::InputInt("Интервал опроса, сек", &poll)) draft_.pollingIntervalSeconds = std::clamp(poll, 1, 60);
    ImGui::Checkbox("Скрывать заголовки браузера", &draft_.hideBrowserWindowTitles);
    ImGui::Checkbox("Запускать вместе с системой", &draft_.startWithSystem);

    ImGui::SeparatorText("Исключения и категории");
    ImGui::TextUnformatted("По одному значению на строку. Шаблоны окон: wildcard или regex:...");
    ImGui::InputTextMultiline("Исключённые программы", &excludedProcesses_, ImVec2(300, 90));
    ImGui::SameLine();
    ImGui::InputTextMultiline("Точные заголовки окон", &excludedTitles_, ImVec2(300, 90));
    ImGui::SameLine();
    ImGui::InputTextMultiline("Шаблоны заголовков", &excludedPatterns_, ImVec2(300, 90));
    ImGui::InputTextMultiline("Исключённые домены", &excludedDomains_, ImVec2(300, 90));
    ImGui::SameLine();
    ImGui::InputTextMultiline("Категории: приложение=категория", &categories_, ImVec2(420, 120));

    if (ImGui::Button("Сохранить настройки")) save(context);
}

void SettingsView::save(Context& context) {
    const bool previousAutostart = appliedAutostart_;
    bool autostartTouched = false;
    try {
        auto candidate = draft_;
        candidate.excludedProcesses = splitLines(excludedProcesses_);
        candidate.excludedWindowTitles = splitLines(excludedTitles_);
        candidate.excludedWindowTitlePatterns = splitLines(excludedPatterns_);
        candidate.excludedBrowserDomains = splitLines(excludedDomains_);
        candidate.categories = parseCategories(categories_);

        const auto autostart = setAutostart(candidate.startWithSystem, context.executablePath, AutostartReason::UserChoice);
        if (!autostart.ok) throw std::runtime_error("автозапуск: " + autostart.error);
        autostartTouched = true;
        context.service.save(candidate);
        appliedAutostart_ = candidate.startWithSystem;
        context.settings = candidate;
        draft_ = candidate;
        context.monitor.setSettings(context.settings);
        context.status = autostart.notice.empty() ? std::string("Настройки сохранены") : "Настройки сохранены. Автозапуск: " + autostart.notice;
        applog::info(std::string("Настройки сохранены, автозапуск ") + (candidate.startWithSystem ? "включён" : "выключен") +
                     (autostart.notice.empty() ? "" : "; " + autostart.notice));
    } catch (const std::exception& e) {
        AutostartResult rollback;
        if (autostartTouched && draft_.startWithSystem != previousAutostart)
            rollback = setAutostart(previousAutostart, context.executablePath, AutostartReason::UserChoice);
        context.status = std::string("Ошибка настроек: ") + e.what();
        if (!rollback.ok) context.status += "; не удалось восстановить автозапуск: " + rollback.error;
        appliedAutostart_ = previousAutostart;
        applog::error(context.status);
    }
}
} // namespace pcat
