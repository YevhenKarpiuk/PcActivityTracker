using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Services;

public sealed class ActivityMonitorService
{
    private readonly IActiveWindowProvider _activeWindowProvider;
    private readonly IIdleTimeProvider _idleTimeProvider;
    private readonly IBrowserUrlProvider? _browserUrlProvider;
    private readonly IActivityRepository _repository;
    private readonly ISettingsService _settingsService;
    private readonly object _sync = new();

    private CancellationTokenSource? _cts;
    private Task? _monitorTask;
    private ActivitySnapshot? _currentSnapshot;
    private string _currentCategory = CategoryResolver.DefaultCategory;
    private DateTime _currentStart;

    public event EventHandler<ActivityRecord>? ActivityRecordClosed;

    public bool IsRunning { get; private set; }
    public bool IsPaused { get; private set; }

    public ActivityMonitorService(
        IActiveWindowProvider activeWindowProvider,
        IIdleTimeProvider idleTimeProvider,
        IActivityRepository repository,
        ISettingsService settingsService,
        IBrowserUrlProvider? browserUrlProvider = null)
    {
        _activeWindowProvider = activeWindowProvider;
        _idleTimeProvider = idleTimeProvider;
        _repository = repository;
        _settingsService = settingsService;
        _browserUrlProvider = browserUrlProvider;
    }

    public Task StartAsync()
    {
        lock (_sync)
        {
            if (IsRunning)
                return Task.CompletedTask;

            _cts = new CancellationTokenSource();
            IsRunning = true;
            _monitorTask = Task.Run(() => MonitorLoopAsync(_cts.Token));
        }

        return Task.CompletedTask;
    }

    public async Task StopAsync()
    {
        CancellationTokenSource? cts;
        Task? task;

        lock (_sync)
        {
            if (!IsRunning)
                return;

            cts = _cts;
            task = _monitorTask;
            IsRunning = false;
            _cts = null;
            _monitorTask = null;
        }

        if (cts is not null)
        {
            cts.Cancel();
            try
            {
                if (task is not null)
                    await task.ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
            }
            finally
            {
                cts.Dispose();
            }
        }

        await CloseCurrentRecordAsync(DateTime.Now, CancellationToken.None).ConfigureAwait(false);
    }

    public void TogglePause()
    {
        IsPaused = !IsPaused;
    }

    public void SetPaused(bool paused)
    {
        IsPaused = paused;
    }

    public ActivityRecord? GetCurrentRecord(DateTime? endTime = null)
    {
        lock (_sync)
        {
            if (_currentSnapshot is null)
                return null;

            return CreateRecord(_currentSnapshot, _currentCategory, _currentStart, endTime ?? DateTime.Now);
        }
    }

    private async Task MonitorLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var delaySeconds = 2;

            try
            {
                var settings = await _settingsService.GetSettingsAsync(cancellationToken).ConfigureAwait(false);
                delaySeconds = Math.Clamp(settings.PollingIntervalSeconds, 1, 60);
                await ProcessSnapshotAsync(settings, cancellationToken).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                throw;
            }
            catch
            {
                // Monitoring must not crash the application.
            }

