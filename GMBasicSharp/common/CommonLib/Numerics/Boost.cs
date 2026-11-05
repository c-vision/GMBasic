// FILE: CommonLib.Numerics.Boost.cs
// Gradient boosting (parità gs_boost.c) con modello serializzato in double[].
namespace CommonLib;

public static partial class Numerics
{
    private sealed class BNode
    {
        public int Feat = -1;
        public double Thr;
        public double Leaf;
        public BNode? L;
        public BNode? R;
    }

    private sealed class BLeaf
    {
        public BNode Nd = null!;
        public int[] Idx = Array.Empty<int>();
        public int N;
        public int Depth;
    }

    private static double BMean(double[] y, int[] idx, int n)
    {
        double s = 0;
        for (var i = 0; i < n; i++) s += y[idx[i]];
        return n > 0 ? s / n : 0.0;
    }

    private static bool BBestSplit(double[][] X, double[] y, int[] idx, int n, int p,
        out int bf, out double bt, out double bgain)
    {
        bf = -1; bt = 0; bgain = 0;
        double s = 0, ss = 0;
        for (var i = 0; i < n; i++) { var v = y[idx[i]]; s += v; ss += v * v; }
        var sseP = ss - s * s / n;
        var best = -1.0;
        for (var f = 0; f < p; f++)
        {
            for (var i = 0; i < n; i++)
            {
                var thr = X[idx[i]][f];
                double sl = 0, sll = 0, sr = 0, srr = 0;
                int nl = 0, nr = 0;
                for (var j = 0; j < n; j++)
                {
                    var v = X[idx[j]][f];
                    var yy = y[idx[j]];
                    if (v <= thr) { sl += yy; sll += yy * yy; nl++; }
                    else { sr += yy; srr += yy * yy; nr++; }
                }
                if (nl < 1 || nr < 1) continue;
                var sse = (sll - sl * sl / nl) + (srr - sr * sr / nr);
                var gain = sseP - sse;
                if (gain > best) { best = gain; bf = f; bt = thr; }
            }
        }
        bgain = best;
        return bf >= 0;
    }

    private static void BPartition(double[][] X, int[] idx, int n, int f, double t,
        out int[] li, out int nl, out int[] ri, out int nr)
    {
        li = new int[n]; ri = new int[n];
        nl = 0; nr = 0;
        for (var j = 0; j < n; j++)
        {
            if (X[idx[j]][f] <= t) li[nl++] = idx[j];
            else ri[nr++] = idx[j];
        }
    }

    private static BNode BNodeNew(double[] y, int[] idx, int n)
        => new() { Feat = -1, Leaf = BMean(y, idx, n) };

    private static BNode BDepthwise(double[][] X, double[] y, int[] idx, int n, int p,
        int depth, int maxd, int minleaf)
    {
        var nd = BNodeNew(y, idx, n);
        if (depth >= maxd || n < 2 * minleaf) return nd;
        if (!BBestSplit(X, y, idx, n, p, out var bf, out var bt, out var g) || g <= 0.0) return nd;
        BPartition(X, idx, n, bf, bt, out var li, out var nl, out var ri, out var nr);
        nd.Feat = bf; nd.Thr = bt;
        nd.L = BDepthwise(X, y, li, nl, p, depth + 1, maxd, minleaf);
        nd.R = BDepthwise(X, y, ri, nr, p, depth + 1, maxd, minleaf);
        return nd;
    }

