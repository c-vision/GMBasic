// FILE: Basic.AI.Rag.BuildState
// Stato e resume condiviso dei build RAG/GRAPH. Sorgente di verità per il resume (riavvio
// in qualunque punto senza duplicare il lavoro) E per il diff UPDATE (solo nuovi/modificati).
// Riusabile da REPL, CLI e BASIC/agenti. Formato <name>.state.json accanto a db/grafo.
using System.Text.Json;

namespace Basic.AI.Rag;

/// <summary>Tipo di build a cui appartiene uno stato.</summary>
public enum BuildKind { Rag, Graph }

/// <summary>Risultato del diff wiki vs stato: 4 insiemi disgiunti.</summary>
public sealed record BuildDiff(
    IReadOnlyList<string> New,       // path non presenti nello stato → da indicizzare
    IReadOnlyList<string> Changed,   // presenti ma hash diverso → da ri-indicizzare
    IReadOnlyList<string> Unchanged, // presenti e hash identico → skip immediato
    IReadOnlyList<string> Removed);  // in stato ma non più nel wiki → solo con --prune

/// <summary>Stato persistente di un build RAG/GRAPH: mappa path relativo → hash SHA256 dei
/// file già processati. Salvato atomicamente (.tmp + rename) dopo ogni batch.</summary>
public sealed class BuildState
{
    public const int CurrentVersion = 1;

    public int Version { get; set; } = CurrentVersion;
    public BuildKind Kind { get; set; } = BuildKind.Rag;
    public string WikiDir { get; set; } = "";
    public DateTime StartedAt { get; set; }
    public DateTime UpdatedAt { get; set; }
    public int TotalFiles { get; set; }
    public int ProcessedFiles { get; set; }
    public string CurrentFile { get; set; } = "";
    public Dictionary<string, string> Files { get; set; } = new(StringComparer.Ordinal);

    public string StatePath { get; private set; } = "";

    /// <summary>Carica (o crea vuoto) lo stato dal path. Ritorna lo stato caricato.</summary>
    public static BuildState Load(string statePath)
    {
        var s = new BuildState { StatePath = statePath };
        if (!File.Exists(statePath)) return s;
        try
        {
            var raw = File.ReadAllText(statePath);
            var loaded = JsonSerializer.Deserialize<BuildState>(raw, JsonOpts);
            if (loaded is not null)
            {
                loaded.StatePath = statePath;
                return loaded;
            }
        }
        catch
        {
            // stato corrotto → riparte da zero (il db è comunque sorgente, lo stato si ricostruisce)
        }
        return s;
    }

    /// <summary>Salva atomicamente (scrive .tmp poi rename) — crash-safe.</summary>
    public void Save()
    {
        UpdatedAt = DateTime.UtcNow;
        if (string.IsNullOrEmpty(StatePath)) return;
        var dir = Path.GetDirectoryName(StatePath);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var tmp = StatePath + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(this, JsonOpts));
        File.Move(tmp, StatePath, overwrite: true);
    }

    /// <summary>Hash SHA256 del contenuto di un file (per il dedup incremental).</summary>
    public static string HashFile(string path)
    {
        using var sha = System.Security.Cryptography.SHA256.Create();
        using var fs = File.OpenRead(path);
        var bytes = sha.ComputeHash(fs);
        return Convert.ToHexString(bytes);
    }

    /// <summary>Hash SHA256 di un testo (per il diff semantico dei grafi).</summary>
    public static string HashText(string text)
    {
        using var sha = System.Security.Cryptography.SHA256.Create();
        var bytes = sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(text));
        return Convert.ToHexString(bytes);
    }

    /// <summary>Diff del wiki (elenco di path relativi reali + hash correnti) contro lo stato.
    /// <paramref name="wikiFiles"/> = mappa path relativo → hash corrente (già calcolati).</summary>
    public BuildDiff Diff(IReadOnlyDictionary<string, string> wikiFiles)
    {
        var newFiles = new List<string>();
        var changed = new List<string>();
        var unchanged = new List<string>();
        var removed = new List<string>();

        foreach (var (path, hash) in wikiFiles)
        {
            if (Files.TryGetValue(path, out var oldHash))
            {
                if (string.Equals(oldHash, hash, StringComparison.Ordinal)) unchanged.Add(path);
                else changed.Add(path);
            }
            else
            {
                newFiles.Add(path);
            }
        }
        foreach (var path in Files.Keys)
            if (!wikiFiles.ContainsKey(path)) removed.Add(path);

        return new BuildDiff(newFiles, changed, unchanged, removed);
    }

    /// <summary>Registra un file come processato (path → hash) e aggiorna contatori.</summary>
    public void MarkProcessed(string relativePath, string hash)
    {
        Files[relativePath] = hash;
        ProcessedFiles = Files.Count;
        CurrentFile = relativePath;
    }

    /// <summary>Rimuove un file (per --prune).</summary>
    public void Remove(string relativePath)
    {
        Files.Remove(relativePath);
        ProcessedFiles = Files.Count;
    }

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
    };
}
