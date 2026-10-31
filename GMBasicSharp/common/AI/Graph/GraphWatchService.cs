// FILE: Basic.AI.Graph.GraphWatchService
// Second Brain — watchdog del GRAPH (parità `graphify --watch`): osserva una cartella wiki e
// rigenera il grafo (GraphBuildService) quando un file .md cambia/crea/cancella. Debounce:
// gli eventi FileSystemWatcher sono raffiche (editor che salva più volte) → si aspetta un
// periodo di quiete (~1s) prima di un rebuild, così non si rigenera per ogni singolo save.
// Architettura NON dipende dal daemon: GRAPH BUILD manuale resta il primario; il watch è
// un'accelerazione opzionale per chi sta scrivendo note e vuole il grafo sempre aggiornato.
// Mai stub: cartella inesistente → errore esplicito; rebuild fallito → messaggio, non crash.
using System.Collections.Concurrent;

namespace Basic.AI.Graph;

/// <summary>Watchdog del grafo: osserva una cartella wiki e rigenera il grafo su ogni modifica
/// (debounced). Gestione per-wiki con vita dei FileSystemWatcher. Thread-safe.</summary>
public static class GraphWatchService
{
    private sealed class WatchEntry
    {
        public FileSystemWatcher? Watcher;
        public string WikiDir = "";
        public string GraphsDir = "";
        public string Name = "";
        public DateTime LastEventUtc;
        public bool RebuildRunning;
        public bool RebuildQueued;
    }

    private static readonly ConcurrentDictionary<string, WatchEntry> _watchers = new(StringComparer.OrdinalIgnoreCase);
    private static readonly object _lock = new();

    /// <summary>Cartelle attualmente osservate (chiave = path wiki assoluto).</summary>
    public static IReadOnlyList<string> WatchedDirectories =>
        _watchers.Keys.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();

    /// <summary>Avvia il watchdog su <paramref name="wikiDir"/>. Ritorna 1 se già/ora attivo.
    /// Cartella inesistente → errore esplicito (ArgumentException/DirectoryNotFoundException).</summary>
    public static int Start(string wikiDir, string graphsDir, string name)
    {
        if (string.IsNullOrWhiteSpace(wikiDir)) throw new ArgumentException("wikiDir mancante");
        var full = Path.GetFullPath(wikiDir);
        if (!Directory.Exists(full)) throw new DirectoryNotFoundException($"cartella wiki non trovata: {full}");
        if (string.IsNullOrWhiteSpace(graphsDir)) graphsDir = Basic.AI.AiConfig.GraphsDefaultDir;
        Directory.CreateDirectory(graphsDir);
        var nm = string.IsNullOrWhiteSpace(name) ? Path.GetFileName(full.TrimEnd('/', '\\')) : name;

        lock (_lock)
        {
            if (_watchers.TryGetValue(full, out var existing) && existing.Watcher is not null)
            {
                existing.GraphsDir = graphsDir;
                existing.Name = nm;
                return 1;   // già attivo
            }
            var entry = _watchers.GetOrAdd(full, _ => new WatchEntry { WikiDir = full });
            entry.GraphsDir = graphsDir;
            entry.Name = nm;
            entry.Watcher = new FileSystemWatcher(full)
            {
                IncludeSubdirectories = true,
                NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size,
            };
            entry.Watcher.Changed += (_, e) => OnFileEvent(entry, e.FullPath);
            entry.Watcher.Created += (_, e) => OnFileEvent(entry, e.FullPath);
            entry.Watcher.Deleted += (_, e) => OnFileEvent(entry, e.FullPath);
            entry.Watcher.Renamed += (_, e) => OnFileEvent(entry, e.FullPath);
            entry.Watcher.Error += (_, _) => { /* sovraccarico: ignora, il rebuild successivo copre */ };
            entry.Watcher.EnableRaisingEvents = true;
        }
        return 1;
    }

    /// <summary>Ferma il watchdog di una cartella (per path wiki). Ritorna 1 se era attivo.</summary>
    public static int Stop(string wikiDir)
    {
        if (string.IsNullOrWhiteSpace(wikiDir)) return 0;
        var full = Path.GetFullPath(wikiDir);
        lock (_lock)
        {
            if (_watchers.TryGetValue(full, out var e) && e.Watcher is not null)
            {
                e.Watcher.Dispose();
                e.Watcher = null;
                return 1;
            }
        }
        return 0;
    }

    /// <summary>Ferma tutti i watcher (fine sessione/test).</summary>
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

    /// <summary>Numero di cartelle osservate (test/diagnostica).</summary>
    public static int WatchedCount => _watchers.Values.Count(e => e.Watcher is not null && e.Watcher.EnableRaisingEvents);

    private static void OnFileEvent(WatchEntry entry, string fullPath)
    {
        // ignora file temporanei/editor
        var fileName = Path.GetFileName(fullPath);
        if (fileName.StartsWith('.') || fileName.EndsWith('~')) return;
        var ext = Path.GetExtension(fileName);
        if (!string.Equals(ext, ".md", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(ext, ".markdown", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(ext, ".txt", StringComparison.OrdinalIgnoreCase)) return;

        lock (entry)
        {
            entry.LastEventUtc = DateTime.UtcNow;
            if (entry.RebuildRunning) { entry.RebuildQueued = true; return; }
            if (entry.RebuildQueued) return;
        }
        // debounce: programma il controllo tra 900ms (se arrivano altri eventi si ri-pianifica)
        _ = Task.Run(async () =>
        {
            while (true)
            {
                DateTime last;
                lock (entry) last = entry.LastEventUtc;
                await Task.Delay(900);
                bool runNow;
                lock (entry)
                {
                    // ancora raffica di eventi → ri-attendi
                    if (DateTime.UtcNow - entry.LastEventUtc < TimeSpan.FromMilliseconds(900)) continue;
                    if (entry.RebuildRunning) { entry.RebuildQueued = true; continue; }
                    entry.RebuildRunning = true;
                    entry.RebuildQueued = false;
                    runNow = true;
                }
                if (!runNow) continue;
                try { RunRebuild(entry); }
                catch { /* RunRebuild già gestisce */ }
                bool again;
                lock (entry) { entry.RebuildRunning = false; again = entry.RebuildQueued; entry.RebuildQueued = false; }
                if (again) continue;   // durante il rebuild sono arrivati altri eventi → nuovo ciclo
                return;
            }
        });
    }

    private static void RunRebuild(WatchEntry entry)
    {
        try
        {
            var opts = new GraphBuildOptions(entry.WikiDir, entry.GraphsDir, entry.Name,
                DryRun: false, Force: false, Prune: true);
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var res = GraphBuildService.Run(opts, progress: null, log: null);
            sw.Stop();
            Console.WriteLine($"[WATCH {entry.Name}] rebuild in {sw.Elapsed.TotalSeconds:F1}s — nodi {res.Nodes}, edge {res.Edges}, dangling {res.Dangling}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[WATCH {entry.Name}] rebuild fallito: {ex.Message}");
        }
    }
}
