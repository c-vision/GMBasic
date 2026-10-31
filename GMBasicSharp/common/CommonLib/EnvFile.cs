// FILE: CommonLib.EnvFile.cs

/*
 * EnvFile.cs: lettura/scrittura del file .env alla root del repo (GmbiPaths.RepoRoot/.env).
 *
 * REGOLA (AGENTS.md §0bis/§10): C# e C sono runtime SEPARATI e NON condividono codice. Questo
 * file serve SOLO al mondo C# (Executor/REPL/common/*), stesso ruolo che GmbiPaths.cs ha per
 * system.settings.json.
 *
 * SCOPO: config/*.json (ai.json, providers.json, ...) sono ORA versionati (2026-09-07) — i
 * segreti (API key, token) non ci possono più vivere in chiaro. .env resta SEMPRE gitignored
 * (vedi .gitignore) e ospita SOLO quei valori. Il resto delle impostazioni continua a vivere
 * nei file JSON come prima; .env copre esclusivamente ciò che non deve mai essere versionato.
 *
 * Convenzione dotenv standard: le variabili già presenti nel processo/shell reale VINCONO sul
 * file (EnsureLoaded non sovrascrive). Set() aggiorna sia il file sia il processo corrente.
 */

using System.Text;

namespace CommonLib;

/// <summary>Lettura/scrittura .env (root repo) — per i segreti che non possono vivere nei
/// file di config JSON versionati. Caricato automaticamente al primo accesso a GmbiPaths.</summary>
public static class EnvFile
{
    private static readonly object Lock = new();
    private static bool _loaded;
    private static string? _pathOverride;
    private static readonly Stack<string?> OverrideStack = new();

    public static string DefaultPath => _pathOverride ?? Path.Combine(GmbiPaths.RepoRoot, ".env");

    /// <summary>True se un override di test è attivo. Usato da GmbiPaths.Build() per NON caricare
    /// il .env reale nel processo quando un test ha già impostato un path isolato (altrimenti
    /// GmbiPaths passerebbe comunque il path reale esplicito, bypassando l'override — bug reale
    /// osservato 2026-09-07: un token HF vero caricato durante i test faceva scattare per errore
    /// Marketplace_PushPull_RoundTrip_WhenTokenAvailable, un push/pull di rete reale verso HF).</summary>
    internal static bool HasOverride { get { lock (Lock) return _pathOverride is not null; } }

    /// <summary>Hook TEST-SCOPED: reindirizza il file .env a un path temporaneo (stesso pattern
    /// di AiConfig.OverridePathForTests) — i test non devono mai toccare il .env reale. A pila
    /// (push/pop): un override annidato (es. una classe di test dentro il [ModuleInitializer]
    /// globale del progetto test) ripristina quello esterno con ResetForTests, MAI il reale.</summary>
    internal static void OverridePathForTests(string path)
    {
        lock (Lock) { OverrideStack.Push(_pathOverride); _pathOverride = path; _loaded = false; }
    }

    internal static void ResetForTests()
    {
        lock (Lock)
        {
            _pathOverride = OverrideStack.Count > 0 ? OverrideStack.Pop() : null;
            _loaded = false;
        }
    }

    /// <summary>Carica .env in Environment (idempotente per processo). Non sovrascrive variabili
    /// già impostate nel processo/shell reale: l'ambiente vince sempre sul file.</summary>
    public static void EnsureLoaded(string? path = null)
    {
        lock (Lock)
        {
            var file = path ?? DefaultPath;
            if (_loaded && path is null) return;
            _loaded = true;
            if (!File.Exists(file)) return;
            try
            {
                foreach (var (key, value) in ParseLines(File.ReadAllLines(file)))
                {
                    if (Environment.GetEnvironmentVariable(key) is null)
                        Environment.SetEnvironmentVariable(key, value);
                }
            }
            catch { /* .env malformato: nessun crash, si procede senza */ }
        }
    }

    /// <summary>Legge una chiave (garantisce prima il caricamento del file nel processo).</summary>
    public static string? Get(string key)
    {
        EnsureLoaded();
        return Environment.GetEnvironmentVariable(key);
    }

    /// <summary>Imposta/aggiorna una chiave sia nel file .env sia nel processo corrente.
    /// Preserva le righe esistenti (commenti inclusi), sostituisce in-place se la chiave c'è
    /// già, altrimenti la aggiunge in coda.</summary>
    public static void Set(string key, string value, string? path = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        var file = path ?? DefaultPath;
        lock (Lock)
        {
            var lines = File.Exists(file) ? new List<string>(File.ReadAllLines(file)) : [];
            var prefix = key + "=";
            var idx = lines.FindIndex(l => l.StartsWith(prefix, StringComparison.Ordinal));
            var line = key + "=" + value;
            if (idx >= 0) lines[idx] = line; else lines.Add(line);

            var dir = Path.GetDirectoryName(file);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(file, string.Join('\n', lines) + '\n', Encoding.UTF8);

            Environment.SetEnvironmentVariable(key, value);
        }
    }

    private static IEnumerable<(string Key, string Value)> ParseLines(IEnumerable<string> lines)
    {
        foreach (var raw in lines)
        {
            var line = raw.Trim();
            if (line.Length == 0 || line[0] == '#') continue;
            var eq = line.IndexOf('=');
            if (eq <= 0) continue;
            var key = line[..eq].Trim();
            var value = line[(eq + 1)..].Trim();
            if (value.Length >= 2 &&
                ((value[0] == '"' && value[^1] == '"') || (value[0] == '\'' && value[^1] == '\'')))
                value = value[1..^1];
            yield return (key, value);
        }
    }
}
