// AiContextEngine.cs — Context Engine (P2 AI-Runtime consolidato, asse B).
// GVS5H: CONTEXT = VIEW curata sullo STATE, non CHAT HISTORY. Fornisce budget,
// selector, policy (compact|truncate|error) e view builder. Opt-in: il loop tool
// usa BuildTranscript legacy a meno che non sia attiva una view.
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Basic.AI.Context;

/// <summary>Configurazione del context engine per un agente.</summary>
public sealed class ContextConfig
{
    public long BudgetChars = 0;        // 0 = nessun limite
    public string Selector = "";        // "task,deps,freshness" (v1: no-op)
    public string Policy = "compact";   // compact|truncate|error
}

/// <summary>Vista di contesto costruita dal Context Engine.</summary>
public sealed class ContextView
{
    public List<string> Sections = new();
    public bool Truncated;
    public long TotalChars;
}

/// <summary>Context Engine: budget + view builder (v1: null-safe, deterministico).</summary>
public static class AiContextEngine
{
    private static readonly object Gate = new();
    private static readonly Dictionary<string, ContextConfig> Configs = new();
    private static readonly Dictionary<string, string> Views = new();
    private static readonly Dictionary<string, long> OverflowChars = new();   // overflow dell'ultima view normalizzata
    private static readonly Dictionary<string, bool> Compacted = new();       // l'ultima view è stata compattata/troncata

