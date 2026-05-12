using System.Collections.ObjectModel;
using System.IO;
using System.Windows;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;
using Microsoft.Win32;
using PcActivityTracker.App.ViewModels;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;
using PcActivityTracker.Core.Services;
using WpfApplication = System.Windows.Application;
using WpfMessageBox = System.Windows.MessageBox;
using WpfSaveFileDialog = Microsoft.Win32.SaveFileDialog;

namespace PcActivityTracker.App;

public partial class MainWindow : Window
{
    private readonly ActivityMonitorService _monitorService;
    private readonly ReportService _reportService;
    private readonly IActivityRepository _repository;
    private readonly ISettingsService _settingsService;
    private readonly CsvExportService _csvExportService;
    private readonly JsonExportService _jsonExportService;
    private readonly TrayIconService _trayIconService;
    private readonly MainViewModel _viewModel = new();
    private bool _allowClose;

    public MainWindow(
        ActivityMonitorService monitorService,
        ReportService reportService,
        IActivityRepository repository,
        ISettingsService settingsService,
        CsvExportService csvExportService,
        JsonExportService jsonExportService,
        TrayIconService trayIconService)
    {
        InitializeComponent();

        _monitorService = monitorService;
        _reportService = reportService;
        _repository = repository;
        _settingsService = settingsService;
        _csvExportService = csvExportService;
        _jsonExportService = jsonExportService;
        _trayIconService = trayIconService;

        DataContext = _viewModel;

        ReportStartDatePicker.SelectedDate = DateTime.Today;
        ReportEndDatePicker.SelectedDate = DateTime.Today;
        HistoryStartDatePicker.SelectedDate = DateTime.Today;
        HistoryEndDatePicker.SelectedDate = DateTime.Today;

        _trayIconService.OpenRequested += (_, _) => ShowFromTray();
        _trayIconService.TogglePauseRequested += (_, _) => TogglePause();
        _trayIconService.ExportTodayCsvRequested += async (_, _) => await ExportTodayCsvToDocumentsAsync();
        _trayIconService.ExportTodayJsonRequested += async (_, _) => await ExportTodayJsonToDocumentsAsync();
        _trayIconService.ExitRequested += async (_, _) => await ExitApplicationAsync();

        Loaded += async (_, _) =>
        {
            await LoadSettingsAsync();
            await LoadProgramChartSettingsAsync(showMessage: false);
            await RefreshTodayAsync();
            await LoadHistoryAsync();
            await RefreshProgramChartAsync();
        };
    }

    private async Task RefreshTodayAsync()
    {
        var (start, end) = GetReportPeriod();
        var records = await _reportService.GetRecordsAsync(start, end);
        var currentRecord = _monitorService.GetCurrentRecord();
        if (currentRecord is not null)
            records = ActivityRecordPeriodHelper.ClipToPeriod(records.Append(currentRecord), start, end)
                .OrderByDescending(x => x.StartTime)
                .ToList();

        var filteredRecords = FilterRecords(
                records,
                ReportProgramFilterTextBox.Text,
                ReportCategoryFilterTextBox.Text,
                ReportWindowFilterTextBox.Text)
            .OrderByDescending(x => x.StartTime)
            .ToList();

        var activeSeconds = filteredRecords.Where(x => !x.IsIdle).Sum(x => x.DurationSeconds);
        var idleSeconds = filteredRecords.Where(x => x.IsIdle).Sum(x => x.DurationSeconds);
        var programs = BuildUsage(filteredRecords.Where(x => !x.IsIdle), x => x.ProcessName).ToList();
        var categories = BuildUsage(filteredRecords.Where(x => !x.IsIdle), x => x.Category).ToList();

        _viewModel.ActiveTimeText = ActivityRecord.FormatDuration(activeSeconds);
        _viewModel.IdleTimeText = ActivityRecord.FormatDuration(idleSeconds);
        _viewModel.MostUsedProgramText = programs.FirstOrDefault()?.Name ?? "—";
        _viewModel.StatusText = _monitorService.IsPaused ? "Пауза" : "Активно";

        _viewModel.ReplaceTodayRecords(filteredRecords);
        _viewModel.ReplaceProgramUsage(programs);
        _viewModel.ReplaceCategoryUsage(categories);
        _viewModel.ProgramSeries = BuildPieSeries(programs);
        _viewModel.CategorySeries = BuildPieSeries(categories);
    }

