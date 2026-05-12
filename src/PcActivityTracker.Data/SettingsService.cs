using System.Text.Json;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;
using PcActivityTracker.Core.Services;

namespace PcActivityTracker.Data;

public sealed class SettingsService : ISettingsService
{
    private readonly SemaphoreSlim _sync = new(1, 1);
    private readonly string _settingsPath;
    private readonly JsonSerializerOptions _jsonOptions = new() { WriteIndented = true };
    private AppSettings? _cached;

    public SettingsService()
    {
        var appDataDirectory = AppStoragePaths.GetAppDataDirectory();
        Directory.CreateDirectory(appDataDirectory);
        _settingsPath = Path.Combine(appDataDirectory, "settings.json");
    }

    public async Task<AppSettings> GetSettingsAsync(CancellationToken cancellationToken = default)
    {
        await _sync.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            if (_cached is not null)
                return Clone(_cached);

            if (!File.Exists(_settingsPath))
            {
                _cached = new AppSettings();
                Normalize(_cached);
                await WriteSettingsAsync(_cached, cancellationToken).ConfigureAwait(false);
                return Clone(_cached);
            }

            try
            {
                await using var stream = File.OpenRead(_settingsPath);
                _cached = await JsonSerializer.DeserializeAsync<AppSettings>(stream, cancellationToken: cancellationToken).ConfigureAwait(false) ?? new AppSettings();
                Normalize(_cached);
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch
            {
                BackupCorruptSettings();
                _cached = new AppSettings();
                Normalize(_cached);
                await WriteSettingsAsync(_cached, cancellationToken).ConfigureAwait(false);
            }

            return Clone(_cached);
        }
        finally
        {
            _sync.Release();
        }
    }

