// FILE: Interpreter.Gpm.Collections.Graph.Core.cs
// ReSharper disable CheckNamespace

/* 
 * Graph.cs: grafo orientato e pesato con lista di adiacenza, BFS, DFS, Dijkstra
 * 
 * FUNZIONI:
 * - AddNode(node): aggiunge nodo
 * - AddEdge(from, to, weight): aggiunge arco pesato
 * - RemoveNode(node): rimuove nodo e archi
 * - RemoveEdge(from, to): rimuove arco
 * - HasNode/HasEdge: verifica esistenza
 * - TryGetEdgeWeight(from, to, out weight): peso arco
 * - GetInDegree(node): grado di entrata
 * - GetNeighbors(node): vicini di un nodo
 * - Bfs(startNode): visita in ampiezza
 * - Dfs(startNode): visita in profondità
 * - Dijkstra(startNode): percorso minimo
 */

namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati Grafo generica, orientata e pesata.
///
/// Teoria e Algoritmi:
/// Un grafo è una struttura dati non lineare composta da un insieme di nodi (o vertici) e un insieme di
/// archi (o lati) che collegano coppie di nodi. Questa implementazione rappresenta un grafo orientato
/// (un arco da A a B non implica un arco da B ad A) e pesato (ogni arco ha un costo numerico associato).
/// La struttura è implementata utilizzando una "lista di adiacenza". Per ogni nodo, viene mantenuto
/// un dizionario che mappa i nodi vicini al peso dell'arco che li collega. Questo approccio è efficiente
/// in termini di spazio per grafi sparsi (con pochi archi rispetto al numero di nodi).
/// Sono implementati alcuni degli algoritmi grafici più fondamentali:
/// - Breadth-First Search (BFS): Un algoritmo di attraversamento che esplora i nodi "a livelli", visitando
///   tutti i vicini di un nodo prima di procedere ai loro vicini. È utilizzato per trovare il percorso più
///   breve in termini di numero di archi.
/// - Depth-First Search (DFS): Un algoritmo di attraversamento che esplora il più in profondità possibile
///   lungo ogni ramo prima di tornare indietro (backtracking).
/// - Algoritmo di Dijkstra: Un algoritmo classico per trovare il percorso più breve (con il costo minimo)
///   tra un nodo di partenza e tutti gli altri nodi in un grafo con pesi non negativi. Utilizza una coda
///   di priorità (implementata con un `PriorityQueue` di .NET) per esplorare in modo efficiente il nodo
///   non visitato più vicino.
///
/// Performance:
/// Le performance dipendono fortemente dalla densità del grafo (numero di archi |E| rispetto al numero di nodi |V|).
/// - Aggiunta/Rimozione di Nodi/Archi: O(1) in media con una rappresentazione a lista di adiacenza.
/// - BFS e DFS: O(|V| + |E|), in quanto ogni nodo e ogni arco vengono visitati una sola volta.
/// - Algoritmo di Dijkstra: O(|E| + |V| log |V|) con un heap binario come coda di priorità, che è l'implementazione
///   standard ed efficiente.
/// </summary>
/// <typeparam name="TNode">Il tipo dei dati memorizzati nei nodi. Deve essere non nullo.</typeparam>
public class Graph<TNode> where TNode : notnull
{
    /// <summary>
    /// La struttura dati principale del grafo, che implementa una lista di adiacenza.
    /// La chiave esterna è il nodo di partenza. Il valore è un altro dizionario
    /// che mappa i nodi di destinazione al peso dell'arco.
    /// </summary>
    private readonly Dictionary<TNode, Dictionary<TNode, double>> _adjacencyList;

    /// <summary>
    /// Ottiene il numero di nodi (vertici) presenti nel grafo.
    /// </summary>
    public int NodeCount => _adjacencyList.Count;

    /// <summary>
    /// Inizializza una nuova istanza vuota della classe <see cref="Graph{TNode}"/>.
    /// </summary>
    public Graph()
    {
        _adjacencyList = new Dictionary<TNode, Dictionary<TNode, double>>();
    }

    /// <summary>
    /// Aggiunge un nuovo nodo (vertice) al grafo.
    /// </summary>
    /// <param name="node">Il nodo da aggiungere.</param>
    /// <returns><c>true</c> se il nodo è stato aggiunto con successo; <c>false</c> se il nodo esisteva già.</returns>
    public bool AddNode(TNode node)
    {
        return _adjacencyList.TryAdd(node, new Dictionary<TNode, double>());
    }

    /// <summary>
    /// Aggiunge un arco orientato e pesato tra due nodi esistenti.
    /// Se l'arco esiste già, il suo peso viene aggiornato.
    /// </summary>
    /// <param name="from">Il nodo di partenza dell'arco.</param>
    /// <param name="to">Il nodo di destinazione dell'arco.</param>
    /// <param name="weight">Il peso (costo) associato all'arco. Deve essere non negativo per algoritmi come Dijkstra.</param>
    /// <exception cref="ArgumentException">Lanciata se uno dei nodi non esiste nel grafo.</exception>
    public void AddEdge(TNode from, TNode to, double weight)
    {
        if (!_adjacencyList.ContainsKey(from) || !_adjacencyList.ContainsKey(to))
        {
            throw new ArgumentException("Entrambi i nodi devono esistere nel grafo per poter aggiungere un arco.");
        }

        _adjacencyList[from][to] = weight;
    }

