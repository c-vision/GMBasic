// FILE: Basic.SQLite.Vector.VectorHnswIndex
namespace Basic.SQLite.Vector;

/// <summary>
/// In-memory HNSW approximate nearest-neighbor index over float32 vectors.
/// </summary>
/// <remarks>
/// ponytail ADR Fase 2: C# in-memory HNSW rebuilt lazily from SQLite BLOB embeddings —
/// not sqlite-vec (native bundle friction with Microsoft.Data.Sqlite on macOS).
/// Ceiling: no disk persistence, full rebuild when embedding count changes;
/// thread-safe via ReaderWriterLockSlim (concurrent reads, exclusive writes).
/// Upgrade path: sqlite-vec load extension or external vector service at 100k+ scale.
/// </remarks>
public sealed class VectorHnswIndex
{
    private const int M = 16;
    private const int MMax0 = M * 2;
    private const int EfConstruction = 64;

    private readonly VectorMetric _metric;
    private readonly List<Node> _nodes = [];
    private int _entryPoint = -1;
    private int _maxLevel;
    private readonly ReaderWriterLockSlim _lock = new();

    public VectorHnswIndex(VectorMetric metric) => _metric = metric;

    public int Count
    {
        get { _lock.EnterReadLock(); try { return _nodes.Count; } finally { _lock.ExitReadLock(); } }
    }

    public void Add(long chunkId, ReadOnlySpan<float> vector)
    {
        _lock.EnterWriteLock();
        try
        {
            var copy = vector.ToArray();
            var level = RandomLevel();
            var node = new Node(chunkId, copy, level);
            var id = _nodes.Count;
            _nodes.Add(node);

            if (_entryPoint < 0)
            {
                _entryPoint = id;
                _maxLevel = level;
                return;
            }

            var query = copy.AsSpan();
            var entry = _entryPoint;
            if (level < _maxLevel)
            {
                var nearest = SearchLayerSingle(query, entry, _maxLevel, level + 1);
                entry = nearest;
            }

            for (var lc = Math.Min(level, _maxLevel); lc >= 0; lc--)
            {
                var candidates = SearchLayer(query, entry, lc, EfConstruction);
                var neighbors = SelectNeighbors(query, candidates, lc == 0 ? MMax0 : M);
                foreach (var n in neighbors)
                    ConnectBidirectional(id, n, lc);

                if (candidates.Count > 0)
                    entry = candidates[0];
            }

            if (level > _maxLevel)
            {
                _maxLevel = level;
                _entryPoint = id;
            }
        }
        finally { _lock.ExitWriteLock(); }
    }

    /// <summary>Returns chunk ids in approximate best-first order (best first).</summary>
    public List<long> Search(ReadOnlySpan<float> query, int topK, int efSearch)
    {
        _lock.EnterReadLock();
        try
        {
            if (_nodes.Count == 0 || topK <= 0)
                return [];

        efSearch = Math.Max(efSearch, topK);
        var entry = _entryPoint;
        for (var lc = _maxLevel; lc > 0; lc--)
            entry = SearchLayerSingle(query, entry, lc, 1);

            var candidates = SearchLayer(query, entry, 0, efSearch);
            var results = new List<long>(Math.Min(topK, candidates.Count));
            for (var i = 0; i < candidates.Count && results.Count < topK; i++)
                results.Add(_nodes[candidates[i]].ChunkId);
            return results;
        }
        finally { _lock.ExitReadLock(); }
    }

    private int RandomLevel()
    {
        var level = 0;
        while (Random.Shared.NextDouble() < 0.5 && level < 16)
            level++;
        return level;
    }

    private float Distance(ReadOnlySpan<float> a, ReadOnlySpan<float> b) =>
        -VectorMath.Score(_metric, a, b);

