namespace PcActivityTracker.Core.Models;

public sealed class UsageItem
{
    public string Name { get; set; } = string.Empty;
    public int DurationSeconds { get; set; }
    public double Percent { get; set; }
    public string DurationFormatted => ActivityRecord.FormatDuration(DurationSeconds);
    public string PercentFormatted => $"{Percent:0.0}%";
}
