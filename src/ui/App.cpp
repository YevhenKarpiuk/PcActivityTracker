#include "ui/App.h"
#include "ui/ReportView.h"
#include "ui/TrayIcon.h"
#include "data/ActivityExporter.h"
#include "data/AppStoragePaths.h"
#include "data/ActivityRepository.h"
#include "data/SettingsService.h"
#include "platform/Autostart.h"
#include "platform/Factory.h"
#include "core/ActivityMonitor.h"
#include "core/TextUtil.h"
#include "core/TimeUtil.h"
#include "reports/ReportEngine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_tray.h>
#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace pcat {
namespace {
struct TrayContext { SDL_Window* window{};ActivityMonitor* monitor{};SDL_TrayEntry* pauseEntry{}; };
void SDLCALL trayShow(void* userdata,SDL_TrayEntry*){auto* c=static_cast<TrayContext*>(userdata);if(c&&c->window){SDL_ShowWindow(c->window);SDL_RaiseWindow(c->window);}}
void SDLCALL trayPause(void* userdata,SDL_TrayEntry*){auto* c=static_cast<TrayContext*>(userdata);if(!c||!c->monitor)return;const bool paused=!c->monitor->isPaused();c->monitor->setPaused(paused);if(c->pauseEntry)SDL_SetTrayEntryChecked(c->pauseEntry,paused);}
void SDLCALL trayQuit(void*,SDL_TrayEntry*){SDL_Event event{};event.type=SDL_EVENT_QUIT;SDL_PushEvent(&event);}

std::string pathUtf8(const std::filesystem::path& path){const auto value=path.u8string();return std::string(reinterpret_cast<const char*>(value.data()),value.size());}
std::string joinLines(const std::vector<std::string>& values){std::string out;for(const auto& value:values){out+=value;out+='\n';}return out;}
std::vector<std::string> splitLines(const char* text){std::vector<std::string> out;std::istringstream stream(text?text:"");std::string line;while(std::getline(stream,line)){line=textutil::trim(line);if(!line.empty())out.push_back(std::move(line));}return out;}
std::string joinCategories(const std::map<std::string,std::string,std::less<>>& values){std::string out;for(const auto&[key,value]:values)out+=key+"="+value+"\n";return out;}
std::map<std::string,std::string,std::less<>> parseCategories(const char* text){std::map<std::string,std::string,std::less<>> result;std::istringstream stream(text?text:"");std::string line;std::size_t lineNumber=0;while(std::getline(stream,line)){++lineNumber;line=textutil::trim(line);if(line.empty())continue;const auto pos=line.find('=');if(pos==std::string::npos)throw std::runtime_error("Категории: строка "+std::to_string(lineNumber)+" должна иметь формат приложение=категория");auto key=textutil::trim(line.substr(0,pos));auto value=textutil::trim(line.substr(pos+1));if(key.empty()||value.empty())throw std::runtime_error("Категории: пустое приложение или категория в строке "+std::to_string(lineNumber));result[key]=value;}return result;}

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
    if (auto* font = io.Fonts->AddFontFromFileTTF(path.c_str(), 17.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) {
        io.FontDefault = font;
    }
}

// Запасная иконка, если системную из ресурсов .exe получить не удалось.
SDL_Surface* createFallbackIcon() {
    auto* icon = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
    if (!icon) return nullptr;
    SDL_FillSurfaceRect(icon, nullptr, SDL_MapSurfaceRGBA(icon, 0, 0, 0, 0));
    const SDL_Rect frame{3, 4, 26, 20};
    const SDL_Rect screen{6, 7, 20, 14};
    const SDL_Rect stand{14, 24, 4, 4};
    const SDL_Rect base{10, 28, 12, 2};
    SDL_FillSurfaceRect(icon, &frame, SDL_MapSurfaceRGBA(icon, 80, 140, 230, 255));
    SDL_FillSurfaceRect(icon, &screen, SDL_MapSurfaceRGBA(icon, 24, 28, 36, 255));
    SDL_FillSurfaceRect(icon, &stand, SDL_MapSurfaceRGBA(icon, 80, 140, 230, 255));
    SDL_FillSurfaceRect(icon, &base, SDL_MapSurfaceRGBA(icon, 80, 140, 230, 255));
    return icon;
}

template<class Fn>
void mutateHistorySafely(ActivityMonitor& monitor, Fn&& operation) {
    const bool wasPaused = monitor.isPaused();
    if (!monitor.pauseAndFlushForMaintenance()) {
        throw std::runtime_error("Не удалось сохранить текущую активность. Мониторинг оставлен на паузе; удаление отменено.");
    }
    try {
        operation();
    } catch (...) {
        if (!wasPaused) monitor.setPaused(false);
        throw;
    }
    if (!wasPaused) monitor.setPaused(false);
}

std::chrono::system_clock::time_point localMidnight(std::chrono::system_clock::time_point now){auto tm=timeutil::localTm(now);tm.tm_hour=tm.tm_min=tm.tm_sec=0;return timeutil::fromLocalTm(tm);}
bool matchesHistory(const ActivityRecord& r,const char* process,const char* category,const char* title){return textutil::containsCaseInsensitive(r.processName,process?process:"")&&textutil::containsCaseInsensitive(r.category,category?category:"")&&textutil::containsCaseInsensitive(r.windowTitle,title?title:"");}
}

