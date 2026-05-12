using Microsoft.Win32;
using System.Diagnostics;

namespace PcActivityTracker.App;

public static class AutostartManager
{
    private const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string AppName = "PcActivityTracker";

    public static bool IsEnabled()
    {
        using var key = Registry.CurrentUser.OpenSubKey(RunKey, writable: false);
        var value = key?.GetValue(AppName)?.ToString();
        return !string.IsNullOrWhiteSpace(value);
    }

    public static void SetEnabled(bool enabled)
    {
        using var key = Registry.CurrentUser.OpenSubKey(RunKey, writable: true)
            ?? Registry.CurrentUser.CreateSubKey(RunKey, writable: true);

        if (!enabled)
        {
            key.DeleteValue(AppName, throwOnMissingValue: false);
            return;
        }

        var exePath = Process.GetCurrentProcess().MainModule?.FileName;
        if (!string.IsNullOrWhiteSpace(exePath))
            key.SetValue(AppName, $"\"{exePath}\"");
    }
}
