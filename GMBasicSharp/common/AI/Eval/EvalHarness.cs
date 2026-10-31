// FILE: Basic.AI.Eval.EvalHarness
// C3/M6 — Eval Harness unificato: suite di metriche (llm.tokps, rag.ndcg, team.acc,
// credit.blocked) che produce eval_report.json. Esegue contro il provider attivo
// (o fixture). Le metriche sono MISURATE e riportate, non assert — il report è
// confrontabile tra backend/sessioni.

using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
using Basic.Agents;
using Basic.AI.Rag;
using Basic.Providers;
using Basic.SQLite.Vector;

namespace Basic.AI.Eval;

public sealed record EvalMetric(string Key, double Value, string Unit);
public sealed record EvalSuiteResult(string Name, int Cases, int Passed, int Failed, List<EvalMetric> Metrics);

public static class EvalHarness
{
    /// <summary>Esegue la suite "default" (llm+rag+team+credit) e ritorna il report JSON.
    /// fixture=true → FixtureLlmCompleter (deterministico, nessuna rete); altrimenti usa
    /// ProviderServer attivo. Nome output vuoto → niente file.</summary>
    public static string RunAll(string? reportPath = null, bool fixture = false, ILlmCompleter? completer = null)
    {
        completer ??= fixture ? new FixtureLlmCompleter() : LlmCompleterResolver.Default
            ?? ProviderServer.Instance.GetActiveCompletion();
        var suites = new List<EvalSuiteResult>
        {
            LlmThroughput(completer),
            RagNdcg(),
            TeamAccuracy(completer),
            CreditBlocked(),
        };
        var totalCases = suites.Sum(s => s.Cases);
        var totalPassed = suites.Sum(s => s.Passed);
        var totalFailed = suites.Sum(s => s.Failed);
        var report = new
        {
            generatedAt = DateTime.UtcNow,
            model = completer?.ModelName ?? "(none)",
            fixture,
            total = new { cases = totalCases, passed = totalPassed, failed = totalFailed },
            suites
        };
        var json = JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
        if (!string.IsNullOrWhiteSpace(reportPath))
        {
            var dir = Path.GetDirectoryName(Path.GetFullPath(reportPath));
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(reportPath, json);
        }
        return json;
    }

    // ── llm.tokps ────────────────────────────────────────────────────────────
    public static EvalSuiteResult LlmThroughput(ILlmCompleter? completer)
    {
        completer ??= new FixtureLlmCompleter();
        var metrics = new List<EvalMetric>();
        var cases = 0; var passed = 0; var failed = 0;
        try
        {
            var sw = Stopwatch.StartNew();
            var outText = completer.Complete("Sei conciso.", "Ripeti 'ciao mondo' dieci volte.");
            sw.Stop();
            var estTokens = Math.Max(1, outText.Length / 4.0); // stima ≈1 token/4 char
            var tokps = estTokens / (sw.ElapsedMilliseconds > 0 ? sw.ElapsedMilliseconds / 1000.0 : 0.001);
            metrics.Add(new EvalMetric("llm.tokps", Math.Round(tokps, 2), "tok/s"));
            metrics.Add(new EvalMetric("llm.chars", outText.Length, "chars"));
            metrics.Add(new EvalMetric("llm.ms", sw.ElapsedMilliseconds, "ms"));
            cases = 1; passed = !string.IsNullOrWhiteSpace(outText) ? 1 : 0; failed = passed == 1 ? 0 : 1;
        }
        catch (Exception ex) { failed = 1; metrics.Add(new EvalMetric("llm.error", 1, ex.Message)); }
        return new EvalSuiteResult("llm", cases, passed, failed, metrics);
    }

