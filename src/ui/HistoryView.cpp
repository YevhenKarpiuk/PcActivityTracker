#include "ui/HistoryView.h"

#include "core/TextUtil.h"
#include "core/TimeUtil.h"
#include "data/ActivityExporter.h"
#include "data/AppLog.h"
#include "ui/UiCommon.h"

#include <imgui.h>

namespace pcat {
bool HistoryView::matches(const ActivityRecord& r) const {
    return textutil::containsCaseInsensitive(r.processName, process_.data()) &&
           textutil::containsCaseInsensitive(r.category, category_.data()) &&
           textutil::containsCaseInsensitive(r.windowTitle, title_.data());
}

void HistoryView::reload(Context& context) {
    try {
        context.data.reload(context.monitor);
    } catch (const std::exception& e) {
        context.status = e.what();
        applog::error(std::string("Чтение истории: ") + e.what());
    }
}

void HistoryView::drawToolbar(Context& context) {
    int days = context.data.historyDays();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Дней", &days)) {
        context.data.setHistoryDays(days);
        reload(context);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170);
    filterDirty_ |= ImGui::InputText("Программа##history", process_.data(), process_.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170);
    filterDirty_ |= ImGui::InputText("Категория##history", category_.data(), category_.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240);
    filterDirty_ |= ImGui::InputText("Окно##history", title_.data(), title_.size());
    ImGui::SameLine();
    if (ImGui::Button("Применить период")) reload(context);
}

void HistoryView::drawDeletePopups(Context& context) {
    if (ImGui::BeginPopupModal("delete-filtered", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Удалить записи за выбранный период, соответствующие фильтрам?\nЭто действие нельзя отменить.");
        if (ImGui::Button("Удалить")) {
            try {
                int count = 0;
                mutateHistorySafely(context.monitor, [&] {
                    const auto now = std::chrono::system_clock::now();
                    count = context.repository.deleteByFilter(context.data.historyStart(), now + std::chrono::seconds(1), process_.data(),
                                                              category_.data(), title_.data());
                });
                context.status = "Удалено записей: " + std::to_string(count);
                applog::info(context.status);
                context.data.reload(context.monitor);
            } catch (const std::exception& e) {
                context.status = e.what();
                applog::error(std::string("Удаление по фильтру: ") + e.what());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("delete-all", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Удалить ВСЮ историю активности?\nЭто действие нельзя отменить.");
        if (ImGui::Button("Удалить всё")) {
            try {
                mutateHistorySafely(context.monitor, [&] { context.repository.deleteAll(); });
                context.status = "История очищена";
                applog::info(context.status);
                context.data.reload(context.monitor);
            } catch (const std::exception& e) {
                context.status = e.what();
                applog::error(std::string("Очистка истории: ") + e.what());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void HistoryView::drawTable(const ActivityDataCache& data, const std::optional<ActivityRecord>& live) {
    constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    // Таблица занимает всё оставшееся место окна, а не фиксированные 470 пикселей.
    if (!ImGui::BeginTable("history", 6, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Начало");
    ImGui::TableSetupColumn("Программа");
    ImGui::TableSetupColumn("Категория");
    ImGui::TableSetupColumn("Окно");
    ImGui::TableSetupColumn("Статус");
    ImGui::TableSetupColumn("Длительность");
    ImGui::TableHeadersRow();

    const auto& history = data.history();
    const std::size_t liveRows = live ? 1 : 0;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered_.size() + liveRows));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto row = static_cast<std::size_t>(i);
            const auto& r = (live && row == 0) ? *live : history[filtered_[row - liveRows]];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(timeutil::formatLocalDateTime(r.startTime).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.processName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.category.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.windowTitle.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.isIdle ? "Простой" : "Активно");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(timeutil::formatDuration(r.durationSeconds).c_str());
        }
    }
    ImGui::EndTable();
}

void HistoryView::draw(Context& context) {
    drawToolbar(context);

    const auto& history = context.data.history();
    if (filterDirty_ || generation_ != context.data.generation()) {
        filtered_.clear();
        for (std::size_t i = 0; i < history.size(); ++i)
            if (matches(history[i])) filtered_.push_back(i);
        filterDirty_ = false;
        generation_ = context.data.generation();
    }

    auto live = context.monitor.currentRecordPreview();
    if (live && (live->endTime <= context.data.historyStart() || !matches(*live))) live.reset();

    const auto exportRows = [&] {
        std::vector<ActivityRecord> rows;
        rows.reserve(filtered_.size() + 1);
        if (live) rows.push_back(*live);
        for (auto index : filtered_) rows.push_back(history[index]);
        return rows;
    };
    const auto exportTo = [&](const char* fileName, auto exporter) {
        try {
            const auto file = context.exportDirectory / fileName;
            exporter(exportRows(), file);
            context.status = "Сохранено: " + pathUtf8(file);
        } catch (const std::exception& e) {
            context.status = e.what();
            applog::error(std::string("Экспорт истории: ") + e.what());
        }
    };
    if (ImGui::Button("Экспорт CSV")) exportTo("history.csv", [](const auto& rows, const auto& file) { exportActivityCsv(rows, file); });
    ImGui::SameLine();
    if (ImGui::Button("Экспорт JSON")) exportTo("history.json", [](const auto& rows, const auto& file) { exportActivityJson(rows, file); });
    ImGui::SameLine();
    if (ImGui::Button("Удалить по фильтру")) ImGui::OpenPopup("delete-filtered");
    ImGui::SameLine();
    if (ImGui::Button("Очистить всю историю")) ImGui::OpenPopup("delete-all");
    drawDeletePopups(context);
    // Удаление могло перечитать данные: индексы фильтра тогда устарели.
    if (generation_ != context.data.generation()) return;
    drawTable(context.data, live);
}
} // namespace pcat
