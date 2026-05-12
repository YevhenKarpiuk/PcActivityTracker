using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Services;

public sealed class JsonExportService
{
    private readonly IActivityRepository _repository;

    public JsonExportService(IActivityRepository repository)
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

        var orderedRecords = records
            .OrderBy(x => x.StartTime)
            .Select(ActivityRecordExportDto.FromRecord)
            .ToList();

        var payload = new ActivityExportPayload
        {
            ExportedAt = DateTime.Now,
            PeriodStart = startInclusive,
            PeriodEndExclusive = endExclusive,
            TotalRecords = orderedRecords.Count,
            ActiveSeconds = orderedRecords.Where(x => !x.IsIdle).Sum(x => x.DurationSeconds),
            IdleSeconds = orderedRecords.Where(x => x.IsIdle).Sum(x => x.DurationSeconds),
            Records = orderedRecords
        };

        var options = new JsonSerializerOptions
        {
            WriteIndented = true,
            Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping
        };

        await using var stream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.Read);
        await JsonSerializer.SerializeAsync(stream, payload, options, cancellationToken).ConfigureAwait(false);
        await stream.WriteAsync(Encoding.UTF8.GetBytes(Environment.NewLine), cancellationToken).ConfigureAwait(false);
    }

    private sealed class ActivityExportPayload
    {
        public DateTime ExportedAt { get; set; }
        public DateTime PeriodStart { get; set; }
        public DateTime PeriodEndExclusive { get; set; }
        public int TotalRecords { get; set; }
        public int ActiveSeconds { get; set; }
        public int IdleSeconds { get; set; }
        public List<ActivityRecordExportDto> Records { get; set; } = new();
    }

    private sealed class ActivityRecordExportDto
    {
        public long Id { get; set; }
        public DateTime StartTime { get; set; }
        public DateTime EndTime { get; set; }
        public int DurationSeconds { get; set; }
        public string ProcessName { get; set; } = string.Empty;
        public string WindowTitle { get; set; } = string.Empty;
        public string BrowserDomain { get; set; } = string.Empty;
        public string BrowserUrl { get; set; } = string.Empty;
        public string ExePath { get; set; } = string.Empty;
        public bool IsIdle { get; set; }
        public string Status { get; set; } = string.Empty;
        public string Category { get; set; } = string.Empty;

        public static ActivityRecordExportDto FromRecord(ActivityRecord record)
        {
            return new ActivityRecordExportDto
            {
                Id = record.Id,
                StartTime = record.StartTime,
                EndTime = record.EndTime,
                DurationSeconds = record.DurationSeconds,
                ProcessName = record.ProcessName,
                WindowTitle = record.WindowTitle,
                BrowserDomain = record.BrowserDomain,
                BrowserUrl = record.BrowserUrl,
                ExePath = record.ExePath,
                IsIdle = record.IsIdle,
                Status = record.Status,
                Category = record.Category
            };
        }
    }
}
