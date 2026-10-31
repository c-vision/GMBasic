// FILE: CommonLib.LibsResolver.cs
//
// LibsResolver — UN solo punto che risponde a "dammi la libreria nativa <file>" per il
// mondo C#: cache → manifest (GMBasicModules) → download+sha → path. Nessun loader deve più
// conoscere i percorsi: MlNativeEngine, CGfxNative, TrayUtils, SqlCsvExtension, FfiService e
// CoreLibsBootstrap chiedono qui.
//
// MODELLO (Knowledge/GMBasicModules-Repo-Fidati.md, Resources-libs-Classificazione-Confine.md)
//   c-vision/GMBasicModules è l'UNICA fonte. Un indice, `libs-manifest.json`, descrive per RID:
//     groups:  support | engine | core  → release GitHub (zip per gruppo)
//              torch                    → EXTERNAL (download.pytorch.org), stesso protocollo
//     files:   nome → { gruppo, sha256, size }   ← è questo che rende il download GRANULARE
//   Le librerie si scaricano ON-DEMAND in `~/.gmbasic/libs/<rid>/` (configurabile con
//   `paths.libsCacheDir`). Il repo non le contiene e non le distribuisce.
//
// REGOLE
//   * niente variabili d'ambiente per configurare (AGENTS §Regola-primaria): settings, non env;
//   * nessun risultato silenzioso: se un file non è risolvibile si lancia un errore ESPLICITO
//     con l'elenco dei posti cercati;
//   * la cache è verificata con sha256 (un file corrotto viene riscaricato, non usato);
//   * il compilatore NON è richiesto: i tre gruppi arrivano firmati/verificati dall'indice.

using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace CommonLib;

/// <summary>Errore esplicito di risoluzione di una libreria nativa (mai silenzioso).</summary>
public sealed class LibsResolutionException : Exception
{
    public LibsResolutionException(string message) : base(message) { }
}

public sealed record LibsManifestFile(
    [property: JsonPropertyName("group")] string Group,
    /// <summary>URL del SINGOLO file (modello "un asset per file", kind="files"). Se è
    /// presente, il resolver scarica solo questo file invece dell'archivio del gruppo.</summary>
    [property: JsonPropertyName("url")] string? Url,
    [property: JsonPropertyName("sha256")] string? Sha256,
    [property: JsonPropertyName("size")] long Size);

public sealed record LibsGroupRid(
    [property: JsonPropertyName("archive")] string? Archive,
    [property: JsonPropertyName("url")] string? Url,
    [property: JsonPropertyName("sha256")] string? Sha256,
    [property: JsonPropertyName("size")] long Size,
    [property: JsonPropertyName("extractRoot")] string? ExtractRoot);

public sealed record LibsGroup(
    [property: JsonPropertyName("kind")] string? Kind,
    [property: JsonPropertyName("tag")] string? Tag,
    [property: JsonPropertyName("rids")] Dictionary<string, LibsGroupRid>? Rids);

public sealed record LibsRid(
    [property: JsonPropertyName("groups")] Dictionary<string, LibsGroup>? Groups,
    [property: JsonPropertyName("files")] Dictionary<string, LibsManifestFile>? Files);

public sealed record LibsManifest(
    [property: JsonPropertyName("version")] string? Version,
    [property: JsonPropertyName("cacheDir")] string? CacheDir,
    [property: JsonPropertyName("rids")] Dictionary<string, LibsRid>? Rids);

/// <summary>Stato di un file rispetto alla cache (per `gmb libs status`/`check`).</summary>
public enum LibsFileStatus { Cached, Missing, BadSha }

public sealed record LibsFileState(string Name, string Group, LibsFileStatus Status, long Size);

public static class LibsResolver
{
    /// <summary>Indice canonico: copia versionata nel repo, altrimenti scaricato da `main`.</summary>
    public const string ManifestUrl =
        "https://raw.githubusercontent.com/c-vision/GMBasicModules/main/libs-manifest.json";

