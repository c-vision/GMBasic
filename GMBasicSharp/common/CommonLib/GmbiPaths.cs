// FILE: CommonLib.GmbiPaths.cs

using System.Text.Json;

/* 
 * GmbiPaths.cs: risoluzione centralizzata dei percorsi dati di GMBASIC per il MONDO C#.
 * 
 * REGOLA (AGENTS.md §0bis/§10): C# e C sono runtime SEPARATI e NON condividono codice.
 * Questo file serve SOLO al mondo C# (Executor/REPL/common/*). Il mondo C risolve gli
 * stessi percorsi nel proprio runtime (gmb_paths in gmb_runtime_c), leggendo lo stesso
 * file system.settings.json.
 * 
 * FONTE: la sezione "paths" di system.settings.json. File cercato in quest'ordine:
 *   1. <root>/../shared/settings/system.settings.json  (CONFIG COMUNE, submodule `shared`)
 *   2. <root>/shared/settings/system.settings.json     (fallback: checkout del solo GMBasic)
 *   3. <CoreAppPath()>/config/system.settings.json (eseguibile — distribuzione)
 *   4. <CoreAppPath()>/system.settings.json
 *   5. <CoreAppPath()>/Assets/Config/system.settings.json
 * dove <root> = directory che contiene GMBASIC.sln (repo) oppure CoreAppPath().
 * 
 * OGNI chiave accetta un percorso assoluto O relativo alla root; supporta ~ e variabili
 * d'ambiente. Se assente/null → default (vedi record PathSettings): le cartelle definitive
 * stanno nella root del progetto (config/, skills/, rag/, graphs/, ...), i componenti runtime
 * scaricati in libs-runtime/<componente>/<rid>, le cache temporanee in
 * System.IO.Path.GetTempPath().
 * 
 * NB: non confondere con SystemSettings (le altre sezioni console/namespace). GmbiPaths
 * espone SOLO i percorsi; il resto del file resta gestito da SettingsManager.
 */

namespace CommonLib;

/// <summary>Risolutore unico dei percorsi dati di GMBASIC (mondo C#).</summary>
public static class GmbiPaths
{
    private static readonly object Lock = new();
    private static Resolved? _resolved;
    private static string? _configDirOverride;

    /// <summary>Hook TEST-SCOPED: reindirizza ConfigDir (e tutto ciò che ne deriva —
    /// RecursiveMasDir, ProvidersFile, AiConfigFile, ...) a una cartella temporanea, senza
    /// toccare RepoRoot/system.settings.json. Serve ai test che esercitano codice reale (es. un
    /// round-trip di rete verso HuggingFace se un token è disponibile) senza scrivere nella
    /// config/ reale e versionata del repo (bug osservato 2026-09-07:
    /// RecursiveMasExtensionsTests scriveva in config/recursivemas/hfteam/ reale).</summary>
    internal static void OverrideConfigDirForTests(string configDir)
    {
        lock (Lock) { _configDirOverride = configDir; _resolved = null; }
    }

    internal static void ResetConfigDirOverrideForTests()
    {
        lock (Lock) { _configDirOverride = null; _resolved = null; }
    }

    /// <summary>Radice UNICA dei dati utente: <c>~/.gmbasic</c>. Prima del 2026-10-29 esistevano
    /// DUE radici (<c>~/.gmb</c> per libs/packages daemon, <c>~/.gmbasic</c> per config e
    /// componenti ML/Chromium): confusione e cache doppie. Ora c'è solo questa; chi aveva
    /// <c>~/.gmb</c> viene migrato una volta (move, non copia) e la vecchia radice sparisce.
    /// Il contenuto è quello del filesystem, quindi la migrazione va fatta PRIMA di risolvere i
    /// path (e prima di ogni Ensure, che altrimenti riscaricherebbe 300+ MB).</summary>
    public static string HomeRoot => System.IO.Path.Combine(Home(), ".gmbasic");

