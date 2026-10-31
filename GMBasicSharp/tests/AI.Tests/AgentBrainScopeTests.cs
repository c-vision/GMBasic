// FILE: AI.Tests.AgentBrainScopeTests
// F2 dello scope dei cervelli (AiWiki Knowledge/Brain-Scope-Modello.md): livello AGENT.
// Copre: parsing della dichiarazione (`finance AS fin, airline`), risoluzione per i tool
// (esplicito → agente → null = default invariato), restringimento effettivo dei tool rag.*/graph.*,
// persistenza dell'agente (AgentPersistence), prompt di sistema.
// Isolato: directory brains/rag/config temporanee (mai lo stato utente).
using Basic.AI;
using Basic.AI.Knowledge;
using Basic.AI.Rag;
using Basic.Agents;
using CommonLib;
using Basic.SQLite.Vector;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class AgentBrainScopeTests : IDisposable
{
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-abrains-" + Guid.NewGuid().ToString("N"));
    private readonly string _ragDir = Path.Combine(Path.GetTempPath(), "gmbasic-abrains-rag-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-abrains-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _configDir = Path.Combine(Path.GetTempPath(), "gmbasic-confdir-" + Guid.NewGuid().ToString("N"));
    private readonly IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.Databases.Rdd.DataSession _ds;

    public AgentBrainScopeTests()
    {
        Directory.CreateDirectory(_configDir);
        GmbiPaths.OverrideConfigDirForTests(_configDir);   // VaultManager (vaults.db) e file config: temporanei
        AiConfig.OverridePathForTests(_configPath);
        Basic.AI.Knowledge.VaultManager.ResetForTests();    // stato process-wide (LastCreated)
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.RagDefaultDir = _ragDir;
        AiConfig.GraphsDefaultDir = Path.Combine(_brainsRoot, "graphs-legacy");
        BrainRegistry.OverridePathForTests(_registryPath);
        Directory.CreateDirectory(_ragDir);
        Directory.CreateDirectory(AiConfig.GraphsDefaultDir);
        // Sessione SQL dedicata (pattern RagAgentLoopTests): senza, il db RAG puede finire in sola lettura.
        _prevEmb = EmbeddingProviderResolver.Default;
        _prevDims = EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        _ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = _ds;
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        GmbiPaths.ResetConfigDirOverrideForTests();
        Basic.AI.Knowledge.VaultManager.ResetForTests();
        BrainRegistry.ResetForTests();
        EmbeddingProviderResolver.Default = _prevEmb;
        EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        try { Directory.Delete(_ragDir, recursive: true); } catch { }
        try { Directory.Delete(_configDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    private static AgentRecord Agent(string brains = "") => new() { Name = "a1", Brains = brains };

    private void MakeRagBrain(string name, string content)
    {
        var dbPath = Path.Combine(_ragDir, name + ".db");
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, name);
        AiVectorRuntime.EnsureCollection(name, "wiki", 8);
        AiVectorRuntime.IndexText(name, "wiki", "/src/" + name + ".md", content);
        BrainRegistry.Register(name, mode: "rag");
    }

    // ── 1. parsing della dichiarazione ────────────────────────────────────
    [Fact(DisplayName = "AgentBrains: parse di nomi e alias")]
    public void ParseNamesAndAliases()
    {
        var refs = AgentBrains.ParseDeclared("finance AS fin, airline");
        refs.Should().NotBeNull();
        refs!.Select(r => r.Name).Should().Equal("finance", "airline");
        refs[0].Alias.Should().Be("fin");
        refs[1].Alias.Should().BeNull();

        AgentBrains.ParseDeclared("").Should().BeNull("vuoto = nessuna dichiarazione (INHERIT)");
        AgentBrains.ParseDeclared(null).Should().BeNull();
        AgentBrains.Describe("finance AS fin").Should().Be("finance AS fin");
        AgentBrains.Describe("").Should().Be("");
    }

    // ── 2. risoluzione per i tool: esplicito → agente → null (default invariato) ──
    [Fact(DisplayName = "AgentBrains: senza dichiarazione → null (il default del consumer NON cambia)")]
    public void NoDeclarationMeansNull()
    {
        AgentBrains.ResolveForTool(Agent(), null).Should().BeNull();
        AgentBrains.ResolveForTool(null, null).Should().BeNull();
        AgentBrains.ResolveForTool(Agent(), "").Should().BeNull();
    }

    [Fact(DisplayName = "AgentBrains: la dichiarazione dell'agente vale quando il tool non specifica 'brain'")]
    public void AgentDeclarationUsed()
    {
        AgentBrains.ResolveForTool(Agent("finance AS fin, airline"), null).Should().Equal("finance", "airline");
    }

    [Fact(DisplayName = "AgentBrains: l'argomento esplicito del tool vince sulla dichiarazione dell'agente")]
    public void ExplicitArgumentWins()
    {
        AgentBrains.ResolveForTool(Agent("finance"), "airline,trading").Should().Equal("airline", "trading");
    }

    // ── 2bis. LIVELLO TASK (F3): precedenza e isolamento fra task paralleli ──
    [Fact(DisplayName = "AgentBrains: precedenza tool-arg → task → agente → null")]
    public void PrecedenceThreeLevels()
    {
        var agent = Agent("agentbrain");
        AgentBrains.ResolveForTool(agent, null, null).Should().Equal(new[] { "agentbrain" });
        AgentBrains.ResolveForTool(agent, null, "taskbrain").Should().Equal(new[] { "taskbrain" });
        AgentBrains.ResolveForTool(agent, "callbrain", "taskbrain").Should().Equal(new[] { "callbrain" });
        AgentBrains.ResolveForTool(Agent(), null, null).Should().BeNull();
        AgentBrains.ResolveForTool(Agent("agentbrain"), null, "").Should().Equal(new[] { "agentbrain" });
    }

    [Fact(DisplayName = "Task paralleli (Task.Run) con scope diversi non si contaminano e non toccano il registry")]
    public async Task ParallelTasksWithDifferentScopesAreIsolated()
    {
        MakeRagBrain("ra", "risk parity allocates bonds and equities");
        MakeRagBrain("rb", "momentum factor for intraday trading");
        BrainRegistry.Use("rb");                     // attivo globalmente: il task su 'ra' NON deve vederlo
        var registryBefore = File.ReadAllText(_registryPath);

        // Due task concorrenti (thread pool, fuori GPM): lo scope è un PARAMETRO, mai stato ambientale.
        var t1 = Task.Run(() => RagAgentLoop.Execute("rag.search", """{"query":"risk parity","topK":5}""", ["ra"]));
        var t2 = Task.Run(() => RagAgentLoop.Execute("rag.search", """{"query":"momentum","topK":5}""", ["rb"]));
        var results = await Task.WhenAll(t1, t2);

        static List<string> BrainsOf(string json) => System.Text.RegularExpressions.Regex
            .Matches(json, "\"brain\":\"([^\"]+)\"")
            .Select(m => m.Groups[1].Value).Distinct().ToList();

        BrainsOf(results[0]).Should().OnlyContain(n => n == "ra");
        BrainsOf(results[1]).Should().OnlyContain(n => n == "rb");
        File.ReadAllText(_registryPath).Should().Be(registryBefore, "un task non scrive MAI il registry globale (§0ter / modello di scope)");
        BrainRegistry.Used().Should().Equal("rb");
    }

    // ── 2ter. F4: il default dei consumer è il set effettivo dello SCOPE ──
    [Fact(DisplayName = "F4: MultiBrainSearch accetta uno scope esplicito e senza scope resta invariato")]
    public void SearchMultiUsesScope()
    {
        MakeRagBrain("ra", "risk parity allocates bonds and equities");
        MakeRagBrain("rb", "risk parity momentum intraday trading");
        BrainRegistry.Use("ra");
        BrainRegistry.Use("rb");

        var scope = AgentBrains.ScopeFor(Agent("ra"));
        var restricted = MultiBrainSearch.SearchMulti("risk parity", 5, "wiki", null, scope: scope);
        System.Text.RegularExpressions.Regex.Matches(restricted, "\"brain\":\"([^\"]+)\"")
            .Select(m => m.Groups[1].Value).Distinct().Should().OnlyContain(n => n == "ra");

        var unchanged = MultiBrainSearch.SearchMulti("risk parity", 5);
        var names = System.Text.RegularExpressions.Regex.Matches(unchanged, "\"brain\":\"([^\"]+)\"")
            .Select(m => m.Groups[1].Value).Distinct().ToList();
        names.Should().Contain("ra").And.Contain("rb", "senza scope il default è PROCESS = brain attivati (invariato)");
    }

    [Fact(DisplayName = "F4: GraphMultiQuery accetta uno scope esplicito")]
    public void QueryMultiUsesScope()
    {
        var graphsDir = AiConfig.GraphsDefaultDir;
        Directory.CreateDirectory(graphsDir);
        File.WriteAllText(Path.Combine(graphsDir, "g1.json"),
            "{\"nodes\":[{\"id\":\"index.md\",\"label\":\"index.md\"}],\"edges\":[]}");
        File.WriteAllText(Path.Combine(graphsDir, "g2.json"),
            "{\"nodes\":[{\"id\":\"altro.md\",\"label\":\"altro.md\"}],\"edges\":[]}");
        BrainRegistry.Register("g1", mode: "graph");
        BrainRegistry.Register("g2", mode: "graph");
        BrainRegistry.Use("g1");
        BrainRegistry.Use("g2");

        var restricted = Basic.AI.Graph.GraphMultiQuery.QueryMulti("index", scope: BrainScope.Process("g1"));
        restricted.Should().Contain("index.md").And.NotContain("altro.md");
    }

    [Fact(DisplayName = "F4: vault non specificato → 1 brain nello scope = usato; 2 brain = errore ESPLICITO (niente 'primo')")]
    public void VaultRootAmbiguityIsExplicit()
    {
        MakeVaultBrain("va");
        Basic.AI.Knowledge.VaultManager.ResetForTests();   // nessun 'ultimo vault creato' residuo
        Basic.AI.Knowledge.NoteManager.VaultRoot("", BrainScope.Process("va")).Should().NotBeNull();

        var act = () => Basic.AI.Knowledge.NoteManager.VaultRoot("", BrainScope.Process("va", "vb"));
        act.Should().Throw<InvalidOperationException>().WithMessage("*vault ambiguo*");
    }

    [Fact(DisplayName = "F4: 'brainvault' è un alias riservato rimappabile dallo scope")]
    public void BrainVaultReservedAlias()
    {
        Basic.AI.Knowledge.BrainVault.ResolveBrainVaultName().Should().Be("brainvault");
        // rimappatura tramite ALIAS riservato (come `BRAIN USE altro AS brainvault`)
        var remapped = BrainScope.ProcessRefs(new BrainRef("altro", "brainvault"));
        Basic.AI.Knowledge.BrainVault.ResolveBrainVaultName(remapped).Should().Be("altro");
        // senza alias resta il brain registrato 'brainvault'
        Basic.AI.Knowledge.BrainVault.ResolveBrainVaultName(BrainScope.Process("altro")).Should().Be("brainvault");
    }

    private void MakeVaultBrain(string name)
    {
        var dir = Path.Combine(_brainsRoot, name);
        Directory.CreateDirectory(Path.Combine(dir, "vault"));
        File.WriteAllText(Path.Combine(dir, "vault", "nota.md"), "# nota");
        BrainRegistry.Register(name, vaultPath: Path.Combine(dir, "vault"), mode: "graph");
    }

    // ── 2quater. F5: enforcement del grant + import conservativo del bundle ──
    [Fact(DisplayName = "F5: un brain fuori dal grant dell'agente è un ERRORE esplicito (nessuna query silenziosa)")]
    public void GrantIsEnforced()
    {
        var agent = Agent("finance");
        AgentBrains.TryResolveForTool(agent, "finance", null, out var okBrains, out var okErr).Should().BeTrue();
        okBrains.Should().Equal(new[] { "finance" });
        okErr.Should().BeNull();

        AgentBrains.TryResolveForTool(agent, "airline", null, out _, out var err).Should().BeFalse();
        err.Should().Contain("non concesso").And.Contain("airline").And.Contain("finance");

        // agente SENZA dichiarazione: nessuna restrizione (comportamento storico) — l'argomento del tool
        // resta valido e, senza argomento, si torna al default del consumer (null)
        AgentBrains.TryResolveForTool(Agent(), "airline", null, out var freeBrains, out var freeErr).Should().BeTrue();
        freeErr.Should().BeNull();
        freeBrains.Should().Equal(new[] { "airline" }, "senza grant non c'è enforcement: l'argomento del tool vale");
        AgentBrains.TryResolveForTool(Agent(), null, null, out var noneBrains, out _).Should().BeTrue();
        noneBrains.Should().BeNull("né tool né agente né task dichiarano → default del consumer");

        // l'alias concesso vale come il nome
        AgentBrains.TryResolveForTool(Agent("finance AS fin"), "fin", null, out _, out var aliasErr).Should().BeTrue();
        aliasErr.Should().BeNull();
    }

    [Fact(DisplayName = "F5: import del bundle CONSERVATIVO — i brain NON viaggiano se non richiesto")]
    public void BundleImportIsConservative()
    {
        var path = Path.Combine(Path.GetTempPath(), "gmbasic-bundle-" + Guid.NewGuid().ToString("N") + ".bagent");
        try
        {
            var snap = AgentPersistence.Capture(new AgentRecord { Name = "bundle-src", Brains = "finance AS fin" });
            File.WriteAllText(path, System.Text.Json.JsonSerializer.Serialize(new
            {
                format = AgentBundle.Format,
                version = AgentBundle.Version,
                snapshot = snap,
                skills = Array.Empty<string>(),
                hasKeypair = false
            }));

            var name = AgentBundle.Import(path);                 // default: conservativo
            var imported = AgentServer.Instance.List().First(a => a.Name == name);
            imported.Brains.Should().BeEmpty("un bundle di terze parti non eredita i cervelli: il grant si dà esplicitamente");

            var nameKeep = AgentBundle.Import(path, "bundle-keep", keepBrains: true);
            var kept = AgentServer.Instance.List().First(a => a.Name == nameKeep);
            kept.Brains.Should().Be("finance AS fin");
            AgentServer.Instance.Delete(name);
            AgentServer.Instance.Delete(nameKeep);
        }
        finally { if (File.Exists(path)) File.Delete(path); }
    }

    // ── 3. effetto REALE sui tool (rifiuto/restrizione) ────────────────────
    [Fact(DisplayName = "rag.search: con dichiarazione dell'agente cerca SOLO nel brain dichiarato")]
    public void RagSearchRestrictedToAgentBrains()
    {
        MakeRagBrain("ra", "risk parity allocates bonds and equities");
        MakeRagBrain("rb", "momentum factor for intraday trading");
        BrainRegistry.Use("rb");                      // globalmente attivo: un agente senza scope lo vedrebbe

        var scope = AgentBrains.ResolveForTool(Agent("ra"), null);
        var res = RagAgentLoop.Execute("rag.search", """{"query":"risk parity","topK":5}""", scope);
        res.Should().Contain("\"status\":\"Success\"");

        var names = System.Text.RegularExpressions.Regex.Matches(res, "\"brain\":\"([^\"]+)\"").Select(m => m.Groups[1].Value).ToList();
        names.Should().NotBeEmpty();
        names.Should().OnlyContain(n => n == "ra", "lo scope dell'agente esclude 'rb'");
    }

    [Fact(DisplayName = "rag.search: senza scope il comportamento resta quello di prima (tutti i collegati)")]
    public void RagSearchWithoutScopeUnchanged()
    {
        MakeRagBrain("ra", "risk parity allocates bonds and equities");
        MakeRagBrain("rb", "risk parity momentum intraday trading");
        BrainRegistry.Use("ra");
        BrainRegistry.Use("rb");

        var res = RagAgentLoop.Execute("rag.search", """{"query":"risk parity","topK":5}""");   // scope null
        var names = System.Text.RegularExpressions.Regex.Matches(res, "\"brain\":\"([^\"]+)\"").Select(m => m.Groups[1].Value).ToList();
        names.Should().Contain("ra");
        names.Should().Contain("rb", "senza dichiarazione di scope nulla cambia");
    }

    [Fact(DisplayName = "graph.query: lo scope dell'agente restringe i cervelli interrogati")]
    public void GraphQueryRestrictedToAgentBrains()
    {
        // grafo minimo per 'ga' (un brain graph-only) e nessuno per 'gb'
        var graphsDir = AiConfig.GraphsDefaultDir;
        Directory.CreateDirectory(graphsDir);
        File.WriteAllText(Path.Combine(graphsDir, "ga.json"), """
        {"nodes":[{"id":"index.md","label":"index.md"},{"id":"x.md","label":"x.md"}],
         "edges":[{"from":"index.md","to":"x.md","type":"link"}]}
        """);
        BrainRegistry.Register("ga", mode: "graph");

        var res = GraphAgentLoop.Execute("graph.query", """{"query":"index"}""", ["ga"]);
        res.Should().NotStartWith("error:");
        res.Should().Contain("index.md");
    }

    // ── 4. persistenza dell'agente ────────────────────────────────────────
    [Fact(DisplayName = "AgentPersistence: la dichiarazione 'brains' sopravvive a save/load")]
    public void BrainsSurvivePersistence()
    {
        var path = Path.Combine(Path.GetTempPath(), "gmbasic-agent-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            var agent = Agent("finance AS fin, airline");
            agent.History.Add(new AgentChatMessage("user", "ciao"));
            AgentPersistence.SaveToFile(path, AgentPersistence.Capture(agent));

            var loaded = AgentPersistence.Restore(AgentPersistence.LoadFromFile(path));
            loaded.Brains.Should().Be("finance AS fin, airline");
        }
        finally { if (File.Exists(path)) File.Delete(path); }
    }

    [Fact(DisplayName = "AgentPersistence: snapshot senza 'brains' (pre-F2) → nessuna dichiarazione")]
    public void LegacySnapshotHasNoBrains()
    {
        var path = Path.Combine(Path.GetTempPath(), "gmbasic-agent-old-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            File.WriteAllText(path, """
            {"version":1,"name":"old","id":"1","model":"m","temperature":0.7,"maxTokens":100,
             "systemPrompt":"s","providerName":null,"toolNames":[],"history":[]}
            """);
            var loaded = AgentPersistence.Restore(AgentPersistence.LoadFromFile(path));
            loaded.Brains.Should().BeEmpty();
            AgentBrains.ResolveForTool(loaded, null).Should().BeNull("compat: nessuna dichiarazione");
        }
        finally { if (File.Exists(path)) File.Delete(path); }
    }

    // ── 5. prompt di sistema ──────────────────────────────────────────────
    [Fact(DisplayName = "Prompt: i cervelli dichiarati compaiono nel system prompt (e solo se dichiarati)")]
    public void PromptDeclaresBrains()
    {
        var withBrains = Agent("finance AS fin").ResolveSystemPrompt();
        withBrains.Should().Contain("Cervelli (second brain) assegnati a te");
        withBrains.Should().Contain("finance AS fin");

        var without = Agent().ResolveSystemPrompt();
        without.Should().NotContain("Cervelli (second brain) assegnati a te", "prompt invariato per gli agenti esistenti");
    }

    // ── 6. scope AGENT + PROCESS (catena) ─────────────────────────────────
    [Fact(DisplayName = "Scope agente: dichiarare i brain è un GRANT → l'agente NON eredita i globali")]
    public void AgentDeclarationIsGrant()
    {
        MakeRagBrain("ra", "risk parity");
        MakeRagBrain("rb", "momentum");
        BrainRegistry.Use("rb").Should().BeTrue("il brain RAG 'rb' è disponibile e va attivato globalmente");

        var scope = AgentBrains.ScopeFor(Agent("ra"));
        scope.Resolve("ra")!.Level.Should().Be(BrainScopeLevel.Agent);
        scope.Resolve("rb").Should().BeNull("con la dichiarazione l'agente vede SOLO i suoi brain");
        scope.Effective().Select(r => r.Name).Should().Equal("ra");
    }

    [Fact(DisplayName = "Scope agente: senza dichiarazione eredita PROCESS (comportamento invariato)")]
    public void AgentWithoutDeclarationInheritsProcess()
    {
        MakeRagBrain("rb", "momentum");
        BrainRegistry.Use("rb").Should().BeTrue();

        var scope = AgentBrains.ScopeFor(Agent());     // nessuna dichiarazione
        scope.Resolve("rb")!.Level.Should().Be(BrainScopeLevel.Process);
        scope.Effective().Select(r => r.Name).Should().Equal("rb");
    }
}