    public async Task SaveSettingsAsync(AppSettings settings, CancellationToken cancellationToken = default)
    {
        await _sync.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            Normalize(settings);
            var copy = Clone(settings);
            await WriteSettingsAsync(copy, cancellationToken).ConfigureAwait(false);
            _cached = copy;
        }
        finally
        {
            _sync.Release();
        }
    }

    private async Task WriteSettingsAsync(AppSettings settings, CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);

        var tempPath = _settingsPath + ".tmp";
        await using (var stream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None))
            await JsonSerializer.SerializeAsync(stream, settings, _jsonOptions, cancellationToken).ConfigureAwait(false);

        File.Move(tempPath, _settingsPath, overwrite: true);
    }

    private void BackupCorruptSettings()
    {
        try
        {
            if (!File.Exists(_settingsPath))
                return;

            var backupPath = $"{_settingsPath}.corrupt.{DateTime.Now:yyyyMMddHHmmss}";
            File.Copy(_settingsPath, backupPath, overwrite: true);
        }
        catch
        {
        }
    }

    private static void Normalize(AppSettings settings)
    {
        settings.IdleThresholdSeconds = Math.Clamp(settings.IdleThresholdSeconds, 10, 86400);
        settings.PollingIntervalSeconds = Math.Clamp(settings.PollingIntervalSeconds, 1, 60);
        settings.ExcludedProcesses ??= new List<string>();
        settings.ExcludedWindowTitles ??= new List<string>();
        settings.ExcludedWindowTitlePatterns ??= new List<string>();
        settings.ExcludedBrowserDomains ??= new List<string>();
        settings.Categories ??= new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        settings.ProgramChartSettings ??= new ProgramChartSettings();

        settings.ExcludedProcesses = settings.ExcludedProcesses
            .Where(x => !string.IsNullOrWhiteSpace(x))
            .Select(x => x.Trim().EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? x.Trim() : x.Trim() + ".exe")
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase)
            .ToList();

        settings.ExcludedWindowTitles = NormalizeTextList(settings.ExcludedWindowTitles);
        settings.ExcludedWindowTitlePatterns = NormalizeTextList(settings.ExcludedWindowTitlePatterns);

        settings.ExcludedBrowserDomains = settings.ExcludedBrowserDomains
            .Where(x => !string.IsNullOrWhiteSpace(x))
            .Select(TrackingExclusionMatcher.NormalizeDomain)
            .Where(x => !string.IsNullOrWhiteSpace(x))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase)
            .ToList();

        settings.Categories = settings.Categories
            .Where(x => !string.IsNullOrWhiteSpace(x.Key) && !string.IsNullOrWhiteSpace(x.Value))
            .ToDictionary(
                x => x.Key.Trim().EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? x.Key.Trim() : x.Key.Trim() + ".exe",
                x => x.Value.Trim(),
                StringComparer.OrdinalIgnoreCase);

        NormalizeProgramChartSettings(settings.ProgramChartSettings);
    }

    private static void NormalizeProgramChartSettings(ProgramChartSettings settings)
    {
        settings.TopItems = Math.Clamp(settings.TopItems, 1, 50);
        settings.GroupBy = IsSupportedProgramChartGroupBy(settings.GroupBy)
            ? settings.GroupBy
            : ProgramChartSettings.GroupByProcessName;

        if (settings.StartDate.HasValue)
            settings.StartDate = settings.StartDate.Value.Date;

        if (settings.EndDate.HasValue)
            settings.EndDate = settings.EndDate.Value.Date;

        if (settings.StartDate.HasValue && settings.EndDate.HasValue && settings.EndDate.Value < settings.StartDate.Value)
            settings.EndDate = settings.StartDate;

        settings.ProgramFilter = NormalizeText(settings.ProgramFilter);
        settings.CategoryFilter = NormalizeText(settings.CategoryFilter);
        settings.BrowserDomainFilter = NormalizeText(settings.BrowserDomainFilter);
        settings.WindowTitleFilter = NormalizeText(settings.WindowTitleFilter);
    }

    private static bool IsSupportedProgramChartGroupBy(string? value)
    {
        return value is ProgramChartSettings.GroupByProcessName
            or ProgramChartSettings.GroupByCategory
            or ProgramChartSettings.GroupByBrowserDomain
            or ProgramChartSettings.GroupByWindowTitle
            or ProgramChartSettings.GroupByStatus
            or ProgramChartSettings.GroupByDay;
    }

    private static string NormalizeText(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? string.Empty : value.Trim();
    }

    private static List<string> NormalizeTextList(IEnumerable<string> values)
    {
        return values
            .Where(x => !string.IsNullOrWhiteSpace(x))
            .Select(x => x.Trim())
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(x => x, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private static AppSettings Clone(AppSettings settings)
    {
        return new AppSettings
        {
            IdleThresholdSeconds = settings.IdleThresholdSeconds,
            PollingIntervalSeconds = settings.PollingIntervalSeconds,
            HideBrowserWindowTitles = settings.HideBrowserWindowTitles,
            StartWithWindows = settings.StartWithWindows,
            ExcludedProcesses = settings.ExcludedProcesses.ToList(),
            ExcludedWindowTitles = settings.ExcludedWindowTitles.ToList(),
            ExcludedWindowTitlePatterns = settings.ExcludedWindowTitlePatterns.ToList(),
            ExcludedBrowserDomains = settings.ExcludedBrowserDomains.ToList(),
            Categories = settings.Categories.ToDictionary(x => x.Key, x => x.Value, StringComparer.OrdinalIgnoreCase),
            ProgramChartSettings = new ProgramChartSettings
            {
                StartDate = settings.ProgramChartSettings.StartDate,
                EndDate = settings.ProgramChartSettings.EndDate,
                GroupBy = settings.ProgramChartSettings.GroupBy,
                TopItems = settings.ProgramChartSettings.TopItems,
                IncludeIdleRecords = settings.ProgramChartSettings.IncludeIdleRecords,
                ProgramFilter = settings.ProgramChartSettings.ProgramFilter,
                CategoryFilter = settings.ProgramChartSettings.CategoryFilter,
                BrowserDomainFilter = settings.ProgramChartSettings.BrowserDomainFilter,
                WindowTitleFilter = settings.ProgramChartSettings.WindowTitleFilter
            }
        };
    }
}
