namespace PcActivityTracker.Core.Models;

public sealed class ProgramChartSettings
{
    public const string GroupByProcessName = "ProcessName";
    public const string GroupByCategory = "Category";
    public const string GroupByBrowserDomain = "BrowserDomain";
    public const string GroupByWindowTitle = "WindowTitle";
    public const string GroupByStatus = "Status";
    public const string GroupByDay = "Day";

    public DateTime? StartDate { get; set; }
    public DateTime? EndDate { get; set; }
    public string GroupBy { get; set; } = GroupByProcessName;
    public int TopItems { get; set; } = 12;
    public bool IncludeIdleRecords { get; set; }
    public string ProgramFilter { get; set; } = string.Empty;
    public string CategoryFilter { get; set; } = string.Empty;
    public string BrowserDomainFilter { get; set; } = string.Empty;
    public string WindowTitleFilter { get; set; } = string.Empty;
}
