// FILE: AI.Tests.TestPaths
// GUARDIA SISTEMICA (incidente 2026-09-16): un test che ripristinava una config e POI cancellava la
// directory derivata da quella config ha cancellato `<repo>/rag` (con dentro brainvault.db) a ogni run
// della suite. Da ora OGNI cancellazione ricorsiva nei test passa da qui: se il path non è sotto la
// directory temporanea del sistema, NON si cancella (si segnala e si prosegue).
using Xunit.Abstractions;

namespace AI.Tests;

internal static class TestPaths
{
    /// <summary>True se <paramref name="path"/> è dentro la directory temporanea del sistema
    /// (o dentro /tmp). Nient'altro è cancellabile dai test.</summary>
    internal static bool IsSafeToDelete(string? path)
    {
        if (string.IsNullOrWhiteSpace(path)) return false;
        string full, tempRoot;
        try { full = Path.GetFullPath(path); } catch { return false; }
        try { tempRoot = Path.GetFullPath(Path.GetTempPath()); } catch { return false; }
        var candidates = new[] { tempRoot, "/tmp", "/private/tmp", "/var/folders" };
        foreach (var root in candidates)
        {
            var r = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            if (full.Equals(r, StringComparison.Ordinal)) continue;      // mai cancellare la root stessa
            if (full.StartsWith(r + Path.DirectorySeparatorChar, StringComparison.Ordinal)) return true;
        }
        return false;
    }

    /// <summary>Cancella una directory di test SOLO se è sotto la temp del sistema. Ritorna true se
    /// ha cancellato; false se rifiutato (path non temporaneo: possibile dato utente/artefatto reale).</summary>
    internal static bool SafeDeleteTree(string? path, ITestOutputHelper? log = null)
    {
        if (string.IsNullOrWhiteSpace(path)) return false;
        if (!IsSafeToDelete(path))
        {
            log?.WriteLine($"[GUARDIA] cancellazione RIFIUTATA (path non temporaneo): {path}");
            return false;
        }
        try { if (Directory.Exists(path)) Directory.Delete(path, recursive: true); return true; }
        catch { return false; }
    }
}