    /// <summary>
    /// Rimuove un nodo dal grafo, insieme a tutti gli archi entranti e uscenti.
    /// </summary>
    /// <param name="node">Il nodo da rimuovere.</param>
    /// <returns><c>true</c> se il nodo è stato trovato e rimosso; altrimenti, <c>false</c>.</returns>
    public bool RemoveNode(TNode node)
    {
        if (!_adjacencyList.Remove(node))
        {
            return false;
        }

        // Rimuove tutti gli archi entranti verso il nodo eliminato
        foreach (var neighbors in _adjacencyList.Values)
        {
            neighbors.Remove(node);
        }

        return true;
    }

    /// <summary>
    /// Rimuove un arco orientato tra due nodi.
    /// </summary>
    /// <param name="from">Il nodo di partenza dell'arco.</param>
    /// <param name="to">Il nodo di destinazione dell'arco.</param>
    /// <returns><c>true</c> se l'arco esisteva ed è stato rimosso; altrimenti, <c>false</c>.</returns>
    public bool RemoveEdge(TNode from, TNode to)
    {
        return _adjacencyList.TryGetValue(from, out var neighbors) && neighbors.Remove(to);
    }

    /// <summary>
    /// Verifica se un nodo esiste nel grafo.
    /// </summary>
    /// <param name="node">Il nodo da cercare.</param>
    /// <returns><c>true</c> se il nodo è presente nel grafo; altrimenti, <c>false</c>.</returns>
    public bool HasNode(TNode node)
    {
        return _adjacencyList.ContainsKey(node);
    }

    /// <summary>
    /// Verifica se esiste un arco orientato da un nodo di partenza a un nodo di destinazione.
    /// </summary>
    /// <param name="from">Il nodo di partenza dell'arco.</param>
    /// <param name="to">Il nodo di destinazione dell'arco.</param>
    /// <returns><c>true</c> se l'arco esiste; altrimenti, <c>false</c>.</returns>
    public bool HasEdge(TNode from, TNode to)
    {
        return _adjacencyList.TryGetValue(from, out var neighbors) && neighbors.ContainsKey(to);
    }

    /// <summary>
    /// Tenta di ottenere il peso di un arco tra due nodi.
    /// </summary>
    /// <param name="from">Il nodo di partenza dell'arco.</param>
    /// <param name="to">Il nodo di destinazione dell'arco.</param>
    /// <param name="weight">Quando il metodo ritorna, contiene il peso dell'arco se trovato; altrimenti, 0.</param>
    /// <returns><c>true</c> se l'arco è stato trovato; altrimenti, <c>false</c>.</returns>
    public bool TryGetEdgeWeight(TNode from, TNode to, out double weight)
    {
        if (_adjacencyList.TryGetValue(from, out var neighbors) && neighbors.TryGetValue(to, out weight))
        {
            return true;
        }

        weight = 0;
        return false;
    }

    /// <summary>
    /// Calcola il grado di entrata (in-degree) di un nodo, ovvero il numero di archi che puntano a esso.
    /// L'operazione ha una complessità di O(|V|) perché deve scansionare le liste di adiacenza di tutti gli altri nodi.
    /// </summary>
    /// <param name="node">Il nodo di cui calcolare il grado di entrata.</param>
    /// <returns>Il numero di archi entranti, o -1 se il nodo non esiste nel grafo.</returns>
    public int GetInDegree(TNode node)
    {
        if (!HasNode(node))
        {
            return -1; // O lanciare un'eccezione, a seconda della semantica desiderata
        }

        var inDegree = 0;
        foreach (var neighbors in _adjacencyList.Values)
        {
            if (neighbors.ContainsKey(node))
            {
                inDegree++;
            }
        }

        return inDegree;
    }

    /// <summary>
    /// Ottiene una collezione di tutti i nodi presenti nel grafo.
    /// </summary>
    public IEnumerable<TNode> Nodes => _adjacencyList.Keys;

    /// <summary>
    /// Ottiene i vicini di un dato nodo (i nodi raggiungibili tramite un arco uscente).
    /// </summary>
    /// <param name="node">Il nodo di cui ottenere i vicini.</param>
    /// <returns>Una collezione di coppie chiave-valore rappresentanti i nodi vicini e i pesi degli archi, o una collezione vuota se il nodo non esiste o non ha archi uscenti.</returns>
    public IEnumerable<KeyValuePair<TNode, double>> GetNeighbors(TNode node)
    {
        return _adjacencyList.TryGetValue(node, out var neighbors)
            ? neighbors
            : Enumerable.Empty<KeyValuePair<TNode, double>>();
    }

