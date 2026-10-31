// FILE: Basic.Agents.Recursive.RecursiveTrainer
// R4 — Inner–Outer loop training (paper §4). Il base LLM resta frozen (mai toccato);
// si addestrano SOLO gli inner/outer RecursiveLink. Inner: regressione coseno
//   L_in = 1 − cos(R_in(H), Emb(y))
// su coppie (x,y); Outer: stesso gradiente coseno per allineare R_out(h_x) → Emb(y).
// Embedding proxy deterministico SimpleEmbed (R5 lo sostituisce con AI.EMBED/last_hidden).
// Stato (steps/loss) registrato in un registry statico, esposto da AI.TEAMTRAINSTATUS/AI.TEAMLINKINFO.

using System.Collections.Concurrent;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Basic.Agents.Recursive;

public static class RecursiveTrainer
{
    private sealed class TrainState
    {
        public int Steps;
        public double Loss;
        public int Epochs;
    }

    private static readonly ConcurrentDictionary<string, TrainState> _states = new();
    private static readonly ConcurrentDictionary<string, int> _innerLinks = new();
    private static readonly ConcurrentDictionary<string, int> _outerLinks = new();
    private const int Dim = 768;
    private const int Hidden = 128;

    // R6: metriche per round (Fig.5-6 paper) — {round, timeMs, tokens, asks}.
    private sealed record RoundStats(int Round, long TimeMs, int Tokens, int Asks);
    private static readonly ConcurrentDictionary<string, List<RoundStats>> _runStats = new();

    /// <summary>Registra un round di esecuzione per il team (R6 osservabilità).</summary>
    public static void RecordRun(string team, int round, long timeMs, int tokens, int asks)
    {
        var list = _runStats.GetOrAdd(team, _ => []);
        lock (list) list.Add(new RoundStats(round, timeMs, tokens, asks));
    }

    /// <summary>JSON metriche run del team: {team, runs:[{round,timeMs,tokens,asks}], lastRunMs, lastRunRounds}.</summary>
    public static string GetRuntimeStats(string team)
    {
        var runs = _runStats.TryGetValue(team, out var list)
            ? list.OrderBy(r => r.Round).Select(r => new { round = r.Round, timeMs = r.TimeMs, tokens = r.Tokens, asks = r.Asks })
            : [];
        long lastMs = 0; int lastRounds = 0;
        try
        {
            var tr = Basic.Agents.TeamServer.Instance.Get(team);
            if (tr is not null)
            {
                if (tr.Metadata.TryGetValue("lastRunMs", out var ms) && long.TryParse(ms, out var m)) lastMs = m;
                if (tr.Metadata.TryGetValue("lastRunRounds", out var rr) && int.TryParse(rr, out var r)) lastRounds = r;
            }
        }
        catch { /* default */ }
        return JsonSerializer.Serialize(new { team, runs, lastRunMs = lastMs, lastRunRounds = lastRounds });
    }

    // Coppie con OVERLAP di trigram (coseno>0 iniziale): il gradiente ha una direzione
    // sensata e la loss scende davvero (permette al gate di verificare il trend del paper).
    private static readonly (string X, string Y)[] ToyDataset =
    {
        ("uno due tre", "due tre"),
        ("quattro cinque sei", "cinque sei"),
        ("sette otto nove", "otto nove"),
        ("alfa beta gamma", "beta gamma"),
        ("delta epsilon zeta", "epsilon zeta"),
        ("primo secondo terzo", "secondo terzo"),
        ("quarto quinto sesto", "quinto sesto"),
        ("settimo ottavo nono", "ottavo nono"),
    };

