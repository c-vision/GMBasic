// FILE: Basic.AI.Graph.GraphMultiQuery
// Second Brain — graph multi-use (R7): carica il grafo JSON (<graphs/<name>.json>) di ogni
// brain collegato (BrainRegistry.Used), fonde i nodi (dedup per path), e interroga l'unione.
// Query matcha per path/title (sottostringa) o, se query è un path esatto, ritorna il vicinato
// 1-hop del nodo (incoming/outgoing edges) con provenance brain (quale brain contiene il nodo).
// Utile per interrogazioni cross-brain: "dove si parla di X in tutti i miei vault?"
using System.Text.Json;

namespace Basic.AI.Graph;

public static class GraphMultiQuery
{
    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = false,
    };

    /// <summary>Query su tutti i brain graph usati (o specificati). Ritorna JSON con
    /// {query, matches:[{path, title, brain, neighbors:[{to, alias, brain}]}]}.
    /// Se query matcha un path esatto, ritorna il nodo + tutti i vicini (1-hop).
    /// Se query non matcha esatto, ricerca per sottostringa in path/title.</summary>
    public static string QueryMulti(string query, string[]? brains = null, Basic.AI.Knowledge.BrainScope? scope = null)
    {
        // F4 — default = set effettivo dello scope; senza scope = PROCESS = brain attivati (invariato).
        var names = brains is { Length: > 0 } ? brains
            : scope is not null ? scope.Effective().Select(r => r.Name).ToArray()
            : Basic.AI.Knowledge.BrainRegistry.Used();
        if (names.Length == 0)
            return "{\"status\":\"InsufficientEvidence\",\"error\":\"nessun brain collegato (AI BRAIN USE)\",\"matches\":[]}";

        // merge: {brain → nodi}
        var allNodes = new Dictionary<string, List<(string Brain, WikiNode Node)>>(StringComparer.Ordinal);
        var allEdges = new List<(string Brain, WikiEdge Edge)>();

        foreach (var name in names)
        {
            var jsonPath = ResolveGraphJsonPath(name);
            if (jsonPath is null || !File.Exists(jsonPath)) continue;
            try
            {
                var doc = JsonDocument.Parse(File.ReadAllText(jsonPath));
                if (!doc.RootElement.TryGetProperty("nodes", out var nodes) || nodes.ValueKind != JsonValueKind.Array) continue;
                foreach (var n in nodes.EnumerateArray())
                {
                    var id = n.TryGetProperty("id", out var idP) ? idP.GetString() ?? "" : "";
                    if (string.IsNullOrEmpty(id)) continue;
                    var title = n.TryGetProperty("title", out var t) ? t.GetString() ?? "" : "";
                    var fm = ParseFrontmatter(n);
                    if (!allNodes.ContainsKey(id))
                        allNodes[id] = [];
                    allNodes[id].Add((name, new WikiNode(id, title, fm)));
                }
                if (doc.RootElement.TryGetProperty("edges", out var edges) && edges.ValueKind == JsonValueKind.Array)
                {
                    foreach (var e in edges.EnumerateArray())
                    {
                        var from = e.TryGetProperty("from", out var f) ? f.GetString() ?? "" : "";
                        var to = e.TryGetProperty("to", out var t) ? t.GetString() ?? "" : "";
                        var alias = e.TryGetProperty("alias", out var a) ? a.GetString() : null;
                        if (!string.IsNullOrEmpty(from) && !string.IsNullOrEmpty(to))
                            allEdges.Add((name, new WikiEdge(from, to, alias)));
                    }
                }
            }
            catch { /* salta brain graph corrotto */ }
        }

        if (allNodes.Count == 0)
            return "{\"status\":\"InsufficientEvidence\",\"error\":\"nessun brain graph interrogabile\",\"matches\":[]}";

        // ricerca: esatto o sottostringa
        var exact = allNodes.Keys.FirstOrDefault(k => string.Equals(k, query, StringComparison.OrdinalIgnoreCase));
        var matches = new List<object>();

        foreach (var (path, sources) in allNodes)
        {
            var firstBrain = sources[0].Brain;
            var node = sources[0].Node;
            var title = node.Title;

            bool match;
            if (exact is not null)
                match = string.Equals(path, exact, StringComparison.OrdinalIgnoreCase);
            else
                match = path.Contains(query, StringComparison.OrdinalIgnoreCase)
                    || title.Contains(query, StringComparison.OrdinalIgnoreCase);

            if (!match) continue;

            // vicinato 1-hop: edges che partono o arrivano al nodo
            var neighbors = new List<object>();
            foreach (var (brain, edge) in allEdges)
            {
                if (string.Equals(edge.From, path, StringComparison.OrdinalIgnoreCase))
                    neighbors.Add(new { to = edge.To, alias = edge.Alias, direction = "out", brain });
                if (string.Equals(edge.To, path, StringComparison.OrdinalIgnoreCase))
                    neighbors.Add(new { from = edge.From, alias = edge.Alias, direction = "in", brain });
            }

            // dedup brain di provenienza (se un nodo è in più brain)
            var srcBrains = sources.Select(s => s.Brain).Distinct().ToList();

            matches.Add(new
            {
                path,
                title,
                brains = srcBrains,
                neighborCount = neighbors.Count,
                neighbors = neighbors.Take(20).ToList(),
            });

            // solo query esatta → un solo risultato (il nodo + suoi vicini)
            if (exact is not null) break;
        }

        return JsonSerializer.Serialize(new
        {
            status = matches.Count > 0 ? "Success" : "InsufficientEvidence",
            query,
            brainCount = names.Length,
            totalNodes = allNodes.Count,
            matches = matches.Take(50).ToList(),
        }, JsonOpts);
    }

    /// <summary>Estrae il frontmatter da un nodo JSON (oggetto) in Dictionary&lt;string,List&gt;
    /// come WikiNode. I campi non-array (es. stringhe) vengono wrappati in una lista.</summary>
    private static Dictionary<string, List<string>> ParseFrontmatter(JsonElement node)
    {
        var result = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        if (!node.TryGetProperty("frontmatter", out var fm) || fm.ValueKind != JsonValueKind.Object)
            return result;
        foreach (var prop in fm.EnumerateObject())
        {
            var values = new List<string>();
            if (prop.Value.ValueKind == JsonValueKind.Array)
                foreach (var item in prop.Value.EnumerateArray())
                    values.Add(item.ValueKind == JsonValueKind.String ? item.GetString() ?? "" : item.ToString());
            else
                values.Add(prop.Value.ValueKind == JsonValueKind.String ? prop.Value.GetString() ?? "" : prop.Value.ToString());
            result[prop.Name] = values;
        }
        return result;
    }

    // ── Primitive single-brain (L5): operano su UN brain graph (nome esplicito) ──

    /// <summary>Carica il WikiGraph di un brain (dal suo graphs/&lt;name&gt;.json), o null se
    /// il brain non è disponibile come graph o il JSON è corrotto. Se il brain coincide con il
    /// grafo ATTIVO costruito da GRAPH BUILD (sessione corrente), lo usa direttamente in memoria
    /// — così i programmi BASIC non dipendono dal registry globale per interrogare la build.</summary>
    private static WikiGraph? LoadBrain(string brain)
    {
        var active = Basic.AI.Knowledge.GraphManager.ActiveGraph;
        if (active is not null && string.Equals(brain, Basic.AI.Knowledge.GraphManager.ActiveName, StringComparison.OrdinalIgnoreCase))
            return Basic.AI.Knowledge.GraphManager.ToWikiGraph(active);
        var jsonPath = Basic.AI.Knowledge.BrainRegistry.GraphJsonPath(brain);
        if (jsonPath is null || !File.Exists(jsonPath)) return null;
        try { return WikiGraph.LoadFromJson(File.ReadAllText(jsonPath)); }
        catch { return null; }
    }

    /// <summary>Risolve il path del JSON del graph di un brain: prima il grafo ATTIVO della
    /// sessione (ActiveDir — cartella work/ impostata dal programma BASIC), poi il registry
    /// (import o legacy GraphsDefaultDir).</summary>
    private static string? ResolveGraphJsonPath(string brain)
    {
        var activeDir = Basic.AI.Knowledge.GraphManager.ActiveDir;
        if (activeDir is { Length: > 0 })
        {
            var candidate = System.IO.Path.Combine(activeDir, brain + ".json");
            if (File.Exists(candidate)) return candidate;
        }
        return Basic.AI.Knowledge.BrainRegistry.GraphJsonPath(brain);
    }

    /// <summary>Converte un WikiGraph in un Graph&lt;string&gt; (CommonLib) per usare gli
    /// algoritmi canonici C# (SCC/topo/bipartite/betweenness/clustering/...) sui brain.</summary>
    private static CommonLib.Collections.Graph<string> ToCommonGraph(WikiGraph wg)
    {
        var g = new CommonLib.Collections.Graph<string>();
        foreach (var n in wg.Nodes.Keys) g.AddNode(n);
        foreach (var e in wg.Edges)
            if (g.HasNode(e.From) && g.HasNode(e.To)) g.AddEdge(e.From, e.To, 1);
        return g;
    }

    /// <summary>GRAPH.NEIGHBORS(brain, node): vicini diretti (outgoing+incoming) di un nodo.
    /// Ritorna JSON {status, brain, node, neighbors:[...]}.</summary>
    public static string Neighbors(string brain, string node)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        if (!g.Nodes.ContainsKey(node)) return JsonSerializer.Serialize(new { status = "Error", error = $"nodo '{node}' non presente in '{brain}'" });
        var nbs = g.Neighbors(node).OrderBy(x => x, StringComparer.Ordinal).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, node, neighbors = nbs }, JsonOpts);
    }

    /// <summary>GRAPH.PATH(brain, from, to): percorso più corto (BFS). Ritorna JSON
    /// {status, brain, from, to, path:[...]}.</summary>
    public static string Path(string brain, string from, string to)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var path = g.ShortestPath(from, to);
        return JsonSerializer.Serialize(new { status = path.Count > 0 ? "Success" : "InsufficientEvidence", brain, from, to, path }, JsonOpts);
    }

    /// <summary>GRAPH.PAGERANK(brain [, topK]): ranking PageRank dei nodi (fallback C# power
    /// iteration, coerente con WikiGraph.PageRank). Ritorna JSON {status, brain, top:[{node,rank}]}.</summary>
    public static string PageRank(string brain, int topK = 10)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var ranks = g.PageRank().Take(Math.Max(1, topK))
            .Select(x => new { node = x.Node, rank = Math.Round(x.Rank, 6) }).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, top = ranks }, JsonOpts);
    }

    /// <summary>GRAPH.CONNECTED(brain): componenti connesse del graph (BFS). Ritorna JSON
    /// {status, brain, components:[[nodi], ...]} ordinato per taglia discendente.</summary>
    public static string Connected(string brain)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var visited = new HashSet<string>(StringComparer.Ordinal);
        var components = new List<List<string>>();
        foreach (var node in g.Nodes.Keys.OrderBy(x => x, StringComparer.Ordinal))
        {
            if (!visited.Add(node)) continue;
            var comp = new List<string>();
            var q = new Queue<string>();
            q.Enqueue(node);
            while (q.Count > 0)
            {
                var cur = q.Dequeue();
                comp.Add(cur);
                foreach (var nb in g.Neighbors(cur))
                    if (visited.Add(nb)) q.Enqueue(nb);
            }
            components.Add(comp);
        }
        components.Sort((a, b) => b.Count.CompareTo(a.Count));
        return JsonSerializer.Serialize(new { status = "Success", brain, componentCount = components.Count, components }, JsonOpts);
    }

    /// <summary>GRAPH.STATS(brain): conteggi del graph (nodi/edge/dangling/componenti) + top PageRank.
    /// Ritorna JSON {status, brain, nodes, edges, dangling, components, pageRankTop}.</summary>
    public static string Stats(string brain)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var top = g.PageRank().Take(5).Select(x => new { node = x.Node, rank = Math.Round(x.Rank, 6) }).ToList();
        return JsonSerializer.Serialize(new
        {
            status = "Success",
            brain,
            nodes = g.NodeCount,
            edges = g.EdgeCount,
            dangling = g.Dangling.Count,
            pageRankTop = top,
        }, JsonOpts);
    }

    // ── Export estesi (F7 P1): mermaid/dot/subgraph/JSON by tag/folder (REPL SHOW) ──

    /// <summary>GRAPH.DOT(brain): export Graphviz dot del grafo.</summary>
    public static string Dot(string brain)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return GraphExporter.ToDot(g, brain);
    }

    /// <summary>GRAPH.SUBGRAPH(brain, root [, depth]): sottografo a distanza ≤ depth dal nodo.</summary>
    public static string Subgraph(string brain, string root, int depth)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        if (!g.Nodes.ContainsKey(root)) return JsonSerializer.Serialize(new { status = "Error", error = $"nodo '{root}' non presente in '{brain}'" });
        return GraphExporter.ToSubgraphMermaid(g, root, depth);
    }

    /// <summary>GRAPH.BYTAG(brain, tag): JSON dei nodi con quel tag + i loro edge.</summary>
    public static string ByTag(string brain, string tag)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return GraphExporter.ToJsonByTag(g, tag);
    }

    /// <summary>GRAPH.BYFOLDER(brain, folder): JSON dei nodi nella cartella (o sotto) + edge.</summary>
    public static string ByFolder(string brain, string folder)
    {
        var g = LoadBrain(brain);
        if (g is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return GraphExporter.ToJsonByFolder(g, folder);
    }

    /// <summary>GRAPH.ISVALID(brain): 1 se il grafo ha almeno un nodo.</summary>
    public static double IsValid(string brain)
    {
        var g = LoadBrain(brain);
        return g is not null && g.NodeCount > 0 ? 1.0 : 0.0;
    }

    // ── Algoritmi avanzati (L5) sui brain — riusano Graph<TNode> del CommonLib ──

    /// <summary>GRAPH.SCC(brain): componenti fortemente connesse (Tarjan). Ritorna JSON
    /// {status, brain, count, components:[[nodi],...]}.</summary>
    public static string StronglyConnected(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var g = ToCommonGraph(wg);
        var scc = g.StronglyConnectedComponents();
        return JsonSerializer.Serialize(new { status = "Success", brain, count = scc.Count, components = scc }, JsonOpts);
    }

    /// <summary>GRAPH.TOPOSORT(brain): ordinamento topologico. Ritorna JSON
    /// {status, brain, order:[nodi]} o errore se c'è un ciclo.</summary>
    public static string TopoSort(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var g = ToCommonGraph(wg);
        var topo = g.TopologicalSort();
        if (topo is null)
            return JsonSerializer.Serialize(new { status = "Error", brain, error = "il grafo contiene un ciclo" });
        return JsonSerializer.Serialize(new { status = "Success", brain, order = topo }, JsonOpts);
    }

    /// <summary>GRAPH.BIPARTITE(brain): true se il graph è bipartito. Ritorna JSON
    /// {status, brain, bipartite}.</summary>
    public static string Bipartite(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return JsonSerializer.Serialize(new { status = "Success", brain, bipartite = ToCommonGraph(wg).IsBipartite() }, JsonOpts);
    }

    /// <summary>GRAPH.BETWEENNESS(brain): betweenness centrality (Brandes). Ritorna JSON
    /// {status, brain, values:[{node, value}]} ordinato decrescente.</summary>
    public static string Betweenness(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var g = ToCommonGraph(wg);
        var cb = g.BetweennessCentrality().OrderByDescending(kv => kv.Value)
            .Select(kv => new { node = kv.Key, value = Math.Round(kv.Value, 6) }).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, values = cb }, JsonOpts);
    }

    /// <summary>GRAPH.CLUSTERING(brain, node): coefficiente di clustering locale. Ritorna JSON
    /// {status, brain, node, clustering}.</summary>
    public static string Clustering(string brain, string node)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return JsonSerializer.Serialize(new { status = "Success", brain, node, clustering = ToCommonGraph(wg).ClusteringCoefficient(node) }, JsonOpts);
    }

    /// <summary>GRAPH.DIAMETER(brain): diametro del graph (sui pesi, qui archi=1). Ritorna JSON
    /// {status, brain, diameter}.</summary>
    public static string Diameter(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return JsonSerializer.Serialize(new { status = "Success", brain, diameter = ToCommonGraph(wg).Diameter() }, JsonOpts);
    }

    /// <summary>GRAPH.MST(brain): minimum spanning tree (Kruskal). Ritorna JSON
    /// {status, brain, weight, edges:[{from,to,weight}]}.</summary>
    public static string MinimumSpanningTree(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var mst = ToCommonGraph(wg).MinimumSpanningTree();
        var edges = mst.Select(e => new { from = e.From, to = e.To, weight = e.Weight }).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, weight = mst.Sum(e => e.Weight), edges }, JsonOpts);
    }

    /// <summary>GRAPH.DIJKSTRA(brain, from, to): path a costo minimo (archi pesati — nei wiki
    /// tutti gli edge hanno peso 1, quindi equivale al BFS ma è il riferimento per grafi pesati
    /// futuri). Ritorna JSON {status, brain, from, to, path:[...]}, path vuoto se non raggiungibile.</summary>
    public static string Dijkstra(string brain, string from, string to)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var path = ToCommonGraph(wg).DijkstraPath(from, to) ?? [];
        return JsonSerializer.Serialize(new { status = "Success", brain, from, to, path }, JsonOpts);
    }

    /// <summary>GRAPH.HASCYCLE(brain): 1.0 se il graph contiene almeno un ciclo, altrimenti 0.0
    /// (ritorno BitType, parità con ISVALID).</summary>
    public static double HasCycle(string brain)
    {
        var g = LoadBrain(brain);
        return g is not null && ToCommonGraph(g).HasCycle() ? 1.0 : 0.0;
    }

    /// <summary>GRAPH.ECCENTRICITY(brain, node): eccentricità del nodo (la distanza massima verso
    /// qualsiasi altro nodo). Ritorna JSON {status, brain, node, eccentricity}.</summary>
    public static string Eccentricity(string brain, string node)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return JsonSerializer.Serialize(new { status = "Success", brain, node, eccentricity = Math.Round(ToCommonGraph(wg).Eccentricity(node), 6) }, JsonOpts);
    }

    /// <summary>GRAPH.DFS(brain, root): visita in profondità dal nodo radice. Ritorna JSON
    /// {status, brain, root, order:[...]}.</summary>
    public static string Dfs(string brain, string root)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var order = ToCommonGraph(wg).Dfs(root).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, root, order }, JsonOpts);
    }

    /// <summary>GRAPH.BFS(brain, root): visita in ampiezza dal nodo radice. Ritorna JSON
    /// {status, brain, root, order:[...]}.</summary>
    public static string Bfs(string brain, string root)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var order = ToCommonGraph(wg).Bfs(root).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, root, order }, JsonOpts);
    }

    /// <summary>GRAPH.LOUVAIN(brain): community detection (Louvain, modularità). Ritorna JSON
    /// {status, brain, count, communities:[{node, community}]} — parità con gmb_graph_louvain.</summary>
    public static string Louvain(string brain)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        var comm = ToCommonGraph(wg).Louvain();
        var count = comm.Values.Distinct().Count();
        var communities = comm.Select(kv => new { node = kv.Key, community = kv.Value }).ToList();
        return JsonSerializer.Serialize(new { status = "Success", brain, count, communities }, JsonOpts);
    }

    /// <summary>GRAPH.MAXFLOW(brain, from, to): flusso massimo (Edmonds-Karp). Ritorna JSON
    /// {status, brain, from, to, flow} — parità con gmb_graph_maxflow.</summary>
    public static string MaxFlow(string brain, string from, string to)
    {
        var wg = LoadBrain(brain);
        if (wg is null) return JsonSerializer.Serialize(new { status = "Error", error = $"brain '{brain}' non disponibile come graph" });
        return JsonSerializer.Serialize(new { status = "Success", brain, from, to, flow = ToCommonGraph(wg).MaxFlow(from, to) }, JsonOpts);
    }
}