    /// <summary>
    /// Esegue un attraversamento del grafo in ampiezza (Breadth-First Search) a partire da un nodo.
    /// </summary>
    /// <param name="startNode">Il nodo da cui iniziare l'attraversamento.</param>
    /// <returns>Una collezione di nodi nell'ordine in cui sono stati visitati.</returns>
    /// <exception cref="ArgumentException">Lanciata se il nodo di partenza non esiste nel grafo.</exception>
    public IEnumerable<TNode> Bfs(TNode startNode)
    {
        if (!_adjacencyList.ContainsKey(startNode))
            throw new ArgumentException("Il nodo di partenza non esiste nel grafo.");

        var visited = new HashSet<TNode>();
        var queue = new Queue<TNode>();

        queue.Enqueue(startNode);
        visited.Add(startNode);

        while (queue.Count > 0)
        {
            var currentNode = queue.Dequeue();
            yield return currentNode;

            foreach (var neighbor in _adjacencyList[currentNode].Keys)
            {
                if (visited.Add(neighbor))
                {
                    queue.Enqueue(neighbor);
                }
            }
        }
    }

    /// <summary>
    /// Esegue un attraversamento del grafo in profondità (Depth-First Search) a partire da un nodo.
    /// </summary>
    /// <param name="startNode">Il nodo da cui iniziare l'attraversamento.</param>
    /// <returns>Una collezione di nodi nell'ordine in cui sono stati visitati.</returns>
    /// <exception cref="ArgumentException">Lanciata se il nodo di partenza non esiste nel grafo.</exception>
    public IEnumerable<TNode> Dfs(TNode startNode)
    {
        if (!_adjacencyList.ContainsKey(startNode))
            throw new ArgumentException("Il nodo di partenza non esiste nel grafo.");

        var visited = new HashSet<TNode>();
        var stack = new Stack<TNode>();
        stack.Push(startNode);

        while (stack.Count > 0)
        {
            var currentNode = stack.Pop();
            if (visited.Add(currentNode))
            {
                yield return currentNode;
                // Itera sui vicini in ordine inverso per mantenere un ordine di visita più intuitivo (simile alla ricorsione)
                foreach (var neighbor in _adjacencyList[currentNode].Keys.Reverse())
                {
                    if (!visited.Contains(neighbor))
                    {
                        stack.Push(neighbor);
                    }
                }
            }
        }
    }

    /// <summary>
    /// Calcola il percorso più breve da un nodo di partenza a tutti gli altri nodi utilizzando l'algoritmo di Dijkstra.
    /// </summary>
    /// <param name="startNode">Il nodo di partenza.</param>
    /// <returns>Un dizionario che mappa ogni nodo raggiungibile al costo totale del percorso più breve a partire da `startNode`.</returns>
    /// <exception cref="ArgumentException">Lanciata se il nodo di partenza non esiste nel grafo.</exception>
    public Dictionary<TNode, double> Dijkstra(TNode startNode)
    {
        if (!_adjacencyList.ContainsKey(startNode))
            throw new ArgumentException("Il nodo di partenza non esiste nel grafo.");

        var distances = new Dictionary<TNode, double>();
        var priorityQueue = new PriorityQueue<TNode, double>();

        foreach (var node in _adjacencyList.Keys)
        {
            distances[node] = double.PositiveInfinity;
        }

        distances[startNode] = 0;

        priorityQueue.Enqueue(startNode, 0);

        while (priorityQueue.Count > 0)
        {
            var currentNode = priorityQueue.Dequeue();
            var currentDistance = distances[currentNode];

            foreach (var (neighbor, weight) in GetNeighbors(currentNode))
            {
                var newDistance = currentDistance + weight;
                if (newDistance < distances[neighbor])
                {
                    distances[neighbor] = newDistance;
                    priorityQueue.Enqueue(neighbor, newDistance);
                }
            }
        }

        return distances;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ALGORITMI CANONICI (L5) — parità con gmb_graph.c del runtime C
    // ────────────────────────────────────────────────────────────────────────

    /// <summary>Grado uscente (numero di archi che partono dal nodo). -1 se il nodo non esiste.</summary>
    public int GetOutDegree(TNode node)
        => _adjacencyList.TryGetValue(node, out var n) ? n.Count : -1;

    /// <summary>Grado totale (uscente + entrante) del nodo. -1 se non esiste.</summary>
    public int GetDegree(TNode node)
    {
        if (!_adjacencyList.ContainsKey(node)) return -1;
        return GetOutDegree(node) + GetInDegree(node);
    }

    /// <summary>Path più corto in termini di NUMERO di archi (BFS) tra from e to.
    /// Ritorna lista di nodi [from, ..., to], o vuota se non raggiungibile.</summary>
    public List<TNode> ShortestPath(TNode from, TNode to)
    {
        if (!_adjacencyList.ContainsKey(from) || !_adjacencyList.ContainsKey(to)) return [];
        if (EqualityComparer<TNode>.Default.Equals(from, to)) return [from];

        var prev = new Dictionary<TNode, TNode>();
        var visited = new HashSet<TNode> { from };
        var q = new Queue<TNode>();
        q.Enqueue(from);
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            foreach (var nb in _adjacencyList[cur].Keys)
            {
                if (visited.Add(nb))
                {
                    prev[nb] = cur;
                    if (EqualityComparer<TNode>.Default.Equals(nb, to))
                        return Reconstruct(from, to, prev);
                    q.Enqueue(nb);
                }
            }
        }
        return [];
    }

