using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Services;

public sealed class ReportService
{
    private readonly IActivityRepository _repository;

    public ReportService(IActivityRepository repository)
    {
        _repository = repository;
    }

    public async Task<DailySummary> GetDailySummaryAsync(DateTime date, CancellationToken cancellationToken = default)
    {
        var start = date.Date;
        var end = start.AddDays(1);
        var records = await _repository.GetRecordsAsync(start, end, cancellationToken).ConfigureAwait(false);
        return BuildDailySummary(date, records);
    }

    public async Task<IReadOnlyList<ActivityRecord>> GetRecordsAsync(DateTime startInclusive, DateTime endExclusive, CancellationToken cancellationToken = default)
    {
        var records = await _repository.GetRecordsAsync(startInclusive, endExclusive, cancellationToken).ConfigureAwait(false);
        return ActivityRecordPeriodHelper.ClipToPeriod(records, startInclusive, endExclusive)
            .OrderByDescending(x => x.StartTime)
            .ToList();
    }

    public static DailySummary BuildDailySummary(DateTime date, IEnumerable<ActivityRecord> records)
    {
        var start = date.Date;
        var end = start.AddDays(1);
        var periodRecords = ActivityRecordPeriodHelper.ClipToPeriod(records, start, end)
            .OrderByDescending(x => x.StartTime)
            .ToList();

        var activeSeconds = periodRecords.Where(x => !x.IsIdle).Sum(x => x.DurationSeconds);
        var idleSeconds = periodRecords.Where(x => x.IsIdle).Sum(x => x.DurationSeconds);
        var programs = BuildUsage(periodRecords.Where(x => !x.IsIdle), x => x.ProcessName).ToList();
        var categories = BuildUsage(periodRecords.Where(x => !x.IsIdle), x => x.Category).ToList();

        return new DailySummary
        {
            Date = start,
            ActiveSeconds = activeSeconds,
            IdleSeconds = idleSeconds,
            MostUsedProgram = programs.FirstOrDefault()?.Name ?? "вЂ”",
            Records = periodRecords,
            Programs = programs,
            Categories = categories
        };
    }

    private static IEnumerable<UsageItem> BuildUsage(IEnumerable<ActivityRecord> records, Func<ActivityRecord, string> keySelector)
    {
        var grouped = records
            .GroupBy(x => string.IsNullOrWhiteSpace(keySelector(x)) ? "вЂ”" : keySelector(x))
            .Select(g => new UsageItem { Name = g.Key, DurationSeconds = g.Sum(x => x.DurationSeconds) })
            .OrderByDescending(x => x.DurationSeconds)
            .ToList();

        var total = grouped.Sum(x => x.DurationSeconds);
        foreach (var item in grouped)
            item.Percent = total <= 0 ? 0 : (double)item.DurationSeconds / total * 100.0;

        return grouped;
    }
}
