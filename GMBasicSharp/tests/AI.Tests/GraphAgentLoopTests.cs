// FILE: AI.Tests.GraphAgentLoopTests
// GraphAgentLoop (2026-09-07): tool graph.* esposti agli agenti (query/nodi/vicini/visite).
// Il dispatch è NATIVO C# verso GraphMultiQuery — testa Execute su un brain graph reale
// costruito dalla fixture, più la registrazione dei tool nel registry MCP.
using Basic.AI;
using Basic.AI.Graph;
using Basic.AI.Knowledge;
using Basic.Agents;
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class GraphAgentLoopTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _graphsDir = Path.Combine(Path.GetTempPath(), "gmbasic-graphs-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N") + ".json");

    public GraphAgentLoopTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.GraphsDefaultDir = _graphsDir;
        BrainRegistry.OverridePathForTests(_registryPath);
        McpToolRegistry.ResetForTests();
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        McpToolRegistry.ResetForTests();
        try { Directory.Delete(_graphsDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

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

    [Fact(DisplayName = "GraphAgentLoop.Register: espone i tool graph.* nel registry")]
    public void Register_ExposesGraphTools()
    {
        GraphAgentLoop.Register();
        var names = McpToolRegistry.Instance.List().Select(t => t.Name).ToList();
        names.Should().Contain("graph.query");
        names.Should().Contain("graph.neighbors");
        names.Should().Contain("graph.path");
        names.Should().Contain("graph.dijkstra");
        names.Should().Contain("graph.bfs");
        names.Should().Contain("graph.dfs");
        names.Should().Contain("graph.stats");
    }

    [Fact(DisplayName = "GraphAgentLoop.Execute: graph.path ritorna il percorso (JSON)")]
    public void Execute_Path_ReturnsJson()
    {
        MakeGraphBrain("gag", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = ["c"],
            ["c.md"] = [],
        });

        var res = GraphAgentLoop.Execute("graph.path", """{"brain":"gag","from":"a.md","to":"c.md"}""");
        res.Should().NotStartWith("error:");
        res.Should().Contain("a.md");
        res.Should().Contain("c.md");
    }

    [Fact(DisplayName = "GraphAgentLoop.Execute: graph.dijkstra su nodo non raggiungibile → path vuoto")]
    public void Execute_Dijkstra_Unreachable_EmptyPath()
    {
        MakeGraphBrain("gagu", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = [],
            ["isolated.md"] = [],
        });

        var res = GraphAgentLoop.Execute("graph.dijkstra", """{"brain":"gagu","from":"a.md","to":"isolated.md"}""");
        res.Should().Contain("\"path\":[]");
    }

    [Fact(DisplayName = "GraphAgentLoop.Execute: tool sconosciuto → errore esplicito")]
    public void Execute_UnknownTool_ReturnsError()
    {
        var res = GraphAgentLoop.Execute("graph.nope", "{}");
        res.Should().StartWith("error:");
    }

    [Fact(DisplayName = "GraphAgentLoop.Execute: argomenti mancanti → errore esplicito")]
    public void Execute_MissingArgs_ReturnsError()
    {
        var res = GraphAgentLoop.Execute("graph.path", "{}");
        res.Should().StartWith("error:");
    }

    [Fact(DisplayName = "GraphAgentLoop.Dispatcher: McpToolCallResult coerente")]
    public void Dispatcher_ReturnsToolCallResult()
    {
        MakeGraphBrain("gagd", new Dictionary<string, string[]>
        {
            ["a.md"] = ["b"],
            ["b.md"] = [],
        });

        var dispatcher = new GraphAgentLoop.Dispatcher();
        var result = dispatcher.DispatchAsync("graph.stats", """{"brain":"gagd"}""", CancellationToken.None)
            .GetAwaiter().GetResult();
        result.IsError.Should().BeFalse();
        result.TextContent.Should().Contain("nodes");
    }
}
