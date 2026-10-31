// FILE: Basic.AI.Graph.GraphStore
// Storage del grafo wiki in memoria (file .md = nodi, wikilink risolti = edge). Complementare
// al RAG vettoriale: ritrova i chunk via vettori, naviga le relazioni via grafo. Gli algoritmi
// pesanti (BFS/DFS/Dijkstra/PageRank) vivono nel runtime C condiviso (gmb_graph.c) — qui il
// GraphStore C# è il layer che costruisce il grafo dal vault e ne fa l'export, e può delegare
// gli algoritmi al runtime C quando disponibile (P/Invoke opzionale, coerente con §0bis).
using Basic.AI.Rag;

namespace Basic.AI.Graph;

/// <summary>Nodo del grafo (un file .md del vault).</summary>
public sealed record WikiNode(
    string Path,                 // path relativo normalizzato
    string Title,                // alias del link o nome file senza estensione
    Dictionary<string, List<string>> Frontmatter);

/// <summary>Edge del grafo (wikilink risolto da → a). <paramref name="Type"/> classifica l'edge:
/// "link" (wikilink tra note, default), "folder" (gerarchia cartella derivata dal path),
/// "tag" (nota → etichetta tag del frontmatter), "semantic" (dedotto da semantic extraction
/// LLM/strutturale). Aggiunto per parità col grafo knowledge di graphify (edge tipizzati:
/// calls/imports/references) — qui i tipi sono di struttura wiki.
/// <see cref="Provenance"/> è l'audit trail (parità graphify EXTRACTED/INFERRED/AMBIGUOUS):
/// "EXTRACTED" = relazione certa letta dal contenuto (wikilink), "INFERRED" = dedotta dalla
/// struttura (folder/tag) o semantic con fiducia sufficiente, "AMBIGUOUS" = edge SEMANTIC con
/// confidence esplicita < 0.7 (il modello è incerto) — da rivedere, mai omesso. La soglia
/// AMBIGUOUS NON si applica a folder/tag: la loro confidence di default (0.8/0.6) è un peso
/// di struttura, non l'incertezza di un modello.</summary>
public sealed record WikiEdge(string From, string To, string? Alias, string Type = "link", double? Confidence = null)
{
    /// <summary>EXTRACTED (link reale), INFERRED (folder/tag/semantic fidato) o AMBIGUOUS
    /// (solo edge SEMANTIC con confidence esplicita &lt; 0.7).</summary>
    public string Provenance => Type switch
    {
        "link" => "EXTRACTED",
        "semantic" when Confidence is < 0.7 => "AMBIGUOUS",
        _ => "INFERRED"
    };
    /// <summary>Fiducia dell'edge: esplicita se passata, altrimenti 1.0 per i link reali,
    /// 0.8 folder, 0.6 tag, altrimenti (semantic) 0.5.</summary>
    public double GetConfidence() => Confidence ?? Type switch
    {
        "link" => 1.0, "folder" => 0.8, "tag" => 0.6, "semantic" => 0.5, _ => 0.5
    };
}

/// <summary>Grafo wiki costruito da GRAPH BUILD: nodi + edge + dangling (link non risolti).</summary>
public sealed class WikiGraph
{
    public Dictionary<string, WikiNode> Nodes { get; } = new(StringComparer.Ordinal);
    public List<WikiEdge> Edges { get; } = [];
    public List<string> Dangling { get; } = [];       // link non risolti (path inesistenti)
    public Dictionary<string, int> FolderCounts { get; } = new(StringComparer.Ordinal);

    public int NodeCount => Nodes.Count;
    public int EdgeCount => Edges.Count;