    // ── rag.ndcg ────────────────────────────────────────────────────────────
    public static EvalSuiteResult RagNdcg(string dbAlias = "eval", string coll = "docs")
    {
        var metrics = new List<EvalMetric>();
        var cases = 0; var passed = 0; var failed = 0;
        try
        {
            var ragDir = Basic.AI.AiConfig.RagDefaultDir;
            if (!Directory.Exists(ragDir)) Directory.CreateDirectory(ragDir);
            RagDbSession.Open(Path.Combine(ragDir, "eval.db"), dbAlias);
            AiVectorRuntime.EnsureCollection(dbAlias, coll, EmbeddingProviderResolver.ActiveDimensions);

            // Ground truth: 4 doc con rilevanza decrescente per la query "motore MLX memoria"
            var docs = new (string id, string text)[]
            {
                ("d1", "Il motore MLX di GMBasic gestisce la memoria KV con retenzione su Apple Silicon."),
                ("d2", "La memoria KV quantizzata riduce il footprint del contesto nei modelli ricorrenti."),
                ("d3", "GMBasic compila BASIC in eseguibili nativi con i backend IL, VM/C e C99."),
                ("d4", "La grafica SDL disegna sprite e tilemap nelle finestre TUI."),
            };
            foreach (var (id, text) in docs)
                AiVectorRuntime.IndexText(dbAlias, coll, "/" + id, text);
            var query = "motore MLX memoria";
            var ideal = new[] { "d1", "d2" }; // attesi in cima
            var hitsJson = AiVectorRuntime.SearchJson(dbAlias, coll, query, 4, hybrid: true, filterJson: null);
            var retrieved = ParseSourceIds(hitsJson);
            var ndcg = NdcgAtK(retrieved, ideal, k: 2);
            metrics.Add(new EvalMetric("rag.ndcg@2", Math.Round(ndcg, 4), "ndcg"));
            metrics.Add(new EvalMetric("rag.hits", retrieved.Count, "n"));
            cases = 1; passed = ndcg > 0 ? 1 : 0; failed = passed == 1 ? 0 : 1;
        }
        catch (Exception ex) { failed = 1; metrics.Add(new EvalMetric("rag.error", 1, ex.Message)); }
        return new EvalSuiteResult("rag", cases, passed, failed, metrics);
    }

    // ── team.acc ────────────────────────────────────────────────────────────
    public static EvalSuiteResult TeamAccuracy(ILlmCompleter? completer)
    {
        completer ??= new FixtureLlmCompleter();
        var metrics = new List<EvalMetric>();
        var cases = 0; var passed = 0; var failed = 0;
        try
        {
            AgentServer.ResetForTests();
            TeamServer.ResetForTests();
            var tasks = new[]
            {
                ("rispondi con la parola esatta: FINE", "FINE"),
                ("nomina il colore del cielo", "blu"),
            };
            int correct = 0;
            foreach (var (task, expected) in tasks)
            {
                cases++;
                try
                {
                    AgentServer.Instance.Create("a" + cases);
                    AgentServer.Instance.Create("b" + cases);
                    var t = "T" + cases;
                    TeamServer.Instance.Create(t);
                    TeamServer.Instance.AddAgent(t, "a" + cases);
                    TeamServer.Instance.AddAgent(t, "b" + cases);
                    var answer = TeamOrchestrator.Run(TeamServer.Instance.Get(t)!, task);
                    if (answer.Contains(expected, StringComparison.OrdinalIgnoreCase)) { correct++; passed++; }
                    else failed++;
                    AgentServer.Instance.Delete("a" + cases);
                    AgentServer.Instance.Delete("b" + cases);
                }
                catch { failed++; }
            }
            var acc = cases > 0 ? (double)correct / cases : 0;
            metrics.Add(new EvalMetric("team.acc", Math.Round(acc, 4), "%"));
        }
        catch (Exception ex) { failed = 1; metrics.Add(new EvalMetric("team.error", 1, ex.Message)); }
        return new EvalSuiteResult("team", cases, passed, failed, metrics);
    }

    // ── credit.blocked ──────────────────────────────────────────────────────
    public static EvalSuiteResult CreditBlocked()
    {
        var metrics = new List<EvalMetric>();
        var cases = 1; var passed = 0; var failed = 0;
        try
        {
            var server = ProviderServer.Instance;
            var name = "eval-credit-" + Guid.NewGuid().ToString("N").Substring(0, 6);
            server.Register(ProviderRole.Completion, name, "fixture");
            server.SetCredits(name, 1);
            server.RecordCompletion(ProviderRole.Completion, name, tokensUsed: 1000); // costa ≥1
            var remaining = server.GetCredits(name);
            // blocked == true quando il tracker ha portato a 0 crediti
            var blocked = remaining == 0;
            metrics.Add(new EvalMetric("credit.remaining", remaining, "credits"));
            metrics.Add(new EvalMetric("credit.blocked", blocked ? 1 : 0, "bool"));
            passed = blocked ? 1 : 0; failed = blocked ? 0 : 1;
        }
        catch (Exception ex) { failed = 1; metrics.Add(new EvalMetric("credit.error", 1, ex.Message)); }
        return new EvalSuiteResult("credit", cases, passed, failed, metrics);
    }

    // ── helpers ─────────────────────────────────────────────────────────────