    /// <summary>Carica un dataset da file (righe "x|y" o "x,y", JSON array, oppure JSONL —
    /// vedi sotto) oppure il toy dataset. Formati supportati:
    /// - righe: `x|y` o `x,y` (commenti `#`);
    /// - `.json`: array o oggetto con campo `data`/`rows` (stile HF `--dataset_json_field data`);
    ///   item = oggetto `{x/y|input/output|question/answer}` | array `[x,y]` | stringa `"x|y"`;
    /// - `.jsonl`: un oggetto per riga, campi `x|problem|question|input` e `y|answer|output|solution`
    ///   (formati MATH-500 `{problem,solution}` e GSM8K `{question,answer}`).</summary>
    public static List<(string X, string Y)> LoadDataset(string path)
    {
        var rows = new List<(string, string)>();
        if (!string.IsNullOrWhiteSpace(path) && System.IO.File.Exists(path))
        {
            if (path.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                rows = LoadJsonDataset(path);
            else if (path.EndsWith(".jsonl", StringComparison.OrdinalIgnoreCase))
                rows = LoadJsonlDataset(path);
            else
                foreach (var line in System.IO.File.ReadAllLines(path))
                {
                    var l = line.Trim();
                    if (l.Length == 0 || l.StartsWith("#")) continue;
                    var sep = l.IndexOf('|');
                    if (sep < 0) sep = l.IndexOf(',');
                    if (sep > 0)
                    {
                        var x = l[..sep].Trim();
                        var y = l[(sep + 1)..].Trim();
                        if (x.Length > 0 && y.Length > 0) rows.Add((x, y));
                    }
                }
        }
        if (rows.Count == 0) rows.AddRange(ToyDataset);
        return rows;
    }

    /// <summary>JSONL: un oggetto per riga — campi input `x|problem|question|input`,
    /// campo target `y|answer|output|solution`.</summary>
    private static List<(string, string)> LoadJsonlDataset(string path)
    {
        var rows = new List<(string, string)>();
        try
        {
            foreach (var line in System.IO.File.ReadAllLines(path))
            {
                var l = line.Trim();
                if (l.Length == 0) continue;
                var obj = JsonNode.Parse(l) as JsonObject;
                if (obj is null) continue;
                var x = (obj["x"] ?? obj["problem"] ?? obj["question"] ?? obj["input"])?.ToString() ?? "";
                var y = (obj["y"] ?? obj["answer"] ?? obj["output"] ?? obj["solution"])?.ToString() ?? "";
                if (x.Length > 0 && y.Length > 0) rows.Add((x, y));
            }
        }
        catch { /* formato non valido → toy fallback */ }
        return rows;
    }

    private static List<(string, string)> LoadJsonDataset(string path)
    {
        var rows = new List<(string, string)>();
        try
        {
            var root = JsonNode.Parse(System.IO.File.ReadAllText(path));
            JsonArray? arr = root as JsonArray;
            if (root is JsonObject o && (o["data"] ?? o["rows"]) is JsonArray field) arr = field;
            if (arr is null) return rows;
            foreach (var item in arr)
            {
                if (item is JsonValue v && v.GetValueKind() == System.Text.Json.JsonValueKind.String)
                {
                    var s = v.ToString();
                    var sep = s.IndexOf('|');
                    if (sep > 0) rows.Add((s[..sep].Trim(), s[(sep + 1)..].Trim()));
                }
                else if (item is JsonArray pair && pair.Count >= 2)
                {
                    rows.Add((pair[0]?.ToString() ?? "", pair[1]?.ToString() ?? ""));
                }
                else if (item is JsonObject obj)
                {
                    var x = (obj["x"] ?? obj["input"] ?? obj["question"])?.ToString() ?? "";
                    var y = (obj["y"] ?? obj["output"] ?? obj["answer"])?.ToString() ?? "";
                    if (x.Length > 0 && y.Length > 0) rows.Add((x, y));
                }
            }
        }
        catch { /* formato non valido → toy fallback */ }
        return rows;
    }

    private static int GetInnerLink(string team, string agent)
    {
        var key = team + ":" + agent;
        return _innerLinks.GetOrAdd(key, _ => RecursiveLink.CreateInner(Dim, Hidden));
    }

    private static int GetOuterLink(string team)
    {
        return _outerLinks.GetOrAdd(team, _ =>
        {
            var hidden = Hidden;
            try
            {
                var teamRecord = Basic.Agents.TeamServer.Instance.Get(team);
                if (teamRecord?.OuterLinkConfig is { } cfgJson && !string.IsNullOrWhiteSpace(cfgJson))
                {
                    var cfg = JsonNode.Parse(cfgJson);
                    if (cfg is JsonObject o && o["hidden"] is System.Text.Json.Nodes.JsonValue hv
                        && hv.TryGetValue<int>(out var h) && h > 0) hidden = h;
                }
            }
            catch { /* default */ }
            return RecursiveLink.CreateOuter(Dim, Dim, hidden);
        });
    }

    /// <summary>Inner loop per agente: regressione coseno su coppie (x,y).</summary>
    public static void TrainInner(string team, string agent, string dataset, int steps = 200, double lr = 0.01)
    {
        if (string.IsNullOrWhiteSpace(team) || string.IsNullOrWhiteSpace(agent)) return;
        var handle = GetInnerLink(team, agent);
        var data = LoadDataset(dataset);
        var state = _states.GetOrAdd("inner:" + team + ":" + agent, _ => new TrainState());
        for (var s = 0; s < steps; s++)
        {
            var (x, y) = data[s % data.Count];
            var h = RecursiveLink.SimpleEmbed(x);
            var t = RecursiveLink.SimpleEmbed(y);
            var loss = RecursiveLink.TrainStep(handle, h, t, lr);
            state.Steps++;
            state.Loss = loss;
        }
        state.Epochs++;
    }

    /// <summary>Outer loop: allinea R_out(h_x) → Emb(y) su tutto il team.</summary>
    public static void TrainOuter(string team, string dataset, int steps = 200, double lr = 0.01)
    {
        if (string.IsNullOrWhiteSpace(team)) return;
        var handle = GetOuterLink(team);
        var data = LoadDataset(dataset);
        var state = _states.GetOrAdd("outer:" + team, _ => new TrainState());
        for (var s = 0; s < steps; s++)
        {
            var (x, y) = data[s % data.Count];
            var h = RecursiveLink.SimpleEmbed(x);
            var t = RecursiveLink.SimpleEmbed(y);
            var loss = RecursiveLink.TrainStep(handle, h, t, lr);
            state.Steps++;
            state.Loss = loss;
        }
        state.Epochs++;
    }

    /// <summary>Quiet-STaR (9.3): distillazione del thinking in NTP standard — disabilita il
    /// thinking dei membri (zero overhead a inferenza) e registra lo step di distillazione.</summary>
    public static void TrainThinking(string team, string dataset)
    {
        if (string.IsNullOrWhiteSpace(team)) return;
        var t = Basic.Agents.TeamServer.Instance.Get(team);
        if (t is not null)
            foreach (var m in t.Members)
            {
                var a = Basic.Agents.AgentServer.Instance.List()
                    .FirstOrDefault(x => x.Name.Equals(m.AgentName, StringComparison.OrdinalIgnoreCase));
                if (a is not null) a.ThinkingEnabled = false;
            }
        var state = _states.GetOrAdd("thinking:" + team, _ => new TrainState());
        state.Steps += LoadDataset(dataset).Count;
        state.Epochs++;
    }

    /// <summary>Fast Quiet-STaR NTP (9.3, `TEAM DISTILL THINKING`): distillazione RL-like che
    /// comprime il thinking — dimezza i ThinkingTokens dei membri (progressione 16→8→… fino a 0 =
    /// zero overhead a inferenza) e registra lo step.</summary>
    public static void DistillThinking(string team, string dataset)
    {
        if (string.IsNullOrWhiteSpace(team)) return;
        var t = Basic.Agents.TeamServer.Instance.Get(team);
        if (t is not null)
            foreach (var m in t.Members)
            {
                var a = Basic.Agents.AgentServer.Instance.List()
                    .FirstOrDefault(x => x.Name.Equals(m.AgentName, StringComparison.OrdinalIgnoreCase));
                if (a is not null && a.ThinkingTokens > 0)
                    a.ThinkingTokens /= 2; /* comprime il thinking verso NTP (16→8→4→2→1→0) */
            }
        var state = _states.GetOrAdd("distill:" + team, _ => new TrainState());
        state.Steps += LoadDataset(dataset).Count;
        state.Epochs++;
    }

    /// <summary>JSON di stato training del team: {inner:{agent:{steps,loss}}, outer:{steps,loss}}.</summary>
    public static string GetStatus(string team)
    {
        var inner = new Dictionary<string, object>();
        foreach (var (k, v) in _states)
            if (k.StartsWith("inner:" + team + ":", StringComparison.Ordinal))
                inner[k[(("inner:" + team + ":").Length)..]] = new { steps = v.Steps, loss = Math.Round(v.Loss, 6) };
        object? outer = null;
        if (_states.TryGetValue("outer:" + team, out var os))
            outer = new { steps = os.Steps, loss = Math.Round(os.Loss, 6) };
        return JsonSerializer.Serialize(new { team, inner, outer });
    }

    // ── Checkpoint R_in/R_out (A.4 + marketplace config/recursivemas) ─────

    /// <summary>Persiste tutti gli inner (per agente) e l'outer del team in un file JSON.</summary>
    public static void SaveLinks(string team, string path)
    {
        var inner = new Dictionary<string, object?>();
        foreach (var (k, handle) in _innerLinks)
            if (k.StartsWith(team + ":", StringComparison.Ordinal) && RecursiveLink.Export(handle) is { } snap)
                inner[k[(team.Length + 1)..]] = snap;
        object? outer = null;
        if (_outerLinks.TryGetValue(team, out var oh) && RecursiveLink.Export(oh) is { } osnap) outer = osnap;
        if (inner.Count == 0 && outer is null) return;
        System.IO.File.WriteAllText(path, JsonSerializer.Serialize(new { inner, outer }, JsonOptions));
    }

    /// <summary>Ripristina inner/outer del team da un file JSON di checkpoint.</summary>
    public static void LoadLinks(string team, string path)
    {
        if (!System.IO.File.Exists(path)) return;
        var root = JsonNode.Parse(System.IO.File.ReadAllText(path)) as JsonObject;
        if (root is null) return;
        if (root["inner"] is JsonObject inn)
            foreach (var (agent, w) in inn)
            {
                if (JsonSerializer.Deserialize<RecursiveLink.LinkSnapshot>(w?.ToJsonString() ?? "", JsonOptions) is { } snap
                    && RecursiveLink.Import(snap) is var h && h >= 0)
                    _innerLinks[team + ":" + agent] = h;
            }
        if (root["outer"] is { } ow &&
            JsonSerializer.Deserialize<RecursiveLink.LinkSnapshot>(ow.ToJsonString(), JsonOptions) is { } osnap &&
            RecursiveLink.Import(osnap) is var oh && oh >= 0)
            _outerLinks[team] = oh;
    }

    /// <summary>JSON configurazione link del team: {inner:{hidden}, outer:{hidden}}.</summary>
    public static string GetLinkInfo(string team)
    {
        int innerHidden = Hidden, outerHidden = Hidden;
        try
        {
            var teamRecord = Basic.Agents.TeamServer.Instance.Get(team);
            if (teamRecord?.OuterLinkConfig is { } cfg && !string.IsNullOrWhiteSpace(cfg))
            {
                var node = JsonNode.Parse(cfg);
                if (node is JsonObject o && o["hidden"] is System.Text.Json.Nodes.JsonValue hv && hv.TryGetValue<int>(out var h))
                    outerHidden = h;
            }
        }
        catch { /* default */ }
        return JsonSerializer.Serialize(new { team, inner = new { hidden = innerHidden }, outer = new { hidden = outerHidden } });
    }

    public static void ResetForTests()
    {
        _states.Clear(); _innerLinks.Clear(); _outerLinks.Clear(); _runStats.Clear();
    }

    private static readonly System.Text.Json.JsonSerializerOptions JsonOptions = new() { WriteIndented = true };
}