    private (DateTime Start, DateTime EndExclusive) GetReportPeriod()
    {
        var start = ReportStartDatePicker.SelectedDate?.Date ?? DateTime.Today;
        var endDate = ReportEndDatePicker.SelectedDate?.Date ?? start;
        if (endDate < start)
            endDate = start;

        return (start, endDate.AddDays(1));
    }

    private (DateTime Start, DateTime EndExclusive) GetHistoryPeriod()
    {
        var start = HistoryStartDatePicker.SelectedDate?.Date ?? DateTime.Today;
        var endDate = HistoryEndDatePicker.SelectedDate?.Date ?? start;
        if (endDate < start)
            endDate = start;

        return (start, endDate.AddDays(1));
    }

    private static IEnumerable<ActivityRecord> FilterRecords(
        IEnumerable<ActivityRecord> records,
        string? programFilter,
        string? categoryFilter,
        string? windowFilter,
        string? domainFilter = null)
    {
        return records
            .Where(x => MatchesFilter(x.ProcessName, programFilter))
            .Where(x => MatchesFilter(x.Category, categoryFilter))
            .Where(x => MatchesFilter(x.WindowTitle, windowFilter))
            .Where(x => MatchesFilter(x.BrowserDomain, domainFilter));
    }

    private static bool MatchesFilter(string? value, string? filter)
    {
        return string.IsNullOrWhiteSpace(filter)
            || (value?.IndexOf(filter.Trim(), StringComparison.OrdinalIgnoreCase) ?? -1) >= 0;
    }

    private static IEnumerable<UsageItem> BuildUsage(IEnumerable<ActivityRecord> records, Func<ActivityRecord, string> keySelector)
    {
        var grouped = records
            .GroupBy(x => string.IsNullOrWhiteSpace(keySelector(x)) ? "—" : keySelector(x))
            .Select(g => new UsageItem { Name = g.Key, DurationSeconds = g.Sum(x => x.DurationSeconds) })
            .OrderByDescending(x => x.DurationSeconds)
            .ToList();

        var total = grouped.Sum(x => x.DurationSeconds);
        foreach (var item in grouped)
            item.Percent = total <= 0 ? 0 : (double)item.DurationSeconds / total * 100.0;

        return grouped;
    }

    private static ISeries[] BuildPieSeries(IEnumerable<UsageItem> items, int maxItems = 12)
    {
        var series = items
            .Where(x => x.DurationSeconds > 0)
            .Take(Math.Max(1, maxItems))
            .Select(x => new PieSeries<double>
            {
                Name = x.Name,
                Values = new ObservableCollection<double> { x.DurationSeconds }
            })
            .Cast<ISeries>()
            .ToArray();

        return series.Length == 0
            ? new ISeries[] { new PieSeries<double> { Name = "Нет данных", Values = new[] { 1.0 } } }
            : series;
    }

