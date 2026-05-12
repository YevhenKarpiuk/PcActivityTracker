using System.Runtime.InteropServices;
using System.Text;

namespace PcActivityTracker.Windows;

internal static class NativeMethods
{
    [DllImport("user32.dll")]
    internal static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll")]
    internal static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    internal static extern bool GetLastInputInfo(ref LastInputInfo plii);

    [StructLayout(LayoutKind.Sequential)]
    internal struct LastInputInfo
    {
        public uint CbSize;
        public uint DwTime;
    }
}
