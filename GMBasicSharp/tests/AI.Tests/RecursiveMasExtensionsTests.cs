// FILE: AI.Tests.RecursiveMasExtensionsTests
// Estensioni RecursiveMAS §9 (9.1-9.6) — meccanismi runtime testati via API (Category=Fast,
// nessun modello reale: fixture completer).
using Basic.Agents;
using Basic.Agents.Recursive;
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RecursiveMasExtensionsTests : IDisposable
{
    public RecursiveMasExtensionsTests()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
        LatentWorkingMemory.ResetForTests();
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
        TeamServer.ResetForTests();
        ProviderServer.ResetForTests();
        RecursiveTrainer.ResetForTests();
        LatentWorkingMemory.ResetForTests();
    }

    private static void TwoMemberTeam(string team)
    {
        AgentServer.Instance.Create(team + "-a");
        AgentServer.Instance.Create(team + "-b");
        TeamServer.Instance.Create(team);
        TeamServer.Instance.AddAgent(team, team + "-a");
        TeamServer.Instance.AddAgent(team, team + "-b");
    }

    [Fact(DisplayName = "9.1: LatentWorkingMemory Put/Get/TransferKv e modalità LATENT kv riusa la working memory")]
    public void LatentWorkingMemory_PutGetTransfer_Works()
    {
        LatentWorkingMemory.Put("T1", "⟨latent⟩1:0.5⟨/latent⟩");
        LatentWorkingMemory.Get("T1").Should().Be("⟨latent⟩1:0.5⟨/latent⟩");
        LatentWorkingMemory.TransferKv("T1", "T2");
        LatentWorkingMemory.Get("T2").Should().Be("⟨latent⟩1:0.5⟨/latent⟩");

        // modalità kv: il contesto del membro successivo è la working memory del team
        TwoMemberTeam("kv-team");
        TeamServer.Instance.SetLatent("kv-team", "kv");
        var team = TeamServer.Instance.Get("kv-team")!;
        TeamOrchestrator.Run(team, "task");
        LatentWorkingMemory.Get("kv-team").Should().NotBeNull("LATENT kv scrive la working memory");
        LatentWorkingMemory.Get("kv-team")!.Should().Contain("⟨latent⟩");
    }

    [Fact(DisplayName = "9.2: CoconutCurriculum — training a stadi gira e riporta una loss per stadio")]
    public void CoconutCurriculum_TrainInnerCurriculum_Runs()
    {
        AgentServer.Instance.Create("cc-a");
        TeamServer.Instance.Create("cc");
        TeamServer.Instance.AddAgent("cc", "cc-a");

        var losses = CoconutCurriculum.TrainInnerCurriculum("cc", "cc-a", n: 3, c: 2, stepsPerStage: 20);
        losses.Should().HaveCount(4, "stadi k=0..3");
        losses.All(double.IsFinite).Should().BeTrue();
        losses.All(l => l is >= 0 and <= 1).Should().BeTrue();
    }

    [Fact(DisplayName = "9.3: Quiet-STaR — AGENT SET thinking ON wrappa il prompt; TRAIN THINKING lo disabilita")]
    public void QuietStar_Thinking_WrapsPrompt_AndDistillationDisables()
    {
        AgentServer.Instance.Create("qs-agent");
        AgentServer.Instance.Set("qs-agent", "thinking", "ON");
        AgentServer.Instance.Set("qs-agent", "thoughttokens", "12");
        AgentServer.Instance.Get("qs-agent", "thinking").Should().Be("ON");
        AgentServer.Instance.Get("qs-agent", "thoughttokens").Should().Be("12");

        var answer = AgentServer.Instance.Ask("qs-agent", "hello");
        answer.Should().NotBeNullOrEmpty();
        var agent = AgentServer.Instance.List().Single(a => a.Name == "qs-agent");
        agent.History[0].Content.Should().Contain("<|start_of_thought|>", "prompt wrappato col marcatore di pensiero");

        // distillazione: TRAIN THINKING disabilita il thinking (zero overhead a inferenza)
        AgentServer.Instance.Create("qs-t");
        TeamServer.Instance.Create("qs-team");
        TeamServer.Instance.AddAgent("qs-team", "qs-t");
        RecursiveTrainer.TrainThinking("qs-team", "toy");
        AgentServer.Instance.Get("qs-t", "thinking").Should().Be("OFF");
    }

    [Fact(DisplayName = "9.4: AGoT DAG — strategia Dag decomponi→nodi→sintesi, conversazione popolata")]
    public void Dag_Strategy_Run_DecomposesAndSynthesizes()
    {
        TwoMemberTeam("dag-team");
        TeamServer.Instance.SetStrategy("dag-team", TeamStrategy.Dag);
        TeamServer.Instance.SetDag("dag-team", 1, 3, 3);

        var team = TeamServer.Instance.Get("dag-team")!;
        var result = TeamOrchestrator.Run(team, "task complesso");

        result.Should().NotBeNullOrEmpty();
        team.Conversation.Count.Should().BeGreaterThanOrEqualTo(3, "decompose + nodi + sintesi");
        TeamServer.Instance.Get("dag-team")!.DMax.Should().Be(1);
    }

    [Fact(DisplayName = "9.5: CriticEnsemble — riflessione multi-persona con sintesi del judge")]
    public void CriticEnsemble_Reflect_Synthesizes()
    {
        AgentServer.Instance.Create("ce-spec");
        AgentServer.Instance.Create("ce-edge");
        AgentServer.Instance.Create("ce-judge");

        var result = CriticEnsemble.Reflect(AgentServer.Instance,
            ["ce-spec", "ce-edge"], "ce-judge", "traiettoria fallita");

        result.Should().NotBeNullOrEmpty();
    }

    [Fact(DisplayName = "9.5: TEAM SET REFLECTION/CRITIC/PRUNE configurano il team")]
    public void TeamServer_ReflectionCriticPrune_Configures()
    {
        TwoMemberTeam("rf-team");
        TeamServer.Instance.SetReflection("rf-team", "both").Should().BeTrue();
        TeamServer.Instance.SetCritic("rf-team", "spec|edge|logic", "rf-team-a").Should().BeTrue();
        TeamServer.Instance.SetPrune("rf-team", 5).Should().BeTrue();

        var t = TeamServer.Instance.Get("rf-team")!;
        t.ReflectionMode.Should().Be("both");
        t.CriticPersonas.Should().Be("spec|edge|logic");
        t.PruneTopK.Should().Be(5);
    }

    [Fact(DisplayName = "9.6: DpsdpTrainer — actor-critic addestra su un team a 2 membri")]
    public void Dpsdp_Team_TrainsActorCritic()
    {
        TwoMemberTeam("dp-team");
        var traj = DpsdpTrainer.TrainTeam("dp-team", "Qual è la risposta?");
        traj.Should().NotBeEmpty("il training actor-critic deve produrre una traiettoria");
        traj.All(s => s.CriticScore is >= 0 and <= 10).Should().BeTrue();
    }

    [Fact(DisplayName = "9.3: DISTILL THINKING comprime i ThinkingTokens dei membri (16→8→…→0)")]
    public void QuietStar_DistillThinking_HalvesThinkingTokens()
    {
        AgentServer.Instance.Create("ds-agent");
        AgentServer.Instance.Set("ds-agent", "thinking", "ON");
        AgentServer.Instance.Set("ds-agent", "thoughttokens", "16");
        TeamServer.Instance.Create("ds-team");
        TeamServer.Instance.AddAgent("ds-team", "ds-agent");

        RecursiveTrainer.DistillThinking("ds-team", "toy");
        AgentServer.Instance.Get("ds-agent", "thoughttokens").Should().Be("8", "la distillazione dimezza i token di pensiero");
        RecursiveTrainer.DistillThinking("ds-team", "toy");
        AgentServer.Instance.Get("ds-agent", "thoughttokens").Should().Be("4");
    }

    [Fact(DisplayName = "9.5: AgentReflection STM/LTM — memoria a breve e lungo termine per agente + reset STM")]
    public void AgentReflection_StmLtm_StoresAndResets()
    {
        AgentReflection.ResetForTests();
        AgentReflection.StmAdd("a1", "turno 1");
        AgentReflection.StmAdd("a1", "turno 2");
        AgentReflection.StmGet("a1").Should().Equal("turno 1", "turno 2");

        AgentReflection.LtmAdd("a1", "traiettoria-1");
        AgentReflection.LtmGet("a1").Should().Equal("traiettoria-1");

        AgentReflection.StmReset("a1");
        AgentReflection.StmGet("a1").Should().BeEmpty("STM resettata a subtask ok");
        AgentReflection.LtmGet("a1").Should().Contain("traiettoria-1", "la LTM persiste dopo il reset STM");
    }

    [Fact(DisplayName = "9.5: ReTreVal prune — mantiene i top-k candidati")]
    public void AgentReflection_Prune_KeepsTopK()
    {
        var candidates = new[] { "breve", "risposta di media lunghezza qui", "risposta molto lunga e dettagliata con piu contenuto" };
        var kept = AgentReflection.Prune(judge: null, "a1", candidates, topK: 2);
        kept.Should().HaveCount(2);
        kept.Should().NotContain("breve", "il candidato più corto (proxy score più basso) viene potato");
    }

    [Fact(DisplayName = "9.1: KV-transfer REALE (native) quando il modello è attivo (env-gated)")]
    public void KvTransfer_Native_ExportImport_WhenModelEnvSet()
    {
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_MODEL");
        if (string.IsNullOrWhiteSpace(modelPath) || !File.Exists(modelPath)) return; // graceful skip

        var descriptor = new ProviderDescriptor(ProviderRole.Completion, "llamacpp-kv", "llamacpp",
            new Dictionary<string, string>
            {
                ["modelPath"] = modelPath,
                ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                ["maxTokens"] = "16"
            });
        var completer = ProviderFactory.CreateCompletion(descriptor);
        if (completer is not ILatentCompleter latent) { (completer as IDisposable)?.Dispose(); return; }
        using var disposable = completer as IDisposable;

        var src = latent.CreateSession();
        var dst = latent.CreateSession();
        try
        {
            latent.AppendMessage(src, "user", "Once upon a time");
            var reply = latent.GenerateFromSession(src, 8);
            reply.Should().NotBeNullOrWhiteSpace();

            // fixture/non-KV completer → graceful false
            LatentWorkingMemory.TransferKv(latent, src, dst).Should().BeTrue("il completer llama.cpp implementa IKvTransfer");
        }
        finally
        {
            latent.DestroySession(src);
            latent.DestroySession(dst);
        }
    }

    [Fact(DisplayName = "HF marketplace: token/repo configurabili via AiConfig (AI SET HFTOKEN/HFREPO)")]
    public void Marketplace_Config_TokenAndRepo_SettableViaAiConfig()
    {
        var oldToken = Basic.AI.AiConfig.HfToken;
        var oldRepo = Basic.AI.AiConfig.HfRepo;
        try
        {
            Basic.AI.AiConfig.HfToken = "hf_config_test_token";
            Basic.AI.AiConfig.HfRepo = "test-org/test-repo";
            RecursiveMasMarketplace.ResolveToken().Should().Be("hf_config_test_token");
            RecursiveMasMarketplace.DefaultRepo().Should().Be("test-org/test-repo");
        }
        finally
        {
            Basic.AI.AiConfig.HfToken = oldToken;
            Basic.AI.AiConfig.HfRepo = oldRepo;
        }
    }

    [Fact(DisplayName = "HF marketplace: push/pull round-trip (token-gated, repo recursivemas-test)")]
    public void Marketplace_PushPull_RoundTrip_WhenTokenAvailable()
    {
        var token = RecursiveMasMarketplace.ResolveToken();
        if (token is null) return; // graceful skip (credenziali assenti)

        // ConfigDir isolato: Push/Pull leggono/scrivono GmbiPaths.RecursiveMasDir (locale) —
        // MAI la config/ reale e versionata del repo (bug osservato 2026-09-07: un token reale
        // disponibile in ambiente faceva scattare questo round-trip di rete VERO, che scriveva
        // in config/recursivemas/hfteam/ reale). Il lato di rete resta reale apposta (il nome del
        // test lo dichiara: "WhenTokenAvailable" — round-trip opportunistico contro HF).
        var tempConfig = Path.Combine(Path.GetTempPath(), $"gmb-config-{Guid.NewGuid():N}");
        CommonLib.GmbiPaths.OverrideConfigDirForTests(tempConfig);
        try
        {
            // Sanitize rimuove i non alfanumerici → "hf-team" → "hfteam"
            var safe = new string("hf-team".Where(char.IsLetterOrDigit).ToArray());
            var dir = Path.Combine(CommonLib.GmbiPaths.RecursiveMasDir, safe);
            if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);

            TwoMemberTeam("hf-team");
            TeamServer.Instance.Save("hf-team"); // scrive <ConfigDir isolato>/recursivemas/hfteam/

            RecursiveMasMarketplace.Push("hf-team", "recursivemas-test", token).Should().BeTrue("push su HF");

            if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true); // rimuovi locale per verificare il pull
            RecursiveMasMarketplace.Pull("hf-team", "recursivemas-test", token).Should().BeTrue("pull da HF");
            File.Exists(Path.Combine(dir, safe + ".team.json")).Should().BeTrue("checkpoint ripristinato dal pull");
        }
        finally
        {
            CommonLib.GmbiPaths.ResetConfigDirOverrideForTests();
            try { Directory.Delete(tempConfig, recursive: true); } catch { /* best effort */ }
        }
    }
}
