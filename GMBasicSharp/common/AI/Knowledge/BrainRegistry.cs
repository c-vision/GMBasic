// FILE: Basic.AI.Knowledge.BrainRegistry
// Second Brain — registry dei cervelli (AI BRAIN USE/DETACH/LIST/INFO/INSTALL).
// Persistenza <configDir>/brains.json. La disponibilità di un cervello è verificata su disco:
// un brain è "available come rag" se esiste <brains>/<name>/rag/<name>.db (o, legacy,
// AiConfig.RagDefaultDir/<name>.db); "available come graph" se esiste <brains>/<name>/graphs/
// <name>.json (o GraphsDefaultDir/<name>.json). Il registry NON genera nulla — usa/disconnette
// solo ciò che è già presente (USE) o registra ciò che è stato importato/costruito.
// Firma/checksum dei file in `brain.manifest.json` (§6.1ex del piano Second-Brain).
using System.Text.Json;
using CommonLib;

namespace Basic.AI.Knowledge;

public sealed record BrainInfo
{
    public string Name { get; init; } = "";
    public string? Source { get; init; }
    public string? VaultPath { get; init; }
    public string Mode { get; init; } = "";
    public List<string> Available { get; init; } = [];
    public string? Checksum { get; init; }
    public string? Version { get; init; }
    public long Docs { get; init; }
    public List<string> Parsers { get; init; } = [];
    public bool Attached { get; init; }
    public string? InstalledAt { get; init; }
    public Dictionary<string, string> Probe { get; init; } = [];
    public string? SignedBy { get; init; }
}

public static class BrainRegistry
{
    private static string? _jsonPathOverride;

    /// <summary>Hook TEST-SCOPED: reindirizza il file del registry a un path temporaneo. I test
    /// NON devono MAI toccare <configDir>/brains.json reale.</summary>
    internal static void OverridePathForTests(string path)
    {
        _jsonPathOverride = path;
    }

    public static string JsonPath => _jsonPathOverride ?? GmbiPaths.BrainsJsonFile;

    private sealed record Persisted(
        List<string> Attached,
        List<BrainInfo> Brains);

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    private static Persisted Load()
    {
        try
        {
            if (!File.Exists(JsonPath)) return new Persisted([], []);
            return JsonSerializer.Deserialize<Persisted>(File.ReadAllText(JsonPath), JsonOpts)
                ?? new Persisted([], []);
        }
        catch { return new Persisted([], []); }
    }

