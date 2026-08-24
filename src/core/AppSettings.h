#pragma once
#include <map>
#include <string>
#include <vector>

namespace pcat {
struct AppSettings {
    int idleThresholdSeconds{60};
    int pollingIntervalSeconds{2};
    bool hideBrowserWindowTitles{true};
    bool startWithSystem{false};
    std::vector<std::string> excludedProcesses{
        "lockapp.exe", "searchhost.exe", "shellexperiencehost.exe",
        "systemsettings.exe", "applicationframehost.exe"
    };
    std::vector<std::string> excludedWindowTitles;
    std::vector<std::string> excludedWindowTitlePatterns;
    std::vector<std::string> excludedBrowserDomains;
    // Keys may be Windows executable names or extension-free application ids. Matching is canonical
    // and therefore code.exe/code, firefox.exe/firefox etc. are equivalent.
    std::map<std::string,std::string,std::less<>> categories{
        {"1cv8.exe","1С / BAS"},{"1cv8c.exe","1С / BAS"},{"code.exe","Разработка"},{"visual studio code","Разработка"},{"devenv.exe","Разработка"},
        {"ssms.exe","SQL Server"},{"chrome.exe","Браузер"},{"google-chrome","Браузер"},{"google chrome","Браузер"},{"chromium","Браузер"},
        {"msedge.exe","Браузер"},{"microsoft edge","Браузер"},{"firefox.exe","Браузер"},{"opera.exe","Браузер"},{"brave.exe","Браузер"},
        {"brave-browser","Браузер"},{"brave browser","Браузер"},{"safari","Браузер"},{"telegram.exe","Общение"},{"telegram-desktop","Общение"},
        {"teams.exe","Общение"},{"microsoft teams","Общение"},{"mstsc.exe","RDP"},{"explorer.exe","Система"}
    };
};
}
