// FILE: AI.Tests.BrainScopeTests
// F1 dello scope dei brain (Knowledge/Brain-Scope-Modello.md): resolver read-only.
// Copre le regole del modello: innermost wins · vuoto ≠ assente · PRIVATE interrompe la catena ·
// alias risolti nel livello che li dichiara · WITHOUT sottrae · dedup con vittoria dell'interno ·
// set effettivo = unione dei livelli · nessuna mutazione del registry globale.
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class BrainScopeTests
{
    // ── 1. innermost wins ─────────────────────────────────────────────────
    [Fact(DisplayName = "Scope: il livello più interno vince sul nome omonimo")]
    public void InnerLevelWins()
    {
        var process = BrainScope.Process("finance", "airline");
        var agent = process.Agent([new BrainRef("finance", "fin")]);

        agent.Resolve("finance")!.Level.Should().Be(BrainScopeLevel.Agent);
        agent.Resolve("finance")!.Ref.Alias.Should().Be("fin");
        agent.Resolve("airline")!.Level.Should().Be(BrainScopeLevel.Process);
    }

    [Fact(DisplayName = "Scope: l'argomento della query (CALL) vince su tutto")]
    public void CallLevelWins()
    {
        var process = BrainScope.Process("airline");
        var agent = process.Agent([new BrainRef("finance")]);
        var task = agent.Task([new BrainRef("trading")]);
        var call = task.Call([new BrainRef("ledger")]);

        call.Resolve("airline")!.Level.Should().Be(BrainScopeLevel.Process);
        call.Resolve("finance")!.Level.Should().Be(BrainScopeLevel.Agent);
        call.Resolve("trading")!.Level.Should().Be(BrainScopeLevel.Task);
        call.Resolve("ledger")!.Level.Should().Be(BrainScopeLevel.Call);
    }

    // ── 2. vuoto ≠ assente ────────────────────────────────────────────────
    [Fact(DisplayName = "Scope: nessuna dichiarazione eredita dal livello esterno")]
    public void NoDeclarationInherits()
    {
        var process = BrainScope.Process("airline");
        var agent = process.Agent(declared: null);   // INHERIT

        agent.Resolve("airline").Should().NotBeNull();
        agent.Effective().Select(r => r.Name).Should().Equal("airline");
    }

    [Fact(DisplayName = "Scope: set VUOTO dichiarato interrompe la catena (vuoto ≠ assente)")]
    public void ExplicitEmptyStopsTheChain()
    {
        var process = BrainScope.Process("airline");
        var agent = process.Agent(declared: []);

        agent.Resolve("airline").Should().BeNull();     // NON eredita: il vuoto è una dichiarazione
        agent.Effective().Should().BeEmpty();
    }

    [Fact(DisplayName = "Scope: PRIVATE ignora i livelli esterni ma tiene i propri")]
    public void PrivateIgnoresOuter()
    {
        var process = BrainScope.Process("airline", "finance");
        var agent = process.Agent([new BrainRef("finance")], isPrivate: true);

        agent.Resolve("airline").Should().BeNull();
        agent.Resolve("finance")!.Level.Should().Be(BrainScopeLevel.Agent);
        agent.Effective().Select(r => r.Name).Should().Equal("finance");
    }

    [Fact(DisplayName = "Scope: PRIVATE senza dichiarazioni = nessun brain visibile")]
    public void PrivateWithoutDeclarationsIsEmpty()
    {
        var process = BrainScope.Process("airline");
        var agent = process.Agent(isPrivate: true);

        agent.Resolve("airline").Should().BeNull();
        agent.Effective().Should().BeEmpty();
    }

    // ── 3. alias locali ───────────────────────────────────────────────────
    [Fact(DisplayName = "Scope: l'alias risolve nel livello che lo dichiara (non in quelli esterni)")]
    public void AliasResolvesInItsOwnLevel()
    {
        var process = BrainScope.Process("finance");
        var agent = process.Agent([new BrainRef("finance", "fin")]);
        var task = agent.Task([new BrainRef("airline", "flights")]);

        // 'fin' è dichiarato dall'AGENT: il TASK non lo vede fino a che non esclude
        task.Resolve("fin")!.Level.Should().Be(BrainScopeLevel.Agent);
        task.Resolve("flights")!.Level.Should().Be(BrainScopeLevel.Task);
        // il NOME resta risolvibile dal livello che lo dichiara (l'alias è un nome aggiuntivo, non un sostituto):
        task.Resolve("finance")!.Level.Should().Be(BrainScopeLevel.Agent);
    }

    [Fact(DisplayName = "Scope: lo stesso brain può avere alias diversi in scope diversi")]
    public void SameBrainDifferentAliases()
    {
        var process = BrainScope.Process("finance");
        var agentA = process.Agent([new BrainRef("finance", "fin")]);
        var agentB = process.Agent([new BrainRef("finance", "markets")]);

        agentA.Resolve("fin").Should().NotBeNull();
        agentA.Resolve("markets").Should().BeNull();
        agentB.Resolve("markets").Should().NotBeNull();
        agentB.Resolve("fin").Should().BeNull();
    }

    // ── 4. WITHOUT ────────────────────────────────────────────────────────
    [Fact(DisplayName = "Scope: WITHOUT sottrae nomi e alias ereditati")]
    public void WithoutSubtractsInherited()
    {
        var process = BrainScope.Process("airline", "finance");
        var agent = process.Agent(without: ["airline"]);

        agent.Resolve("airline").Should().BeNull();
        agent.Resolve("finance").Should().NotBeNull();
        agent.Effective().Select(r => r.Name).Should().Equal("finance");
    }

    [Fact(DisplayName = "Scope: WITHOUT agisce anche sugli alias (match per identificatore)")]
    public void WithoutSubtractsAliases()
    {
        var process = BrainScope.Process("airline", "finance");
        var agent = process.Agent([new BrainRef("finance", "fin")]);
        var task = agent.Task(without: ["fin"]);

        // il riferimento 'finance AS fin' dichiarato dall'AGENT è nascosto (match sull'alias):
        task.Resolve("fin").Should().BeNull();
        // ...ma la sottrazione è per IDENTIFICATORE: il riferimento 'finance' (senza alias) del PROCESS
        // resta visibile — nessuna risoluzione implicita «per identità del brain» dentro il resolver.
        task.Resolve("finance")!.Level.Should().Be(BrainScopeLevel.Process);
        task.Effective().Select(r => r.Name).Should().Equal("airline", "finance");   // ordine: prima i livelli esterni rimasti
    }

    // ── 5. set effettivo ──────────────────────────────────────────────────
    [Fact(DisplayName = "Scope: Effective() è l'unione dei livelli, con dedup e vittoria dell'interno")]
    public void EffectiveIsUnionWithInnerWins()
    {
        var process = BrainScope.Process("airline", "finance");
        var agent = process.Agent([new BrainRef("finance", "fin"), new BrainRef("trading")]);
        var task = agent.Task([new BrainRef("ledger")]);

        var eff = task.Effective();
        eff.Select(r => r.Name).Should().Equal("ledger", "finance", "trading", "airline");
        eff.Single(r => r.Name == "finance").Alias.Should().Be("fin");   // vince il livello AGENT
    }

    [Fact(DisplayName = "Scope: Effective() senza dichiarazioni su tutta la catena è vuoto")]
    public void EffectiveEmptyWhenNothingDeclared()
    {
        var process = BrainScope.Process();   // esplicito: nessuna lettura del registry
        process.Agent().Task().Effective().Should().BeEmpty();
    }

    // ── 6. catena e livelli ───────────────────────────────────────────────
    [Fact(DisplayName = "Scope: i livelli innestati seguono l'ordine Process→Session→Agent→Task→Call")]
    public void LevelsFollowTheChainOrder()
    {
        var process = BrainScope.Process("a");
        var session = process.Session();
        var agent = session.Agent();
        var task = agent.Task();
        var call = task.Call();

        process.Level.Should().Be(BrainScopeLevel.Process);
        session.Level.Should().Be(BrainScopeLevel.Session);
        agent.Level.Should().Be(BrainScopeLevel.Agent);
        task.Level.Should().Be(BrainScopeLevel.Task);
        call.Level.Should().Be(BrainScopeLevel.Call);
    }

    [Fact(DisplayName = "Scope: Explain() descrive ogni livello, la catena interrotta e il set effettivo")]
    public void ExplainDescribesTheChain()
    {
        var process = BrainScope.Process("airline");
        var agent = process.Agent([new BrainRef("finance", "fin")]);
        var task = agent.Task(declared: []);

        var lines = task.Explain();
        lines.Should().Contain(l => l.Contains("Task") && l.Contains("set VUOTO"));
        lines.Should().NotContain(l => l.Contains("Agent"));            // la catena si ferma al set vuoto
        lines.Should().NotContain(l => l.Contains("Process"));
        lines.Last().Should().Contain("(vuoto)");
    }

    // ── 7. read-only sul registry ─────────────────────────────────────────
    [Fact(DisplayName = "Scope: ForProcess() legge il registry senza modificarlo")]
    public void ForProcessIsReadOnly()
    {
        var registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brainscope-" + Guid.NewGuid().ToString("N") + ".json");
        var brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-brainscope-" + Guid.NewGuid().ToString("N"));
        var configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            // ⚠ AiConfig.OverridePathForTests PRIMA di qualunque setter: senza, `BrainsDefaultDir = …`
            // scrive il config UTENTE reale (<repo>/config/ai.json) — incidente 2026-09-16 in questa suite.
            Basic.AI.AiConfig.OverridePathForTests(configPath);
            Basic.AI.AiConfig.BrainsDefaultDir = brainsRoot;
            BrainRegistry.OverridePathForTests(registryPath);
            BrainRegistry.Register("finance", mode: "graph");
            BrainRegistry.Use("finance");
            var before = File.Exists(registryPath) ? File.ReadAllText(registryPath) : "";

            var scope = BrainScope.FromRegistry();
            scope.Resolve("finance").Should().NotBeNull();
            scope.Effective().Select(r => r.Name).Should().Equal("finance");

            var after = File.Exists(registryPath) ? File.ReadAllText(registryPath) : "";
            after.Should().Be(before, "il resolver non deve MAI scrivere il registry globale");
        }
        finally
        {
            Basic.AI.AiConfig.ResetForTests();
            BrainRegistry.ResetForTests();
            try { Directory.Delete(brainsRoot, recursive: true); } catch { }
            if (File.Exists(registryPath)) File.Delete(registryPath);
            if (File.Exists(configPath)) File.Delete(configPath);
        }
    }

    [Fact(DisplayName = "Scope: nome vuoto/inesistente → null (il chiamante deve fallire esplicito)")]
    public void UnknownNameIsNull()
    {
        var scope = BrainScope.Process("finance");
        scope.Resolve("").Should().BeNull();
        scope.Resolve("   ").Should().BeNull();
        scope.Resolve("non-esiste").Should().BeNull();
    }
}
