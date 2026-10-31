// FILE: Interpreter.Parser.PackageManager.cs
// PackageManager v2 — intelligente, multi-backend + FFI (GMBasic).
//
// Gestisce tutti i casi della tassonomia gmb-package.json v2:
//   contentType: source | dotnet | c-ffi | mixed | docs-only
//   backends  : ["il","c99","vm"]  (per cosa è già precompilato; [] = source-only)
//   edition   : Community | Commercial | any
//   binaries  : backend -> platform -> path  (v1: platform -> path, normalizzato)
//   ffi       : { api, headers, libs, link, declareExample }  (c-abi | gmb-native)
//   settings  : { minGmbVersion }
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
public enum PackageContentType
{
    Source,     // solo .bas in exports (compila con backend corrente)
    DotNet,     // precompilato .dll IL (backend il) — assembly .NET generato da IlProgramCompiler
    VmChunk,    // precompilato chunk A1/GMBC (.prg/.chunk, backend vm) — eseguito da gmb_vm_host
    CFFI,       // libreria C/FFI (c-abi o gmb-native), anche senza .bas
    CFFISource, // libreria C/FFI di cui il CLIENT compila il sorgente all'install (blocco `build`)
    Mixed,      // exports + binaries + ffi insieme
    DocsOnly,   // solo README/examples
    Brain       // CERVELLO: dati (rag/graphs) installati da `AI BRAIN INSTALL`, non codice
}

/// <summary>Contesto di build corrente (backend + piattaforma).</summary>
public sealed class PackageBuildContext
{
    public string Backend { get; init; } = "il";       // il | c99 | vm
    public string PlatformKey { get; init; } = "";      // os-arch (vuoto → autodetect)
    public string? RegistryUrl { get; init; }           // registry per la negoziazione
    public IReadOnlyCollection<string> Features { get; set; } = Array.Empty<string>(); // feature attivate (dal manifest di progetto)
    /// <summary>Directory vendor (offline): se un pacchetto/versione è presente qui, viene usata senza registry/cache.</summary>
    public string? VendorDir { get; init; }
    /// <summary>Modalità offline: usa SOLO cache/vendor, mai la rete (--offline).</summary>
    public bool Offline { get; init; }
    /// <summary>Overrides del progetto (nome → vincolo): FORZANO la versione, transitivi inclusi.
    /// Chiavi "parent>child" = override annidato (solo sotto quel parent).</summary>
    public IReadOnlyDictionary<string, string> Overrides { get; set; } = new Dictionary<string, string>();

    /// <summary>Registry per-scope/nome (es. "@org" → url): sovrascrivono il registry di default.</summary>
    public IReadOnlyDictionary<string, string> Registries { get; set; } = new Dictionary<string, string>();
    /// <summary>Pacchetti LOCALI del workspace (nome → directory): risolti dalla loro dir, senza registry/cache.</summary>
    public IReadOnlyDictionary<string, string> LocalPackages { get; set; } = new Dictionary<string, string>();
}

/// <summary>Sezione FFI di un pacchetto (c-abi semplice o gmb-native).</summary>
public sealed record FfiSection
{
    [JsonPropertyName("api")] public string? Api { get; init; }             // "c-abi" | "gmb-native"
    [JsonPropertyName("headers")] public ImmutableArray<string> Headers { get; init; } = [];
    [JsonPropertyName("libs")] public ImmutableDictionary<string, string> Libs { get; init; } = ImmutableDictionary<string, string>.Empty;
    [JsonPropertyName("link")] public string? Link { get; init; }           // es. "-lvec"
    [JsonPropertyName("declareExample")] public string? DeclareExample { get; init; }
}

/// <summary>Sezione test del manifest (modello contributor, Fase B): file da eseguire + backend.</summary>
public sealed class PackageTestSection
{
    public string? Entry { get; set; }                                  // test/self_test.bas
    public ImmutableArray<string> Backends { get; set; } = [];          // ["il"] (default il)
}

/// <summary>Impostazioni/requisiti di un pacchetto.</summary>
public sealed record PackageSettings
{
    [JsonPropertyName("minGmbVersion")] public string? MinGmbVersion { get; init; }
}

/// <summary>Entry dei binari: v2 = backend→(platform→path); v1 = platform→path (normalizzato).</summary>
public sealed class BinaryEntry
{
    public string? Path { get; set; }                          // v1: path per la platform di riferimento
    public Dictionary<string, string>? Platforms { get; set; } // v2: platform -> path
}