    /// <summary>True se l'agente ha una configurazione di context engine (OPT-IN: senza, il tool loop
    /// usa il transcript legacy, comportamento invariato).</summary>
    public static bool IsConfigured(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate)
        {
            if (Views.TryGetValue(agent, out var v) && !string.IsNullOrEmpty(v)) return true;
            if (!Configs.TryGetValue(agent, out var c)) return false;
            return c.BudgetChars > 0 || !string.IsNullOrEmpty(c.Selector) || !string.IsNullOrEmpty(c.Policy);
        }
    }

    /// <summary>
    /// Normalizza un testo secondo budget + policy. Deterministico e identico al runtime C
    /// (`gmb_context.c`, parità 5/5):
    ///   • budget &lt;= 0 o testo entro budget → invariato;
    ///   • `truncate` → taglio netto ai primi `budget` char;
    ///   • `compact` (default) → testa 60% + marcatore con i char omessi + coda, garantito ≤ budget;
    ///   • `error` → eccezione esplicita (mai un taglio silenzioso).
    /// Ritorna (testo, overflowChar) dove overflow = lunghezza originale − budget quando si taglia.
    /// </summary>
    public static (string Text, long Overflow) Normalize(string agent, string text)
    {
        text ??= "";
        var budget = GetBudget(agent);
        if (budget <= 0 || text.Length <= budget) return (text, 0);
        var policy = GetPolicy(agent);
        if (policy == "error")
            throw new InvalidOperationException(
                $"Context overflow per agent '{agent}': {text.Length} > budget {budget}");
        var overflow = text.Length - budget;
        if (policy == "truncate") return (text[..(int)budget], overflow);

        // compact: marcatore FISSO + testa 60% / coda 40% dello spazio restante → risultato di
        // lunghezza ESATTAMENTE budget (il numero di char omessi è già esposto da AI.CONTEXTOVERFLOW).
        const string Marker = "\n…compattato…\n";   // 14 char, fisso: parità byte-identica con gmb_context.c
        if (budget <= Marker.Length) return (text[..(int)budget], overflow);   // budget minimo: taglio netto
        var room = (int)budget - Marker.Length;
        var head = room * 6 / 10;
        var tail = room - head;
        if (head + tail > text.Length) { head = text.Length; tail = 0; }
        return (text[..head] + Marker + text[(text.Length - tail)..], overflow);
    }

    /// <summary>Imposta il budget in caratteri per un agente.</summary>
    public static void SetBudget(string agent, long budgetChars)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate)
        {
            if (!Configs.TryGetValue(agent, out var cfg)) { cfg = new ContextConfig(); Configs[agent] = cfg; }
            cfg.BudgetChars = budgetChars;
        }
    }

    /// <summary>Imposta lo selector (v1: accettato e conservato, non applicato).</summary>
    public static void SetSelector(string agent, string selector)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate)
        {
            if (!Configs.TryGetValue(agent, out var cfg)) { cfg = new ContextConfig(); Configs[agent] = cfg; }
            cfg.Selector = selector ?? "";
        }
    }

    /// <summary>Imposta la policy compact|truncate|error.</summary>
    public static void SetPolicy(string agent, string policy)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate)
        {
            if (!Configs.TryGetValue(agent, out var cfg)) { cfg = new ContextConfig(); Configs[agent] = cfg; }
            cfg.Policy = policy ?? "compact";
        }
    }

    /// <summary>Imposta la vista curata per la prossima chiamata dell'agente. La vista viene
    /// NORMALIZZATA SUBITO secondo budget+policy (v. <see cref="Normalize"/>): così
    /// `AI.CONTEXTUSED/OVERFLOW/VIEW$` riflettono ciò che il loop userà davvero (prima la policy
    /// era applicata solo in BuildView, che nessuno chiamava: engine di fatto inerte).</summary>
    public static bool SetView(string agent, string viewText)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        string norm; long overflow;
        try { (norm, overflow) = Normalize(agent, viewText ?? ""); }
        catch (InvalidOperationException) { return false; }   // policy 'error': vista NON aggiornata
        lock (Gate)
        {
            Views[agent] = norm;
            OverflowChars[agent] = overflow;
            Compacted[agent] = overflow > 0;
        }
        return true;
    }

    /// <summary>Legge la vista corrente (a scopo di audit AI.CONTEXTVIEW$).</summary>
    public static string GetView(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate) return Views.TryGetValue(agent, out var v) ? v : "";
    }

    /// <summary>Budget configurato per l'agente (caratteri).</summary>
    public static long GetBudget(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate) return Configs.TryGetValue(agent, out var c) ? c.BudgetChars : 0;
    }

    /// <summary>Caratteri usati dall'ultima vista costruita.</summary>
    public static long GetUsed(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate) return Views.TryGetValue(agent, out var v) ? (long)v.Length : 0;
    }

    /// <summary>Caratteri oltre il budget dell'ULTIMA VISTA NORMALIZZATA (0 se entro budget o nessun
    /// budget). È calcolato al momento di SetView (l'overflow del testo ORIGINALE), non dalla vista
    /// già tagliata — altrimenti dopo un `truncate` l'overflow risulterebbe sempre 0.</summary>
    public static long GetOverflow(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate) return OverflowChars.TryGetValue(agent, out var o) ? o : 0;
    }

    /// <summary>True se l'ultima vista dell'agente è stata compattata/troncata (audit).</summary>
    public static bool WasCompacted(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        lock (Gate) return Compacted.TryGetValue(agent, out var c) && c;
    }

    /// <summary>Costruisce (o riusa) la vista curata dell'agente secondo policy/budget.
    /// Se nessuna view è stata impostata, ricade sul transcript passato.</summary>
    public static ContextView BuildView(string agent, string fallbackTranscript)
    {
        string viewText;
        bool truncated = false;
        lock (Gate)
        {
            var hasView = Views.TryGetValue(agent, out var v) && !string.IsNullOrEmpty(v);
            viewText = hasView ? v! : (fallbackTranscript ?? "");
        }

        var (norm, overflow) = Normalize(agent, viewText);
        viewText = norm;
        truncated = overflow > 0;
        lock (Gate) OverflowChars[agent] = overflow;

        return new ContextView
        {
            Sections = new List<string> { viewText },
            Truncated = truncated,
            TotalChars = viewText.Length
        };
    }

    /// <summary>Policy corrente dell'agente (compact|truncate|error) — per il loop del worker,
    /// che decide lo stop del run PRIMA di costruire la vista (mai un taglio silenzioso).</summary>
    public static string GetPolicyFor(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        return GetPolicy(agent);
    }

    private static string GetPolicy(string agent)
    {
        lock (Gate) return Configs.TryGetValue(agent, out var c) ? c.Policy : "compact";
    }

    /// <summary>Comando AI CONTEXT SET: interpreta "BUDGET|SELECTOR|POLICY agent, value" da un singolo argomento.</summary>
    public static void SetContext(string agentValue, string value)
    {
        // agentValue può contenere "BUDGET agent|SELECTOR agent|POLICY agent + value"
        // v1: setta semplicemente budget/selector/policy nella configurazione dell'agente.
        var parts = value.Split(new[] { '=' }, 2);
        if (parts.Length == 2)
        {
            var key = parts[0].Trim().ToLowerInvariant();
            var val = parts[1].Trim();
            switch (key)
            {
                case "budget": if (long.TryParse(val, out var c)) SetBudget(agentValue, c); break;
                case "selector": SetSelector(agentValue, val); break;
                case "policy": SetPolicy(agentValue, val); break;
            }
        }
    }

    // ─────────────────── PARSEC §3.6 — context builder a SEGMENTI (lato AGENTE) ───────────────────
    // Speculare a gmb_context.c (parità 5/5). Lo store di segmenti vive accanto alla view.
    private sealed class Seg { public string Kind = "OTHER"; public string Text = ""; }
    private const int MaxSegs = 32;
    private static readonly Dictionary<string, List<Seg>> Segs = new();

    private static string NormKind(string? kind)
    {
        if (string.IsNullOrEmpty(kind)) return "OTHER";
        var k = kind!.Trim();
        if (k.Equals("SYSTEM", StringComparison.OrdinalIgnoreCase)) return "SYSTEM";
        if (k.Equals("HISTORY", StringComparison.OrdinalIgnoreCase)) return "HISTORY";
        if (k.Equals("TOOL", StringComparison.OrdinalIgnoreCase)) return "TOOL";
        return "OTHER";
    }

    private static List<Seg> SegList(string agent)
    {
        if (string.IsNullOrEmpty(agent)) agent = "@default";
        if (!Segs.TryGetValue(agent, out var l)) { l = new List<Seg>(); Segs[agent] = l; }
        return l;
    }

    /// <summary>AI CONTEXT CREATE <agent> — azzera i segmenti dello store.</summary>
    public static bool Create(string agent) { lock (Gate) { SegList(agent).Clear(); return true; } }

    /// <summary>AI CONTEXT ADD <agent>, <kind>, <text> — appende un segmento (kind normalizzato).</summary>
    public static bool Add(string agent, string kind, string text)
    {
        lock (Gate)
        {
            var l = SegList(agent);
            if (l.Count >= MaxSegs) return false;
            l.Add(new Seg { Kind = NormKind(kind), Text = text ?? "" });
            return true;
        }
    }

    /// <summary>AI CONTEXT MAP <agent> — report deterministico (stessa forma di gmb_ctx_map).</summary>
    public static string Map(string agent)
    {
        lock (Gate)
        {
            var a = string.IsNullOrEmpty(agent) ? "@default" : agent;
            var l = Segs.TryGetValue(a, out var s) ? s : new List<Seg>();
            long total = 0, sys = 0, hist = 0, tool = 0, other = 0;
            foreach (var g in l)
            {
                long L = g.Text.Length; total += L;
                switch (g.Kind) { case "SYSTEM": sys += L; break; case "HISTORY": hist += L; break; case "TOOL": tool += L; break; default: other += L; break; }
            }
            return $"ctx {a}: segs={l.Count} chars={total} budget={GetBudget(a)} overflow={GetOverflow(a)}\n" +
                   $"SYSTEM={sys} HISTORY={hist} TOOL={tool} OTHER={other}";
        }
    }

    /// <summary>AI CONTEXT TRIM <agent> — elimina i segmenti non-SYSTEM più vecchi finché totale <= budget.</summary>
    public static long Trim(string agent)
    {
        lock (Gate)
        {
            var a = string.IsNullOrEmpty(agent) ? "@default" : agent;
            var l = Segs.TryGetValue(a, out var s) ? s : null; if (l == null) return 0;
            var budget = GetBudget(a);
            if (budget <= 0) return 0;
            long total = l.Sum(g => (long)g.Text.Length), dropped = 0;
            while (total > budget)
            {
                int idx = l.FindIndex(g => g.Kind != "SYSTEM");
                if (idx < 0) break;
                total -= l[idx].Text.Length; l.RemoveAt(idx); dropped++;
            }
            return dropped;
        }
    }

    /// <summary>AI CONTEXT GC <agent> — eviction: duplicati (kind+text) + HISTORY oltre gli ultimi 8.</summary>
    public static long Gc(string agent)
    {
        lock (Gate)
        {
            var a = string.IsNullOrEmpty(agent) ? "@default" : agent;
            var l = Segs.TryGetValue(a, out var s) ? s : null; if (l == null) return 0;
            long dropped = 0;
            for (int i = 0; i < l.Count; i++)
                for (int j = i + 1; j < l.Count;)
                    if (l[i].Kind == l[j].Kind && l[i].Text == l[j].Text) { l.RemoveAt(j); dropped++; }
                    else j++;
            int hist = l.Count(g => g.Kind == "HISTORY");
            for (int i = 0; i < l.Count && hist > 8;)
                if (l[i].Kind == "HISTORY") { l.RemoveAt(i); hist--; dropped++; } else i++;
            return dropped;
        }
    }

    /// <summary>AI CONTEXT COMMIT <agent> — view = segmenti (SYSTEM per primi, poi gli altri) normalizzata.</summary>
    public static bool Commit(string agent)
    {
        lock (Gate)
        {
            var a = string.IsNullOrEmpty(agent) ? "@default" : agent;
            var l = Segs.TryGetValue(a, out var s) ? s : new List<Seg>();
            var sb = new StringBuilder();
            foreach (var g in l) if (g.Kind == "SYSTEM") sb.Append(g.Text);
            foreach (var g in l) if (g.Kind != "SYSTEM") sb.Append(g.Text);
            return SetView(a, sb.ToString());
        }
    }

    // §3.6 PARSEC — cache/version-hash (coordinamento context-cache ↔ prefix KV).
    /// <summary>Hash FNV-1a (hex 16) della vista curata: chiave della prefix-cache KV (l'invalidazione
    /// avviene quando la vista cambia → hash diverso).</summary>
    public static string ContextHash(string agent)
    {
        var v = GetView(agent);
        ulong h = 1469598103934665603UL;
        foreach (var ch in v) { h ^= (byte)ch; h *= 1099511628211UL; }
        return h.ToString("x16");
    }

    /// <summary>True se `rev` coincide col hash corrente della vista: accetta il hash nudo o "rev=&lt;hash&gt;".</summary>
    public static bool ContextValid(string agent, string rev)
    {
        var h = ContextHash(agent);
        return string.Equals(rev, h, StringComparison.Ordinal) ||
               string.Equals(rev, "rev=" + h, StringComparison.Ordinal);
    }

    /// <summary>Reset (test hook).</summary>
    public static void Reset()
    {
        lock (Gate) { Configs.Clear(); Views.Clear(); OverflowChars.Clear(); Compacted.Clear(); Segs.Clear(); }
    }
}
