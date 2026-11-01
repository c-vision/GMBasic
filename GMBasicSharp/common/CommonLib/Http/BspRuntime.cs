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
        int EntryPoint = 0,
        // File da cui la pagina dipende (la pagina stessa + gli `.inc` importati): se uno
        // qualsiasi cambia, la cache è STALE e la pagina si ricompila. Senza, modificare un
        // helper in include/*.inc non si vedeva finché non si riavviava il processo.
        string[]? Dependencies = null);

    /// <summary>Cerca una pagina nella cache. Restituisce null se non trovata o modificata
    /// (la pagina o UNA QUALUNQUE delle sue dipendenze — gli `.inc` importati).</summary>
    public static CachedPage? CacheGet(string path)
    {
        if (_pageCache.TryGetValue(path, out var cached))
        {
            try
            {
                var fresh = File.Exists(path) && File.GetLastWriteTimeUtc(path) <= cached.SourceModified;
                if (fresh && cached.Dependencies is { Length: > 0 } deps)
                {
                    foreach (var dep in deps)
                        if (File.Exists(dep) && File.GetLastWriteTimeUtc(dep) > cached.SourceModified)
                        {
                            fresh = false;   // un .inc importato è più recente → ricompila
                            break;
                        }
                }
                if (fresh) return cached;
            }
            catch { /* file inaccessibile, ricompila */ }
            _pageCache.TryRemove(path, out _);
        }
        return null;
    }

    /// <summary>Salva una pagina nella cache.</summary>
    public static void CacheStore(string path, byte[] pcode, object[] constants, int[] lines, int globalCount = 0,
        IReadOnlyDictionary<string, int>? globalNameSlots = null, int entryPoint = 0, string[]? dependencies = null)
    {
        try
        {
            // SourceModified = MAX(mtime pagina, mtime dipendenze): la cache deve risultare
            // "fresca" subito dopo il salvataggio anche se un `.inc` importato è stato toccato
            // DOPO l'ultima modifica della pagina (caso normale: si modifica l'helper e non la
            // pagina). Con la sola mtime della pagina, il controllo sulle dipendenze invalidava
            // l'entry appena creata → ricompilazione a ogni richiesta (e il chiamante che
            // ricontrollava la cache trovava null → "0 opcodes").
            var newest = File.Exists(path) ? File.GetLastWriteTimeUtc(path) : DateTime.UtcNow;
            if (dependencies != null)
                foreach (var dep in dependencies)
                    if (File.Exists(dep))
                    {
                        var m = File.GetLastWriteTimeUtc(dep);
                        if (m > newest) newest = m;
                    }
            _pageCache[path] = new CachedPage(pcode, constants, lines, globalCount, DateTime.UtcNow,
                newest, globalNameSlots, entryPoint, dependencies);
            // Una ricompilazione invalida la resa memoizzata di quella pagina.
            RenderInvalidate(path);
        }
        catch { /* best-effort */ }
    }

    // ─── Resa memoizzata (opt-in) ────────────────────────────────────────────────
    // Il costo per richiesta di una pagina è l'ESECUZIONE (misurato: ~2 ms per una pagina
    // da 47 KB con 6 query; host+VM costano ~0,07 ms). Con molti client in polling la stessa
    // pagina viene rieseguita identica decine di volte al secondo: qui si memoizza il CORPO
    // per una finestra breve (TTL configurabile, 0 = disattivato) e si serve con ETag/304.
    // Chiave = path + query string (una pagina può dipendere da QUERY).
    private static readonly ConcurrentDictionary<string, Rendered> _renderCache = new();

    private sealed record Rendered(byte[] Body, DateTime At);

    /// <summary>TTL della resa memoizzata in ms (0 = nessuna cache: si esegue sempre la pagina).</summary>
    public static int RenderCacheTtlMs { get; set; }

    /// <summary>Motore di esecuzione delle pagine: managed (default, ha lo stato del runtime AI/DB)
    /// oppure C (≈4,7× più veloce ma senza stato managed; richiede il sidecar `.bsp.chunk`).
    /// Impostato da `HTTPCONFIG.BspEngine`.</summary>
    public static BspEngine EnginePreference { get; set; } = BspEngine.Managed;

    /// <summary>Pagine (nome file, es. "canvas.bsp") che usano il motore C anche quando la
    /// preferenza globale è `managed`. Il motore C non ha lo stato del runtime managed: una
    /// pagina che interroga agenti/team/workflow/memoria deve restare su `managed`.</summary>
    public static HashSet<string> EngineCPages { get; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Imposta l'elenco delle pagine da servire con il motore C (separatori `,` `;`).</summary>
    public static void SetEngineCPages(string? list)
    {
        EngineCPages.Clear();
        if (string.IsNullOrWhiteSpace(list)) return;
        foreach (var part in list.Split([',', ';'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            EngineCPages.Add(Path.GetFileName(part));
    }

    /// <summary>Pagine (CSV) dichiarate renderizzabili in parallelo dal motore C.</summary>
    public static string ConcurrentPages { get; set; } = "";

    /// <summary>True se la pagina va servita dal motore C (preferenza globale o elenco per pagina).</summary>
    public static bool UseCEngine(string pageFilePath)
    {
        if (EnginePreference == BspEngine.C) return true;
        return EngineCPages.Count > 0 && EngineCPages.Contains(Path.GetFileName(pageFilePath));
    }

    public static byte[]? RenderGet(string key)
    {
        if (RenderCacheTtlMs <= 0) return null;
        if (!_renderCache.TryGetValue(key, out var r)) return null;
        if ((DateTime.UtcNow - r.At).TotalMilliseconds > RenderCacheTtlMs)
        {
            _renderCache.TryRemove(key, out _);
            return null;
        }
        return r.Body;
    }

    public static void RenderPut(string key, byte[] body)
    {
        if (RenderCacheTtlMs <= 0) return;
        _renderCache[key] = new Rendered(body, DateTime.UtcNow);
    }

    /// <summary>Invalida la resa memoizzata di una pagina (tutte le chiavi con quel prefisso).</summary>
    public static void RenderInvalidate(string pagePath)
    {
        foreach (var k in _renderCache.Keys)
            if (k.StartsWith(pagePath, StringComparison.Ordinal)) _renderCache.TryRemove(k, out _);
    }

    public static void ClearAll()
    {
        _pageCache.Clear();
        _renderCache.Clear();
    }

    // ─── HTTP: ETag, 304, gzip ───────────────────────────────────────────────────

    /// <summary>ETag debole (FNV-1a 64 bit) del corpo: niente dipendenze crypto.</summary>
    public static string ComputeETag(byte[] body)
    {
        ulong h = 14695981039346656037UL;
        foreach (var b in body) { h ^= b; h *= 1099511628211UL; }
        return "W/\"" + h.ToString("x16") + "\"";
    }

    /// <summary>True se l'header If-None-Match del client contiene l'ETag corrente.</summary>
    public static bool ETagMatches(string? ifNoneMatch, string etag)
    {
        if (string.IsNullOrWhiteSpace(ifNoneMatch)) return false;
        foreach (var part in ifNoneMatch.Split(','))
            if (part.Trim().Equals(etag, StringComparison.Ordinal)) return true;
        return false;
    }

    /// <summary>True se il client accetta gzip (Accept-Encoding).</summary>
    public static bool AcceptsGzip(string? acceptEncoding)
        => !string.IsNullOrWhiteSpace(acceptEncoding) &&
           acceptEncoding.Contains("gzip", StringComparison.OrdinalIgnoreCase);

    /// <summary>Comprime in gzip con compressione rapida (la pagina è HTML, non archivio).</summary>
    public static byte[] Gzip(byte[] body)
    {
        using var ms = new System.IO.MemoryStream(body.Length / 4);
        using (var gz = new System.IO.Compression.GZipStream(ms, System.IO.Compression.CompressionLevel.Fastest, leaveOpen: true))
            gz.Write(body, 0, body.Length);
        return ms.ToArray();
    }

    /// <summary>Svuota tutta la cache.</summary>
    public static void CacheClear() => _pageCache.Clear();

    /// <summary>Numero di pagine in cache.</summary>
    public static int CacheCount => _pageCache.Count;
}