    /// <summary>Migrazione una tantum <c>~/.gmbasic/*</c> → <c>~/.gmbasic/*</c>. Idempotente e
    /// conservativa: sposta solo ciò che nella destinazione non esiste ancora (merge per voce),
    /// e alla fine rimuove <c>~/.gmb</c> se è rimasto vuoto. Best-effort: un errore non deve
    /// impedire l'avvio (si riproverà al giro successivo).</summary>
    private static void MigrateLegacyHomeOnce()
    {
        try
        {
            var home = Home();
            var oldRoot = System.IO.Path.Combine(home, ".gmb");
            var newRoot = System.IO.Path.Combine(home, ".gmbasic");
            if (Directory.Exists(oldRoot))
            {
                Directory.CreateDirectory(newRoot);
                foreach (var src in Directory.EnumerateFileSystemEntries(oldRoot))
                {
                    var name = System.IO.Path.GetFileName(src);
                    var dst = System.IO.Path.Combine(newRoot, name);
                    if (!File.Exists(dst) && !Directory.Exists(dst)) { File.Move(src, dst); continue; }
                    if (Directory.Exists(src) && Directory.Exists(dst))
                    {
                        // merge: sposta le voci mancanti nella destinazione
                        foreach (var child in Directory.EnumerateFileSystemEntries(src))
                        {
                            var cn = System.IO.Path.GetFileName(child);
                            var cd = System.IO.Path.Combine(dst, cn);
                            if (!File.Exists(cd) && !Directory.Exists(cd)) File.Move(child, cd);
                        }
                        try { if (!Directory.EnumerateFileSystemEntries(src).Any()) Directory.Delete(src); } catch { }
                    }
                }
                try { if (!Directory.EnumerateFileSystemEntries(oldRoot).Any()) Directory.Delete(oldRoot); } catch { }
            }
            // componenti legacy AI/<browser> direttamente sotto ~/.gmbasic → dentro libs/
            foreach (var box in new[] { "ml", "chromium", "ocr", "obscura" })
            {
                var legacy = System.IO.Path.Combine(newRoot, box);
                var target = System.IO.Path.Combine(newRoot, "libs", box);
                if (!Directory.Exists(legacy) || Directory.Exists(target)) continue;
                Directory.CreateDirectory(System.IO.Path.GetDirectoryName(target)!);
                Directory.Move(legacy, target);
            }
        }
        catch { /* best-effort: la migrazione non deve mai impedire l'avvio */ }
    }

    private sealed record Resolved(
        string RepoRoot,
        string SettingsFile,
        PathSettings Paths,
        string ConfigDir,
        string SkillsDir,
        string PluginsDir,
        string LogsDir,
        string RagDir,
        string GraphsDir,
        string BrainsDir,
        string BrainsExportsDir,
        string LibsDir,
        string LibsCacheRoot,
        string PackagesDir,
        string TempDir);

    // ========================================================================
    // Proprietà pubbliche (lazy, cache singleton)
    // ========================================================================

    /// <summary>Root del progetto GMBASIC (dir con GMBASIC.sln) o CoreAppPath().</summary>
    public static string RepoRoot => Current.RepoRoot;

    /// <summary>Percorso effettivo di system.settings.json usato (potrebbe non esistere).</summary>
    public static string SettingsFile => Current.SettingsFile;

    /// <summary>Sezione paths effettiva (default applicati dove mancante).</summary>
    public static PathSettings Paths => Current.Paths;

    /// <summary>Cartella file di stato/config AI (ai.json, vaults.db, ...). Default: &lt;root&gt;/config</summary>
    public static string ConfigDir => Current.ConfigDir;

    /// <summary>Archivio skills. Default: &lt;root&gt;/skills</summary>
    public static string SkillsDir => Current.SkillsDir;

    /// <summary>Archivio plugin. Default: &lt;root&gt;/plugins</summary>
    public static string PluginsDir => Current.PluginsDir;

    /// <summary>Log agenti AI. Default: &lt;root&gt;/ai-logs</summary>
    public static string LogsDir => Current.LogsDir;

    /// <summary>Database RAG. Default: &lt;root&gt;/rag</summary>
    public static string RagDir => Current.RagDir;

    /// <summary>Export graph. Default: &lt;root&gt;/graphs</summary>
    public static string GraphsDir => Current.GraphsDir;

    /// <summary>Cervelli installati. Default: &lt;root&gt;/brains</summary>
    public static string BrainsDir => Current.BrainsDir;

    /// <summary>Export BRAIN EXPORT. Default: &lt;root&gt;/brains-exports</summary>
    public static string BrainsExportsDir => Current.BrainsExportsDir;