    /// <summary>Path a costo minimo (Dijkstra, archi non negativi) tra from e to.
    /// Ritorna lista di nodi [from, ..., to], o vuota se non raggiungibile.</summary>
    public List<TNode> DijkstraPath(TNode from, TNode to)
    {
        if (!_adjacencyList.ContainsKey(from) || !_adjacencyList.ContainsKey(to)) return [];
        if (EqualityComparer<TNode>.Default.Equals(from, to)) return [from];

        var dist = new Dictionary<TNode, double> { [from] = 0 };
        var prev = new Dictionary<TNode, TNode>();
        var pq = new PriorityQueue<TNode, double>();
        pq.Enqueue(from, 0);
        while (pq.Count > 0)
        {
            var cur = pq.Dequeue();
            if (!dist.TryGetValue(cur, out var cd)) continue;
            if (EqualityComparer<TNode>.Default.Equals(cur, to)) break;
            foreach (var (nb, w) in GetNeighbors(cur))
            {
                var nd = cd + w;
                if (!dist.TryGetValue(nb, out var existing) || nd < existing)
                {
                    dist[nb] = nd;
                    prev[nb] = cur;
                    pq.Enqueue(nb, nd);
                }
            }
        }
        if (!prev.ContainsKey(to)) return [];
        return Reconstruct(from, to, prev);
    }

    /// <summary>Bellman-Ford: distanze minime da start (ammette pesi negativi, rifiuta cicli
    /// negativi). Ritorna (distanze, haCicloNegativo). Le distanze dei nodi non raggiungibili
    /// sono +inf.</summary>
    public (Dictionary<TNode, double> Distances, bool HasNegativeCycle) BellmanFord(TNode start)
    {
        var dist = new Dictionary<TNode, double>();
        foreach (var n in _adjacencyList.Keys) dist[n] = double.PositiveInfinity;
        if (!_adjacencyList.ContainsKey(start)) return (dist, false);
        dist[start] = 0;

        var edges = AllEdges();
        for (var i = 0; i < _adjacencyList.Count - 1; i++)
        {
            var relaxed = false;
            foreach (var (f, t, w) in edges)
            {
                if (double.IsPositiveInfinity(dist[f])) continue;
                var nd = dist[f] + w;
                if (nd < dist[t]) { dist[t] = nd; relaxed = true; }
            }
            if (!relaxed) break;
        }
        foreach (var (f, t, w) in edges)
            if (!double.IsPositiveInfinity(dist[f]) && dist[f] + w < dist[t])
                return (dist, true);
        return (dist, false);
    }

    /// <summary>Floyd-Warshall: distanze minime tra TUTTE le coppie. Ritorna matrice
    /// [from][to] → distanza (Infinity se non raggiungibile).</summary>
    public Dictionary<TNode, Dictionary<TNode, double>> FloydWarshall()
    {
        var d = new Dictionary<TNode, Dictionary<TNode, double>>();
        foreach (var n in _adjacencyList.Keys)
            d[n] = _adjacencyList.Keys.ToDictionary(x => x, _ => double.PositiveInfinity);
        foreach (var n in _adjacencyList.Keys) d[n][n] = 0;
        foreach (var (f, t, w) in AllEdges()) d[f][t] = Math.Min(d[f][t], w);
        foreach (var k in _adjacencyList.Keys)
            foreach (var i in _adjacencyList.Keys)
                foreach (var j in _adjacencyList.Keys)
                    if (d[i][k] + d[k][j] < d[i][j]) d[i][j] = d[i][k] + d[k][j];
        return d;
    }

    /// <summary>FindPath con strategia esplicita: "bfs" (minimo numero archi), "dijkstra"
    /// (costo minimo), "astar" (costo minimo con euristica = peso minimo, equivalente a
    /// Dijkstra ma con coda ordinata da g+h). Ritorna [from, ..., to] o vuota.</summary>
    public List<TNode> FindPath(TNode from, TNode to, string strategy)
    {
        return strategy.ToLowerInvariant() switch
        {
            "bfs" or "breadth" => ShortestPath(from, to),
            "dijkstra" or "cost" => DijkstraPath(from, to),
            "astar" or "a*" => AStar(from, to),
            _ => DijkstraPath(from, to),
        };
    }

    /// <summary>A* con euristica default = 0 (equivalente a Dijkstra, coda g+h). Archi non
    /// negativi. Ritorna [from, ..., to] o vuota.</summary>
    public List<TNode> AStar(TNode from, TNode to, Func<TNode, TNode, double>? heuristic = null)
    {
        if (!_adjacencyList.ContainsKey(from) || !_adjacencyList.ContainsKey(to)) return [];
        if (EqualityComparer<TNode>.Default.Equals(from, to)) return [from];
        heuristic ??= (_, _) => 0;

        var g = new Dictionary<TNode, double> { [from] = 0 };
        var prev = new Dictionary<TNode, TNode>();
        var open = new PriorityQueue<TNode, double>();
        open.Enqueue(from, heuristic(from, to));
        var closed = new HashSet<TNode>();

        while (open.Count > 0)
        {
            var cur = open.Dequeue();
            if (EqualityComparer<TNode>.Default.Equals(cur, to)) break;
            if (!closed.Add(cur)) continue;
            foreach (var (nb, w) in GetNeighbors(cur))
            {
                var ng = g[cur] + w;
                if (g.TryGetValue(nb, out var existing) && ng >= existing) continue;
                g[nb] = ng;
                prev[nb] = cur;
                open.Enqueue(nb, ng + heuristic(nb, to));
            }
        }
        if (!prev.ContainsKey(to)) return [];
        return Reconstruct(from, to, prev);
    }

