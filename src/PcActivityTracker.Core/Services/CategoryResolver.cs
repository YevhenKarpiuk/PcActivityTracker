namespace PcActivityTracker.Core.Services;

public static class CategoryResolver
{
    public const string DefaultCategory = "Без категории";

    public static string Resolve(string processName, IDictionary<string, string>? categories)
    {
        if (string.IsNullOrWhiteSpace(processName) || categories is null)
            return DefaultCategory;

        var normalized = NormalizeProcessName(processName);

        foreach (var pair in categories)
        {
            if (string.Equals(NormalizeProcessName(pair.Key), normalized, StringComparison.OrdinalIgnoreCase))
                return string.IsNullOrWhiteSpace(pair.Value) ? DefaultCategory : pair.Value.Trim();
        }

        return DefaultCategory;
    }

    public static string NormalizeProcessName(string processName)
    {
        var value = (processName ?? string.Empty).Trim();
        if (string.IsNullOrWhiteSpace(value))
            return "unknown.exe";

        return value.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? value : value + ".exe";
    }
}
