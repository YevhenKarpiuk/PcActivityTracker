#include "ui/App.h"

#include "core/ActivityMonitor.h"
#include "data/ActivityRepository.h"
#include "data/AppLog.h"
#include "data/AppStoragePaths.h"
#include "data/SettingsService.h"
#include "platform/Autostart.h"
#include "platform/Factory.h"
#include "platform/SessionEnd.h"
#include "platform/SingleInstance.h"
#include "ui/ActivityDataCache.h"
#include "ui/HistoryView.h"
#include "ui/ReportView.h"
#include "ui/SettingsView.h"
#include "ui/TodayView.h"
#include "ui/TrayIcon.h"
#include "ui/UiCommon.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_tray.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <implot.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace pcat {
namespace {
// ---------- Владение ресурсами SDL/ImGui (RAII вместо ручной очистки на каждом пути ошибки) ----------
struct SdlDeleter {
    void operator()(SDL_Window* p) const noexcept { SDL_DestroyWindow(p); }
    void operator()(SDL_Renderer* p) const noexcept { SDL_DestroyRenderer(p); }
    void operator()(SDL_Surface* p) const noexcept { SDL_DestroySurface(p); }
    void operator()(SDL_Tray* p) const noexcept { SDL_DestroyTray(p); }
};
template<class T> using SdlPtr = std::unique_ptr<T, SdlDeleter>;

class SdlVideo {
public:
    SdlVideo() {
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
    }
    ~SdlVideo() { SDL_Quit(); }
    SdlVideo(const SdlVideo&) = delete;
    SdlVideo& operator=(const SdlVideo&) = delete;
};

class ImGuiSession {
public:
    ImGuiSession(SDL_Window* window, SDL_Renderer* renderer) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();
        platformReady_ = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        rendererReady_ = platformReady_ && ImGui_ImplSDLRenderer3_Init(renderer);
        if (!rendererReady_) {
            shutdown();
            throw std::runtime_error("Cannot initialize Dear ImGui SDL3 backend");
        }
    }
    ~ImGuiSession() { shutdown(); }
    ImGuiSession(const ImGuiSession&) = delete;
    ImGuiSession& operator=(const ImGuiSession&) = delete;

private:
    bool platformReady_{false};
    bool rendererReady_{false};
    bool shutDown_{false};
    void shutdown() noexcept {
        if (shutDown_) return;
        shutDown_ = true;
        if (rendererReady_) ImGui_ImplSDLRenderer3_Shutdown();
        if (platformReady_) ImGui_ImplSDL3_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }
};

// ---------- Трей ----------
struct TrayContext {
    SDL_Window* window{};
    ActivityMonitor* monitor{};
    SDL_TrayEntry* pauseEntry{};
};
void showWindow(SDL_Window* window) {
    SDL_ShowWindow(window);
    SDL_RestoreWindow(window);
    SDL_RaiseWindow(window);
}
void SDLCALL trayShow(void* userdata, SDL_TrayEntry*) {
    auto* c = static_cast<TrayContext*>(userdata);
    if (c && c->window) showWindow(c->window);
}
void SDLCALL trayPause(void* userdata, SDL_TrayEntry*) {
    auto* c = static_cast<TrayContext*>(userdata);
    if (!c || !c->monitor) return;
    const bool paused = !c->monitor->isPaused();
    c->monitor->setPaused(paused);
    if (c->pauseEntry) SDL_SetTrayEntryChecked(c->pauseEntry, paused);
}
void SDLCALL trayQuit(void*, SDL_TrayEntry*) {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

SdlPtr<SDL_Tray> createTray(SDL_Surface* icon, TrayContext& context) {
    SdlPtr<SDL_Tray> tray(SDL_CreateTray(icon, "PcActivityTracker"));
    if (!tray) return tray;
    auto* menu = SDL_CreateTrayMenu(tray.get());
    if (!menu) return nullptr;
    if (auto* show = SDL_InsertTrayEntryAt(menu, -1, "Показать", SDL_TRAYENTRY_BUTTON)) SDL_SetTrayEntryCallback(show, trayShow, &context);
    context.pauseEntry = SDL_InsertTrayEntryAt(menu, -1, "Пауза мониторинга", SDL_TRAYENTRY_CHECKBOX);
    if (context.pauseEntry) SDL_SetTrayEntryCallback(context.pauseEntry, trayPause, &context);
    if (auto* quit = SDL_InsertTrayEntryAt(menu, -1, "Выход", SDL_TRAYENTRY_BUTTON)) SDL_SetTrayEntryCallback(quit, trayQuit, &context);
    return tray;
}

// ---------- Шрифт и иконка ----------
std::optional<std::filesystem::path> findUiFont() {
    // Do not bundle a font: use common Unicode-capable system fonts and keep the application small.
    // The first existing candidate is loaded with ImGui's Cyrillic range so Russian/Ukrainian UI text
    // is rendered correctly even with renderers that still use the legacy fixed atlas path.
    const std::filesystem::path candidates[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#elif __APPLE__
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
#endif
    };
    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && !ec) return candidate;
        ec.clear();
    }
    return std::nullopt;
}