    /// <summary>Base componenti runtime scaricati on-demand (torch/ml, chromium, obscura, ocr): i
    /// loader cercano in &lt;libsDir&gt;/&lt;componente&gt;/&lt;rid&gt;/. Default: la CACHE UTENTE
    /// `~/.gmbasic/libs` (configurabile con `paths.libsDir`); una sola radice, niente più
    /// `libs-runtime/` nel checkout.</summary>
    public static string LibsDir => Current.LibsDir;

    /// <summary>Radice della CACHE delle librerie native scaricate on-demand
    /// (`&lt;libsCacheRoot&gt;/&lt;rid&gt;/`). Default: ~/.gmbasic/libs. Vale per checkout e client:
    /// le librerie non stanno né nel repo né accanto all'eseguibile.</summary>
    public static string LibsCacheRoot => Current.LibsCacheRoot;

    /// <summary>Cartella della cache librerie per il RID corrente.</summary>
    public static string LibsCacheDir(string rid) => Path.Combine(Current.LibsCacheRoot, rid);

    /// <summary>Cartella dei PACCHETTI installati dal package manager (stile node_modules):
    /// &lt;packagesDir&gt;/&lt;nome&gt;/&lt;versione&gt;/. Default: &lt;root&gt;/packages (TUTTO ciò che è
    /// scaricato resta dentro GMBasic — niente file fuori dal progetto, niente ~/.gmb).</summary>
    public static string PackagesDir => Current.PackagesDir;

    /// <summary>Cartella ABI di LLMEnv come CONFIGURATA in `paths.abiDir` (espansa), o null se non ancora valorizzata.
    /// La localizzazione/scrittura la fa il runtime C alla prima necessita' (vedi PathSettings.AbiDir).</summary>
    public static string? AbiDir =>
        string.IsNullOrWhiteSpace(Current.Paths.AbiDir) ? null : Expand(Current.Paths.AbiDir);

    /// <summary>Cartella delle NOSTRE librerie interne COMPILATE (`&lt;monorepo&gt;/shared/libs/&lt;nome&gt;/`):
    /// è **condivisa dai due compilatori**, quindi vive in `shared/`, non nel checkout di GMBasic.
    /// Risolta in ASSOLUTO, così qualunque programma BASIC (creato ovunque) trova le lib interne.
    /// Fallback: `&lt;root&gt;/libs` per un checkout del solo GMBasic.</summary>
    public static string RepoLibsDir
    {
        get
        {
            try
            {
                var shared = Path.GetFullPath(Path.Combine(RepoRoot, "..", "shared", "libs"));
                if (Directory.Exists(shared)) return shared;
            }
            catch { }
            var local = Path.Combine(RepoRoot, "libs");
            return Directory.Exists(local) ? local : Path.Combine(RepoRoot, "..", "shared", "libs");
        }
    }

    /// <summary>Cartella `shared/settings/` CONDIVISA dai due mondi (C e C#): UNA sola, nel
    /// submodule `shared`. Contiene `system.settings.json` e gli indici (`libs-manifest.json` e i
    /// manifest dei componenti ml/chromium/obscura): roba di TESTO, non asset — i font e le
    /// immagini stanno in `Resources/`.
    /// Ordine: &lt;RepoRoot&gt;/../shared/settings → &lt;RepoRoot&gt;/shared/settings → &lt;exe&gt;/config → CWD.
    /// Il checkout vince sull'output: la copia in &lt;exe&gt;/config è generata dalla build e può
    /// essere più vecchia del canonico.</summary>
    public static string SharedConfigDir => _sharedConfig.Value;

    private static readonly Lazy<string> _sharedConfig = new(() =>
    {
        var cands = new List<string>();
        // Il CHECKOUT vince sull'output (una copia generata dalla build può essere più vecchia
        // del canonico: bug visto, `libs status` leggeva un indice stantio). MA i percorsi del
        // repo valgono SOLO in un checkout: in un'app installata `RepoRoot` ripiega sulla dir
        // dell'eseguibile, e `<exe>/../shared/settings` sarebbe una cartella a caso del vicino.
        var checkout = false;
        try { checkout = IsSourceCheckout(RepoRoot); } catch { }
        if (checkout)
        {
            try { cands.Add(Path.GetFullPath(Path.Combine(RepoRoot, "..", "shared", "settings"))); } catch { }
            try { cands.Add(Path.Combine(RepoRoot, "shared", "settings")); } catch { }
        }
        try { cands.Add(Path.Combine(AppContext.BaseDirectory, "config")); } catch { }
        try { cands.Add(Path.Combine(Directory.GetCurrentDirectory(), "config")); } catch { }
        foreach (var c in cands)
        {
            try { if (Directory.Exists(c) && File.Exists(Path.Combine(c, "system.settings.json"))) return c; } catch { }
        }
        // Nessuna cartella trovata: percorso PREVEDIBILE per il messaggio d'errore — quello del
        // repo in un checkout, quello dell'eseguibile in un'app installata.
        try
        {
            return checkout
                ? Path.GetFullPath(Path.Combine(RepoRoot, "..", "shared", "settings"))
                : Path.Combine(AppContext.BaseDirectory, "config");
        }
        catch { return "shared/settings"; }
    });

