using System.Globalization;
using Microsoft.Data.Sqlite;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Data;

public sealed class ActivityRepository : IActivityRepository
{
    private readonly string _databasePath;

    public ActivityRepository(string databasePath)
    {
        _databasePath = databasePath;
    }

    public async Task InsertAsync(ActivityRecord record, CancellationToken cancellationToken = default)
    {
        await using var connection = CreateConnection();
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO activity_records
            (start_time, end_time, duration_seconds, process_name, window_title, exe_path, browser_url, browser_domain, is_idle, category)
            VALUES
            ($start_time, $end_time, $duration_seconds, $process_name, $window_title, $exe_path, $browser_url, $browser_domain, $is_idle, $category);
            """;

        command.Parameters.AddWithValue("$start_time", record.StartTime.ToString("O"));
        command.Parameters.AddWithValue("$end_time", record.EndTime.ToString("O"));
        command.Parameters.AddWithValue("$duration_seconds", record.DurationSeconds);
        command.Parameters.AddWithValue("$process_name", record.ProcessName);
        command.Parameters.AddWithValue("$window_title", record.WindowTitle);
        command.Parameters.AddWithValue("$exe_path", record.ExePath);
        command.Parameters.AddWithValue("$browser_url", record.BrowserUrl);
        command.Parameters.AddWithValue("$browser_domain", record.BrowserDomain);
        command.Parameters.AddWithValue("$is_idle", record.IsIdle ? 1 : 0);
        command.Parameters.AddWithValue("$category", record.Category);

        await command.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }

    public async Task<IReadOnlyList<ActivityRecord>> GetRecordsAsync(DateTime startInclusive, DateTime endExclusive, CancellationToken cancellationToken = default)
    {
        var result = new List<ActivityRecord>();

        await using var connection = CreateConnection();
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = """
            SELECT id, start_time, end_time, duration_seconds, process_name, window_title, exe_path, browser_url, browser_domain, is_idle, category
            FROM activity_records
            WHERE end_time > $start_time AND start_time < $end_time
            ORDER BY start_time DESC;
            """;

        command.Parameters.AddWithValue("$start_time", startInclusive.ToString("O"));
        command.Parameters.AddWithValue("$end_time", endExclusive.ToString("O"));

        await using var reader = await command.ExecuteReaderAsync(cancellationToken).ConfigureAwait(false);
        while (await reader.ReadAsync(cancellationToken).ConfigureAwait(false))
        {
            result.Add(new ActivityRecord
            {
                Id = reader.GetInt64(0),
                StartTime = DateTime.Parse(reader.GetString(1), CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind),
                EndTime = DateTime.Parse(reader.GetString(2), CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind),
                DurationSeconds = reader.GetInt32(3),
                ProcessName = reader.IsDBNull(4) ? string.Empty : reader.GetString(4),
                WindowTitle = reader.IsDBNull(5) ? string.Empty : reader.GetString(5),
                ExePath = reader.IsDBNull(6) ? string.Empty : reader.GetString(6),
                BrowserUrl = reader.IsDBNull(7) ? string.Empty : reader.GetString(7),
                BrowserDomain = reader.IsDBNull(8) ? string.Empty : reader.GetString(8),
                IsIdle = reader.GetInt32(9) == 1,
                Category = reader.IsDBNull(10) ? "Без категории" : reader.GetString(10)
            });
        }

        return result;
    }

    public async Task DeleteAllAsync(CancellationToken cancellationToken = default)
    {
        await using var connection = CreateConnection();
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = "DELETE FROM activity_records;";
        await command.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }

    public async Task DeleteOlderThanAsync(DateTime threshold, CancellationToken cancellationToken = default)
    {
        await using var connection = CreateConnection();
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = "DELETE FROM activity_records WHERE start_time < $threshold;";
        command.Parameters.AddWithValue("$threshold", threshold.ToString("O"));
        await command.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }

    public async Task<int> DeleteByFilterAsync(
        DateTime startInclusive,
        DateTime endExclusive,
        string? processNameFilter,
        string? categoryFilter,
        string? windowTitleFilter,
        CancellationToken cancellationToken = default)
    {
        await using var connection = CreateConnection();
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = """
            DELETE FROM activity_records
            WHERE end_time > $start_time
              AND start_time < $end_time
              AND ($process_name = '' OR instr(lower(coalesce(process_name, '')), lower($process_name)) > 0)
              AND ($category = '' OR instr(lower(coalesce(category, '')), lower($category)) > 0)
              AND ($window_title = '' OR instr(lower(coalesce(window_title, '')), lower($window_title)) > 0);
            """;

        command.Parameters.AddWithValue("$start_time", startInclusive.ToString("O"));
        command.Parameters.AddWithValue("$end_time", endExclusive.ToString("O"));
        command.Parameters.AddWithValue("$process_name", NormalizeFilter(processNameFilter));
        command.Parameters.AddWithValue("$category", NormalizeFilter(categoryFilter));
        command.Parameters.AddWithValue("$window_title", NormalizeFilter(windowTitleFilter));

        return await command.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }

    private static string NormalizeFilter(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? string.Empty : value.Trim();
    }

    private SqliteConnection CreateConnection()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_databasePath)!);
        return new SqliteConnection($"Data Source={_databasePath}");
    }
}
