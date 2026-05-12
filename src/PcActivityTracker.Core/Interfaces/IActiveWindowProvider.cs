using PcActivityTracker.Core.Models;

namespace PcActivityTracker.Core.Interfaces;

public interface IActiveWindowProvider
{
    ActivitySnapshot GetCurrentWindow();
}