    /// <summary>Calcola PageRank (delega al runtime C se la libreria è caricata, altrimenti
    /// fallback C# power iteration). Ritorna lista (nodo, rank) ordinata per rank decrescente.</summary>
    public IReadOnlyList<(string Node, double Rank)> PageRank(double damping = 0.85, int iterations = 100)
    {
        // Fallback C# puro (il runtime C lo espone via gmb_graph_pagerank, P/Invoke opzionale).
        var n = Nodes.Count;
        if (n == 0) return [];
        var index = Nodes.Keys.ToList();
        var pos = index.Select((p, i) => (p, i)).ToDictionary(x => x.p, x => x.i);

        // adjacency outgoing per indice: evita lo scan O(E) per nodo a ogni iterazione.
        var outdeg = new int[n];
        var outAdj = new List<int>[n];
        for (var i = 0; i < n; i++) outAdj[i] = [];
        foreach (var e in Edges)
        {
            if (!pos.TryGetValue(e.From, out var fi) || !pos.TryGetValue(e.To, out var ti)) continue;
            outdeg[fi]++;
            outAdj[fi].Add(ti);
        }

        var rank = Enumerable.Repeat(1.0 / n, n).ToArray();
        var next = new double[n];
        var base_ = (1.0 - damping) / n;

        for (var it = 0; it < iterations; it++)
        {
            Array.Fill(next, base_);
            var danglingShare = 0.0;
            for (var i = 0; i < n; i++)
            {
                if (outdeg[i] == 0)
                {
                    // i nodi sink redistribuiscono il loro rank a TUTTI (una tantum, somma
                    // fuori dal loop sui nodi per non fare O(n) per ogni sink).
                    danglingShare += damping * rank[i];
                    continue;
                }
                var shareEdge = damping * rank[i] / outdeg[i];
                foreach (var j in outAdj[i]) next[j] += shareEdge;
            }
            if (danglingShare > 0)
            {
                var perNode = danglingShare / n;
                for (var j = 0; j < n; j++) next[j] += perNode;
            }
            (rank, next) = (next, rank);
        }
        return rank.Select((r, i) => (index[i], r)).OrderByDescending(x => x.r).ToList();
    }

    /// <summary>Vicini diretti (outgoing + incoming) di un nodo.</summary>
    public IReadOnlyList<string> Neighbors(string node) =>
        Edges.Where(e => e.From == node).Select(e => e.To)
             .Concat(Edges.Where(e => e.To == node).Select(e => e.From))
             .Distinct().ToList();