    /// <summary>Radice dei SAMPLE dei pacchetti installati (`<root>/samples`, dove il C crea
    /// `packages/<nome>@<versione>/`). In un checkout è **`<monorepo>/shared/samples`** (i sample del
    /// monorepo vivono lì: scriverli in `<RepoRoot>/samples` creava una seconda `samples/` dentro
    /// GMBasic). In un'app installata resta `<RepoRoot>/samples`.</summary>
    public static string SharedSamplesRoot => _sharedSamplesRoot.Value;

    private static readonly Lazy<string> _sharedSamplesRoot = new(() =>
    {
        try
        {
            if (IsSourceCheckout(RepoRoot))
            {
                var shared = Path.GetFullPath(Path.Combine(RepoRoot, "..", "shared"));
                if (Directory.Exists(Path.Combine(shared, "samples"))) return shared;
            }
        }
        catch { }
        return RepoRoot;
    });

    /// <summary>Cartella `Resources/` CONDIVISA dai due mondi (C e C#): UNA sola, alla radice del
    /// monorepo. Contiene gli asset che servono a entrambi — font (`font.ttf`, SFMono, C64 per la
    /// console a celle), sprite/PNG, `system.settings.json`, i manifest dei componenti. Prima erano
    /// in `GMBasicSharp/Resources/` (ora rimossa) e il runtime C li cercava a mano con `<repo>/Resources`.
    /// Ordine di ricerca (il primo che esiste vince):
    ///   1. &lt;RepoRoot&gt;/../Resources     monorepo (GMBasic + GMBasicSharp) — il CHECKOUT vince
    ///   2. &lt;RepoRoot&gt;/Resources        checkout del solo GMBasic
    ///   3. &lt;exe&gt;/Resources             app installata (la build li copia lì)
    ///   4. &lt;CWD&gt;/Resources
    /// Se nessuna esiste si restituisce la 3: un percorso prevedibile per l'errore.</summary>
    public static string SharedResourcesDir => _sharedResources.Value;

    private static readonly Lazy<string> _sharedResources = new(() =>
    {
        var cands = new List<string>();
        // Checkout prima dell'output (la copia in <exe>/Resources è generata dalla build), ma i
        // percorsi del repo valgono SOLO in un checkout: in un'app installata finirebbero su una
        // cartella del vicino (`<exe>/../Resources`). L'eseguibile viene comunque prima del CWD.
        var checkout = false;
        try { checkout = IsSourceCheckout(RepoRoot); } catch { }
        if (checkout)
        {
            try { cands.Add(Path.GetFullPath(Path.Combine(RepoRoot, "..", "Resources"))); } catch { }
            try { cands.Add(Path.Combine(RepoRoot, "Resources")); } catch { }
        }
        try { cands.Add(Path.Combine(AppContext.BaseDirectory, "Resources")); } catch { }
        try { cands.Add(Path.Combine(Directory.GetCurrentDirectory(), "Resources")); } catch { }
        foreach (var c in cands)
        {
            try { if (Directory.Exists(c) && File.Exists(Path.Combine(c, "font.ttf"))) return Path.GetFullPath(c); } catch { }
        }
        try { return Path.GetFullPath(cands[0]); } catch { return "Resources"; }
    });

    /// <summary>Cartella delle librerie BUILTIN per piattaforma (&lt;root&gt;/Resources/libs/&lt;rid&gt;/):
    /// lib di terze parti nostre consolidate (hdf5, SDL, pdf, crypto, …) + eventuali builtin.</summary>
    // NB: `Resources/libs/<rid>` NON esiste più (Resources/ contiene solo font e immagini: le
    // librerie stanno in `shared/libs/` nel checkout e nella cache `~/.gmbasic/libs/<rid>/`).
    // Non reintrodurre un candidato che punti lì: verrebbe scelto PRIMA della cache e
    // riporterebbe in vita il bug "libreria stale" (vedi Bugs/(open) - Lib-AI-Pubblicate-Stale-Cache).