    private async Task RefreshProgramChartAsync()
    {
        var (start, end) = GetProgramChartPeriod();
        var records = await _reportService.GetRecordsAsync(start, end);
        var currentRecord = _monitorService.GetCurrentRecord();
        if (currentRecord is not null)
            records = ActivityRecordPeriodHelper.ClipToPeriod(records.Append(currentRecord), start, end)
                .OrderByDescending(x => x.StartTime)
                .ToList();

        var filteredRecords = FilterRecords(
            records,
            ProgramChartProgramFilterTextBox.Text,
            ProgramChartCategoryFilterTextBox.Text,
            ProgramChartWindowFilterTextBox.Text,
            ProgramChartDomainFilterTextBox.Text);

        if (ProgramChartIncludeIdleCheckBox.IsChecked != true)
            filteredRecords = filteredRecords.Where(x => !x.IsIdle);

        var groupBy = GetSelectedProgramChartGroupBy();
        var topItems = GetProgramChartTopItems();
        var items = BuildUsage(filteredRecords, GetProgramChartKeySelector(groupBy))
            .Take(topItems)
            .ToList();

        _viewModel.ProgramChartTitle = $"Топ {topItems}: {GetProgramChartGroupByLabel(groupBy)}";
        _viewModel.ReplaceProgramChartUsage(items);
        _viewModel.ProgramChartSeries = BuildPieSeries(items, topItems);
    }

    private (DateTime Start, DateTime EndExclusive) GetProgramChartPeriod()
    {
        var start = ProgramChartStartDatePicker.SelectedDate?.Date ?? DateTime.Today;
        var endDate = ProgramChartEndDatePicker.SelectedDate?.Date ?? start;
        if (endDate < start)
            endDate = start;

        return (start, endDate.AddDays(1));
    }

    private int GetProgramChartTopItems()
    {
        var normalizedTopItems = int.TryParse(ProgramChartTopItemsTextBox.Text, out var topItems)
            ? Math.Clamp(topItems, 1, 50)
            : 12;

        ProgramChartTopItemsTextBox.Text = normalizedTopItems.ToString();
        return normalizedTopItems;
    }

    private string GetSelectedProgramChartGroupBy()
    {
        return ProgramChartGroupByComboBox.SelectedValue as string ?? ProgramChartSettings.GroupByProcessName;
    }

    private static Func<ActivityRecord, string> GetProgramChartKeySelector(string groupBy)
    {
        return groupBy switch
        {
            ProgramChartSettings.GroupByCategory => x => x.Category,
            ProgramChartSettings.GroupByBrowserDomain => x => x.BrowserDomain,
            ProgramChartSettings.GroupByWindowTitle => x => x.WindowTitle,
            ProgramChartSettings.GroupByStatus => x => x.IsIdle ? "Простой" : "Активно",
            ProgramChartSettings.GroupByDay => x => x.StartTime.ToString("yyyy-MM-dd"),
            _ => x => x.ProcessName
        };
    }

    private static string GetProgramChartGroupByLabel(string groupBy)
    {
        return groupBy switch
        {
            ProgramChartSettings.GroupByCategory => "Категории",
            ProgramChartSettings.GroupByBrowserDomain => "Домены",
            ProgramChartSettings.GroupByWindowTitle => "Окна",
            ProgramChartSettings.GroupByStatus => "Статусы",
            ProgramChartSettings.GroupByDay => "Дни",
            _ => "Программы"
        };
    }

