using System.Collections.Concurrent;

namespace CommonLib.Http;

/// <summary>
/// Runtime per BASIC Scripting Pages: cache compilazione e funzioni di supporto.
/// CommonLib NON referenzia BRT/Ast — i tipi IRuntimeHost, JsonWrapper, HttpRequest/Response 
/// vengono gestiti dall'host (PCodeVm/Executor) che li inietta nel contesto BSP.
/// </summary>
public static class BspRuntime
{
    // Cache compilazione BSP (condivisa tra tutte le richieste)
    private static readonly ConcurrentDictionary<string, CachedPage> _pageCache = new();

    public record CachedPage(
        byte[] PCode, object[] Constants, int[] Lines,
        int GlobalCount,
        DateTime Compiled, DateTime SourceModified,
        IReadOnlyDictionary<string, int>? GlobalNameSlots = null,
        // Indirizzo di ingresso del chunk compilato (CompiledChunk.EntryPoint): quando il
        // programma ha inizializzatori globali/static (es. `DIM opts = NEW HTTPOPTIONS` a
        // livello modulo, sempre presente in una pagina .bsp — vedi BspCompiler.Compile), il
        // Compiler NON fa partire l'esecuzione da __main ma da un piccolo PROLOGO che chiama
        // __global_init/__static_init PRIMA di __main (altrimenti quegli inizializzatori non
        // girerebbero mai). Senza salvare qui questo indirizzo, il chunk ricostruito dalla
        // cache aveva EntryPoint di default (0) — che spesso coincide PROPRIO con l'inizio del
        // corpo di __global_init stesso (se non ci sono UDT/CLASS/PROCEDURE compilate prima) —
        // quindi il task eseguiva __global_init DIRETTAMENTE, MAI tramite OP_CALL dal prologo:
        // al suo RETURN di chiusura, task.Frame è null (nessuna vera chiamata) e il task
        // finiva subito, SENZA MAI raggiungere __main (bug: pagina BSP con qualunque DIM
        // globale inizializzata — es. `DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS` — restava
        // vuota, l'esecuzione si fermava subito dopo l'ultimo inizializzatore).
        int EntryPoint = 0);

    /// <summary>Cerca una pagina nella cache. Restituisce null se non trovata o modificata.</summary>
    public static CachedPage? CacheGet(string path)
    {
        if (_pageCache.TryGetValue(path, out var cached))
        {
            try
            {
                if (File.Exists(path) && File.GetLastWriteTimeUtc(path) <= cached.SourceModified)
                    return cached;
            }
            catch { /* file inaccessibile, ricompila */ }
            _pageCache.TryRemove(path, out _);
        }
        return null;
    }

    /// <summary>Salva una pagina nella cache.</summary>
    public static void CacheStore(string path, byte[] pcode, object[] constants, int[] lines, int globalCount = 0,
        IReadOnlyDictionary<string, int>? globalNameSlots = null, int entryPoint = 0)
    {
        try
        {
            _pageCache[path] = new CachedPage(pcode, constants, lines, globalCount, DateTime.UtcNow,
                File.Exists(path) ? File.GetLastWriteTimeUtc(path) : DateTime.UtcNow, globalNameSlots, entryPoint);
        }
        catch { /* best-effort */ }
    }

    /// <summary>Svuota tutta la cache.</summary>
    public static void CacheClear() => _pageCache.Clear();

    /// <summary>Numero di pagine in cache.</summary>
    public static int CacheCount => _pageCache.Count;
}
