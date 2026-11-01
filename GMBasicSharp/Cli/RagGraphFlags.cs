// REPL/RagGraphFlags.cs — parsing flag dei comandi RAG/GRAPH (CLI + REPL condividono questo).
// Ogni flag ha un nome lungo (--rag) e un alias breve (-r) per ridurre la digitazione.
// Priorità: il nome lungo e l'alias sono equivalenti; se entrambi presenti vince il primo trovato.
// Convenzione alias: -r rag, -g graphs, -n name, -c chunk, -C collection, -o overlap, -t topk,
// -d damping, -i iterations, -D dry-run, -f force, -p prune.

namespace Basic.Executor;

internal static class RagGraphFlags
{
    /// <summary>Ritorna il valore del flag (nome lungo o alias), o null se assente.</summary>
    public static string? Value(string[] args, string longName, string shortName)
    {
        for (var i = 0; i < args.Length; i++)
        {
            if (args[i].Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                args[i].Equals(shortName, StringComparison.OrdinalIgnoreCase))
                return i + 1 < args.Length ? args[i + 1] : null;
        }
        return null;
    }

    /// <summary>Vero se il flag booleano (nome lungo o alias) è presente.</summary>
    public static bool Has(string[] args, string longName, string shortName) =>
        args.Any(a => a.Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                      a.Equals(shortName, StringComparison.OrdinalIgnoreCase));

    // ── Overload per List<string> (usato dal REPL) ──────────────────────────
    public static string? Value(List<string> args, string longName, string shortName)
    {
        for (var i = 0; i < args.Count; i++)
        {
            if (args[i].Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                args[i].Equals(shortName, StringComparison.OrdinalIgnoreCase))
                return i + 1 < args.Count ? args[i + 1] : null;
        }
        return null;
    }

    public static bool Has(List<string> args, string longName, string shortName) =>
        args.Any(a => a.Equals(longName, StringComparison.OrdinalIgnoreCase) ||
                      a.Equals(shortName, StringComparison.OrdinalIgnoreCase));

    // ── Default name allineato alla BUILD ────────────────────────────────────
    // build crea <dir>/<basename-wiki>.db senza --name; status/query NON hanno il wikiDir,
    // quindi il default non può derivare dal basename del wiki. Regola coerente: se nella dir
    // c'è ESATTAMENTE UN artefatto, si usa il suo basename (è il db che la build ha creato);
    // se ce ne sono più di uno o zero, chi chiama emette un messaggio che chiede --name.
    /// <summary>Basename dell'unico file che matcha <paramref name="pattern"/> in <paramref name="dir"/>
    /// (senza estensione), o null se 0 o più di 1. Il pattern è un glob semplice '*.ext'.</summary>
    public static string? SingleArtifactName(string dir, string pattern)
    {
        if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir)) return null;
        var ext = Path.GetExtension(pattern);
        var files = Directory.GetFiles(dir, pattern)
            .Where(f => f.EndsWith(ext, StringComparison.OrdinalIgnoreCase))
            .ToArray();
        if (files.Length != 1) return null;
        return Path.GetFileNameWithoutExtension(files[0]);
    }

    /// <summary>Nome del graph univoco in <paramref name="dir"/>: basename di un unico
    /// *.state.json con il suffisso ".state" rimosso (la build produce &lt;name&gt;.state.json).</summary>
    public static string? SingleGraphName(string dir)
    {
        var raw = SingleArtifactName(dir, "*.state.json");
        if (raw is null) return null;
        return raw.EndsWith(".state", StringComparison.OrdinalIgnoreCase)
            ? raw[..^".state".Length]
            : raw;
    }
}
