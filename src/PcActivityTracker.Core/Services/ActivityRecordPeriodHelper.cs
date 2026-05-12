using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Services;

public static class ActivityRecordPeriodHelper
{
    public static IEnumerable<ActivityRecord> ClipToPeriod(IEnumerable<ActivityRecord> records, DateTime startInclusive, DateTime endExclusive)
    {
        foreach (var record in records)
        {
            var clipped = ClipToPeriod(record, startInclusive, endExclusive);
            if (clipped is not null)
                yield return clipped;
        }
    }

    public static ActivityRecord? ClipToPeriod(ActivityRecord record, DateTime startInclusive, DateTime endExclusive)
    {
        var start = record.StartTime > startInclusive ? record.StartTime : startInclusive;
        var end = record.EndTime < endExclusive ? record.EndTime : endExclusive;

        if (end <= start)
            return null;

        return new ActivityRecord
        {
            Id = record.Id,
            StartTime = start,
            EndTime = end,
            DurationSeconds = Math.Max(1, (int)Math.Round((end - start).TotalSeconds)),
            ProcessName = record.ProcessName,
            WindowTitle = record.WindowTitle,
            ExePath = record.ExePath,
            BrowserUrl = record.BrowserUrl,
            BrowserDomain = record.BrowserDomain,
            IsIdle = record.IsIdle,
            Category = record.Category
        };
    }
}