    // ========================================================================
    // Componenti runtime scaricati (dentro LibsDir/<componente>/<rid>)
    // ========================================================================

    /// <summary>Cartella di un COMPONENTE: sempre e solo la cache utente
    /// (`&lt;libsDir&gt;/&lt;comp&gt;/&lt;rid&gt;`). Il vecchio ripiego su `&lt;root&gt;/libs-runtime` è stato
    /// rimosso insieme alla cartella: una sola posizione, niente seconde copie.</summary>
    private static string ComponentDir(string comp, string rid) => Path.Combine(LibsDir, comp, rid);

    /// <summary>Cache TORCH (libtorch CPU ~2GB): &lt;libsDir&gt;/ml/&lt;rid&gt;.</summary>
    public static string MlDir(string rid) => ComponentDir("ml", rid);

    /// <summary>Cache BROWSER (Chromium): &lt;libsDir&gt;/chromium/&lt;rid&gt;.</summary>
    public static string ChromiumDir(string rid) => ComponentDir("chromium", rid);

    /// <summary>Cache BROWSER (Obscura, Rust+V8, headless agente): &lt;libsDir&gt;/obscura/&lt;rid&gt;.</summary>
    public static string ObscuraDir(string rid) => ComponentDir("obscura", rid);

    /// <summary>Cache OCR: &lt;libsDir&gt;/ocr/&lt;rid&gt;.</summary>
    public static string OcrDir(string rid) => ComponentDir("ocr", rid);

    /// <summary>Cartella cache/temporanea (kv-swap, clone marketplace, staging download).
    /// Default: System.IO.Path.GetTempPath() — il SO la pulisce.</summary>
    public static string TempDir => Current.TempDir;

    // ========================================================================
    // File di stato/config (dentro ConfigDir)
    // ========================================================================

    /// <summary>ai.json — config AI unificata (ex ~/.gmbasic/ai.json).</summary>
    public static string AiConfigFile => Path.Combine(ConfigDir, "ai.json");

    /// <summary>providers.json — provider AI configurati (ex ~/.gmbasic/providers.json).</summary>
    public static string ProvidersFile => Path.Combine(ConfigDir, "providers.json");

    /// <summary>mcp.json — server MCP (ex ~/.gmbasic/mcp.json).</summary>
    public static string McpFile => Path.Combine(ConfigDir, "mcp.json");

    /// <summary>ide.settings.json — impostazioni IDE (ex ~/.gmbasic/ide.settings.json).</summary>
    public static string IdeSettingsFile => Path.Combine(ConfigDir, "ide.settings.json");

    /// <summary>vaults.db — registry vault (ex ~/.gmbasic/vaults.db).</summary>
    public static string VaultsDbFile => Path.Combine(ConfigDir, "vaults.db");

    /// <summary>brain.db — memoria persistente (ex ~/.gmbasic/brain.db).</summary>
    public static string BrainDbFile => Path.Combine(ConfigDir, "brain.db");

    /// <summary>brains.json — registry dei cervelli (ex ~/.gmbasic/brains.json).</summary>
    public static string BrainsJsonFile => Path.Combine(ConfigDir, "brains.json");

    /// <summary>recursivemas/ — team del marketplace RecursiveMAS (ex ~/.gmbasic/recursivemas).</summary>
    public static string RecursiveMasDir => Path.Combine(ConfigDir, "recursivemas");

    // ========================================================================
    // Cache (dentro TempDir — il SO la pulisce, niente file definitivi qui)
    // ========================================================================

    /// <summary>Clone dei marketplace skills (ex ~/.gmbasic/skills-marketplaces).</summary>
    public static string SkillsMarketplacesDir => Path.Combine(TempDir, "gmbasic-skills-marketplaces");

    /// <summary>Clone dei marketplace plugins (ex ~/.gmbasic/plugins-marketplaces).</summary>
    public static string PluginsMarketplacesDir => Path.Combine(TempDir, "gmbasic-plugins-marketplaces");