    private async Task LoadProgramChartSettingsAsync(bool showMessage)
    {
        var settings = await _settingsService.GetSettingsAsync();
        ApplyProgramChartSettings(settings.ProgramChartSettings);

        if (showMessage)
            WpfMessageBox.Show("Настройки графика загружены.", "PcActivityTracker", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private async Task SaveProgramChartSettingsAsync()
    {
        var settings = await _settingsService.GetSettingsAsync();
        settings.ProgramChartSettings = ReadProgramChartSettingsFromControls();

        await _settingsService.SaveSettingsAsync(settings);
        await LoadProgramChartSettingsAsync(showMessage: false);

        WpfMessageBox.Show("Настройки графика сохранены.", "PcActivityTracker", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void ApplyProgramChartSettings(ProgramChartSettings settings)
    {
        ProgramChartStartDatePicker.SelectedDate = settings.StartDate ?? DateTime.Today;
        ProgramChartEndDatePicker.SelectedDate = settings.EndDate ?? ProgramChartStartDatePicker.SelectedDate;
        ProgramChartTopItemsTextBox.Text = Math.Clamp(settings.TopItems, 1, 50).ToString();
        ProgramChartIncludeIdleCheckBox.IsChecked = settings.IncludeIdleRecords;
        ProgramChartProgramFilterTextBox.Text = settings.ProgramFilter;
        ProgramChartCategoryFilterTextBox.Text = settings.CategoryFilter;
        ProgramChartDomainFilterTextBox.Text = settings.BrowserDomainFilter;
        ProgramChartWindowFilterTextBox.Text = settings.WindowTitleFilter;

        ProgramChartGroupByComboBox.SelectedValue = settings.GroupBy;
        if (ProgramChartGroupByComboBox.SelectedValue as string != settings.GroupBy)
            ProgramChartGroupByComboBox.SelectedValue = ProgramChartSettings.GroupByProcessName;
    }

    private ProgramChartSettings ReadProgramChartSettingsFromControls()
    {
        var (start, end) = GetProgramChartPeriod();
        return new ProgramChartSettings
        {
            StartDate = start,
            EndDate = end.AddDays(-1),
            GroupBy = GetSelectedProgramChartGroupBy(),
            TopItems = GetProgramChartTopItems(),
            IncludeIdleRecords = ProgramChartIncludeIdleCheckBox.IsChecked == true,
            ProgramFilter = ProgramChartProgramFilterTextBox.Text,
            CategoryFilter = ProgramChartCategoryFilterTextBox.Text,
            BrowserDomainFilter = ProgramChartDomainFilterTextBox.Text,
            WindowTitleFilter = ProgramChartWindowFilterTextBox.Text
        };
    }

    private async Task LoadSettingsAsync()
    {
        var settings = await _settingsService.GetSettingsAsync();

        IdleThresholdTextBox.Text = settings.IdleThresholdSeconds.ToString();
        PollingIntervalTextBox.Text = settings.PollingIntervalSeconds.ToString();
        HideBrowserTitlesCheckBox.IsChecked = settings.HideBrowserWindowTitles;
        AutostartCheckBox.IsChecked = AutostartManager.IsEnabled();
        ExcludedProcessesTextBox.Text = string.Join(Environment.NewLine, settings.ExcludedProcesses.OrderBy(x => x));
        ExcludedWindowTitlesTextBox.Text = string.Join(Environment.NewLine, settings.ExcludedWindowTitles.OrderBy(x => x));
        ExcludedWindowTitlePatternsTextBox.Text = string.Join(Environment.NewLine, settings.ExcludedWindowTitlePatterns.OrderBy(x => x));
        ExcludedBrowserDomainsTextBox.Text = string.Join(Environment.NewLine, settings.ExcludedBrowserDomains.OrderBy(x => x));
        CategoriesTextBox.Text = string.Join(Environment.NewLine, settings.Categories.OrderBy(x => x.Key).Select(x => $"{x.Key}={x.Value}"));
    }

    private static List<string> ParseLines(string? text)
    {
        return (text ?? string.Empty)
            .Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .ToList();
    }

    private async Task SaveSettingsAsync()
    {
        var settings = await _settingsService.GetSettingsAsync();

        if (int.TryParse(IdleThresholdTextBox.Text, out var idleThreshold))
            settings.IdleThresholdSeconds = idleThreshold;

        if (int.TryParse(PollingIntervalTextBox.Text, out var pollingInterval))
            settings.PollingIntervalSeconds = pollingInterval;

        settings.HideBrowserWindowTitles = HideBrowserTitlesCheckBox.IsChecked == true;
        settings.StartWithWindows = AutostartCheckBox.IsChecked == true;

        settings.ExcludedProcesses = ParseLines(ExcludedProcessesTextBox.Text);
        settings.ExcludedWindowTitles = ParseLines(ExcludedWindowTitlesTextBox.Text);
        settings.ExcludedWindowTitlePatterns = ParseLines(ExcludedWindowTitlePatternsTextBox.Text);
        settings.ExcludedBrowserDomains = ParseLines(ExcludedBrowserDomainsTextBox.Text);

        settings.Categories = CategoriesTextBox.Text
            .Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Select(line => line.Split('=', 2, StringSplitOptions.TrimEntries))
            .Where(parts => parts.Length == 2 && !string.IsNullOrWhiteSpace(parts[0]) && !string.IsNullOrWhiteSpace(parts[1]))
            .ToDictionary(parts => parts[0], parts => parts[1], StringComparer.OrdinalIgnoreCase);

        await _settingsService.SaveSettingsAsync(settings);
        AutostartManager.SetEnabled(settings.StartWithWindows);
        await LoadSettingsAsync();

        WpfMessageBox.Show("Настройки сохранены.", "PcActivityTracker", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private async Task LoadHistoryAsync()
    {
        var (start, end) = GetHistoryPeriod();
        var records = await _reportService.GetRecordsAsync(start, end);
        var filteredRecords = FilterRecords(
                records,
                HistoryProgramFilterTextBox.Text,
                HistoryCategoryFilterTextBox.Text,
                HistoryWindowFilterTextBox.Text)
            .OrderByDescending(x => x.StartTime);

        _viewModel.ReplaceHistoryRecords(filteredRecords);
    }

    private void TogglePause()
    {
        _monitorService.TogglePause();
        PauseButton.Content = _monitorService.IsPaused ? "Продолжить" : "Пауза";
        _trayIconService.SetPaused(_monitorService.IsPaused);
        _viewModel.StatusText = _monitorService.IsPaused ? "Пауза" : "Активно";
    }

    private void ShowFromTray()
    {
        Show();
        WindowState = WindowState.Normal;
        Activate();
    }

    private async Task ExitApplicationAsync()
    {
        _allowClose = true;
        await _monitorService.StopAsync();
        _trayIconService.Dispose();
        WpfApplication.Current.Shutdown();
    }

    private async Task ExportTodayCsvToDocumentsAsync()
    {
        var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        var directory = Path.Combine(documents, "PcActivityTracker");
        Directory.CreateDirectory(directory);

        var path = Path.Combine(directory, $"activity_{DateTime.Today:yyyyMMdd}.csv");
        await _csvExportService.ExportAsync(DateTime.Today, DateTime.Today.AddDays(1), path);
        _trayIconService.ShowInfo("Экспорт выполнен", path);
    }

    private async Task ExportTodayJsonToDocumentsAsync()
    {
        var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        var directory = Path.Combine(documents, "PcActivityTracker");
        Directory.CreateDirectory(directory);

        var path = Path.Combine(directory, $"activity_{DateTime.Today:yyyyMMdd}.json");
        await _jsonExportService.ExportAsync(DateTime.Today, DateTime.Today.AddDays(1), path);
        _trayIconService.ShowInfo("Экспорт JSON выполнен", path);
    }

    private async void PauseButton_Click(object sender, RoutedEventArgs e)
    {
        TogglePause();
        await RefreshTodayAsync();
    }

    private async void RefreshButton_Click(object sender, RoutedEventArgs e)
    {
        await RefreshTodayAsync();
    }

    private async void LoadHistoryButton_Click(object sender, RoutedEventArgs e)
    {
        await LoadHistoryAsync();
    }

    private async void RefreshProgramChartButton_Click(object sender, RoutedEventArgs e)
    {
        await RefreshProgramChartAsync();
    }

    private async void SaveProgramChartSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        await SaveProgramChartSettingsAsync();
        await RefreshProgramChartAsync();
    }

    private async void LoadProgramChartSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        await LoadProgramChartSettingsAsync(showMessage: true);
        await RefreshProgramChartAsync();
    }

    private async void ExportCsvButton_Click(object sender, RoutedEventArgs e)
    {
        var (start, end) = GetHistoryPeriod();

        var dialog = new WpfSaveFileDialog
        {
            Title = "Экспорт активности в CSV",
            Filter = "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
            FileName = $"activity_{start:yyyyMMdd}_{end.AddDays(-1):yyyyMMdd}.csv"
        };

        if (dialog.ShowDialog(this) == true)
        {
            await _csvExportService.ExportAsync(start, end, dialog.FileName);
            WpfMessageBox.Show("Экспорт выполнен.", "PcActivityTracker", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }

    private async void ExportJsonButton_Click(object sender, RoutedEventArgs e)
    {
        var (start, end) = GetHistoryPeriod();

        var dialog = new WpfSaveFileDialog
        {
            Title = "Экспорт активности в JSON",
            Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*",
            FileName = $"activity_{start:yyyyMMdd}_{end.AddDays(-1):yyyyMMdd}.json"
        };

        if (dialog.ShowDialog(this) == true)
        {
            await _jsonExportService.ExportAsync(start, end, dialog.FileName);
            WpfMessageBox.Show("Экспорт JSON выполнен.", "PcActivityTracker", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }

    private async void SaveSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        await SaveSettingsAsync();
    }

    private async void ClearHistoryButton_Click(object sender, RoutedEventArgs e)
    {
        var result = WpfMessageBox.Show(
            "Удалить всю историю активности? Это действие нельзя отменить.",
            "PcActivityTracker",
            MessageBoxButton.YesNo,
            MessageBoxImage.Warning);

        if (result != MessageBoxResult.Yes)
            return;

        await _repository.DeleteAllAsync();
        await RefreshTodayAsync();
        await LoadHistoryAsync();
    }

    private async void DeleteFilteredHistoryButton_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(HistoryProgramFilterTextBox.Text)
            && string.IsNullOrWhiteSpace(HistoryCategoryFilterTextBox.Text)
            && string.IsNullOrWhiteSpace(HistoryWindowFilterTextBox.Text))
        {
            WpfMessageBox.Show(
                "Укажите программу, категорию или окно для удаления по отбору.",
                "PcActivityTracker",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        var (start, end) = GetHistoryPeriod();
        var records = await _reportService.GetRecordsAsync(start, end);
        var matchedCount = FilterRecords(
                records,
                HistoryProgramFilterTextBox.Text,
                HistoryCategoryFilterTextBox.Text,
                HistoryWindowFilterTextBox.Text)
            .Count();

        if (matchedCount == 0)
        {
            WpfMessageBox.Show(
                "По текущему отбору записей не найдено.",
                "PcActivityTracker",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            return;
        }

        var result = WpfMessageBox.Show(
            $"Удалить записи по текущему отбору? Найдено записей: {matchedCount}. Это действие нельзя отменить.",
            "PcActivityTracker",
            MessageBoxButton.YesNo,
            MessageBoxImage.Warning);

        if (result != MessageBoxResult.Yes)
            return;

        var deletedCount = await _repository.DeleteByFilterAsync(
            start,
            end,
            HistoryProgramFilterTextBox.Text,
            HistoryCategoryFilterTextBox.Text,
            HistoryWindowFilterTextBox.Text);

        await RefreshTodayAsync();
        await LoadHistoryAsync();

        WpfMessageBox.Show(
            $"Удалено записей: {deletedCount}.",
            "PcActivityTracker",
            MessageBoxButton.OK,
            MessageBoxImage.Information);
    }

    private void Window_StateChanged(object? sender, EventArgs e)
    {
        if (WindowState == WindowState.Minimized)
            Hide();
    }

    private void Window_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        if (_allowClose)
            return;

        e.Cancel = true;
        Hide();
        _trayIconService.ShowInfo("PcActivityTracker", "Приложение продолжает работать в трее.");
    }
}