    /// <summary>Gruppi che compongono il runtime "base": senza questi nessuna dylib nostra si carica.
    /// `engine` NON è il motore AI ma il suo PLUMBING (codec/token/vision wrapper): ogni `libgmb_*`
    /// linka `libllmcodec_{fp8,ggml,mlx_affine}` e `libllmtoken` (verificato con `otool -L`, e un
    /// dlopen senza di loro fallisce con "Library not loaded"). I motori pesanti (MLX/llama) stanno
    /// nel gruppo `llmmotor`, che serve solo per l'inferenza locale.</summary>
    public static readonly string[] LegacyBaseGroups = ["support", "core", "engine"];

    /// <summary>Gruppo UNICO che rende un'app funzionante (dal 2026-10-30): boot + core + engine +
    /// support. Esiste perché `core` richiede `engine` e i nostri runtime linkano terze parti:
    /// tenerli separati produceva combinazioni installabili ma non funzionanti.</summary>
    public const string RuntimeGroup = "runtime";

    /// <summary>Gruppo del PAYLOAD DI COMPILAZIONE (header + statiche nostre).</summary>
    public const string DevGroup = "dev";

    /// <summary>Gruppo delle NOSTRE lib FFI (amqp, grpc, kafka, media, mqtt, nats, protobuf):
    /// caricate con `DECLARE … LIB` a runtime, mai linkate. Sono ~290 KB in tutto e servono perché
    /// `REQUIRE amqp` funzioni SUBITO su ogni gamba: la VM/C gira in un processo esterno
    /// (`gmb_vm_host`) che non ha un `<exe>.libs.txt` e quindi non può scaricarle on-demand, e la
    /// gamba C99 le scarica accanto all'eseguibile. Con `ffi` nei gruppi base il `gmb libs ensure`
    /// (e il bootstrap) le mette in cache una volta per tutte.</summary>
    public const string FfiGroup = "ffi";

    private static readonly Dictionary<string, string[]> _baseGroups = new(StringComparer.Ordinal);
    private static readonly object _baseGroupsLock = new();

    /// <summary>Gruppi base per il RID, risolti sull'indice: `runtime` se l'indice lo espone,
    /// altrimenti i tre storici (`support`+`core`+`engine`) degli indici fino al 2026-10-29.
    /// NON è una costante: cambiare la costante a mano ruppe `gmb setup status` ("stato=assente")
    /// sui RID nuovi, dove `core`/`engine` non esistono più.</summary>
    public static string[] BaseGroupsFor(string? rid = null)
    {
        rid ??= Rid();
        lock (_baseGroupsLock)
        {
            if (_baseGroups.TryGetValue(rid, out var hit)) return hit;
            var res = LegacyBaseGroups;
            try
            {
                var m = LoadManifestAsync().GetAwaiter().GetResult();
                if (m.Rids is not null && m.Rids.TryGetValue(rid, out var r) && r.Files is not null
                    && r.Files.Values.Any(f => string.Equals(f.Group, RuntimeGroup, StringComparison.OrdinalIgnoreCase)))
                    res = [RuntimeGroup];
                // `ffi` (le nostre lib FFI, ~290 KB): se l'indice lo espone per questo RID va
                // garantito insieme al runtime — vedi la nota su FfiGroup (VM/C non ha libs.txt).
                if (m.Rids is not null && m.Rids.TryGetValue(rid, out var rf) && rf.Files is not null
                    && rf.Files.Values.Any(f => string.Equals(f.Group, FfiGroup, StringComparison.OrdinalIgnoreCase)))
                    res = [.. res, FfiGroup];
            }
            catch { /* indice non leggibile: si resta sui gruppi storici */ }
            _baseGroups[rid] = res;
            return res;
        }
    }

    /// <summary>Gruppi che un'app deve avere per funzionare (vedi `BaseGroupsFor`).</summary>
    public static string[] BaseGroups => BaseGroupsFor();

    /// <summary>Motori di inferenza locale (MLX + metallib, llama.cpp/ggml/mtmd + wrapper).</summary>
    public const string MotorGroup = "llmmotor";

    /// <summary>Gruppo esterno (pytorch.org), scaricato per-file quando serve.</summary>
    public const string TorchGroup = "torch";

