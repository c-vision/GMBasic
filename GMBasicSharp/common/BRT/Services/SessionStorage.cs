namespace Basic.Runtime.Services;

/// <summary>
/// Configura lo store sessioni di processo (CommonLib.Http.SessionStores) dal file SQLite
/// indicato in HTTP CONFIG .SessionFile. Path vuoto → torna in-memory. Idempotente sul path.
/// </summary>
public static class SessionStorage
{
    private static string? _configuredPath;

    public static void ConfigureFile(string filePath)
    {
        var path = string.IsNullOrWhiteSpace(filePath) ? "" : Path.GetFullPath(filePath);
        if (string.Equals(_configuredPath, path, StringComparison.OrdinalIgnoreCase)) return;
        _configuredPath = path;
        if (path.Length == 0)
            CommonLib.Http.SessionStores.Configure(null);
        else
            CommonLib.Http.SessionStores.Configure(new SqliteSessionPersistence(path));
    }
}