    private static BNode BLeafwise(double[][] X, double[] y, int[] idx0, int n0, int p,
        int maxd, int minleaf)
    {
        var root = BNodeNew(y, idx0, n0);
        var cap = 1 << (maxd + 1);
        var lv = new BLeaf[cap];
        var nl = 1;
        var a = new int[n0];
        for (var i = 0; i < n0; i++) a[i] = idx0[i];
        lv[0] = new BLeaf { Nd = root, Idx = a, N = n0, Depth = 0 };
        var maxNodes = (1 << maxd) - 1;
        var nodes = 1;
        while (nodes < maxNodes)
        {
            var best = -1; var bbf = -1; double bbt = 0, bg = 0;
            for (var k = 0; k < nl; k++)
            {
                if (lv[k].Depth >= maxd || lv[k].N < 2 * minleaf) continue;
                if (BBestSplit(X, y, lv[k].Idx, lv[k].N, p, out var bf, out var bt, out var g) && g > bg)
                { bg = g; best = k; bbf = bf; bbt = bt; }
            }
            if (best < 0 || bg <= 0) break;
            var sp = lv[best];
            BPartition(X, sp.Idx, sp.N, bbf, bbt, out var li, out var nli, out var ri, out var nri);
            sp.Nd.Feat = bbf; sp.Nd.Thr = bbt;
            sp.Nd.L = BNodeNew(y, li, nli);
            sp.Nd.R = BNodeNew(y, ri, nri);
            lv[best] = new BLeaf { Nd = sp.Nd.L, Idx = li, N = nli, Depth = sp.Depth + 1 };
            lv[nl] = new BLeaf { Nd = sp.Nd.R, Idx = ri, N = nri, Depth = sp.Depth + 1 };
            nl++;
            nodes++;
        }
        return root;
    }

    private static BNode BSymmetric(double[][] X, double[] y, int[] idx0, int n0, int p,
        int maxd, int minleaf)
    {
        var root = BNodeNew(y, idx0, n0);
        var cap = 1 << (maxd + 1);
        var lv = new BLeaf[cap];
        var nl = 1;
        var a = new int[n0];
        for (var i = 0; i < n0; i++) a[i] = idx0[i];
        lv[0] = new BLeaf { Nd = root, Idx = a, N = n0, Depth = 0 };
        for (var depth = 0; depth < maxd; depth++)
        {
            var tot = 0;
            for (var k = 0; k < nl; k++) tot += lv[k].N;
            if (tot == 0) break;
            var comb = new int[tot];
            var c = 0;
            for (var k = 0; k < nl; k++)
                for (var i = 0; i < lv[k].N; i++) comb[c++] = lv[k].Idx[i];
            int bf = -1;
            double bt = 0, g = 0;
            var ok = tot >= 2 * minleaf && BBestSplit(X, y, comb, tot, p, out bf, out bt, out g) && g > 0;
            if (!ok) break;
            var nxt = new BLeaf[cap];
            var nn = 0;
            for (var k = 0; k < nl; k++)
            {
                var sp = lv[k];
                BPartition(X, sp.Idx, sp.N, bf, bt, out var li, out var nli, out var ri, out var nri);
                sp.Nd.Feat = bf; sp.Nd.Thr = bt;
                sp.Nd.L = BNodeNew(y, li, nli);
                sp.Nd.R = BNodeNew(y, ri, nri);
                nxt[nn++] = new BLeaf { Nd = sp.Nd.L, Idx = li, N = nli, Depth = depth + 1 };
                nxt[nn++] = new BLeaf { Nd = sp.Nd.R, Idx = ri, N = nri, Depth = depth + 1 };
            }
            lv = nxt; nl = nn;
        }
        return root;
    }

    private static BNode BFitsTree(int engine, double[][] X, double[] y, int[] idx, int n, int p,
        int maxd, int minleaf)
        => engine switch
        {
            1 => BLeafwise(X, y, idx, n, p, maxd, minleaf),
            2 => BSymmetric(X, y, idx, n, p, maxd, minleaf),
            _ => BDepthwise(X, y, idx, n, p, 0, maxd, minleaf),
        };

    private static double BTreePredict(BNode n, double[] x)
    {
        while (n.Feat >= 0) n = x[n.Feat] <= n.Thr ? n.L! : n.R!;
        return n.Leaf;
    }

    private static int BCount(BNode? n) => n is null ? 0 : 1 + BCount(n.L) + BCount(n.R);