    /// <summary>kv-swap dei provider (ex ~/.gmbasic/kv-swap). Cache temporanea: il SO la pulisce.</summary>
    public static string KvSwapDir => Path.Combine(TempDir, "gmbasic-kv-swap");

    // ========================================================================
    // Implementazione (lazy + cache)
    // ========================================================================

    private static Resolved Current
    {
        get
        {
            lock (Lock)
            {
                if (_resolved is null)
                {
                    MigrateLegacyHomeOnce();   // ~/.gmb → ~/.gmbasic (una tantum, prima di risolvere)
                    _resolved = Build();
                }
                return _resolved;
            }
        }
    }

    /// <summary>Invalida la cache (usato dai test dopo aver scritto un system.settings.json).</summary>
    public static void ResetCache() { lock (Lock) { _resolved = null; } }

    private static Resolved Build()
    {
        var root = DiscoverRepoRoot();
        // Se un test ha già isolato EnvFile (OverridePathForTests), NON caricare il .env reale:
        // passare un path esplicito qui bypasserebbe l'override (bug reale 2026-09-07).
        if (!EnvFile.HasOverride) EnvFile.EnsureLoaded(Path.Combine(root, ".env"));
        var settingsFile = LocateSettingsFile(root);
        var paths = LoadPaths(settingsFile) ?? new PathSettings();

        string Resolve(string? value, string defaultRel, string? fallbackAbsolute = null)
        {
            var v = Expand(value ?? "");
            if (string.IsNullOrWhiteSpace(v))
                v = fallbackAbsolute ?? Path.Combine(root, defaultRel);
            else if (!Path.IsPathRooted(v))
                v = Path.Combine(root, v);
            return v;
        }

        /* skills/ e plugins/ sono CONDIVISI dai due mondi: nel monorepo la copia UNICA sta in
         * `<root>/../shared/<nome>`. Un checkout del solo GMBasic (o un'app installata) non ha
         * `shared/`: lì si ricade su `<root>/<nome>`. Un valore esplicito in paths.* vince. */
        string ResolveShared(string? value, string defaultRel)
        {
            if (!string.IsNullOrWhiteSpace(Expand(value ?? ""))) return Resolve(value, defaultRel);
            var shared = Path.GetFullPath(Path.Combine(root, "..", "shared", defaultRel));
            return Directory.Exists(shared) ? shared : Resolve(value, defaultRel);
        }

        var configDir = _configDirOverride ?? Resolve(paths.ConfigDir, "config");
        var tempDir = string.IsNullOrWhiteSpace(Expand(paths.TempDir ?? ""))
            ? Path.GetTempPath()
            : Resolve(paths.TempDir, "tmp");

        /* PACCHETTI: UNA regola, due macchine (identica a gmb_paths_packages_dir nel runtime C).
         *   1. `packagesDir` configurato → vince sempre;
         *   2. CHECKOUT SORGENTE (sviluppo, compilatore presente) → <root>/packages e, nel
         *      monorepo, <root>/../packages (i pacchetti stanno alla radice del monorepo);
         *   3. CLIENT (app installata, nessun compilatore/checkout) → ~/.gmbasic/packages.
         * Senza il punto 3 il client scriveva in <dir dell'eseguibile>/packages (o nel CWD):
         * i pacchetti sparivano a ogni aggiornamento dell'app e non erano quelli del PM. */
        var packagesDir = PackagesDirFor(root, paths.PackagesDir);

        /* CACHE LIBRERIE: una sola radice utente (`~/.gmbasic/libs`). */
        var libsCacheRoot = string.IsNullOrWhiteSpace(Expand(paths.LibsCacheDir ?? ""))
            ? Path.Combine(Home(), ".gmbasic", "libs")
            : Resolve(paths.LibsCacheDir, ".gmbasic/libs");

        return new Resolved(
            RepoRoot: root,
            SettingsFile: settingsFile,
            Paths: paths,
            ConfigDir: configDir,
            SkillsDir: ResolveShared(paths.SkillsDir, "skills"),
            PluginsDir: ResolveShared(paths.PluginsDir, "plugins"),
            LogsDir: Resolve(paths.LogsDir, "ai-logs"),
            RagDir: ResolveShared(paths.RagDir, "rag"),
            GraphsDir: ResolveShared(paths.GraphsDir, "graphs"),
            BrainsDir: Resolve(paths.BrainsDir, "brains"),
            BrainsExportsDir: Resolve(paths.BrainsExportsDir, "brains-exports"),
            // Componenti (torch/chromium/obscura/ocr) nella CACHE UTENTE, accanto alle librerie:
            // <cache>/<componente>/<rid>/. Non più nel checkout (`libs-runtime/`), che era una
            // SECONDA sorgente di verità da 928 MB e non viaggia col client.
            LibsDir: Resolve(paths.LibsDir, "libs", libsCacheRoot),
            LibsCacheRoot: libsCacheRoot,
            PackagesDir: packagesDir,
            TempDir: tempDir);
    }