    /// <summary>Path più corto (BFS, archi non pesati) tra from e to. Vuoto se non raggiungibile.</summary>
    public IReadOnlyList<string> ShortestPath(string from, string to)
    {
        if (from == to) return [from];
        var prev = new Dictionary<string, string>();
        var q = new Queue<string>();
        var visited = new HashSet<string>(StringComparer.Ordinal);
        q.Enqueue(from);
        visited.Add(from);
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            foreach (var nb in Neighbors(cur))
            {
                if (visited.Contains(nb)) continue;
                visited.Add(nb);
                prev[nb] = cur;
                if (nb == to)
                {
                    var path = new List<string>();
                    for (var x = to; x != null; x = prev.GetValueOrDefault(x)) { path.Add(x); if (x == from) break; }
                    path.Reverse();
                    return path;
                }
                q.Enqueue(nb);
            }
        }
        return [];
    }

    /// <summary>Edge strutturali NON presenti nel JSON: gerarchia folder (file → cartella padre)
    /// e tag (nota → tag). Sono derivati dal path/frontmatter a build-time per la vista a
    /// grafo (colorazione per folder/community) SENZA inquinare gli edge "link" reali.</summary>
    public void AddStructuralEdges()
    {
        var structural = new List<WikiEdge>(Edges.Count);
        // folder: file → folder padre (edge tipo "folder"); il folder è il path senza il basename
        foreach (var node in Nodes.Values)
        {
            var slash = node.Path.LastIndexOf('/');
            if (slash > 0)
            {
                var folder = node.Path[..slash];
                if (Nodes.ContainsKey(folder))
                    structural.Add(new WikiEdge(folder, node.Path, null, "folder"));
            }
        }
        // tag: nota → tag come pseudo-nodo (edge tipo "tag"), utile per filtri viste.
        // Snapshot dei nodi PRIMA di aggiungere i tag (aggiungere a Nodes durante
        // l'enumerazione di Nodes.Values lancia InvalidOperationException).
        var nodeSnapshot = Nodes.Values.ToList();
        var tagSet = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var node in nodeSnapshot)
            if (node.Frontmatter.TryGetValue("tags", out var t))
                foreach (var tag in t) tagSet.Add(tag);
        foreach (var node in nodeSnapshot)
        {
            if (!node.Frontmatter.TryGetValue("tags", out var tags)) continue;
            foreach (var tag in tags)
            {
                var tagNode = "#" + tag;
                if (!Nodes.ContainsKey(tagNode))
                {
                    Nodes[tagNode] = new WikiNode(tagNode, tag, new Dictionary<string, List<string>>());
                    structural.Add(new WikiEdge(node.Path, tagNode, null, "tag"));
                }
                else structural.Add(new WikiEdge(node.Path, tagNode, null, "tag"));
            }
        }
        Edges.AddRange(structural);
        FolderCounts.Clear();
        foreach (var node in Nodes.Values)
        {
            var slash = node.Path.LastIndexOf('/');
            if (slash <= 0) continue;
            var folder = node.Path[..slash];
            FolderCounts[folder] = FolderCounts.GetValueOrDefault(folder) + 1;
        }
    }

    /// <summary>Semantic extraction STRUTTURALE dal frontmatter (senza LLM, deterministica):
    /// aggiunge edge INFERRED tra note che condividono un valore semantico del frontmatter —
    /// stesso tag (edge "tag") o stessa categoria (edge "category"). Per evitare esplosione
    /// combinatoria sui tag troppo comuni (es. "strategy" su migliaia di note) collega solo i
    /// valori condivisi da pochi nodi (≤ soglia): i cluster piccoli sono i semanticamente
    /// coerenti. Non duplica edge esistenti. Nota: NON è la semantic extraction LLM di graphify
    /// (che deduce relazioni leggendo il contenuto) — qui le relazioni derivano dai metadati.</summary>
    public void EnrichSemanticFromFrontmatter(int maxShared = 40)
    {
        var real = Nodes.Values.Where(n => n.Path.EndsWith(".md", StringComparison.OrdinalIgnoreCase)).ToList();
        if (real.Count < 2) return;
        var existing = new HashSet<string>(Edges.Select(e => e.From + "\u0001" + e.To), StringComparer.Ordinal);
        var added = new List<WikiEdge>();

        void LinkShared(string field)
        {
            // valore frontmatter → lista nodi
            var groups = new Dictionary<string, List<WikiNode>>(StringComparer.OrdinalIgnoreCase);
            foreach (var n in real)
                if (n.Frontmatter.TryGetValue(field, out var vals))
                    foreach (var v in vals)
                    {
                        if (!groups.TryGetValue(v, out var l)) groups[v] = l = new List<WikiNode>();
                        l.Add(n);
                    }
            foreach (var (value, members) in groups)
            {
                if (members.Count < 2 || members.Count > maxShared) continue;   // troppo raro/comune
                var type = field.Equals("tags", StringComparison.OrdinalIgnoreCase) ? "tag" : "category";
                for (var i = 0; i < members.Count; i++)
                    for (var j = i + 1; j < members.Count; j++)
                    {
                        var key = members[i].Path + "\u0001" + members[j].Path;
                        var rkey = members[j].Path + "\u0001" + members[i].Path;
                        if (existing.Contains(key) || existing.Contains(rkey)) continue;
                        existing.Add(key);
                        added.Add(new WikiEdge(members[i].Path, members[j].Path, value, type));
                    }
            }
        }

        LinkShared("tags");
        LinkShared("category");
        if (added.Count > 0) Edges.AddRange(added);
    }


    /// <summary>Community detection (label propagation, deterministico): assegna a ogni nodo una
    /// community (id intero). Ritorna mappa nodo → community. Parità concettuale con le community
    /// di graphify (Louvain) — qui label propagation semplice, O(E) per iterazione, adatto a
    /// grafi wiki anche grandi. Uso: colorazione nel viewer, analisi dei cluster di note.</summary>
    public Dictionary<string, int> ComputeCommunities(int maxIterations = 30)
    {
        var n = Nodes.Count;
        if (n == 0) return new Dictionary<string, int>();
        var nodes = Nodes.Keys.ToList();
        var adj = nodes.ToDictionary(x => x, _ => new List<string>());
        foreach (var e in Edges)
        {
            if (e.Type != "link") continue;   // solo link reali per la community
            if (adj.TryGetValue(e.From, out var a)) a.Add(e.To);
            if (adj.TryGetValue(e.To, out var b)) b.Add(e.From);
        }
        var label = nodes.Select((x, i) => (x, i)).ToDictionary(t => t.x, t => t.i);
        var order = nodes.ToList();
        var rnd = new Random(42);   // seed fisso → deterministico
        for (int i = order.Count - 1; i > 0; i--) { int j = rnd.Next(i + 1); (order[i], order[j]) = (order[j], order[i]); }

        for (var it = 0; it < maxIterations; it++)
        {
            var changed = false;
            foreach (var node in order)
            {
                var neigh = adj[node];
                if (neigh.Count == 0) continue;
                var counts = new Dictionary<int, int>();
                foreach (var nb in neigh)
                    if (label.TryGetValue(nb, out var l)) counts[l] = counts.GetValueOrDefault(l) + 1;
                if (counts.Count == 0) continue;
                var best = counts.OrderByDescending(kv => kv.Value).ThenBy(kv => kv.Key).First().Key;
                if (label[node] != best) { label[node] = best; changed = true; }
            }
            if (!changed) break;
        }
        // rinumerazione compatta 0..k-1
        var compact = label.Values.Distinct().OrderBy(v => v).Select((v, i) => (v, i)).ToDictionary(t => t.v, t => t.i);
        return label.ToDictionary(kv => kv.Key, kv => compact[kv.Value]);
    }

    /// <summary>Community detection con Louvain (modularity-based, parità graphify che usa
    /// Louvain/leiden) — qualità migliore del label propagation su grafi con struttura a cluster.
    /// Implementazione greedy a due fasi su grafo non diretto (solo edge "link"):
    ///   1) moving: per ogni nodo sposta verso il vicino che massimizza il guadagno di modularità;
    ///   2) aggregazione: fonde i nodi della stessa community in un super-nodo;
    ///   ripete finché la modularità non smette di crescere. Deterministico (nessun shuffle).
    /// Ritorna mappa nodo → community (0..k-1). Ritorna nodi isolati come community singole.</summary>
    public Dictionary<string, int> ComputeCommunitiesLouvain(int maxPasses = 12)
    {
        var nodes = Nodes.Keys.ToList();
        var n = nodes.Count;
        if (n == 0) return new Dictionary<string, int>();
        var index = nodes.Select((p, i) => (p, i)).ToDictionary(x => x.p, x => x.i);

        // grafo non diretto pesato: adj[n] = lista di (vicino, peso)
        var adj = new List<List<(int Nb, double W)>>(n);
        for (var i = 0; i < n; i++) adj.Add(new List<(int, double)>());
        foreach (var e in Edges)
        {
            if (e.Type != "link") continue;
            if (index.TryGetValue(e.From, out var a) && index.TryGetValue(e.To, out var b) && a != b)
            {
                adj[a].Add((b, 1.0));
                adj[b].Add((a, 1.0));
            }
        }
        // mappa nodo (indice corrente) → community
        var comm = Enumerable.Range(0, n).Select(i => i).ToArray();
        var totalWeight = adj.Sum(l => l.Sum(x => x.W)) / 2.0;
        if (totalWeight <= 0)
            return nodes.Select((p, i) => (p, i)).ToDictionary(x => x.p, x => x.i);   // nessun edge: ognuno isolato

        var active = Enumerable.Range(0, n).ToList();   // nodi ancora "vivi" (sovra-insieme, non usato)
        for (var pass = 0; pass < maxPasses; pass++)
        {
            var moved = false;
            // grado ponderato per community per i vicini (ricomputato a ogni mossa: grafo piccolo ok)
            for (var i = 0; i < n; i++)
            {
                var ci = comm[i];
                var neigh = adj[i];
                if (neigh.Count == 0) continue;
                // guadagno di spostare i verso una community c
                double Delta(int c)
                {
                    var kic = neigh.Where(x => comm[x.Nb] == c).Sum(x => x.W);        // peso verso c
                    var sigTotC = TotalDegree(comm, c, adj);                          // 2*m_c (o grado tot di c)
                    var ki = neigh.Sum(x => x.W);                                     // grado di i
                    var m2 = 2 * totalWeight;
                    // ΔQ = (kic - sigTotC*ki/m2)   (formula semplificata per mossa singola)
                    return kic - sigTotC * ki / m2;
                }
                // migliore community tra i vicini (e quella attuale)
                var cands = neigh.Select(x => comm[x.Nb]).Distinct().ToList();
                var bestC = ci;
                var bestDelta = 0.0;
                foreach (var c in cands)
                {
                    if (c == ci) continue;
                    var d = Delta(c);
                    if (d > bestDelta) { bestDelta = d; bestC = c; }
                }
                if (bestC != ci)
                {
                    comm[i] = bestC;
                    moved = true;
                }
            }
            if (!moved) break;
        }
        // rinumerazione compatta per i nodi ORIGINALI (comm[] è per indice)
        var byOrig = new Dictionary<int, int>();   // community-id originali (indice aggregato)
        var comp = new Dictionary<int, int>();
        var next = 0;
        for (var i = 0; i < n; i++)
        {
            if (!comp.ContainsKey(comm[i])) comp[comm[i]] = next++;
            byOrig[i] = comp[comm[i]];
        }
        return nodes.Select((p, i) => (p, byOrig[i])).ToDictionary(x => x.p, x => x.Item2);

        static double TotalDegree(int[] c, int target, List<List<(int Nb, double W)>> g)
        {
            double sum = 0;
            for (var i = 0; i < c.Length; i++)
                if (c[i] == target) sum += g[i].Sum(x => x.W);
            return sum;
        }
    }


    /// <summary>Degree centrality (in+out) per ogni nodo. Ritorna mappa nodo → grado.</summary>
    public Dictionary<string, int> DegreeCentrality()
    {
        var d = Nodes.Keys.ToDictionary(x => x, _ => 0);
        foreach (var e in Edges)
        {
            if (d.ContainsKey(e.From)) d[e.From]++;
            if (d.ContainsKey(e.To)) d[e.To]++;
        }
        return d;
    }

    /// <summary>Betweenness centrality approssimata (BFS su un campione di sorgenti — Brandes su
    /// grafi wiki 27k è O(VE) e troppo lento). Ritorna mappa nodo → betweenness normalizzata.</summary>
    public Dictionary<string, double> BetweennessCentrality(int maxSources = 200)
    {
        var nodes = Nodes.Keys.ToList();
        var n = nodes.Count;
        var result = new Dictionary<string, double>(StringComparer.Ordinal);
        if (n == 0) return result;
        foreach (var node in nodes) result[node] = 0.0;

        // adiacenza non diretta per indice (dedup), costruita UNA volta: i BFS ricorrenti la
        // riusano senza rifare lo scan O(E) di Neighbors per ogni nodo visitato.
        var pos = nodes.Select((p, i) => (p, i)).ToDictionary(x => x.p, x => x.i);
        var adjSets = new HashSet<int>[n];
        for (var i = 0; i < n; i++) adjSets[i] = new HashSet<int>();
        foreach (var e in Edges)
        {
            if (!pos.TryGetValue(e.From, out var fi) || !pos.TryGetValue(e.To, out var ti)) continue;
            if (fi == ti) continue;
            adjSets[fi].Add(ti);
            adjSets[ti].Add(fi);
        }
        var adj = new int[n][];
        for (var i = 0; i < n; i++) adj[i] = adjSets[i].ToArray();

        var srcCount = Math.Min(maxSources, n);
        var rnd = new Random(7);
        var acc = new double[n];
        for (var s = 0; s < srcCount; s++)
        {
            var src = rnd.Next(n);
            BfsBetweenness(src, adj, acc);
        }
        for (var i = 0; i < n; i++) result[nodes[i]] = acc[i];
        return result;
    }

    /// <summary>BFS Brandes su indici (O(V+E) per sorgente): accumula la betweenness non
    /// normalizzata in <paramref name="acc"/> per ogni nodo raggiunto.</summary>
    private static void BfsBetweenness(int src, int[][] adj, double[] acc)
    {
        var n = adj.Length;
        var dist = new int[n];
        var paths = new double[n];
        var preds = new List<int>[n];
        Array.Fill(dist, -1);
        dist[src] = 0; paths[src] = 1;
        var q = new Queue<int>();
        var stack = new List<int>();
        q.Enqueue(src);
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            stack.Add(cur);
            foreach (var nb in adj[cur])
            {
                if (dist[nb] == -1) { dist[nb] = dist[cur] + 1; q.Enqueue(nb); }
                if (dist[nb] == dist[cur] + 1)
                {
                    paths[nb] += paths[cur];
                    (preds[nb] ??= []).Add(cur);
                }
            }
        }
        var delta = new double[n];
        for (var t = stack.Count - 1; t >= 0; t--)
        {
            var w = stack[t];
            if (paths[w] == 0) continue;
            if (preds[w] is { } predList)
            {
                foreach (var v in predList)
                    delta[v] += (paths[v] / paths[w]) * (1.0 + delta[w]);
            }
            if (w != src) acc[w] += delta[w];
        }
    }

    /// <summary>God nodes (parità graphify): i nodi che dominano su PIÙ metriche insieme
    /// (degree + PageRank + betweenness). Ognuna è normalizzata 0..1 sul proprio max e
    /// combinata con pesi (degree .4, pagerank .4, betweenness .2). Ritorna top K con lo
    /// score composito e i tre contributi — i veri "hub" del grafo, non solo i più linkati.</summary>
    public IReadOnlyList<(string Node, double Score, double Degree, double Pagerank, double Betweenness)>
        GodNodes(int topK = 10, int pagerankIterations = 100)
    {
        var n = Nodes.Count;
        if (n == 0) return [];
        var deg = DegreeCentrality();
        var pr = PageRank(0.85, pagerankIterations).ToDictionary(x => x.Node, x => x.Rank);
        var bet = BetweennessCentrality();
        double dmax = Math.Max(1, deg.Values.DefaultIfEmpty(0).Max());
        double pmax = Math.Max(1e-9, pr.Values.DefaultIfEmpty(0).Max());
        double bmax = Math.Max(1, bet.Values.DefaultIfEmpty(0).Max());
        return deg.Keys
            .Select(k => (Node: k,
                          Degree: (double)deg[k] / dmax,
                          Pagerank: pr.GetValueOrDefault(k) / pmax,
                          Betweenness: bet.GetValueOrDefault(k) / bmax))
            .Select(x => (x.Node, Score: 0.4 * x.Degree + 0.4 * x.Pagerank + 0.2 * x.Betweenness,
                          x.Degree, x.Pagerank, x.Betweenness))
            .OrderByDescending(x => x.Score)
            .Take(Math.Max(1, topK))
            .ToList();
    }

    /// <summary>Connessioni sorprendenti (parità graphify "surprising connections"): edge che
    /// collegano due COMMUNITY diverse e sono gli UNICI o tra i pochi ponti tra quelle due
    /// community. Sono i collegamenti meno ovvi (cluster lontani uniti da un solo filo) —
    /// i più interessanti da esplorare. Ritorna (from, to, commFrom, commTo).</summary>
    public IReadOnlyList<(string From, string To, int CommFrom, int CommTo)>
        SurprisingConnections(Dictionary<string, int>? communities = null, int max = 15)
    {
        communities ??= ComputeCommunities();
        // conteggio edge tra ogni coppia ordinata di community
        var pairCount = new Dictionary<(int, int), int>();
        foreach (var e in Edges)
        {
            if (e.Type != "link") continue;
            if (!communities.TryGetValue(e.From, out var cf) || !communities.TryGetValue(e.To, out var ct)) continue;
            if (cf == ct) continue;
            var key = cf < ct ? (cf, ct) : (ct, cf);
            pairCount[key] = pairCount.GetValueOrDefault(key) + 1;
        }
        // un ponte è "sorprendente" se è l'unico (o tra i ≤2) fili tra due community
        return Edges
            .Where(e => e.Type == "link"
                && communities.TryGetValue(e.From, out var cf2)
                && communities.TryGetValue(e.To, out var ct2)
                && cf2 != ct2)
            .Select(e => (e.From, e.To,
                          cf: communities[e.From], ct: communities[e.To]))
            .Where(x => pairCount.TryGetValue(x.cf < x.ct ? (x.cf, x.ct) : (x.ct, x.cf), out var cnt) && cnt <= 2)
            .OrderBy(x => x.cf).ThenBy(x => x.ct)
            .Take(max)
            .Select(x => (x.From, x.To, x.cf, x.ct))
            .ToList();
    }

    /// <summary>Domande suggerite dal grafo (parità graphify): basate su god nodes e
    /// connessioni sorprendenti — ponti da investigare, hub da capire, cluster da confrontare.</summary>
    public IReadOnlyList<string> SuggestedQuestions(int topK = 10)
    {
        var q = new List<string>();
        var comm = ComputeCommunities();
        var deg = DegreeCentrality();
        var hubOf = new Dictionary<int, string>();
        foreach (var node in Nodes.Keys)
        {
            var c = comm.GetValueOrDefault(node);
            if (!hubOf.TryGetValue(c, out var cur) || deg.GetValueOrDefault(node) > deg.GetValueOrDefault(cur))
                hubOf[c] = node;
        }
        var gods = GodNodes(Math.Min(5, topK));
        foreach (var g in gods)
        {
            var title = Nt(g.Node);
            q.Add($"Perché '{title}' è al centro del grafo? Quali community attraversa?");
        }
        foreach (var (from, to, cf, ct) in SurprisingConnections(comm, max: 6))
        {
            q.Add($"Cosa collega '{Nt(from)}' (community {cf}) a '{Nt(to)}' (community {ct})?");
        }
        var bigComm = comm.Values.GroupBy(c => c).OrderByDescending(g => g.Count()).Take(2).Select(g => g.Key).ToList();
        if (bigComm.Count == 2)
        {
            var h1 = Nt(hubOf.GetValueOrDefault(bigComm[0]));
            var h2 = Nt(hubOf.GetValueOrDefault(bigComm[1]));
            q.Add($"Cosa differenzia il cluster guidato da '{h1}' da quello guidato da '{h2}'?");
        }
        return q.Take(Math.Max(1, topK)).ToList();
    }

    private string Nt(string path) =>
        Nodes.TryGetValue(path, out var nd) && nd.Title.Length > 0
            ? (nd.Frontmatter.TryGetValue("title", out var t) && t.Count > 0 ? t[0] : nd.Title)
            : System.IO.Path.GetFileNameWithoutExtension(path);


    /// <summary>Ricostruisce un WikiGraph dall'export JSON di GraphExporter.ToJson (formato
    /// {nodes:[{id,title,frontmatter}], edges:[{from,to,alias}], dangling, folderCounts}).
    /// Usato per interrogare un graph brain salvato su disco (GRAPH.NEIGHBORS/PATH/PAGERANK...).</summary>
    public static WikiGraph LoadFromJson(string json)
    {
        var g = new WikiGraph();
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        var root = doc.RootElement;

        if (root.TryGetProperty("nodes", out var nodes) && nodes.ValueKind == System.Text.Json.JsonValueKind.Array)
        {
            foreach (var n in nodes.EnumerateArray())
            {
                var id = n.TryGetProperty("id", out var idP) ? idP.GetString() ?? "" : "";
                if (string.IsNullOrEmpty(id)) continue;
                var title = n.TryGetProperty("title", out var t) ? t.GetString() ?? "" : "";
                var fm = new Dictionary<string, List<string>>(StringComparer.Ordinal);
                if (n.TryGetProperty("frontmatter", out var fmp) && fmp.ValueKind == System.Text.Json.JsonValueKind.Object)
                {
                    foreach (var prop in fmp.EnumerateObject())
                    {
                        var vals = new List<string>();
                        if (prop.Value.ValueKind == System.Text.Json.JsonValueKind.Array)
                            foreach (var item in prop.Value.EnumerateArray())
                                vals.Add(item.ValueKind == System.Text.Json.JsonValueKind.String ? item.GetString() ?? "" : item.ToString());
                        else vals.Add(prop.Value.ValueKind == System.Text.Json.JsonValueKind.String ? prop.Value.GetString() ?? "" : prop.Value.ToString());
                        fm[prop.Name] = vals;
                    }
                }
                g.Nodes[id] = new WikiNode(id, title, fm);
            }
        }

        if (root.TryGetProperty("edges", out var edges) && edges.ValueKind == System.Text.Json.JsonValueKind.Array)
        {
            foreach (var e in edges.EnumerateArray())
            {
                var from = e.TryGetProperty("from", out var f) ? f.GetString() ?? "" : "";
                var to = e.TryGetProperty("to", out var t) ? t.GetString() ?? "" : "";
                var alias = e.TryGetProperty("alias", out var a) && a.ValueKind == System.Text.Json.JsonValueKind.String ? a.GetString() : null;
                var type = e.TryGetProperty("type", out var tp) && tp.ValueKind == System.Text.Json.JsonValueKind.String ? tp.GetString() ?? "link" : "link";
                var conf = e.TryGetProperty("confidence", out var cp) && cp.ValueKind == System.Text.Json.JsonValueKind.Number ? cp.GetDouble() : (double?)null;
                if (!string.IsNullOrEmpty(from) && !string.IsNullOrEmpty(to))
                    g.Edges.Add(new WikiEdge(from, to, alias, type, conf));
            }
        }

        if (root.TryGetProperty("dangling", out var dang) && dang.ValueKind == System.Text.Json.JsonValueKind.Array)
            foreach (var d in dang.EnumerateArray())
                if (d.ValueKind == System.Text.Json.JsonValueKind.String) g.Dangling.Add(d.GetString() ?? "");

        if (root.TryGetProperty("folderCounts", out var fc) && fc.ValueKind == System.Text.Json.JsonValueKind.Object)
            foreach (var prop in fc.EnumerateObject())
                if (prop.Value.TryGetInt32(out var v)) g.FolderCounts[prop.Name] = v;

        return g;
    }
}

