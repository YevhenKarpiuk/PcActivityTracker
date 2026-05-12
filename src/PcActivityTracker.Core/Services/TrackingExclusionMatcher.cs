using System.Text.RegularExpressions;
using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Services;

public static class TrackingExclusionMatcher
{
    public static bool IsExcluded(ActivitySnapshot snapshot, AppSettings settings)
    {
        return IsProcessExcluded(snapshot.ProcessName, settings.ExcludedProcesses)
            || IsWindowTitleExcluded(snapshot.WindowTitle, settings.ExcludedWindowTitles)
            || IsWindowTitlePatternExcluded(snapshot.WindowTitle, settings.ExcludedWindowTitlePatterns)
            || IsBrowserDomainExcluded(snapshot.BrowserDomain, settings.ExcludedBrowserDomains);
    }

    public static bool IsBrowserProcess(string processName)
    {
        var normalized = CategoryResolver.NormalizeProcessName(processName);
        return normalized.Equals("chrome.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("msedge.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("firefox.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("opera.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("brave.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("vivaldi.exe", StringComparison.OrdinalIgnoreCase)
            || normalized.Equals("browser.exe", StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsProcessExcluded(string processName, IEnumerable<string>? excludedProcesses)
    {
        if (excludedProcesses is null)
            return false;

        var normalized = CategoryResolver.NormalizeProcessName(processName);
        return excludedProcesses.Any(x => string.Equals(CategoryResolver.NormalizeProcessName(x), normalized, StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsWindowTitleExcluded(string windowTitle, IEnumerable<string>? exactTitles)
    {
        if (exactTitles is null || string.IsNullOrWhiteSpace(windowTitle))
            return false;

        return exactTitles.Any(x => string.Equals(x?.Trim(), windowTitle.Trim(), StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsWindowTitlePatternExcluded(string windowTitle, IEnumerable<string>? patterns)
    {
        if (patterns is null || string.IsNullOrWhiteSpace(windowTitle))
            return false;

        foreach (var pattern in patterns.Where(x => !string.IsNullOrWhiteSpace(x)).Select(x => x.Trim()))
        {
            if (MatchesPattern(windowTitle, pattern))
                return true;
        }

        return false;
    }

    private static bool IsBrowserDomainExcluded(string browserDomain, IEnumerable<string>? excludedDomains)
    {
        if (excludedDomains is null || string.IsNullOrWhiteSpace(browserDomain))
            return false;

        var domain = NormalizeDomain(browserDomain);
        if (string.IsNullOrWhiteSpace(domain))
            return false;

        foreach (var rawRule in excludedDomains.Where(x => !string.IsNullOrWhiteSpace(x)))
        {
            var rule = NormalizeDomain(rawRule);
            if (string.IsNullOrWhiteSpace(rule))
                continue;

            if (rule.StartsWith("*.", StringComparison.Ordinal))
            {
                var suffix = rule[2..];
                if (domain.Equals(suffix, StringComparison.OrdinalIgnoreCase)
                    || domain.EndsWith("." + suffix, StringComparison.OrdinalIgnoreCase))
                    return true;

                continue;
            }

            if (domain.Equals(rule, StringComparison.OrdinalIgnoreCase)
                || domain.EndsWith("." + rule, StringComparison.OrdinalIgnoreCase))
                return true;
        }

        return false;
    }

    public static string NormalizeDomain(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return string.Empty;

        var text = value.Trim().ToLowerInvariant();

        if (text.StartsWith("*.", StringComparison.Ordinal))
            return "*." + text[2..].Split('/')[0].Trim().Trim('.');

        if (!text.Contains("://", StringComparison.Ordinal))
            text = "https://" + text;

        if (Uri.TryCreate(text, UriKind.Absolute, out var uri) && !string.IsNullOrWhiteSpace(uri.Host))
            return uri.Host.Trim().Trim('.').ToLowerInvariant();

        return value.Trim().Split('/')[0].Trim().Trim('.').ToLowerInvariant();
    }

    private static bool MatchesPattern(string value, string pattern)
    {
        try
        {
            if (pattern.StartsWith("regex:", StringComparison.OrdinalIgnoreCase))
            {
                var regexPattern = pattern[6..];
                return Regex.IsMatch(value, regexPattern, RegexOptions.IgnoreCase | RegexOptions.CultureInvariant, TimeSpan.FromMilliseconds(200));
            }

            var wildcardRegex = "^" + Regex.Escape(pattern).Replace("\\*", ".*").Replace("\\?", ".") + "$";
            return Regex.IsMatch(value, wildcardRegex, RegexOptions.IgnoreCase | RegexOptions.CultureInvariant, TimeSpan.FromMilliseconds(200));
        }
        catch
        {
            return false;
        }
    }
}
