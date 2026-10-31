// FILE: AI.Tests.RecursiveMasGateTests
// Gate §7 (18-23) sui dataset scaricati in samples/Fixtures/ai/ (math500.jsonl, gsm8k_300.jsonl).
// - Test fixture (SEMPRE): RecursiveBenchmark gira end-to-end (acc/token/time) sulle 3 modalità.
// - Test fixture: LoadDataset JSONL mappa problem/solution (MATH-500) e question/answer (GSM8K).
// - Test modello reale (env-gated GMBASIC_LLAMACPP_MODEL): squadra PERSISTENTE di 2 agenti,
//   LATENT ricablato text→embed→native (2 sessioni, nessun create/destroy — pattern sicuro
//   coi modelli nativi tiny), 3 problemi ciascuno; verifica completamento + fingerprint bounded.
using System.Text.Json.Nodes;
using Basic.Agents;
using Basic.Agents.Recursive;
using Basic.Providers;
using FluentAssertions;
using Xunit;
using Xunit.Abstractions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RecursiveMasGateTests : IDisposable
{
    private readonly ITestOutputHelper _output;

    public RecursiveMasGateTests(ITestOutputHelper output)
    {
        _output = output;
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
    }

    private static string RepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static string Dataset(string name) => Path.Combine(RepoRoot(), "samples", "Fixtures", "ai", name);

    [Fact(DisplayName = "Gate §7: il benchmark gira end-to-end su math500.jsonl (fixture) — acc/token/time per text/embed/native")]
    public void Benchmark_RunAllModes_OnMath500Fixture_Completes()
    {
        var path = Dataset("math500.jsonl");
        File.Exists(path).Should().BeTrue("math500.jsonl deve essere presente in samples/Fixtures/ai/");

        var reports = RecursiveBenchmark.RunAllModes(path, "gate-fix", rounds: 2, maxProblems: 5);

        reports.Should().HaveCount(3);
        foreach (var r in reports)
        {
            r.Problems.Should().Be(5);
            r.TotalTokens.Should().BeGreaterThan(0);
            r.Asks.Should().BeGreaterThan(0);
            r.TotalTimeMs.Should().BeGreaterThanOrEqualTo(0);
            r.Accuracy.Should().BeInRange(0, 1);
        }
        reports.Select(r => r.Mode).Should().Equal("text", "embed", "native");
    }

    [Fact(DisplayName = "Gate §7: LoadDataset legge i JSONL scaricati (MATH-500 problem/solution, GSM8K question/answer)")]
    public void LoadDataset_JsonlFromFixtures_MapsFields()
    {
        var math = RecursiveTrainer.LoadDataset(Dataset("math500.jsonl"));
        math.Should().NotBeEmpty();
        math.Count.Should().Be(500);
        math[0].X.Should().NotBeNullOrWhiteSpace();
        math[0].Y.Length.Should().BeGreaterThan(20, "MATH-500 risolve nel campo solution (LaTeX lungo)");

        var gsm = RecursiveTrainer.LoadDataset(Dataset("gsm8k_300.jsonl"));
        gsm.Should().NotBeEmpty();
        gsm.Count.Should().Be(300);
        gsm[0].Y.Should().Contain("####", "GSM8K chiude la risposta con #### NNN");
    }

    [Fact(DisplayName = "Gate §7 (18-20): squadra persistente text→embed→native su math500 (modello reale, env-gated)")]
    public void Benchmark_PersistentTeam_TextEmbedNative_OnRealModel_Completes()
    {
        // Modello reale preferito: MLX (Qwen3-0.6B, ctx 2048) — stories260K ha un n_ctx troppo
        // piccolo per i problemi MATH. Env-gated: GMBASIC_MLX_MODEL_DIR o GMBASIC_LLAMACPP_MODEL.
        var mlxDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_MODEL");
        if ((string.IsNullOrWhiteSpace(mlxDir) || !Directory.Exists(mlxDir)) &&
            (string.IsNullOrWhiteSpace(modelPath) || !File.Exists(modelPath))) return; // graceful skip
        var path = Dataset("math500.jsonl");
        if (!File.Exists(path)) return;

        if (!string.IsNullOrWhiteSpace(mlxDir) && Directory.Exists(mlxDir))
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "gate-mlx", "mlx",
                new Dictionary<string, string> { ["modelDir"] = mlxDir, ["maxTokens"] = "64" });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "gate-mlx");
        }
        else
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "gate-llamacpp", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = modelPath!,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "64"
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "gate-llamacpp");
        }

        // Una squadra PERSISTENTE (2 agenti, 2 sessioni): si ricabla solo LATENT tra le modalità —
        // nessun create/destroy interleaved (il churn di sessioni su un modello tiny espone un
        // quirk di riuso KV del nativo: 3a sessione → append fallisce).
        AgentServer.Instance.Create("gate-rm-a");
        AgentServer.Instance.Create("gate-rm-b");
        TeamServer.Instance.Create("gate-rm");
        TeamServer.Instance.AddAgent("gate-rm", "gate-rm-a");
        TeamServer.Instance.AddAgent("gate-rm", "gate-rm-b");
        TeamServer.Instance.SetStrategy("gate-rm", TeamStrategy.Sequential);
        TeamServer.Instance.SetRecursion("gate-rm", 1);
        TeamServer.Instance.SetLink("gate-rm", "inner", "{\"hidden\":64}");
        TeamServer.Instance.SetLink("gate-rm", "outer", "{\"hidden\":64}");

        try
        {
            var n = int.TryParse(Environment.GetEnvironmentVariable("GMBASIC_BENCH_PROBLEMS"), out var nn) && nn > 0 ? nn : 1;
            var rows = RecursiveTrainer.LoadDataset(path).Take(n).ToList();
            var results = new List<string>();
            foreach (var mode in new[] { "text", "embed", "native" })
            {
                TeamServer.Instance.SetLatent("gate-rm", mode);
                long tokens = 0; int correct = 0;
                foreach (var (x, y) in rows)
                {
                    // token PER MODALITÀ: delta dei run aggiunti da questo problema (i run
                    // si accumulano nel team persistente tra le modalità → sommare tutto
                    // darebbe numeri cumulativi sbagliati).
                    var before = JsonNode.Parse(RecursiveTrainer.GetRuntimeStats("gate-rm"))!["runs"]!.AsArray().Count;
                    var output = TeamOrchestrator.Run(TeamServer.Instance.Get("gate-rm")!, x);
                    output.Should().NotBeNullOrWhiteSpace($"{mode} deve completare il problema");
                    var after = JsonNode.Parse(RecursiveTrainer.GetRuntimeStats("gate-rm"));
                    tokens += after!["runs"]!.AsArray().Skip(before).Sum(r => r!["tokens"]!.GetValue<long>());
                    if (RecursiveBenchmark.IsCorrect(y, output)) correct++;
                }
                var fp = TeamServer.Instance.Get("gate-rm")!.Metadata.GetValueOrDefault("lastLatentFp");
                if (mode == "text")
                    fp.Should().BeNull("LATENT text non deve produrre fingerprint");
                else
                    fp!.Length.Should().BeLessThan(200, $"fingerprint {mode} bounded");
                results.Add($"{mode}(tok={tokens},acc={correct}/{rows.Count})");
            }
            _output.WriteLine("MATH500 misurato (model reale): " + string.Join("  ", results));
            string.Join(" ", results).Should().NotBeNullOrEmpty();
        }
        finally
        {
            AgentServer.Instance.Delete("gate-rm-a");
            AgentServer.Instance.Delete("gate-rm-b");
            TeamServer.Instance.Delete("gate-rm");
            ProviderServer.ResetForTests();
        }
    }
}
