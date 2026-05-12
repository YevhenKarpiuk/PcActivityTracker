using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using LiveChartsCore;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.App.ViewModels;

public sealed class MainViewModel : INotifyPropertyChanged
{
    private string _activeTimeText = "00:00:00";
    private string _idleTimeText = "00:00:00";
    private string _mostUsedProgramText = "—";
    private string _statusText = "Активно";
    private ISeries[] _programSeries = Array.Empty<ISeries>();
    private ISeries[] _categorySeries = Array.Empty<ISeries>();
    private ISeries[] _programChartSeries = Array.Empty<ISeries>();
    private string _programChartTitle = "График программ";

    public event PropertyChangedEventHandler? PropertyChanged;

    public string ActiveTimeText { get => _activeTimeText; set => SetField(ref _activeTimeText, value); }
    public string IdleTimeText { get => _idleTimeText; set => SetField(ref _idleTimeText, value); }
    public string MostUsedProgramText { get => _mostUsedProgramText; set => SetField(ref _mostUsedProgramText, value); }
    public string StatusText { get => _statusText; set => SetField(ref _statusText, value); }
    public ISeries[] ProgramSeries { get => _programSeries; set => SetField(ref _programSeries, value); }
    public ISeries[] CategorySeries { get => _categorySeries; set => SetField(ref _categorySeries, value); }
    public ISeries[] ProgramChartSeries { get => _programChartSeries; set => SetField(ref _programChartSeries, value); }
    public string ProgramChartTitle { get => _programChartTitle; set => SetField(ref _programChartTitle, value); }

    public ObservableCollection<ActivityRecord> TodayRecords { get; } = new();
    public ObservableCollection<ActivityRecord> HistoryRecords { get; } = new();
    public ObservableCollection<UsageItem> ProgramUsage { get; } = new();
    public ObservableCollection<UsageItem> CategoryUsage { get; } = new();
    public ObservableCollection<UsageItem> ProgramChartUsage { get; } = new();

    public void ReplaceTodayRecords(IEnumerable<ActivityRecord> records) => Replace(TodayRecords, records);
    public void ReplaceHistoryRecords(IEnumerable<ActivityRecord> records) => Replace(HistoryRecords, records);
    public void ReplaceProgramUsage(IEnumerable<UsageItem> items) => Replace(ProgramUsage, items);
    public void ReplaceCategoryUsage(IEnumerable<UsageItem> items) => Replace(CategoryUsage, items);
    public void ReplaceProgramChartUsage(IEnumerable<UsageItem> items) => Replace(ProgramChartUsage, items);

    private static void Replace<T>(ObservableCollection<T> collection, IEnumerable<T> items)
    {
        collection.Clear();
        foreach (var item in items)
            collection.Add(item);
    }

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
            return false;

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        return true;
    }
}
