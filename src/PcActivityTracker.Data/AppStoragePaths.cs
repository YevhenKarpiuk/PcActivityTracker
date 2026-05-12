namespace PcActivityTracker.Data;

internal static class AppStoragePaths
{
    private const string AppDirectoryName = "PcActivityTracker";
    private const string PortableMarkerFileName = "portable.txt";
    private const string PortableDataDirectoryName = "data";
    private const string DatabaseFileName = "activity_tracker.db";
    private const string SettingsFileName = "settings.json";

    public static string GetAppDataDirectory()
    {
        var executableDirectory = AppContext.BaseDirectory;
        var portableMarkerPath = Path.Combine(executableDirectory, PortableMarkerFileName);
        var portableDataDirectory = Path.Combine(executableDirectory, PortableDataDirectoryName);

        if (File.Exists(portableMarkerPath) || HasExistingPortableData(portableDataDirectory))
            return portableDataDirectory;

        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), AppDirectoryName);
    }

    private static bool HasExistingPortableData(string portableDataDirectory)
    {
        return File.Exists(Path.Combine(portableDataDirectory, DatabaseFileName))
            || File.Exists(Path.Combine(portableDataDirectory, SettingsFileName));
    }
}
