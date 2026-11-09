// FILE: StdLib.Numerics.KdTree.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>Nodo del kd-tree: punto, dimensione di split, piano di split.</summary>
public sealed class KdTreeNode
{
    public int PointIndex;
    public int SplitDim;
    public double SplitValue;
    public KdTreeNode? Left;
    public KdTreeNode? Right;
}

/// <summary>kd-tree per nearest-neighbor (scipy.spatial.KDTree): build ricorsiva con split
/// sulla dimensione a massima dispersione, query k-NN branch-and-bound e query_ball_point.</summary>
public sealed class KdTree
{
    public double[,] Points { get; init; } = new double[0, 0];
    public int Count { get; init; }
    public int Dims { get; init; }
    public KdTreeNode? Root { get; init; }
}

/// <summary>Implementazione kd-tree con parità a scipy.spatial.KDTree (query/query_ball_point).</summary>
public static partial class Numerics
{
    public static KdTree KdTreeBuild(double[,] points)
    {
        var n = points.GetLength(0);
        var m = points.GetLength(1);
        var idx = new int[n];
        for (var i = 0; i < n; i++) idx[i] = i;
        return new KdTree { Points = points, Count = n, Dims = m, Root = BuildNode(points, idx, m) };
    }

    private static KdTreeNode? BuildNode(double[,] pts, int[] idx, int m)
    {
        if (idx.Length == 0) return null;
        int splitDim = 0;
        double bestSpread = -1;
        for (var d = 0; d < m; d++)
        {
            double min = double.MaxValue, max = double.MinValue;
            foreach (var i in idx) { var v = pts[i, d]; if (v < min) min = v; if (v > max) max = v; }
            var spread = max - min;
            if (spread > bestSpread) { bestSpread = spread; splitDim = d; }
        }
        var sorted = idx.OrderBy(i => pts[i, splitDim]).ToArray();
        var mid = sorted.Length / 2;
        var node = new KdTreeNode
        {
            PointIndex = sorted[mid],
            SplitDim = splitDim,
            SplitValue = pts[sorted[mid], splitDim]
        };
        var left = new int[mid];
        Array.Copy(sorted, 0, left, 0, mid);
        var right = new int[sorted.Length - mid - 1];
        Array.Copy(sorted, mid + 1, right, 0, right.Length);
        node.Left = BuildNode(pts, left, m);
        node.Right = BuildNode(pts, right, m);
        return node;
    }

    /// <summary>query k-NN (scipy KDTree.query): k punti più vicini a q, ordinati per distanza
    /// (prodotto euclideo). Ritorna distanze e indici.</summary>
    public static (double[] distances, int[] indices) KdTreeQuery(KdTree tree, double[] query, int k)
    {
        if (tree.Root == null) return (Array.Empty<double>(), Array.Empty<int>());
        if (k < 1) k = 1;
        var best = new List<(double d, int i)>();
        var worst = new[] { double.PositiveInfinity };
        Search(query, tree, tree.Root, best, k, worst);
        var dists = new double[best.Count];
        var idxs = new int[best.Count];
        for (var j = 0; j < best.Count; j++) { dists[j] = Math.Sqrt(best[j].d); idxs[j] = best[j].i; }
        return (dists, idxs);
    }

    private static void Search(double[] q, KdTree tree, KdTreeNode? node, List<(double d, int i)> best, int k, double[] worst)
    {
        if (node == null) return;
        var dx = q[0] - tree.Points[node.PointIndex, 0];
        var dy = q[1] - tree.Points[node.PointIndex, 1];
        var d2 = dx * dx + dy * dy;
        if (tree.Dims > 2)
            for (var d = 2; d < tree.Dims; d++)
            {
                var dz = q[d] - tree.Points[node.PointIndex, d];
                d2 += dz * dz;
            }
        if (best.Count < k) { best.Add((d2, node.PointIndex)); best.Sort(static (a, b) => a.d.CompareTo(b.d)); worst[0] = best[^1].d; }
        else if (d2 < worst[0])
        {
            best.RemoveAt(best.Count - 1);
            best.Add((d2, node.PointIndex));
            best.Sort(static (a, b) => a.d.CompareTo(b.d));
            worst[0] = best[^1].d;
        }
        var plane = q[node.SplitDim] - node.SplitValue;
        var near = plane < 0 ? node.Left : node.Right;
        var far = plane < 0 ? node.Right : node.Left;
        Search(q, tree, near, best, k, worst);
        if (best.Count < k || plane * plane < worst[0]) Search(q, tree, far, best, k, worst);
    }

    /// <summary>query_ball_point (scipy KDTree.query_ball_point): indici dei punti entro raggio r.</summary>
    public static int[] KdTreeQueryBall(KdTree tree, double[] query, double radius)
    {
        var result = new List<int>();
        SearchBall(query, tree, tree.Root, radius, radius * radius, result);
        return result.ToArray();
    }

    private static void SearchBall(double[] q, KdTree tree, KdTreeNode? node, double r, double r2, List<int> outList)
    {
        if (node == null) return;
        var dx = q[0] - tree.Points[node.PointIndex, 0];
        var dy = q[1] - tree.Points[node.PointIndex, 1];
        var d2 = dx * dx + dy * dy;
        if (tree.Dims > 2)
            for (var d = 2; d < tree.Dims; d++)
            {
                var dz = q[d] - tree.Points[node.PointIndex, d];
                d2 += dz * dz;
            }
        if (d2 <= r2) outList.Add(node.PointIndex);
        var plane = q[node.SplitDim] - node.SplitValue;
        if (plane < 0)
        {
            SearchBall(q, tree, node.Left, r, r2, outList);
            if (plane * plane <= r2) SearchBall(q, tree, node.Right, r, r2, outList);
        }
        else
        {
            SearchBall(q, tree, node.Right, r, r2, outList);
            if (plane * plane <= r2) SearchBall(q, tree, node.Left, r, r2, outList);
        }
    }
}