/// <summary>
/// Sorgente esterna di un pacchetto: da dove il PM recupera gli artefatti pesanti (di norma i
/// binari precompilati per piattaforma) invece di ospitarli nel registry. Fase 1:
/// type="github-release" — alla risoluzione scarica l'asset della release GitHub per la
/// piattaforma corrente e lo estrae nella cartella indicata (default lib/&lt;platform&gt;).
/// Versionato: il tag è un template ("v{version}" con la versione del pacchetto).
/// </summary>
public sealed class PackageSource
{
    [JsonPropertyName("type")] public string Type { get; set; } = "";    // "github-release"
    [JsonPropertyName("repo")] public string Repo { get; set; } = "";    // "owner/repo"
    [JsonPropertyName("tag")] public string? Tag { get; set; }           // template, default "v{version}"
    /// <summary>platformKey → nome dell'asset nella release (es. "osx-arm64" → "libduckdb-osx-universal.zip"). "any" = fallback.</summary>
    [JsonPropertyName("assets")] public Dictionary<string, string> Assets { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    /// <summary>Cartella relativa (dentro l'install) dove estrarre l'asset. Template "{platform}".</summary>
    [JsonPropertyName("dest")] public string Dest { get; set; } = "lib/{platform}";
    /// <summary>Nomi di file nell'archivio da copiare anche in include/ (header).</summary>
    [JsonPropertyName("headers")] public ImmutableArray<string> Headers { get; set; } = [];
    /// <summary>platformKey → sha256 esadecimale atteso dell'asset (opzionale; se assente usa il digest GitHub).</summary>
    [JsonPropertyName("sha256")] public Dictionary<string, string> Sha256 { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Risolve il tag della release dal template ("{version}") e dalla versione del pacchetto.</summary>
    public string ResolveTag(string version) => (Tag ?? "v{version}").Replace("{version}", version);

    /// <summary>Asset per la piattaforma, con fallback su "any".</summary>
    public string? PickAsset(string platformKey)
    {
        if (Assets.TryGetValue(platformKey, out var a) && !string.IsNullOrEmpty(a)) return a;
        if (Assets.TryGetValue("any", out var any) && !string.IsNullOrEmpty(any)) return any;
        return null;
    }
}

/// <summary>Spec di BUILD del pacchetto (publisher-side: compila il C per la piattaforma corrente).
/// Usata da `gmb pkg build` / `gmb pack --build` per produrre gli artefatti ffi prima di packare/pubblicare.</summary>
public sealed class PackageBuildSpec
{
    [JsonPropertyName("system")] public string? System { get; set; }    // "make" | "cmake" | "command"
    [JsonPropertyName("target")] public string? Target { get; set; }    // target make/cmake
    [JsonPropertyName("command")] public string? Command { get; set; }  // comando shell (alternativo a system+target)
    [JsonPropertyName("cwd")] public string? Cwd { get; set; }          // cartella (relativa alla dir del manifest) dove eseguire
    [JsonPropertyName("tool")] public string? Tool { get; set; }        // tool richiesto (cc/gcc/clang/cmake) — solo informativo
    /// <summary>File che il build deve produrre (relativi alla dir del manifest). "{platform}" → platformKey.</summary>
    [JsonPropertyName("outputs")] public ImmutableArray<string> Outputs { get; set; } = [];
}

/// <summary>
/// Rappresenta il manifest di un pacchetto (gmb-package.json) — v2 con firma di contenuto.
/// </summary>
public sealed class PackageManifest
{
    public string Name { get; set; } = string.Empty;
    public string Version { get; set; } = string.Empty;
    public string Description { get; set; } = string.Empty;
    public PackageContentType ContentType { get; set; } = PackageContentType.Source;
    public ImmutableArray<string> Backends { get; set; } = [];
    public ImmutableArray<string> Exports { get; set; } = [];
    /// <summary>Cartelle di esempi inclusi nel pacchetto (default "samples" se presente nella dir del pacchetto).
    /// All'install vengono copiati in &lt;root&gt;/samples/packages/&lt;nome&gt;@&lt;versione&gt;/.</summary>
    public ImmutableArray<string> Samples { get; set; } = [];
    /// <summary>Glob dei workspace (monorepo): es. ["packages/*"]. Solo nel manifest di progetto.</summary>
    public ImmutableArray<string> Workspaces { get; set; } = [];
    /// <summary>Allowlist dei file/cartelle da includere nel .gmbpkg (glob; vuoto = tutti gli artefatti dichiarati).</summary>
    public ImmutableArray<string> Files { get; set; } = [];
    /// <summary>Pattern (glob) da ESCLUDERE dal .gmbpkg.</summary>
    public ImmutableArray<string> Ignore { get; set; } = [];
    public Dictionary<string, BinaryEntry> Binaries { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    public FfiSection? Ffi { get; set; }
    /// <summary>Sorgente esterna (Fase 1: GitHub release) da cui materializzare i binari pesanti.</summary>
    public PackageSource? Source { get; set; }
    /// <summary>Spec di build C (publisher-side): produce gli artefatti ffi per la piattaforma corrente.</summary>
    public PackageBuildSpec? Build { get; set; }
    public PackageTestSection? Test { get; set; }
    public PackageSettings? Settings { get; set; }
    public ImmutableDictionary<string, string> Dependencies { get; set; } = ImmutableDictionary<string, string>.Empty;
    /// <summary>Dipendenze di SVILUPPO: installate solo per il pacchetto root del progetto (escluse con --production).</summary>
    public ImmutableDictionary<string, string> DevDependencies { get; set; } = ImmutableDictionary<string, string>.Empty;
    /// <summary>Dipendenze OPZIONALI: se non risolvibili l'install prosegue (solo avviso).</summary>
    public ImmutableDictionary<string, string> OptionalDependencies { get; set; } = ImmutableDictionary<string, string>.Empty;
    /// <summary>PEER dependencies: dichiarate ma NON installate automaticamente (devono fornirle il consumatore).</summary>
    public ImmutableDictionary<string, string> PeerDependencies { get; set; } = ImmutableDictionary<string, string>.Empty;
    /// <summary>OVERRIDES: forza la versione (o il range) di un pacchetto, anche transitivo (nome → vincolo).</summary>
    public ImmutableDictionary<string, string> Overrides { get; set; } = ImmutableDictionary<string, string>.Empty;
    /// <summary>Features/dipendenze opzionali (cargo-style): nome → [dipende da ...]; "default" attiva di base.</summary>
    public ImmutableDictionary<string, ImmutableArray<string>> Features { get; set; } = ImmutableDictionary<string, ImmutableArray<string>>.Empty;
    /// <summary>Algoritmo firma pubblica (es. "ecdsa-p256-sha256").</summary>
    public string? SignatureAlgo { get; set; }
    /// <summary>Chiave pubblica PEM per verificare la firma.</summary>
    public string? SignaturePublicKey { get; set; }
    /// <summary>Firma base64 del manifest.</summary>
    public string? SignatureValue { get; set; }
}

/// <summary>Rappresenta un pacchetto installato localmente nella cache.</summary>
public sealed record InstalledPackage(PackageManifest Manifest, string InstallPath);

/// <summary>Configurazione utente (credential store) — ~/.gmbasic/config.json, chmod 600.</summary>
public sealed class GmbConfig
{
    public string RegistryUrl { get; set; } = Basic.Providers.GmbEnv.Get("GMB_REGISTRY") ?? "http://localhost:8087";
    public string? Token { get; set; }
    /// <summary>Account registry (nome utente registrato) — modello contributor, Fase B.</summary>
    public string? Username { get; set; }
    /// <summary>Repo git pubblicati con gmb publish --repo (nome → git-url).</summary>
    public Dictionary<string, string> Repos { get; set; } = new(StringComparer.OrdinalIgnoreCase);
    /// <summary>Registries nominati (official/company/local) → URL, per multi-registry di progetto.</summary>
    public Dictionary<string, string> Registries { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    public static string ConfigPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".gmbasic", "config.json");

    public static GmbConfig Load()
    {
        try
        {
            if (File.Exists(ConfigPath))
                return JsonSerializer.Deserialize<GmbConfig>(File.ReadAllText(ConfigPath)) ?? new GmbConfig();
        }
        catch { /* config corrotta: default */ }
        return new GmbConfig();
    }

    public void Save()
    {
        try
        {
            var dir = Path.GetDirectoryName(ConfigPath);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
            if (!OperatingSystem.IsWindows())
                try { File.SetUnixFileMode(ConfigPath, UnixFileMode.UserRead | UnixFileMode.UserWrite); } catch { }
        }
        catch { /* best-effort */ }
    }
}

/// <summary>Flag di link C99 (include/-L/-l) dalle librerie FFI/binaries c99 dei pacchetti risolti.</summary>
public sealed record C99LinkInfo(string[] Includes, string[] LibDirs, string[] Libs)
{
    public string ToCcFlags()
        => string.Join(" ", Includes.Select(i => $"-I\"{i}\""))
         + (LibDirs.Length > 0 ? " " + string.Join(" ", LibDirs.Select(d => $"-L\"{d}\"")) : "")
         + (Libs.Length > 0 ? " " + string.Join(" ", Libs.Select(l => $"-l{l}")) : "");
}

/// <summary>Artefatto selezionato dal resolver per il contesto di build corrente.</summary>
public sealed class PackageArtifact
{
    public string PackageName { get; init; } = string.Empty;
    public string Version { get; init; } = string.Empty;
    public string InstallPath { get; init; } = string.Empty;
    public PackageContentType ContentType { get; init; }
    public string? SourceDir { get; init; }        // root degli exports .bas (risolto assoluto)
    public string? BinaryPath { get; init; }       // path binario scelto (risolto assoluto): .dll IL o .so C
    public string? ChunkPath { get; init; }        // path chunk A1/GMBC (.prg/.chunk, backend vm)
    public string[] FfiLibs { get; init; } = [];   // path assoluti delle .so/.a/.dll
    public string[] FfiHeaders { get; init; } = [];// path assoluti degli header
    public string? FfiLink { get; init; }          // flag -l
    public string? DeclareExample { get; init; }   // esempio DECLARE (per pacchetti c-abi)
    public string Backend { get; init; } = "any";
    public string Platform { get; init; } = "any";
}


public sealed class PackageException : Exception
{
    public PackageException(string message) : base(message) { }
    public PackageException(string message, Exception inner) : base(message, inner) { }
}