int runApp(const std::filesystem::path& executablePath){
    const auto executableDirectory=executablePath.has_parent_path()?executablePath.parent_path():std::filesystem::current_path();
    auto dataDir=appDataDirectory(executableDirectory);ActivityRepository repo(dataDir/"activity_tracker.db");repo.initialize();SettingsService settingsService(dataDir/"settings.json");auto settings=settingsService.load();
    std::string startupStatus;{std::string autostartError;if(!setAutostart(settings.startWithSystem,executablePath,autostartError))startupStatus="Автозапуск: "+autostartError;}
    // Поля settings.json с неверным типом больше не сбрасывают весь файл, поэтому о них надо сказать явно.
    for(const auto& warning:settingsService.lastLoadWarnings()){if(!startupStatus.empty())startupStatus+=" | ";startupStatus+="settings.json: "+warning;}
    // Записи, чьё время не удалось восстановить, не попадают ни в один период. Молча терять их нельзя.
    try{if(const auto undated=repo.undatedRecordCount();undated>0){if(!startupStatus.empty())startupStatus+=" | ";startupStatus+="Записей с нечитаемой датой: "+std::to_string(undated)+" (не отображаются в истории и отчётах)";}}catch(const std::exception&){}
    std::atomic_bool dataDirty{true};
    auto provider=createActivityProvider();ActivityMonitor monitor(*provider,repo,settings);
    monitor.setRecordCallback([&](const ActivityRecord&){dataDirty.store(true);});monitor.start();

    if(!SDL_Init(SDL_INIT_VIDEO)){monitor.stop();throw std::runtime_error(std::string("SDL initialization failed: ")+SDL_GetError());}
    SDL_Window* window=SDL_CreateWindow("PcActivityTracker",1280,760,SDL_WINDOW_RESIZABLE);if(!window){monitor.stop();const std::string error=SDL_GetError();SDL_Quit();throw std::runtime_error("Cannot create application window: "+error);}
        // Настоящая иконка приложения лежит ресурсом в самом .exe, поэтому окно и трей показывают
    // то же изображение, что проводник и панель задач, а не нарисованный в коде прямоугольник.
    SDL_Surface* appIcon=loadApplicationIconSurface(IconSize::Window);if(!appIcon)appIcon=createFallbackIcon();if(appIcon)SDL_SetWindowIcon(window,appIcon);
    SDL_Renderer* renderer=SDL_CreateRenderer(window,nullptr);if(!renderer){monitor.stop();const std::string error=SDL_GetError();if(appIcon)SDL_DestroySurface(appIcon);SDL_DestroyWindow(window);SDL_Quit();throw std::runtime_error("Cannot create SDL renderer: "+error);}SDL_SetRenderVSync(renderer,1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImPlot::CreateContext();ImGui::StyleColorsDark();loadUiFont();
    const bool imguiPlatformReady=ImGui_ImplSDL3_InitForSDLRenderer(window,renderer);
    const bool imguiRendererReady=imguiPlatformReady&&ImGui_ImplSDLRenderer3_Init(renderer);
    if(!imguiRendererReady){monitor.stop();if(imguiPlatformReady)ImGui_ImplSDL3_Shutdown();ImPlot::DestroyContext();ImGui::DestroyContext();SDL_DestroyRenderer(renderer);if(appIcon)SDL_DestroySurface(appIcon);SDL_DestroyWindow(window);SDL_Quit();throw std::runtime_error("Cannot initialize Dear ImGui SDL3 backend");}

    bool done=false;ReportView reportView;reportView.setExportDirectory(dataDir);std::vector<ActivityRecord> todayRecords,historyRecords;int historyDays=30;auto historyStart=std::chrono::system_clock::now()-std::chrono::hours(24*historyDays);std::string uiStatus=std::move(startupStatus);
    std::array<char,128> historyProcess{},historyCategory{};std::array<char,256> historyTitle{};
    std::vector<std::size_t> filteredHistory;
    bool historyFilterDirty=true,summaryDirty=true;
    TodaySummary summary;
    auto lastSummary=std::chrono::steady_clock::time_point{};
    AppSettings draftSettings=settings;
    std::string excludedProcesses=joinLines(settings.excludedProcesses);
    std::string excludedTitles=joinLines(settings.excludedWindowTitles);
    std::string excludedPatterns=joinLines(settings.excludedWindowTitlePatterns);
    std::string excludedDomains=joinLines(settings.excludedBrowserDomains);
    std::string categories=joinCategories(settings.categories);
    bool appliedAutostart=settings.startWithSystem;

    // Каждый закрытый интервал помечает данные грязными. Перечитывать за это всю историю (до 3650
    // дней) на каждое событие нельзя, поэтому фоновые обновления коалесцируются по времени.
    // Кнопка «Обновить данные» и смена периода перечитывают немедленно.
    auto lastReload=std::chrono::steady_clock::now();
    constexpr auto reloadInterval=std::chrono::seconds(5);
    auto reloadData=[&]{const auto now=std::chrono::system_clock::now();historyDays=std::clamp(historyDays,1,3650);historyStart=now-std::chrono::hours(24LL*historyDays);historyRecords=monitor.readSnapshot(historyStart,now+std::chrono::seconds(1)).records;todayRecords.clear();const auto midnight=localMidnight(now);for(const auto& record:historyRecords)if(record.endTime>midnight)todayRecords.push_back(record);filteredHistory.clear();historyFilterDirty=true;summaryDirty=true;lastReload=std::chrono::steady_clock::now();};dataDirty.store(false);reloadData();

    // Трею нужен свой размер: SDL масштабирует хуже, чем сама Windows при извлечении из .ico,
    // где для мелких размеров лежат отдельно отрисованные варианты.
    SDL_Surface* trayIcon=loadApplicationIconSurface(IconSize::Tray);if(!trayIcon)trayIcon=createFallbackIcon();
    TrayContext trayContext{window,&monitor,nullptr};SDL_Tray* tray=SDL_CreateTray(trayIcon?trayIcon:appIcon,"PcActivityTracker");
    if(tray){
        if(auto* menu=SDL_CreateTrayMenu(tray)){
            if(auto* show=SDL_InsertTrayEntryAt(menu,-1,"Показать",SDL_TRAYENTRY_BUTTON))SDL_SetTrayEntryCallback(show,trayShow,&trayContext);
            trayContext.pauseEntry=SDL_InsertTrayEntryAt(menu,-1,"Пауза мониторинга",SDL_TRAYENTRY_CHECKBOX);
            if(trayContext.pauseEntry)SDL_SetTrayEntryCallback(trayContext.pauseEntry,trayPause,&trayContext);
            if(auto* quit=SDL_InsertTrayEntryAt(menu,-1,"Выход",SDL_TRAYENTRY_BUTTON))SDL_SetTrayEntryCallback(quit,trayQuit,&trayContext);
        }else{SDL_DestroyTray(tray);tray=nullptr;}
    }

    while(!done){
        SDL_Event event;while(SDL_PollEvent(&event)){ImGui_ImplSDL3_ProcessEvent(&event);if(event.type==SDL_EVENT_QUIT)done=true;if(event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&event.window.windowID==SDL_GetWindowID(window)){if(tray)SDL_HideWindow(window);else done=true;}}
        if(dataDirty.load()&&std::chrono::steady_clock::now()-lastReload>=reloadInterval){dataDirty.store(false);try{reloadData();}catch(const std::exception&e){uiStatus=e.what();dataDirty.store(true);}}
        ImGui_ImplSDLRenderer3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);ImGui::Begin("PcActivityTracker",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
        const auto caps=provider->capabilities();ImGui::Text("Мониторинг: %s | %s",monitor.isPaused()?"пауза":"работает",caps.note.c_str());ImGui::SameLine();if(ImGui::Button(monitor.isPaused()?"Продолжить":"Пауза")){monitor.setPaused(!monitor.isPaused());if(trayContext.pauseEntry)SDL_SetTrayEntryChecked(trayContext.pauseEntry,monitor.isPaused());}ImGui::SameLine();if(ImGui::Button("Обновить данные")){try{reloadData();uiStatus="Данные обновлены";}catch(const std::exception&e){uiStatus=e.what();}}if(tray){ImGui::SameLine();if(ImGui::Button("Скрыть в трей"))SDL_HideWindow(window);}
        if(const auto error=monitor.lastError();!error.empty()){ImGui::TextColored(ImVec4(1.0f,0.45f,0.35f,1.0f),"Ошибка мониторинга: %s",error.c_str());}
        if(!uiStatus.empty())ImGui::TextUnformatted(uiStatus.c_str());

        if(ImGui::BeginTabBar("tabs")){
            if(ImGui::BeginTabItem("Сегодня")){
                const auto summaryNow=std::chrono::system_clock::now();auto live=monitor.currentRecordPreview(summaryNow);
                if(summaryDirty||std::chrono::steady_clock::now()-lastSummary>=std::chrono::seconds(1)){
                    auto records=todayRecords;if(live)records.push_back(*live);
                    summary=buildTodaySummary(records,localMidnight(summaryNow),summaryNow);
                    lastSummary=std::chrono::steady_clock::now();summaryDirty=false;
                }
                ImGui::Text("Активно: %s",timeutil::formatDuration(summary.values.activeSeconds).c_str());ImGui::SameLine();ImGui::Text("Простой: %s",timeutil::formatDuration(summary.values.idleSeconds).c_str());if(!summary.topProcess.empty()){ImGui::Text("Самая активная программа: %s (%s)",summary.topProcess.c_str(),timeutil::formatDuration(summary.topActiveSeconds).c_str());}
                ImGui::Separator();if(live){ImGui::TextColored(ImVec4(0.65f,0.9f,0.65f,1),"Сейчас: %s | %s | %s",live->processName.c_str(),live->category.c_str(),timeutil::formatDuration(live->durationSeconds).c_str());}
                // Список за день может содержать тысячи интервалов: рисуем только видимые строки.
                {ImGuiListClipper clipper;clipper.Begin(static_cast<int>(todayRecords.size()));
                 while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto&r=todayRecords[static_cast<std::size_t>(i)];ImGui::Text("%s | %s | %s",r.processName.c_str(),r.category.c_str(),timeutil::formatDuration(r.durationSeconds).c_str());}}
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("История")){
                ImGui::SetNextItemWidth(100);if(ImGui::InputInt("Дней",&historyDays)){historyDays=std::clamp(historyDays,1,3650);try{reloadData();}catch(const std::exception&e){uiStatus=e.what();}}ImGui::SameLine();ImGui::SetNextItemWidth(170);historyFilterDirty|=ImGui::InputText("Программа##history",historyProcess.data(),historyProcess.size());ImGui::SameLine();ImGui::SetNextItemWidth(170);historyFilterDirty|=ImGui::InputText("Категория##history",historyCategory.data(),historyCategory.size());ImGui::SameLine();ImGui::SetNextItemWidth(240);historyFilterDirty|=ImGui::InputText("Окно##history",historyTitle.data(),historyTitle.size());ImGui::SameLine();if(ImGui::Button("Применить период")){try{reloadData();}catch(const std::exception&e){uiStatus=e.what();}}
                if(historyFilterDirty){filteredHistory.clear();for(std::size_t i=0;i<historyRecords.size();++i)if(matchesHistory(historyRecords[i],historyProcess.data(),historyCategory.data(),historyTitle.data()))filteredHistory.push_back(i);historyFilterDirty=false;}
                auto live=monitor.currentRecordPreview();if(live&&(live->endTime<=historyStart||!matchesHistory(*live,historyProcess.data(),historyCategory.data(),historyTitle.data())))live.reset();
                const auto exportRows=[&]{std::vector<ActivityRecord> rows;rows.reserve(filteredHistory.size()+1);if(live)rows.push_back(*live);for(auto index:filteredHistory)rows.push_back(historyRecords[index]);return rows;};
                if(ImGui::Button("Экспорт CSV")){try{const auto file=dataDir/"history.csv";exportActivityCsv(exportRows(),file);uiStatus="Сохранено: "+pathUtf8(file);}catch(const std::exception&e){uiStatus=e.what();}}ImGui::SameLine();if(ImGui::Button("Экспорт JSON")){try{const auto file=dataDir/"history.json";exportActivityJson(exportRows(),file);uiStatus="Сохранено: "+pathUtf8(file);}catch(const std::exception&e){uiStatus=e.what();}}ImGui::SameLine();if(ImGui::Button("Удалить по фильтру"))ImGui::OpenPopup("delete-filtered");ImGui::SameLine();if(ImGui::Button("Очистить всю историю"))ImGui::OpenPopup("delete-all");
                if(ImGui::BeginPopupModal("delete-filtered",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::Text("Удалить записи за выбранный период, соответствующие фильтрам?\nЭто действие нельзя отменить.");if(ImGui::Button("Удалить")){try{int count=0;mutateHistorySafely(monitor,[&]{const auto now=std::chrono::system_clock::now();count=repo.deleteByFilter(historyStart,now+std::chrono::seconds(1),historyProcess.data(),historyCategory.data(),historyTitle.data());});uiStatus="Удалено записей: "+std::to_string(count);reloadData();}catch(const std::exception&e){uiStatus=e.what();}ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Отмена"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
                if(ImGui::BeginPopupModal("delete-all",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::Text("Удалить ВСЮ историю активности?\nЭто действие нельзя отменить.");if(ImGui::Button("Удалить всё")){try{mutateHistorySafely(monitor,[&]{repo.deleteAll();});uiStatus="История очищена";reloadData();}catch(const std::exception&e){uiStatus=e.what();}ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Отмена"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
                if(ImGui::BeginTable("history",6,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,ImVec2(0,470))){ImGui::TableSetupColumn("Начало");ImGui::TableSetupColumn("Программа");ImGui::TableSetupColumn("Категория");ImGui::TableSetupColumn("Окно");ImGui::TableSetupColumn("Статус");ImGui::TableSetupColumn("Длительность");ImGui::TableHeadersRow();
                    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(filteredHistory.size()+(live?1:0)));
                    while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto&r=(live&&i==0)?*live:historyRecords[filteredHistory[static_cast<std::size_t>(i)-(live?1:0)]];ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(timeutil::toIso8601Utc(r.startTime).c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(r.processName.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(r.category.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(r.windowTitle.c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(r.isIdle?"Простой":"Активно");ImGui::TableNextColumn();ImGui::TextUnformatted(timeutil::formatDuration(r.durationSeconds).c_str());}
                    ImGui::EndTable();}
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Отчёты")){reportView.draw(monitor);ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Настройки")){
                int idle=draftSettings.idleThresholdSeconds,poll=draftSettings.pollingIntervalSeconds;bool hide=draftSettings.hideBrowserWindowTitles,startWithSystem=draftSettings.startWithSystem;if(ImGui::InputInt("Порог простоя, сек",&idle))draftSettings.idleThresholdSeconds=std::max(10,idle);if(ImGui::InputInt("Интервал опроса, сек",&poll))draftSettings.pollingIntervalSeconds=std::clamp(poll,1,60);ImGui::Checkbox("Скрывать заголовки браузера",&hide);draftSettings.hideBrowserWindowTitles=hide;ImGui::Checkbox("Запускать вместе с системой",&startWithSystem);draftSettings.startWithSystem=startWithSystem;
                ImGui::SeparatorText("Исключения и категории");ImGui::TextUnformatted("По одному значению на строку. Шаблоны окон: wildcard или regex:...");ImGui::InputTextMultiline("Исключённые программы",&excludedProcesses,ImVec2(300,90));ImGui::SameLine();ImGui::InputTextMultiline("Точные заголовки окон",&excludedTitles,ImVec2(300,90));ImGui::SameLine();ImGui::InputTextMultiline("Шаблоны заголовков",&excludedPatterns,ImVec2(300,90));ImGui::InputTextMultiline("Исключённые домены",&excludedDomains,ImVec2(300,90));ImGui::SameLine();ImGui::InputTextMultiline("Категории: приложение=категория",&categories,ImVec2(420,120));
                if(ImGui::Button("Сохранить настройки")){
                    const bool previousAutostart=appliedAutostart;
                    bool autostartTouched=false;
                    try{
                        auto candidate=draftSettings;
                        candidate.excludedProcesses=splitLines(excludedProcesses.c_str());
                        candidate.excludedWindowTitles=splitLines(excludedTitles.c_str());
                        candidate.excludedWindowTitlePatterns=splitLines(excludedPatterns.c_str());
                        candidate.excludedBrowserDomains=splitLines(excludedDomains.c_str());
                        candidate.categories=parseCategories(categories.c_str());
                        std::string error;
                        if(!setAutostart(candidate.startWithSystem,executablePath,error))throw std::runtime_error(error);
                        autostartTouched=true;
                        settingsService.save(candidate);
                        appliedAutostart=candidate.startWithSystem;
                        settings=candidate;
                        draftSettings=candidate;
                        monitor.setSettings(settings);
                        uiStatus="Настройки сохранены";
                    }catch(const std::exception&e){
                        std::string rollbackError;
                        if(autostartTouched&&draftSettings.startWithSystem!=previousAutostart&&!setAutostart(previousAutostart,executablePath,rollbackError)){
                            uiStatus=std::string("Ошибка настроек: ")+e.what()+"; не удалось восстановить автозапуск: "+rollbackError;
                        }else{
                            uiStatus=std::string("Ошибка настроек: ")+e.what();
                        }
                        appliedAutostart=previousAutostart;
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();ImGui::Render();SDL_SetRenderDrawColor(renderer,25,27,31,255);SDL_RenderClear(renderer);ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),renderer);SDL_RenderPresent(renderer);if((SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN)!=0)SDL_Delay(100);
    }

    if(tray)SDL_DestroyTray(tray);
    monitor.stop();ImGui_ImplSDLRenderer3_Shutdown();ImGui_ImplSDL3_Shutdown();ImPlot::DestroyContext();ImGui::DestroyContext();SDL_DestroyRenderer(renderer);
    if(trayIcon)SDL_DestroySurface(trayIcon);
    if(appIcon)SDL_DestroySurface(appIcon);
    SDL_DestroyWindow(window);SDL_Quit();return 0;
}
}