void loadUiFont() {
    const auto fontPath = findUiFont();
    if (!fontPath) return; // ImGui's embedded font remains a last-resort fallback.
    auto& io = ImGui::GetIO();
    const auto path = fontPath->string(); // system candidates above are ASCII paths on supported OSes.
    if (auto* font = io.Fonts->AddFontFromFileTTF(path.c_str(), 17.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) io.FontDefault = font;
}

// Запасная иконка, если системную из ресурсов .exe получить не удалось.
SdlPtr<SDL_Surface> createFallbackIcon() {
    SdlPtr<SDL_Surface> icon(SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32));
    if (!icon) return icon;
    auto* s = icon.get();
    SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, 0, 0, 0, 0));
    const SDL_Rect frame{3, 4, 26, 20};
    const SDL_Rect screen{6, 7, 20, 14};
    const SDL_Rect stand{14, 24, 4, 4};
    const SDL_Rect base{10, 28, 12, 2};
    SDL_FillSurfaceRect(s, &frame, SDL_MapSurfaceRGBA(s, 80, 140, 230, 255));
    SDL_FillSurfaceRect(s, &screen, SDL_MapSurfaceRGBA(s, 24, 28, 36, 255));
    SDL_FillSurfaceRect(s, &stand, SDL_MapSurfaceRGBA(s, 80, 140, 230, 255));
    SDL_FillSurfaceRect(s, &base, SDL_MapSurfaceRGBA(s, 80, 140, 230, 255));
    return icon;
}

// Настоящая иконка приложения лежит ресурсом в самом .exe, поэтому окно и трей показывают
// то же изображение, что проводник и панель задач.
SdlPtr<SDL_Surface> applicationIcon(IconSize size) {
    SdlPtr<SDL_Surface> icon(loadApplicationIconSurface(size));
    return icon ? std::move(icon) : createFallbackIcon();
}

void appendStatus(std::string& status, const std::string& part) {
    if (!status.empty()) status += " | ";
    status += part;
}
} // namespace

