using System.Diagnostics;
using System.Text;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;
using PcActivityTracker.Core.Services;

namespace PcActivityTracker.Windows;

public sealed class ActiveWindowProvider : IActiveWindowProvider
{
    public ActivitySnapshot GetCurrentWindow()
    {
        var snapshot = new ActivitySnapshot { Timestamp = DateTime.Now };

        try
        {
            var handle = NativeMethods.GetForegroundWindow();
            snapshot.WindowHandle = handle;
            if (handle == IntPtr.Zero)
                return Unknown(snapshot, "Нет активного окна");

            snapshot.WindowTitle = GetWindowTitle(handle);

            NativeMethods.GetWindowThreadProcessId(handle, out var processId);
            if (processId == 0)
                return Unknown(snapshot, snapshot.WindowTitle);

            using var process = Process.GetProcessById((int)processId);
            snapshot.ProcessName = CategoryResolver.NormalizeProcessName(process.ProcessName);

            try
            {
                snapshot.ExePath = process.MainModule?.FileName ?? string.Empty;
            }
            catch
            {
                snapshot.ExePath = string.Empty;
            }
        }
        catch
        {
            return Unknown(snapshot, "Ошибка чтения окна");
        }

        if (string.IsNullOrWhiteSpace(snapshot.ProcessName))
            snapshot.ProcessName = "unknown.exe";

        if (string.IsNullOrWhiteSpace(snapshot.WindowTitle))
            snapshot.WindowTitle = snapshot.ProcessName;

        return snapshot;
    }

    private static ActivitySnapshot Unknown(ActivitySnapshot snapshot, string title)
    {
        snapshot.ProcessName = "unknown.exe";
        snapshot.WindowTitle = title;
        snapshot.ExePath = string.Empty;
        return snapshot;
    }

    private static string GetWindowTitle(IntPtr handle)
    {
        var length = NativeMethods.GetWindowTextLength(handle);
        if (length <= 0)
            length = 256;

        var builder = new StringBuilder(length + 1);
        var read = NativeMethods.GetWindowText(handle, builder, builder.Capacity);
        return read <= 0 ? string.Empty : builder.ToString();
    }
}
