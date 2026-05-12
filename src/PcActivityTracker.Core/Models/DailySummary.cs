namespace PcActivityTracker.Core.Models;

public sealed class DailySummary
{
    public DateTime Date { get; set; }
    public int ActiveSeconds { get; set; }
    public int IdleSeconds { get; set; }
    public string MostUsedProgram { get; set; } = "—";
    public List<ActivityRecord> Records { get; set; } = new();
    public List<UsageItem> Programs { get; set; } = new();
    public List<UsageItem> Categories { get; set; } = new();
}