            await Task.Delay(TimeSpan.FromSeconds(delaySeconds), cancellationToken).ConfigureAwait(false);
        }
    }

    private async Task ProcessSnapshotAsync(AppSettings settings, CancellationToken cancellationToken)
    {
        if (IsPaused)
        {
            await CloseCurrentRecordAsync(DateTime.Now, cancellationToken).ConfigureAwait(false);
            return;
        }

        var snapshot = _activeWindowProvider.GetCurrentWindow();
        snapshot.Timestamp = DateTime.Now;
        snapshot.IdleSeconds = _idleTimeProvider.GetIdleSeconds();
        snapshot.IsIdle = snapshot.IdleSeconds >= Math.Max(10, settings.IdleThresholdSeconds);
        snapshot.ProcessName = CategoryResolver.NormalizeProcessName(snapshot.ProcessName);

        if (TrackingExclusionMatcher.IsBrowserProcess(snapshot.ProcessName) && _browserUrlProvider is not null)
        {
            var browserUrl = _browserUrlProvider.TryGetBrowserUrl(snapshot);
            if (browserUrl is not null)
            {
                snapshot.BrowserUrl = browserUrl.Url;
                snapshot.BrowserDomain = browserUrl.Domain;
            }
        }

        // Exclusions are checked before optional title hiding, so rules can still match the real window title.
        if (TrackingExclusionMatcher.IsExcluded(snapshot, settings))
        {
            await CloseCurrentRecordAsync(snapshot.Timestamp, cancellationToken).ConfigureAwait(false);
            return;
        }

        if (settings.HideBrowserWindowTitles && TrackingExclusionMatcher.IsBrowserProcess(snapshot.ProcessName))
        {
            snapshot.WindowTitle = string.IsNullOrWhiteSpace(snapshot.BrowserDomain) ? "Браузер" : $"Браузер: {snapshot.BrowserDomain}";
            snapshot.BrowserUrl = string.Empty;
        }

        var category = CategoryResolver.Resolve(snapshot.ProcessName, settings.Categories);

        if (_currentSnapshot is null)
        {
            StartNewRecord(snapshot, category);
            return;
        }

        if (IsDifferent(_currentSnapshot, snapshot, _currentCategory, category))
        {
            await CloseCurrentRecordAsync(snapshot.Timestamp, cancellationToken).ConfigureAwait(false);
            StartNewRecord(snapshot, category);
        }
    }

    private void StartNewRecord(ActivitySnapshot snapshot, string category)
    {
        lock (_sync)
        {
            _currentSnapshot = snapshot;
            _currentCategory = category;
            _currentStart = snapshot.Timestamp;
        }
    }

    private async Task CloseCurrentRecordAsync(DateTime endTime, CancellationToken cancellationToken)
    {
        ActivityRecord record;

        lock (_sync)
        {
            var snapshot = _currentSnapshot;
            if (snapshot is null)
                return;

            record = CreateRecord(snapshot, _currentCategory, _currentStart, endTime);

            _currentSnapshot = null;
            _currentCategory = CategoryResolver.DefaultCategory;
            _currentStart = default;
        }

        await _repository.InsertAsync(record, cancellationToken).ConfigureAwait(false);
        ActivityRecordClosed?.Invoke(this, record);
    }

    private static ActivityRecord CreateRecord(ActivitySnapshot snapshot, string category, DateTime startTime, DateTime endTime)
    {
        if (endTime <= startTime)
            endTime = startTime.AddSeconds(1);

        var duration = Math.Max(1, (int)Math.Round((endTime - startTime).TotalSeconds));

        return new ActivityRecord
        {
            StartTime = startTime,
            EndTime = endTime,
            DurationSeconds = duration,
            ProcessName = snapshot.ProcessName,
            WindowTitle = snapshot.WindowTitle,
            ExePath = snapshot.ExePath,
            BrowserUrl = snapshot.BrowserUrl,
            BrowserDomain = snapshot.BrowserDomain,
            IsIdle = snapshot.IsIdle,
            Category = category
        };
    }

    private static bool IsDifferent(ActivitySnapshot oldSnapshot, ActivitySnapshot newSnapshot, string oldCategory, string newCategory)
    {
        return !string.Equals(oldSnapshot.ProcessName, newSnapshot.ProcessName, StringComparison.OrdinalIgnoreCase)
            || !string.Equals(oldSnapshot.WindowTitle, newSnapshot.WindowTitle, StringComparison.Ordinal)
            || !string.Equals(oldSnapshot.BrowserDomain, newSnapshot.BrowserDomain, StringComparison.OrdinalIgnoreCase)
            || oldSnapshot.IsIdle != newSnapshot.IsIdle
            || !string.Equals(oldCategory, newCategory, StringComparison.OrdinalIgnoreCase);
    }
}
