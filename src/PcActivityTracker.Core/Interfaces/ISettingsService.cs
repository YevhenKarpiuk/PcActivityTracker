using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Interfaces;

public interface ISettingsService
{
    Task<AppSettings> GetSettingsAsync(CancellationToken cancellationToken = default);
    Task SaveSettingsAsync(AppSettings settings, CancellationToken cancellationToken = default);
}