    /// <summary>Cartella dei PACCHETTI LOCALI dell'applicazione: `<dir dell'eseguibile>/packages`
    /// (modello `node_modules`). La crea l'installer (`gmb pkg install --local`); nella ricerca ha
    /// la PRECEDENZA sulla cache. Per le gambe interpretate è la stessa cartella del compilato:
    /// il runner (che è l'eseguibile dell'app distribuita) la trova accanto a sé.</summary>
    public static string LocalPackagesDir => Path.Combine(FileSystemUtils.CoreAppPath(), "packages");

    /// <summary>Ordine di ricerca dei pacchetti (il primo che soddisfa il vincolo vince):
    /// 1. installazione LOCALE dell'app &lt;dir dell'eseguibile&gt;/packages
    /// 2. cache condivisa (checkout: &lt;root&gt;/packages · client: ~/.gmbasic/packages)</summary>
    public static IReadOnlyList<string> PackagesSearchPath()
    {
        var local = LocalPackagesDir;
        var cache = PackagesDir;
        return string.Equals(Path.GetFullPath(local), Path.GetFullPath(cache), StringComparison.OrdinalIgnoreCase)
            ? new[] { cache }
            : new[] { local, cache };
    }

    /// <summary>Trova la root repo: risale da AppContext.BaseDirectory finché trova GMBASIC.sln;
    /// fallback: CoreAppPath().</summary>
    public static string DiscoverRepoRoot()
    {
        var dir = AppContext.BaseDirectory;
        for (var i = 0; i < 10 && dir != null; i++, dir = Path.GetDirectoryName(dir))
        {
            if (File.Exists(Path.Combine(dir, "GMBASIC.sln"))) return dir;
            // Monorepo: GMBasicSharp/GMBasicSharp.slnx -> root GMBasic (Resources, libs-runtime, config).
            if (File.Exists(Path.Combine(dir, "GMBasicSharp.slnx")))
            {
                var gm = Path.Combine(Path.GetDirectoryName(dir) ?? dir, "GMBasic");
                if (Directory.Exists(gm)) return gm;
            }
            // Checkout C-only del monorepo (nessuna .sln): marker stabile del runtime condiviso
            // (stessa regola di ir_walk_sln nel frontend C).
            if (File.Exists(Path.Combine(dir, "native", "gmb_runtime_c", "include", "gmb_runtime.h"))) return dir;
        }
        return FileSystemUtils.CoreAppPath();
    }

    /// <summary>1 se `root` è un CHECKOUT SORGENTE (macchina di sviluppo: compilatore presente).
    /// Stessa condizione di <see cref="DiscoverRepoRoot"/> e di ir_walk_sln (C).</summary>
    public static bool IsSourceCheckout(string root)
    {
        if (string.IsNullOrWhiteSpace(root)) return false;
        return File.Exists(Path.Combine(root, "GMBASIC.sln"))
            || File.Exists(Path.Combine(root, "GMBasicSharp.slnx"))
            || File.Exists(Path.Combine(root, "native", "gmb_runtime_c", "include", "gmb_runtime.h"));
    }