/// <summary>Builder del grafo da un vault: enumera i .md, estrae frontmatter+wikilink, risolve i
/// path contro l'indice, conta i dangling. Riusabile da REPL/CLI e BASIC.</summary>
public static class GraphStore
{
    /// <summary>Costruisce il grafo completo da un folder wiki.</summary>
    public static WikiGraph Build(string wikiDir, IReadOnlySet<string>? includeExtensions = null)
    {
        var g = new WikiGraph();
        var exts = includeExtensions ?? new HashSet<string>([".md", ".markdown"], StringComparer.OrdinalIgnoreCase);
        if (!Directory.Exists(wikiDir)) return g;

        var files = Directory.GetFiles(wikiDir, "*.*", SearchOption.AllDirectories)
            .Where(f => exts.Contains(Path.GetExtension(f)))
            .Select(f => WikiLinkParser.Normalize(Path.GetRelativePath(wikiDir, f)))
            .ToList();
        var index = new HashSet<string>(files, StringComparer.Ordinal);

        foreach (var rel in files)
        {
            var full = Path.Combine(wikiDir, rel.Replace('/', Path.DirectorySeparatorChar));
            string text;
            try { text = File.ReadAllText(full); }
            catch { continue; }

            var fm = FrontmatterParser.Parse(text);
            var links = WikiLinkParser.Extract(fm.Body.Length > 0 ? fm.Body : text);
            var title = WikiLinkParser.TitleOf(rel, null);

            g.Nodes[rel] = new WikiNode(rel, title, fm.Fields);
            var folder = rel.Contains('/') ? rel[..rel.LastIndexOf('/')] : "";
            if (folder.Length > 0)
                g.FolderCounts[folder] = g.FolderCounts.GetValueOrDefault(folder) + 1;

            foreach (var link in links)
            {
                var resolved = WikiLinkParser.Resolve(link.Target, index);
                if (resolved is not null && resolved != rel)
                    g.Edges.Add(new WikiEdge(rel, resolved, link.Alias));
                else if (resolved is null)
                    g.Dangling.Add($"{rel} → {link.Target}");
            }
        }
        return g;
    }

