// FILE: Basic.AI.Knowledge.KnowledgeGraphRuntime
// B3.5 — KNOWLEDGEGRAPH.ADDNODE/ADDEDGE/QUERY: runtime mutabile su RichGraph (GraphManager).
// Esposto a BASIC come AI.KGADDNODE/AI.KGADDEDGE/AI.KGQUERY (namespace AI, funzioni).
// Singleton in-process; mai stub: errori espliciti su argomenti invalidi.

using System.Text.Json;

namespace Basic.AI.Knowledge;

public static class KnowledgeGraphRuntime
{
    private static readonly object Gate = new();
    private static RichGraph? _graph;

    public static RichGraph Graph
    {
        get { lock (Gate) return _graph ??= NewGraph(); }
    }

    private static RichGraph NewGraph()
    {
        var g = new RichGraph();
        return g;
    }

    /// <summary>Reset del grafo in-memory (test).</summary>
    public static void Reset() { lock (Gate) _graph = NewGraph(); }

    /// <summary>ADDNODE: garantisce che il nodo esista (default title=folder=nome). Ritorna 1.</summary>
    public static double AddNode(string node)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(node);
        var g = Graph;
        var key = Normalize(node);
        if (g.Nodes.ContainsKey(key)) return 1;
        g.Nodes[key] = new GraphNode(
            key, key, "", [], null, null, null, null, [], []);
        return 1;
    }

    /// <summary>ADDEDGE a→b (edge tipizzato "knows" pesato 1). Ritorna 1.</summary>
    public static double AddEdge(string from, string to)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(from);
        ArgumentException.ThrowIfNullOrWhiteSpace(to);
        var g = Graph;
        var a = Normalize(from); var b = Normalize(to);
        AddNode(a); AddNode(b);
        // evita duplicati
        if (g.Edges.Any(e => e.Source == a && e.Target == b)) return 1;
        g.Edges.Add(new GraphEdge(a, b, GraphEdgeType.Link, 1.0));
        return 1;
    }

    /// <summary>QUERY: JSON {node, neighbors:[...], incident:[...]} o {from,to,path:[...]} per "path a→b".</summary>
    public static string Query(string query)
    {
        if (string.IsNullOrWhiteSpace(query)) return "{\"error\":\"query vuota\"}";
        var g = Graph;
        var lower = query.ToLowerInvariant();
        try
        {
            if (lower.StartsWith("path ", StringComparison.Ordinal))
            {
                var parts = query.Substring(5).Split(new[] { "->", "→" }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length != 2) return "{\"error\":\"path richiede 'path a->b'\"}";
                var a = Normalize(parts[0].Trim()); var b = Normalize(parts[1].Trim());
                var path = g.ShortestPath(a, b);
                return JsonSerializer.Serialize(new { from = a, to = b, path });
            }
            var n = Normalize(query);
            if (!g.Nodes.ContainsKey(n))
                return "{\"node\":\"" + n + "\",\"error\":\"nodo non trovato\"}";
            var neighbors = g.Neighbors(n);
            var incident = g.Incident(n).Select(e => new { src = e.Source, tgt = e.Target, w = e.Weight }).ToArray();
            return JsonSerializer.Serialize(new { node = n, neighbors, incident });
        }
        catch (Exception ex) { return "{\"error\":\"" + ex.Message.Replace("\"", "\\\"") + "\"}"; }
    }

    private static string Normalize(string s) => s.Trim().TrimStart('/');
}