    /// <summary>Dove stanno i pacchetti. UNA regola, identica a gmb_paths_packages_dir (C):
    ///  1. `configured` (paths.packagesDir) → vince sempre;
    ///  2. CHECKOUT SORGENTE → &lt;root&gt;/packages, poi &lt;root&gt;/../packages (monorepo);
    ///  3. CLIENT (nessun checkout: app installata, senza compilatore) → ~/.gmbasic/packages.
    /// Il caso 3 è quello che mancava: il client scriveva accanto all'eseguibile.</summary>
    public static string PackagesDirFor(string root, string? configured)
    {
        if (!string.IsNullOrWhiteSpace(configured))
        {
            var v = configured!.Trim();
            if (v.StartsWith("~"))
            {
                var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                v = v.Length > 1 ? Path.Combine(home, v[2..]) : home;
            }
            return Path.IsPathRooted(v) ? v : Path.Combine(root, v);
        }
        if (IsSourceCheckout(root))
        {
            var local = Path.Combine(root, "packages");
            if (Directory.Exists(local)) return local;
            var parent = Path.Combine(Path.GetDirectoryName(root.TrimEnd(Path.DirectorySeparatorChar)) ?? root, "packages");
            if (Directory.Exists(parent)) return parent;
            return local;   // checkout senza cache popolata: <root>/packages (la crea l'install)
        }
        var homeDir = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        return string.IsNullOrEmpty(homeDir)
            ? Path.Combine(root, "packages")
            : Path.Combine(homeDir, ".gmbasic", "packages");
    }

    /// <summary>Cerca system.settings.json: &lt;root&gt;/config → &lt;root&gt;/Resources → eseguibile
    /// (system.settings.json e Assets/Config). Ritorna il primo esistente, o il path canonico
    /// &lt;root&gt;/config/system.settings.json se nessuno esiste.</summary>
    public static string LocateSettingsFile(string root)
    {
        /* Configurazione ≠ asset: `system.settings.json` è CONDIVISO dai due compilatori e vive
         * nella `config/` alla radice del monorepo. Quella comune viene PRIMA: una copia locale
         * stantia in `<root>/config/` non può più scavalcare quella versionata (era il caso
         * reale: due file divergenti e si usava quello sbagliato). */
        var candidates = new[]
        {
            Path.GetFullPath(Path.Combine(root, "..", "shared", "settings", "system.settings.json")), // CONFIG COMUNE
            Path.Combine(root, "shared", "settings", "system.settings.json"),                          // fallback: checkout singolo
        };
        foreach (var c in candidates)
            if (File.Exists(c)) return c;
        var exe = FileSystemUtils.CoreAppPath();
        var exeCandidates = new[]
        {
            Path.Combine(exe, "config", "system.settings.json"),
            Path.Combine(exe, "system.settings.json"),
            Path.Combine(exe, "Assets", "Config", "system.settings.json"),
        };
        foreach (var c in exeCandidates)
            if (File.Exists(c)) return c;
        return candidates[0];
    }

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    /// <summary>`app.settings.json`: accanto all'eseguibile (compilato distribuito) o nella CWD
    /// (sviluppo). Contiene le sezioni di RUNTIME unite dal compilatore (`ProgramBundle`).</summary>
    private static string? AppSettingsPath()
    {
        try
        {
            var exe = Path.Combine(AppContext.BaseDirectory, "app.settings.json");
            if (File.Exists(exe)) return exe;
        }
        catch { }
        try
        {
            var cwd = Path.Combine(Directory.GetCurrentDirectory(), "app.settings.json");
            if (File.Exists(cwd)) return cwd;
        }
        catch { }
        return null;
    }

    private static PathSettings? LoadPaths(string file)
    {
        var sys = ParsePaths(file);
        /* Un compilato distribuito NON porta `system.settings.json` (è del compilatore): i percorsi
         * che gli servono stanno nel suo `app.settings.json`, che il build ha già unito alle
         * sezioni di runtime dei settings condivisi. Quando invece il file globale ESISTE (checkout,
         * app installata) valgono i suoi percorsi: un `app.settings.json` trovato per strada non
         * deve cambiare le cartelle dell'applicazione. */
        if (sys is null) return ParsePaths(AppSettingsPath());
        return sys;
    }

    private static PathSettings? ParsePaths(string? file)
    {
        if (string.IsNullOrWhiteSpace(file) || !File.Exists(file)) return null;
        try
        {
            var json = File.ReadAllText(file);
            var sys = JsonSerializer.Deserialize<SystemSettings>(json, JsonOpts);
            return sys?.Paths;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>Espande ~ e variabili d'ambiente (riuso leggero, senza dipendenze dal layer AI).</summary>
    public static string Expand(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return path;
        var p = path.Trim();
        if (p == "~") return Home();
        if (p.StartsWith("~/", StringComparison.Ordinal) || p.StartsWith("~\\", StringComparison.Ordinal))
            p = Path.Combine(Home(), p[2..]);
        p = Environment.ExpandEnvironmentVariables(p);
        return p;
    }

    private static string Home() =>
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
}
