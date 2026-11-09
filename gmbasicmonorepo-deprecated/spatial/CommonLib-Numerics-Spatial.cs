/* ============================================================================
 * CommonLib-Numerics-Spatial.cs — ARCHIVIATO (non compilato).
 *
 * Implementazione C# storica delle 4 funzioni SPATIAL, estratta da
 * GMBasicSharp/common/CommonLib/Numerics/SciPyP1.cs durante la migrazione a
 * shared/libs/spatial (libgmbspatial, FFI). I metodi rimossi dal file vivo
 * sono: DistMetric (helper), Pdist, Cdist, DistanceMatrix, ConvexHull.
 * Il resto di SciPyP1.cs (FFT, integrazione, ecc.) NON è deprecato.
 * ============================================================================ */
using System;
using System.Collections.Generic;
using System.Linq;

namespace CommonLib
{
    public static partial class Numerics
    {
    private static double DistMetric(double[] a, double[] b, string metric, double p)
    {
        switch (metric.ToLowerInvariant())
        {
            case "cityblock" or "manhattan":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s += Math.Abs(a[i] - b[i]);
                return s;
            }
            case "chebyshev":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s = Math.Max(s, Math.Abs(a[i] - b[i]));
                return s;
            }
            case "cosine":
            {
                double ab = 0, aa = 0, bb = 0;
                for (var i = 0; i < a.Length; i++) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
                var denom = Math.Sqrt(aa * bb);
                return denom > 0 ? 1.0 - ab / denom : 0;
            }
            case "minkowski":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s += Math.Pow(Math.Abs(a[i] - b[i]), p);
                return Math.Pow(s, 1.0 / p);
            }
            default:
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) { var d = a[i] - b[i]; s += d * d; }
                return Math.Sqrt(s);
            }
        }
    }

    /// <summary>scipy.spatial.distance.pdist — distanze a coppie condensate (triangolo superiore, i&lt;j).</summary>
    public static double[] Pdist(double[][] X, string metric, double p)
    {
        var n = X.Length;
        var m = n * (n - 1) / 2;
        var result = new double[m];
        var k = 0;
        for (var i = 0; i < n; i++)
            for (var j = i + 1; j < n; j++)
                result[k++] = DistMetric(X[i], X[j], metric, p);
        return result;
    }

    /// <summary>scipy.spatial.distance.cdist — distanze tra ogni riga di X e ogni riga di Y.</summary>
    public static double[,] Cdist(double[][] X, double[][] Y, string metric, double p)
    {
        var result = new double[X.Length, Y.Length];
        for (var i = 0; i < X.Length; i++)
            for (var j = 0; j < Y.Length; j++)
                result[i, j] = DistMetric(X[i], Y[j], metric, p);
        return result;
    }

    /// <summary>scipy.spatial.distance_matrix — matrice di distanze euclidee.</summary>
    public static double[,] DistanceMatrix(double[][] X, double[][] Y)
    {
        var result = new double[X.Length, Y.Length];
        for (var i = 0; i < X.Length; i++)
            for (var j = 0; j < Y.Length; j++)
            {
                double s = 0;
                for (var k = 0; k < X[i].Length; k++)
                {
                    var d = X[i][k] - Y[j][k];
                    s += d * d;
                }
                result[i, j] = Math.Sqrt(s);
            }
        return result;
    }

    public static double[] ConvexHull(double[][] points)
    {
        var n = points.Length;
        var idx = Enumerable.Range(0, n).ToArray();
        Array.Sort(idx, (a, b) => points[a][0] != points[b][0] ? points[a][0].CompareTo(points[b][0]) : points[a][1].CompareTo(points[b][1]));
        double Cross(int o, int a, int b)
        {
            var (x1, y1) = (points[idx[a]][0] - points[idx[o]][0], points[idx[a]][1] - points[idx[o]][1]);
            var (x2, y2) = (points[idx[b]][0] - points[idx[o]][0], points[idx[b]][1] - points[idx[o]][1]);
            return x1 * y2 - y1 * x2;
        }
        var lower = new List<int>();
        for (var i = 0; i < n; i++)
        {
            while (lower.Count >= 2 && Cross(lower[^2], lower[^1], i) <= 0) lower.RemoveAt(lower.Count - 1);
            lower.Add(i);
        }
        var upper = new List<int>();
        for (var i = n - 1; i >= 0; i--)
        {
            while (upper.Count >= 2 && Cross(upper[^2], upper[^1], i) <= 0) upper.RemoveAt(upper.Count - 1);
            upper.Add(i);
        }
        lower.RemoveAt(lower.Count - 1);
        upper.RemoveAt(upper.Count - 1);
        lower.AddRange(upper);
        return lower.Select(k => (double)idx[k]).ToArray();
    }
    }
}
