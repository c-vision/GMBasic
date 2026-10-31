// FILE: AI.Tests.KnowledgeGraphTests
using Basic.AI.Knowledge;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class KnowledgeGraphTests : IDisposable
{
    public KnowledgeGraphTests() => KnowledgeGraphRuntime.Reset();
    public void Dispose() => KnowledgeGraphRuntime.Reset();

    [Fact]
    public void AddNodeAddEdge_Then_NeighborsAndShortestPath()
    {
        KnowledgeGraphRuntime.AddNode("alice");
        KnowledgeGraphRuntime.AddNode("bob");
        KnowledgeGraphRuntime.AddNode("carol");
        KnowledgeGraphRuntime.AddEdge("alice", "bob");
        KnowledgeGraphRuntime.AddEdge("bob", "carol");

        var nb = KnowledgeGraphRuntime.Query("alice");
        Assert.Contains("bob", nb);

        var path = KnowledgeGraphRuntime.Query("path alice->carol");
        Assert.Contains("alice", path);
        Assert.Contains("bob", path);
        Assert.Contains("carol", path);
    }

    [Fact]
    public void Query_MissingNode_ReturnsExplicitError()
    {
        var res = KnowledgeGraphRuntime.Query("ghost");
        Assert.Contains("nodo non trovato", res);
    }

    [Fact]
    public void AddEdge_IsIdempotent()
    {
        KnowledgeGraphRuntime.AddEdge("x", "y");
        KnowledgeGraphRuntime.AddEdge("x", "y");
        var nb = KnowledgeGraphRuntime.Query("x");
        using var doc = System.Text.Json.JsonDocument.Parse(nb);
        var neighbors = doc.RootElement.GetProperty("neighbors");
        Assert.Equal(1, neighbors.EnumerateArray().Count(e => e.GetString() == "y"));
    }
}