    private static List<string> ParseSourceIds(string hitsJson)
    {
        var list = new List<string>();
        try
        {
            using var doc = JsonDocument.Parse(hitsJson);
            if (doc.RootElement.TryGetProperty("hits", out var hits) && hits.ValueKind == JsonValueKind.Array)
                foreach (var h in hits.EnumerateArray())
                    if (h.TryGetProperty("sourceId", out var sid))
                        list.Add((sid.GetString() ?? "").Trim('/'));
        }
        catch { }
        return list;
    }

    /// <summary>NDCG@k: relevance binaria (1 se nel set ideale), DCG/IDCG.</summary>
    public static double NdcgAtK(IReadOnlyList<string> retrieved, IReadOnlyList<string> ideal, int k)
    {
        var n = Math.Min(k, retrieved.Count);
        if (n == 0) return 0;
        double dcg = 0;
        for (var i = 0; i < n; i++)
        {
            var rel = ideal.Contains(retrieved[i], StringComparer.OrdinalIgnoreCase) ? 1.0 : 0.0;
            dcg += rel / Math.Log2(i + 2);
        }
        var idealN = Math.Min(k, ideal.Count);
        double idcg = 0;
        for (var i = 0; i < idealN; i++) idcg += 1.0 / Math.Log2(i + 2);
        return idcg > 0 ? dcg / idcg : 0;
    }

    /// <summary>rag.retrieval — quartetto metriche R1 (P@K/R@K/MRR/NDCG) sul medesimo toy del
    /// rag.ndcg (todo Rag-Eval-Gap-Analysis Fase A). Le 4 metriche usano le funzioni canoniche
    /// AiVectorRuntime (stesse del linguaggio VECTORDB.*); niente duplicazione di formule.
    /// RE-APPLICATO 2026-09-14 sera: la stesura era stata sovrascritta da una sessione
    /// concorrente (incidente working tree multi-AI).</summary>
    public static EvalSuiteResult RagRetrieval(string dbAlias = "eval", string coll = "docs")
    {
        var metrics = new List<EvalMetric>();
        var cases = 0; var passed = 0; var failed = 0;
        try
        {
            var ragDir = Basic.AI.AiConfig.RagDefaultDir;
            if (!Directory.Exists(ragDir)) Directory.CreateDirectory(ragDir);
            RagDbSession.Open(Path.Combine(ragDir, "eval.db"), dbAlias);
            AiVectorRuntime.EnsureCollection(dbAlias, coll, EmbeddingProviderResolver.ActiveDimensions);

            var docs = new (string id, string text)[]
            {
                ("d1", "Il motore MLX di GMBasic gestisce la memoria KV con retenzione su Apple Silicon."),
                ("d2", "La memoria KV quantizzata riduce il footprint del contesto nei modelli ricorrenti."),
                ("d3", "GMBasic compila BASIC in eseguibili nativi con i backend IL, VM/C e C99."),
                ("d4", "La grafica SDL disegna sprite e tilemap nelle finestre TUI."),
            };
            foreach (var (id, text) in docs)
                AiVectorRuntime.IndexText(dbAlias, coll, "/" + id, text);
            var query = "motore MLX memoria";
            var ideal = new[] { "d1", "d2" };
            var hitsJson = AiVectorRuntime.SearchJson(dbAlias, coll, query, 4, hybrid: true, filterJson: null);
            var retrieved = ParseSourceIds(hitsJson);
            var p2 = AiVectorRuntime.PrecisionAt(string.Join(",", retrieved), string.Join(",", ideal), 2);
            var r2 = AiVectorRuntime.RecallAt(string.Join(",", retrieved), string.Join(",", ideal), 2);
            var mrr = AiVectorRuntime.Mrr(string.Join(",", retrieved), string.Join(",", ideal));
            var ndcg = NdcgAtK(retrieved, ideal, k: 2);
            metrics.Add(new EvalMetric("rag.p@2", Math.Round(p2, 4), "precision"));
            metrics.Add(new EvalMetric("rag.r@2", Math.Round(r2, 4), "recall"));
            metrics.Add(new EvalMetric("rag.mrr", Math.Round(mrr, 4), "mrr"));
            metrics.Add(new EvalMetric("rag.ndcg@2", Math.Round(ndcg, 4), "ndcg"));
            metrics.Add(new EvalMetric("rag.hits", retrieved.Count, "n"));
            cases = 1; passed = p2 > 0 && r2 > 0 ? 1 : 0; failed = passed == 1 ? 0 : 1;
        }
        catch (Exception ex) { failed = 1; metrics.Add(new EvalMetric("rag.error", 1, ex.Message)); }
        return new EvalSuiteResult("rag", cases, passed, failed, metrics);
    }
}
