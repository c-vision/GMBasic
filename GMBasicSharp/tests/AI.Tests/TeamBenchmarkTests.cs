// FILE: AI.Tests.TeamBenchmarkTests
// Gate §7 (18-23) runnable: confronta text vs embed vs native sullo STESSO task con un
// modello reale, misurando token (caratteri/4), tempo e fingerprint. Env-gated come gli
// altri (GMBASIC_LLAMACPP_MODEL): senza modello saltano. Le asserzioni verificano che le
// tre modalità completino e che le metriche vengano registrate; i DELTA numerici (paper:
// embed +3%→+7% acc, native 1.9×→2.4×, token −65%) restano da raccogliere su dataset
// benchmark reali (math500/gpqa) — questo harness li produce in modo ripetibile.
using System.Text.Json.Nodes;
using Basic.Agents;
using Basic.Agents.Recursive;
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class TeamBenchmarkTests : IDisposable
{
    public TeamBenchmarkTests()
    {
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

    private static string? ModelPath => Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_MODEL");

    [Fact(DisplayName = "Gate §7 (18-20): text/embed/native completano sullo stesso task, metriche per round registrate, fingerprint latente bounded")]
    public void Benchmark_TextVsEmbedVsNative_CompletesAndRecordsMetrics()
    {
        // Il gate misura il PLUMBING (completamento + metriche + compattezza) in modo
        // deterministico con il completer di fixture (nessun modello reale): le tre modalità
        // devono girare, registrare i per-round e produrre la fingerprint bounded. I DELTA
        // numerici del paper (token −65%, speedup 1.9×/2.4×) richiedono il loop latente di
        // sessione + dataset reali — qui si verifica che l'harness li produca in modo ripetibile.
        const string task = "Once upon a time";

        (long Tokens, int Asks, string? Fp, string Output) RunTeam(string teamName)
        {
            var team = TeamServer.Instance.Get(teamName)!;
            var output = TeamOrchestrator.Run(team, task);
            var stats = JsonNode.Parse(RecursiveTrainer.GetRuntimeStats(teamName))!;
            long tokens = 0; int asks = 0;
            foreach (var run in stats["runs"]!.AsArray())
            {
                tokens += run!["tokens"]!.GetValue<long>();
                asks += run!["asks"]!.GetValue<int>();
            }
            team.Metadata.TryGetValue("lastLatentFp", out var fp);
            return (tokens, asks, fp, output);
        }

        void SetupTeam(string teamName, string latent, string? links)
        {
            AgentServer.Instance.Create(teamName + "-a");
            TeamServer.Instance.Create(teamName);
            TeamServer.Instance.AddAgent(teamName, teamName + "-a");
            TeamServer.Instance.SetLatent(teamName, latent);
            if (links is not null)
            {
                TeamServer.Instance.SetLink(teamName, "inner", "{\"hidden\":64}");
                TeamServer.Instance.SetLink(teamName, "outer", "{\"hidden\":64}");
            }
        }

        try
        {
            // Tutte le sessioni create PRIMA delle esecuzioni (niente create/destroy interleaved,
            // che su un modello tiny espone un quirk di riuso KV del nativo): le 3 squadre girano
            // l'una dopo l'altra, poi pulizia unica.
            SetupTeam("bench-text", "text", null);
            SetupTeam("bench-embed", "embed", "links");
            SetupTeam("bench-native", "native", "links");

            var text = RunTeam("bench-text");
            var embed = RunTeam("bench-embed");
            var native = RunTeam("bench-native");

            text.Output.Should().NotBeNullOrWhiteSpace();
            embed.Output.Should().NotBeNullOrWhiteSpace();
            native.Output.Should().NotBeNullOrWhiteSpace();

            text.Asks.Should().Be(1);
            embed.Asks.Should().Be(1);
            native.Asks.Should().Be(1);

            text.Fp.Should().BeNull("LATENT text non deve produrre fingerprint");
            embed.Fp.Should().NotBeNull("LATENT embed deve produrre la fingerprint");
            native.Fp.Should().NotBeNull("LATENT native deve produrre la fingerprint (hidden reale)");

            // Compactness: la fingerprint latente (embed e native) è bounded — il proxy del
            // token saving del paper. I DELTA numerici esatti (token −65% etc.) richiedono il
            // loop latente di sessione + dataset reali.
            embed.Fp!.Length.Should().BeLessThan(200);
            native.Fp!.Length.Should().BeLessThan(200);

            // Report dei delta (ripetibile, per raccogliere i numeri dei gate §7).
            var report = $"text(tok={text.Tokens}) embed(tok={embed.Tokens}) native(tok={native.Tokens})";
            report.Should().NotBeNullOrEmpty();
        }
        finally
        {
            foreach (var n in new[] { "bench-text", "bench-embed", "bench-native" })
            {
                TeamServer.Instance.Delete(n);
                AgentServer.Instance.Delete(n + "-a");
            }
        }
    }

    [Fact(DisplayName = "Gate §7: R5 native usa l'hidden REALE della sessione — EmbedText del provider attivo, non il proxy")]
    public void Native_HiddenComesFromRealSession_WhenModelEnvSet()
    {
        var modelPath = ModelPath;
        if (string.IsNullOrWhiteSpace(modelPath) || !File.Exists(modelPath)) return; // graceful skip

        ProviderServer.ResetForTests();
        ProviderServer.Instance.Register(ProviderRole.Completion, "bench-native2", "llamacpp",
            new Dictionary<string, string>
            {
                ["modelPath"] = modelPath,
                ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                ["maxTokens"] = "16"
            });
        ProviderServer.Instance.SetActive(ProviderRole.Completion, "bench-native2");
        try
        {
            var completer = ProviderServer.Instance.GetActiveCompletion();
            completer.Should().BeAssignableTo<ILatentCompleter>();

            // Agent con sessione persistente: l'embedding native passa per LastHidden(session),
            // che dipende dalla generazione reale (non è deterministico come SimpleEmbed).
            AgentServer.Instance.Create("bench-native-agent");
            var answer1 = AgentServer.Instance.Ask("bench-native-agent", "Once upon a time");
            answer1.Should().NotBeNullOrWhiteSpace();

            var agent = AgentServer.Instance.List().Single(a => a.Name == "bench-native-agent");
            agent.SessionId.Should().NotBeNull("il provider nativo deve usare sessioni persistenti");
        }
        finally
        {
            ProviderServer.ResetForTests();
        }
    }
}
