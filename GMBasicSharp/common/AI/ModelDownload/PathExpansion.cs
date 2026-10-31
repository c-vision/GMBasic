// AI/ModelDownload/PathExpansion.cs — '~' expansion shared by AIPATH consumers
namespace Basic.ModelDownload;

public static class PathExpansion {
    /// <summary>Expands a leading '~' (or '~/...') to the user's home directory. AIPATH entries
    /// and --dest values are stored/typed with '~' for portability across machines; nothing
    /// upstream expands it, so every consumer that turns a path into an actual Directory/File
    /// call must go through this first.</summary>
    public static string Expand(string path)
    {
        if (string.IsNullOrEmpty(path) || path[0] != '~') return path;
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        if (path.Length == 1) return home;
        return path[1] is '/' or '\\' ? Path.Combine(home, path[2..]) : path;
    }
}