    private static void Save(Persisted p)
    {
        var dir = Path.GetDirectoryName(JsonPath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(JsonPath, JsonSerializer.Serialize(p, JsonOpts));
    }

    // ── Disponibilità (BrainAvailability) ──────────────────────────────────

    /// <summary>Root EFFETTIVA del cervello. La risolve il runtime C (`CommonLib.NativeBrains.Lookup`):
    /// <c>&lt;progetto&gt;/vendor/brains</c> → <c>&lt;exe&gt;/vendor/brains</c> → <c>&lt;exe&gt;/brains</c>
    /// (brainsDir) → <c>~/.gmbasic/brains</c> (cache dei download). Così un cervello installato
    /// da `AI BRAIN INSTALL`/package manager è visibile agli agenti anche se non sta in
    /// <c>brains/</c>. Se la dylib non è disponibile si ricade su
    /// <c>AiConfig.BrainsDefaultDir/&lt;name&gt;</c> (comportamento storico).</summary>
    public static string? ResolveRoot(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return null;
        var resolved = CommonLib.NativeBrains.Lookup(name);
        if (!string.IsNullOrEmpty(resolved) && Directory.Exists(resolved)) return resolved;
        var import = Path.Combine(AiConfig.BrainsDefaultDir, name);
        return Directory.Exists(import) ? import : null;
    }

    /// <summary>True se il brain è utilizzabile come RAG: esiste un db vettoriale.
    /// Cerca nella root risolta (vendor/exe/cache), poi il legacy
    /// AiConfig.RagDefaultDir/&lt;name&gt;.db (generato da RAG BUILD).</summary>
    public static bool IsAvailableAsRag(string name)
    {
        if (RagDbPath(name) is not null) return true;
        var legacy = Path.Combine(AiConfig.RagDefaultDir, name + ".db");
        return File.Exists(legacy);
    }

    /// <summary>True se il brain è utilizzabile come graph: esiste l'export JSON del grafo.
    /// Cerca nella root risolta, poi il legacy AiConfig.GraphsDefaultDir/&lt;name&gt;.json.</summary>
    public static bool IsAvailableAsGraph(string name)
    {
        if (GraphJsonPath(name) is not null) return true;
        var legacy = Path.Combine(AiConfig.GraphsDefaultDir, name + ".json");
        return File.Exists(legacy);
    }

    /// <summary>Le forme disponibili del brain: ["rag"]|["graph"]|["both"]|[]. Ordine
    /// stabile rag,graph.</summary>
    public static string[] AvailableModes(string name)
    {
        var rag = IsAvailableAsRag(name);
        var graph = IsAvailableAsGraph(name);
        if (rag && graph) return ["rag", "graph"];
        if (rag) return ["rag"];
        if (graph) return ["graph"];
        return [];
    }

    /// <summary>Il path effettivo del db RAG di un brain (root risolta o legacy), o null.</summary>
    public static string? RagDbPath(string name)
    {
        var root = ResolveRoot(name);
        if (root is not null)
        {
            var import = Path.Combine(root, "rag", name + ".db");
            if (File.Exists(import)) return import;
            // cervello con un solo .db dentro rag/ (nome interno diverso dal nome logico)
            try
            {
                if (Directory.Exists(Path.Combine(root, "rag")))
                {
                    var any = Directory.GetFiles(Path.Combine(root, "rag"), "*.db").FirstOrDefault();
                    if (any is not null) return any;
                }
            }
            catch { /* permessi: si prova il legacy */ }
        }
        var legacy = Path.Combine(AiConfig.RagDefaultDir, name + ".db");
        return File.Exists(legacy) ? legacy : null;
    }

    /// <summary>Il path effettivo dell'export JSON del graph di un brain (root risolta o
    /// legacy), o null.</summary>
    public static string? GraphJsonPath(string name)
    {
        var root = ResolveRoot(name);
        if (root is not null)
        {
            var import = Path.Combine(root, "graphs", name + ".json");
            if (File.Exists(import)) return import;
            try
            {
                if (Directory.Exists(Path.Combine(root, "graphs")))
                {
                    var any = Directory.GetFiles(Path.Combine(root, "graphs"), "*.json").FirstOrDefault();
                    if (any is not null) return any;
                }
            }
            catch { /* permessi: si prova il legacy */ }
        }
        var legacy = Path.Combine(AiConfig.GraphsDefaultDir, name + ".json");
        return File.Exists(legacy) ? legacy : null;
    }

    /// <summary>La root del brain: quella risolta dal runtime C, altrimenti il primo path
    /// legacy che lo contiene (rag o graphs), altrimenti null.</summary>
    public static string? BrainRoot(string name)
    {
        var root = ResolveRoot(name);
        if (root is not null) return root;
        if (RagDbPath(name) is string rag) return Path.GetDirectoryName(Path.GetDirectoryName(rag));
        if (GraphJsonPath(name) is string graph) return Path.GetDirectoryName(Path.GetDirectoryName(graph));
        return null;
    }

    // ── USE / DETACH / LIST / INFO ─────────────────────────────────────────

    /// <summary>Collega un brain (lo aggiunge ad attached[]) SOLO se è disponibile su disco.
    /// Ritorna true se collegato, false con errore esplicito se non disponibile.</summary>
    public static bool Use(string name)
    {
        if (AvailableModes(name).Length == 0) return false;
        var p = Load();
        if (p.Attached.Contains(name, StringComparer.OrdinalIgnoreCase)) return true;
        Save(p with { Attached = [.. p.Attached, name] });
        return true;
    }

    /// <summary>Scollega un brain (lo toglie da attached[]) senza eliminare i file.</summary>
    public static bool Detach(string name)
    {
        var p = Load();
        var filtered = p.Attached.Where(x => !string.Equals(x, name, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count == p.Attached.Count) return false;
        Save(p with { Attached = filtered });
        return true;
    }

    /// <summary>Nomi dei brain collegati (attached).</summary>
    public static string[] Used() => [.. Load().Attached];

    /// <summary>Nomi di tutti i brain: quelli registrati (attached o no) PIÙ quelli trovati dal
    /// resolver C su disco (vendor → exe → brainsDir → cache <c>~/.gmbasic/brains</c>). Il
    /// secondo insieme serve perché un cervello installato da <c>AI BRAIN INSTALL</c> vive sul
    /// filesystem: senza l'unione, IL/PCodeVM non lo elencherebbero mentre C99Native/VM-C sì.</summary>
    public static string[] List()
    {
        var p = Load();
        var names = p.Brains.Select(b => b.Name).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        foreach (var n in p.Attached)
            if (!names.Contains(n, StringComparer.OrdinalIgnoreCase)) names.Add(n);
        foreach (var n in CommonLib.NativeBrains.List())
            if (!string.IsNullOrWhiteSpace(n) && !names.Contains(n, StringComparer.OrdinalIgnoreCase)) names.Add(n);
        return [.. names.OrderBy(x => x, StringComparer.OrdinalIgnoreCase)];
    }

    /// <summary>Info del brain: unisce la voce registrata (se esiste) con lo stato reale su
    /// disco (available da AvailableModes, checksum dal manifest, probe, attached). Ritorna
    /// null se il brain non è né registrato né disponibile.</summary>
    public static BrainInfo? Info(string name)
    {
        var p = Load();
        var rec = p.Brains.FirstOrDefault(b => string.Equals(b.Name, name, StringComparison.OrdinalIgnoreCase));
        var available = AvailableModes(name);
        if (available.Length == 0 && rec is null) return null;

        var manifest = TryLoadManifest(name);
        var checksum = rec?.Checksum ?? manifest?.Checksum;
        var mode = rec?.Mode
            ?? (available.Length == 2 ? "both" : available.FirstOrDefault() ?? "");

        return new BrainInfo
        {
            Name = name,
            Source = rec?.Source,
            VaultPath = rec?.VaultPath,
            Mode = mode,
            Available = [.. available],
            Checksum = checksum,
            Version = rec?.Version ?? manifest?.Version,
            Docs = rec?.Docs ?? manifest?.Docs ?? 0,
            Parsers = rec?.Parsers ?? manifest?.Parsers ?? [],
            Attached = p.Attached.Contains(name, StringComparer.OrdinalIgnoreCase),
            InstalledAt = rec?.InstalledAt,
            Probe = rec?.Probe ?? manifest?.Probe ?? [],
            SignedBy = rec?.SignedBy ?? manifest?.SignedBy,
        };
    }

    /// <summary>Registra (o aggiorna) un brain nel registry. Usato da BrainInstaller (import),
    /// BrainSigner (firma) e dai generatori. Aggiorna anche la disponibilità reale.</summary>
    public static BrainInfo Register(string name, string? source = null, string? vaultPath = null,
        string? mode = null, string? checksum = null, string? version = null, long docs = 0,
        List<string>? parsers = null, Dictionary<string, string>? probe = null, string? signedBy = null)
    {
        var p = Load();
        var existing = p.Brains.FirstOrDefault(b => string.Equals(b.Name, name, StringComparison.OrdinalIgnoreCase));
        var updated = new BrainInfo
        {
            Name = name,
            Source = source ?? existing?.Source,
            VaultPath = vaultPath ?? existing?.VaultPath,
            Mode = mode ?? existing?.Mode ?? "",
            Available = [.. AvailableModes(name)],
            Checksum = checksum ?? existing?.Checksum,
            Version = version ?? existing?.Version,
            Docs = docs > 0 ? docs : existing?.Docs ?? 0,
            Parsers = parsers ?? existing?.Parsers ?? [],
            Attached = existing?.Attached ?? p.Attached.Contains(name, StringComparer.OrdinalIgnoreCase),
            InstalledAt = existing?.InstalledAt ?? DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss"),
            Probe = probe ?? existing?.Probe ?? [],
            SignedBy = signedBy ?? existing?.SignedBy,
        };
        var list = p.Brains.Where(b => !string.Equals(b.Name, name, StringComparison.OrdinalIgnoreCase)).ToList();
        list.Add(updated);
        Save(p with { Brains = list });
        return updated;
    }

    /// <summary>Rimuove un brain dal registry (UNINSTALL). NON elimina i file su disco.</summary>
    public static bool Unregister(string name)
    {
        var p = Load();
        var filtered = p.Brains.Where(b => !string.Equals(b.Name, name, StringComparison.OrdinalIgnoreCase)).ToList();
        var attached = p.Attached.Where(x => !string.Equals(x, name, StringComparison.OrdinalIgnoreCase)).ToList();
        if (filtered.Count == p.Brains.Count && attached.Count == p.Attached.Count) return false;
        Save(p with { Attached = attached, Brains = filtered });
        return true;
    }

    // ── Manifest (firma) ───────────────────────────────────────────────────

    /// <summary>Legge brain.manifest.json dalla root del brain, se presente.</summary>
    public static BrainManifest? TryLoadManifest(string name)
    {
        var root = BrainRoot(name);
        if (root is null) return null;
        var path = Path.Combine(root, "brain.manifest.json");
        if (!File.Exists(path)) return null;
        try
        {
            return JsonSerializer.Deserialize<BrainManifest>(File.ReadAllText(path), JsonOpts);
        }
        catch { return null; }
    }

    public static string ManifestPath(string name) =>
        BrainRoot(name) is string root ? Path.Combine(root, "brain.manifest.json") : "";

    /// <summary>Azzera lo stato (test): rimuove il file del registry.</summary>
    internal static void ResetForTests()
    {
        try { File.Delete(JsonPath); } catch { }
    }
}