    /// <summary>Componenti (debolmente) connesse: tratta gli archi come non orientati (BFS).
    /// Ritorna lista di componenti, ognuna una lista di nodi.</summary>
    public List<List<TNode>> ConnectedComponents()
    {
        var visited = new HashSet<TNode>();
        var comps = new List<List<TNode>>();
        foreach (var n in _adjacencyList.Keys)
        {
            if (!visited.Add(n)) continue;
            var comp = new List<TNode>();
            var q = new Queue<TNode>();
            q.Enqueue(n);
            while (q.Count > 0)
            {
                var cur = q.Dequeue();
                comp.Add(cur);
                foreach (var nb in UndirectedNeighbors(cur))
                    if (visited.Add(nb)) q.Enqueue(nb);
            }
            comps.Add(comp);
        }
        comps.Sort((a, b) => b.Count.CompareTo(a.Count));
        return comps;
    }

    /// <summary>Componenti fortemente connesse (Tarjan). Ritorna lista di SCC, ognuna una
    /// lista di nodi.</summary>
    public List<List<TNode>> StronglyConnectedComponents()
    {
        var index = new Dictionary<TNode, int>();
        var low = new Dictionary<TNode, int>();
        var onStack = new HashSet<TNode>();
        var stack = new Stack<TNode>();
        var result = new List<List<TNode>>();
        var counter = 0;

        void StrongConnect(TNode v)
        {
            index[v] = low[v] = counter++;
            stack.Push(v);
            onStack.Add(v);
            foreach (var w in _adjacencyList[v].Keys)
            {
                if (!index.ContainsKey(w))
                {
                    StrongConnect(w);
                    low[v] = Math.Min(low[v], low[w]);
                }
                else if (onStack.Contains(w))
                    low[v] = Math.Min(low[v], index[w]);
            }
            if (low[v] == index[v])
            {
                var scc = new List<TNode>();
                TNode w;
                do { w = stack.Pop(); onStack.Remove(w); scc.Add(w); } while (!EqualityComparer<TNode>.Default.Equals(w, v));
                result.Add(scc);
            }
        }

        foreach (var n in _adjacencyList.Keys)
            if (!index.ContainsKey(n)) StrongConnect(n);
        result.Sort((a, b) => b.Count.CompareTo(a.Count));
        return result;
    }

    /// <summary>True se il grafo contiene almeno un ciclo (DFS, colorazione 3 stati).</summary>
    public bool HasCycle()
    {
        var color = new Dictionary<TNode, int>(); // 0=bianco 1=grigio 2=nero
        bool Dfs(TNode n)
        {
            color[n] = 1;
            foreach (var nb in _adjacencyList[n].Keys)
            {
                if (!color.ContainsKey(nb) && Dfs(nb)) return true;
                else if (color[nb] == 1) return true;
            }
            color[n] = 2;
            return false;
        }
        foreach (var n in _adjacencyList.Keys)
            if (!color.ContainsKey(n) && Dfs(n)) return true;
        return false;
    }

    /// <summary>Ordinamento topologico (Kahn). Ritorna null se il grafo ha un ciclo.</summary>
    public List<TNode>? TopologicalSort()
    {
        var indeg = new Dictionary<TNode, int>();
        foreach (var n in _adjacencyList.Keys) indeg[n] = 0;
        foreach (var (_, t, _) in AllEdges()) indeg[t] = indeg.GetValueOrDefault(t) + 1;
        var q = new Queue<TNode>(indeg.Where(kv => kv.Value == 0).Select(kv => kv.Key));
        var result = new List<TNode>();
        while (q.Count > 0)
        {
            var n = q.Dequeue();
            result.Add(n);
            foreach (var nb in _adjacencyList[n].Keys)
                if (--indeg[nb] == 0) q.Enqueue(nb);
        }
        return result.Count == _adjacencyList.Count ? result : null;
    }

    /// <summary>True se il grafo è bipartito (2-colorazione BFS, archi trattati come non
    /// orientati). I nodi isolati sono ignorati.</summary>
    public bool IsBipartite()
    {
        var color = new Dictionary<TNode, int>();
        foreach (var n in _adjacencyList.Keys)
        {
            if (color.ContainsKey(n)) continue;
            var q = new Queue<TNode>();
            q.Enqueue(n);
            color[n] = 0;
            while (q.Count > 0)
            {
                var cur = q.Dequeue();
                foreach (var nb in UndirectedNeighbors(cur))
                {
                    if (!color.ContainsKey(nb)) { color[nb] = 1 - color[cur]; q.Enqueue(nb); }
                    else if (color[nb] == color[cur]) return false;
                }
            }
        }
        return true;
    }