int runApp(const std::filesystem::path& executablePath, const AppLaunchOptions& options) {
    const auto executableDirectory = executablePath.has_parent_path() ? executablePath.parent_path() : std::filesystem::current_path();
    const auto dataDir = appDataDirectory(executableDirectory);
    // С portable.txt каталог data ещё может не существовать (первый запуск распакованного архива),
    // а блокировке экземпляра и журналу он нужен раньше, чем базе.
    {
        std::error_code ec;
        std::filesystem::create_directories(dataDir, ec);
    }

    // Второй процесс на ту же базу удвоил бы записанное время. Вместо этого он просит уже
    // работающую копию показать окно и выходит. Повторный автозапуск выходит молча.
    SingleInstance instance(dataDir);
    if (!instance.isPrimary()) {
        if (!options.startHidden) instance.requestShowPrimary();
        return 0;
    }

    try {
        applog::open(dataDir / "PcActivityTracker.log");
    } catch (...) {
        // Журнал — вспомогательный: без него программа работает как раньше.
    }
    applog::info("Запуск: " + pathUtf8(executablePath) + (options.startHidden ? " (автозапуск, в трей)" : "") +
                 "; данные: " + pathUtf8(dataDir));

    ActivityRepository repo(dataDir / "activity_tracker.db");
    repo.initialize();
    SettingsService settingsService(dataDir / "settings.json");
    auto settings = settingsService.load();

    std::string status;
    if (const auto autostart = setAutostart(settings.startWithSystem, executablePath, AutostartReason::Startup); !autostart.ok) {
        appendStatus(status, "Автозапуск: " + autostart.error);
        applog::error("Автозапуск: " + autostart.error);
    } else if (!autostart.notice.empty()) {
        appendStatus(status, "Автозапуск: " + autostart.notice);
        applog::warning("Автозапуск: " + autostart.notice);
    }
    // Поля settings.json с неверным типом больше не сбрасывают весь файл, поэтому о них надо сказать явно.
    for (const auto& warning : settingsService.lastLoadWarnings()) {
        appendStatus(status, "settings.json: " + warning);
        applog::warning("settings.json: " + warning);
    }
    // Записи, чьё время не удалось восстановить, не попадают ни в один период. Молча терять их нельзя.
    try {
        if (const auto undated = repo.undatedRecordCount(); undated > 0)
            appendStatus(status, "Записей с нечитаемой датой: " + std::to_string(undated) + " (не отображаются в истории и отчётах)");
    } catch (const std::exception&) {
    }

    ActivityDataCache data;
    auto provider = createActivityProvider();
    ActivityMonitor monitor(*provider, repo, settings);
    monitor.setRecordCallback([&data](const ActivityRecord&) { data.markDirty(); });
    monitor.start();
    // Деструктор ActivityMonitor сам останавливает мониторинг и сбрасывает текущий интервал,
    // поэтому исключения ниже не теряют данные.

    SdlVideo video;
    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | (options.startHidden ? SDL_WINDOW_HIDDEN : SDL_WindowFlags{0});
    SdlPtr<SDL_Window> window(SDL_CreateWindow("PcActivityTracker", 1280, 760, windowFlags));
    if (!window) throw std::runtime_error(std::string("Cannot create application window: ") + SDL_GetError());
    const auto appIcon = applicationIcon(IconSize::Window);
    if (appIcon) SDL_SetWindowIcon(window.get(), appIcon.get());
    SdlPtr<SDL_Renderer> renderer(SDL_CreateRenderer(window.get(), nullptr));
    if (!renderer) throw std::runtime_error(std::string("Cannot create SDL renderer: ") + SDL_GetError());
    SDL_SetRenderVSync(renderer.get(), 1);
    // imgui.ini по умолчанию пишется в текущий каталог процесса, а у GUI-приложения это
    // случайное место (при автозапуске — C:\Windows\System32). Держим его в каталоге данных.
    // Строка объявлена до ImGuiSession: ImGui сохраняет ini при уничтожении контекста.
    const std::string imguiIni = pathUtf8(dataDir / "imgui.ini");
    ImGuiSession imgui(window.get(), renderer.get());
    ImGui::GetIO().IniFilename = imguiIni.c_str();
    loadUiFont();

    // Выход из Windows / выключение: дописать текущий интервал, пока процесс ещё жив.
    const SessionEndWatcher sessionEnd(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window.get()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr), [&monitor] {
            applog::info("Завершение сеанса ОС: сохраняю текущий интервал");
            monitor.stop();
        });

    try {
        data.reload(monitor);
    } catch (const std::exception& e) {
        appendStatus(status, e.what());
        applog::error(std::string("Чтение истории: ") + e.what());
    }

    // Трею нужен свой размер: SDL масштабирует хуже, чем сама Windows при извлечении из .ico,
    // где для мелких размеров лежат отдельно отрисованные варианты.
    const auto trayIcon = applicationIcon(IconSize::Tray);
    TrayContext trayContext{window.get(), &monitor, nullptr};
    const auto tray = createTray(trayIcon ? trayIcon.get() : appIcon.get(), trayContext);
    // Без трея скрытое окно было бы невозможно вернуть: показываем его сразу.
    if (options.startHidden && !tray) {
        SDL_ShowWindow(window.get());
        applog::warning("Трей недоступен, окно показано");
    }

    TodayView todayView;
    HistoryView historyView;
    ReportView reportView;
    reportView.setExportDirectory(dataDir);
    SettingsView settingsView(settings);
    HistoryView::Context historyContext{monitor, repo, data, dataDir, status};
    SettingsView::Context settingsContext{settings, settingsService, monitor, executablePath, status};
    std::string lastMonitorError;

    bool done = false;
    while (!done) {
        if (instance.consumeShowRequest()) showWindow(window.get());
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window.get())) {
                // Скрытое окно пользователь закрыть не может: WM_CLOSE ему шлёт система или
                // taskkill без /F. Это просьба завершиться, а не свернуть окно ещё раз.
                const bool hidden = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_HIDDEN) != 0;
                if (tray && !hidden) SDL_HideWindow(window.get());
                else done = true;
            }
        }
        if (data.reloadDue()) {
            try {
                data.reload(monitor);
            } catch (const std::exception& e) {
                status = e.what();
                applog::error(std::string("Чтение истории: ") + e.what());
            }
        }
        const auto monitorError = monitor.lastError();
        if (monitorError != lastMonitorError) {
            if (!monitorError.empty()) applog::error("Мониторинг: " + monitorError);
            else if (!lastMonitorError.empty()) applog::info("Мониторинг восстановлен");
            lastMonitorError = monitorError;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("PcActivityTracker", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        const auto caps = provider->capabilities();
        ImGui::Text("Мониторинг: %s | %s", monitor.isPaused() ? "пауза" : "работает", caps.note.c_str());
        ImGui::SameLine();
        if (ImGui::Button(monitor.isPaused() ? "Продолжить" : "Пауза")) {
            monitor.setPaused(!monitor.isPaused());
            if (trayContext.pauseEntry) SDL_SetTrayEntryChecked(trayContext.pauseEntry, monitor.isPaused());
        }
        ImGui::SameLine();
        if (ImGui::Button("Обновить данные")) {
            try {
                data.reload(monitor);
                status = "Данные обновлены";
            } catch (const std::exception& e) {
                status = e.what();
            }
        }
        if (tray) {
            ImGui::SameLine();
            if (ImGui::Button("Скрыть в трей")) SDL_HideWindow(window.get());
        }
        if (!monitorError.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Ошибка мониторинга: %s", monitorError.c_str());
        if (!status.empty()) ImGui::TextUnformatted(status.c_str());

        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("Сегодня")) {
                todayView.draw(monitor, data);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("История")) {
                historyView.draw(historyContext);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Отчёты")) {
                reportView.draw(monitor);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Настройки")) {
                settingsView.draw(settingsContext);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer.get(), 25, 27, 31, 255);
        SDL_RenderClear(renderer.get());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
        SDL_RenderPresent(renderer.get());
        // Скрытое окно не нуждается в 60 кадрах в секунду.
        if ((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_HIDDEN) != 0) SDL_Delay(100);
    }

    monitor.stop();
    applog::info("Завершение работы");
    return 0;
}
} // namespace pcat
