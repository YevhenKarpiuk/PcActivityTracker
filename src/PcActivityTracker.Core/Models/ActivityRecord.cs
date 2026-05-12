namespace PcActivityTracker.Core.Models;

public sealed class ActivityRecord
{
    public long Id { get; set; }
    public DateTime StartTime { get; set; }
    public DateTime EndTime { get; set; }
    public int DurationSeconds { get; set; }
    public string ProcessName { get; set; } = string.Empty;
    public string WindowTitle { get; set; } = string.Empty;
    public string ExePath { get; set; } = string.Empty;
    public string BrowserUrl { get; set; } = string.Empty;
    public string BrowserDomain { get; set; } = string.Empty;
    public bool IsIdle { get; set; }
    public string Category { get; set; } = "Без категории";

    public string Status => IsIdle ? "Idle" : "Active";
    public string DurationFormatted => FormatDuration(DurationSeconds);

    public static string FormatDuration(int seconds)
    {
        if (seconds < 0) seconds = 0;
        var ts = TimeSpan.FromSeconds(seconds);
        return ts.TotalDays >= 1
            ? $"{(int)ts.TotalDays} д {ts:hh\\:mm\\:ss}"
            : ts.ToString(@"hh\:mm\:ss");
    }
}
