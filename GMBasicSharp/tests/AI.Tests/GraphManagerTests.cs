// FILE: AI.Tests.GraphManagerTests
// L5 — GraphManager: costruzione del knowledge graph arricchito (8 campi + edge tipizzati
// weighted) da un vault .md, persistenza JSON round-trip, e le primitive di interrogazione
// (ShortestPath/ConnectedComponents). Usa la fixture samples/Fixtures/graph-vault.
using Basic.AI;
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class GraphManagerTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _tmp = Path.Combine(Path.GetTempPath(), "gmbasic-graphmgr-" + Guid.NewGuid().ToString("N"));

    public GraphManagerTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        GraphManager.ActiveGraph = null;
        GraphManager.ActiveName = null;
        GraphManager.ActiveDir = null;
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        GraphManager.ActiveGraph = null;
        GraphManager.ActiveName = null;
        GraphManager.ActiveDir = null;
        try { Directory.Delete(_tmp, recursive: true); } catch { }
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Path della fixture vault condivisa (samples/Fixtures/graph-vault), relativo
    /// alla cartella di lavoro del test (repo root).</summary>
    private static string VaultFixture()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null && !Directory.Exists(Path.Combine(dir.FullName, "samples", "Fixtures")))
            dir = dir.Parent;
        Assert.NotNull(dir);
        return Path.Combine(dir.FullName, "samples", "Fixtures", "graph-vault");
    }

    [Fact]
    public void Build_PopulatesEightFields()
    {
        var g = GraphManager.Build(VaultFixture());
        g.NodeCount.Should().Be(5);

        var node = g.Nodes["strategies/trend-following.md"];
        node.Title.Should().Be("trend-following"); // TitleOf usa il filename (non il frontmatter title)
        node.Folder.Should().Be("strategies");
        node.Tags.Should().Contain("strategy").And.Contain("trend");
        node.Author.Should().Be("gmb");
        node.Date.Should().Be("2026-08-01");
        node.Strategy.Should().Be("trend");
        node.Asset.Should().Be("equity");
        node.Linked.Should().Contain("trading/momentum.md");

        var airline = g.Nodes["airline/load-factor.md"];
        airline.Concepts.Should().Contain("load");
        airline.Folder.Should().Be("airline");
    }

    [Fact]
    public void Build_TypedEdges()
    {
        var g = GraphManager.Build(VaultFixture());

        // LINK: wikilink risolti, peso 1.0.
        g.Edges.Should().Contain(e =>
            e.Type == GraphEdgeType.Link && e.Source == "index.md" && e.Target == "trading/momentum.md" && e.Weight == 1.0);

        // TAG_SHARE: Jaccard ≥ 0.5 — index (hub,overview) vs ... nessun tag condiviso oltre soglia.
        // trend-following (strategy,trend) e momentum (trading,momentum) → 0 condivisi.
        g.Edges.Should().NotContain(e => e.Type == GraphEdgeType.TagShare && e.Weight < 0.5);

        // FOLDER_CHILD: co-locazione in strategies → trend-following ↔ mean-reversion peso 0.8.
        g.Edges.Should().Contain(e =>
            e.Type == GraphEdgeType.FolderChild && e.Weight == 0.8 &&
            ((e.Source == "strategies/trend-following.md" && e.Target == "strategies/mean-reversion.md") ||
             (e.Source == "strategies/mean-reversion.md" && e.Target == "strategies/trend-following.md")));

        // CONTENT_MENTION: "trend-following" (title=filename di trend-following) appare nel body
        // di mean-reversion senza essere un wikilink.
        g.Edges.Should().Contain(e =>
            e.Type == GraphEdgeType.ContentMention && e.Weight == 0.6 &&
            e.Source == "strategies/mean-reversion.md" && e.Target == "strategies/trend-following.md");

        // TEMPORAL_NEAR: date a ≤7 giorni (index 08-01 ↔ trend 08-01, momentum 08-02, mean 08-03).
        g.Edges.Should().Contain(e => e.Type == GraphEdgeType.TemporalNear && e.Weight > 0);
    }

    [Fact]
    public void ShortestPath_FindsPath()
    {
        var g = GraphManager.Build(VaultFixture());
        var path = g.ShortestPath("index.md", "trading/momentum.md");
        path.Should().NotBeEmpty();
        path[0].Should().Be("index.md");
        path[^1].Should().Be("trading/momentum.md");
    }

    [Fact]
    public void ConnectedComponents_SplitsIsolatedNodes()
    {
        var g = GraphManager.Build(VaultFixture());
        var comps = g.ConnectedComponents();
        comps.Should().NotBeEmpty();
        comps.Should().Contain(c => c.Contains("index.md"));
        // Load Factor (airline) è isolato (dangling) → componente separata.
        comps.Should().Contain(c => c.Contains("airline/load-factor.md"));
    }

    [Fact]
    public void JsonRoundTrip_PreservesNodesAndEdges()
    {
        var g = GraphManager.Build(VaultFixture());
        var json = GraphManager.ToJson(g);
        var back = GraphManager.LoadFromJson(json);

        back.NodeCount.Should().Be(g.NodeCount);
        back.EdgeCount.Should().Be(g.EdgeCount);
        back.Nodes["index.md"].Folder.Should().Be("");
        back.Nodes["strategies/trend-following.md"].Strategy.Should().Be("trend");
        back.Edges.Should().Contain(e => e.Type == GraphEdgeType.Link && e.Weight == 1.0);
        back.Edges.Should().Contain(e => e.Type == GraphEdgeType.FolderChild && e.Weight == 0.8);
    }

    [Fact]
    public void SaveLoad_FileRoundTrip()
    {
        Directory.CreateDirectory(_tmp);
        var g = GraphManager.Build(VaultFixture());
        GraphManager.Save(g, _tmp, "test");

        File.Exists(Path.Combine(_tmp, "test.graph.json")).Should().BeTrue();
        var loaded = GraphManager.TryLoad(_tmp, "test");
        loaded.Should().NotBeNull();
        loaded!.NodeCount.Should().Be(g.NodeCount);
    }

    [Fact]
    public void ParseBackend_MapsKnownValues()
    {
        GraphManager.ParseBackend("memory").Should().Be(GraphRddBackend.Memory);
        GraphManager.ParseBackend("JSON").Should().Be(GraphRddBackend.Json);
        GraphManager.ParseBackend("sqlite").Should().Be(GraphRddBackend.Sqlite);
        GraphManager.ParseBackend("isam").Should().Be(GraphRddBackend.Isam);
        GraphManager.ParseBackend("dbf").Should().Be(GraphRddBackend.Dbf);
        GraphManager.ParseBackend("boh").Should().Be(GraphRddBackend.Memory);
    }

    [Fact]
    public void SqliteRoundTrip_PreservesNodesAndTypedEdges()
    {
        Directory.CreateDirectory(_tmp);
        var g = GraphManager.Build(VaultFixture());
        GraphManager.SaveSqlite(g, _tmp, "testsql");

        File.Exists(GraphManager.SqlitePath(_tmp, "testsql")).Should().BeTrue();
        var loaded = GraphManager.TryLoadSqlite(_tmp, "testsql");
        loaded.Should().NotBeNull();
        loaded!.NodeCount.Should().Be(g.NodeCount);
        loaded.EdgeCount.Should().Be(g.EdgeCount);
        // 8 campi preservati (es. folder/strategy) e edge tipizzati con peso.
        loaded.Nodes["strategies/trend-following.md"].Strategy.Should().Be("trend");
        loaded.Nodes["strategies/trend-following.md"].Folder.Should().Be("strategies");
        loaded.Edges.Should().Contain(e => e.Type == GraphEdgeType.FolderChild && e.Weight == 0.8);
        loaded.Edges.Should().Contain(e => e.Type == GraphEdgeType.Link && e.Weight == 1.0);
    }

    [Fact]
    public void SqliteRoundTrip_ShortestPath()
    {
        Directory.CreateDirectory(_tmp);
        var g = GraphManager.Build(VaultFixture());
        GraphManager.SaveSqlite(g, _tmp, "testsql");
        var loaded = GraphManager.TryLoadSqlite(_tmp, "testsql");
        loaded.Should().NotBeNull();
        var path = loaded!.ShortestPath("index.md", "trading/momentum.md");
        path.Should().NotBeEmpty();
        path[0].Should().Be("index.md");
        path[^1].Should().Be("trading/momentum.md");
    }
}
