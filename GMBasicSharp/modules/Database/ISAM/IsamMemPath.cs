// FILE: Basic.ISAM.IsamMemPath

namespace Basic.ISAM;

/// <summary>
/// Identifies ISAM tables backed by process memory instead of a disk <c>.isdb</c> file.
/// ponytail: single-process only; data is lost on exit; no cross-process WAL.
/// </summary>
public static class IsamMemPath
{
    /// <summary>Logical path prefix for in-memory ISAM tables (e.g. <c>MEM:MyTable</c>).</summary>
    public const string Prefix = "MEM:";

    /// <summary>Returns true when <paramref name="path"/> uses the <see cref="Prefix"/> scheme.</summary>
    public static bool IsMemPath(string? path) =>
        !string.IsNullOrEmpty(path) &&
        path.StartsWith(Prefix, StringComparison.OrdinalIgnoreCase);

    /// <summary>Extracts the registry key from a <c>MEM:</c> path.</summary>
    public static string NormalizeKey(string memPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(memPath);
        if (!IsMemPath(memPath))
        {
            throw new ArgumentException($"Expected path starting with '{Prefix}'.", nameof(memPath));
        }

        var key = memPath[Prefix.Length..].Trim();
        if (string.IsNullOrEmpty(key))
        {
            throw new ArgumentException($"Table name required after '{Prefix}'.", nameof(memPath));
        }

        return key;
    }

    /// <summary>Builds a canonical <c>MEM:</c> path from a table name.</summary>
    public static string FromTableName(string tableName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(tableName);
        return Prefix + tableName.Trim();
    }
}
