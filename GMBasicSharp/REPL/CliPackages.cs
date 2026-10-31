// FILE: Executor/CliPackages.cs
// CLI pacchetti (gap pip/cargo): gmb init | pack | pkg ... | publish
//   gmb init <dir> --name X --version 1.0.0 --type source|dotnet|vm|c-ffi|mixed [--edition E] [--api c-abi|gmb-native]
//   gmb pack <dir> [--out out.gmbpkg] [--backend il|c99|vm|all] [--key secret]
//   gmb pkg install <name[@ver]> [--registry url] [--cache dir]
//   gmb pkg list [--cache dir]
//   gmb pkg remove <name> [--cache dir]
//   gmb pkg info <name> [--cache dir]
//   gmb pkg search <term> [--registry url]
//   gmb publish <file.gmbpkg> [--registry url] [--token t]
//   gmb restore [dir] [--registry url] [--cache dir]  -- scan REQUIRE nei sorgenti del progetto e installa tutte le dipendenze (poi scrive gmb-package.lock)
using Basic;

namespace Basic.Executor;

using System.Collections.Immutable;

public static class CliPackages
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0) return false;
        var cmd = args[0].ToLowerInvariant();
        var rest = args.Skip(1).ToArray();
        switch (cmd)
        {
            case "help": case "--help": case "-h": case "-?": PrintHelp(); return true;
            case "init": Run(Init, rest); return true;
            case "pack": Run(Pack, rest); return true;
            case "keygen": Run(KeyGen, rest); return true;
            case "bump": Run(Bump, rest); return true;
            case "add": Run(Add, rest); return true;
            case "remove": Run(RemoveDep, rest); return true;
            case "login": Run(Login, rest); return true;
            case "register": Console.WriteLine("gmb register è stato rimosso: gli utenti li crea l'admin. Usa 'gmb admin user create <name>' poi 'gmb admin user setkey/status'."); return true;
            case "logout": Run(Logout, rest); return true;
            case "cache": Run(Cache, rest); return true;
            case "admin": Run(Admin, rest); return true;
            case "pkg": Run(Pkg, rest); return true;
            case "publish": Run(Publish, rest); return true;
            case "restore": Run(Restore, rest); return true;
            case "update": Run(Update, rest); return true;
            case "trust": Run(Trust, rest); return true;
            case "rollback": Run(Rollback, rest); return true;
            default: return false;
        }
    }

    private static void PrintHelp()
    {
        Console.WriteLine("gmb — package manager e tooling progetto");
        Console.WriteLine("Uso: gmb <comando> [opzioni]");
        Console.WriteLine();
        Console.WriteLine("  help | --help                    Mostra questo aiuto.");
        Console.WriteLine("  init <dir> --name X --version V --type source|dotnet|vm|c-ffi|mixed [--edition E] [--api c-abi|gmb-native]");
        Console.WriteLine("  pack <dir> [--out file.gmbpkg] [--backend il|c99|vm|all] [--key secret|--key-file priv] [--dry-run]");
        Console.WriteLine("  bump <dir> <major|minor|patch|prerelease|X.Y.Z>   aggiorna la versione nel manifest");
        Console.WriteLine("  add <name[@ver]> [--dev] [--dir dir]              aggiunge/aggiorna una dipendenza al manifest");
        Console.WriteLine("  remove <name> [--dir dir]                         rimuove una dipendenza dal manifest");
        Console.WriteLine("  restore [dir] [--locked] [--registry url] [--cache dir]");
        Console.WriteLine("                                   Scansiona i REQUIRE del progetto (.bas/.inc) e installa");
        Console.WriteLine("                                   le dipendenze; --locked usa le versioni di gmb-package.lock.");
        Console.WriteLine("  update [dir] [--check] [--yes] [--registry url] [--cache dir]");
        Console.WriteLine("                                   Aggiorna TUTTI i pacchetti del progetto (REQUIRE +");
        Console.WriteLine("                                   gmb-package.json) all'ultima versione consentita; riscrive il lock.");
        Console.WriteLine("  pkg install <name[@ver]> [--local] [--registry url] [--cache dir]");
        Console.WriteLine("                                   Senza --local installa nella CACHE (spostando li' un pacchetto");
        Console.WriteLine("                                   presente in <exe>/packages); --local installa in");
        Console.WriteLine("                                   <dir dell'eseguibile>/packages (creata) spostando dalla cache.");
        Console.WriteLine("  pkg list | remove <name> | info <name> | search <term> | tree [--lock f] | outdated | update | vendor <progetto> [--lock f]");
        Console.WriteLine("  pkg why <name> | prune [--lock f] | dedupe [--lock f]   (grafo / pulizia cache)");
        Console.WriteLine("  pkg audit [fix] [--advisories f] | sbom [--out f]      (vulnerabilità / fix / SBOM CycloneDX)");
        Console.WriteLine("  pkg deprecate <name@ver> [msg] | undeprecate <name@ver> (owner-only)");
        Console.WriteLine("  restore/update: --frozen (CI) · --offline · --production (no dev) · --verify · --require-signed");
        Console.WriteLine("  rollback [dir] [--list] [--to <timestamp>]   ripristina uno storico gmb-package.lock");
        Console.WriteLine("  trust add <name> <pubkey|file> | trust remove <name> | trust list | trust fingerprint <pubkey|file>");
        Console.WriteLine("                                   Keyring locale delle chiavi fidate (~/.gmbasic/trusted-keys.json).");
        Console.WriteLine("                                   install/restore/update verificano firma+chiave; --require-signed = strict,");
        Console.WriteLine("                                   --insecure/--no-verify = salta, --verify = verifica integrità vs lock.");
        Console.WriteLine("  publish <file.gmbpkg> [--registry url] [--token t]");
        Console.WriteLine("  login <token> [--registry url] | logout | cache list|verify|dedup|gc|clean | keygen <dir>");
        Console.WriteLine("       cache verify [--lock gmb-package.lock]            integrità cache vs hash del lock (tamper-detection)");
        Console.WriteLine();
        Console.WriteLine("Cache pacchetti: <checkout>/packages in sviluppo, ~/.gmbasic/packages su un'app client (senza");
        Console.WriteLine("  compilatore), <dir dell'eseguibile>/packages con --local. Ricerca: vendor -> locale -> cache.");
        Console.WriteLine("Config: ~/.gmbasic/config.json   Registry default: http://localhost:8087   (env GMB_REGISTRY)");
    }

    private static void Login(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb login <token> [--registry url] [--name registryName]");
        var cfg = GmbConfig.Load();
        if (flags.TryGetValue("registry", out var reg)) { cfg.RegistryUrl = reg; }
        if (flags.TryGetValue("name", out var name)) cfg.Registries[name] = flags.GetValueOrDefault("registry") ?? cfg.RegistryUrl;
        cfg.Token = pos[0];
        cfg.Save();
        Console.WriteLine($"Credenziali salvate in {GmbConfig.ConfigPath} (chmod 600)");
    }

    private static void Logout(string[] args)
    {
        var cfg = GmbConfig.Load();
        cfg.Token = null;
        cfg.Save();
        Console.WriteLine("Token rimosso.");
    }

    private static void Register(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb register <username> [--email e] [--registry url]");
        var pm = NewPm(flags);
        var token = pm.RegisterUserAsync(pos[0], flags.GetValueOrDefault("email") ?? "", flags.GetValueOrDefault("registry"))
            .GetAwaiter().GetResult();
        var cfg = GmbHConfigPatch(flags, token);
        cfg.Save();
        Console.WriteLine($"Registrato: {pos[0]} (token salvato in {GmbConfig.ConfigPath})");
    }

    private static GmbConfig GmbHConfigPatch(Dictionary<string, string> flags, string token)
    {
        var cfg = GmbConfig.Load();
        if (flags.TryGetValue("registry", out var reg) && reg != null) cfg.RegistryUrl = reg;
        cfg.Token = token;
        return cfg;
    }

    public static void Cache(string[] args)
    {
        var flags = Flags(args, out var pos);
        var sub = pos.Count > 0 ? pos[0].ToLowerInvariant() : "";
        var pm = NewPm(flags);
        var cache = pm.CacheRoot;
        switch (sub)
        {
            case "list":
                foreach (var pkgDir in CachePackageDirs(cache))
                {
                    var versions = Directory.GetDirectories(pkgDir).Select(Path.GetFileName).Where(v => v != null).OrderByDescending(v => v);
                    Console.WriteLine($"{Path.GetFileName(pkgDir)}: {string.Join(", ", versions)}");
                }
                break;
            case "verify":
            {
                // Verifica REALE (tamper-detection): confronta i file in cache con gli hash
                // registrati nel LOCK su disco — non con gli hash ricalcolati dal resolve
                // (che renderebbero il confronto tautologico). Delegata al C (LockVerify).
                var lockPath = flags.TryGetValue("lock", out var lf)
                    ? Path.GetFullPath(lf)
                    : Path.Combine(Directory.GetCurrentDirectory(), "gmb-package.lock");
                if (!File.Exists(lockPath))
                {
                    Console.WriteLine($"cache verify: manca il lock di riferimento ({lockPath}); usa --lock <file>.");
                    Environment.ExitCode = 1;
                    break;
                }
                var issues = pm.VerifyLockedIntegrity(lockPath);
                if (issues.Count == 0) { Console.WriteLine("Cache integra (vs lock)."); break; }
                foreach (var (name, list) in issues)
                    foreach (var issue in list) Console.WriteLine($"  {name}: {issue}");
                Environment.ExitCode = 1;
                break;
            }
            case "dedup":
            {
                // content-addressable: interna tutti i file dei pacchetti (blob + hardlink)
                var contentRoot = Path.Combine(cache, CacheStore.DirName);
                var n = 0;
                foreach (var pkgDir in CachePackageDirs(cache)) n += CacheStore.InternDir(contentRoot, pkgDir);
                Console.WriteLine($"dedup: {n} file internati in {contentRoot}.");
                break;
            }
            case "gc":
            {
                var removed = CacheStore.Gc(Path.Combine(cache, CacheStore.DirName), cache);
                Console.WriteLine($"gc: {removed} blob non referenziati rimossi.");
                break;
            }
            case "clean":
                // MAI cancellare il contenuto VERSIONATO del progetto: in un checkout `packages/`
                // è la cartella dei pacchetti del repo (manifest/.inc/sorgenti stanno in git), e un
                // `cache clean` cieco cancella il WORKING TREE (successo davvero il 2026-10-28).
                // La regola è in CacheCleanBlocked(), testabile senza toccare la cache vera.
                if (Parser.PackageManager.CacheCleanBlocked(cache, flags.ContainsKey("force")))
                {
                    throw new ArgumentException(
                        $"cache clean: '{cache}' è la cartella dei pacchetti del PROGETTO (checkout sorgente): " +
                        "contiene file VERSIONATI (manifest, .inc, sorgenti). Rilancia con --force per svuotarla, " +
                        "oppure usa --cache <dir> per pulire una cache separata.");
                }
                foreach (var pkgDir in CachePackageDirs(cache)) Directory.Delete(pkgDir, true);
                Console.WriteLine($"Cache pulita: {cache}");
                break;
            default:
                throw new ArgumentException("Uso: gmb cache list|verify|dedup|gc|clean [--cache dir]");
        }
    }

    private static void KeyGen(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb keygen <dir>");
        Directory.CreateDirectory(pos[0]);
        var (priv, pub) = PackageBuilder.GenerateKeyPair();
        File.WriteAllText(Path.Combine(pos[0], "private.key"), priv);
        File.WriteAllText(Path.Combine(pos[0], "public.key"), pub);
        Console.WriteLine($"KeyPair Ed25519 generata in {pos[0]} (private.key / public.key, seed 32B base64)");
    }

    private static void Run(Action<string[]> fn, string[] args)
    {
        try { fn(args); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); Environment.ExitCode = 1; }
    }

    // Flag booleani (non consumano il token successivo): evita che `--locked ./proj` diventi locked="./proj".
    private static readonly HashSet<string> BooleanFlags = new(StringComparer.OrdinalIgnoreCase)
    {
        "locked", "verify", "no-verify", "check", "yes", "insecure", "require-signed",
        "dry-run", "force", "frozen", "offline", "prefer-offline", "json", "help", "all"
    };

    private static Dictionary<string, string> Flags(string[] args, out List<string> positional)
    {
        var flags = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        positional = new List<string>();
        for (var i = 0; i < args.Length; i++)
        {
            if (args[i].StartsWith("--"))
            {
                var key = args[i][2..].ToLowerInvariant();
                // --flag value  oppure  --flag (booleano: manca il valore, segue un altro --flag, o è noto booleano)
                if (i + 1 < args.Length && !args[i + 1].StartsWith("--") && !BooleanFlags.Contains(key)) flags[key] = args[++i];
                else flags[key] = "";
            }
            else positional.Add(args[i]);
        }
        return flags;
    }

    /// <summary>Politica di fiducia firme dai flag: --require-signed = Strict; --insecure/--no-verify = Allow; default Tofu.</summary>
    private static TrustPolicy TrustPolicyOf(Dictionary<string, string> flags)
        => flags.ContainsKey("insecure") || flags.ContainsKey("no-verify") ? TrustPolicy.Allow
         : flags.ContainsKey("require-signed") ? TrustPolicy.Strict
         : TrustPolicy.Tofu;

    /// <summary>Verifica firma/fiducia di TUTTI i pacchetti risolti. Fallisce (exit 1) su firma invalida o chiave cambiata.</summary>
    private static void CheckTrust(IReadOnlyDictionary<string, InstalledPackage> pkgs, TrustPolicy policy)
    {
        if (pkgs.Count == 0) return;
        foreach (var (name, pkg) in pkgs.OrderBy(k => k.Key, StringComparer.OrdinalIgnoreCase))
        {
            var (res, _, msg) = Parser.PackageManager.VerifyInstalledTrust(pkg.Manifest, pkg.InstallPath, name, policy);
            var fail = res is TrustResult.Invalid or TrustResult.KeyChanged or TrustResult.Untrusted;
            if (fail || res != TrustResult.Trusted) Console.WriteLine($"  {(fail ? "ERR " : "firma ")}{msg}");
            if (fail) Environment.ExitCode = 1;
        }
    }

    /// <summary>Directory dei pacchetti nella cache radice, ESCLUDENDO le cartelle nascoste (.content = blob store).</summary>
    /// <summary>Scarica un URL in memoria: download su file temporaneo DELEGATO al C (gmb_net_http_download).</summary>
    private static byte[]? HttpDownloadBytes(string url)
    {
        var tmp = Path.GetTempFileName();
        try { return NativePkg.HttpDownload(url, tmp, false) ? File.ReadAllBytes(tmp) : null; }
        finally { try { File.Delete(tmp); } catch { } }
    }

    private static IEnumerable<string> CachePackageDirs(string cache)
        => Directory.Exists(cache)
            ? Directory.GetDirectories(cache).Where(d => !Path.GetFileName(d).StartsWith('.'))
            : Enumerable.Empty<string>();

    /// <summary>Verifica l'integrità della cache contro il lock file ESISTENTE (tamper-detection).</summary>
    private static bool CheckIntegrity(Parser.PackageManager pm, string root, bool fail)
    {
        var lockPath = Path.Combine(root, "gmb-package.lock");
        if (!File.Exists(lockPath)) return true;
        var issues = pm.VerifyLockedIntegrity(lockPath);
        if (issues.Count == 0) return true;
        foreach (var (n, list) in issues)
            foreach (var i in list) Console.WriteLine($"integrità {n}: {i}");
        if (fail) Environment.ExitCode = 1;
        return false;
    }

    private static void Init(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb init <dir> --name X --version 1.0.0 [--type source|dotnet|vm|c-ffi|mixed] [--edition E] [--api c-abi|gmb-native]");
        var dir = pos[0];
        var name = flags.GetValueOrDefault("name") ?? throw new ArgumentException("--name obbligatorio");
        var version = flags.GetValueOrDefault("version") ?? "1.0.0";
        var type = flags.GetValueOrDefault("type", "source") switch
        {
            "dotnet" => PackageContentType.DotNet,
            "vm" or "vmc" => PackageContentType.VmChunk,
            "c-ffi" or "cffi" => PackageContentType.CFFI,
            "mixed" => PackageContentType.Mixed,
            _ => PackageContentType.Source
        };
        var edition = flags.GetValueOrDefault("edition", "Community");
        var api = flags.GetValueOrDefault("api");
        var path = PackageBuilder.Init(dir, name, version, type, edition, api);
        Console.WriteLine($"Init: {path} (type={type}, edition={edition})");
    }

    private static void Pack(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb pack <dir> [--out out.gmbpkg] [--backend il|c99|vm|all] [--key secret|--key-file priv] [--dry-run]");
        var dir = pos[0];
        using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Path.Combine(dir, "gmb-package.json")));
        var manifest = Parser.PackageManager.ParseManifest(doc.RootElement);
        var backends = flags.TryGetValue("backend", out var b) && b == "all" ? null : b?.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        if (flags.ContainsKey("dry-run"))
        {
            var list = PackageBuilder.ComputePackFiles(manifest, dir, backends);
            Console.WriteLine($"pack --dry-run: {manifest.Name} v{manifest.Version}, {list.Count} file:");
            foreach (var f in list) Console.WriteLine($"  {f}");
            return;
        }

        var outFile = flags.GetValueOrDefault("out");
        if (string.IsNullOrEmpty(outFile)) outFile = Path.Combine(dir, $"{manifest.Name}-{manifest.Version}.gmbpkg");
        var key = flags.GetValueOrDefault("key");
        string? privatePem = null;
        if (flags.TryGetValue("key-file", out var keyFile))
        {
            // Un key-file dichiarato e INESISTENTE è un errore, non "nessuna firma": prima si
            // pubblicava un pacchetto NON firmato credendo di averlo firmato (silenzioso).
            if (!File.Exists(keyFile))
                throw new ArgumentException($"pack: key-file non trovato: {keyFile} (la firma NON è stata applicata)");
            privatePem = File.ReadAllText(keyFile);
        }
        PackageBuilder.Pack(dir, outFile, backends, key, privatePem);
        Console.WriteLine($"Pack: {Path.GetFullPath(outFile)} (firma: {(privatePem != null ? "pubblica Ed25519" : key != null ? "pubblica Ed25519 (seed SHA-256 di --key)" : "nessuna")})");
    }

    // gmb add <name[@ver]> [--dev] [--dir dir] — aggiunge/aggiorna una dipendenza nel gmb-package.json.
    public static void Add(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb add <name[@version]> [--dev] [--dir dir]");
        var dir = Path.GetFullPath(flags.GetValueOrDefault("dir") ?? Directory.GetCurrentDirectory());
        var manPath = Path.Combine(dir, "gmb-package.json");
        if (!File.Exists(manPath)) throw new ArgumentException($"gmb-package.json non trovato in {dir} (usa 'gmb init').");
        var spec = pos[0];
        var at = spec.IndexOf('@');
        var name = at > 0 ? spec[..at] : spec;
        var ver = at > 0 ? spec[(at + 1)..] : "";
        var range = ver.Length == 0 ? "*" : (Parser.PackageManager.ParseSemVer(ver) != null ? "^" + ver : ver);
        var section = flags.ContainsKey("dev") ? "devDependencies" : "dependencies";
        // Edit del manifest DELEGATO al C (gmb_pkg_manifest_set_dep): nessuna logica JSON in C#.
        if (!NativePkg.SetDep(manPath, section, name, range)) throw new ArgumentException($"add fallito su {manPath}");
        Console.WriteLine($"{section}: {name} {range}  ({manPath})");
    }

    // gmb remove <name> [--dir dir] — rimuove una dipendenza dal gmb-package.json.
    public static void RemoveDep(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb remove <name> [--dir dir]");
        var dir = Path.GetFullPath(flags.GetValueOrDefault("dir") ?? Directory.GetCurrentDirectory());
        var manPath = Path.Combine(dir, "gmb-package.json");
        if (!File.Exists(manPath)) throw new ArgumentException($"gmb-package.json non trovato in {dir}.");
        // Rimozione DELEGATA al C (gmb_pkg_manifest_remove_dep): nessuna logica JSON in C#.
        if (!NativePkg.RemoveDep(manPath, pos[0])) throw new ArgumentException($"remove fallito su {manPath}");
        Console.WriteLine($"rimosso: {pos[0]}  ({manPath})");
    }

    // gmb bump <dir> major|minor|patch|prerelease|X.Y.Z — aggiorna "version" nel manifest.
    public static void Bump(string[] args)
    {
        var flags = Flags(args, out var pos);
        _ = flags;
        if (pos.Count < 2) throw new ArgumentException("Uso: gmb bump <dir> <major|minor|patch|prerelease|X.Y.Z>");
        var manPath = Path.Combine(pos[0], "gmb-package.json");
        if (!File.Exists(manPath)) throw new ArgumentException($"gmb-package.json non trovato in {pos[0]}");
        var text = File.ReadAllText(manPath);
        using var doc = System.Text.Json.JsonDocument.Parse(text);
        var cur = Parser.PackageManager.ParseSemVer(doc.RootElement.GetProperty("version").GetString() ?? "")
            ?? throw new ArgumentException("la versione corrente non è SemVer");
        var newVer = pos[1].ToLowerInvariant() switch
        {
            "major" => $"{cur.Major + 1}.0.0",
            "minor" => $"{cur.Major}.{cur.Minor + 1}.0",
            "patch" => $"{cur.Major}.{cur.Minor}.{cur.Patch + 1}",
            "prerelease" or "pre" => $"{cur.Major}.{cur.Minor}.{cur.Patch + 1}-rc.1",
            _ => Parser.PackageManager.ParseSemVer(pos[1]) != null ? pos[1] : throw new ArgumentException($"bump non valido: '{pos[1]}'")
        };
        var updated = System.Text.RegularExpressions.Regex.Replace(text, "(\"version\"\\s*:\\s*\")[^\"]*(\")",
            m => m.Groups[1].Value + newVer + m.Groups[2].Value, System.Text.RegularExpressions.RegexOptions.None, TimeSpan.FromSeconds(5));
        File.WriteAllText(manPath, updated);
        Console.WriteLine($"bump: {cur} -> {newVer}  ({manPath})");
    }

    // gmb restore [dir] [--locked] [--registry url] [--cache dir] [--verify] [--require-signed]
    private static void Restore(string[] args)
    {
        var flags = Flags(args, out var pos);
        var root = pos.Count > 0 ? Path.GetFullPath(pos[0]) : Directory.GetCurrentDirectory();
        var pm = NewPm(flags);
        var ctx = Ctx(flags);
        ctx.Overrides = ReadProjectOverrides(root);
        ctx.LocalPackages = DiscoverLocalPackages(root);
        // integrità contro il lock ESISTENTE (prima di riscriverlo)
        if (flags.ContainsKey("verify") && !CheckIntegrity(pm, root, fail: true))
        { Console.WriteLine("--verify: integrità fallita, restore annullato."); return; }
        DoRestore(pm, ctx, root, flags.ContainsKey("locked"), flags.ContainsKey("frozen"), flags.ContainsKey("production"));
        CheckPeers(pm);
        CheckTrust(pm.ResolvedPackages, TrustPolicyOf(flags));
    }

    private static void CheckPeers(Parser.PackageManager pm)
        => (pm.CheckPeers()).ForEach(w => Console.WriteLine($"  peer: {w}"));

    // gmb rollback [dir] [--list] [--to <timestamp|prefix>] — ripristina uno storico lock e re-installa.
    public static void Rollback(string[] args)
    {
        var flags = Flags(args, out var pos);
        var root = Path.GetFullPath(pos.Count > 0 ? pos[0] : Directory.GetCurrentDirectory());
        var histDir = Path.Combine(root, ".gmbasic", "lock-history");
        var files = Directory.Exists(histDir)
            ? Directory.GetFiles(histDir, "*.lock").OrderByDescending(f => f).ToList()
            : new List<string>();

        if (flags.ContainsKey("list"))
        {
            if (files.Count == 0) { Console.WriteLine("(nessuno storico lock)"); return; }
            foreach (var f in files) Console.WriteLine(Path.GetFileName(f));
            return;
        }
        if (files.Count == 0) throw new ArgumentException($"Nessuno storico lock in {histDir}");

        var to = flags.GetValueOrDefault("to");
        var chosen = to is null ? files[0]
            : files.FirstOrDefault(f => Path.GetFileName(f).StartsWith(to, StringComparison.OrdinalIgnoreCase))
              ?? throw new ArgumentException($"Storico lock non trovato: {to}");

        var lockPath = Path.Combine(root, "gmb-package.lock");
        Directory.CreateDirectory(histDir);
        if (File.Exists(lockPath)) // archivia il corrente (rollback reversibile)
            File.Copy(lockPath, Path.Combine(histDir, DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff") + ".lock"), true);
        File.Copy(chosen, lockPath, true);
        Console.WriteLine($"rollback: lock ripristinato da {Path.GetFileName(chosen)}");

        var pm = NewPm(flags);
        var ctx = Ctx(flags);
        ctx.Overrides = ReadProjectOverrides(root);
        ctx.LocalPackages = DiscoverLocalPackages(root);
        DoRestore(pm, ctx, root, locked: true);
    }

    // gmb trust add <name> <pubkey|file> | remove <name> | list | fingerprint <pubkey|file>
    public static void Trust(string[] args)
    {
        var sub = args.Length > 0 ? args[0].ToLowerInvariant() : "list";
        var rest = args.Skip(1).ToArray();
        static string Key(string s) => File.Exists(s) ? File.ReadAllText(s).Trim() : s;
        switch (sub)
        {
            case "add":
                if (rest.Length < 2) throw new ArgumentException("Uso: gmb trust add <name> <publicKey|publicKeyFile>");
                var key = Key(rest[1]);
                TrustStore.Add(rest[0], key);
                Console.WriteLine($"Fidato: {rest[0]} ({TrustStore.Fingerprint(key)})");
                break;
            case "remove":
            case "rm":
                if (rest.Length < 1) throw new ArgumentException("Uso: gmb trust remove <name>");
                Console.WriteLine(TrustStore.Remove(rest[0]) ? $"Rimosso: {rest[0]}" : $"Non presente: {rest[0]}");
                break;
            case "fingerprint":
            case "fp":
                if (rest.Length < 1) throw new ArgumentException("Uso: gmb trust fingerprint <publicKey|file>");
                Console.WriteLine(TrustStore.Fingerprint(Key(rest[0])));
                break;
            case "list":
                var keys = TrustStore.Load();
                if (keys.Count == 0) { Console.WriteLine("(keyring vuoto)"); break; }
                foreach (var (n, k) in keys.OrderBy(x => x.Key, StringComparer.OrdinalIgnoreCase))
                    Console.WriteLine($"{n}: {TrustStore.Fingerprint(k)}");
                break;
            default:
                throw new ArgumentException("Uso: gmb trust add|remove|list|fingerprint");
        }
    }

    // Logica condivisa di restore (usata dal CLI 'gmb restore' e dal REPL 'RESTORE').
    //   locked=false -> scansiona i sorgenti (.bas/.inc) per REQUIRE (ScanRequirementsFromTokens)
    //   locked=true  -> usa le versioni pinnate in gmb-package.lock (riproducibile / offline)
    // In entrambi i casi installa le dipendenze (download se mancanti) e riscrive il lock.
    public static void DoRestore(Parser.PackageManager pm, PackageBuildContext ctx, string root, bool locked, bool frozen = false, bool production = false)
    {
        var reqs = new List<RequireStmt>();
        if (locked)
        {
            var lockPath = Path.Combine(root, "gmb-package.lock");
            var lockedVers = Parser.PackageManager.ReadLockFile(lockPath);
            if (lockedVers.Count == 0) { Console.WriteLine($"--locked: lock non trovato o vuoto ({lockPath})."); return; }
            foreach (var (n, v) in lockedVers) reqs.Add(Req(n, v));
            Console.WriteLine($"Lock ({reqs.Count}): {string.Join(", ", reqs.Select(r => r.PackageName.Value + "@" + r.VersionSpecifier!.Value))}");
        }
        else
        {
            var sep = Path.DirectorySeparatorChar;
            var files = Directory.EnumerateFiles(root, "*.bas", SearchOption.AllDirectories)
                .Concat(Directory.EnumerateFiles(root, "*.inc", SearchOption.AllDirectories))
                .Where(f => !f.Contains($"{sep}vendor{sep}") && !f.Contains($"{sep}.gmb{sep}")
                         && !f.Contains($"{sep}obj{sep}") && !f.Contains($"{sep}bin{sep}"))
                .Distinct().ToList();
            var program = new BasicProgram(SettingsManager.LoadSettings(Path.Combine(root, "restore.bas")), null, root);
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var f in files)
            {
                string text;
                try { text = File.ReadAllText(f); } catch { continue; }
                var lines = text.Split(["\r\n", "\r", "\n"], StringSplitOptions.None);
                var fileId = program.GetOrAddFileId(f);
                var smb = ImmutableArray.CreateBuilder<SourceLine>();
                for (var i = 0; i < lines.Length; i++) smb.Add(new SourceLine(fileId, f, i + 1, lines[i]));
                var sm = smb.ToImmutable();
                var toks = new Lexer(sm, program, false).ScanTokens().ToImmutableArray();
                foreach (var r in Parser.ScanRequirementsFromTokens(toks, sm, program, f, StdLib.Instance))
                    if (seen.Add(r.PackageName.Value + "@" + (r.VersionSpecifier?.Value ?? "*"))) reqs.Add(r);
            }
            Console.WriteLine(reqs.Count == 0
                ? $"Nessun REQUIRE trovato in {root} (si guarda anche il manifesto di progetto)."
                : $"REQUIRE trovati ({reqs.Count}): {string.Join(", ", reqs.Select(r => r.PackageName.Value))}");
        }

        // ── Manifesto di PROGETTO (gmb-package.json) ────────────────────────────────────
            // L'installazione e' compito della CLI, non del compilatore: e' qui che le dipendenze
            // dichiarate nel manifesto vengono unite a quelle dei REQUIRE. Ogni dichiarazione
            // entra come vincolo INDIPENDENTE: due vincoli sullo stesso pacchetto si intersecano,
            // quindi un REQUIRE e un manifesto coerenti si comportano come prima.
            var manifestSpecs = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (!locked)
            {
                var projManifest = Parser.PackageManager.ReadProjectManifestAsync(root).GetAwaiter().GetResult();
                if (projManifest != null)
                {
                    var declared = new List<KeyValuePair<string, string>>(projManifest.Dependencies);
                    declared.AddRange(projManifest.OptionalDependencies);
                    if (!production) declared.AddRange(projManifest.DevDependencies); // dev deps solo del root, escluse con --production
                    foreach (var (_, wdir) in DiscoverLocalPackages(root))
                    {
                        try
                        {
                            var wm = Parser.PackageManager.ReadManifestFromDirAsync(wdir).GetAwaiter().GetResult();
                            declared.AddRange(wm.Dependencies);
                            declared.AddRange(wm.OptionalDependencies);
                            if (!production) declared.AddRange(wm.DevDependencies);
                        }
                        catch { }
                    }
                    foreach (var (depName, depSpec) in declared)
                    {
                        if (string.IsNullOrWhiteSpace(depName)) continue;
                        manifestSpecs[depName] = string.IsNullOrWhiteSpace(depSpec) ? "*" : depSpec.Trim();
                    }
                    if (manifestSpecs.Count > 0)
                        Console.WriteLine($"Manifesto di progetto: {manifestSpecs.Count} dipendenze dichiarate"
                            + (production ? " (--production: dev escluse)" : "") + ".");
                    // Fase 5: le feature dichiarate dal progetto (chiavi di `features`) → ctx.Features,
                    // così i pacchetti attivano i loro gruppi opzionali. Dev deps: già unite sopra.
                    if (projManifest.Features.Count > 0)
                    {
                        ctx.Features = projManifest.Features.Keys.ToList();
                        Console.WriteLine($"Manifesto di progetto: feature attivate: {string.Join(", ", ctx.Features)}.");
                    }
                }
            }

            // Ricostruisce la lista: REQUIRE + manifesto (uno o due vincoli per pacchetto).
            // `unionFor` elenca i pacchetti in conflitto: per quelli i due vincoli diventano un
            // unico OR, cosi' vince la versione PIU' NUOVA invece di fallire.
            var unionFor = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            List<RequireStmt> BuildRequests()
            {
                var byName = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
                foreach (var r in reqs)
                {
                    if (!byName.TryGetValue(r.PackageName.Value, out var l)) byName[r.PackageName.Value] = l = [];
                    l.Add(r.VersionSpecifier?.Value ?? "*");
                }
                foreach (var (n, spec) in manifestSpecs)
                {
                    if (!byName.TryGetValue(n, out var l)) byName[n] = l = [];
                    l.Add(spec);
                }
                var list = new List<RequireStmt>();
                foreach (var (n, specs) in byName)
                {
                    var distinct = specs.Where(x => !string.IsNullOrWhiteSpace(x)).Distinct().ToList();
                    if (unionFor.Contains(n) && distinct.Count > 1)
                        list.Add(Req(n, string.Join(" || ", distinct)));   // conflitto: vince la piu' nuova
                    else
                        foreach (var x in distinct) list.Add(Req(n, x)); // intersezione (comportamento di prima)
                }
                return list;
            }

        // La regola del merge: se i vincoli di un pacchetto NON si intersecano, vince la versione
        // PIU' NUOVA. Lo stabilisce l'engine gia' esistente (nessuna logica duplicata): si prova
        // l'intersezione, e solo per il pacchetto indicato dall'errore si riprova con l'OR dei
        // vincoli. Il conflitto viene comunicato, mai risolto in silenzio.
        try
        {
        for (var attempt = 0; attempt <= manifestSpecs.Count; attempt++)
        {
            reqs = BuildRequests();
            if (reqs.Count == 0)
            {
                Console.WriteLine($"Nessuna dipendenza da installare in {root}.");
                return;
            }
            try
            {
                pm.ResolveDependenciesAsync(reqs, ctx).GetAwaiter().GetResult();
                break;
            }
            catch (BasicException ex) when (ex.Code == ErrorCode.FileNotFound && !locked)
            {
                /* NON e' ogni FileNotFound un conflitto: "non trovato nella cache" significa che
                 * va INSTALLATO (il fallback sotto lo fa dal registry), non che i vincoli non si
                 * intersecano. Confletto = messaggio "non soddisfa i vincoli" E pacchetto
                 * dichiarato sia nei REQUIRE sia nel manifesto. Tutto il resto si rilancia, cosi'
                 * l'installazione procede come prima. */
                if (!ex.Message.Contains("soddisfa i vincoli", StringComparison.OrdinalIgnoreCase)) throw;
                var m = System.Text.RegularExpressions.Regex.Match(ex.Message, "'([^']+)'");
                var conflicting = m.Success ? m.Groups[1].Value : null;
                if (conflicting == null || !manifestSpecs.ContainsKey(conflicting) || !unionFor.Add(conflicting)) throw;
                var reqSpec = reqs.FirstOrDefault(r => r.PackageName.Value.Equals(conflicting, StringComparison.OrdinalIgnoreCase))?.VersionSpecifier?.Value;
                Console.WriteLine($"  conflitto su '{conflicting}': [{reqSpec}] (REQUIRE) vs [{manifestSpecs[conflicting]}] (manifesto) -> vince la piu' nuova");
            }
        }
        if (reqs.Count == 0) { Console.WriteLine($"Nessuna dipendenza da risolvere in {root}."); return; }
        pm.ResolveDependenciesAsync(reqs, ctx).GetAwaiter().GetResult();
        }
        catch (BasicException ex) when (ex.Code == ErrorCode.FileNotFound)
        {
            if (ctx.Offline)
            {
                Console.WriteLine($"--offline: dipendenza non in cache/vendor — {ex.Message}");
                Environment.ExitCode = 1;
                return;
            }
            // Cache vuota/parziale: installa i mancanti dal registry IN PARALLELO (max 4) con progress.
            var todo = reqs.GroupBy(r => r.PackageName.Value, StringComparer.OrdinalIgnoreCase)
                           .Select(g => (Name: g.Key, Ver: g.First().VersionSpecifier?.Value))
                           .ToList();
            using var gate = new SemaphoreSlim(4);
            var tasks = todo.Select(async t =>
            {
                await gate.WaitAsync();
                try
                {
                    await pm.DownloadPackageAsync(t.Name, t.Ver, ctx: ctx);
                    Console.WriteLine($"  installato: {t.Name}" + (t.Ver != null ? $"@{t.Ver}" : ""));
                }
                catch (Exception dl) { Console.WriteLine($"  FAIL {t.Name}: {dl.Message}"); Environment.ExitCode = 1; }
                finally { gate.Release(); }
            }).ToList();
            Task.WhenAll(tasks).GetAwaiter().GetResult();
            pm.ResolveDependenciesAsync(reqs, ctx).GetAwaiter().GetResult();
        }

        var lockFile = Path.Combine(root, "gmb-package.lock");
        if (frozen)
        {
            // CI: il lock NON deve cambiare. Confronta le versioni risolte col lock ESISTENTE.
            var existing = Parser.PackageManager.ReadLockFile(lockFile);
            if (existing.Count == 0)
            { Console.WriteLine("--frozen: nessun lock presente (esegui prima 'gmb restore')."); Environment.ExitCode = 1; return; }
            var fresh = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var (name, line) in pm.LockData)
            {
                var at = line.IndexOf('@'); var bar = at >= 0 ? line.IndexOf('|', at + 1) : -1;
                if (at >= 0 && bar > at) fresh[name] = line[(at + 1)..bar];
            }
            var changed = fresh.Where(kv => !existing.TryGetValue(kv.Key, out var v) || v != kv.Value).ToList();
            var removed = existing.Keys.Where(k => !fresh.ContainsKey(k)).ToList();
            if (changed.Count > 0 || removed.Count > 0)
            {
                Console.WriteLine("--frozen: il lock divergerebbe dai REQUIRE:");
                foreach (var (k, v) in changed) Console.WriteLine($"  {k}: lock {existing.GetValueOrDefault(k, "(assente)")} -> risolto {v}");
                foreach (var k in removed) Console.WriteLine($"  {k}: non piu' richiesto");
                Environment.ExitCode = 1;
                return;
            }
            Console.WriteLine("--frozen: lock coerente (nessuna scrittura).");
            return;
        }

        pm.WriteLockFile(root);
        Console.WriteLine($"restore completato. Lock: {lockFile}");
    }

    // ── UPDATE (brew upgrade del progetto) ──────────────────────────────────────────────────
    // Aggiorna TUTTI i pacchetti del progetto (dai REQUIRE .bas/.inc + dalle dipendenze in
    // gmb-package.json) all'ultima versione consentita dai vincoli; riscrive gmb-package.lock.
    //   gmb update [dir] [--check]   --check = solo il piano (dry-run), non tocca la cache
    //   --yes  = nessuna conferma   ·   --registry/--cache come restore
    private static void Update(string[] args)
    {
        var flags = Flags(args, out var pos);
        var root = Path.GetFullPath(pos.Count > 0 ? pos[0] : Directory.GetCurrentDirectory());
        var pm = NewPm(flags);
        var ctx = Ctx(flags);
        ctx.Overrides = ReadProjectOverrides(root);
        ctx.LocalPackages = DiscoverLocalPackages(root);
        CheckIntegrity(pm, root, fail: flags.ContainsKey("verify"));
        DoUpdate(pm, ctx, root, flags.ContainsKey("check"), flags.ContainsKey("yes"), flags.ContainsKey("production"));
        CheckPeers(pm);
        CheckTrust(pm.ResolvedPackages, TrustPolicyOf(flags));
    }

    /// <summary>Core di <c>gmb update</c>: aggiorna i pacchetti del progetto. Riusabile dal REPL.</summary>
    public static void DoUpdate(Parser.PackageManager pm, PackageBuildContext ctx, string root, bool check, bool yes, bool production = false)
    {
        var reqs = CollectProjectRequirements(root, production);
        if (reqs.Count == 0) { Console.WriteLine($"Nessuna dipendenza trovata in {root}."); return; }

        // Risolve prima (per conoscere l'installato); se la cache è vuota, prosegue e basta.
        try { pm.ResolveDependenciesAsync(reqs, ctx).GetAwaiter().GetResult(); }
        catch (BasicException) { /* pacchetti non in cache: verranno installati sotto */ }

        var names = new HashSet<string>(reqs.Select(r => r.PackageName.Value), StringComparer.OrdinalIgnoreCase);
        var constraintsByName = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var r in reqs)
        {
            if (!constraintsByName.TryGetValue(r.PackageName.Value, out var l)) constraintsByName[r.PackageName.Value] = l = [];
            var spec = r.VersionSpecifier?.Value;
            if (!string.IsNullOrWhiteSpace(spec)) l.Add(spec);
        }
        var outdated = pm.GetOutdatedAsync(constraintsByName, ctx).GetAwaiter().GetResult()
            .Where(kv => names.Contains(kv.Key))
            .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase).ToList();

        if (outdated.Count == 0) { Console.WriteLine($"Tutto aggiornato ({reqs.Count} pacchetti del progetto)."); return; }

        Console.WriteLine($"Aggiornamenti disponibili ({outdated.Count}):");
        foreach (var (n, pair) in outdated) Console.WriteLine($"  {n}: {pair.Installed} -> {pair.Latest}");
        if (check) { Console.WriteLine("(--check: nessuna modifica applicata)"); return; }

        if (!yes && !Console.IsInputRedirected && !Console.IsOutputRedirected)
        {
            Console.Write("Procedere con l'aggiornamento? [y/N] ");
            var a = Console.ReadLine();
            if (a == null || !(a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase) || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase)))
            { Console.WriteLine("annullato."); return; }
        }

        foreach (var (n, _) in outdated)
        {
            try { pm.UpdatePackageAsync(n, ctx).GetAwaiter().GetResult(); Console.WriteLine($"  aggiornato: {n}"); }
            catch (Exception ex) { Console.WriteLine($"  FAIL {n}: {ex.Message}"); Environment.ExitCode = 1; }
        }

        try { pm.ResolveDependenciesAsync(CollectProjectRequirements(root, production), ctx).GetAwaiter().GetResult(); }
        catch (BasicException) { }
        pm.WriteLockFile(root);
        Console.WriteLine($"update completato. Lock: {Path.Combine(root, "gmb-package.lock")}");
    }

    /// <summary>Vincoli del progetto: REQUIRE (.bas/.inc) + dependencies (+ dev se non --production, + optional) di gmb-package.json.</summary>
    public static List<RequireStmt> CollectProjectRequirements(string root, bool production = false)
    {
        var reqs = new List<RequireStmt>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var sep = Path.DirectorySeparatorChar;
        var files = Directory.EnumerateFiles(root, "*.bas", SearchOption.AllDirectories)
            .Concat(Directory.EnumerateFiles(root, "*.inc", SearchOption.AllDirectories))
            .Where(f => !f.Contains($"{sep}vendor{sep}") && !f.Contains($"{sep}.gmb{sep}")
                     && !f.Contains($"{sep}obj{sep}") && !f.Contains($"{sep}bin{sep}") && !f.Contains($"{sep}work{sep}"))
            .Distinct().ToList();
        var program = new BasicProgram(SettingsManager.LoadSettings(Path.Combine(root, "restore.bas")), null, root);
        foreach (var f in files)
        {
            string text;
            try { text = File.ReadAllText(f); } catch { continue; }
            var lines = text.Split(["\r\n", "\r", "\n"], StringSplitOptions.None);
            var fileId = program.GetOrAddFileId(f);
            var smb = ImmutableArray.CreateBuilder<SourceLine>();
            for (var i = 0; i < lines.Length; i++) smb.Add(new SourceLine(fileId, f, i + 1, lines[i]));
            var sm = smb.ToImmutable();
            var toks = new Lexer(sm, program, false).ScanTokens().ToImmutableArray();
            foreach (var r in Parser.ScanRequirementsFromTokens(toks, sm, program, f, StdLib.Instance))
                if (seen.Add(r.PackageName.Value)) reqs.Add(r);
        }
        // Manifesto di progetto: aggiunge le dipendenze come vincoli (il resolver le interseca).
        try
        {
            var projManifest = Parser.PackageManager.ReadProjectManifestAsync(root).GetAwaiter().GetResult();
            if (projManifest != null)
            {
                var declared = new List<KeyValuePair<string, string>>(projManifest.Dependencies);
                declared.AddRange(projManifest.OptionalDependencies);
                if (!production) declared.AddRange(projManifest.DevDependencies);
                // dipendenze dei workspace (monorepo): ognuno contribuisce al lock unico del root.
                foreach (var (_, wdir) in DiscoverLocalPackages(root))
                {
                    try
                    {
                        var wm = Parser.PackageManager.ReadManifestFromDirAsync(wdir).GetAwaiter().GetResult();
                        declared.AddRange(wm.Dependencies);
                        declared.AddRange(wm.OptionalDependencies);
                        if (!production) declared.AddRange(wm.DevDependencies);
                    }
                    catch { }
                }
                foreach (var (n, spec) in declared)
                    if (!string.IsNullOrWhiteSpace(n))
                        reqs.Add(Req(n, string.IsNullOrWhiteSpace(spec) ? "*" : spec.Trim()));
            }
        }
        catch { /* nessun manifesto di progetto */ }
        return reqs;
    }

    /// <summary>Scopre i pacchetti LOCALI del workspace (monorepo) dal manifest root: espande i glob → mappa nome→dir.</summary>
    private static Dictionary<string, string> DiscoverLocalPackages(string root)
    {
        var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        try
        {
            var proj = Parser.PackageManager.ReadProjectManifestAsync(root).GetAwaiter().GetResult();
            if (proj == null || proj.Workspaces.IsDefaultOrEmpty) return map;
            var dirs = new List<string>();
            foreach (var glob in proj.Workspaces)
            {
                var g = glob.Replace('\\', '/').TrimEnd('/');
                var star = g.IndexOf('*');
                if (star < 0)
                {
                    var d = Path.Combine(root, g.Replace('/', Path.DirectorySeparatorChar));
                    if (Directory.Exists(d)) dirs.Add(d);
                    continue;
                }
                var baseDir = Path.Combine(root, g[..star].TrimEnd('/').Replace('/', Path.DirectorySeparatorChar));
                if (Directory.Exists(baseDir))
                    dirs.AddRange(Directory.GetDirectories(baseDir).Where(d => File.Exists(Path.Combine(d, "gmb-package.json"))));
            }
            foreach (var d in dirs.Distinct())
            {
                try
                {
                    var m = Parser.PackageManager.ReadManifestFromDirAsync(d).GetAwaiter().GetResult();
                    if (!string.IsNullOrWhiteSpace(m.Name)) map[m.Name] = Path.GetFullPath(d);
                }
                catch { }
            }
        }
        catch { }
        return map;
    }

    /// <summary>Overrides del progetto (gmb-package.json → overrides) per il contesto di build.</summary>
    private static Dictionary<string, string> ReadProjectOverrides(string root)
    {
        try
        {
            var m = Parser.PackageManager.ReadProjectManifestAsync(root).GetAwaiter().GetResult();
            if (m != null && m.Overrides.Count > 0)
                return m.Overrides.ToDictionary(kv => kv.Key, kv => kv.Value, StringComparer.OrdinalIgnoreCase);
        }
        catch { }
        return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
    }

    private static Parser.PackageManager NewPm(Dictionary<string, string> flags)
    {
        var pm = new Parser.PackageManager();
        if (flags.TryGetValue("registry", out var reg)) { pm.RegistryUrl = reg; pm.RegistryUrls = [reg]; }
        if (flags.TryGetValue("cache", out var cache)) pm.CacheRoot = Path.GetFullPath(cache);
        return pm;
    }

    private static PackageBuildContext Ctx(Dictionary<string, string> flags)
        => new()
        {
            Backend = "il",
            VendorDir = flags.GetValueOrDefault("vendor"),
            Offline = flags.ContainsKey("offline"), // --prefer-offline = comportamento di default (cache-first, fallback rete)
            Registries = GmbConfig.Load().Registries // registry per-scope/nome (@org → url)
        };

    public static void Pkg(string[] args)
    {
        var flags = Flags(args, out var pos);
        var sub = pos.Count > 0 ? pos[0].ToLowerInvariant() : "";
        var pm = NewPm(flags);
        switch (sub)
        {
            case "install":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg install <name[@ver]> [--local] [--registry url] [--cache dir]");
                var spec = pos[1];
                var at = spec.IndexOf('@');
                var name = at > 0 ? spec[..at] : spec;
                var ver = at > 0 ? spec[(at + 1)..] : null;
                // --local: installa nella cartella `packages` dell'ESEGUIBILE (modello node_modules),
                // spostando dalla cache la stessa versione se c'è, altrimenti scaricandola.
                // SENZA --local: destinazione la CACHE — e se lì manca ma la stessa versione è
                // nell'installazione locale, la SPOSTA (direttiva: "sposta in cache i package
                // locali con la stessa versione, oppure li installa se assenti").
                var useLocal = flags.ContainsKey("local");
                var localDir = CommonLib.GmbiPaths.LocalPackagesDir;
                if (useLocal) Directory.CreateDirectory(localDir);   // creata se manca (direttiva)
                var targetDir = useLocal ? localDir : pm.CacheRoot;
                var pkg = pm.InstallIntoAsync(name, ver, targetDir).GetAwaiter().GetResult();
                // Gate di compatibilità + fiducia firma (keyring/TOFU) sul pacchetto appena installato.
                Parser.PackageManager.CheckCompatibility(pkg.Manifest, Ctx(flags));
                var (trustRes, _, trustMsg) = Parser.PackageManager.VerifyInstalledTrust(
                    pkg.Manifest, pkg.InstallPath, pkg.Manifest.Name, TrustPolicyOf(flags));
                Console.WriteLine($"Installed{(useLocal ? " (local)" : "")}: {pkg.Manifest.Name} v{pkg.Manifest.Version} -> {pkg.InstallPath}");
                Console.WriteLine($"  firma: {trustMsg}");
                if (trustRes is TrustResult.Invalid or TrustResult.KeyChanged or TrustResult.Untrusted)
                { Environment.ExitCode = 1; }
                break;
            }
            case "build":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg build <dir[:packaging]>  (compila il C del pacchetto per la piattaforma corrente)");
                var pdir = Path.GetFullPath(pos[1]);
                var manPath = File.Exists(pdir) ? pdir
                    : File.Exists(Path.Combine(pdir, "gmb-package.json")) ? Path.Combine(pdir, "gmb-package.json")
                    : Path.Combine(pdir, "packaging", "gmb-package.json");
                if (!File.Exists(manPath)) throw new ArgumentException($"gmb-package.json non trovato in {pdir}");
                using var bdoc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(manPath));
                var bman = Parser.PackageManager.ParseManifest(bdoc.RootElement);
                var bdir = Path.GetDirectoryName(manPath)!;
                var plat = Parser.PackageManager.GetCurrentPlatformKey();
                Console.WriteLine($"[build] {bman.Name} @ {plat} ...");
                var (ok, log) = PackageBuilder.RunBuild(bdir, bman, plat);
                Console.WriteLine(log);
                if (!ok) { Environment.ExitCode = 1; } else { Console.WriteLine("[build] OK"); }
                break;
            }
            case "list":
            {
                var cache = pm.CacheRoot;
                foreach (var pkgDir in CachePackageDirs(cache))
                {
                    var versions = Directory.GetDirectories(pkgDir).Select(Path.GetFileName).Where(v => v != null).OrderByDescending(v => v);
                    Console.WriteLine($"{Path.GetFileName(pkgDir)}: {string.Join(", ", versions)}");
                }
                break;
            }
            case "remove":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg remove <name> [--cache dir]");
                var pkgDir = Path.Combine(pm.CacheRoot, pos[1]);
                if (!Directory.Exists(pkgDir)) throw new ArgumentException($"Non installato: {pos[1]}");
                Directory.Delete(pkgDir, true);
                Console.WriteLine($"Removed: {pos[1]}");
                break;
            }
            case "info":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg info <name> [--cache dir]");
                var pmLocal = NewPm(flags);
                pmLocal.ResolveDependenciesAsync([new RequireStmt(new StringExpr(pos[1], new Token(TokenKind.StringLiteral, pos[1], pos[1], SourcePosition.Unknown)), null, new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown))], Ctx(flags)).GetAwaiter().GetResult();
                var art = pmLocal.ResolvedArtifacts[pos[1]];
                Console.WriteLine($"{art.PackageName}@{art.Version}");
                Console.WriteLine($"  contentType={art.ContentType} backend={art.Backend} platform={art.Platform}");
                if (art.SourceDir != null) Console.WriteLine($"  source={art.SourceDir}");
                if (art.BinaryPath != null) Console.WriteLine($"  binary={art.BinaryPath}");
                if (art.ChunkPath != null) Console.WriteLine($"  chunk={art.ChunkPath}");
                if (art.FfiLibs.Length > 0) Console.WriteLine($"  ffi libs={string.Join(",", art.FfiLibs)} link={art.FfiLink}");
                break;
            }
            case "search":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg search <term> [--registry url]");
                var reg = pm.RegistryUrl.TrimEnd('/');
                var json = NativePkg.RegistryGet($"{reg}/api/packages/search?q={Uri.EscapeDataString(pos[1])}");
                /* Registry irraggiungibile = ERRORE, non output vuoto (sembrava "nessun risultato"). */
                if (json == null)
                    throw new PackageException(
                        $"search: registry NON RAGGIUNGIBILE su {reg}. Verifica che il server sia attivo e l'URL (--registry / gmb login).");
                Console.WriteLine(json);
                break;
            }
            case "tree":
            {
                var lockPath = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var reqs = new List<RequireStmt>();
                // con --lock: lega i nomi dalle versioni pinnate
                var locked = Parser.PackageManager.ReadLockFile(lockPath);
                foreach (var (n, v) in locked)
                    reqs.Add(Req(n, v));
                if (reqs.Count == 0) throw new ArgumentException("Uso: gmb pkg tree [--lock gmb-package.lock]");
                pm.ResolveDependenciesAsync(reqs, Ctx(flags), lockPath).GetAwaiter().GetResult();
                foreach (var (parent, kids) in pm.GetDependencyTree().OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
                {
                    // `-> ` con niente a destra sembrava un bug: i pacchetti senza dipendenze
                    // (il caso normale) vanno detti esplicitamente, con la versione risolta.
                    var ver = pm.ResolvedPackages.TryGetValue(parent, out var ip) ? "@" + ip.Manifest.Version : "";
                    var rhs = !kids.Any() ? "(nessuna dipendenza)" : string.Join(", ", kids);
                    Console.WriteLine($"{parent}{ver} -> {rhs}");
                }
                break;
            }
            case "owners":
            {
                if (pos.Count < 4) throw new ArgumentException("Uso: gmb pkg owners <add|remove> <package> <user> [--registry url]");
                var result = pm.ManageOwnersAsync(pos[2], pos[1], pos[3], flags.GetValueOrDefault("registry"),
                                                  flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
                Console.WriteLine(result);
                break;
            }
            case "yank":
            {
                if (pos.Count < 3) throw new ArgumentException("Uso: gmb pkg yank <package> <version> [--un] [--reason r] [--registry url]");
                // --un è un flag booleano: Flags() lo tratta come flag con valore e mangia
                // l'argomento successivo, sporcando i posizionali. Estraiamo i veri argomenti
                // dal raw args: il 3° e 4° elemento dopo il verbo (sub) sono name e version,
                // saltando eventuali --flag* e i loro valori.
                var tail = args.Skip(1).Where(a => !a.StartsWith('-')).ToList();
                var un = args.Contains("--un", StringComparer.OrdinalIgnoreCase);
                if (tail.Count < 2) throw new ArgumentException("Uso: gmb pkg yank <package> <version> [--un] [--reason r] [--registry url]");
                // registry/url direttamente dal raw (Flags può averlo ingoiato come valore di --un)
                var regIdx = Array.FindIndex(args, a => a.Equals("--registry", StringComparison.OrdinalIgnoreCase));
                var registryRaw = regIdx >= 0 && regIdx + 1 < args.Length ? args[regIdx + 1] : flags.GetValueOrDefault("registry");
                var result = pm.YankVersionAsync(tail[0], tail[1], !un, flags.GetValueOrDefault("reason"), registryRaw, flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
                Console.WriteLine(result);
                break;
            }
            case "deprecate":
            case "undeprecate":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg deprecate <name@ver> [message] | gmb pkg undeprecate <name@ver>");
                var specD = pos[1];
                var atD = specD.IndexOf('@');
                var dname = atD > 0 ? specD[..atD] : specD;
                var dver = atD > 0 ? specD[(atD + 1)..] : "";
                if (dver.Length == 0) throw new ArgumentException("Serve <name@version> (es. mongo@2.5.5)");
                var dmsg = pos.Count > 2 ? string.Join(' ', pos.Skip(2)) : null;
                pm.DeprecateVersionAsync(dname, dver, dmsg, sub == "deprecate",
                    flags.GetValueOrDefault("registry"), flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
                Console.WriteLine($"{(sub == "deprecate" ? "deprecato" : "riabilitato")}: {dname}@{dver}" + (dmsg != null ? $" — {dmsg}" : ""));
                break;
            }
            case "sync":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg sync <package> [--tag vX] [--registry url]");
                var tag = flags.GetValueOrDefault("tag");
                if (string.IsNullOrEmpty(tag)) throw new ArgumentException("--tag obbligatorio (es. v1.1.0)");
                var result = pm.SyncRepoAsync(pos[1], tag, flags.GetValueOrDefault("registry"), flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
                Console.WriteLine(result);
                break;
            }
            case "outdated":
            {
                // resolve con ciò che è in cache, poi confronta col registry
                var reqs2 = new List<RequireStmt>();
                foreach (var pkgDir in CachePackageDirs(pm.CacheRoot))
                    reqs2.Add(Req(Path.GetFileName(pkgDir)!, "*"));
                pm.ResolveDependenciesAsync(reqs2, Ctx(flags)).GetAwaiter().GetResult();
                var outdated = pm.GetOutdatedAsync().GetAwaiter().GetResult();
                if (outdated.Count == 0) { Console.WriteLine("Tutto aggiornato."); break; }
                foreach (var (n, pair) in outdated.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
                    Console.WriteLine($"{n}: {pair.Installed} -> {pair.Latest}");
                break;
            }
            case "why":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg why <name> [--lock gmb-package.lock]");
                var target = pos[1];
                var whyLock = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var whyReqs = new List<RequireStmt>();
                if (File.Exists(whyLock))
                    foreach (var (n, v) in Parser.PackageManager.ReadLockFile(whyLock)) whyReqs.Add(Req(n, v));
                else
                    foreach (var pkgDir in CachePackageDirs(pm.CacheRoot)) whyReqs.Add(Req(Path.GetFileName(pkgDir)!, "*"));
                pm.ResolveDependenciesAsync(whyReqs, Ctx(flags)).GetAwaiter().GetResult();
                var tree = pm.GetDependencyTree();
                var ver2 = pm.ResolvedArtifacts.TryGetValue(target, out var wa) ? wa.Version : "(non risolto)";
                Console.WriteLine($"{target}@{ver2}");
                var parents = tree.Where(kv => kv.Value.Contains(target, StringComparer.OrdinalIgnoreCase))
                                  .Select(kv => kv.Key).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
                if (parents.Count == 0) Console.WriteLine("  (nessun genitore nel grafo: dipendenza diretta o assente)");
                foreach (var p in parents)
                {
                    pm.ResolvedArtifacts.TryGetValue(p, out var pa);
                    Console.WriteLine($"  <- {p} {pa?.Version}");
                }
                break;
            }
            case "prune":
            {
                var pruneLock = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var kept = Parser.PackageManager.ReadLockFile(pruneLock);
                if (kept.Count == 0) throw new ArgumentException($"gmb pkg prune: lock vuoto/assente ({pruneLock})");
                var removed = 0;
                foreach (var pkgDir in CachePackageDirs(pm.CacheRoot))
                {
                    var name = Path.GetFileName(pkgDir)!;
                    if (!kept.TryGetValue(name, out var keepVer)) continue;
                    foreach (var vDir in Directory.GetDirectories(pkgDir))
                        if (Path.GetFileName(vDir) != keepVer) { Directory.Delete(vDir, true); removed++; }
                }
                Console.WriteLine($"prune: {removed} versioni non nel lock rimosse (extraneous non toccate).");
                break;
            }
            case "dedupe":
            {
                var dedupeLock = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var want = Parser.PackageManager.ReadLockFile(dedupeLock);
                var removed2 = 0;
                foreach (var pkgDir in CachePackageDirs(pm.CacheRoot))
                {
                    var name = Path.GetFileName(pkgDir)!;
                    var versions = Directory.GetDirectories(pkgDir).Select(Path.GetFileName).Where(v => v != null).ToList();
                    if (versions.Count <= 1) continue;
                    var keep = want.TryGetValue(name, out var wv) ? wv
                        : versions.OrderByDescending(x => Parser.PackageManager.ParseSemVer(x!) ?? new Parser.SemVer(0, 0, 0)).First();
                    foreach (var v in versions) if (v != keep) { Directory.Delete(Path.Combine(pkgDir, v!), true); removed2++; }
                }
                Console.WriteLine($"dedupe: {removed2} versioni duplicate rimosse.");
                break;
            }
            case "audit":
            {
                var fixMode = pos.Count >= 2 && pos[1].Equals("fix", StringComparison.OrdinalIgnoreCase);
                var auditLock = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var auditReqs = new List<RequireStmt>();
                if (File.Exists(auditLock))
                    foreach (var (n, v) in Parser.PackageManager.ReadLockFile(auditLock)) auditReqs.Add(Req(n, v));
                else
                    foreach (var pkgDir in CachePackageDirs(pm.CacheRoot)) auditReqs.Add(Req(Path.GetFileName(pkgDir)!, "*"));
                if (auditReqs.Count > 0)
                    pm.ResolveDependenciesAsync(auditReqs, Ctx(flags)).GetAwaiter().GetResult();
                // advisory (JSON grezzo da file o registry) + audit DELEGATI al C (gmb_pkg_audit).
                var advJson = flags.TryGetValue("advisories", out var af)
                    ? (File.Exists(af) ? File.ReadAllText(af) : "[]")
                    : (AdvisoryDb.LoadRegistryJson(pm.RegistryUrl) ?? "[]");
                var names = pm.ResolvedPackages.Keys.OrderBy(k => k, StringComparer.OrdinalIgnoreCase).ToArray();
                var versions = names.Select(n => pm.ResolvedPackages[n].Manifest.Version).ToArray();
                var hits = NativePkg.Audit(names, versions, advJson);

                if (!fixMode)
                {
                    if (hits.Count == 0) { Console.WriteLine($"audit: nessuna vulnerabilita' nota in {names.Length} pacchetti."); break; }
                    foreach (var h in hits.OrderByDescending(x => x.Severity))
                        Console.WriteLine($"  [{h.Severity}] {h.Name}@{h.Version} — {h.Id}: {h.Summary}" + (h.Fixed.Length > 0 ? $" (fix: {h.Fixed})" : ""));
                    Environment.ExitCode = 1;
                    break;
                }

                // Modalità FIX: forza le versioni correttive note (override) e ri-risolve; riscrive il lock.
                var fixable = hits.Where(h => h.Fixed.Length > 0)
                    .GroupBy(h => h.Name, StringComparer.OrdinalIgnoreCase).Select(g => g.First()).ToList();
                if (fixable.Count == 0) { Console.WriteLine("audit fix: nessun advisory ha una versione correttiva nota."); Environment.ExitCode = 1; break; }
                var ctxFix = Ctx(flags);
                var ovr = new Dictionary<string, string>(ctxFix.Overrides, StringComparer.OrdinalIgnoreCase);
                foreach (var h in fixable) ovr[h.Name] = h.Fixed;
                ctxFix.Overrides = ovr;
                pm.ResolveDependenciesAsync(auditReqs, ctxFix).GetAwaiter().GetResult();
                var n2 = pm.ResolvedPackages.Keys.OrderBy(k => k, StringComparer.OrdinalIgnoreCase).ToArray();
                var v2 = n2.Select(n => pm.ResolvedPackages[n].Manifest.Version).ToArray();
                var hits2 = NativePkg.Audit(n2, v2, advJson);
                foreach (var h in fixable) Console.WriteLine($"  fix: {h.Name} {h.Version} -> {h.Fixed}");
                var fixDir = Path.GetFullPath(flags.GetValueOrDefault("dir") ?? Directory.GetCurrentDirectory());
                if (File.Exists(Path.Combine(fixDir, "gmb-package.lock"))) pm.WriteLockFile(fixDir);
                Console.WriteLine($"audit fix: {fixable.Count} aggiornati, {hits2.Count} vulnerabilita' residue.");
                if (hits2.Count > 0) Environment.ExitCode = 1;
                break;
            }
            case "sbom":
            {
                var sbomLock = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var sbomReqs = new List<RequireStmt>();
                if (File.Exists(sbomLock))
                    foreach (var (n, v) in Parser.PackageManager.ReadLockFile(sbomLock)) sbomReqs.Add(Req(n, v));
                else
                    foreach (var pkgDir in CachePackageDirs(pm.CacheRoot)) sbomReqs.Add(Req(Path.GetFileName(pkgDir)!, "*"));
                if (sbomReqs.Count > 0)
                    pm.ResolveDependenciesAsync(sbomReqs, Ctx(flags)).GetAwaiter().GetResult();
                // SBOM CycloneDX DELEGATO al C (gmb_pkg_sbom).
                var snames = pm.ResolvedPackages.Keys.OrderBy(k => k, StringComparer.OrdinalIgnoreCase).ToArray();
                var svers = snames.Select(n => pm.ResolvedPackages[n].Manifest.Version).ToArray();
                var sdirs = snames.Select(n => pm.ResolvedPackages[n].InstallPath).ToArray();
                var json = NativePkg.Sbom(snames, svers, sdirs);
                if (flags.TryGetValue("out", out var outPath)) { File.WriteAllText(outPath, json); Console.WriteLine($"SBOM: {outPath} ({snames.Length} componenti)"); }
                else Console.WriteLine(json);
                break;
            }
            case "update":
            {
                if (pos.Count < 2)
                {
                    // nessun nome -> aggiorna TUTTI i pacchetti del progetto (come `gmb update`)
                    var dir = flags.GetValueOrDefault("dir") ?? Directory.GetCurrentDirectory();
                    DoUpdate(pm, Ctx(flags), Path.GetFullPath(dir), flags.ContainsKey("check"), flags.ContainsKey("yes"));
                    break;
                }
                pm.UpdatePackageAsync(pos[1]).GetAwaiter().GetResult();
                var ver = pm.ResolvedArtifacts.TryGetValue(pos[1], out var art) ? art.Version : "?";
                Console.WriteLine($"Updated: {pos[1]} -> {ver}");
                break;
            }
            case "tag":
            {
                if (pos.Count < 4) throw new ArgumentException("Uso: gmb pkg tag <name> <tag> <version> [--registry url] [--token t]");
                pm.SetTagAsync(pos[1], pos[2], pos[3], flags.GetValueOrDefault("registry"), flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
                Console.WriteLine($"Tag: {pos[1]} {pos[2]} -> {pos[3]}");
                break;
            }
            case "tags":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg tags <name> [--registry url]");
                var reg = (flags.GetValueOrDefault("registry") ?? pm.RegistryUrl).TrimEnd('/');
                var json = NativePkg.RegistryGet($"{reg}/api/packages/{pos[1]}/tags") ?? "";
                Console.WriteLine(json);
                break;
            }
            case "vendor":
            {
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb pkg vendor <dir> [--cache dir] [--lock <file>]");
                var lockPath = flags.GetValueOrDefault("lock", "./gmb-package.lock");
                var reqsV = new List<RequireStmt>();
                var lockedV = Parser.PackageManager.ReadLockFile(lockPath);
                foreach (var (n, v) in lockedV) reqsV.Add(Req(n, v));
                // Senza lock si usano le dipendenze del MANIFESTO del progetto: prima `vendor` su
                // un progetto con `gmb-package.json` (e senza lock) non faceva NULLA e stampava
                // comunque "Vendored" — successo silenzioso con zero pacchetti.
                if (reqsV.Count == 0)
                {
                    var projMan = Parser.PackageManager.ReadProjectManifestAsync(pos[1]).GetAwaiter().GetResult();
                    if (projMan != null)
                        foreach (var d in projMan.Dependencies) reqsV.Add(Req(d.Key, d.Value));
                }
                if (reqsV.Count == 0)
                    throw new ArgumentException(
                        $"vendor: nessuna dipendenza da vendere (lock '{lockPath}' vuoto/assente e '{pos[1]}/gmb-package.json' senza dependencies)");
                pm.ResolveDependenciesAsync(reqsV, Ctx(flags), lockPath).GetAwaiter().GetResult();
                if (pm.ResolvedPackages.Count == 0)
                    throw new ArgumentException("vendor: la risoluzione non ha prodotto pacchetti (vedi i vincoli/le dipendenze)");
                // Layout documentato e usato dal compilatore: <progetto>/vendor/<nome>/<versione>/
                // (prima scriveva in <dir>/<nome>/<versione> e il messaggio citava un flag --vendor
                // che il driver non ha: il vendoring non veniva mai trovato dal REQUIRE).
                var vendorRoot = Path.Combine(pos[1], "vendor");
                pm.Vendor(vendorRoot);
                Console.WriteLine($"Vendored {pm.ResolvedPackages.Count} pacchetti in {vendorRoot}");
                break;
            }
            default:
                throw new ArgumentException("Uso: gmb pkg install|list|remove|info|search|tree|outdated|update|tag|tags|vendor");
        }
    }

    private static RequireStmt Req(string name, string constraint) => new(
        new StringExpr(name, new Token(TokenKind.StringLiteral, name, name, SourcePosition.Unknown)),
        constraint == "*" ? null : new StringExpr(constraint, new Token(TokenKind.StringLiteral, constraint, constraint, SourcePosition.Unknown)),
        new Token(TokenKind.Require, "REQUIRE", null, SourcePosition.Unknown));

    private static void Admin(string[] args)
    {
        var flags = Flags(args, out var pos);
        var sub = pos.Count > 0 ? pos[0].ToLowerInvariant() : "";
        var pm = NewPm(flags);
        var tok = flags.GetValueOrDefault("token") ?? GmbConfig.Load().Token;
        var reg = flags.GetValueOrDefault("registry");
        switch (sub)
        {
            case "reserve":
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb admin reserve <name> [--registry url] [--token t]");
                Console.WriteLine(pm.AdminReserveAsync(pos[1], reg, tok).GetAwaiter().GetResult());
                break;
            case "assign":
                if (pos.Count < 3) throw new ArgumentException("Uso: gmb admin assign <name> <owner> [--registry url] [--token t]");
                Console.WriteLine(pm.AdminAssignAsync(pos[1], pos[2], reg, tok).GetAwaiter().GetResult());
                break;
            case "revoke":
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb admin revoke <name> [--keep-reserved] [--registry url] [--token t]");
                Console.WriteLine(pm.AdminRevokeAsync(pos[1], flags.ContainsKey("keep-reserved"), reg, tok).GetAwaiter().GetResult());
                break;
            case "password":
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb admin password <newToken> [--registry url] [--token t]");
                Console.WriteLine(pm.AdminPasswordAsync(pos[1], reg, tok).GetAwaiter().GetResult());
                break;
            case "user":
                if (pos.Count < 2) throw new ArgumentException("Uso: gmb admin user <create|setkey|status|list> ...");
                switch (pos[1].ToLowerInvariant())
                {
                    case "create":
                        if (pos.Count < 3) throw new ArgumentException("Uso: gmb admin user create <name> [--registry url] [--token t]");
                        Console.WriteLine(pm.AdminUserCreateAsync(pos[2], reg, tok).GetAwaiter().GetResult());
                        break;
                    case "setkey":
                        if (pos.Count < 4) throw new ArgumentException("Uso: gmb admin user setkey <name> <publicKey> [--registry url] [--token t]");
                        Console.WriteLine(pm.AdminUserSetKeyAsync(pos[2], pos[3], reg, tok).GetAwaiter().GetResult());
                        break;
                    case "status":
                        if (pos.Count < 4) throw new ArgumentException("Uso: gmb admin user status <name> <active|blocked> [--registry url] [--token t]");
                        Console.WriteLine(pm.AdminUserStatusAsync(pos[2], pos[3], reg, tok).GetAwaiter().GetResult());
                        break;
                    case "list":
                        Console.WriteLine(pm.AdminUserListAsync(reg, tok).GetAwaiter().GetResult());
                        break;
                    default:
                        throw new ArgumentException("Uso: gmb admin user <create|setkey|status|list> ...");
                }
                break;
            default:
                throw new ArgumentException("Uso: gmb admin reserve|assign|revoke|password|user");
        }
    }

    private static void Publish(string[] args)
    {
        var flags = Flags(args, out var pos);
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb publish <file.gmbpkg> [--registry url] [--token t]");
        if (flags.ContainsKey("repo"))
        {
            PublishRepo(flags, pos);
            return;
        }
        var pm = NewPm(flags);
        pm.PublishPackageAsync(pos[0], flags.GetValueOrDefault("registry"), flags.GetValueOrDefault("token")).GetAwaiter().GetResult();
        Console.WriteLine($"Published: {pos[0]}");
    }

    /// <summary>
    /// Fase B — gmb publish --repo <git-url|path>: verifica locale del repo contributor
    /// (struttura → git → compilazione → self-test) e registra il pacchetto sul registry proxy.
    /// Genera un report di verifica JSON inviato a POST /api/packages/register.
    /// </summary>
    private static void PublishRepo(Dictionary<string, string> flags, List<string> pos)
    {
        var repoSpec = flags["repo"];
        if (pos.Count < 1) throw new ArgumentException("Uso: gmb publish --repo <git-url|path> <name@tag> [--registry url]");
        var nameTag = pos[0];
        // nome può essere "@scope/name" (con @ interno) — il separatore name@tag è l'ULTIMO '@'
        var at = nameTag.LastIndexOf('@');
        var name = at > 0 ? nameTag[..at] : nameTag;
        var tag = at > 0 ? nameTag[(at + 1)..] : "HEAD";

        // clona (o usa path locale) in una temp dir di lavoro
        var work = Path.Combine(Path.GetTempPath(), "gmb_repo_" + Guid.NewGuid().ToString("N")[..8]);
        Directory.CreateDirectory(work);
        try
        {
            if (Directory.Exists(repoSpec))
            {
                // repo locale: copia shallow via git fetch del tag/HEAD
                RunGit(work, "init");
                RunGit(work, "remote", "add", "origin", repoSpec);
                RunGit(work, "fetch", "--depth", "1", "origin", tag == "HEAD" ? "HEAD" : $"refs/tags/{tag}");
                RunGit(work, "checkout", "-q", "FETCH_HEAD");
            }
            else
            {
                // repo remoto (URL): clone shallow (solo default branch), poi checkout del tag
                RunGit(work, "init");
                RunGit(work, "remote", "add", "origin", repoSpec);
                RunGit(work, "fetch", "--depth", "1", "origin", tag == "HEAD" ? "HEAD" : $"refs/tags/{tag}");
                RunGit(work, "checkout", "-q", "FETCH_HEAD");
            }

            // B1: struttura — manifest in root, valido
            var manifestPath = Path.Combine(work, "gmb-package.json");
            if (!File.Exists(manifestPath)) throw new ArgumentException("gmb-package.json mancante in root del repo");
            var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(manifestPath));
            var manifest = Parser.PackageManager.ParseManifest(doc.RootElement);
            PackageBuilder.ValidateManifest(manifest);
            Console.WriteLine($"  [1] manifest OK: {manifest.Name}@{manifest.Version}");

            // B2: git — il manifest è versionato
            var tracked = RunGitOut(work, "ls-files", "--error-unmatch", "gmb-package.json");
            if (tracked.ExitCode != 0) throw new ArgumentException("gmb-package.json non è versionato nel repo");
            Console.WriteLine($"  [2] git OK (manifest versionato, tag: {tag})");

            // B3: compilazione dell'entry di test. Un pacchetto di LIBRERIA ha exports
            // dichiarativi (.inc con DECLARE/FUNCTION) e NESSUN entry point: usare
            // exports[0] come programma faceva fallire la verifica con
            // "No valid entry point found" (pacchetti legittimi rifiutati). Ordine:
            //   1) manifest.test.entry (dichiarata dal pacchetto)
            //   2) un sample .bas eseguibile (SUB Main o riga numerata)
            //   3) nessuna → verifica di compilazione SALTATA (struttura+git restano verificati)
            var entry = manifest.Test?.Entry;
            if (string.IsNullOrEmpty(entry))
            {
                foreach (var cand in Directory.GetFiles(work, "*.bas", SearchOption.AllDirectories))
                {
                    string text;
                    try { text = File.ReadAllText(cand); } catch { continue; }
                    if (!System.Text.RegularExpressions.Regex.IsMatch(text,
                            @"^\s*(SUB\s+Main\b|\d+\s)", System.Text.RegularExpressions.RegexOptions.IgnoreCase |
                                                          System.Text.RegularExpressions.RegexOptions.Multiline)) continue;
                    entry = Path.GetRelativePath(work, cand);
                    break;
                }
            }
            var entryAbs = string.IsNullOrEmpty(entry) ? null : Path.Combine(work, entry!);
            string testOutput = "";
            int testExitCode = 0;
            if (entryAbs != null && !File.Exists(entryAbs))
                throw new ArgumentException($"entry non trovata: {entry}");
            if (entryAbs == null)
            {
                Console.WriteLine("  [3] compilazione SALTATA: nessuna entry eseguibile (pacchetto di sole dichiarazioni)");
                Console.WriteLine("  [4] self-test SALTATO (niente da eseguire)");
            }
            else
            {
                var program = global::Basic.Orchestrator.Compile(entryAbs, null, false, "vm");
                if (program?.MainModule == null) throw new ArgumentException("Compilazione fallita");
                var chunk = new global::Compiler.Compiler().Compile(program);
                Console.WriteLine($"  [3] compilazione OK ({entry})");

                // B4: self-test — esegue il chunk, termina senza errori
                var res = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
                if (!res.Success) throw new ArgumentException("Self-test fallito (exit non zero)");
                testOutput = res.Output ?? "";
                testExitCode = res.ExitCode;
                Console.WriteLine($"  [4] self-test OK (exit 0, {testOutput.Length} byte output)");
            }

            // B5: report di verifica (manifest completo incluso per la materializzazione C3bis)
            var manifestJson = File.ReadAllText(manifestPath);
            var report = new
            {
                name,
                repo = repoSpec,
                tag,
                manifest = new { name = manifest.Name, version = manifest.Version },
                manifestJson,
                backend = "il",
                testOutput = testOutput.Length > 2000 ? testOutput[..2000] : testOutput,
                testExitCode = testExitCode,
                commit = RunGitOut(work, "rev-parse", "HEAD").Output.Trim(),
                verifiedAt = DateTimeOffset.UtcNow.ToString("o"),
            };

            // registra sul registry (proxy): POST /api/packages/register
            var registry = flags.GetValueOrDefault("registry") ?? GmbConfig.Load().RegistryUrl;
            var payload = new { repo = repoSpec, name, tag, verify_report = report };
            var token = flags.GetValueOrDefault("token") ?? GmbConfig.Load().Token;
            // HTTP DELEGATO al C (gmb_pkg_registry_post).
            var okReg = NativePkg.RegistryPost($"{registry.TrimEnd('/')}/api/packages/register",
                System.Text.Json.JsonSerializer.Serialize(payload), token, out var regStatus, out var body);
            if (!okReg) throw new ArgumentException($"Registrazione fallita ({regStatus}): {body}");
            Console.WriteLine($"Registered: {name}@{tag} (status {regStatus})");

            // C3ter: PUBBLICA l'artefatto verificato. Prima il repo veniva solo INDICIZZATO
            // (repos.json) e `pkg install <nome>` rispondeva "Registry non pubblica versioni
            // utilizzabili" — il pacchetto contributor non era installabile. Ora l'albero
            // verificato (manifest + exports + header + lib + samples) viene impacchettato e
            // pubblicato come .gmbpkg, quindi entra nel normale flusso install/download.
            try
            {
                var gmbpkg = Path.Combine(work, $"{name.Replace('/', '_')}-{manifest.Version}.gmbpkg");
                PackageBuilder.Pack(work, gmbpkg);
                NewPm(flags).PublishPackageAsync(gmbpkg, registry, token).GetAwaiter().GetResult();
                Console.WriteLine($"Published: {name}@{manifest.Version} (installabile)");
            }
            catch (Exception px)
            {
                // 409 = versione gia' pubblicata: non e' un errore di questa verifica.
                Console.WriteLine($"Publish dell'artefatto SALTATO: {px.Message}");
            }

            // C3bis: materializzazione binari — il client scarica gli asset URL del manifest
            // e li carica al registry (base64), così il server li serve da cache locale.
            try
            {
                var assetMap = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                var mdoc = System.Text.Json.JsonDocument.Parse(manifestJson);
                if (mdoc.RootElement.TryGetProperty("binaries", out var bins) && bins.ValueKind == System.Text.Json.JsonValueKind.Object)
                {
                    foreach (var bkv in bins.EnumerateObject())
                    {
                        // backend -> { platform -> url } | platform -> url
                        if (bkv.Value.ValueKind == System.Text.Json.JsonValueKind.Object)
                        {
                            foreach (var pkv in bkv.Value.EnumerateObject())
                            {
                                var url = pkv.Value.ValueKind == System.Text.Json.JsonValueKind.String ? pkv.Value.GetString() : null;
                                if (string.IsNullOrEmpty(url)) continue;
                                if (!Uri.TryCreate(url, UriKind.Absolute, out _)) continue;
                                var bytes = HttpDownloadBytes(url);
                                if (bytes == null) continue;
                                assetMap[$"{bkv.Name}/{pkv.Name}"] = Convert.ToBase64String(bytes);
                                Console.WriteLine($"  [materialize] {bkv.Name}/{pkv.Name} <- {url} ({bytes.Length} byte)");
                            }
                        }
                        else if (bkv.Value.ValueKind == System.Text.Json.JsonValueKind.String)
                        {
                            var url = bkv.Value.GetString();
                            if (!string.IsNullOrEmpty(url) && Uri.TryCreate(url, UriKind.Absolute, out _))
                            {
                                var bytes = HttpDownloadBytes(url);
                                if (bytes != null)
                                {
                                    assetMap[$"any/{bkv.Name}"] = Convert.ToBase64String(bytes);
                                    Console.WriteLine($"  [materialize] {bkv.Name} <- {url} ({bytes.Length} byte)");
                                }
                            }
                        }
                    }
                }
                if (assetMap.Count > 0)
                {
                    var matPayload = new { name, version = manifest.Version, assets = assetMap };
                    // HTTP DELEGATO al C (gmb_pkg_registry_post).
                    NativePkg.RegistryPost($"{registry.TrimEnd('/')}/api/packages/{Uri.EscapeDataString(name)}/materialize",
                        System.Text.Json.JsonSerializer.Serialize(matPayload), token, out var matStatus, out var matBody);
                    Console.WriteLine($"Materialize: {matStatus} ({assetMap.Count} asset) {matBody}");
                }
                else
                {
                    Console.WriteLine("Materialize: nessun asset binario (source-only)");
                }
            }
            catch (Exception mx)
            {
                Console.WriteLine($"Materialize skipped: {mx.Message}");
            }
        }
        finally
        {
            try { Directory.Delete(work, true); } catch { }
        }
    }

    private static (int ExitCode, string Output) RunGitOut(string workDir, params string[] args)
    {
        var psi = new System.Diagnostics.ProcessStartInfo("git") { WorkingDirectory = workDir, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = System.Diagnostics.Process.Start(psi)!;
        var outT = p.StandardOutput.ReadToEnd();
        var errT = p.StandardError.ReadToEnd();
        p.WaitForExit();
        return (p.ExitCode, (outT + errT).Trim());
    }

    private static void RunGit(string workDir, params string[] args)
    {
        var r = RunGitOut(workDir, args);
        if (r.ExitCode != 0) throw new ArgumentException($"git {args[0]} fallito: {r.Output}");
    }
}
