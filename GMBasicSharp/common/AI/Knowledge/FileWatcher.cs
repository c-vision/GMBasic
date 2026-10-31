// FILE: Basic.AI.Knowledge.FileWatcher
// Second Brain — F7 P2 (residuo): watchdog del vault con policy di indicizzazione.
//   manual    → nessun watcher (VAULT INDEX esplicito resta il primario)
//   auto      → FileSystemWatcher cross-platform: on create/change reindicizza il file
//               (IndexFile upsert hash), on delete/rename rimuove la voce stale (RemoveFile)
// Architettura NON dipende dal daemon: VAULT INDEX manuale + incremental (mtime/hash) restano
// il percorso primario; il watcher è un'accelerazione opzionale. Mai stub: policy ignota →
// errore esplicito; watcher senza vault → errore esplicito.

namespace Basic.AI.Knowledge;

/// <summary>Policy di indicizzazione del vault.</summary>
public enum VaultIndexingPolicy
{
    Manual,   // nessun watcher (default)
    Auto,     // reindicizza on-change
}

/// <summary>Watchdog del vault: osserva la cartella root e reindicizza/rimuove i file che
/// cambiano. Gestione per-vault con vita dei FileSystemWatcher.</summary>
public static class FileWatcher
{
    private sealed class WatchEntry
    {
        public FileSystemWatcher? Watcher;
        public string Root = "";
        public string Pattern = "*";
        public long VaultId = -1;
    }

    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, WatchEntry> _watchers =
        new(StringComparer.OrdinalIgnoreCase);

    private static readonly object _lock = new();

    /// <summary>Policy corrente del vault (default Manual).</summary>
    public static VaultIndexingPolicy GetPolicy(string vaultName)
    {
        var entry = _watchers.TryGetValue(vaultName, out var w) ? w : null;
        return entry?.Watcher is not null ? VaultIndexingPolicy.Auto : VaultIndexingPolicy.Manual;
    }

    /// <summary>Imposta la policy e avvia/ferma il watcher. Ritorna 1 su OK.
    /// Vault inesistente → errore esplicito. Auto su un vault senza root → errore esplicito.</summary>
    public static int SetPolicy(string vaultName, VaultIndexingPolicy policy)
    {
        if (policy == VaultIndexingPolicy.Manual)
        {
            lock (_lock)
            {
                if (_watchers.TryGetValue(vaultName, out var e) && e.Watcher is not null)
                {
                    e.Watcher.Dispose();
                    e.Watcher = null;
                }
            }
            return 1;
        }

        // Auto: serve il vault registrato (root/pattern).
        var info = Basic.AI.Knowledge.VaultManager.VaultInfo(vaultName);
        if (string.IsNullOrEmpty(info)) throw new ArgumentException($"vault '{vaultName}' non registrato");
        string root = "", pattern = "*";
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(info);
            root = doc.RootElement.GetProperty("rootPath").GetString() ?? "";
            pattern = doc.RootElement.GetProperty("pattern").GetString() ?? "*";
        }
        catch { throw new ArgumentException($"vault '{vaultName}' non registrato"); }
        if (!Directory.Exists(root)) throw new DirectoryNotFoundException($"root del vault non trovata: {root}");

        lock (_lock)
        {
            if (_watchers.TryGetValue(vaultName, out var existing) && existing.Watcher is not null)
                existing.Watcher.Dispose();
            var entry = _watchers.GetOrAdd(vaultName, _ => new WatchEntry { Root = root, Pattern = pattern });
            entry.Root = root;
            entry.Pattern = pattern;
            entry.Watcher = new FileSystemWatcher(root)
            {
                IncludeSubdirectories = true,
                NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size,
            };
            entry.Watcher.Changed += (_, e) => OnFileEvent(vaultName, e.FullPath);
            entry.Watcher.Created += (_, e) => OnFileEvent(vaultName, e.FullPath);
            entry.Watcher.Deleted += (_, e) => OnDeleted(vaultName, e.FullPath);
            entry.Watcher.Renamed += (_, e) => OnRenamed(vaultName, e.OldFullPath, e.FullPath);
            entry.Watcher.EnableRaisingEvents = true;
        }
        return 1;
    }

    /// <summary>Numero di vault attualmente osservati (per test/diagnostica).</summary>
    public static int WatchedCount =>
        _watchers.Values.Count(e => e.Watcher is not null && e.Watcher.EnableRaisingEvents);

    /// <summary>Ferma tutti i watcher e libera le risorse (fine sessione).</summary>
    public static void StopAll()
    {
        lock (_lock)
        {
            foreach (var e in _watchers.Values)
            {
                e.Watcher?.Dispose();
                e.Watcher = null;
            }
        }
    }

    private static void OnFileEvent(string vaultName, string fullPath)
    {
        // ignora file temporanei/editor (es. .swp, ~, .tmp)
        var name = Path.GetFileName(fullPath);
        if (name.StartsWith('.') || name.EndsWith("~") || name.EndsWith(".tmp", StringComparison.OrdinalIgnoreCase)) return;
        // best-effort: la reindicizzazione non deve mai rompere il turno
        try { VaultManager.IndexFile(vaultName, fullPath); }
        catch { /* watchdog best-effort */ }
    }

    private static void OnDeleted(string vaultName, string fullPath)
    {
        try { VaultManager.RemoveFile(vaultName, fullPath); }
        catch { /* watchdog best-effort */ }
    }

    private static void OnRenamed(string vaultName, string oldPath, string newPath)
    {
        try
        {
            VaultManager.RemoveFile(vaultName, oldPath);
            VaultManager.IndexFile(vaultName, newPath);
        }
        catch { /* watchdog best-effort */ }
    }
}
