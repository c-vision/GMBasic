/* FILE: native/gmb_db_c/src/ISAM/IsamMemPath.c */
/* Port 1:1 di modules/Database/ISAM/IsamMemPath.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamMemPath
//
//namespace Basic.ISAM;
//
///// <summary>
///// Identifies ISAM tables backed by process memory instead of a disk <c>.isdb</c> file.
///// ponytail: single-process only; data is lost on exit; no cross-process WAL.
///// </summary>
//public static class IsamMemPath
//{
//    /// <summary>Logical path prefix for in-memory ISAM tables (e.g. <c>MEM:MyTable</c>).</summary>
//    public const string Prefix = "MEM:";
//
//    /// <summary>Returns true when <paramref name="path"/> uses the <see cref="Prefix"/> scheme.</summary>
//    public static bool IsMemPath(string? path) =>
//        !string.IsNullOrEmpty(path) &&
//        path.StartsWith(Prefix, StringComparison.OrdinalIgnoreCase);
//
//    /// <summary>Extracts the registry key from a <c>MEM:</c> path.</summary>
//    public static string NormalizeKey(string memPath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(memPath);
//        if (!IsMemPath(memPath))
//        {
//            throw new ArgumentException($"Expected path starting with '{Prefix}'.", nameof(memPath));
//        }
//
//        var key = memPath[Prefix.Length..].Trim();
//        if (string.IsNullOrEmpty(key))
//        {
//            throw new ArgumentException($"Table name required after '{Prefix}'.", nameof(memPath));
//        }
//
//        return key;
//    }
//
//    /// <summary>Builds a canonical <c>MEM:</c> path from a table name.</summary>
//    public static string FromTableName(string tableName)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(tableName);
//        return Prefix + tableName.Trim();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

int IsamMemPath_IsMemPath(const char* path)
{
    return path && path[0] && gmb_strncasecmp(path, IsamMemPath_Prefix, 4) == 0;
}

int IsamMemPath_NormalizeKey(const char* memPath, char* out, int cap)
{
    const char* p;
    int n;
    if (!out || cap <= 0) return GMB_DB_ARG;
    out[0] = 0;
    if (!IsamMemPath_IsMemPath(memPath)) return GMB_DB_ARG;
    p = memPath + 4;
    while (*p == ' ' || *p == '\t') p++;
    n = (int)strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
    if (n <= 0) return GMB_DB_ARG;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    return GMB_DB_OK;
}

int IsamMemPath_FromTableName(const char* tableName, char* out, int cap)
{
    const char* p;
    int n;
    if (!tableName || !out || cap <= 0) return GMB_DB_ARG;
    p = tableName;
    while (*p == ' ' || *p == '\t') p++;
    n = (int)strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
    if (n <= 0) return GMB_DB_ARG;
    snprintf(out, (size_t)cap, "%s%.*s", IsamMemPath_Prefix, n, p);
    return GMB_DB_OK;
}
