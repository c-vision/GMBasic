// FILE: GMBASIC.IlBackend.Tests.GraphAlgorithmsTests
// Test degli algoritmi canonici del tipo Graph<TNode> (CommonLib.Collections).
// Grafo noto: a→b, a→c, b→d, c→d, d→e (path a→e = a,b,d,e o a,c,d,e; distanza 3).
// Usato come oracolo per la parità con gmb_graph.c del runtime C.
using CommonLib.Collections;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class GraphAlgorithmsTests
{
    private static Graph<string> MakeChain()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c", "d", "e" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("a", "c", 2);
        g.AddEdge("b", "d", 3);
        g.AddEdge("c", "d", 1);
        g.AddEdge("d", "e", 2);
        return g;
    }

    [Fact]
    public void Degrees_AreCorrect()
    {
        var g = MakeChain();
        Assert.Equal(2, g.GetOutDegree("a"));
        Assert.Equal(0, g.GetInDegree("a"));
        Assert.Equal(2, g.GetDegree("a"));
        Assert.Equal(0, g.GetOutDegree("e"));
        Assert.Equal(1, g.GetInDegree("e"));
        Assert.Equal(-1, g.GetDegree("x"));
    }

    [Fact]
    public void ShortestPath_Bfs_MinEdges()
    {
        Assert.Equal(new[] { "a", "b", "d", "e" }, MakeChain().ShortestPath("a", "e"));
    }

    [Fact]
    public void DijkstraPath_MinCost()
    {
        // a→c(2)→d(1)→e(2) = 5 vs a→b(1)→d(3)→e(2) = 6
        Assert.Equal(new[] { "a", "c", "d", "e" }, MakeChain().DijkstraPath("a", "e"));
    }

    [Fact]
    public void FindPath_Strategies()
    {
        var g = MakeChain();
        Assert.Equal(4, g.FindPath("a", "e", "bfs").Count);
        Assert.Equal(new[] { "a", "c", "d", "e" }, g.FindPath("a", "e", "dijkstra"));
        Assert.Equal(new[] { "a", "c", "d", "e" }, g.FindPath("a", "e", "astar"));
        Assert.Equal(new[] { "a", "c", "d", "e" }, g.FindPath("a", "e", "x"));
    }

    [Fact]
    public void BellmanFord_WithNegative_Handled()
    {
        var g = new Graph<string>();
        g.AddNode("a"); g.AddNode("b"); g.AddNode("c");
        g.AddEdge("a", "b", -1);
        g.AddEdge("b", "c", 3);
        g.AddEdge("c", "a", -2);
        var (dist, neg) = g.BellmanFord("a");
        Assert.False(neg);
        Assert.Equal(-1, dist["b"]);
        Assert.Equal(2, dist["c"]);
    }

    [Fact]
    public void BellmanFord_NegativeCycle_Detected()
    {
        var g = new Graph<string>();
        g.AddNode("a"); g.AddNode("b");
        g.AddEdge("a", "b", -5);
        g.AddEdge("b", "a", 1);
        var (_, neg) = g.BellmanFord("a");
        Assert.True(neg);
    }

    [Fact]
    public void ConnectedComponents_Weak()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c", "x", "y" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "c", 1);
        g.AddEdge("x", "y", 1);
        var comps = g.ConnectedComponents();
        Assert.Equal(2, comps.Count);
        Assert.Contains(comps[0], n => n == "a");
        Assert.Contains(comps[0], n => n == "b");
        Assert.Contains(comps[0], n => n == "c");
    }

    [Fact]
    public void StronglyConnectedComponents_Tarjan()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "a", 1);
        g.AddEdge("c", "a", 1);
        var scc = g.StronglyConnectedComponents();
        Assert.Contains(scc, c => c.Contains("a") && c.Contains("b"));
        Assert.Contains(scc, c => c.Contains("c"));
    }

    [Fact]
    public void HasCycle_Detects()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "c", 1);
        Assert.False(g.HasCycle());
        g.AddEdge("c", "a", 1);
        Assert.True(g.HasCycle());
    }

    [Fact]
    public void TopologicalSort_Linear()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "c", 1);
        var topo = g.TopologicalSort();
        Assert.NotNull(topo);
        Assert.Equal(new[] { "a", "b", "c" }, topo);
        g.AddEdge("c", "a", 1);
        Assert.Null(g.TopologicalSort());
    }

    [Fact]
    public void IsBipartite_Works()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "c", 1);
        Assert.True(g.IsBipartite());
        g.AddEdge("c", "a", 1);
        Assert.False(g.IsBipartite());
    }

    [Fact]
    public void PageRank_HubIsTop()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "hub", "l1", "l2" }) g.AddNode(n);
        g.AddEdge("hub", "l1", 1);
        g.AddEdge("hub", "l2", 1);
        g.AddEdge("l1", "hub", 1);
        g.AddEdge("l2", "hub", 1);
        var pr = g.PageRank();
        Assert.Equal("hub", pr[0].Node);
    }

    [Fact]
    public void Diameter_Eccentricity()
    {
        // Diameter è sui PESI (FloydWarshall): a→e minimo = 5 (via c: 2+1+2). Eccentricity in archi = 3.
        Assert.Equal(5, MakeChain().Diameter());
        Assert.Equal(3, MakeChain().Eccentricity("a"));
    }

    [Fact]
    public void ClusteringCoefficient_Triangle()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("a", "c", 1);
        g.AddEdge("b", "c", 1);
        Assert.Equal(1.0, g.ClusteringCoefficient("a"));
    }

    [Fact]
    public void MinimumSpanningTree_Kruskal()
    {
        var g = new Graph<string>();
        foreach (var n in new[] { "a", "b", "c", "d" }) g.AddNode(n);
        g.AddEdge("a", "b", 1);
        g.AddEdge("b", "c", 2);
        g.AddEdge("c", "d", 3);
        g.AddEdge("a", "d", 10);
        var mst = g.MinimumSpanningTree();
        Assert.Equal(3, mst.Count);
        Assert.Equal(6, mst.Sum(e => e.Weight));
    }

    [Fact]
    public void FloydWarshall_AllPairs()
    {
        var fw = MakeChain().FloydWarshall();
        Assert.Equal(5, fw["a"]["e"]);
    }
}