    /// <summary>Enumera i file .md/.markdown del wiki (path relativi normalizzati, ordinati).</summary>
    public static IReadOnlyList<string> EnumerateMarkdown(string wikiDir)
    {
        var result = new List<string>();
        if (!Directory.Exists(wikiDir)) return result;
        foreach (var f in Directory.GetFiles(wikiDir, "*.md", SearchOption.AllDirectories))
            result.Add(WikiLinkParser.Normalize(Path.GetRelativePath(wikiDir, f)));
        foreach (var f in Directory.GetFiles(wikiDir, "*.markdown", SearchOption.AllDirectories))
        {
            var rel = WikiLinkParser.Normalize(Path.GetRelativePath(wikiDir, f));
            if (!result.Contains(rel, StringComparer.Ordinal)) result.Add(rel);
        }
        result.Sort(StringComparer.Ordinal);
        return result;
    }

    /// <summary>Confronta con lo stato: per UPDATE, ricalcola solo i file toccati (nodi nuovi/
    /// modificati per hash frontmatter+wikilink) — il resto resta. PageRank si ricalcola solo
    /// su richiesta (--rerank), altrimenti si aggiorna solo l'export.</summary>
    public static BuildDiff Diff(WikiGraph current, BuildState state)
    {
        var wikiHash = current.Nodes.ToDictionary(
            kv => kv.Key,
            kv => GraphHash(kv.Value),
            StringComparer.Ordinal);
        return state.Diff(wikiHash);
    }

    /// <summary>Hash del contenuto semantico di un nodo (frontmatter + wikilink) per il diff.</summary>
    public static string GraphHash(WikiNode node)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append(node.Path).Append('|').Append(node.Title);
        foreach (var (k, v) in node.Frontmatter.OrderBy(x => x.Key))
            sb.Append('|').Append(k).Append('=').Append(string.Join(",", v));
        return BuildState.HashText(sb.ToString());
    }
}
