namespace PcActivityTracker.Core.Models;

public sealed class ActivitySnapshot
{
    public DateTime Timestamp { get; set; } = DateTime.Now;
    public nint WindowHandle { get; set; }
    public string ProcessName { get; set; } = string.Empty;
    public string WindowTitle { get; set; } = string.Empty;
    public string ExePath { get; set; } = string.Empty;
    public string BrowserUrl { get; set; } = string.Empty;
    public string BrowserDomain { get; set; } = string.Empty;
    public bool IsIdle { get; set; }
    public int IdleSeconds { get; set; }
}
