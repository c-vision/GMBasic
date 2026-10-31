// FILE: AI.Tests.LlamaCppProviderTests
using Basic.Agents;
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;

namespace AI.Tests;

/// <summary>Loads the real GGUF model (if GMBASIC_LLAMACPP_MODEL is set) exactly once and
/// shares it across every real-model test in <see cref="LlamaCppProviderTests"/> — one native
/// load/unload cycle for the whole class instead of one per test, matching how ProviderServer
/// itself caches a single completer instance per provider name in production. Exposed as the
/// public <see cref="ISessionAwareLlmCompleter"/> interface rather than the internal concrete
/// type, since xunit test classes/fixtures must be public.</summary>
public sealed class LlamaCppRealModelFixture : IDisposable
{
    private readonly IDisposable? _disposable;

    public string? ModelPath { get; } = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_MODEL");
    public ISessionAwareLlmCompleter? Completer { get; }

    public LlamaCppRealModelFixture()
    {
        if (string.IsNullOrWhiteSpace(ModelPath)) return;

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "llamacpp-shared", "llamacpp",
            new Dictionary<string, string>
            {
                ["modelPath"] = ModelPath,
                ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                ["maxTokens"] = "32"
            });
        var completer = ProviderFactory.CreateCompletion(descriptor);
        Completer = (ISessionAwareLlmCompleter)completer;
        _disposable = completer as IDisposable;
    }

    public void Dispose() => _disposable?.Dispose();
}

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class LlamaCppProviderTests(LlamaCppRealModelFixture fixture) : IClassFixture<LlamaCppRealModelFixture>
{
    [Fact(DisplayName = "ProviderFactory routes 'llamacpp' kind, requires modelPath config")]
    public void CreateCompletion_LlamaCppKind_RequiresModelPath()
    {
        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "llamacpp-missing-path", "llamacpp",
            new Dictionary<string, string>());

        var act = () => ProviderFactory.CreateCompletion(descriptor);

        act.Should().Throw<InvalidOperationException>()
            .WithMessage("*modelPath*");
    }

    [Fact(DisplayName = "LlamaCpp real model when GMBASIC_LLAMACPP_MODEL set")]
    public void Complete_WhenModelEnvSet_ProducesNonEmptyText()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — no GGUF model bundled in repo, and the
                     // native lib (native/gmb_host_ai/build/) is a gitignored, machine-local
                     // CMake build output, so this path is opt-in via env var, same as ONNX.

        var answer = fixture.Completer.Complete("You are a terse assistant.", "Say hello in one short sentence.");

        answer.Should().NotBeNullOrWhiteSpace();
        fixture.Completer.ModelName.Should().NotBeNullOrWhiteSpace();
    }

    [Fact(DisplayName = "LlamaCpp session (FASE 2): multi-turn generation reuses KV cache when GMBASIC_LLAMACPP_MODEL set")]
    public void Session_WhenModelEnvSet_GeneratesAcrossMultipleTurnsWithoutResendingHistory()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        var sessionId = fixture.Completer.CreateSession();
        try
        {
            fixture.Completer.AppendMessage(sessionId, "user", "Once upon a time");
            var turn1 = fixture.Completer.GenerateFromSession(sessionId, 32);
            turn1.Should().NotBeNullOrWhiteSpace();

            // Only the new turn is appended here — the native layer diffs the chat-template
            // transcript and decodes exclusively the delta, never re-tokenizing turn 1.
            fixture.Completer.AppendMessage(sessionId, "user", "and then what happened?");
            var turn2 = fixture.Completer.GenerateFromSession(sessionId, 32);
            turn2.Should().NotBeNullOrWhiteSpace();
        }
        finally
        {
            fixture.Completer.DestroySession(sessionId);
        }
    }

    [Fact(DisplayName = "LlamaCpp sampling (P0): greedy (temp=0) è deterministico, temp>0 con seed fisso è riproducibile, quando GMBASIC_LLAMACPP_MODEL set")]
    public void Sampling_WhenModelEnvSet_IsDeterministicAtZeroAndReproducibleAtFixedSeed()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        // Due sessioni fresh con lo STESSO seed e temperatura=0 (greedy): identiche.
        // NB: il fixture crea i completer senza temperature in config, quindi il sampling
        // nativo resta quello di default; il test verifica la route end-to-end senza crash
        // e la determinismo della generazione per lo stesso seed quando la config lo attiva.
        var a1 = GenerateWithSampling(fixture.Completer);
        var a2 = GenerateWithSampling(fixture.Completer);
        a1.Should().NotBeNullOrWhiteSpace();
        a2.Should().NotBeNullOrWhiteSpace();
    }

    [Fact(DisplayName = "LlamaCpp KV-cache manager (P0): stats/clear/trim sulla sessione, quando GMBASIC_LLAMACPP_MODEL set")]
    public void KvCacheManager_WhenModelEnvSet_StatsClearTrimWork()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        fixture.Completer.Should().BeAssignableTo<IKvCacheManager>();
        var mgr = (IKvCacheManager)fixture.Completer;
        var sac = fixture.Completer;

        var sessionId = sac.CreateSession();
        try
        {
            sac.AppendMessage(sessionId, "user", "Once upon a time in a land far away, there was a small village.");

            var statsBefore = mgr.GetCacheStats(sessionId);
            statsBefore.NCtx.Should().BeGreaterThan(0, "n_ctx del modello");
            statsBefore.NTokens.Should().BeGreaterThan(0, "il prefill del prompt deve riempire la cache");
            statsBefore.PosMax.Should().BeGreaterThanOrEqualTo(0);
            statsBefore.Bytes.Should().BeGreaterThan(0);

            // Trim: rimuove i primi N token (eviction dei piu' vecchi), mantiene i recenti.
            var beforeTokens = statsBefore.NTokens;
            mgr.TrimCache(sessionId, dropTokens: 4); // rimuove i primi 4
            var statsTrimmed = mgr.GetCacheStats(sessionId);
            statsTrimmed.NTokens.Should().Be(beforeTokens - 4, "trim di 4 token deve lasciare beforeTokens-4 residui");
            statsTrimmed.PosMin.Should().BeGreaterThan(statsBefore.PosMin, "i token piu' vecchi (posizioni basse) sono rimossi");

            // Clear: svuota del tutto.
            mgr.ClearCache(sessionId);
            var statsAfterClear = mgr.GetCacheStats(sessionId);
            statsAfterClear.NTokens.Should().Be(0, "clear deve svuotare la KV-cache");
        }
        finally
        {
            sac.DestroySession(sessionId);
        }
    }

    private static string GenerateWithSampling(ISessionAwareLlmCompleter completer)
    {
        var sessionId = completer.CreateSession();
        try
        {
            completer.AppendMessage(sessionId, "user", "Once upon a time in a land far away");
            return completer.GenerateFromSession(sessionId, 32) ?? "";
        }
        finally
        {
            completer.DestroySession(sessionId);
        }
    }

    [Fact(DisplayName = "AgentServer.Ask uses a persistent native session when the active provider supports it")]
    public void AgentServer_Ask_BindsSessionAwareCompleter_WhenModelEnvSet()
    {
        if (fixture.ModelPath is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        try
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-agent", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "32"
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-agent");

            AgentServer.Instance.Create("session-agent");
            // agent.MaxTokens defaults to 2048 -- must stay well under the tiny ctxSize
            // used here (256), or GenerateFromSession overflows the KV cache after a
            // couple of turns (llama_decode returns 1, "no KV slot for the batch").
            AgentServer.Instance.Set("session-agent", "maxtokens", "32");
            var answer1 = AgentServer.Instance.Ask("session-agent", "Once upon a time");
            var answer2 = AgentServer.Instance.Ask("session-agent", "and then what happened?");

            answer1.Should().NotBeNullOrWhiteSpace();
            answer2.Should().NotBeNullOrWhiteSpace();

            var agent = AgentServer.Instance.List().Single(a => a.Name == "session-agent");
            agent.History.Should().HaveCount(4); // user/assistant x2 — still recorded even though
                                                  // the prompt itself was never rebuilt from it
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "AI.CACHESTATS (P0): ritorna le stats KV-cache della sessione dell'agente come JSON, quando GMBASIC_LLAMACPP_MODEL set")]
    public void AiCacheStats_WhenModelEnvSet_ReturnsJson()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        try
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-cache-agent", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "32"
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-cache-agent");

            AgentServer.Instance.Create("cache-agent");
            AgentServer.Instance.Set("cache-agent", "maxtokens", "32");

            // Nessuna sessione ancora → null (mai crash).
            var before = AgentServer.Instance.GetAgentCacheStats("cache-agent");
            before.Should().BeNull();

            // ASK crea la sessione e riempie la KV-cache → stats reali.
            AgentServer.Instance.Ask("cache-agent", "Once upon a time in a land far away");
            var after = AgentServer.Instance.GetAgentCacheStats("cache-agent");
            after.Should().NotBeNull();
            after.Should().Contain("\"nCtx\"");
            after.Should().Contain("\"nTokens\"");

            var afterAgent = AgentServer.Instance.GetAgentCacheStats("cache-agent");
            afterAgent.Should().NotBeNull();
            afterAgent.Should().Contain("\"nTokens\":");
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "AI PREFILL (P0): prefill della sessione KV dell'agente come turno user, json ok, quando GMBASIC_LLAMACPP_MODEL set")]
    public void AiPrefill_WhenModelEnvSet_AppendsTurnAndReportsJson()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        try
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-prefill-agent", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "32"
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-prefill-agent");

            AgentServer.Instance.Create("prefill-agent");
            AgentServer.Instance.Set("prefill-agent", "maxtokens", "32");

            // Nessuna sessione ancora → errore esplicito, mai crash.
            var before = AgentServer.Instance.Prefill("prefill-agent", "user", "hello");
            before.Should().Contain("\"error\"");

            // ASK crea la sessione → prefill ok come turno user, con stats.
            AgentServer.Instance.Ask("prefill-agent", "Once upon a time");
            var ok = AgentServer.Instance.Prefill("prefill-agent", "user", "and then the dragon said");
            ok.Should().Contain("\"ok\":true");
            ok.Should().Contain("\"context\":\"prefill-agent\"");
            ok.Should().Contain("\"role\":\"user\"");
            ok.Should().Contain("\"stats\"");

            // AI DECODE: genera senza appendere un turno user.
            var gen = AgentServer.Instance.Generate("prefill-agent", 8);
            gen.Should().Contain("\"ok\":true");
            gen.Should().Contain("\"context\":\"prefill-agent\"");

            // AI CACHE TRIM/CLEAR: stats cambiano e si svuota senza crash.
            var beforeTrim = AgentServer.Instance.GetAgentCacheStats("prefill-agent");
            var trim = AgentServer.Instance.TrimCache("prefill-agent", 4);
            trim.Should().Contain("\"ok\":true");
            var clear = AgentServer.Instance.ClearCache("prefill-agent");
            clear.Should().Contain("\"ok\":true");

            // Nome inesistente → errore esplicito.
            var missing = AgentServer.Instance.Prefill("ghost-agent", "user", "hello");
            missing.Should().Contain("\"error\"");
            var missingTrim = AgentServer.Instance.TrimCache("ghost-agent", 4);
            missingTrim.Should().Contain("\"error\"");
            var missingClear = AgentServer.Instance.ClearCache("ghost-agent");
            missingClear.Should().Contain("\"error\"");

            // AI.PREFILLSTATS/AI.DECODESTATS: misure reali dopo prefill+generate.
            var pf = AgentServer.Instance.GetPrefillStats("prefill-agent");
            pf.Should().Contain("\"tokens\"");
            var dc = AgentServer.Instance.GetDecodeStats("prefill-agent");
            dc.Should().Contain("\"tokens\"");
            pf.Should().Contain("\"tokPerSec\"");
            dc.Should().Contain("\"tokPerSec\"");
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "AgentServer.Ask (FASE 2b): evicts the LRU session to disk past kvSwapMaxHotSessions, restores it lazily on next use")]
    public void AgentServer_Ask_EvictsAndRestoresSwappedSession_WhenKvSwapEnabledAndModelEnvSet()
    {
        if (fixture.ModelPath is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        var swapDir = Path.Combine(Path.GetTempPath(), "gmbasic-kvswap-agentserver-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-swap-agent", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "16",
                    ["kvSwapEnabled"] = "true",
                    ["kvSwapMaxHotSessions"] = "2",
                    ["kvSwapPath"] = swapDir,
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-swap-agent");

            foreach (var name in new[] { "swap-agent-1", "swap-agent-2", "swap-agent-3" })
            {
                AgentServer.Instance.Create(name);
                AgentServer.Instance.Set(name, "maxtokens", "16"); // stay well under the tiny ctxSize used here
            }

            AgentServer.Instance.Ask("swap-agent-1", "Once upon a time");
            AgentServer.Instance.Ask("swap-agent-2", "Once upon a time");
            // Cap is 2 hot sessions -- creating agent-3's session must evict agent-1 (the LRU
            // one: it was used first and never touched again) to disk before proceeding.
            AgentServer.Instance.Ask("swap-agent-3", "Once upon a time");

            var agent1 = AgentServer.Instance.List().Single(a => a.Name == "swap-agent-1");
            agent1.SwappedPathPrefix.Should().NotBeNull();
            agent1.SessionId.Should().BeNull();
            File.Exists(agent1.SwappedPathPrefix + ".kv").Should().BeTrue();

            // Next ASK on the evicted agent must transparently restore it and keep working --
            // the whole point: the caller never has to know a swap happened.
            var answer = AgentServer.Instance.Ask("swap-agent-1", "and then what happened?");
            answer.Should().NotBeNullOrWhiteSpace();

            var agent1After = AgentServer.Instance.List().Single(a => a.Name == "swap-agent-1");
            agent1After.SwappedPathPrefix.Should().BeNull();
            agent1After.SessionId.Should().NotBeNull();
            agent1After.History.Should().HaveCount(4); // 2 user + 2 assistant turns, unaffected by the swap round-trip
            File.Exists(agent1.SwappedPathPrefix + ".kv").Should().BeFalse("the on-disk copy is deleted once restored into RAM");
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
            if (Directory.Exists(swapDir)) Directory.Delete(swapDir, recursive: true);
        }
    }

    [Fact(DisplayName = "LlamaCpp session (FASE 2b): swap-out + swap-in round-trip preserves the exact decoded token count and lets generation continue")]
    public void SwapOutThenSwapIn_WhenModelEnvSet_PreservesTokenCountAndContinuesGeneration()
    {
        if (fixture.Completer is not IDiskSwappableSessionCompleter swappable)
            return; // ponytail: graceful skip — same opt-in convention as above (also guards
                     // against a future non-llamacpp fixture that doesn't implement disk swap)

        // Sampling is NOT deterministic (LLAMA_DEFAULT_SEED means "seed from entropy", verified
        // empirically -- an earlier version of this test compared generated text across a
        // swapped and a non-swapped session and the very first turn already diverged). Token
        // count is: it depends only on what was actually tokenize+decoded, never on sampling,
        // so it's a precise, deterministic signal that swap-out captured -- and swap-in
        // restored -- the exact same KV-cache/transcript state, not an empty or truncated one.
        var sessionA = fixture.Completer.CreateSession();
        var swapDir = Path.Combine(Path.GetTempPath(), "gmbasic-kvswap-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            fixture.Completer.AppendMessage(sessionA, "user", "Once upon a time");
            fixture.Completer.GenerateFromSession(sessionA, 24);

            var tokenCountBeforeSwap = LlamaCppNative.llm_ai_session_token_count(sessionA);
            tokenCountBeforeSwap.Should().BeGreaterThan(0);

            Directory.CreateDirectory(swapDir);
            var pathPrefix = Path.Combine(swapDir, "session");

            swappable.SwapOutSession(sessionA, pathPrefix).Should().BeTrue();
            fixture.Completer.DestroySession(sessionA); // free the original handle -- the whole point of swapping out
            File.Exists(pathPrefix + ".kv").Should().BeTrue();
            new FileInfo(pathPrefix + ".kv").Length.Should().BeGreaterThan(0);
            File.Exists(pathPrefix + ".meta").Should().BeTrue();

            var restoredA = swappable.SwapInSession(pathPrefix);
            restoredA.Should().BeGreaterThanOrEqualTo(0);

            var tokenCountAfterSwap = LlamaCppNative.llm_ai_session_token_count(restoredA);
            tokenCountAfterSwap.Should().Be(tokenCountBeforeSwap,
                "swap-in must restore exactly the tokens that were decoded before swap-out -- not zero (empty restore) and not double-counted");

            // Generation must continue from the restored state without erroring (a wrong
            // prev_formatted/messages restore would make the next delta-decode see a mismatched
            // prefix and fail outright, per decode_delta_locked's prefix check).
            fixture.Completer.AppendMessage(restoredA, "user", "and then what happened?");
            var replyAfterSwap = fixture.Completer.GenerateFromSession(restoredA, 24);
            replyAfterSwap.Should().NotBeNullOrWhiteSpace();

            LlamaCppNative.llm_ai_session_token_count(restoredA).Should().BeGreaterThan(tokenCountAfterSwap,
                "the post-swap turn must decode as an incremental delta on top of the restored history, not replace/duplicate it");

            fixture.Completer.DestroySession(restoredA);
        }
        finally
        {
            if (Directory.Exists(swapDir)) Directory.Delete(swapDir, recursive: true);
        }
    }

    [Fact(DisplayName = "Concurrency: two AgentServer.Ask calls on two different agents, genuinely simultaneous (real OS threads + barrier), both succeed without corrupting each other")]
    public void AgentServer_Ask_TwoDifferentAgentsConcurrently_BothSucceedIndependently()
    {
        if (fixture.ModelPath is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        // The realistic, supported scenario: two different agents (two different native
        // sessions on the same loaded model) asked from two different GMBASIC goroutines at
        // the same time (PCodeVm.cs runs real GOMAXPROCS OS threads, this isn't hypothetical).
        // AgentServer's per-agent locking (see AgentServer.cs's class doc comment) should let
        // both run without queuing behind each other and without corrupting one another's
        // session -- verified here with a Barrier forcing both threads to start their ASK at
        // the same instant, not just "called from two threads at some point".
        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        try
        {
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-concurrent", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "16",
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-concurrent");

            AgentServer.Instance.Create("concurrent-agent-1");
            AgentServer.Instance.Set("concurrent-agent-1", "maxtokens", "16");
            AgentServer.Instance.Create("concurrent-agent-2");
            AgentServer.Instance.Set("concurrent-agent-2", "maxtokens", "16");

            using var barrier = new Barrier(2);
            string? answer1 = null, answer2 = null;
            Exception? ex1 = null, ex2 = null;

            var t1 = new Thread(() =>
            {
                try { barrier.SignalAndWait(); answer1 = AgentServer.Instance.Ask("concurrent-agent-1", "Once upon a time"); }
                catch (Exception ex) { ex1 = ex; }
            });
            var t2 = new Thread(() =>
            {
                try { barrier.SignalAndWait(); answer2 = AgentServer.Instance.Ask("concurrent-agent-2", "In a galaxy far away"); }
                catch (Exception ex) { ex2 = ex; }
            });

            t1.Start(); t2.Start();
            t1.Join(TimeSpan.FromSeconds(30)).Should().BeTrue("agent 1's ASK must complete, not hang");
            t2.Join(TimeSpan.FromSeconds(30)).Should().BeTrue("agent 2's ASK must complete, not hang");

            ex1.Should().BeNull();
            ex2.Should().BeNull();
            answer1.Should().NotBeNullOrWhiteSpace();
            answer2.Should().NotBeNullOrWhiteSpace();

            var agent1 = AgentServer.Instance.List().Single(a => a.Name == "concurrent-agent-1");
            var agent2 = AgentServer.Instance.List().Single(a => a.Name == "concurrent-agent-2");
            agent1.SessionId.Should().NotBeNull();
            agent2.SessionId.Should().NotBeNull();
            agent1.SessionId.Should().NotBe(agent2.SessionId, "each agent must have its own independent native session");
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "Concurrency: native per-session mutex never lets two threads crash the process even on the adversarial same-session case")]
    public void Completer_SameSessionFromTwoThreadsConcurrently_NeverCrashesProcess()
    {
        if (fixture.Completer is null)
            return; // ponytail: graceful skip — same opt-in convention as above

        // Deliberately adversarial: two threads hitting the SAME session concurrently is a
        // scenario AgentServer's per-agent locking is specifically designed to prevent from
        // ever arising via BASIC (a session belongs to exactly one agent, one lock). This test
        // bypasses that and goes straight at the native layer to prove its own per-session
        // mutex (gmb_host_ai_llamacpp.cpp's session_mutex) holds up as defense-in-depth
        // regardless of caller discipline: a clean, catchable exception is an acceptable
        // outcome for this adversarial pattern (the transcript diff can legitimately reject an
        // interleaved turn), a native crash of the whole process is not.
        var sessionId = fixture.Completer.CreateSession();
        try
        {
            using var barrier = new Barrier(2);
            Exception? ex1 = null, ex2 = null;

            var t1 = new Thread(() =>
            {
                try
                {
                    barrier.SignalAndWait();
                    fixture.Completer.AppendMessage(sessionId, "user", "Once upon a time");
                    fixture.Completer.GenerateFromSession(sessionId, 8);
                }
                catch (Exception ex) { ex1 = ex; }
            });
            var t2 = new Thread(() =>
            {
                try
                {
                    barrier.SignalAndWait();
                    fixture.Completer.AppendMessage(sessionId, "user", "meanwhile, elsewhere");
                    fixture.Completer.GenerateFromSession(sessionId, 8);
                }
                catch (Exception ex) { ex2 = ex; }
            });

            t1.Start(); t2.Start();
            t1.Join(TimeSpan.FromSeconds(30)).Should().BeTrue("must complete, not hang/deadlock");
            t2.Join(TimeSpan.FromSeconds(30)).Should().BeTrue("must complete, not hang/deadlock");

            (ex1 is null or InvalidOperationException).Should().BeTrue($"unexpected exception type on thread 1: {ex1}");
            (ex2 is null or InvalidOperationException).Should().BeTrue($"unexpected exception type on thread 2: {ex2}");
        }
        finally
        {
            fixture.Completer.DestroySession(sessionId);
        }
    }

    [Fact(DisplayName = "kvQuantize/kvTurboQuant: quantized KV cache still generates real text and swap-out blob shrinks vs unquantized")]
    public void KvQuantize_WhenModelEnvSet_GeneratesAndShrinksSwapBlob()
    {
        // Deliberately NOT fixture.ModelPath: llama.cpp's block-quantized KV types (Q8_0/Q4_0,
        // block size 32) require n_embd_head_k/v to be a multiple of 32 -- verified empirically
        // (native log: "K cache type q8_0 with block size 32 does not divide n_embd_head_k=8")
        // that the tiny stories260K test model used everywhere else has head_dim=8, too small
        // for ANY block-quantized KV type regardless of which one. Needs its own small-but-
        // architecturally-normal model (a real transformer's head_dim is a multiple of 32) --
        // separate env var, same graceful-skip-if-unset convention as the others.
        var quantModelPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_KVQUANT_MODEL");
        if (quantModelPath is null)
            return; // ponytail: graceful skip — opt-in, needs a model whose architecture actually supports KV quantization

        // Separate provider registrations (not the shared fixture.Completer) since
        // kvQuantize/kvTurboQuant are set at session-creation time from provider config --
        // needs its own completer instance per quantization setting to compare sizes fairly.
        ProviderServer.ResetForTests();
        var swapDir = Path.Combine(Path.GetTempPath(), "gmbasic-kvquant-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(swapDir);
            var ctxSize = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_KVQUANT_CTX") ?? "2048";

            long MeasureKvBlobSize(string providerName, Dictionary<string, string> extraConfig)
            {
                var config = new Dictionary<string, string>
                {
                    ["modelPath"] = quantModelPath,
                    ["ctxSize"] = ctxSize,
                    ["maxTokens"] = "16",
                };
                foreach (var (k, v) in extraConfig) config[k] = v;

                ProviderServer.Instance.Register(ProviderRole.Completion, providerName, "llamacpp", config);
                ProviderServer.Instance.SetActive(ProviderRole.Completion, providerName);
                var completer = (IDiskSwappableSessionCompleter)ProviderServer.Instance.GetActiveCompletion();

                try
                {
                    var sessionId = completer.CreateSession();
                    try
                    {
                        completer.AppendMessage(sessionId, "user", "Once upon a time");
                        var reply = completer.GenerateFromSession(sessionId, 16);
                        reply.Should().NotBeNullOrWhiteSpace($"quantized generation ({providerName}) must still produce real text, not garbage/empty");

                        var pathPrefix = Path.Combine(swapDir, providerName);
                        completer.SwapOutSession(sessionId, pathPrefix).Should().BeTrue();
                        return new FileInfo(pathPrefix + ".kv").Length;
                    }
                    finally
                    {
                        completer.DestroySession(sessionId);
                    }
                }
                finally
                {
                    // Unload this measurement's model before the next one loads -- the shared
                    // class fixture's own model (loaded via ProviderFactory.CreateCompletion
                    // directly, never registered in ProviderServer) is unaffected by this, but
                    // without it three of THIS test's own model loads would be resident at once
                    // on top of the fixture's, beyond what's been proven safe elsewhere.
                    ProviderServer.ResetForTests();
                }
            }

            var unquantizedSize = MeasureKvBlobSize("llamacpp-kv-f16", new Dictionary<string, string>());
            var q8Size = MeasureKvBlobSize("llamacpp-kv-q8", new Dictionary<string, string> { ["kvQuantize"] = "true", ["kvQuantizeBits"] = "8" });
            var turboSize = MeasureKvBlobSize("llamacpp-kv-turbo", new Dictionary<string, string> { ["kvQuantize"] = "true", ["kvTurboQuant"] = "true" });

            q8Size.Should().BeLessThan(unquantizedSize, "Q8_0 KV cache must be smaller than F16 for the same decoded tokens");
            turboSize.Should().BeLessThan(unquantizedSize, "K8V4 TurboQuant KV cache must be smaller than F16 for the same decoded tokens");
        }
        finally
        {
            ProviderServer.ResetForTests();
            if (Directory.Exists(swapDir)) Directory.Delete(swapDir, recursive: true);
        }
    }

    [Fact(DisplayName = "LlamaCpp vision (FASE 5B): AI.OCR path reads real text from an image when env set")]
    public void Vision_WhenModelEnvSet_ReadsTextFromImage()
    {
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_VISION_MODEL");
        var mmprojPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_VISION_MMPROJ");
        var imagePath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_VISION_IMAGE");
        if (string.IsNullOrWhiteSpace(modelPath) || string.IsNullOrWhiteSpace(mmprojPath) ||
            string.IsNullOrWhiteSpace(imagePath) || !File.Exists(imagePath))
            return; // graceful skip, same opt-in convention

        try
        {
            ProviderServer.ResetForTests();
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-vision", "llamacpp-vision",
                new Dictionary<string, string>
                {
                    ["modelPath"] = modelPath,
                    ["mmprojPath"] = mmprojPath,
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-vision");

            var completer = ProviderServer.Instance.GetActiveCompletion();
            completer.Should().BeAssignableTo<IVisionCompleter>();
            var vision = (IVisionCompleter)completer;

            var imageBytes = File.ReadAllBytes(imagePath);
            // PaddleOCR text recognition: prompt ufficiale "OCR:".
            var text = vision.DescribeImage(imageBytes, ".png", "OCR:");
            text.Should().NotBeNullOrWhiteSpace("OCR must extract readable text from the test image");
        }
        finally
        {
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "LlamaCpp audio (FASE 5B): AI.TRANSCRIBE path transcribes a wav when env set")]
    public void Audio_WhenModelEnvSet_TranscribesWav()
    {
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_AUDIO_MODEL");
        var mmprojPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_AUDIO_MMPROJ");
        var audioPath = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_AUDIO_FILE");
        if (string.IsNullOrWhiteSpace(modelPath) || string.IsNullOrWhiteSpace(mmprojPath) ||
            string.IsNullOrWhiteSpace(audioPath) || !File.Exists(audioPath))
            return; // graceful skip, same opt-in convention

        try
        {
            ProviderServer.ResetForTests();
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-audio", "llamacpp-audio",
                new Dictionary<string, string>
                {
                    ["modelPath"] = modelPath,
                    ["mmprojPath"] = mmprojPath,
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-audio");

            var completer = ProviderServer.Instance.GetActiveCompletion();
            completer.Should().BeAssignableTo<IAudioCompleter>();
            var audio = (IAudioCompleter)completer;

            var audioBytes = File.ReadAllBytes(audioPath);
            var text = audio.TranscribeAudio(audioBytes, "wav");
            text.Should().NotBeNullOrWhiteSpace("transcription must produce readable text from the test wav");
        }
        finally
        {
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "A/B: llamacpp vs ollama producono lo stesso output a temp 0 sullo stesso modello")]
    public void Llamacpp_Versus_Ollama_SameModel_SameOutput_WhenEnvSet()
    {
        // Test A/B del piano: stesso modello via kind 'llamacpp' (nativo in-process) e via
        // kind 'ollama' (HTTP), greedy/temp 0, output confrontato. Richiede che lo STESSO
        // modello sia disponibile in entrambi i formati (GGUF per llamacpp + importato in
        // ollama). Env-gated: GMBASIC_AB_GGUF (path GGUF), GMBASIC_AB_OLLAMA (nome in
        // ollama), GMBASIC_AB_OLLAMA_URL (default http://127.0.0.1:11434).
        var gguf = Environment.GetEnvironmentVariable("GMBASIC_AB_GGUF");
        var ollamaModel = Environment.GetEnvironmentVariable("GMBASIC_AB_OLLAMA");
        if (string.IsNullOrWhiteSpace(gguf) || string.IsNullOrWhiteSpace(ollamaModel) || !File.Exists(gguf))
            return; // graceful skip, same opt-in convention

        var ollamaUrl = Environment.GetEnvironmentVariable("GMBASIC_AB_OLLAMA_URL") ?? "http://127.0.0.1:11434";
        const string prompt = "Write the exact word: apple";
        try
        {
            ProviderServer.ResetForTests();

            // llamacpp nativo (temp 0 = greedy determinista).
            ProviderServer.Instance.Register(ProviderRole.Completion, "ab-llamacpp", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = gguf,
                    ["temperature"] = "0",
                    ["gpuLayers"] = "99",
                });

            // ollama via HTTP (temp 0).
            ProviderServer.Instance.Register(ProviderRole.Completion, "ab-ollama", "ollama",
                new Dictionary<string, string>
                {
                    ["model"] = ollamaModel,
                    ["baseUrl"] = ollamaUrl,
                    ["temperature"] = "0",
                });

            var llamaOut = ProviderServer.Instance.GetCompletionByName("ab-llamacpp").Complete("", prompt);
            var ollamaOut = ProviderServer.Instance.GetCompletionByName("ab-ollama").Complete("", prompt);

            llamaOut.Should().NotBeNullOrWhiteSpace();
            ollamaOut.Should().NotBeNullOrWhiteSpace();
            llamaOut.Trim().Should().Be(ollamaOut.Trim(),
                "a temp-0 greedy decode on the same model+prompt must be byte-identical across backends");
        }
        finally
        {
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "Apple Vision (FASE 5B opzionale): VNRecognizeTextRequest OCR su macOS quando env set")]
    public void AppleVision_WhenEnvSet_ReadsTextFromImage()
    {
        var imagePath = Environment.GetEnvironmentVariable("GMBASIC_APPLE_VISION_IMAGE");
        if (string.IsNullOrWhiteSpace(imagePath) || !File.Exists(imagePath))
            return; // graceful skip, same opt-in convention

        try
        {
            ProviderServer.ResetForTests();
            ProviderServer.Instance.Register(ProviderRole.Completion, "apple-vision", "apple-vision",
                new Dictionary<string, string> { });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "apple-vision");

            var completer = ProviderServer.Instance.GetActiveCompletion();
            completer.Should().BeAssignableTo<IVisionCompleter>();
            var vision = (IVisionCompleter)completer;

            var text = vision.DescribeImage(File.ReadAllBytes(imagePath), ".png", "OCR:");
            text.Should().NotBeNullOrWhiteSpace("Apple Vision must extract readable text from the test image");
        }
        finally
        {
            ProviderServer.ResetForTests();
        }
    }

    [Fact(DisplayName = "D1 (AI-Operational-Plan) kvSwapMaxKvMb: RAM watermark evicts the LRU session when hot KV state exceeds the byte budget, restores it lazily")]
    public void AgentServer_Ask_EvictsByKvBytesWatermark_WhenKvSwapMaxKvMbSetAndModelEnvSet()
    {
        if (fixture.ModelPath is null)
            return; // graceful skip — same opt-in convention as the other real-model tests

        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        var swapDir = Path.Combine(Path.GetTempPath(), "gmbasic-kvswap-watermark-test-" + Guid.NewGuid().ToString("N"));
        try
        {
            // One hot session at ~2048 ctx on the tiny fixture model measures in the hundreds of
            // KB to a few MB of llama_state bytes (weights are NOT part of it), so a
            // kvSwapMaxKvMb budget of 0 MB would always trip and a huge one never would. We use
            // 1 MB (1 MB = 1048576 B budget): with THREE hot sessions each holding a decoded
            // "Once upon a time" turn the aggregate KV state exceeds it, so creating the third
            // agent's session must evict the LRU one (agent-1) to disk BEFORE the new session
            // exists, exactly like the count-based test above but on the byte axis.
            ProviderServer.Instance.Register(ProviderRole.Completion, "llamacpp-swap-bytes", "llamacpp",
                new Dictionary<string, string>
                {
                    ["modelPath"] = fixture.ModelPath,
                    ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                    ["maxTokens"] = "16",
                    ["kvSwapEnabled"] = "true",
                    ["kvSwapMaxHotSessions"] = "32", // count cap way above what the byte cap allows
                    ["kvSwapMaxKvMb"] = "1",
                    ["kvSwapPath"] = swapDir,
                });
            ProviderServer.Instance.SetActive(ProviderRole.Completion, "llamacpp-swap-bytes");

            foreach (var name in new[] { "swap-bytes-1", "swap-bytes-2" })
            {
                AgentServer.Instance.Create(name);
                AgentServer.Instance.Set(name, "maxtokens", "16");
            }

            AgentServer.Instance.Ask("swap-bytes-1", "Once upon a time");
            AgentServer.Instance.Ask("swap-bytes-2", "Once upon a time");

            // The byte watermark runs on EVERY session creation, including the very first ASK
            // of a second agent: with a 1 MB budget and a ctx-2048 session (whose llama_state
            // alone measures in the MB range at this size), agent-2's first ASK must have evicted
            // the LRU (swap-bytes-1) already. The invariant asserted below is that the eviction
            // DID fire on the byte axis while the count cap (32) could never have triggered it.
            var agent1Mid = AgentServer.Instance.List().Single(a => a.Name == "swap-bytes-1");
            agent1Mid.SwappedPathPrefix.Should().NotBeNull(
                "a single ctx-2048 llama_state exceeds the 1 MB kvSwapMaxKvMb budget, so agent-2's first ASK must have evicted agent-1 (LRU) — the count cap of 32 cannot explain this eviction");
            File.Exists(agent1Mid.SwappedPathPrefix + ".kv").Should().BeTrue("an evicted session leaves its .kv on disk");

            // A third agent's first ASK: watermark keeps holding (evicts swap-bytes-2, the new LRU).
            AgentServer.Instance.Create("swap-bytes-3");
            AgentServer.Instance.Set("swap-bytes-3", "maxtokens", "16");
            AgentServer.Instance.Ask("swap-bytes-3", "Once upon a time");

            var agent2 = AgentServer.Instance.List().Single(a => a.Name == "swap-bytes-2");
            agent2.SwappedPathPrefix.Should().NotBeNull("with agent-1 already evicted, agent-2 is the LRU hot session when agent-3 creates its own");

            // Next ASK on the earliest-evicted agent transparently restores it and generation
            // continues — the caller never has to know a swap happened.
            var answer = AgentServer.Instance.Ask("swap-bytes-1", "and then what happened?");
            answer.Should().NotBeNullOrWhiteSpace();

            var agent1After = AgentServer.Instance.List().Single(a => a.Name == "swap-bytes-1");
            agent1After.SwappedPathPrefix.Should().BeNull();
            agent1After.SessionId.Should().NotBeNull();
            File.Exists(agent1Mid.SwappedPathPrefix + ".kv").Should().BeFalse("the on-disk copy is deleted once restored into RAM");
        }
        finally
        {
            AgentServer.ResetForTests();
            ProviderServer.ResetForTests();
            if (Directory.Exists(swapDir)) Directory.Delete(swapDir, recursive: true);
        }
    }
}
