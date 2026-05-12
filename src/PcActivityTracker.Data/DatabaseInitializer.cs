using Microsoft.Data.Sqlite;

namespace PcActivityTracker.Data;

public sealed class DatabaseInitializer
{
    public string AppDataDirectory { get; }
    public string DatabasePath { get; }

    public DatabaseInitializer()
    {
        AppDataDirectory = AppStoragePaths.GetAppDataDirectory();
        DatabasePath = Path.Combine(AppDataDirectory, "activity_tracker.db");
    }

    public async Task InitializeAsync(CancellationToken cancellationToken = default)
    {
        Directory.CreateDirectory(AppDataDirectory);

        await using var connection = new SqliteConnection($"Data Source={DatabasePath}");
        await connection.OpenAsync(cancellationToken).ConfigureAwait(false);

        var command = connection.CreateCommand();
        command.CommandText = """
            PRAGMA journal_mode=WAL;

            CREATE TABLE IF NOT EXISTS activity_records (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                start_time TEXT NOT NULL,
                end_time TEXT NOT NULL,
                duration_seconds INTEGER NOT NULL,
                process_name TEXT,
                window_title TEXT,
                exe_path TEXT,
                browser_url TEXT,
                browser_domain TEXT,
                is_idle INTEGER NOT NULL DEFAULT 0,
                category TEXT
            );

            CREATE TABLE IF NOT EXISTS app_categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                process_name TEXT NOT NULL UNIQUE,
                category TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS app_settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            );
            """;

        await command.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);

        await AddColumnIfMissingAsync(connection, "activity_records", "exe_path", "TEXT", cancellationToken).ConfigureAwait(false);
        await AddColumnIfMissingAsync(connection, "activity_records", "browser_domain", "TEXT", cancellationToken).ConfigureAwait(false);
        await AddColumnIfMissingAsync(connection, "activity_records", "browser_url", "TEXT", cancellationToken).ConfigureAwait(false);
        await AddColumnIfMissingAsync(connection, "activity_records", "is_idle", "INTEGER NOT NULL DEFAULT 0", cancellationToken).ConfigureAwait(false);
        await AddColumnIfMissingAsync(connection, "activity_records", "category", "TEXT", cancellationToken).ConfigureAwait(false);

        var indexCommand = connection.CreateCommand();
        indexCommand.CommandText = """
            CREATE INDEX IF NOT EXISTS idx_activity_records_start_time ON activity_records(start_time);
            CREATE INDEX IF NOT EXISTS idx_activity_records_end_time ON activity_records(end_time);
            CREATE INDEX IF NOT EXISTS idx_activity_records_process_name ON activity_records(process_name);
            CREATE INDEX IF NOT EXISTS idx_activity_records_category ON activity_records(category);
            CREATE INDEX IF NOT EXISTS idx_activity_records_browser_domain ON activity_records(browser_domain);
            """;
        await indexCommand.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }

    private static async Task AddColumnIfMissingAsync(SqliteConnection connection, string tableName, string columnName, string columnDefinition, CancellationToken cancellationToken)
    {
        var existingColumns = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        var readCommand = connection.CreateCommand();
        readCommand.CommandText = $"PRAGMA table_info({tableName});";

        await using (var reader = await readCommand.ExecuteReaderAsync(cancellationToken).ConfigureAwait(false))
        {
            while (await reader.ReadAsync(cancellationToken).ConfigureAwait(false))
                existingColumns.Add(reader.GetString(1));
        }

        if (existingColumns.Contains(columnName))
            return;

        var alterCommand = connection.CreateCommand();
        alterCommand.CommandText = $"ALTER TABLE {tableName} ADD COLUMN {columnName} {columnDefinition};";
        await alterCommand.ExecuteNonQueryAsync(cancellationToken).ConfigureAwait(false);
    }
}
