using System.Runtime.InteropServices;
using PcActivityTracker.Core.Interfaces;

namespace PcActivityTracker.Windows;

public sealed class IdleTimeProvider : IIdleTimeProvider
{
    public int GetIdleSeconds()
    {
        var info = new NativeMethods.LastInputInfo
        {
            CbSize = (uint)Marshal.SizeOf<NativeMethods.LastInputInfo>()
        };

        if (!NativeMethods.GetLastInputInfo(ref info))
            return 0;

        var tickCount = unchecked((uint)Environment.TickCount);
        var idleMilliseconds = unchecked(tickCount - info.DwTime);

        return (int)Math.Min(int.MaxValue, idleMilliseconds / 1000);
    }
}
