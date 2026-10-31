// FILE: CommonLib.Numerics.VectorMetrics.cs
// Metriche di distanza tra vettori (scipy.spatial.distance) per GMBasic.
// Vengono usate da IlRuntime (handle) e dal PCode (MathNet Vector<double>).
// Nessun duplicato: prima di aggiungere, verificato che VECTOR espone solo
// CREATE/PRODUCT (dot)/EUCLIDEA (norma) e CommonLib solo VectorDotProduct/VectorNorm.

using System;

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>Lunghezza incompatibile → NaN (convenzione GMBasic, mai eccezione).</summary>
    private static double[]? CheckPair(double[] a, double[] b)
        => a != null && b != null && a.Length == b.Length ? a : null;

    /// <summary>Distanza di Manhattan (L1) = Σ|a_i − b_i|.</summary>
    public static double VectorManhattan(double[] a, double[] b)
    {
        if (CheckPair(a, b) == null) return double.NaN;
        double s = 0;
        for (var i = 0; i < a.Length; i++) s += Math.Abs(a[i] - b[i]);
        return s;
    }

    /// <summary>Distanza di Chebyshev (L∞) = max_i |a_i − b_i|.</summary>
    public static double VectorChebyshev(double[] a, double[] b)
    {
        if (CheckPair(a, b) == null) return double.NaN;
        double m = 0;
        for (var i = 0; i < a.Length; i++) m = Math.Max(m, Math.Abs(a[i] - b[i]));
        return m;
    }

    /// <summary>Distanza coseno = 1 − (a·b)/(‖a‖·‖b‖). Norma nulla → NaN.</summary>
    public static double VectorCosineDistance(double[] a, double[] b)
    {
        if (CheckPair(a, b) == null) return double.NaN;
        double dot = 0, na = 0, nb = 0;
        for (var i = 0; i < a.Length; i++)
        {
            dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i];
        }
        var den = Math.Sqrt(na) * Math.Sqrt(nb);
        return den == 0 ? double.NaN : 1.0 - dot / den;
    }

    /// <summary>Distanza di Minkowski = (Σ|a_i − b_i|^p)^(1/p). p &lt;= 0 o lunghezza diversa → NaN.</summary>
    public static double VectorMinkowski(double[] a, double[] b, double p)
    {
        if (CheckPair(a, b) == null || double.IsNaN(p) || p <= 0) return double.NaN;
        double s = 0;
        for (var i = 0; i < a.Length; i++) s += Math.Pow(Math.Abs(a[i] - b[i]), p);
        return Math.Pow(s, 1.0 / p);
    }
}
