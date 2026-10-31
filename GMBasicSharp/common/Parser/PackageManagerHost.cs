// FILE: Interpreter.Parser.PackageManager.cs
// PackageManager v2 — intelligente, multi-backend + FFI (GMBasic).
//
// Gestisce tutti i casi della tassonomia gmb-package.json v2:
//   contentType: source | dotnet | c-ffi | mixed | docs-only
//   backends  : ["il","c99","vm"]  (per cosa è già precompilato; [] = source-only)
//   edition   : Community | Commercial | any
//   binaries  : backend -> platform -> path  (v1: platform -> path, normalizzato)
//   ffi       : { api, headers, libs, link, declareExample }  (c-abi | gmb-native)
//   settings  : { requiresCommercial, minGmbVersion }
//
// Il resolver SCEGLIE l'artefatto in base al contesto di build corrente
// (backend + platform + edition), NON scarica sempre e ricompila. Retrocompatibile
// con v1 (binaries flat {platform: path} → backend "any").

using System.Collections.Concurrent;
using System.Collections.Immutable;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic;

/// <summary>Tipologia di contenuto di un pacchetto.</summary>
public sealed partial class Parser
{
    /// <summary>
    /// PackageManager v2 — risolve le dipendenze REQUIRE con negoziazione
    /// backend/platform/edition e selezione artefatto (binaries vs exports vs ffi).
    /// </summary>
    public sealed class PackageManager
    {
        private readonly ConcurrentDictionary<string, InstalledPackage> _resolvedPackages =
            new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<string, PackageArtifact> _resolvedArtifacts =
            new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<string, string> _lockData =
            new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _visiting = new(StringComparer.OrdinalIgnoreCase);

        /// <summary>URL del registry di default. Override con env GMB_REGISTRY.</summary>
        public string RegistryUrl { get; set; } = Basic.Providers.GmbEnv.Get("GMB_REGISTRY") ?? "http://localhost:8087";

        /// <summary>Cartella cache/install dei pacchetti (stile node_modules). Default:
        /// &lt;root&gt;/packages (GmbiPaths.PackagesDir) — TUTTO dentro GMBasic, niente ~/.gmb.
        /// Override per test/progetto (es. flag --cache del CLI).</summary>
        public string CacheRoot { get; set; } = CommonLib.GmbiPaths.PackagesDir;

        /// <summary>Blob store content-addressable della cache (dedup dei file).
        /// Default: &lt;CacheRoot&gt;/.content. L'installer che scrive in una cartella LOCALE
        /// (`--local`) punta questo sulla cache vera, per non creare un blob store nell'app.</summary>
        public string? ContentRootOverride { get; set; }
        public string ContentRoot => ContentRootOverride ?? Path.Combine(CacheRoot, CacheStore.DirName);

        /// <summary>Multi-registry ordinati per priorità (Official → Company → Local).</summary>
        public List<string> RegistryUrls { get; set; } = [];

        /// <summary>Versione dell'engine GMBasic (gate manifest settings.minGmbVersion).</summary>
        public const string EngineVersionString = "1.0.0";

        /// <summary>Versione engine parsata (SemVer).</summary>
        public static readonly SemVer EngineVersion = SemVer.Parse(EngineVersionString)!;

        /// <summary>
        /// Gate di compatibilità di un manifest col contesto corrente: `settings.minGmbVersion`
        /// (engine troppo vecchio), `settings.requiresCommercial`/`edition` (edizione non combacia).
        /// Lancia <see cref="PackageException"/> se il pacchetto non è utilizzabile qui.
        /// </summary>
        public static void CheckCompatibility(PackageManifest m, PackageBuildContext ctx)
        {
            // Gate DELEGATO al riferimento C (gmb_pkg_compat_check): minGmbVersion/requiresCommercial/edition.
            var json = System.Text.Json.JsonSerializer.Serialize(new
            {
                name = m.Name,
                version = m.Version,
                edition = m.Edition,
                settings = m.Settings == null ? null
                    : new { minGmbVersion = m.Settings.MinGmbVersion, requiresCommercial = m.Settings.RequiresCommercial },
            });
            var err = NativePkg.CompatCheck(json, ctx.Edition);
            if (err.Length > 0) throw new PackageException(err);
        }

        /// <summary>Ottiene i percorsi alle directory dei moduli sorgente (.bas) esportati (delegato al C).</summary>
        public IEnumerable<string> ResolvedModuleSourcePaths => _resolvedPackages.Values
            .SelectMany(p => NativePkg.ModuleSources(p.InstallPath))
            .Distinct();

        /// <summary>Ottiene i percorsi alle directory delle librerie binarie scelte per la piattaforma (delegato al C).</summary>
        public IEnumerable<string> ResolvedBinaryPaths
        {
            get
            {
                var platformKey = GetCurrentPlatformKey();
                return _resolvedPackages.Values
                    .Select(p => NativePkg.BinaryPath(p.InstallPath, "any", platformKey))
                    .Where(path => path is not null)
                    .Select(path => Path.GetDirectoryName(path!))
                    .Where(dir => dir is not null)
                    .Select(dir => dir!)
                    .Distinct();
            }
        }

        /// <summary>Artefatti scelti per il contesto di build (per nome pacchetto).</summary>
        public IReadOnlyDictionary<string, PackageArtifact> ResolvedArtifacts => _resolvedArtifacts;

        /// <summary>Pacchetti risolti (nome → manifest + install path) — per la verifica firma/integrità.</summary>
        public IReadOnlyDictionary<string, InstalledPackage> ResolvedPackages => _resolvedPackages;

        /// <summary>Vincoli di versione finali (per lock file).</summary>
        public IReadOnlyDictionary<string, string> ResolvedConstraints { get; private set; } = new Dictionary<string, string>();

        /// <summary>Dati per il lock file (pacchetto → riga lock).</summary>
        public IReadOnlyDictionary<string, string> LockData => _lockData;

        public PackageManager()
        {
            RegistryUrls.Add(RegistryUrl);
        }

        /// <summary>Parsa una versione SemVer 2.0 (es. "1.2.3-rc.1+b5").</summary>
        public static SemVer? ParseSemVer(string version) => SemVer.Parse(version);

        /// <summary>Chiave piattaforma corrente (es. "linux-x64", "osx-arm64").</summary>
        /// <summary>Chiave piattaforma corrente (delegato al C: gmb_pkg_platform_key).</summary>
        public static string GetCurrentPlatformKey() => NativePkg.PlatformKey();

        /// <summary>Path del chunk vm standalone (A1/GMBC .chunk/.prg) di un pacchetto, o null se
        /// non ha un binario vm (PackageManager-BSP §1: eseguire il pacchetto come PROGRAMMA).
        /// Selezione DELEGATA al C (gmb_pkg_binary_path).</summary>
        public string? ResolveVmChunkPath(InstalledPackage p)
        {
            return NativePkg.BinaryPath(p.InstallPath, "vm", GetCurrentPlatformKey());
        }

        /// <summary>Seleziona l'artefatto per un pacchetto nel contesto di build.</summary>
        private PackageArtifact SelectArtifact(InstalledPackage p, PackageBuildContext ctx)
        {
            var platformKey = string.IsNullOrEmpty(ctx.PlatformKey) ? GetCurrentPlatformKey() : ctx.PlatformKey;
            var backend = ctx.Backend.ToLowerInvariant();
            // Selezione artefatto + classificazione contentType DELEGATE al riferimento C
            // (gmb_pkg_artifact_select). Il C# materializza solo l'oggetto gestito.
            var d = NativePkg.ArtifactSelect(p.InstallPath, backend, platformKey);
            return new PackageArtifact
            {
                PackageName = p.Manifest.Name,
                Version = p.Manifest.Version,
                InstallPath = p.InstallPath,
                ContentType = d == null ? p.Manifest.ContentType : (PackageContentType)d.ContentType,
                SourceDir = d?.SourceDir,
                BinaryPath = d?.Binary,
                ChunkPath = d?.Chunk,
                FfiLibs = d?.FfiLibs ?? [],
                FfiHeaders = d?.FfiHeaders ?? [],
                FfiLink = d?.FfiLink,
                DeclareExample = d?.Declare,
                Backend = d?.Backend ?? "any",
                Platform = d?.Platform ?? platformKey,
            };
        }

