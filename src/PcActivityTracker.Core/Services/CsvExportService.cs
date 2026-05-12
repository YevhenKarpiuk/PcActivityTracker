using System.Text;
using PcActivityTracker.Core.Interfaces;

namespace PcActivityTracker.Core.Services;

public sealed class CsvExportService
{
    private readonly IActivityRepository _repository;

    public CsvExportService(IActivityRepository repository)
    {
        _repository = repository;
    }

    public async Task ExportAsync(DateTime startInclusive, DateTime endExclusive, string filePath, CancellationToken cancellationToken = default)
    {
        var records = ActivityRecordPeriodHelper.ClipToPeriod(
                await _repository.GetRecordsAsync(startInclusive, endExclusive, cancellationToken).ConfigureAwait(false),
                startInclusive,
                endExclusive)
            .ToList();
        Directory.CreateDirectory(Path.GetDirectoryName(filePath)!);

        await using var stream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.Read);
        await using var writer = new StreamWriter(stream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: true));

        await writer.WriteLineAsync("StartTime;EndTime;DurationSeconds;ProcessName;WindowTitle;BrowserDomain;BrowserUrl;Category;IsIdle;ExePath").ConfigureAwait(false);

        foreach (var r in records.OrderBy(x => x.StartTime))
        {
            var line = string.Join(';', new[]
            {
                Escape(r.StartTime.ToString("yyyy-MM-dd HH:mm:ss")),
                Escape(r.EndTime.ToString("yyyy-MM-dd HH:mm:ss")),
                Escape(r.DurationSeconds.ToString()),
                Escape(r.ProcessName),
                Escape(r.WindowTitle),
                Escape(r.BrowserDomain),
                Escape(r.BrowserUrl),
                Escape(r.Category),
                Escape(r.IsIdle ? "1" : "0"),
                Escape(r.ExePath)
            });

            await writer.WriteLineAsync(line).ConfigureAwait(false);
        }
    }

    private static string Escape(string? value)
    {
        value ??= string.Empty;
        var mustQuote = value.Contains(';') || value.Contains('"') || value.Contains('\n') || value.Contains('\r');
        value = value.Replace("\"", "\"\"");
        return mustQuote ? $"\"{value}\"" : value;
    }
}
