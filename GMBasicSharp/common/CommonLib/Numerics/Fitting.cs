// FILE: CommonLib.Numerics.Fitting.cs
// Curve fitting (parità gmbstats gs_fit.c).
namespace CommonLib;

public static partial class Numerics
{
    public static double ChebEval(double x, double[] coef)
    {
        var m = coef.Length;
        if (m < 1) return double.NaN;
        double b1 = 0.0, b2 = 0.0;
        for (var j = m - 1; j >= 1; j--)
        {
            var b0 = 2.0 * x * b1 - b2 + coef[j];
            b2 = b1; b1 = b0;
        }
        return x * b1 - b2 + coef[0];
    }

    public static double PadeEval(double x, double[] a, double[] b)
    {
        double num = 0.0, xp = 1.0;
        for (var i = 0; i < a.Length; i++) { num += a[i] * xp; xp *= x; }
        double den = 0.0;
        xp = 1.0;
        for (var i = 0; i < b.Length; i++) { den += b[i] * xp; xp *= x; }
        return den != 0.0 ? num / den : 0.0;
    }

    public static double PolyEval(double x, double[] coef)
    {
        double s = 0.0;
        for (var i = coef.Length - 1; i >= 0; i--) s = s * x + coef[i];
        return s;
    }

    public static double PolyLs(double[] x, double[] y, int m, double[] coef)
    {
        var n = Math.Min(x.Length, y.Length);
        var p = m + 1;
        if (n < 1 || m < 0 || p > 32) return 0.0;
        var N = new double[p * p];
        var r = new double[p];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < p; j++)
            {
                var xj = Math.Pow(x[i], j);
                r[j] += xj * y[i];
                for (var k = 0; k < p; k++) N[j * p + k] += xj * Math.Pow(x[i], k);
            }
        if (!InferSolve(p, N, r, coef)) return 0.0;
        double ssq = 0.0;
        for (var i = 0; i < n; i++)
        {
            var e = y[i] - PolyEval(x[i], coef);
            ssq += e * e;
        }
        return ssq;
    }

    public static (double[] Coef, double Ssq) PolyFit(double[] x, double[] y, int m)
    {
        var coef = new double[m + 1];
        var ssq = PolyLs(x, y, m, coef);
        return (coef, ssq);
    }

    public static (double R2, double Angle) RSquared(double[] data)
    {
        var n = data.Length;
        double r2 = 0, angle = 0;
        if (n <= 2) return (r2, angle);
        int i, k = 0;
        double Sx = 0, Sy = 0, Sxy = 0, Sx2 = 0, Sy2 = 0;
        for (i = 0; i < n; ++i)
        {
            var v = data[i];
            if (v == double.MaxValue || !double.IsFinite(v)) continue;
            double x = i + 1;
            Sx += x; Sy += v; Sxy += x * v; Sx2 += x * x; Sy2 += v * v; ++k;
        }
        if (k <= 2) return (r2, angle);
        {
            var nn = (double)k;
            var Sx22 = Sx * Sx / nn;
            var Sy22 = Sy * Sy / nn;
            var SxSy = Sx * Sy / nn;
            var div = (Sx2 - Sx22) * (Sy2 - Sy22);
            if (Math.Abs(div) < 2.220446049250313e-16) return (r2, angle);
            r2 = (Sxy - SxSy) * (Sxy - SxSy) / div;
            angle = (Sxy - SxSy) / (Sx2 - Sx22);
        }
        return (r2, angle);
    }

    public static double RSquaredTest(double[] data)
    {
        var (r2, angle) = RSquared(data);
        var weight = 1.0 - 1.0 / Math.Sqrt(data.Length + 1.0);
        if (angle < 0) return -Math.Abs(r2) * weight;
        return r2 * weight;
    }
}
