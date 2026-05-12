namespace PcActivityTracker.Core.Models;

public sealed class AppSettings
{
    public int IdleThresholdSeconds { get; set; } = 60;
    public int PollingIntervalSeconds { get; set; } = 2;
    public bool HideBrowserWindowTitles { get; set; } = true;
    public bool StartWithWindows { get; set; }
    public ProgramChartSettings ProgramChartSettings { get; set; } = new();

    /// <summary>
    /// Processes that must not be tracked, one executable name per item.
    /// Example: chrome.exe, 1cv8.exe, telegram.exe.
    /// </summary>
    public List<string> ExcludedProcesses { get; set; } = new()
    {
        "lockapp.exe",
        "searchhost.exe",
        "shellexperiencehost.exe",
        "systemsettings.exe",
        "applicationframehost.exe"
    };

    /// <summary>
    /// Exact active window titles that must not be tracked.
    /// Comparison is case-insensitive.
    /// </summary>
    public List<string> ExcludedWindowTitles { get; set; } = new();

    /// <summary>
    /// Active window title masks that must not be tracked.
    /// Supports wildcard masks: *private*, *пароль*, Customer ?.
    /// Supports regex masks with prefix regex:, for example regex:^.*secret.*$.
    /// </summary>
    public List<string> ExcludedWindowTitlePatterns { get; set; } = new();

    /// <summary>
    /// Browser domains that must not be tracked.
    /// Example: youtube.com also matches www.youtube.com and m.youtube.com.
    /// Wildcard example: *.example.com.
    /// </summary>
    public List<string> ExcludedBrowserDomains { get; set; } = new();

    public Dictionary<string, string> Categories { get; set; } = new(StringComparer.OrdinalIgnoreCase)
    {
        ["1cv8.exe"] = "1С / BAS",
        ["1cv8c.exe"] = "1С / BAS",
        ["code.exe"] = "Разработка",
        ["devenv.exe"] = "Разработка",
        ["ssms.exe"] = "SQL Server",
        ["chrome.exe"] = "Браузер",
        ["msedge.exe"] = "Браузер",
        ["firefox.exe"] = "Браузер",
        ["opera.exe"] = "Браузер",
        ["brave.exe"] = "Браузер",
        ["telegram.exe"] = "Общение",
        ["teams.exe"] = "Общение",
        ["mstsc.exe"] = "RDP",
        ["explorer.exe"] = "Система"
    };
}
