// FILE: AI.Tests.BrainToolchainTests
// Toolchain dei cervelli V1 (AiWiki Todo/(open) - Brain-Toolchain-REPL-CLI.md):
// STATS · VERIFY (integrità) · CREATE (dry-run e reale) · OPTIMIZE · BACKUP.
// Isolato: brains/rag/graphs/config in directory temporanee (mai stato utente).
using Basic.AI;
using Basic.AI.Graph;
using Basic.AI.Knowledge;
using Basic.AI.Rag;
using Basic.SQLite.Vector;
using CommonLib;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class BrainToolchainTests : IDisposable
{
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-tc-" + Guid.NewGuid().ToString("N"));
    private readonly string _ragDir;
    private readonly string _graphsDir;
    private readonly string _configDir;
    private readonly string _configPath;
    private readonly string _registryPath;
    private readonly IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.Databases.Rdd.DataSession _ds;
    private readonly string _vault;

    public BrainToolchainTests()
    {
        _ragDir = Path.Combine(_brainsRoot, "rag");
        _graphsDir = Path.Combine(_brainsRoot, "graphs");
        _configDir = Path.Combine(_brainsRoot, "config");
        _configPath = Path.Combine(_brainsRoot, "ai.json");
        _registryPath = Path.Combine(_brainsRoot, "brains.json");
        Directory.CreateDirectory(_configDir);
        GmbiPaths.OverrideConfigDirForTests(_configDir);
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.RagDefaultDir = _ragDir;
        AiConfig.GraphsDefaultDir = _graphsDir;
        // ⚠ anche BrainsExportsDir: `Backup` senza --out scrive lì (senza override → cartella REALE del repo:
        // lezione dell'incidente 2026-09-16, due zip leakati in <repo>/brains-exports)
        AiConfig.BrainsExportsDir = Path.Combine(_brainsRoot, "exports");
        BrainRegistry.OverridePathForTests(_registryPath);
        VaultManager.ResetForTests();
        Directory.CreateDirectory(_ragDir);
        Directory.CreateDirectory(_graphsDir);

        _prevEmb = EmbeddingProviderResolver.Default;
        _prevDims = EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        _ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = _ds;
        // I test usano il FixtureEmbeddingProvider (deterministico): in produzione è rifiutato per
        // progetto — hook test-scoped (pattern AiConfig/BrainRegistry.OverridePathForTests).
        BrainToolchain.SkipEmbeddingValidationForTests = true;

        // vault sorgente minimo (create non usa ConversionLib nei test: solo .md)
        _vault = Path.Combine(_brainsRoot, "vault");
        Directory.CreateDirectory(_vault);
        File.WriteAllText(Path.Combine(_vault, "a.md"), "# A\n\n[[b]]");
        File.WriteAllText(Path.Combine(_vault, "b.md"), "# B\n\nrisk parity");
    }

    public void Dispose()
    {
        GmbiPaths.ResetConfigDirOverrideForTests();
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        VaultManager.ResetForTests();
        EmbeddingProviderResolver.Default = _prevEmb;
        EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        BrainToolchain.SkipEmbeddingValidationForTests = false;
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
    }

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

    private void MakeGraphBrain(string name)
    {
        File.WriteAllText(Path.Combine(_graphsDir, name + ".json"),
            "{\"nodes\":[{\"id\":\"a.md\",\"label\":\"a.md\"},{\"id\":\"b.md\",\"label\":\"b.md\"}],"
            + "\"edges\":[{\"from\":\"a.md\",\"to\":\"b.md\",\"type\":\"link\"}],\"dangling\":[]}");
        BrainRegistry.Register(name, mode: "graph");
    }

    // ── STATS ─────────────────────────────────────────────────────────────
    [Fact(DisplayName = "STATS: RAG (documenti/chunk/dimensione) e GRAPH (nodi/edge)")]
    public void StatsRagAndGraph()
    {
        MakeRagBrain("sr", "risk parity allocates bonds and equities");
        var rag = BrainToolchain.Stats("sr");
        rag.Ok.Should().BeTrue();
        rag.Text.Should().Contain("rag:").And.Contain("documenti");

        MakeGraphBrain("sg");
        var graph = BrainToolchain.Stats("sg");
        graph.Ok.Should().BeTrue();
        graph.Text.Should().Contain("2 nodi").And.Contain("1 edge");

        var json = BrainToolchain.Stats("sg", asJson: true);
        json.Text.Should().Contain("\"nodes\":2").And.Contain("\"name\":\"sg\"");
    }

    [Fact(DisplayName = "STATS: cervello inesistente → errore esplicito")]
    public void StatsUnknownBrain()
    {
        var r = BrainToolchain.Stats("non-esiste");
        r.Ok.Should().BeFalse();
        r.Error.Should().Contain("non registrato");
    }

    // ── VERIFY ────────────────────────────────────────────────────────────
    [Fact(DisplayName = "VERIFY: brain RAG+GRAPH sani → ok (manifest assente non è un errore)")]
    public void VerifyHealthy()
    {
        MakeRagBrain("vr", "risk parity");
        MakeGraphBrain("vr");
        var r = BrainToolchain.Verify("vr");
        r.Ok.Should().BeTrue(r.Text);
        r.Text.Should().Contain("BRAIN VERIFY").And.Contain("rag.integrity");
        r.Text.Should().Contain("graph.json");
    }

    [Fact(DisplayName = "VERIFY: manifest presente con checksum incoerente → KO")]
    public void VerifyDetectsTamperedManifest()
    {
        MakeGraphBrain("vt");
        var rel = "graphs/vt.json";
        var root = BrainRegistry.BrainRoot("vt") ?? _brainsRoot;
        var manifest = new BrainManifest
        {
            Name = "vt",
            Mode = "graph",
            Available = ["graph"],
            Files = new Dictionary<string, string> { [rel] = "sha256:DEADBEEF" }
        };
        Directory.CreateDirectory(root);
        File.WriteAllText(BrainRegistry.ManifestPath("vt"), manifest.ToJson(withChecksum: true));

        var r = BrainToolchain.Verify("vt");
        r.Ok.Should().BeFalse("un manifest con checksum diverso è un errore");
        r.Text.Should().Contain("manifest.checksum").And.Contain("checksum diverso");
    }

    [Fact(DisplayName = "VERIFY: cervello registrato ma SENZA artefatti → KO esplicito")]
    public void VerifyEmptyRegisteredBrain()
    {
        BrainRegistry.Register("vuoto", mode: "graph");     // registrato, nessun file su disco
        var r = BrainToolchain.Verify("vuoto");
        r.Ok.Should().BeFalse();
        r.Text.Should().Contain("artefatti").And.Contain("vuoto");
        r.Error.Should().NotBeNull();
    }

    // ── CREATE ────────────────────────────────────────────────────────────
    [Fact(DisplayName = "CREATE --dry-run: nessun artefatto scritto")]
    public void CreateDryRunWritesNothing()
    {
        var r = BrainToolchain.Create("newbrain", _vault, "both", register: true, activate: false, dryRun: true);
        r.Ok.Should().BeTrue(r.Text);
        r.Text.Should().Contain("[dry-run] RAG").And.Contain("[dry-run] GRAPH");
        File.Exists(Path.Combine(_ragDir, "newbrain.db")).Should().BeFalse();
        File.Exists(Path.Combine(_graphsDir, "newbrain.json")).Should().BeFalse();
        BrainRegistry.Info("newbrain").Should().BeNull("dry-run non registra");
    }

    [Fact(DisplayName = "CREATE graph + register: costruisce il grafo dal vault e lo registra")]
    public void CreateGraphRegisters()
    {
        var r = BrainToolchain.Create("gb", _vault, "graph", register: true, activate: false, dryRun: false);
        r.Ok.Should().BeTrue(r.Text);
        File.Exists(Path.Combine(_graphsDir, "gb.json")).Should().BeTrue();
        File.Exists(Path.Combine(_graphsDir, "gb.stats.json")).Should().BeTrue();
        var info = BrainRegistry.Info("gb");
        info.Should().NotBeNull();
        info!.Available.Should().Contain("graph");
    }

    [Fact(DisplayName = "CREATE: cartella inesistente o mode invalido → errore esplicito")]
    public void CreateValidatesInput()
    {
        BrainToolchain.Create("x", Path.Combine(_brainsRoot, "non-c-e"), "graph", false, false, false).Ok.Should().BeFalse();
        BrainToolchain.Create("x", _vault, "boh", false, false, false).Error.Should().Contain("--mode");
    }

    // ── OPTIMIZE ──────────────────────────────────────────────────────────
    [Fact(DisplayName = "OPTIMIZE: manutenzione RAG (VACUUM/optimize/FTS5) e export grafo")]
    public void OptimizeRuns()
    {
        MakeRagBrain("or", "risk parity allocates bonds and equities");
        MakeGraphBrain("or");

        var dry = BrainToolchain.Optimize("or", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Text);
        dry.Text.Should().Contain("[dry-run]").And.Contain("VACUUM");

        var real = BrainToolchain.Optimize("or", dryRun: false, reindexFts: true);
        real.Ok.Should().BeTrue(real.Text);
        real.Text.Should().Contain("FTS5 rebuild").And.Contain("GRAPH: export ricalcolati");

        // il brain resta interrogabile dopo l'ottimizzazione
        var stats = BrainToolchain.Stats("or");
        stats.Ok.Should().BeTrue();
        stats.Text.Should().Contain("documenti");
    }

    // ── RENAME (V1b) ──────────────────────────────────────────────────────
    [Fact(DisplayName = "RENAME: sposta gli artefatti, aggiorna il registry e conserva il collegamento")]
    public void RenameMovesArtifactsAndRegistry()
    {
        MakeGraphBrain("oldn");
        BrainRegistry.Use("oldn").Should().BeTrue();

        var dry = BrainToolchain.Rename("oldn", "newn", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Text);
        dry.Text.Should().Contain("[dry-run]");
        File.Exists(Path.Combine(_graphsDir, "oldn.json")).Should().BeTrue("dry-run non muove nulla");

        var r = BrainToolchain.Rename("oldn", "newn");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        File.Exists(Path.Combine(_graphsDir, "oldn.json")).Should().BeFalse();
        File.Exists(Path.Combine(_graphsDir, "newn.json")).Should().BeTrue();
        BrainRegistry.Info("oldn").Should().BeNull();
        BrainRegistry.Info("newn").Should().NotBeNull();
        BrainRegistry.Used().Should().Contain("newn").And.NotContain("oldn", "lo stato collegato si conserva");
        BrainToolchain.Stats("newn").Ok.Should().BeTrue();
    }

    [Fact(DisplayName = "RENAME: nome già in uso o brain inesistente → errore esplicito (nessuna mossa)")]
    public void RenameGuards()
    {
        MakeGraphBrain("a1");
        MakeGraphBrain("a2");
        BrainToolchain.Rename("a1", "a2").Ok.Should().BeFalse();
        BrainToolchain.Rename("a1", "a1").Error.Should().Contain("identico");
        BrainToolchain.Rename("non-esiste", "x").Error.Should().Contain("non registrato");
        File.Exists(Path.Combine(_graphsDir, "a1.json")).Should().BeTrue("le mosse non devono avvenire");
    }

    // ── V2: DEDUPE / EXTRACT / MERGE ──────────────────────────────────────
    [Fact(DisplayName = "DEDUPE: rileva i chunk duplicati (testo identico) e li rimuove con --apply")]
    public void DedupeRemovesIdenticalChunks()
    {
        var dbPath = Path.Combine(_ragDir, "dp.db");
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, "dp");
        AiVectorRuntime.EnsureCollection("dp", "wiki", 8);
        AiVectorRuntime.IndexText("dp", "wiki", "/src/a.md", "risk parity allocates bonds and equities");
        AiVectorRuntime.IndexText("dp", "wiki", "/src/b.md", "risk parity allocates bonds and equities");   // identico
        BrainRegistry.Register("dp", mode: "rag");

        var report = BrainToolchain.Dedupe("dp");
        report.Ok.Should().BeTrue(report.Error ?? report.Text);
        report.Text.Should().Contain("duplicati");
        report.Text.Should().Contain("(report: usa --apply");

        var applied = BrainToolchain.Dedupe("dp", apply: true);
        applied.Ok.Should().BeTrue(applied.Error ?? applied.Text);
        applied.Text.Should().Contain("rimossi");

        var stats = BrainToolchain.Stats("dp");
        stats.Ok.Should().BeTrue();
        stats.Text.Should().Contain("documenti");   // brain ancora interrogabile
    }

    [Fact(DisplayName = "DEDUPE: edge duplicati del grafo rimossi e export ricalcolati")]
    public void DedupeGraphEdges()
    {
        File.WriteAllText(Path.Combine(_graphsDir, "dpg.json"),
            "{\"nodes\":[{\"id\":\"a.md\",\"label\":\"a\"},{\"id\":\"b.md\",\"label\":\"b\"}],"
            + "\"edges\":[{\"from\":\"a.md\",\"to\":\"b.md\",\"type\":\"link\"},"
            + "{\"from\":\"a.md\",\"to\":\"b.md\",\"type\":\"link\"},"
            + "{\"from\":\"a.md\",\"to\":\"a.md\",\"type\":\"link\"}],\"dangling\":[]}");
        BrainRegistry.Register("dpg", mode: "graph");

        var r = BrainToolchain.Dedupe("dpg", apply: true);
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("1 edge duplicati").And.Contain("1 self-loop");
        var json = File.ReadAllText(Path.Combine(_graphsDir, "dpg.json"));
        json.Should().NotContain("\"from\":\"a.md\",\"to\":\"a.md\"");
    }

    [Fact(DisplayName = "EXTRACT: copia solo i documenti filtrati in un nuovo cervello (no re-embedding)")]
    public void ExtractSubset()
    {
        var dbPath = Path.Combine(_ragDir, "ex.db");
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, "ex");
        AiVectorRuntime.EnsureCollection("ex", "wiki", 8);
        AiVectorRuntime.IndexText("ex", "wiki", "/strategie/momentum.md", "momentum factor");
        AiVectorRuntime.IndexText("ex", "wiki", "/portfolio/riskparity.md", "risk parity bonds");
        BrainRegistry.Register("ex", mode: "rag");

        BrainToolchain.Extract("ex", "ex-sub", sourceFilter: null, nodeFilter: null).Ok.Should().BeFalse();
        var dry = BrainToolchain.Extract("ex", "ex-sub", sourceFilter: "strategie", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Error ?? dry.Text);
        dry.Text.Should().Contain("[dry-run]");
        File.Exists(Path.Combine(_ragDir, "ex-sub.db")).Should().BeFalse();

        var r = BrainToolchain.Extract("ex", "ex-sub", sourceFilter: "strategie");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("1 documenti");
        BrainRegistry.Info("ex-sub").Should().NotBeNull();

        var sub = BrainToolchain.Verify("ex-sub");
        sub.Ok.Should().BeTrue(sub.Error ?? sub.Text);
        sub.Text.Should().Contain("1 documenti");
    }

    [Fact(DisplayName = "EXTRACT: sottografo per nodo + hop")]
    public void ExtractSubgraph()
    {
        File.WriteAllText(Path.Combine(_graphsDir, "gx.json"),
            "{\"nodes\":[{\"id\":\"index.md\",\"label\":\"index\"},{\"id\":\"trading/a.md\",\"label\":\"a\"},"
            + "{\"id\":\"altro.md\",\"label\":\"altro\"}],"
            + "\"edges\":[{\"from\":\"index.md\",\"to\":\"trading/a.md\",\"type\":\"link\"}],\"dangling\":[]}");
        BrainRegistry.Register("gx", mode: "graph");

        var r = BrainToolchain.Extract("gx", "gx-sub", nodeFilter: "trading/", hops: 1);
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        var json = File.ReadAllText(Path.Combine(_graphsDir, "gx-sub.json"));
        json.Should().Contain("trading/a.md");
        json.Should().Contain("index.md");            // vicino a 1 hop
        json.Should().NotContain("altro.md");
    }

    [Fact(DisplayName = "MERGE: unione rag (dedup per source) + unione grafi, con provenance")]
    public void MergeBrains()
    {
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        foreach (var (name, src, text) in new[]
                 {
                     ("m1", "/src/uno.md", "risk parity"),
                     ("m2", "/src/due.md", "momentum"),
                     ("m3", "/src/uno.md", "risk parity duplicato")     // stesso source di m1
                 })
        {
            var db = Path.Combine(_ragDir, name + ".db");
            RagDbSession.Open(db, name);
            AiVectorRuntime.EnsureCollection(name, "wiki", 8);
            AiVectorRuntime.IndexText(name, "wiki", src, text);
            BrainRegistry.Register(name, mode: "rag");
        }
        File.WriteAllText(Path.Combine(_graphsDir, "m1.json"),
            "{\"nodes\":[{\"id\":\"a\",\"label\":\"a\"}],\"edges\":[],\"dangling\":[]}");
        File.WriteAllText(Path.Combine(_graphsDir, "m2.json"),
            "{\"nodes\":[{\"id\":\"b\",\"label\":\"b\"}],\"edges\":[],\"dangling\":[]}");

        var dry = BrainToolchain.Merge(["m1", "m2", "m3"], "mm", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Error ?? dry.Text);
        dry.Text.Should().Contain("[dry-run]");
        File.Exists(Path.Combine(_ragDir, "mm.db")).Should().BeFalse();

        var r = BrainToolchain.Merge(["m1", "m2", "m3"], "mm", onConflict: "skip");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("2 documenti", "m3 ha lo stesso source di m1: dedup per source_id");
        BrainRegistry.Info("mm")!.Mode.Should().Be("both", "ci sono artefatti sia rag sia graph");
        var stats = BrainToolchain.Stats("mm");
        stats.Ok.Should().BeTrue(stats.Error ?? stats.Text);
        stats.Text.Should().Contain("2 documenti");
        stats.Text.Should().Contain("2 nodi", "unione dei nodi dei due grafi (a + b)");
    }

    [Fact(DisplayName = "MERGE: meno di due cervelli o dimensione embedding diversa → errore esplicito")]
    public void MergeGuards()
    {
        MakeRagBrain("g1", "risk parity");
        BrainToolchain.Merge(["g1"], "out").Error.Should().Contain("<a> <b>");
        BrainToolchain.Merge(["g1", "non-esiste"], "out").Error.Should().Contain("non registrato");
    }

    [Fact(DisplayName = "RAG build: esclude le cartelle di servizio (.obsidian/.git/cache) dal vault")]
    public void RagSkipsServiceFolders()
    {
        var vault = Path.Combine(_brainsRoot, "vault-serv");
        Directory.CreateDirectory(Path.Combine(vault, ".obsidian"));
        Directory.CreateDirectory(Path.Combine(vault, ".git"));
        Directory.CreateDirectory(Path.Combine(vault, "note"));
        File.WriteAllText(Path.Combine(vault, "note", "a.md"), "# A");
        File.WriteAllText(Path.Combine(vault, ".obsidian", "app.json"), "{}");
        File.WriteAllText(Path.Combine(vault, ".git", "config.json"), "{}");
        File.WriteAllText(Path.Combine(vault, "b.md"), "# B");

        var files = RagIndexService.EnumerateFiles(vault);
        files.Should().Contain("note/a.md").And.Contain("b.md");
        files.Should().NotContain(f => f.Contains(".obsidian") || f.Contains(".git/"),
            "le cartelle di servizio non sono contenuto del vault (osservato sul brain airline)");
    }

    // ── V3: CONVERT / VERIFY --golden / DIFF / DOCTOR ─────────────────────
    [Fact(DisplayName = "CONVERT rag→graph: deriva il grafo dai source_id e dai wikilink dei chunk")]
    public void ConvertRagToGraph()
    {
        var dbPath = Path.Combine(_ragDir, "cv.db");
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, "cv");
        AiVectorRuntime.EnsureCollection("cv", "wiki", 8);
        AiVectorRuntime.IndexText("cv", "wiki", "vault/a.md", "# A\nvedi [[b]] e [[c]]");
        AiVectorRuntime.IndexText("cv", "wiki", "vault/b.md", "# B");
        AiVectorRuntime.IndexText("cv", "wiki", "vault/c.md", "# C\n[[b]]");
        BrainRegistry.Register("cv", mode: "rag");

        var dry = BrainToolchain.ConvertMode("cv", "graph", outName: "cv2", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Error ?? dry.Text);
        dry.Text.Should().Contain("[dry-run]").And.Contain("3 nodi");
        File.Exists(Path.Combine(_graphsDir, "cv2.json")).Should().BeFalse();

        var r = BrainToolchain.ConvertMode("cv", "graph", outName: "cv2");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("3 nodi").And.Contain("3 edge");
        BrainRegistry.Info("cv2")!.Available.Should().Contain("graph");

        // sotto la soglia di edge → errore esplicito, non un grafo vuoto
        BrainToolchain.ConvertMode("cv", "graph", outName: "cv3", minEdges: 99).Ok.Should().BeFalse();
    }

    [Fact(DisplayName = "CONVERT graph→rag: indicizza un documento per nodo (embedder attivo)")]
    public void ConvertGraphToRag()
    {
        File.WriteAllText(Path.Combine(_graphsDir, "gc.json"),
            "{\"nodes\":[{\"id\":\"a\",\"title\":\"A\",\"frontmatter\":{},\"community\":0,\"degree\":1},"
            + "{\"id\":\"b\",\"title\":\"B\",\"frontmatter\":{},\"community\":0,\"degree\":1}],"
            + "\"edges\":[{\"from\":\"a\",\"to\":\"b\",\"type\":\"link\"}],\"dangling\":[]}");
        BrainRegistry.Register("gc", mode: "graph");

        // dry-run: il walk dei nodi è verificabile senza embedder
        var dry = BrainToolchain.ConvertMode("gc", "rag", outName: "gc-rag", dryRun: true);
        dry.Ok.Should().BeTrue(dry.Error ?? dry.Text);
        dry.Text.Should().Contain("[dry-run]").And.Contain("2 documenti");
        // senza un provider embedding REALE l'indicizzazione è un ERRORE ESPLICITO (mai un no-op)
        var r = BrainToolchain.ConvertMode("gc", "rag", outName: "gc-rag");
        r.Ok.Should().BeFalse("il fixture embedding non è un provider reale: RAG BUILD/convert lo rifiutano");
        r.Error.Should().NotBeNullOrEmpty();
    }

    [Fact(DisplayName = "VERIFY --golden: P@1/P@3/recall/MRR su un golden set e soglia --min-p1")]
    public void VerifyGoldenMetrics()
    {
        var dbPath = Path.Combine(_ragDir, "gold.db");
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, "gold");
        AiVectorRuntime.EnsureCollection("gold", "wiki", 8);
        AiVectorRuntime.IndexText("gold", "wiki", "vault/riskparity.md", "risk parity allocates bonds and equities");
        AiVectorRuntime.IndexText("gold", "wiki", "vault/momentum.md", "momentum factor for intraday trading");
        BrainRegistry.Register("gold", mode: "rag");

        var golden = Path.Combine(_brainsRoot, "golden.jsonl");
        File.WriteAllLines(golden, new[]
        {
            "{\"query\":\"risk parity allocates bonds\",\"expect_sources\":[\"riskparity.md\"]}",
            "{\"query\":\"momentum intraday trading\",\"expect_sources\":[\"momentum.md\"]}"
        });

        var r = BrainToolchain.VerifyGolden("gold", golden, topK: 3, minP1: 0.5);
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("2 query").And.Contain("P@1").And.Contain("MRR");

        var strict = BrainToolchain.VerifyGolden("gold", golden, topK: 3, minP1: 1.01);
        strict.Ok.Should().BeFalse("soglia impossibile → KO esplicito");
        strict.Error.Should().Contain("sotto la soglia");
    }

    [Fact(DisplayName = "DIFF: differenze fra due cervelli (documenti RAG e nodi)")]
    public void DiffBrains()
    {
        MakeRagBrain("d1", "risk parity");
        MakeGraphBrain("d1");
        MakeRagBrain("d2", "momentum");
        File.WriteAllText(Path.Combine(_graphsDir, "d2.json"),
            "{\"nodes\":[{\"id\":\"x\",\"label\":\"x\"}],\"edges\":[],\"dangling\":[]}");
        BrainRegistry.Register("d2", mode: "both");

        var r = BrainToolchain.Diff("d1", "d2");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("RAG:").And.Contain("GRAPH:").And.Contain("solo-A").And.Contain("solo-B");

        var json = BrainToolchain.Diff("d1", "d2", asJson: true);
        json.Text.Should().Contain("\"a_documents\"");
    }

    [Fact(DisplayName = "DOCTOR: applica i fix sicuri e riverifica (dangling rimossi dal grafo)")]
    public void DoctorFixes()
    {
        // grafo con rapporto dangling alto (3 dangling su 1 edge) → VERIFY lo segnala → DOCTOR deve fixare
        File.WriteAllText(Path.Combine(_graphsDir, "doc.json"),
            "{\"nodes\":[{\"id\":\"a\",\"label\":\"a\"},{\"id\":\"b\",\"label\":\"b\"}],"
            + "\"edges\":[{\"from\":\"a\",\"to\":\"b\",\"type\":\"link\"}],"
            + "\"dangling\":[\"a -> m1\",\"b -> m2\",\"a -> m3\"]}");
        BrainRegistry.Register("doc", mode: "graph");
        BrainToolchain.Verify("doc").Ok.Should().BeFalse("prima del doctor il rapporto dangling supera la soglia");

        var r = BrainToolchain.Doctor("doc");
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        r.Text.Should().Contain("rimossi 3 dangling");
        File.ReadAllText(Path.Combine(_graphsDir, "doc.json")).Should().NotContain("a -> m1");
        BrainToolchain.Verify("doc").Ok.Should().BeTrue();

        // cervello sano → nessun fix (nessun effetto collaterale)
        MakeGraphBrain("doc2");
        BrainToolchain.Doctor("doc2").Text.Should().Contain("nessun problema");
    }

    // ── BACKUP ────────────────────────────────────────────────────────────
    [Fact(DisplayName = "BACKUP: produce uno zip .gmbrain con timestamp e manifest")]
    public void BackupCreatesZip()
    {
        MakeGraphBrain("bk");
        var outZip = Path.Combine(_brainsRoot, "bk.gmbrain.zip");
        var r = BrainToolchain.Backup("bk", outZip);
        r.Ok.Should().BeTrue(r.Error ?? r.Text);
        File.Exists(outZip).Should().BeTrue();
        r.Text.Should().Contain("backup:");

        var auto = BrainToolchain.Backup("bk");     // nome con timestamp in brains-exports/
        auto.Ok.Should().BeTrue(auto.Error ?? auto.Text);
        auto.Text.Should().Contain(".gmbrain.zip");
    }
}
