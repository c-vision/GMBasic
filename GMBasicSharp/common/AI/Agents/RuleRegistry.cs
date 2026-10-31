// FILE: Basic.Agents.RuleRegistry — Orchestrazione agenti (A5): Rules injection gerarchica.
// Registro di regole per SCOPE (directory): ogni scope ha un contenuto (es. AGENTS.md). La
// risoluzione per un path concatena le regole di tutti gli scope ANCESTRALI, dal più generico
// (root) al più specifico (leaf) — come la risoluzione gerarchica di AGENTS.md.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_rules.c):
// stesse operazioni, stesso formato JSON. Thread-safe. v1 in-memory (il caricamento da disco
// di AGENTS.md/rules è un follow-on).
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe delle regole per scope. ADD/REMOVE/CLEAR scrivono,
/// FOR/LIST leggono.</summary>
public static class RuleRegistry
{
    private static readonly ConcurrentDictionary<string, string> _rules =
        new(StringComparer.OrdinalIgnoreCase);

    private static string Norm(string scope)
        => scope.TrimEnd('/', '\\');

    /// <summary>Registra/aggiorna le regole di uno scope. 1 = ok, 0 = scope vuoto.</summary>
    public static double Add(string scope, string content)
    {
        if (string.IsNullOrWhiteSpace(scope)) return 0.0;
        _rules[Norm(scope)] = content ?? "";
        return 1.0;
    }

    /// <summary>Rimuove le regole di uno scope. 1 = rimosso, 0 = assente.</summary>
    public static double Remove(string scope)
    {
        if (string.IsNullOrWhiteSpace(scope)) return 0.0;
        return _rules.TryRemove(Norm(scope), out _) ? 1.0 : 0.0;
    }

    /// <summary>JSON di tutti gli scope, ordinati per scope.</summary>
    public static string ListJson()
    {
        var keys = _rules.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"rules\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append("{\"scope\":\"").Append(Escape(keys[i]))
              .Append("\",\"content\":\"").Append(Escape(_rules[keys[i]])).Append("\"}");
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro. Ritorna il numero di scope rimossi.</summary>
    public static double Clear()
    {
        var n = _rules.Count;
        _rules.Clear();
        return n;
    }

    /// <summary>Persistenza: `scope\x1fcontent` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _rules)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            sb.Append(kv.Key).Append('\u001f').Append(kv.Value);
        }
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        foreach (var rec in state.Split('\u001e'))
        {
            var i = rec.IndexOf('\u001f');
            if (i > 0) Add(rec[..i], rec[(i + 1)..]);
        }
    }

    /// <summary>Risolve le regole applicabili a un path: concatena i contenuti degli scope
    /// ancestrali, dal più generico (scope più corto) al più specifico, separati da newline.
    /// Scope "root" ("" o "/") si applica a tutto.</summary>
    public static string Resolve(string path)
    {
        var p = Norm(path ?? "");
        var matches = new List<(string Scope, string Content)>();
        foreach (var kv in _rules)
        {
            var s = kv.Key;
            if (s.Length == 0 || s == "/")
            {
                matches.Add((s, kv.Value));
                continue;
            }
            if (p.Equals(s, StringComparison.OrdinalIgnoreCase) ||
                p.StartsWith(s + "/", StringComparison.OrdinalIgnoreCase))
                matches.Add((s, kv.Value));
        }
        matches.Sort((a, b) => a.Scope.Length.CompareTo(b.Scope.Length));
        var sb = new StringBuilder();
        foreach (var m in matches)
        {
            if (sb.Length > 0) sb.Append('\n');
            sb.Append(m.Content);
        }
        return sb.ToString();
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
