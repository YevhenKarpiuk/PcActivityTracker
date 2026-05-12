using System.Windows;
using PcActivityTracker.Core.Services;
using PcActivityTracker.Data;
using PcActivityTracker.Windows;

namespace PcActivityTracker.App;

public partial class App : System.Windows.Application
{
    private ActivityMonitorService? _monitorService;
    private TrayIconService? _trayIconService;

    private async void Application_Startup(object sender, StartupEventArgs e)
    {
        var databaseInitializer = new DatabaseInitializer();
        await databaseInitializer.InitializeAsync();

        var settingsService = new SettingsService();
        var repository = new ActivityRepository(databaseInitializer.DatabasePath);
        var reportService = new ReportService(repository);
        var csvExportService = new CsvExportService(repository);
        var jsonExportService = new JsonExportService(repository);
        var activeWindowProvider = new ActiveWindowProvider();
        var idleTimeProvider = new IdleTimeProvider();
        var browserUrlProvider = new BrowserUrlProvider();

        _monitorService = new ActivityMonitorService(activeWindowProvider, idleTimeProvider, repository, settingsService, browserUrlProvider);
        _trayIconService = new TrayIconService();

        var mainWindow = new MainWindow(_monitorService, reportService, repository, settingsService, csvExportService, jsonExportService, _trayIconService);

        MainWindow = mainWindow;
        mainWindow.Show();
        await _monitorService.StartAsync();
    }

    private void Application_Exit(object sender, ExitEventArgs e)
    {
        try
        {
            _monitorService?.StopAsync().GetAwaiter().GetResult();
        }
        catch
        {
        }

        _trayIconService?.Dispose();
    }
}
