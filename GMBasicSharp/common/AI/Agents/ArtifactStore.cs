// FILE: Basic.Agents.ArtifactStore — Orchestrazione agenti (A6): Artifact Store.
// Registro di artefatti REFERENZIABILI senza duplicarne il contenuto: ogni artefatto è
// (nome, tipo, riferimento). PUT registra un file; LINK registra una URI (filesystem/Git/);
// GET risolve il riferimento; LIST elenca; REMOVE elimina. Non copia mai il contenuto.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_artifact.c):
// stesse operazioni, stesso formato JSON. Thread-safe.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe di artefatti (nome → tipo + riferimento). PUT/LINK/REMOVE
/// scrivono, GET/LIST leggono. Il contenuto non è mai duplicato: si memorizza solo il riferimento.</summary>
public static class ArtifactStore
{
    private sealed class Artifact
    {
        public string Name = "";
        public string Kind = "";   // "file" | "link"
        public string Ref = "";
    }

    private static readonly ConcurrentDictionary<string, Artifact> _arts =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Registra un artefatto FILE (nome → path). 1 = ok, 0 = nome/path vuoti.</summary>
    public static double Put(string name, string path)
        => Store(name, "file", path);

    /// <summary>Registra un artefatto LINK (nome → uri). 1 = ok, 0 = nome/uri vuoti.</summary>
    public static double Link(string name, string uri)
        => Store(name, "link", uri);

    private static double Store(string name, string kind, string reference)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(reference)) return 0.0;
        _arts[name] = new Artifact { Name = name, Kind = kind, Ref = reference };
        return 1.0;
    }

    /// <summary>Riferimento dell'artefatto ("" se assente).</summary>
    public static string Get(string name)
        => !string.IsNullOrWhiteSpace(name) && _arts.TryGetValue(name, out var a) ? a.Ref : "";

    /// <summary>Tipo dell'artefatto: "file"/"link"/"" (assente).</summary>
    public static string Kind(string name)
        => !string.IsNullOrWhiteSpace(name) && _arts.TryGetValue(name, out var a) ? a.Kind : "";

    /// <summary>Rimuove un artefatto. 1 = rimosso, 0 = assente.</summary>
    public static double Remove(string name)
        => !string.IsNullOrWhiteSpace(name) && _arts.TryRemove(name, out _) ? 1.0 : 0.0;

    /// <summary>JSON di tutti gli artefatti, ordinati per nome.</summary>
    public static string ListJson()
    {
        var keys = _arts.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"artifacts\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            var a = _arts[keys[i]];
            sb.Append("{\"name\":\"").Append(Escape(a.Name))
              .Append("\",\"kind\":\"").Append(Escape(a.Kind))
              .Append("\",\"ref\":\"").Append(Escape(a.Ref)).Append("\"}");
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() => _arts.Clear();

    /// <summary>Serializza lo stato (persistenza): record `name\x1fkind\x1fref` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _arts)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            sb.Append(kv.Value.Name).Append('\u001f').Append(kv.Value.Kind).Append('\u001f').Append(kv.Value.Ref);
        }
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        foreach (var rec in state.Split('\u001e'))
        {
            var f = rec.Split('\u001f');
            if (f.Length >= 3) Store(f[0], f[1], f[2]);
        }
    }

    private static string Escape(string s)
    {
        var sb = new StringBuilder(s.Length + 2);
        foreach (var c in s)
        {
            switch (c)
            {
                case '"': sb.Append("\\\""); break;
                case '\\': sb.Append("\\\\"); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default: sb.Append(c); break;
            }
        }
        return sb.ToString();
    }
}