    /// <summary>Closeness centrality (BFS su archi non pesati): 1 / (somma distanze verso tutti
    /// i nodi raggiungibili). 0 se il nodo non esiste.</summary>
    public double ClosenessCentrality(TNode node)
    {
        if (!_adjacencyList.ContainsKey(node)) return 0;
        var dist = new Dictionary<TNode, int>();
        var q = new Queue<TNode>();
        q.Enqueue(node);
        dist[node] = 0;
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            foreach (var nb in _adjacencyList[cur].Keys)
                if (!dist.ContainsKey(nb)) { dist[nb] = dist[cur] + 1; q.Enqueue(nb); }
        }
        var sum = dist.Values.Where(d => d > 0).Sum(d => (double)d);
        return sum > 0 ? (dist.Count - 1) / sum : 0;
    }

    /// <summary>Betweenness centrality (Brandes, archi non pesati) per ogni nodo. Ritorna
    /// mappa nodo → betweenness normalizzata (0..1 con normalizzazione per coppie).</summary>
    public Dictionary<TNode, double> BetweennessCentrality()
    {
        var cb = _adjacencyList.Keys.ToDictionary(n => n, _ => 0.0);
        foreach (var s in _adjacencyList.Keys)
        {
            var stack = new Stack<TNode>();
            var pred = new Dictionary<TNode, List<TNode>>();
            var sigma = _adjacencyList.Keys.ToDictionary(n => n, _ => 0.0);
            var dist = _adjacencyList.Keys.ToDictionary(n => n, _ => -1);
            sigma[s] = 1;
            dist[s] = 0;
            var q = new Queue<TNode>();
            q.Enqueue(s);
            while (q.Count > 0)
            {
                var v = q.Dequeue();
                stack.Push(v);
                foreach (var w in _adjacencyList[v].Keys)
                {
                    if (dist[w] < 0)
                    {
                        dist[w] = dist[v] + 1;
                        q.Enqueue(w);
                    }
                    if (dist[w] == dist[v] + 1)
                    {
                        sigma[w] += sigma[v];
                        if (!pred.ContainsKey(w)) pred[w] = [];
                        pred[w].Add(v);
                    }
                }
            }
            var delta = _adjacencyList.Keys.ToDictionary(n => n, _ => 0.0);
            while (stack.Count > 0)
            {
                var w = stack.Pop();
                if (pred.TryGetValue(w, out var pw))
                    foreach (var v in pw)
                        delta[v] += sigma[v] / sigma[w] * (1 + delta[w]);
                if (!EqualityComparer<TNode>.Default.Equals(w, s)) cb[w] += delta[w];
            }
        }
        // normalizzazione (archi non pesati): n<=2 → 0; altrimenti /((n-1)(n-2))
        var n = _adjacencyList.Count;
        var norm = n <= 2 ? 0.0 : 1.0 / ((n - 1) * (n - 2));
        return cb.ToDictionary(kv => kv.Key, kv => kv.Value * norm);
    }

    /// <summary>PageRank (power iteration, stessa semantica del runtime C e di WikiGraph).
    /// Ritorna (nodo, rank) per tutti i nodi, ordinati per rank decrescente.</summary>
    public List<(TNode Node, double Rank)> PageRank(double damping = 0.85, int iterations = 100)
    {
        var n = _adjacencyList.Count;
        if (n == 0) return [];
        var nodes = _adjacencyList.Keys.ToList();
        var rank = nodes.ToDictionary(x => x, _ => 1.0 / n);
        var outdeg = nodes.ToDictionary(x => x, x => _adjacencyList[x].Count);
        var maxIter = iterations > 0 ? iterations : 100;
        var d = damping <= 0 ? 0.85 : damping;
        var b = (1 - d) / n;

        for (var it = 0; it < maxIter; it++)
        {
            var next = nodes.ToDictionary(x => x, _ => b);
            foreach (var i in nodes)
            {
                if (outdeg[i] == 0) continue;
                var share = d * rank[i] / outdeg[i];
                foreach (var (nb, _) in GetNeighbors(i)) next[nb] += share;
            }
            foreach (var i in nodes)
                if (outdeg[i] == 0)
                {
                    var share = d * rank[i] / n;
                    foreach (var nb in nodes) next[nb] += share;
                }
            rank = next;
        }
        return nodes.Select(x => (x, rank[x])).OrderByDescending(x => x.Item2).ToList();
    }

    /// <summary>Diametro (archi non pesati): massima eccentricità tra le coppie connesse.
    /// Ritorna la distanza massima, o 0 se il grafo ha < 2 nodi.</summary>
    public double Diameter()
    {
        var fw = FloydWarshall();
        var max = 0.0;
        foreach (var (_, row) in fw)
            foreach (var d in row.Values)
                if (!double.IsPositiveInfinity(d) && d > max) max = d;
        return max;
    }

    /// <summary>Eccentricità di un nodo (massima distanza verso i nodi raggiungibili).
    /// -1 se il nodo non esiste.</summary>
    public double Eccentricity(TNode node)
    {
        if (!_adjacencyList.ContainsKey(node)) return -1;
        var dist = new Dictionary<TNode, int>();
        var q = new Queue<TNode>();
        q.Enqueue(node);
        dist[node] = 0;
        while (q.Count > 0)
        {
            var cur = q.Dequeue();
            foreach (var nb in _adjacencyList[cur].Keys)
                if (!dist.ContainsKey(nb)) { dist[nb] = dist[cur] + 1; q.Enqueue(nb); }
        }
        return dist.Count > 1 ? dist.Values.Max(d => (double)d) : 0;
    }

    /// <summary>Coefficiente di clustering locale di un nodo (frazione delle coppie di vicini
    /// collegati tra loro, con archi non orientati). 0 se il nodo non esiste o ha < 2 vicini.</summary>
    public double ClusteringCoefficient(TNode node)
    {
        if (!_adjacencyList.TryGetValue(node, out var neigh)) return 0;
        var neighbors = neigh.Keys.ToHashSet();
        if (neighbors.Count < 2) return 0;
        var links = 0;
        var nbList = neighbors.ToList();
        for (var i = 0; i < nbList.Count; i++)
            for (var j = i + 1; j < nbList.Count; j++)
            {
                var a = nbList[i]; var b = nbList[j];
                if (_adjacencyList.TryGetValue(a, out var na) && na.ContainsKey(b)) links++;
                else if (_adjacencyList.TryGetValue(b, out var nb2) && nb2.ContainsKey(a)) links++;
            }
        var possible = (double)neighbors.Count * (neighbors.Count - 1) / 2;
        return possible > 0 ? links / possible : 0;
    }

    /// <summary>Minimum Spanning Tree (Kruskal, archi trattati come non orientati).
    /// Ritorna lista di (from, to, weight) dell'albero di copertura minimo.</summary>
    public List<(TNode From, TNode To, double Weight)> MinimumSpanningTree()
    {
        var parent = new Dictionary<TNode, TNode>();
        TNode Find(TNode x)
        {
            // path compression iterativa (NO path halving che non converge quando la radice
            // è raggiunta: parent[x]=parent[parent[x]] resta radice e il while non termina).
            var root = x;
            while (!EqualityComparer<TNode>.Default.Equals(parent[root], root)) root = parent[root];
            while (!EqualityComparer<TNode>.Default.Equals(parent[x], x))
            {
                var next = parent[x];
                parent[x] = root;
                x = next;
            }
            return root;
        }
        foreach (var n in _adjacencyList.Keys) parent[n] = n;
        var edges = AllEdges().OrderBy(e => e.Weight).ToList();
        var mst = new List<(TNode, TNode, double)>();
        foreach (var (f, t, w) in edges)
        {
            var rf = Find(f); var rt = Find(t);
            if (EqualityComparer<TNode>.Default.Equals(rf, rt)) continue;
            parent[rf] = rt;
            mst.Add((f, t, w));
        }
        return mst;
    }

    /// <summary>Edge list: tutte le coppie (from, to, weight).</summary>
    public List<(TNode From, TNode To, double Weight)> AllEdges()
    {
        var list = new List<(TNode, TNode, double)>();
        foreach (var (from, neighbors) in _adjacencyList)
            foreach (var (to, w) in neighbors)
                list.Add((from, to, w));
        return list;
    }

    /// <summary>Flusso massimo (Edmonds-Karp) da source a sink. Capacità = pesi degli archi
    /// (orientati; archi paralleli sommati). Ritorna il flusso (>=0), o -1 se source/sink mancano.</summary>
    public double MaxFlow(TNode source, TNode sink)
    {
        if (!_adjacencyList.ContainsKey(source) || !_adjacencyList.ContainsKey(sink)) return -1.0;
        var nodes = _adjacencyList.Keys.ToList();
        var idx = nodes.Select((x, i) => (x, i)).ToDictionary(t => t.x, t => t.i);
        var n = nodes.Count;
        var cap = new double[n, n];
        foreach (var (f, ns) in _adjacencyList)
            foreach (var (t, w) in ns)
                if (!EqualityComparer<TNode>.Default.Equals(f, t) && w > 0) cap[idx[f], idx[t]] += w;
        int s = idx[source], tt = idx[sink];
        if (s == tt) return 0.0;
        var flow = 0.0;
        while (true)
        {
            var prev = new int[n];
            for (var i = 0; i < n; i++) prev[i] = -1;
            prev[s] = s;
            var q = new Queue<int>();
            q.Enqueue(s);
            while (q.Count > 0 && prev[tt] == -1)
            {
                var u = q.Dequeue();
                for (var v = 0; v < n; v++)
                    if (prev[v] == -1 && cap[u, v] > 1e-12) { prev[v] = u; q.Enqueue(v); }
            }
            if (prev[tt] == -1) break;
            var aug = double.PositiveInfinity;
            for (var v = tt; v != s; v = prev[v]) aug = Math.Min(aug, cap[prev[v], v]);
            for (var v = tt; v != s; v = prev[v]) { cap[prev[v], v] -= aug; cap[v, prev[v]] += aug; }
            flow += aug;
        }
        return flow;
    }

    /// <summary>Community detection (Louvain, archi trattati come non orientati). resolution = 1.0
    /// tipico. Ritorna la community (id compatto) assegnata a ogni nodo.</summary>
    public Dictionary<TNode, int> Louvain(double resolution = 1.0)
    {
        var nodes = _adjacencyList.Keys.ToList();
        var n0 = nodes.Count;
        var result = new Dictionary<TNode, int>();
        if (n0 == 0) return result;
        if (resolution <= 0) resolution = 1.0;
        var idx = nodes.Select((x, i) => (x, i)).ToDictionary(t => t.x, t => t.i);
        var w0 = new double[n0, n0];
        var deg0 = new double[n0];
        foreach (var (f, ns) in _adjacencyList)
            foreach (var (t, w) in ns)
            {
                if (EqualityComparer<TNode>.Default.Equals(f, t)) continue;
                int a = idx[f], b = idx[t];
                var ww = w > 0 ? w : 0.0;
                w0[a, b] += ww; w0[b, a] += ww; deg0[a] += ww; deg0[b] += ww;
            }
        var n = n0;
        var cW = w0; var cdeg = deg0;
        var nodeComm = Enumerable.Range(0, n0).ToArray();
        for (var level = 0; level < 64; level++)
        {
            var m2 = 0.0;
            for (var i = 0; i < n; i++) m2 += cdeg[i];
            if (m2 <= 0) break;
            var comm = Enumerable.Range(0, n).ToArray();
            var stot = new double[n];
            for (var i = 0; i < n; i++) stot[i] = cdeg[i];
            var moved = LouvainLocalMove(n, cW, cdeg, m2, resolution, comm, stot);
            var remap = new int[n];
            for (var i = 0; i < n; i++) remap[i] = -1;
            var nc = 0;
            for (var i = 0; i < n; i++) if (remap[comm[i]] < 0) remap[comm[i]] = nc++;
            for (var i = 0; i < n0; i++) nodeComm[i] = remap[comm[nodeComm[i]]];
            if (!moved || nc == n) break;
            var nW = new double[nc, nc];
            var ndeg = new double[nc];
            for (var i = 0; i < n; i++)
            {
                var ri = remap[comm[i]];
                ndeg[ri] += cdeg[i];
                for (var j = 0; j < n; j++)
                {
                    var w = cW[i, j];
                    if (w <= 0) continue;
                    var rj = remap[comm[j]];
                    if (ri != rj) nW[ri, rj] += w;
                }
            }
            cW = nW; cdeg = ndeg; n = nc;
        }
        var seen = new Dictionary<int, int>();
        var cid = 0;
        for (var i = 0; i < n0; i++)
        {
            var c = nodeComm[i];
            if (!seen.TryGetValue(c, out var id)) { id = cid++; seen[c] = id; }
            result[nodes[i]] = id;
        }
        return result;
    }

    private static bool LouvainLocalMove(int n, double[,] w, double[] deg, double m2, double resolution, int[] comm, double[] stot)
    {
        var kintoc = new double[n];
        var touched = new int[n];
        var moved = false;
        var changed = true;
        while (changed)
        {
            changed = false;
            for (var i = 0; i < n; i++)
            {
                var ci = comm[i];
                var nt = 0;
                for (var j = 0; j < n; j++)
                {
                    var ww = w[i, j];
                    if (ww > 0) { var cj = comm[j]; if (kintoc[cj] == 0.0) touched[nt++] = cj; kintoc[cj] += ww; }
                }
                stot[ci] -= deg[i];
                var bestC = ci;
                var bestGain = kintoc[ci] - resolution * stot[ci] * deg[i] / m2;
                for (var q = 0; q < nt; q++)
                {
                    var c = touched[q];
                    if (c == ci) continue;
                    var gain = kintoc[c] - resolution * stot[c] * deg[i] / m2;
                    if (gain > bestGain) { bestGain = gain; bestC = c; }
                }
                stot[bestC] += deg[i];
                if (bestC != ci) { comm[i] = bestC; changed = true; moved = true; }
                for (var q = 0; q < nt; q++) kintoc[touched[q]] = 0.0;
            }
        }
        return moved;
    }

    /// <summary>Vicini trattando gli archi come non orientati (per componenti/bipartito).</summary>
    private IEnumerable<TNode> UndirectedNeighbors(TNode node)
    {
        if (_adjacencyList.TryGetValue(node, out var outN))
            foreach (var n in outN.Keys) yield return n;
        foreach (var (from, neighbors) in _adjacencyList)
            if (!EqualityComparer<TNode>.Default.Equals(from, node) && neighbors.ContainsKey(node))
                yield return from;
    }

    private static List<TNode> Reconstruct(TNode from, TNode to, Dictionary<TNode, TNode> prev)
    {
        var path = new List<TNode>();
        for (var x = to; ; x = prev[x])
        {
            path.Add(x);
            if (EqualityComparer<TNode>.Default.Equals(x, from)) break;
            if (!prev.ContainsKey(x)) break;
        }
        path.Reverse();
        return path;
    }
}
