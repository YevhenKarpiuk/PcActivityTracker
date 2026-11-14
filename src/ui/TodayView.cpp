#include "ui/TodayView.h"

#include "core/TimeUtil.h"
#include "ui/UiCommon.h"

#include <imgui.h>

namespace pcat {
void TodayView::draw(const ActivityMonitor& monitor, const ActivityDataCache& data) {
    const auto now = std::chrono::system_clock::now();
    const auto live = monitor.currentRecordPreview(now);
    // Текущий интервал растёт каждую секунду, поэтому итоги пересчитываются раз в секунду,
    // а не каждый кадр.
    if (generation_ != data.generation() || std::chrono::steady_clock::now() - lastSummary_ >= std::chrono::seconds(1)) {
        auto records = data.today();
        if (live) records.push_back(*live);
        summary_ = buildTodaySummary(records, localMidnight(now), now);
        lastSummary_ = std::chrono::steady_clock::now();
        generation_ = data.generation();
    }

    ImGui::Text("Активно: %s", timeutil::formatDuration(summary_.values.activeSeconds).c_str());
    ImGui::SameLine();
    ImGui::Text("Простой: %s", timeutil::formatDuration(summary_.values.idleSeconds).c_str());
    if (!summary_.topProcess.empty())
        ImGui::Text("Самая активная программа: %s (%s)", summary_.topProcess.c_str(),
                    timeutil::formatDuration(summary_.topActiveSeconds).c_str());
    ImGui::Separator();
    if (live)
        ImGui::TextColored(ImVec4(0.65f, 0.9f, 0.65f, 1), "Сейчас: %s | %s | %s", live->processName.c_str(), live->category.c_str(),
                           timeutil::formatDuration(live->durationSeconds).c_str());

    // Список за день может содержать тысячи интервалов: рисуем только видимые строки.
    const auto& today = data.today();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(today.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& r = today[static_cast<std::size_t>(i)];
            ImGui::Text("%s | %s | %s | %s", timeutil::formatLocalDateTime(r.startTime).substr(11).c_str(), r.processName.c_str(),
                        r.category.c_str(), timeutil::formatDuration(r.durationSeconds).c_str());
        }
    }
}
} // namespace pcat
