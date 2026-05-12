using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Interfaces;

public interface IActivityRepository
{
    Task InsertAsync(ActivityRecord record, CancellationToken cancellationToken = default);
    Task<IReadOnlyList<ActivityRecord>> GetRecordsAsync(DateTime startInclusive, DateTime endExclusive, CancellationToken cancellationToken = default);
    Task DeleteAllAsync(CancellationToken cancellationToken = default);
    Task DeleteOlderThanAsync(DateTime threshold, CancellationToken cancellationToken = default);
    Task<int> DeleteByFilterAsync(
        DateTime startInclusive,
        DateTime endExclusive,
        string? processNameFilter,
        string? categoryFilter,
        string? windowTitleFilter,
        CancellationToken cancellationToken = default);
}
