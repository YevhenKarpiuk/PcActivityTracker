using System.Text.RegularExpressions;
using System.Windows.Automation;
using PcActivityTracker.Core.Interfaces;
using PcActivityTracker.Core.Models;
using PcActivityTracker.Core.Services;

namespace PcActivityTracker.Windows;

public sealed class BrowserUrlProvider : IBrowserUrlProvider
{
    private static readonly Regex UrlInTextRegex = new("https?://[^\\s\\\"'<>]+", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    public BrowserUrlInfo? TryGetBrowserUrl(ActivitySnapshot snapshot)
    {
        if (!TrackingExclusionMatcher.IsBrowserProcess(snapshot.ProcessName))
            return null;

        var fromAutomation = TryReadAddressBar(snapshot.WindowHandle);
        var urlInfo = BrowserUrlInfo.FromUrl(fromAutomation);
        if (urlInfo is not null)
            return urlInfo;

        var fromTitle = TryExtractUrlFromTitle(snapshot.WindowTitle);
        return BrowserUrlInfo.FromUrl(fromTitle);
    }

    private static string? TryReadAddressBar(nint windowHandle)
    {
        if (windowHandle == 0)
            return null;

        try
        {
            var root = AutomationElement.FromHandle((IntPtr)windowHandle);
            if (root is null)
                return null;

            var edits = root.FindAll(TreeScope.Descendants, new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.Edit));
            foreach (AutomationElement edit in edits)
            {
                var value = TryGetValue(edit);
                if (LooksLikeUrl(value))
                    return value;
            }
        }
        catch
        {
            return null;
        }

        return null;
    }

    private static string? TryGetValue(AutomationElement element)
    {
        try
        {
            if (element.TryGetCurrentPattern(ValuePattern.Pattern, out var pattern) && pattern is ValuePattern valuePattern)
            {
                var value = valuePattern.Current.Value;
                if (!string.IsNullOrWhiteSpace(value))
                    return value.Trim();
            }

            var name = element.Current.Name;
            if (!string.IsNullOrWhiteSpace(name))
                return name.Trim();
        }
        catch
        {
            return null;
        }

        return null;
    }

    private static bool LooksLikeUrl(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return false;

        var text = value.Trim();
        if (text.Contains(' '))
            return false;

        return text.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
            || text.StartsWith("https://", StringComparison.OrdinalIgnoreCase)
            || (text.Contains('.', StringComparison.Ordinal) && !text.Contains('\\'));
    }

    private static string? TryExtractUrlFromTitle(string? title)
    {
        if (string.IsNullOrWhiteSpace(title))
            return null;

        var match = UrlInTextRegex.Match(title);
        return match.Success ? match.Value : null;
    }
}
