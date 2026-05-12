namespace PcActivityTracker.Core.Models;

public sealed class BrowserUrlInfo
{
    public string Url { get; set; } = string.Empty;
    public string Domain { get; set; } = string.Empty;

    public static BrowserUrlInfo? FromUrl(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return null;

        var url = value.Trim();
        if (url.Contains(' '))
            return null;

        if (!url.Contains("://", StringComparison.Ordinal))
        {
            if (!url.Contains('.', StringComparison.Ordinal))
                return null;

            url = "https://" + url;
        }

        if (!Uri.TryCreate(url, UriKind.Absolute, out var uri))
            return null;

        if (!uri.Scheme.Equals(Uri.UriSchemeHttp, StringComparison.OrdinalIgnoreCase)
            && !uri.Scheme.Equals(Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase))
            return null;

        if (string.IsNullOrWhiteSpace(uri.Host))
            return null;

        return new BrowserUrlInfo
        {
            Url = uri.ToString(),
            Domain = uri.Host.Trim().Trim('.').ToLowerInvariant()
        };
    }
}
