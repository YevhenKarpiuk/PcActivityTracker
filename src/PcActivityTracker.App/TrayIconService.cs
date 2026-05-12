using Drawing = System.Drawing;
using WinForms = System.Windows.Forms;

namespace PcActivityTracker.App;

public sealed class TrayIconService : IDisposable
{
    private readonly WinForms.NotifyIcon _notifyIcon;
    private readonly WinForms.ToolStripMenuItem _pauseMenuItem;
    private readonly Drawing.Icon _icon;

    public event EventHandler? OpenRequested;
    public event EventHandler? TogglePauseRequested;
    public event EventHandler? ExportTodayCsvRequested;
    public event EventHandler? ExportTodayJsonRequested;
    public event EventHandler? ExitRequested;

    public TrayIconService()
    {
        _pauseMenuItem = new WinForms.ToolStripMenuItem("Пауза");
        _pauseMenuItem.Click += (_, _) => TogglePauseRequested?.Invoke(this, EventArgs.Empty);

        var openItem = new WinForms.ToolStripMenuItem("Открыть");
        openItem.Click += (_, _) => OpenRequested?.Invoke(this, EventArgs.Empty);

        var exportCsvItem = new WinForms.ToolStripMenuItem("Экспорт CSV за сегодня");
        exportCsvItem.Click += (_, _) => ExportTodayCsvRequested?.Invoke(this, EventArgs.Empty);

        var exportJsonItem = new WinForms.ToolStripMenuItem("Экспорт JSON за сегодня");
        exportJsonItem.Click += (_, _) => ExportTodayJsonRequested?.Invoke(this, EventArgs.Empty);

        var exitItem = new WinForms.ToolStripMenuItem("Выход");
        exitItem.Click += (_, _) => ExitRequested?.Invoke(this, EventArgs.Empty);

        var menu = new WinForms.ContextMenuStrip();
        menu.Items.Add(openItem);
        menu.Items.Add(_pauseMenuItem);
        menu.Items.Add(exportCsvItem);
        menu.Items.Add(exportJsonItem);
        menu.Items.Add(new WinForms.ToolStripSeparator());
        menu.Items.Add(exitItem);

        _icon = LoadApplicationIcon();

        _notifyIcon = new WinForms.NotifyIcon
        {
            Text = "PcActivityTracker",
            Icon = _icon,
            ContextMenuStrip = menu,
            Visible = true
        };

        _notifyIcon.DoubleClick += (_, _) => OpenRequested?.Invoke(this, EventArgs.Empty);
    }

    public void SetPaused(bool paused)
    {
        _pauseMenuItem.Text = paused ? "Продолжить" : "Пауза";
        _notifyIcon.Text = paused ? "PcActivityTracker — пауза" : "PcActivityTracker";
    }

    public void ShowInfo(string title, string message)
    {
        _notifyIcon.BalloonTipTitle = title;
        _notifyIcon.BalloonTipText = message;
        _notifyIcon.ShowBalloonTip(3000);
    }

    public void Dispose()
    {
        _notifyIcon.Visible = false;
        _notifyIcon.Dispose();
        _icon.Dispose();
    }

    private static Drawing.Icon LoadApplicationIcon()
    {
        var resourceInfo = System.Windows.Application.GetResourceStream(
            new Uri("pack://application:,,,/Assets/PcActivityTracker.ico"));

        if (resourceInfo is null)
        {
            return (Drawing.Icon)Drawing.SystemIcons.Application.Clone();
        }

        using var stream = resourceInfo.Stream;
        using var icon = new Drawing.Icon(stream);

        return (Drawing.Icon)icon.Clone();
    }
}