    private int SearchLayerSingle(ReadOnlySpan<float> query, int entry, int level, int ef)
    {
        var visited = new HashSet<int> { entry };
        var candidates = new PriorityQueue<int, float>();
        var results = new PriorityQueue<int, float>();
        var distEntry = Distance(query, _nodes[entry].Vector);
        candidates.Enqueue(entry, distEntry);
        results.Enqueue(entry, -distEntry);
        var worst = distEntry;

        while (candidates.TryDequeue(out var current, out var currentDist))
        {
            if (currentDist > worst)
                break;

            foreach (var neighbor in _nodes[current].Neighbors[level])
            {
                if (!visited.Add(neighbor))
                    continue;

                var d = Distance(query, _nodes[neighbor].Vector);
                if (results.Count < ef || d < worst)
                {
                    candidates.Enqueue(neighbor, d);
                    results.Enqueue(neighbor, -d);
                    if (results.Count > ef)
                    {
                        results.TryDequeue(out _, out var removed);
                        worst = -removed;
                    }
                }
            }
        }

        results.TryDequeue(out var best, out _);
        return best;
    }

    private List<int> SearchLayer(ReadOnlySpan<float> query, int entry, int level, int ef)
    {
        var visited = new HashSet<int> { entry };
        var candidates = new PriorityQueue<int, float>();
        var results = new PriorityQueue<int, float>();
        var distEntry = Distance(query, _nodes[entry].Vector);
        candidates.Enqueue(entry, distEntry);
        results.Enqueue(entry, -distEntry);
        var worst = distEntry;

        while (candidates.TryDequeue(out var current, out var currentDist))
        {
            if (currentDist > worst)
                break;

            foreach (var neighbor in _nodes[current].Neighbors[level])
            {
                if (!visited.Add(neighbor))
                    continue;

                var d = Distance(query, _nodes[neighbor].Vector);
                if (results.Count < ef || d < worst)
                {
                    candidates.Enqueue(neighbor, d);
                    results.Enqueue(neighbor, -d);
                    if (results.Count > ef)
                    {
                        results.TryDequeue(out _, out var removed);
                        worst = -removed;
                    }
                }
            }
        }

        var ordered = new List<int>(results.Count);
        while (results.Count > 0)
        {
            results.TryDequeue(out var id, out _);
            ordered.Add(id);
        }

        ordered.Reverse();
        return ordered;
    }

    private List<int> SelectNeighbors(ReadOnlySpan<float> query, List<int> candidates, int maxNeighbors)
    {
        if (candidates.Count <= maxNeighbors)
            return candidates;

        var scored = new List<(int Id, float Dist)>(candidates.Count);
        foreach (var id in candidates)
            scored.Add((id, Distance(query, _nodes[id].Vector)));
        scored.Sort((a, b) => a.Dist.CompareTo(b.Dist));
        return scored.Take(maxNeighbors).Select(x => x.Id).ToList();
    }

    private void ConnectBidirectional(int from, int to, int level)
    {
        var maxConn = level == 0 ? MMax0 : M;
        AddNeighbor(from, to, level, maxConn);
        AddNeighbor(to, from, level, maxConn);
    }

    private void AddNeighbor(int nodeId, int neighborId, int level, int maxConn)
    {
        var list = _nodes[nodeId].Neighbors[level];
        if (list.Contains(neighborId))
            return;

        list.Add(neighborId);
        if (list.Count <= maxConn)
            return;

        var nodeVector = _nodes[nodeId].Vector;
        var farthest = list[0];
        var farthestDist = Distance(nodeVector, _nodes[farthest].Vector);
        for (var i = 1; i < list.Count; i++)
        {
            var d = Distance(nodeVector, _nodes[list[i]].Vector);
            if (d > farthestDist)
            {
                farthest = list[i];
                farthestDist = d;
            }
        }

        list.Remove(farthest);
    }

    private sealed class Node
    {
        public Node(long chunkId, float[] vector, int level)
        {
            ChunkId = chunkId;
            Vector = vector;
            Neighbors = new List<int>[level + 1];
            for (var i = 0; i <= level; i++)
                Neighbors[i] = [];
        }

        public long ChunkId { get; }
        public float[] Vector { get; }
        public List<int>[] Neighbors { get; }
    }
}