        /// <summary>
        /// Risolve le dipendenze DELEGANDO al riferimento C (gmb_pkg_resolve): BFS transitivi,
        /// vincoli multipli, overrides, pin (--locked), pacchetti locali (workspace), features,
        /// cache+vendor, dist-tag e download dal registry. Il C# materializza gli artefatti e gli
        /// step non-puri (source/build/FFI/samples) a partire dall'installDir risolto dal C.
        /// </summary>
        public async Task ResolveDependenciesAsync(IEnumerable<RequireStmt> requirements, PackageBuildContext? ctx = null, string? lockFile = null)
        {
            var buildCtx = ctx ?? new PackageBuildContext();
            var reqs = requirements.Select(r => (r.PackageName.Value, r.VersionSpecifier?.Value ?? "*")).ToList();
            var locked = new List<(string, string)>();
            if (lockFile != null && File.Exists(lockFile))
                foreach (var (n, v) in ReadLockFile(lockFile)) locked.Add((n, v));
            var local = buildCtx.LocalPackages.Select(kv => (kv.Key, kv.Value)).ToList();
            var overrides = buildCtx.Overrides.Select(kv => (kv.Key, kv.Value)).ToList();
            var features = buildCtx.Features.ToList();
            var platformKey = string.IsNullOrEmpty(buildCtx.PlatformKey) ? GetCurrentPlatformKey() : buildCtx.PlatformKey;

            var (items, error) = NativePkg.Resolve(
                CacheRoot, buildCtx.VendorDir, buildCtx.Backend, platformKey, buildCtx.Edition, buildCtx.Offline, RegistryUrl.TrimEnd('/'),
                reqs, overrides, features, locked, local,
                buildCtx.Registries.Select(kv => (kv.Key, kv.Value)),
                CommonLib.GmbiPaths.LocalPackagesDir);   /* locale dell'app PRIMA della cache */
            if (error != null)
                throw new BasicException(ErrorCode.FileNotFound, SourcePosition.Unknown, error);

            _resolvedPackages.Clear();
            _resolvedArtifacts.Clear();
            _lockData.Clear();
            var finalResolvedConstraints = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var g in reqs.GroupBy(r => r.Item1, StringComparer.OrdinalIgnoreCase))
                finalResolvedConstraints[g.Key] = string.Join(" & ", g.Select(x => x.Item2).Distinct());

            foreach (var it in items)
            {
                var manifest = await ReadManifestFromDirAsync(it.InstallDir);
                CheckCompatibility(manifest, buildCtx);
                var installed = new InstalledPackage(manifest, it.InstallDir);
                await EnsureSourceAsync(installed, buildCtx);   // materializza la sorgente esterna se dichiarata
                await EnsureBuildAsync(installed, buildCtx);    // client-build: compila il C se manca il precompilato
                CopySamplesToProject(manifest, it.InstallDir);
                _resolvedPackages[it.Name] = installed;
                _resolvedArtifacts[it.Name] = SelectArtifact(installed, buildCtx);
                _lockData[it.Name] = it.LockEntry;
            }

            _visiting.Clear();
            ResolvedConstraints = finalResolvedConstraints;
        }

        /// <summary>Scrive gmb-package.lock (con sha256 per-file) nella cartella del progetto.
        /// Se il contenuto è CAMBIATO rispetto al lock esistente, ne archivia una copia in
        /// &lt;progetto&gt;/.gmbasic/lock-history/ (per `gmb rollback`).</summary>
        public void WriteLockFile(string projectDir)
        {
            if (string.IsNullOrEmpty(projectDir)) return;
            var lockPath = Path.Combine(projectDir, "gmb-package.lock");
            var dataLines = _lockData.Values.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
            // Scrittura canonica + archivio storico lock DELEGATI al C (gmb_pkg_lock_write_with_history).
            NativePkg.LockWriteWithHistory(projectDir, lockPath, string.Join('\n', dataLines));
        }

        /// <summary>Legge il lock con versioni E hash per-file: nome → (versione, {file:sha256}).</summary>
        public static Dictionary<string, (string Version, Dictionary<string, string> Hashes)> ReadLockFileDetailed(string lockPath)
        {
            var result = new Dictionary<string, (string, Dictionary<string, string>)>(StringComparer.OrdinalIgnoreCase);
            var l = NativePkg.LockRead(lockPath); // parse delegato al riferimento C
            if (l == IntPtr.Zero) return result;
            try
            {
                var n = NativePkg.LockCount(l);
                for (var i = 0; i < n; i++)
                    if (NativePkg.LockEntry(l, i, out var name, out var ver, out _, out _, out _))
                    {
                        var hashes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                        var nh = NativePkg.LockHashCount(l, i);
                        for (var j = 0; j < nh; j++)
                            if (NativePkg.LockHash(l, i, j, out var file, out var hash)) hashes[file] = hash;
                        result[name] = (ver, hashes);
                    }
            }
            finally { NativePkg.LockFree(l); }
            return result;
        }

        /// <summary>
        /// Verifica l'integrità REALE dei pacchetti in cache contro gli hash registrati nel lock file
        /// (tamper-detection: confronta i file in cache con quelli firmati nel lock, non con se stesso).
        /// Ritorna i problemi per pacchetto; vuoto = integro. I pacchetti non in cache vengono saltati.
        /// </summary>
        public Dictionary<string, List<string>> VerifyLockedIntegrity(string lockPath, PackageBuildContext? ctx = null)
        {
            _ = ctx;
            var issues = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            var l = NativePkg.LockRead(lockPath); // verifica delegata al riferimento C
            if (l == IntPtr.Zero) return issues;
            try
            {
                NativePkg.LockVerify(l, CacheRoot, out var report);
                foreach (var line in report.Split('\n', StringSplitOptions.RemoveEmptyEntries))
                {
                    var idx = line.IndexOf(": ", StringComparison.Ordinal);
                    if (idx <= 0) continue;
                    var name = line[..idx];
                    if (!issues.TryGetValue(name, out var list)) issues[name] = list = [];
                    list.Add(line[(idx + 2)..]);
                }
            }
            finally { NativePkg.LockFree(l); }
            return issues;
        }

        /// <summary>Chiave pubblica con cui un pacchetto installato è firmato (detached .gmbpkg.pub o
        /// sezione `signature.publicKey` del manifest). Null se non firmato.</summary>
        public static string? ReadInstalledPublicKey(PackageManifest m, string installDir)
        {
            _ = m; // la lettura (.gmbpkg.pub + fallback signature.publicKey) e' delegata al C
            return NativePkg.InstalledPublicKey(installDir);
        }

        /// <summary>
        /// Verifica di fiducia di un pacchetto installato: firma crittografica + chiave contro il keyring
        /// (<see cref="TrustStore"/>). Con <see cref="TrustPolicy.Tofu"/> la prima chiave vista viene
        /// registrata; un cambio di chiave segnala <see cref="TrustResult.KeyChanged"/>.
        /// </summary>
        public static (TrustResult Result, string? Signer, string Message) VerifyInstalledTrust(
            PackageManifest m, string installDir, string packageName, TrustPolicy policy)
        {
            var pub = ReadInstalledPublicKey(m, installDir);
            if (pub == null)
            {
                var msg = policy == TrustPolicy.Strict
                    ? $"'{packageName}' NON firmato (richiesta firma valida)."
                    : $"'{packageName}' non firmato.";
                return (policy == TrustPolicy.Strict ? TrustResult.Untrusted : TrustResult.Unsigned, null, msg);
            }
            if (!PackageBuilder.VerifyInstalledSignature(m, installDir))
                return (TrustResult.Invalid, TrustStore.Fingerprint(pub), $"firma di '{packageName}' NON valida!");

            var fp = TrustStore.Fingerprint(pub);
            var trusted = TrustStore.Get(packageName);
            if (trusted == null)
            {
                if (policy == TrustPolicy.Strict)
                    return (TrustResult.Untrusted, fp, $"chiave di '{packageName}' non fidata ({fp}); usa 'gmb trust add {packageName} <pub>'.");
                if (policy == TrustPolicy.Tofu)
                {
                    TrustStore.Add(packageName, pub);
                    return (TrustResult.FirstUse, fp, $"chiave di '{packageName}' registrata (TOFU): {fp}");
                }
                return (TrustResult.FirstUse, fp, $"chiave di '{packageName}': {fp} (non registrata)");
            }
            if (!trusted.Trim().Equals(pub.Trim(), StringComparison.Ordinal))
                return (TrustResult.KeyChanged, fp,
                    $"la chiave di '{packageName}' è CAMBIATA (attesa {TrustStore.Fingerprint(trusted)}, trovata {fp}).");
            return (TrustResult.Trusted, fp, $"'{packageName}' firmato da chiave fidata {fp}.");
        }