    // ── percorsi ────────────────────────────────────────────────────────────────────────────
    /// <summary>Radice della cache librerie (`~/.gmbasic/libs`, override in paths.libsCacheDir).</summary>
    public static string CacheRoot => GmbiPaths.LibsCacheRoot;

    /// <summary>Cartella della cache per il RID: `<cache>/<rid>/` (piatta, nomi di file reali).</summary>
    public static string CacheDir(string rid) => Path.Combine(CacheRoot, rid);

    /// <summary>RID della macchina corrente (stessa mappatura del runtime C).</summary>
    public static string Rid()
    {
        var arch = RuntimeInformation.ProcessArchitecture;
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return arch == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows)) return "win-x64";
        return arch == Architecture.Arm64 ? "linux-arm64" : "linux-x64";
    }

    /// <summary>Cartelle "legacy" (checkout di sviluppo) da cui importare una libreria assente
    /// in cache: prima di scaricare si prova a copiare da qui (una volta sola).</summary>
    public static IEnumerable<string> LegacyDirs(string rid)
    {
        yield return Path.Combine(GmbiPaths.LibsDir, rid);
        yield return GmbiPaths.RepoLibsDir;
        yield return Path.Combine(FileSystemUtils.CoreAppPath(), "libs", rid);
    }

    // ── manifest ────────────────────────────────────────────────────────────────────────────
    /// <summary>Percorso del manifest usato: la `config/` CONDIVISA alla radice del monorepo
    /// (una sola copia; `Resources/` è per font e immagini, non per gli indici).</summary>
    public static string LocalManifestPath => Path.Combine(GmbiPaths.SharedConfigDir, "libs-manifest.json");

    /// <summary>Copia spedita con l'app (distribuzione senza checkout): &lt;exe&gt;/config/libs-manifest.json
    /// (la build la prende dalla `config/` condivisa).</summary>
    public static string ShippedManifestPath => Path.Combine(AppContext.BaseDirectory, "config", "libs-manifest.json");

    /// <summary>Carica il manifest: copia nel repo → copia spedita → cache locale → download da `main`.</summary>
    public static async Task<LibsManifest> LoadManifestAsync(HttpClient? http = null, Action<string>? log = null,
        CancellationToken ct = default)
    {
        var cached = Path.Combine(CacheRoot, ".manifest.json");
        foreach (var p in new[] { LocalManifestPath, ShippedManifestPath, cached })
        {
            if (!File.Exists(p)) continue;
            try
            {
                var m = JsonSerializer.Deserialize<LibsManifest>(File.ReadAllText(p));
                if (m?.Rids is { Count: > 0 }) return m;
            }
            catch (Exception ex) { log?.Invoke($"[libs] manifest illeggibile ({p}): {ex.Message}"); }
        }

        http ??= new HttpClient { Timeout = TimeSpan.FromMinutes(2) };
        log?.Invoke($"[libs] scarico l'indice: {ManifestUrl}");
        var json = await http.GetStringAsync(ManifestUrl, ct).ConfigureAwait(false);
        var man = JsonSerializer.Deserialize<LibsManifest>(json)
                  ?? throw new LibsResolutionException($"indice non valido: {ManifestUrl}");
        Directory.CreateDirectory(CacheRoot);
        File.WriteAllText(cached, json);
        return man;
    }

    /// <summary>File del gruppo dichiarati nel manifest per il RID.</summary>
    public static IReadOnlyList<string> GroupFiles(LibsManifest m, string rid, string group)
    {
        if (m.Rids is null || !m.Rids.TryGetValue(rid, out var r) || r.Files is null) return [];
        return r.Files.Where(kv => string.Equals(kv.Value.Group, group, StringComparison.OrdinalIgnoreCase))
                      .Select(kv => kv.Key).OrderBy(x => x, StringComparer.Ordinal).ToArray();
    }

    // ── stato ───────────────────────────────────────────────────────────────────────────────
    /// <summary>Stato dei file del manifest rispetto alla cache (sha verificato).</summary>
    public static IReadOnlyList<LibsFileState> Status(LibsManifest m, string rid)
    {
        var list = new List<LibsFileState>();
        if (m.Rids is null || !m.Rids.TryGetValue(rid, out var r) || r.Files is null) return list;
        var dir = CacheDir(rid);
        foreach (var (name, f) in r.Files.OrderBy(k => k.Key, StringComparer.Ordinal))
        {
            var path = Path.Combine(dir, name.Replace('/', Path.DirectorySeparatorChar));
            var st = !File.Exists(path) ? LibsFileStatus.Missing
                   : ArchiveFetcher.VerifySha256(path, f.Sha256) ? LibsFileStatus.Cached
                   : LibsFileStatus.BadSha;
            list.Add(new LibsFileState(name, f.Group, st, f.Size));
        }
        return list;
    }

    // ── ensure ──────────────────────────────────────────────────────────────────────────────
    /// <summary>Garantisce TUTTI i file di un gruppo, scaricando il bundle una sola volta.</summary>
    public static async Task<int> EnsureGroupAsync(string group, string? rid = null, Action<string>? log = null,
        HttpClient? http = null, CancellationToken ct = default)
    {
        rid ??= Rid();
        var m = await LoadManifestAsync(http, log, ct).ConfigureAwait(false);
        var names = GroupFiles(m, rid, group);
        if (names.Count == 0)
            throw new LibsResolutionException(
                $"il gruppo '{group}' non ha file per il RID '{rid}' nell'indice (version {m.Version})");
        var n = await EnsureAsync(names, rid, log, http, ct).ConfigureAwait(false);
        // alias da linker anche per i file già in cache (idempotente)
        foreach (var name in names)
        {
            var p = CachePath(rid, name);
            if (File.Exists(p)) EnsureLinkerAlias(p);
        }
        return n;
    }

    /// <summary>Garantisce un singolo file (lo trova nel suo gruppo tramite l'indice).</summary>
    public static Task<string> EnsureAsync(string fileName, string? rid = null, Action<string>? log = null,
        HttpClient? http = null, CancellationToken ct = default)
        => EnsureOneAsync(fileName, rid ?? Rid(), log, http, ct);

    /// <summary>Garantisce un insieme di file, raggruppati per bundle (un download per gruppo).</summary>
    public static async Task<int> EnsureAsync(IEnumerable<string> fileNames, string? rid = null,
        Action<string>? log = null, HttpClient? http = null, CancellationToken ct = default)
    {
        rid ??= Rid();
        var m = await LoadManifestAsync(http, log, ct).ConfigureAwait(false);
        if (m.Rids is null || !m.Rids.TryGetValue(rid, out var r) || r.Files is null)
            throw new LibsResolutionException($"RID '{rid}' assente nell'indice (version {m.Version})");
        http ??= new HttpClient { Timeout = TimeSpan.FromMinutes(30) };

        var missing = new List<string>();
        foreach (var n in fileNames.Distinct(StringComparer.Ordinal))
        {
            var path = CachePath(rid, n);
            if (File.Exists(path) && ArchiveFetcher.VerifySha256(path, r.Files[n].Sha256)) continue;
            missing.Add(n);
        }
        if (missing.Count == 0) return 0;

        var done = 0;
        foreach (var grp in missing.GroupBy(n => r.Files[n].Group, StringComparer.OrdinalIgnoreCase))
        {
            // Modello "un asset per file" (kind="files"): ogni file ha il proprio URL → si
            // scarica SOLO quello (nessun archivio di gruppo). È il motivo per cui esiste:
            // un programma che usa una libreria non deve scaricare 30 MB di gruppo.
            var direct = grp.Where(n => !string.IsNullOrEmpty(r.Files[n].Url)).ToArray();
            var archived = grp.Where(n => string.IsNullOrEmpty(r.Files[n].Url)).ToArray();

            foreach (var n in direct)
                done += await InstallFileAsync(n, r.Files[n], rid, log, http, ct).ConfigureAwait(false);

            if (archived.Length == 0) continue;
            if (r.Groups is null || !r.Groups.TryGetValue(grp.Key, out var g) || g.Rids is null
                || !g.Rids.TryGetValue(rid, out var gr) || string.IsNullOrEmpty(gr.Url))
                throw new LibsResolutionException(
                    $"gruppo '{grp.Key}' senza URL per il RID '{rid}' nell'indice (version {m.Version})");
            done += await InstallFromArchiveAsync(grp.Key, gr, r.Files, archived, rid, log, http, ct)
                        .ConfigureAwait(false);
        }
        return done;
    }

    private static string CachePath(string rid, string name)
        => Path.Combine(CacheDir(rid), name.Replace('/', Path.DirectorySeparatorChar));

    /// <summary>Scarica UN file dal suo URL (modello per-file), verifica lo sha256 e lo mette in
    /// cache in modo atomico (<c>.part</c> → rename). Un file per richiesta: nessun archivio.</summary>
    private static async Task<int> InstallFileAsync(string name, LibsManifestFile f, string rid,
        Action<string>? log, HttpClient http, CancellationToken ct)
    {
        if (string.IsNullOrEmpty(f.Url))
            throw new LibsResolutionException($"'{name}': URL assente nell'indice");
        var dest = CachePath(rid, name);
        Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
        var tmp = dest + ".part";
        try
        {
            log?.Invoke($"[libs] scarico {name} ({Fmt(f.Size)})");
            using (var resp = await http.GetAsync(f.Url, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false))
            {
                resp.EnsureSuccessStatusCode();
                await using var src = await resp.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
                await using var dst = File.Create(tmp);
                await src.CopyToAsync(dst, ct).ConfigureAwait(false);
            }
            if (!ArchiveFetcher.VerifySha256(tmp, f.Sha256))
            {
                try { File.Delete(tmp); } catch { }
                throw new LibsResolutionException(
                    $"sha256 di '{name}' NON valido dopo il download (asset appena ripubblicato? la CDN può servire byte vecchi: riprova tra 1-2 minuti)");
            }
            File.Move(tmp, dest, overwrite: true);
            EnsureLinkerAlias(dest);
            log?.Invoke($"[libs]   ✓ {name}");
            return 1;
        }
        catch
        {
            try { if (File.Exists(tmp)) File.Delete(tmp); } catch { }
            throw;
        }
    }

    private static async Task<string> EnsureOneAsync(string fileName, string rid, Action<string>? log,
        HttpClient? http, CancellationToken ct)
    {
        var m = await LoadManifestAsync(http, log, ct).ConfigureAwait(false);
        var path = CachePath(rid, fileName);

        if (m.Rids is not null && m.Rids.TryGetValue(rid, out var r) && r.Files is not null
            && r.Files.TryGetValue(fileName, out var f))
        {
            if (File.Exists(path) && ArchiveFetcher.VerifySha256(path, f.Sha256)) return path;
            await EnsureAsync([fileName], rid, log, http, ct).ConfigureAwait(false);
            if (File.Exists(path) && ArchiveFetcher.VerifySha256(path, f.Sha256)) return path;
            throw new LibsResolutionException($"'{fileName}' non è stato installato correttamente in {path}");
        }

        // non indicizzato: importo dal checkout legacy (una volta), mai silenziosamente assente
        Directory.CreateDirectory(CacheDir(rid));
        var tried = new List<string>();
        foreach (var dir in LegacyDirs(rid))
        {
            tried.Add(dir);
            var cand = Path.Combine(dir, fileName);
            if (!File.Exists(cand)) cand = Path.Combine(dir, "tessdata", Path.GetFileName(fileName));
            if (!File.Exists(cand)) continue;
            File.Copy(cand, path, overwrite: true);
            log?.Invoke($"[libs] '{fileName}' importato dal checkout: {cand}");
            return path;
        }
        throw new LibsResolutionException(
            $"'{fileName}' non è nell'indice delle librerie (version {m.Version}) e non esiste in: " +
            string.Join(" · ", tried) + ". Aggiungerlo a un gruppo con tools/libs_publish_gh.sh.");
    }

    /// <summary>Scarica (una volta) il bundle e ne estrae SOLO i file richiesti, verificando lo sha.</summary>
    private static async Task<int> InstallFromArchiveAsync(string group, LibsGroupRid gr,
        Dictionary<string, LibsManifestFile> files, string[] wanted, string rid, Action<string>? log,
        HttpClient http, CancellationToken ct)
    {
        if (string.IsNullOrEmpty(gr.Url))
            throw new LibsResolutionException($"gruppo '{group}': URL mancante per il RID '{rid}'");

        var downloads = Path.Combine(CacheRoot, ".downloads");
        Directory.CreateDirectory(downloads);
        var archive = Path.Combine(downloads, string.IsNullOrEmpty(gr.Archive) ? $"{group}.zip" : gr.Archive);
        var reused = File.Exists(archive) && ArchiveFetcher.VerifySha256(archive, gr.Sha256);

        if (!reused)
        {
            // Download con RETRY: subito dopo un `gh release upload --clobber` la CDN di GitHub
            // può ancora servire i byte VECCHI per qualche decina di secondi (caso reale: sha
            // mismatch al primo tentativo, corretto al secondo). Si riprova, poi errore esplicito.
            var attempts = string.IsNullOrEmpty(gr.Sha256) ? 1 : 3;
            for (var attempt = 1; attempt <= attempts; attempt++)
            {
                log?.Invoke($"[libs] scarico il gruppo '{group}' ({Fmt(gr.Size)}) — {gr.Url}" +
                            (attempt > 1 ? $"  (tentativo {attempt}/{attempts})" : ""));
                var lastPct = -1;
                await ArchiveFetcher.DownloadAsync(http, gr.Url, archive, (done, total) =>
                {
                    if (total <= 0) return;
                    var pct = (int)(done * 100 / total);
                    if (pct >= lastPct + 20) { lastPct = pct; log?.Invoke($"[libs]   {pct}% ({Fmt(done)}/{Fmt(total)})"); }
                }, ct).ConfigureAwait(false);
                if (string.IsNullOrEmpty(gr.Sha256) || ArchiveFetcher.VerifySha256(archive, gr.Sha256))
                    break;
                if (attempt == attempts)
                    throw new LibsResolutionException(
                        $"sha256 dell'archivio '{Path.GetFileName(archive)}' NON valido dopo {attempts} tentativi " +
                        "(asset appena ripubblicato? la CDN può servire i byte vecchi: riprova tra 1-2 minuti)");
                log?.Invoke($"[libs]   sha non corrispondente (CDN in aggiornamento) — riprovo tra 5s…");
                try { File.Delete(archive); } catch { }
                await Task.Delay(TimeSpan.FromSeconds(5), ct).ConfigureAwait(false);
            }
        }
        else log?.Invoke($"[libs] archivio '{group}' già scaricato e verificato: {archive}");

        var dir = CacheDir(rid);
        Directory.CreateDirectory(dir);
        var installed = 0;
        using var zip = ZipFile.OpenRead(archive);
        foreach (var name in wanted)
        {
            ct.ThrowIfCancellationRequested();
            var rel = name.Replace('/', '/');
            var entry = FindEntry(zip, rel, gr.ExtractRoot);
            if (entry is null)
                throw new LibsResolutionException(
                    $"'{name}' non è dentro il bundle '{group}' ({Path.GetFileName(archive)}) — indice e asset divergono");
            var dest = CachePath(rid, name);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            var tmp = dest + ".part";
            entry.ExtractToFile(tmp, overwrite: true);
            if (files.TryGetValue(name, out var f) && !ArchiveFetcher.VerifySha256(tmp, f.Sha256))
            {
                File.Delete(tmp);
                throw new LibsResolutionException($"sha256 di '{name}' NON valido dopo l'estrazione (bundle '{group}')");
            }
            File.Move(tmp, dest, overwrite: true);
            EnsureLinkerAlias(dest);
            installed++;
            log?.Invoke($"[libs]   ✓ {name} ({Fmt(files.TryGetValue(name, out var z) ? z.Size : 0)})");
        }

        // l'archivio non serve più: la cache tiene i FILE (verificati), non i bundle
        try { zip.Dispose(); File.Delete(archive); } catch { /* lock transitorio: si riproverà */ }
        return installed;
    }

    /// <summary>Alias per il LINKER: dai bundle arrivano i soname versionati
    /// (`libzstd.1.5.7.dylib`, `libcrypto.3.dylib`, `liblz4.1.10.0.dylib`), ma `-lzstd`/`find_library`
    /// cercano il nome NON versionato. Crea un symlink relativo `lib<base>.dylib` quando manca
    /// (idempotente; `prune` non li tocca perché sono symlink derivati).</summary>
    private static void EnsureLinkerAlias(string path)
    {
        try
        {
            var dir = Path.GetDirectoryName(path);
            var name = Path.GetFileName(path);
            var m = System.Text.RegularExpressions.Regex.Match(name,
                @"^(?<base>lib.+?)(\.\d[\d.]*)?(\.dylib|\.so(\.[\d.]+)*|\.dll)$");
            if (!m.Success) return;
            var canonical = m.Groups["base"].Value + (name.EndsWith(".dylib") ? ".dylib" : name.EndsWith(".dll") ? ".dll" : ".so");
            if (string.Equals(canonical, name, StringComparison.Ordinal)) return;
            var alias = Path.Combine(dir!, canonical);
            if (File.Exists(alias) || Directory.Exists(alias)) return;
            File.CreateSymbolicLink(alias, name);   // relativo: la cache resta spostabile
        }
        catch { /* niente symlink (es. Windows senza privilegi): il file versionato resta usabile */ }
    }

    /// <summary>Trova la voce del file: per nome, preferendo la sottocartella `extractRoot`.</summary>
    private static ZipArchiveEntry? FindEntry(ZipArchive zip, string name, string? extractRoot)
    {
        var want = name;
        ZipArchiveEntry? byBase = null;
        foreach (var e in zip.Entries)
        {
            if (e.FullName.EndsWith("/") || e.Name.Length == 0) continue;
            if (e.FullName.Replace('\\', '/').EndsWith("/" + want, StringComparison.Ordinal)
                || e.FullName.Replace('\\', '/') == want)
            {
                if (!string.IsNullOrEmpty(extractRoot)
                    && e.FullName.Replace('\\', '/').Contains(extractRoot + "/", StringComparison.Ordinal))
                    return e;
                byBase ??= e;
            }
        }
        return byBase;
    }

    private static string Fmt(long b) =>
        b >= 1_048_576 ? $"{b / 1048576.0:F1} MB" : b >= 1024 ? $"{b / 1024.0:F0} KB" : $"{b} B";

    /// <summary>Verifica di integrità dei gruppi indicati (uso: bootstrap, CI, `gmb libs check`).</summary>
    public static async Task<IReadOnlyList<LibsFileState>> CheckAsync(IEnumerable<string>? groups = null,
        string? rid = null, HttpClient? http = null, CancellationToken ct = default)
    {
        rid ??= Rid();
        var m = await LoadManifestAsync(http, null, ct).ConfigureAwait(false);
        var wanted = groups?.ToHashSet(StringComparer.OrdinalIgnoreCase);
        return Status(m, rid).Where(s => wanted is null || wanted.Contains(s.Group)).ToArray();
    }

    /// <summary>Elenco dei gruppi presenti nell'indice per il RID (con numero di file e byte).</summary>
    public static IReadOnlyList<(string Group, int Files, long Bytes, string Kind, string? Tag)> Groups(
        LibsManifest m, string rid)
    {
        if (m.Rids is null || !m.Rids.TryGetValue(rid, out var r) || r.Files is null) return [];
        return r.Files.GroupBy(kv => kv.Value.Group, StringComparer.OrdinalIgnoreCase)
            .OrderBy(g => g.Key, StringComparer.Ordinal)
            .Select(g =>
            {
                var grp = r.Groups is not null && r.Groups.TryGetValue(g.Key, out var gg) ? gg : null;
                return (g.Key, g.Count(), g.Sum(x => x.Value.Size), grp?.Kind ?? "github-release", grp?.Tag);
            }).ToArray();
    }
}