    private static int BSerialize(BNode n, double[] outp, int baseIdx, ref int next)
    {
        var me = next++;
        outp[baseIdx + me * 5 + 0] = n.Feat;
        outp[baseIdx + me * 5 + 1] = n.Thr;
        outp[baseIdx + me * 5 + 2] = n.Leaf;
        if (n.Feat >= 0)
        {
            var l = BSerialize(n.L!, outp, baseIdx, ref next);
            var r = BSerialize(n.R!, outp, baseIdx, ref next);
            outp[baseIdx + me * 5 + 3] = l;
            outp[baseIdx + me * 5 + 4] = r;
        }
        else
        {
            outp[baseIdx + me * 5 + 3] = -1;
            outp[baseIdx + me * 5 + 4] = -1;
        }
        return me;
    }

    public static double[] BoostFit(double[][] X, double[] y, int engine, int nEst, double lr, int maxDepth)
    {
        var n = X.Length;
        var p = n > 0 ? X[0].Length : 0;
        if (n <= 1 || p <= 0 || engine < 0 || engine > 2 || nEst <= 0 || maxDepth < 1 || maxDepth > 20)
            return Array.Empty<double>();
        if (!(lr > 0.0 && lr <= 1.0)) lr = 0.1;
        var f = new double[n];
        var r = new double[n];
        var idx = new int[n];
        double s0 = 0;
        for (var i = 0; i < n; i++) s0 += y[i];
        s0 /= n;
        for (var i = 0; i < n; i++) { f[i] = s0; idx[i] = i; }
        var trees = new List<BNode>();
        for (var t = 0; t < nEst; t++)
        {
            for (var i = 0; i < n; i++) r[i] = y[i] - f[i];
            var tree = BFitsTree(engine, X, r, idx, n, p, maxDepth, 1);
            if (tree is null) break;
            trees.Add(tree);
            for (var i = 0; i < n; i++) f[i] += lr * BTreePredict(tree, X[i]);
        }
        var total = 4;
        foreach (var tr in trees) total += 1 + BCount(tr) * 5;
        var model = new double[total];
        model[0] = p; model[1] = trees.Count; model[2] = lr;
        double sy = 0;
        for (var i = 0; i < n; i++) sy += y[i];
        model[3] = sy / n;
        var pos = 4;
        foreach (var tr in trees)
        {
            var cnt = BCount(tr);
            model[pos] = cnt;
            var next = 0;
            BSerialize(tr, model, pos + 1, ref next);
            pos += 1 + cnt * 5;
        }
        return model;
    }

    public static double BoostPredict(double[] model, double[] x)
    {
        if (model.Length < 4 || x.Length < 1) return double.NaN;
        var nt = (int)model[1];
        var lr = model[2];
        var v = model[3];
        var pos = 4;
        for (var t = 0; t < nt; t++)
        {
            var cnt = (int)model[pos];
            var baseIdx = pos + 1;
            var node = 0;
            while (baseIdx + node * 5 + 4 < model.Length)
            {
                var feat = (int)model[baseIdx + node * 5 + 0];
                if (feat < 0) { v += lr * model[baseIdx + node * 5 + 2]; break; }
                node = x[feat] <= model[baseIdx + node * 5 + 1]
                    ? (int)model[baseIdx + node * 5 + 3]
                    : (int)model[baseIdx + node * 5 + 4];
            }
            pos += 1 + cnt * 5;
        }
        return v;
    }

    public static double BoostScore(double[] model, double[][] X, double[] y)
    {
        var n = X.Length;
        var p = n > 0 ? X[0].Length : 0;
        if (model.Length < 4 || n <= 0 || p <= 0) return double.NaN;
        double ym = 0;
        for (var i = 0; i < n; i++) ym += y[i];
        ym /= n;
        double sst = 0, sse = 0;
        for (var i = 0; i < n; i++)
        {
            var pr = BoostPredict(model, X[i]);
            sst += (y[i] - ym) * (y[i] - ym);
            sse += (y[i] - pr) * (y[i] - pr);
        }
        return sst > 0 ? 1.0 - sse / sst : 0.0;
    }
}
