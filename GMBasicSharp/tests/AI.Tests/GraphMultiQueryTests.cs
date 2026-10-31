// FILE: AI.Tests.GraphMultiQueryTests
// GRAPH.QUERYMULTI (R7): merge dei graph dei brain usati, dedup nodi per path, query per
// path/title + vicinato 1-hop con provenance brain. Usa GraphStore.Build per generare graph
// JSON reali da vault fixture, isolati da config utente reale.
using Basic.AI;
using Basic.AI.Graph;
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class GraphMultiQueryTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N"));
    private readonly string _graphsDir = Path.Combine(Path.GetTempPath(), "gmbasic-graphs-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N") + ".json");

    public GraphMultiQueryTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.GraphsDefaultDir = _graphsDir;
        BrainRegistry.OverridePathForTests(_registryPath);
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        try { Directory.Delete(_graphsDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Costruisce un vault con wikilink e genera il graph JSON in GraphsDefaultDir,
    /// poi lo registra come brain disponibile (IsAvailableAsGraph).</summary>
    private void MakeGraphBrain(string name, Dictionary<string, string[]> files)
    {
        var vault = Path.Combine(_graphsDir, name + "-vault");
        Directory.CreateDirectory(vault);
        foreach (var (rel, links) in files)
        {
            var full = Path.Combine(vault, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(full)!);
            var body = string.Join("\n", links.Select(l => $"[[{l}]]"));
            File.WriteAllText(full, $"---\ntitle: {Path.GetFileNameWithoutExtension(rel)}\n---\n{body}\n");
        }
        var g = GraphStore.Build(vault);
        var export = GraphExporter.Export(g, name);
        File.WriteAllText(Path.Combine(_graphsDir, name + ".json"), export.Json);
        BrainRegistry.Register(name, mode: "graph");
    }

    [Fact]
    public void QueryMulti_NoBrains_ReturnsError()
    {
        var res = GraphMultiQuery.QueryMulti("prova");
        res.Should().Contain("nessun brain collegato");
    }

    [Fact]
    public void QueryMulti_ExactNode_ReturnsNeighbors_WithBrain()
    {
        MakeGraphBrain("ga", new Dictionary<string, string[]>
        {
            ["alpha.md"] = ["beta"],
            ["beta.md"] = ["alpha"],
        });
        BrainRegistry.Use("ga");

        var res = GraphMultiQuery.QueryMulti("alpha.md", ["ga"]);

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("alpha.md");
        res.Should().Contain("beta.md");
        res.Should().Contain("\"brain\":\"ga\"");
    }

    [Fact]
    public void QueryMulti_TwoBrains_ReturnsHitsFromBoth()
    {
        MakeGraphBrain("gb", new Dictionary<string, string[]>
        {
            ["momentum.md"] = ["risk"],
        });
        MakeGraphBrain("gc", new Dictionary<string, string[]>
        {
            ["value.md"] = ["intrinsic"],
            ["intrinsic.md"] = [],
        });
        BrainRegistry.Use("gb");
        BrainRegistry.Use("gc");

        // query per sottostringa: "value" → solo in gc
        var res = GraphMultiQuery.QueryMulti("value", ["gb", "gc"]);
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("value.md");
        res.Should().Contain("\"brain\":\"gc\"");
        // non deve contenere hit del solo gb che non parla di value
        res.Should().NotContain("momentum.md");
    }

    [Fact]
    public void QueryMulti_NoMatch_ReturnsInsufficient()
    {
        MakeGraphBrain("gd", new Dictionary<string, string[]>
        {
            ["x.md"] = [],
        });
        BrainRegistry.Use("gd");

        var res = GraphMultiQuery.QueryMulti("zzz-nothing", ["gd"]);
        res.Should().Contain("InsufficientEvidence");
    }

    // ── Primitive single-brain (L5): NEIGHBORS/PATH/PAGERANK/CONNECTED/STATS ──

    [Fact]
    public void Neighbors_ReturnsDirectLinks()
    {
        MakeGraphBrain("gn", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b", "c"],
            ["b.md"] = ["a"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Neighbors("gn", "a.md");

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("b.md");
        res.Should().Contain("c.md");
    }

    [Fact]
    public void Neighbors_UnknownNode_ReturnsError()
    {
        MakeGraphBrain("gne", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = [],
        });

        var res = GraphMultiQuery.Neighbors("gne", "zzz.md");
        res.Should().Contain("\"status\":\"Error\"");
    }

    [Fact]
    public void Path_Shortest_ReturnsChain()
    {
        MakeGraphBrain("gp", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Path("gp", "a.md", "c.md");

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("a.md");
        res.Should().Contain("b.md");
        res.Should().Contain("c.md");
    }

    [Fact]
    public void Path_Unreachable_ReturnsInsufficient()
    {
        MakeGraphBrain("gpu", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = [],
            ["isolated.md"] = [],
        });

        var res = GraphMultiQuery.Path("gpu", "a.md", "isolated.md");
        res.Should().Contain("InsufficientEvidence");
    }

    [Fact]
    public void PageRank_ReturnsRankedNodes()
    {
        MakeGraphBrain("gpr", new Dictionary<string, string[]>
        {
            ["hub.md"] = ["leaf1", "leaf2"],
            ["leaf1.md"] = ["hub"],
            ["leaf2.md"] = ["hub"],
        });

        var res = GraphMultiQuery.PageRank("gpr", 5);

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("hub.md");
        res.Should().Contain("\"rank\"");
    }

    [Fact]
    public void Connected_ReturnsComponents()
    {
        MakeGraphBrain("gc2", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["a"],
            ["solo.md"] = [],
        });

        var res = GraphMultiQuery.Connected("gc2");

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"componentCount\":2");
        res.Should().Contain("solo.md");
    }

    [Fact]
    public void Stats_ReturnsCounts()
    {
        MakeGraphBrain("gst", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["a"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Stats("gst");

        res.Should().Contain("\"nodes\":3");
        res.Should().Contain("\"edges\":2");
    }

    [Fact]
    public void SingleBrain_UnknownBrain_ReturnsError()
    {
        var res = GraphMultiQuery.Stats("ghost");
        res.Should().Contain("\"status\":\"Error\"");
    }

    [Fact(DisplayName = "GRAPH.DIJKSTRA: path a costo minimo tra due nodi")]
    public void Dijkstra_ReturnsPath()
    {
        MakeGraphBrain("gdij", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Dijkstra("gdij", "a.md", "c.md");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"path\"");
        res.Should().Contain("a.md");
        res.Should().Contain("c.md");
    }

    [Fact(DisplayName = "GRAPH.DIJKSTRA: nodo non raggiungibile → path vuoto, non errore")]
    public void Dijkstra_Unreachable_ReturnsEmptyPath()
    {
        MakeGraphBrain("gdju", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = [],
            ["isolated.md"] = [],
        });

        var res = GraphMultiQuery.Dijkstra("gdju", "a.md", "isolated.md");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"path\":[]");
    }

    [Fact(DisplayName = "GRAPH.HASCYCLE: 1 con ciclo, 0 senza (BitType)")]
    public void HasCycle_DetectsCycle()
    {
        MakeGraphBrain("ghc1", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = ["a"],
        });
        GraphMultiQuery.HasCycle("ghc1").Should().Be(1.0);

        MakeGraphBrain("ghc0", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });
        GraphMultiQuery.HasCycle("ghc0").Should().Be(0.0);
    }

    [Fact(DisplayName = "GRAPH.ECCENTRICITY: eccentricità del nodo (distanza max)")]
    public void Eccentricity_ReturnsValue()
    {
        MakeGraphBrain("gecc", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Eccentricity("gecc", "a.md");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"eccentricity\":2");
    }

    [Fact(DisplayName = "GRAPH.DFS: visita in profondità dal root")]
    public void Dfs_ReturnsOrder()
    {
        MakeGraphBrain("gdfs", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Dfs("gdfs", "a.md");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"order\"");
        res.Should().Contain("a.md");
        res.Should().Contain("b.md");
        res.Should().Contain("c.md");
    }

    [Fact(DisplayName = "GRAPH.BFS: visita in ampiezza dal root")]
    public void Bfs_ReturnsOrder()
    {
        MakeGraphBrain("gbfs", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphMultiQuery.Bfs("gbfs", "a.md");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"order\"");
        res.Should().Contain("a.md");
        res.Should().Contain("b.md");
        res.Should().Contain("c.md");
    }
}
