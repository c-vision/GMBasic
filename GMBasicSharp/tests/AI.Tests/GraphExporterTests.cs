// FILE: AI.Tests.GraphExporterTests
// F7 P1 — GraphExporter esteso: ToDot, ToSubgraphMermaid, ToJsonByTag, ToJsonByFolder, IsValid.
// Usa un vault con frontmatter tags/wikilink costruito su fixture, isolato da config utente reale.
using Basic.AI;
using Basic.AI.Graph;
using Basic.AI.Knowledge;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class GraphExporterTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _graphsDir = Path.Combine(Path.GetTempPath(), "gmbasic-gx-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-gx-" + Guid.NewGuid().ToString("N") + ".json");

    public GraphExporterTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.GraphsDefaultDir = _graphsDir;
        BrainRegistry.OverridePathForTests(_registryPath);
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_graphsDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Costruisce un vault con wikilink + frontmatter tags, genera il graph JSON e
    /// registra il brain.</summary>
    private void MakeGraphBrain(string name, Dictionary<string, (string[] Links, string[] Tags)> files)
    {
        var vault = Path.Combine(_graphsDir, name + "-vault");
        Directory.CreateDirectory(vault);
        foreach (var (rel, (links, tags)) in files)
        {
            var full = Path.Combine(vault, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(full)!);
            var body = string.Join("\n", links.Select(l => $"[[{l}]]"));
            var tagLine = tags.Length > 0 ? "tags: [" + string.Join(", ", tags) + "]\n" : "";
            File.WriteAllText(full, $"---\ntitle: {Path.GetFileNameWithoutExtension(rel)}\n{tagLine}---\n{body}\n");
        }
        var g = GraphStore.Build(vault);
        var export = GraphExporter.Export(g, name);
        File.WriteAllText(Path.Combine(_graphsDir, name + ".json"), export.Json);
        BrainRegistry.Register(name, mode: "graph");
    }

    private static WikiGraph BuildSample()
    {
        var vault = Path.Combine(Path.GetTempPath(), "gmbasic-gx-vault-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(vault, "strategies"));
        File.WriteAllText(Path.Combine(vault, "index.md"),
            "---\ntitle: Index\ntags: [hub, overview]\n---\nVedi [[strategies/trend]].\n");
        File.WriteAllText(Path.Combine(vault, "strategies", "trend.md"),
            "---\ntitle: Trend\ntags: [strategy, trend]\n---\nLinka [[index]].\n");
        File.WriteAllText(Path.Combine(vault, "strategies", "mean.md"),
            "---\ntitle: Mean\ntags: [strategy, mean]\n---\nControparte di trend.\n");
        try { return GraphStore.Build(vault); }
        finally { try { Directory.Delete(vault, recursive: true); } catch { } }
    }

    [Fact]
    public void ToDot_EmitsGraphvizHeaderAndEdges()
    {
        var g = BuildSample();
        var dot = GraphExporter.ToDot(g, "test");
        dot.Should().Contain("digraph test");
        dot.Should().Contain("rankdir=LR");
        dot.Should().Contain("\"index.md\" -> \"strategies/trend.md\"");
        dot.Should().Contain("\"strategies/trend.md\" -> \"index.md\"");
    }

    [Fact]
    public void ToSubgraphMermaid_IncludesNeighborsWithinDepth()
    {
        var g = BuildSample();
        var m = GraphExporter.ToSubgraphMermaid(g, "index.md", 1);
        m.Should().Contain("graph TD");
        m.Should().Contain("Nindex_md");
        m.Should().Contain("Nstrategies_trend_md");
        // depth 1: i vicini diretti, non il loro vicinato
        m.Should().Contain("Nindex_md --> Nstrategies_trend_md");
    }

    [Fact]
    public void ToJsonByTag_FiltersNodesAndEdges()
    {
        var g = BuildSample();
        var json = GraphExporter.ToJsonByTag(g, "strategy");
        json.Should().Contain("\"tag\": \"strategy\"");
        json.Should().Contain("strategies/trend.md");
        json.Should().Contain("strategies/mean.md");
        json.Should().NotContain("index.md");
        json.Should().Contain("\"count\": 2");
    }

    [Fact]
    public void ToJsonByFolder_FiltersBySubtree()
    {
        var g = BuildSample();
        var json = GraphExporter.ToJsonByFolder(g, "strategies");
        json.Should().Contain("\"folder\": \"strategies\"");
        json.Should().Contain("strategies/trend.md");
        json.Should().Contain("strategies/mean.md");
        json.Should().NotContain("index.md");
    }

    [Fact]
    public void IsValid_EmptyGraph_ReturnsFalse()
    {
        GraphExporter.IsValid(new WikiGraph()).Should().BeFalse();
        GraphExporter.IsValid(BuildSample()).Should().BeTrue();
    }

    [Fact]
    public void GraphMultiQuery_ExportFunctions_WorkOnBrain()
    {
        MakeGraphBrain("gb", new Dictionary<string, (string[], string[])>
        {
            ["alpha.md"] = (["beta"], ["strategy"]),
            ["beta.md"] = (["alpha"], ["strategy"]),
        });
        GraphMultiQuery.Dot("gb").Should().Contain("digraph gb");
        GraphMultiQuery.Subgraph("gb", "alpha.md", 1).Should().Contain("Nalpha_md");
        GraphMultiQuery.ByTag("gb", "strategy").Should().Contain("\"count\": 2");
        GraphMultiQuery.ByFolder("gb", "").Should().Contain("alpha.md");
        GraphMultiQuery.IsValid("gb").Should().Be(1.0);
        GraphMultiQuery.Dot("missing").Should().Contain("\"Error\"");
    }
}
