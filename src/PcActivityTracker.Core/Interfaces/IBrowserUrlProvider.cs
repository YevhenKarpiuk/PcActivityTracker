using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Interfaces;

public interface IBrowserUrlProvider
{
    BrowserUrlInfo? TryGetBrowserUrl(ActivitySnapshot snapshot);
}