        /// <summary>Dipendenze abilitate dalle features del manifest (default + richieste dal ctx).</summary>
        /// <summary>Legge gmb-package.lock → { nome → versione }.</summary>
        public static Dictionary<string, string> ReadLockFile(string lockPath)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var (name, entry) in ReadLockFileDetailed(lockPath)) result[name] = entry.Version;
            return result;
        }

        /// <summary>Albero delle dipendenze dei pacchetti risolti (parent → figli diretti).</summary>
        public IReadOnlyDictionary<string, string[]> GetDependencyTree()
        {
            var names = _resolvedPackages.Keys.ToArray();
            var dirs = names.Select(n => _resolvedPackages[n].InstallPath).ToArray();
            return NativePkg.DepTree(names, dirs); // C (gmb_pkg_dep_tree)
        }

        /// <summary>Peer dependency non soddisfatte dai pacchetti risolti (avvisi, non errori).</summary>
        public List<string> CheckPeers()
        {
            var names = _resolvedPackages.Keys.ToArray();
            var dirs = names.Select(n => _resolvedPackages[n].InstallPath).ToArray();
            return NativePkg.CheckPeersList(names, dirs); // C (gmb_pkg_check_peers)
        }

        /// <summary>Versioni installate superate (registry ha una versione più alta che soddisfa i vincoli).</summary>
        public Task<Dictionary<string, (string Installed, string Latest)>> GetOutdatedAsync(PackageBuildContext? ctx = null)
            => GetOutdatedAsync(null, ctx);

        /// <summary>Versioni superate. Con <paramref name="constraints"/> il target e' la piu' alta che
        /// SODDISFA i vincoli del progetto (via <see cref="FindBestMatch"/>); senza, l'ultima assoluta.
        /// Richiede una resolve preventiva (popola <c>_resolvedPackages</c>).</summary>
        public async Task<Dictionary<string, (string Installed, string Latest)>> GetOutdatedAsync(
            IReadOnlyDictionary<string, List<string>>? constraints, PackageBuildContext? ctx = null)
        {
            var result = new Dictionary<string, (string, string)>(StringComparer.OrdinalIgnoreCase);
            var buildCtx = ctx ?? new PackageBuildContext();
            foreach (var (name, pkg) in _resolvedPackages)
            {
                try
                {
                    var vlist = NativePkg.RegistryVersions(RegistryUrl.TrimEnd('/'), name); // C (gmb_pkg_registry_detail)
                    if (vlist.Count == 0) continue;
                    var available = vlist.Select(SemVer.Parse).Where(v => v != null).Select(v => v!).ToList();
                    var installed = SemVer.Parse(pkg.Manifest.Version);
                    if (installed == null) continue;
                    SemVer? target = constraints != null && constraints.TryGetValue(name, out var cons) && cons.Count > 0
                        ? SemVer.FindBestMatch(cons, available)   // rispetta i vincoli del progetto
                        : available.OrderByDescending(v => v).FirstOrDefault(); // ultima assoluta
                    if (target != null && target.CompareTo(installed) > 0)
                        result[name] = (installed.ToString(), target.ToString());
                }
                catch { /* registry non raggiungibile: salta */ }
            }
            return result;
        }

        /// <summary>Aggiorna un pacchetto alla versione più alta disponibile (scarica se serve).</summary>
        public async Task UpdatePackageAsync(string packageName, PackageBuildContext? ctx = null)
        {
            var buildCtx = ctx ?? new PackageBuildContext();
            var pkgDir = Path.Combine(CacheRoot, SafePackageDir(packageName));
            if (Directory.Exists(pkgDir))
            {
                var versions = Directory.GetDirectories(pkgDir).Select(Path.GetFileName).Where(v => v != null).ToList();
                var kept = versions.OrderByDescending(v => SemVer.Parse(v!)?.CompareTo(SemVer.Parse("0.0.0")!) ?? 0).FirstOrDefault();
                foreach (var v in versions) if (v != kept) Directory.Delete(Path.Combine(pkgDir, v!), true);
            }
            await DownloadPackageAsync(packageName, ctx: buildCtx); // senza vincolo → latest
        }

        /// <summary>Risolve un dist-tag (es. "next") alla versione, via registry. "latest" → null (SemVer già risolve).</summary>
        /// <summary>Imposta un dist-tag sul registry (POST /api/packages/{name}/tags; token dal config se non passato).</summary>
        public Task SetTagAsync(string packageName, string tag, string version, string? registryUrl = null, string? token = null)
        {
            token ??= GmbConfig.Load().Token;
            var reg = registryUrl ?? RegistryUrl;
            PostJson(reg, $"/api/packages/{packageName}/tags",
                JsonSerializer.Serialize(new { tag, version, token }), token, "Tag");
            return Task.CompletedTask;
        }

        /// <summary>Vendoring (cargo vendor): copia i pacchetti risolti in vendorDir/<name>/<version>/ (offline).</summary>
        public void Vendor(string vendorDir)
        {
            foreach (var pkg in _resolvedPackages.Values)
            {
                var dest = Path.Combine(vendorDir, SafePackageDir(pkg.Manifest.Name), pkg.Manifest.Version);
                Directory.CreateDirectory(dest);
                foreach (var f in Directory.EnumerateFiles(pkg.InstallPath, "*", SearchOption.AllDirectories))
                {
                    var rel = Path.GetRelativePath(pkg.InstallPath, f);
                    var target = Path.Combine(dest, rel);
                    Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                    File.Copy(f, target, true);
                }
            }
        }

        /// <summary>Pubblica un .gmbpkg sul registry (POST JSON base64 + token auth; token dal config se non passato).</summary>
        public async Task PublishPackageAsync(string gmbpkgPath, string? registryUrl = null, string? token = null)
            => await PublishPackageAsyncInternal(gmbpkgPath, registryUrl, token ?? GmbConfig.Load().Token);

        /// <summary>Flag di link C99 dalle librerie FFI/binaries c99 dei pacchetti risolti (per `cc`).</summary>
        public C99LinkInfo GetC99LinkInfo(PackageBuildContext? ctx = null)
        {
            var first = _resolvedArtifacts.Values.FirstOrDefault();
            var platformKey = string.IsNullOrEmpty(ctx?.PlatformKey) ? (first?.Platform ?? GetCurrentPlatformKey()) : ctx!.PlatformKey;
            var backend = (ctx?.Backend ?? (first != null && first.Backend != "any" ? first.Backend : "c99")).ToLowerInvariant();
            var names = _resolvedArtifacts.Keys.ToArray();
            var dirs = names.Select(n => _resolvedArtifacts[n].InstallPath).ToArray();
            // Flag -I/-L/-l DELEGATI al riferimento C (gmb_pkg_c99_link_info, ri-seleziona gli artefatti).
            var (inc, libDirs, libs) = NativePkg.C99LinkInfo(names, dirs, backend, platformKey);
            return new C99LinkInfo(inc, libDirs, libs);
        }

        /// <summary>POST JSON al registry — HTTP DELEGATO al C (gmb_pkg_registry_post). Corpo; lancia su non-2xx.</summary>
        private static string PostJson(string registry, string path, string payloadJson, string? token, string label)
        {
            var where = $"{registry.TrimEnd('/')}{path}";
            var ok = NativePkg.RegistryPost(where, payloadJson, token, out var status, out var body);
            if (!ok)
            {
                /* status 0 = NESSUNA risposta HTTP: il server non è in ascolto / URL sbagliato.
                 * Prima il messaggio era "Publish fallito (0): " (vuoto) e non diceva niente:
                 * la diagnosi richiedeva di indovinare. Ora dice dove e cosa controllare. */
                if (status == 0)
                    throw new PackageException(
                        $"{label} fallito: registry NON RAGGIUNGIBILE su {where}. " +
                        "Verifica che il server sia attivo (bind in package-registry.json) e che l'URL sia giusto (gmb login --registry <url>).");
                throw new PackageException($"{label} fallito ({status}) su {where}: {body}");
            }
            return body;
        }

        /// <summary>GET autenticato al registry — HTTP DELEGATO al C (gmb_pkg_registry_get_auth).</summary>
        private static string GetJsonAuth(string registry, string path, string? token, string label)
        {
            var where = $"{registry.TrimEnd('/')}{path}";
            var ok = NativePkg.RegistryGetAuth(where, token, out var status, out var body);
            if (!ok)
            {
                if (status == 0)
                    throw new PackageException(
                        $"{label} fallito: registry NON RAGGIUNGIBILE su {where}. " +
                        "Verifica che il server sia attivo (bind in package-registry.json) e che l'URL sia giusto (gmb login --registry <url>).");
                throw new PackageException($"{label} fallito ({status}) su {where}: {body}");
            }
            return body;
        }

        private Task PublishPackageAsyncInternal(string gmbpkgPath, string? registryUrl, string? token)
        {
            if (!File.Exists(gmbpkgPath)) throw new PackageException($"gmbpkg non trovato: {gmbpkgPath}");
            var registry = registryUrl ?? RegistryUrl;

            // name/version dal manifest dentro lo zip
            string name = "", version = "";
            using (var zip = ZipFile.OpenRead(gmbpkgPath))
            {
                var me = zip.GetEntry("gmb-package.json") ?? throw new PackageException("gmb-package.json mancante nel gmbpkg");
                using var sr = new StreamReader(me.Open());
                using var doc = JsonDocument.Parse(sr.ReadToEnd());
                var root = doc.RootElement;
                name = root.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "";
                version = root.TryGetProperty("version", out var v) ? v.GetString() ?? "" : "";
            }
            if (name.Length == 0 || version.Length == 0) throw new PackageException("name/version mancanti nel manifest");

            var payload = JsonSerializer.Serialize(new
            {
                name,
                version,
                file = Convert.ToBase64String(File.ReadAllBytes(gmbpkgPath)),
                token,
            });
            PostJson(registry, "/api/packages", payload, token, "Publish");
            return Task.CompletedTask;
        }

        /// <summary>
        /// Registra un account utente sul registry e restituisce il token opaco (Fase A1).
        /// </summary>
        public Task<string> RegisterUserAsync(string userName, string email, string? registryUrl = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            var body = PostJson(registry, "/api/users/register", JsonSerializer.Serialize(new { name = userName, email }), null, "Registrazione");
            using var doc = JsonDocument.Parse(body);
            var token = doc.RootElement.GetProperty("token").GetString() ?? "";
            if (token.Length == 0) throw new PackageException("Token non restituito dal registry");
            return Task.FromResult(token);
        }

        /// <summary>
        /// Gestisce i collaboratori di un pacchetto: add/remove (Fase A5). Richiede il token dell'owner.
        /// </summary>
        public Task<string> ManageOwnersAsync(string packageName, string action, string user, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, $"/api/packages/{Uri.EscapeDataString(packageName)}/owners",
                JsonSerializer.Serialize(new { action, user }), token, $"Owners {action}"));
        }

        /// <summary>
        /// Yank/unyank di una versione (Fase A6). yank=true ritira, false riabilita.
        /// </summary>
        public Task<string> YankVersionAsync(string packageName, string version, bool yank, string? reason = null, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, $"/api/packages/{Uri.EscapeDataString(packageName)}/yank",
                JsonSerializer.Serialize(new { version, yank = yank ? "1" : "0", reason }), token, "Yank"));
        }

        /// <summary>Depreca una versione (con messaggio) o la riabilita (POST /api/packages/{name}/deprecate).</summary>
        public Task<string> DeprecateVersionAsync(string packageName, string version, string? message = null,
            bool deprecate = true, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, $"/api/packages/{Uri.EscapeDataString(packageName)}/deprecate",
                JsonSerializer.Serialize(new { version, message, deprecated = deprecate ? "1" : "0" }), token, "Deprecate"));
        }

        /// <summary>
        /// Sincronizza il tag di un pacchetto contributor (Fase C4): il registry aggiorna il
        /// tag corrente in repos.json (client già rifiutato in fase di verifica).
        /// </summary>
        public Task<string> SyncRepoAsync(string packageName, string tag, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, $"/api/packages/{Uri.EscapeDataString(packageName)}/sync",
                JsonSerializer.Serialize(new { tag }), token, "Sync"));
        }

        /// <summary>
        /// Admin: riserva un nome (blocca il claim automatico). body { name }.
        /// </summary>
        public Task<string> AdminReserveAsync(string packageName, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/reserve",
                JsonSerializer.Serialize(new { name = packageName }), token, "Reserve"));
        }

        /// <summary>
        /// Admin: assegna l'owner di un nome (riservato o libero). body { name, owner }.
        /// </summary>
        public Task<string> AdminAssignAsync(string packageName, string owner, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/assign",
                JsonSerializer.Serialize(new { name = packageName, owner }), token, "Assign"));
        }

        /// <summary>
        /// Admin: revoca un claim/ownership. body { name, reserve: "1"|"0" }.
        /// reserve="1" lascia il nome riservato (niente claim automatico).
        /// </summary>
        public Task<string> AdminRevokeAsync(string packageName, bool keepReserved = false, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/revoke",
                JsonSerializer.Serialize(new { name = packageName, reserve = keepReserved ? "1" : "0" }), token, "Revoke"));
        }

        /// <summary>
        /// Admin: cambia il token admin. body { newToken }.
        /// </summary>
        public Task<string> AdminPasswordAsync(string newToken, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/password",
                JsonSerializer.Serialize(new { newToken }), token, "Password admin"));
        }

        /// <summary>
        /// Admin: crea un utente autorizzato (genera il token, status "blocked"). body { name }.
        /// </summary>
        public Task<string> AdminUserCreateAsync(string name, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/users/create",
                JsonSerializer.Serialize(new { name }), token, "Creazione utente"));
        }

        /// <summary>
        /// Admin: registra la chiave pubblica Ed25519 di un utente. body { name, publicKey }.
        /// </summary>
        public Task<string> AdminUserSetKeyAsync(string name, string publicKey, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/users/setkey",
                JsonSerializer.Serialize(new { name, publicKey }), token, "SetKey"));
        }

        /// <summary>
        /// Admin: attiva/blocca un utente. body { name, status: active|blocked }.
        /// </summary>
        public Task<string> AdminUserStatusAsync(string name, string status, string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(PostJson(registry, "/api/admin/users/status",
                JsonSerializer.Serialize(new { name, status }), token, "Status utente"));
        }

        /// <summary>
        /// Admin: elenca gli utenti autorizzati (senza token in chiaro).
        /// </summary>
        public Task<string> AdminUserListAsync(string? registryUrl = null, string? token = null)
        {
            var registry = registryUrl ?? RegistryUrl;
            token ??= GmbConfig.Load().Token;
            return Task.FromResult(GetJsonAuth(registry, "/api/admin/users/list", token, "Lista utenti"));
        }

        /// <summary>
        /// CLIENT-BUILD (opt-in): se il pacchetto dichiara un `build` e NON ha un artefatto ffi
        /// precompilato per la piattaforma corrente, compila il C all'install. Rileva il compilatore;
        /// se assente chiede il consenso per installarlo; build in-place nella dir del pacchetto;
        /// verifica gli outputs. Idempotente (salta se precompilato o già costruito).
        /// </summary>
        public Task<bool> EnsureBuildAsync(InstalledPackage pkg, PackageBuildContext? ctx = null)
        {
            if (pkg.Manifest.Build == null) return Task.FromResult(false);
            var platformKey = ctx != null && !string.IsNullOrEmpty(ctx.PlatformKey) ? ctx.PlatformKey : GetCurrentPlatformKey();
            // Client-build DELEGATO al riferimento C (gmb_pkg_ensure_build): precompilato/outputs
            // presenti → skip; rileva compilatore; esegue make/cmake/command; verifica gli output.
            var r = NativePkg.EnsureBuild(pkg.InstallPath, platformKey);
            if (r < 0)
                throw new PackageException($"build client di '{pkg.Manifest.Name}' ({platformKey}) fallito (compilatore assente o errore di build).");
            return Task.FromResult(r == 1);
        }

        /// <summary>Copia gli esempi del pacchetto — DELEGATO al riferimento C (gmb_pkg_copy_samples):
        /// dest <project_root>/samples/packages/<name>@<version>/. La project_root è PASSATA
        /// ESPLICITAMENTE (GmbiPaths.RepoRoot, dalla base dell'eseguibile): MAI dedotta dalla CWD,
        /// che può trovarsi dentro il sorgente di un pacchetto con una propria samples/ (bug reale).</summary>
        private static void CopySamplesToProject(PackageManifest m, string installDir)
        {
            _ = m; // il C legge il manifest da installDir
            // DESTINAZIONE = la cartella `samples` CONDIVISA (`<monorepo>/shared/samples`, vedi
            // GmbiPaths.SharedSamplesRoot). Prima si passava `RepoRoot` (= `<monorepo>/GMBasic`):
            // ogni install di pacchetto ricreava `GMBasic/samples/packages/<nome>@<versione>/`,
            // cioè una `samples/` dentro l'install invece di quella di `shared` (segnalato
            // dall'utente; riprodotto con packages/require_avif.bas).
            NativePkg.CopySamples(installDir, CommonLib.GmbiPaths.SharedSamplesRoot);
        }

        /// <summary>
        /// Fase 1 — materializza la sorgente esterna di un pacchetto (oggi: GitHub release).
        /// Scarica l'asset della piattaforma corrente, ne verifica l'integrità (sha256 da manifest o
        /// digest GitHub) e lo estrae nella cartella dichiarata (default lib/&lt;platform&gt;), copiando
        /// gli header in include/. Idempotente: se la cartella di destinazione esiste già e non è vuota
        /// non riscarica (una volta scaricata resta in cache). Ritorna true se ha scaricato qualcosa.
        /// </summary>
        public Task<bool> EnsureSourceAsync(InstalledPackage pkg, PackageBuildContext? ctx = null)
        {
            var src = pkg.Manifest.Source;
            if (src == null || !src.Type.Equals("github-release", StringComparison.OrdinalIgnoreCase)) return Task.FromResult(false);
            var platformKey = ctx != null && !string.IsNullOrEmpty(ctx.PlatformKey) ? ctx.PlatformKey : GetCurrentPlatformKey();
            // Materializzazione sorgente esterna DELEGATA al C (gmb_pkg_ensure_source): scarica
            // l'asset della piattaforma, verifica sha256, estrae in dest (default lib/<platform>).
            return Task.FromResult(NativePkg.EnsureSource(pkg.InstallPath, platformKey));
        }

        /// <summary>
        /// Estrae un archivio (zip/tar.gz/tgz/tar/gz — via ArchiveFetcher) in destDir e, se indicati,
        /// copia gli header in includeDir. Metodo puro (nessuna rete), verificabile nei test.
        /// </summary>
        public static void MaterializeArchive(string archivePath, string destDir,
            IEnumerable<string>? headers = null, string? includeDir = null)
        {
            CommonLib.ArchiveFetcher.Extract(archivePath, destDir);
            if (headers != null && includeDir != null)
            {
                Directory.CreateDirectory(includeDir);
                foreach (var hdr in headers)
                    foreach (var found in Directory.EnumerateFiles(destDir, Path.GetFileName(hdr), SearchOption.AllDirectories))
                        File.Copy(found, Path.Combine(includeDir, Path.GetFileName(found)), overwrite: true);
            }
        }

        /// <summary>
        /// Scarica un pacchetto dal registry (con negoziazione backend/platform/edition) e lo installa.
        /// </summary>
        /// <summary>Regola di `gmb cache clean`: true = RIFIUTA. La cache di un checkout sorgente è
        /// `<checkout>/packages`, che contiene file VERSIONATI (manifest, .inc, sorgenti): svuotarla
        /// cancella il working tree (è successo il 2026-10-28). `force` la scavalca.
        /// Funzione pura: un test NON deve mai arrivare a cancellare la cache vera.</summary>
        public static bool CacheCleanBlocked(string cacheDir, bool force)
            => !force
               && CommonLib.GmbiPaths.IsSourceCheckout(CommonLib.GmbiPaths.RepoRoot)
               && string.Equals(Path.GetFullPath(cacheDir).TrimEnd(Path.DirectorySeparatorChar),
                                Path.GetFullPath(CommonLib.GmbiPaths.PackagesDir).TrimEnd(Path.DirectorySeparatorChar),
                                StringComparison.OrdinalIgnoreCase);

        /// <summary>1 se l'artefatto dichiarato dal manifest esiste davvero nella cartella del
        /// pacchetto: i file di `exports` e la lib di `ffi.libs[<piattaforma>]`. Serve a non
        /// considerare "installato" un pacchetto la cui cartella esiste ma è stata svuotata
        /// (sintomo reale 2026-10-28: `lib/` mancante e `pkg install` che non faceva nulla).
        /// Legge il JSON del manifest: nessun cambio di modello.</summary>
        public static bool ArtifactPresent(string dir)
        {
            try
            {
                var mpath = Path.Combine(dir, "gmb-package.json");
                if (!File.Exists(mpath)) return false;
                using var doc = JsonDocument.Parse(File.ReadAllText(mpath));
                var root = doc.RootElement;
                if (root.TryGetProperty("exports", out var ex) && ex.ValueKind == JsonValueKind.Array)
                {
                    foreach (var e in ex.EnumerateArray())
                    {
                        var rel = e.GetString();
                        if (!string.IsNullOrEmpty(rel) && !File.Exists(Path.Combine(dir, rel))) return false;
                    }
                }
                var plat = GetCurrentPlatformKey();
                if (root.TryGetProperty("ffi", out var ffi) && ffi.ValueKind == JsonValueKind.Object &&
                    ffi.TryGetProperty("libs", out var libs) && libs.ValueKind == JsonValueKind.Object &&
                    libs.TryGetProperty(plat, out var lib) && lib.ValueKind == JsonValueKind.String)
                {
                    var rel = lib.GetString();
                    if (!string.IsNullOrEmpty(rel) && !File.Exists(Path.Combine(dir, rel))) return false;
                }
                return true;
            }
            catch { return true; }   /* in dubbio non bloccare l'installazione */
        }

        /// <summary>Installa in una cartella di DESTINAZIONE (installer `--local`: la cartella
        /// `packages` accanto all'eseguibile; REPL `LOCAL`/`TO`). Regole (direttiva utente):
        ///  1. se la stessa versione è già nella destinazione → niente da fare;
        ///  2. se la stessa versione è nella cache → **SPOSTA** (copia + rimozione dalla cache);
        ///  3. altrimenti la scarica dal registry direttamente nella destinazione.
        /// La cache (blob store content-addressable) NON viene replicata nella destinazione.</summary>
        public async Task<InstalledPackage> InstallIntoAsync(string packageName, string? version,
            string targetDir, string? sourceDir = null, PackageBuildContext? ctx = null)
        {
            var safe = SafePackageDir(packageName);
            var destPkgDir = Path.Combine(targetDir, safe);
            // SORGENTE da cui SPOSTARE (stessa versione): se non indicata è l'"altro" dei due
            // posti — target locale → cache, target cache → installazione locale.
            if (sourceDir == null)
            {
                var local = CommonLib.GmbiPaths.LocalPackagesDir;
                var same = string.Equals(Path.GetFullPath(targetDir).TrimEnd(Path.DirectorySeparatorChar),
                                         Path.GetFullPath(local).TrimEnd(Path.DirectorySeparatorChar),
                                         StringComparison.OrdinalIgnoreCase);
                sourceDir = same ? CacheRoot : local;
            }
            var cachePkgDir = Path.Combine(sourceDir, safe);
            var sourceIsCache = string.Equals(Path.GetFullPath(sourceDir).TrimEnd(Path.DirectorySeparatorChar),
                                              Path.GetFullPath(CacheRoot).TrimEnd(Path.DirectorySeparatorChar),
                                              StringComparison.OrdinalIgnoreCase);

            string? Chosen(string dir)
            {
                if (!Directory.Exists(dir)) return null;
                var versions = Directory.GetDirectories(dir)
                    .Select(Path.GetFileName)
                    .Where(v => v != null && SemVer.Parse(v!) != null)
                    .Select(v => v!)
                    .ToList();
                if (versions.Count == 0) return null;
                if (version != null)
                    return versions.FirstOrDefault(v => string.Equals(v, version, StringComparison.OrdinalIgnoreCase));
                return versions.OrderByDescending(v => SemVer.Parse(v)!).First();
            }

            var alreadyThere = Chosen(destPkgDir);
            if (alreadyThere != null && (version == null || string.Equals(alreadyThere, version, StringComparison.OrdinalIgnoreCase)))
            {
                var candDir = Path.Combine(destPkgDir, alreadyThere);
                var m0 = await ReadManifestFromDirAsync(candDir);
                // "Esiste la cartella" NON basta: un pacchetto può essere incompleto (es. senza
                // lib/<platform>/ dopo una cancellazione parziale) e resterebbe "installato" per
                // sempre, senza che nulla lo ripari. Si verifica l'ARTEFATTO dichiarato.
                if (ArtifactPresent(candDir))
                {
                    _resolvedPackages[packageName] = new InstalledPackage(m0, candDir);
                    return _resolvedPackages[packageName];
                }
                try { Directory.Delete(candDir, true); } catch { /* reinstalla */ }
            }

            var movable = Chosen(cachePkgDir);
            if (movable != null)
            {
                var from = Path.Combine(cachePkgDir, movable);
                var to = Path.Combine(destPkgDir, movable);
                Directory.CreateDirectory(destPkgDir);
                if (!Directory.Exists(to))
                {
                    try { Directory.Move(from, to); }
                    catch (IOException)
                    {
                        // volumi diversi / file in uso: copia ricorsiva e poi rimozione
                        CopyDir(from, to);
                        try { Directory.Delete(from, true); } catch { /* resta in cache: non è un errore */ }
                    }
                    catch (UnauthorizedAccessException)
                    {
                        CopyDir(from, to);
                        try { Directory.Delete(from, true); } catch { }
                    }
                }
                var m1 = await ReadManifestFromDirAsync(to);
                // la cartella <sorgente>/<nome> resta VUOTA dopo lo spostamento: rimuoverla,
                // altrimenti `pkg list`/`prune` vedono un pacchetto fantasma senza versioni.
                try { if (Directory.Exists(cachePkgDir) && Directory.GetDirectories(cachePkgDir).Length == 0) Directory.Delete(cachePkgDir, false); } catch { }
                _resolvedPackages[packageName] = new InstalledPackage(m1, to);
                return _resolvedPackages[packageName];
            }

            // assente dalla cache: scarica nella DESTINAZIONE (senza creare un blob store lì)
            var prevRoot = CacheRoot;
            var prevContent = ContentRootOverride;
            try
            {
                CacheRoot = targetDir;
                ContentRootOverride = Path.Combine(prevRoot, CacheStore.DirName);
                return await DownloadPackageAsync(packageName, version, ctx: ctx);
            }
            finally { CacheRoot = prevRoot; ContentRootOverride = prevContent; }
        }

        private static void CopyDir(string src, string dst)
        {
            Directory.CreateDirectory(dst);
            foreach (var f in Directory.GetFiles(src, "*", SearchOption.AllDirectories))
            {
                var rel = Path.GetRelativePath(src, f);
                var outFile = Path.Combine(dst, rel);
                Directory.CreateDirectory(Path.GetDirectoryName(outFile)!);
                File.Copy(f, outFile, true);
            }
        }

        public async Task<InstalledPackage> DownloadPackageAsync(string packageName, string? version = null,
            string? registryUrl = null, PackageBuildContext? ctx = null)
        {
            var registries = RegistryUrls.Count > 0 ? RegistryUrls : [RegistryUrl];
            if (registryUrl != null && !registries.Contains(registryUrl)) registries.Add(registryUrl);
            PackageException? lastError = null;

            foreach (var reg in registries)
            {
                try
                {
                    return await DownloadFromRegistryAsync(packageName, version, reg, ctx);
                }
                catch (PackageException ex) { lastError = ex; }
                catch (HttpRequestException ex) { lastError = new PackageException(ex.Message, ex); }
            }
            throw lastError ?? new PackageException($"Download '{packageName}' fallito: nessun registry raggiungibile.");
        }

        private async Task<InstalledPackage> DownloadFromRegistryAsync(string packageName, string? version, string registry, PackageBuildContext? ctx)
        {
            var globalCachePath = CacheRoot;

            if (string.IsNullOrWhiteSpace(packageName))
                throw new ArgumentException("Package name is required.", nameof(packageName));

            if (_resolvedPackages.TryGetValue(packageName, out var installed) &&
                (version == null || installed.Manifest.Version == version))
                return installed;

            // Negoziazione: query backend/platform/edition/contentType.
            var q = new List<string>();
            if (ctx != null)
            {
                q.Add($"backend={Uri.EscapeDataString(ctx.Backend)}");
                var pk = string.IsNullOrEmpty(ctx.PlatformKey) ? GetCurrentPlatformKey() : ctx.PlatformKey;
                q.Add($"platform={Uri.EscapeDataString(pk)}");
                q.Add($"edition={Uri.EscapeDataString(ctx.Edition)}");
            }
            var qs = q.Count > 0 ? "?" + string.Join("&", q) : "";

            // Scoperta versioni DELEGATA al C (gmb_pkg_registry_detail via NativePkg.RegistryVersions):
            // UNA sola sorgente per il dettaglio del registry, niente HttpClient ne' parsing delle
            // forme JSON in C#. Il server pubblica 'versionList' (vedi PackageServer.bas).
            var candidateStrings = new List<string>();
            var candidates = new List<SemVer>();
            foreach (var raw in NativePkg.RegistryVersions(registry.TrimEnd('/'), packageName))
            {
                var t = raw.Trim();
                if (t.Length == 0 || candidateStrings.Contains(t)) continue;
                var parsed = SemVer.Parse(t);
                if (parsed == null) continue;
                candidateStrings.Add(t);
                candidates.Add(parsed);
            }
            if (candidates.Count == 0)
                throw new PackageException($"Registry non pubblica versioni utilizzabili per '{packageName}'.");

            /* `version` puo' essere una VERSIONE CONCRETA (pin) OPPURE un VINCOLO (>=1.0, ^2.1,
             * ~3.0, latest...): se e' una versione esatta la si usa, altrimenti si sceglie la piu'
             * alta che soddisfa il vincolo fra quelle pubblicate. */
            string targetVersion;
            var spec = version?.Trim();
            if (!string.IsNullOrEmpty(spec) && SemVer.Parse(spec!) != null)
            {
                targetVersion = spec!;
            }
            else
            {
                var picked = string.IsNullOrEmpty(spec) || spec!.Equals("latest", StringComparison.OrdinalIgnoreCase)
                    ? candidates.Max()
                    : SemVer.FindBestMatch([spec!], candidates);
                if (picked == null)
                    throw new PackageException(
                        $"Nessuna versione di '{packageName}' soddisfa il vincolo '{spec}' (disponibili: {string.Join(", ", candidateStrings)}).");
                targetVersion = picked.ToString();
            }

            // Download PER-PIATTAFORMA: porta al server la stessa negoziazione (backend/platform/
            // edition) del dettaglio, così il registry serve SOLO i file della piattaforma corrente
            // (niente download di tutte le piattaforme di un pacchetto multi-RID). Senza ctx -> qs="" -> intero.
            var tmpZip = Path.GetTempFileName() + ".zip";
            try
            {
                // Download DELEGATO al C (gmb_pkg_registry_download): UNA sola sorgente, niente
                // HttpClient. Stesso URL del registry: <base>/download/<name>/<ver><qs>.
                if (!NativePkg.RegistryDownload(registry.TrimEnd('/'), packageName, targetVersion, qs, tmpZip))
                    throw new PackageException($"Download di '{packageName}@{targetVersion}' fallito da {registry}.");

                var destDir = Path.Combine(globalCachePath, SafePackageDir(packageName), targetVersion);
                if (Directory.Exists(destDir)) Directory.Delete(destDir, true);
                Directory.CreateDirectory(destDir);
                // Estrazione DELEGATA al C (gmb_unzip_files).
                if (!NativePkg.Unzip(tmpZip, destDir))
                    throw new PackageException($"Estrazione di '{packageName}@{targetVersion}' fallita.");

                var manifest = await ReadManifestAsync(globalCachePath, packageName, SemVer.Parse(targetVersion)!, new Token(TokenKind.Unknown, "", null, SourcePosition.Unknown));
                var pkg = new InstalledPackage(manifest, destDir);
                await EnsureSourceAsync(pkg, ctx);   // Fase 1: materializza la sorgente esterna (GitHub release) se dichiarata
                await EnsureBuildAsync(pkg, ctx);    // client-build: compila il C se manca il precompilato per la piattaforma
                CacheStore.InternDir(ContentRoot, destDir);   // content-addressable: dedup dei file via blob+hardlink
                CopySamplesToProject(manifest, destDir);   // esempi del pacchetto → <root>/samples/packages/<nome>@<versione>/
                _resolvedPackages[packageName] = pkg;
                return pkg;
            }
            catch (HttpRequestException ex)
            {
                throw new PackageException($"Failed to download '{packageName}@{targetVersion}' from {registry}: {ex.Message}");
            }
            finally
            {
                try { if (File.Exists(tmpZip)) File.Delete(tmpZip); } catch { }
            }
        }

        /// <summary>
        /// Normalizza il nome logico del pacchetto (può essere "@scope/name") in un nome di
        /// cartella sicuro per il filesystem: "/" → "__" (mai "/" nel path). "@" è valido.
        /// Il nome LOGICO (con scope) resta quello canonico per lock/grafo/dizionari.
        /// </summary>
        /// <summary>Nome cartella sicuro (delegato al C: gmb_pkg_safe_dir).</summary>
        public static string SafePackageDir(string packageName) => NativePkg.SafeDir(packageName);

        /// <summary>Legge il manifesto di PROGETTO (`gmb-package.json` nella cartella del progetto).
    /// Riusa lo stesso parser dei manifesti dei pacchetti: un solo formato, una sola grammatica.
    /// Se il file non c'e' -&gt; null (nessun manifesto, comportamento di prima). Se c'e' ma e'
    /// malformato -&gt; errore esplicito: meglio che dipendenze mancanti in silenzio.</summary>
    public static async Task<PackageManifest?> ReadProjectManifestAsync(string projectDir)
    {
        var path = Path.Combine(projectDir, "gmb-package.json");
        if (!File.Exists(path)) return null;
        try
        {
            using var doc = JsonDocument.Parse(await File.ReadAllTextAsync(path));
            return ParseManifest(doc.RootElement);
        }
        catch (JsonException ex)
        {
            throw new BasicException(ErrorCode.JsonParseError, SourcePosition.Unknown,
                $"Manifesto di progetto '{path}' e' corrotto: {ex.Message}");
        }
        catch (IOException ex)
        {
            throw new BasicException(ErrorCode.FileNotFound, SourcePosition.Unknown,
                $"Errore I/O sul manifesto di progetto '{path}': {ex.Message}", null, ex);
        }
    }

    /// <summary>Legge gmb-package.json da una directory arbitraria (pacchetto LOCALE del workspace).</summary>
    public static async Task<PackageManifest> ReadManifestFromDirAsync(string dir, Token? token = null)
    {
        var path = Path.Combine(dir, "gmb-package.json");
        if (!File.Exists(path))
            throw new BasicException(ErrorCode.FileNotFound, token?.Position ?? SourcePosition.Unknown, $"gmb-package.json mancante in '{dir}'.");
        using var doc = JsonDocument.Parse(await File.ReadAllTextAsync(path));
        return ParseManifest(doc.RootElement);
    }

    private static async Task<PackageManifest> ReadManifestAsync(string cachePath, string packageName, SemVer version, Token contextToken)
        {
            var manifestPath = Path.Combine(cachePath, SafePackageDir(packageName), version.ToString(), "gmb-package.json");
            if (!File.Exists(manifestPath))
                throw new BasicException(ErrorCode.FileNotFound, contextToken.Position,
                    $"Manifest 'gmb-package.json' non trovato per il pacchetto '{packageName}' v{version}.");

            try
            {
                var manifestJson = await File.ReadAllTextAsync(manifestPath);
                using var doc = JsonDocument.Parse(manifestJson);
                return ParseManifest(doc.RootElement, contextToken.Position);
            }
            catch (JsonException ex)
            {
                throw new BasicException(ErrorCode.JsonParseError, contextToken.Position,
                    $"Manifest del pacchetto '{packageName}' v{version} in '{manifestPath}' è corrotto: {ex.Message}", null, ex);
            }
            catch (IOException ex)
            {
                throw new BasicException(ErrorCode.FileNotFound, contextToken.Position,
                    $"Errore I/O manifest '{manifestPath}': {ex.Message}", null, ex);
            }
        }

        /// <summary>Parsa gmb-package.json v1+v2 e normalizza i binari in BinaryEntry.</summary>
        /// <summary>Parsa il manifest DELEGANDO al riferimento C (gmb_pkg_manifest_*).</summary>
        public static PackageManifest ParseManifest(JsonElement root, SourcePosition pos = default)
            => ParseManifest(root.GetRawText(), pos);

        /// <summary>Parsa gmb-package.json delegando al C (fonte unica; nessun parsing C# duplicato).</summary>
        public static PackageManifest ParseManifest(string json, SourcePosition pos = default)
        {
            var h = NativePkg.ManifestParse(json);
            if (h == IntPtr.Zero)
                throw new BasicException(ErrorCode.JsonParseError, pos, "Manifest non è un oggetto JSON valido.");
            try
            {
                var m = new PackageManifest
                {
                    Name = NativePkg.ManifestGet(h, "name"),
                    Version = NativePkg.ManifestGet(h, "version"),
                    Description = NativePkg.ManifestGet(h, "description"),
                    Edition = Nz(NativePkg.ManifestGet(h, "edition")) ?? "any",
                };
                m.ContentType = Nz(NativePkg.ManifestGet(h, "contentType"))?.ToLowerInvariant() switch
                {
                    "dotnet" => PackageContentType.DotNet,
                    "vm" or "vm-chunk" or "vmc" => PackageContentType.VmChunk,
                    "c-ffi" or "cffi" => PackageContentType.CFFI,
                    "c-ffi-source" or "cffi-source" => PackageContentType.CFFISource,
                    "mixed" => PackageContentType.Mixed,
                    "docs-only" => PackageContentType.DocsOnly,
                    "brain" => PackageContentType.Brain,
                    _ => PackageContentType.Source
                };
                m.Backends = StrArr(h, "backends");
                m.Exports = StrArr(h, "exports");
                m.Samples = StrArr(h, "samples");
                m.Workspaces = StrArr(h, "workspaces");
                m.Files = StrArr(h, "files");
                m.Ignore = StrArr(h, "ignore");

                // binaries: v1 { platform: path } (val stringa) | v2 { backend: { platform: path } } (val oggetto)
                var nb = NativePkg.ManifestCount(h, "binaries");
                for (var i = 0; i < nb; i++)
                    if (NativePkg.ManifestItem(h, "binaries", i, out var bkey, out var bval))
                    {
                        if (!string.IsNullOrEmpty(bval)) m.Binaries[bkey] = new BinaryEntry { Path = bval };
                        else
                        {
                            var platforms = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                            var np = NativePkg.ManifestCount(h, "binaries." + bkey);
                            for (var j = 0; j < np; j++)
                                if (NativePkg.ManifestItem(h, "binaries." + bkey, j, out var pk, out var pv)) platforms[pk] = pv;
                            m.Binaries[bkey] = new BinaryEntry { Platforms = platforms };
                        }
                    }

                if (NativePkg.ManifestHas(h, "ffi"))
                {
                    var libs = ImmutableDictionary.CreateBuilder<string, string>(StringComparer.OrdinalIgnoreCase);
                    var nl = NativePkg.ManifestCount(h, "ffi.libs");
                    for (var i = 0; i < nl; i++)
                        if (NativePkg.ManifestItem(h, "ffi.libs", i, out var lk, out var lv)) libs[lk] = lv;
                    m.Ffi = new FfiSection
                    {
                        Api = Nz(NativePkg.ManifestGet(h, "ffi.api")),
                        Headers = StrArr(h, "ffi.headers"),
                        Libs = libs.ToImmutable(),
                        Link = Nz(NativePkg.ManifestGet(h, "ffi.link")),
                        DeclareExample = Nz(NativePkg.ManifestGet(h, "ffi.declareExample")),
                    };
                }

                if (NativePkg.ManifestHas(h, "source"))
                {
                    var psrc = new PackageSource
                    {
                        Type = NativePkg.ManifestGet(h, "source.type"),
                        Repo = NativePkg.ManifestGet(h, "source.repo"),
                        Tag = Nz(NativePkg.ManifestGet(h, "source.tag")),
                        Dest = Nz(NativePkg.ManifestGet(h, "source.dest")) ?? "lib/{platform}",
                        Headers = StrArr(h, "source.headers"),
                    };
                    var na = NativePkg.ManifestCount(h, "source.assets");
                    for (var i = 0; i < na; i++)
                        if (NativePkg.ManifestItem(h, "source.assets", i, out var ak, out var av)) psrc.Assets[ak] = av;
                    var nsh = NativePkg.ManifestCount(h, "source.sha256");
                    for (var i = 0; i < nsh; i++)
                        if (NativePkg.ManifestItem(h, "source.sha256", i, out var sk, out var sv)) psrc.Sha256[sk] = sv;
                    m.Source = psrc;
                }

                if (NativePkg.ManifestHas(h, "build"))
                    m.Build = new PackageBuildSpec
                    {
                        System = Nz(NativePkg.ManifestGet(h, "build.system")),
                        Target = Nz(NativePkg.ManifestGet(h, "build.target")),
                        Command = Nz(NativePkg.ManifestGet(h, "build.command")),
                        Cwd = Nz(NativePkg.ManifestGet(h, "build.cwd")),
                        Tool = Nz(NativePkg.ManifestGet(h, "build.tool")),
                        Outputs = StrArr(h, "build.outputs"),
                    };

                if (NativePkg.ManifestHas(h, "test"))
                    m.Test = new PackageTestSection
                    {
                        Entry = Nz(NativePkg.ManifestGet(h, "test.entry")),
                        Backends = StrArr(h, "test.backends"),
                    };

                if (NativePkg.ManifestHas(h, "settings"))
                    m.Settings = new PackageSettings
                    {
                        RequiresCommercial = NativePkg.ManifestGetNum(h, "settings.requiresCommercial", out var rc) && rc != 0,
                        MinGmbVersion = Nz(NativePkg.ManifestGet(h, "settings.minGmbVersion")),
                    };

                m.Dependencies = StrMap(h, "dependencies");
                m.DevDependencies = StrMap(h, "devDependencies");
                m.OptionalDependencies = StrMap(h, "optionalDependencies");
                m.PeerDependencies = StrMap(h, "peerDependencies");
                m.Overrides = StrMap(h, "overrides");

                m.SignatureAlgo = Nz(NativePkg.ManifestGet(h, "signature.algo"));
                m.SignaturePublicKey = Nz(NativePkg.ManifestGet(h, "signature.publicKey"));
                m.SignatureValue = Nz(NativePkg.ManifestGet(h, "signature.value")) ?? Nz(NativePkg.ManifestGet(h, "signature"));

                var nf = NativePkg.ManifestCount(h, "features");
                if (nf > 0)
                {
                    var fb = ImmutableDictionary.CreateBuilder<string, ImmutableArray<string>>(StringComparer.OrdinalIgnoreCase);
                    for (var i = 0; i < nf; i++)
                        if (NativePkg.ManifestItem(h, "features", i, out var fk, out _))
                        {
                            var items = new List<string>();
                            var isList = NativePkg.ManifestIsList(h, "features." + fk);
                            var nc = NativePkg.ManifestCount(h, "features." + fk);
                            for (var j = 0; j < nc; j++)
                                if (NativePkg.ManifestItem(h, "features." + fk, j, out var ik, out var iv))
                                    items.Add(isList ? iv : ik); // array → valori; oggetto → nomi (chiavi)
                            fb[fk] = items.ToImmutableArray();   // anche feature senza dipendenze (flag)
                        }
                    m.Features = fb.ToImmutable();
                }

                return m;
            }
            finally { NativePkg.ManifestFree(h); }
        }

        private static string? Nz(string s) => string.IsNullOrEmpty(s) ? null : s;

        private static ImmutableArray<string> StrArr(IntPtr h, string path)
        {
            var n = NativePkg.ManifestCount(h, path);
            if (n <= 0) return [];
            var b = ImmutableArray.CreateBuilder<string>(n);
            for (var i = 0; i < n; i++)
                if (NativePkg.ManifestItem(h, path, i, out _, out var v) && v.Length > 0) b.Add(v);
            return b.ToImmutable();
        }

        private static ImmutableDictionary<string, string> StrMap(IntPtr h, string path)
        {
            var n = NativePkg.ManifestCount(h, path);
            if (n <= 0) return ImmutableDictionary<string, string>.Empty;
            var b = ImmutableDictionary.CreateBuilder<string, string>(StringComparer.OrdinalIgnoreCase);
            for (var i = 0; i < n; i++)
                if (NativePkg.ManifestItem(h, path, i, out var k, out var v)) b[k] = string.IsNullOrEmpty(v) ? "*" : v;
            return b.ToImmutable();
        }
    }

    /// <summary>SemVer 2.0.0 — include prerelease (-rc.1) e build (+sha); `||` per range OR.</summary>
    public sealed record SemVer(int Major, int Minor, int Patch, string? Prerelease = null, string? Build = null) : IComparable<SemVer>
    {
        /// <summary>Parsa via il riferimento C (gmb_semver_parse): fonte unica, niente logica duplicata.</summary>
        public static SemVer? Parse(string version)
        {
            if (string.IsNullOrWhiteSpace(version)) return null;
            if (!NativePkg.Parse(version, out var c)) return null;
            return new SemVer(c.major, c.minor, c.patch,
                c.hasPre != 0 ? c.prerelease : null,
                string.IsNullOrEmpty(c.build) ? null : c.build);
        }

        /// <summary>Specchio gestito di gmb_semver_t passato alle funzioni C.</summary>
        private static NativePkg.Csemver ToNative(SemVer v) => new()
        {
            major = v.Major,
            minor = v.Minor,
            patch = v.Patch,
            hasPre = v.Prerelease != null ? 1 : 0,
            prerelease = v.Prerelease ?? "",
            build = v.Build ?? ""
        };

        public override string ToString()
            => $"{Major}.{Minor}.{Patch}{(Prerelease != null ? "-" + Prerelease : "")}{(Build != null ? "+" + Build : "")}";

        /// <summary>Precedenza SemVer 2.0: prerelease &lt; release (build ignorata).</summary>
        public int CompareTo(SemVer? other)
            => other == null ? 1 : NativePkg.Cmp(ToNative(this), ToNative(other));

        public bool IsPrerelease => Prerelease != null;

        /// <summary>Sceglie la versione più alta che soddisfa TUTTI i vincoli — via gmb_semver_find_best (C).</summary>
        public static SemVer? FindBestMatch(List<string> constraints, IEnumerable<SemVer> versions)
        {
            var chosen = NativePkg.FindBest(constraints.ToArray(), versions.Select(v => v.ToString()).ToArray());
            return chosen == null ? null : Parse(chosen);
        }

    }
}

/// <summary>Eccezione specifica per errori del package manager.</summary>
