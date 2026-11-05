// FILE: CommonLib.Numerics.Bo.cs
// Bayesian optimization, gambler's ruin, inferenza causale (parità gs_math.c).
namespace CommonLib;

public static partial class Numerics
{
    private static double BoPhi(double z) => 0.3989422804014327 * Math.Exp(-0.5 * z * z);
    private static double BoPhiCdf(double z) => NormalCdf(0.0, 1.0, z);

    public static double BoEi(double mu, double sd, double best, double xi)
    {
        if (sd <= 1e-300)
        {
            var d0 = mu - best - xi;
            return d0 > 0.0 ? d0 : 0.0;
        }
        var d = mu - best - xi;
        var z = d / sd;
        return d * BoPhiCdf(z) + sd * BoPhi(z);
    }

    public static double BoPi(double mu, double sd, double best, double xi)
    {
        if (sd <= 1e-300) return mu - best - xi > 0.0 ? 1.0 : 0.0;
        return BoPhiCdf((mu - best - xi) / sd);
    }

    public static double BoUcb(double mu, double sd, double kappa) => mu + kappa * sd;

    public static (int Index, double Value) BoNext(double[] x, double[] y, double[] cand,
        double l2, double noise, double kappa)
    {
        if (x.Length < 2 || cand.Length < 1) return (-1, double.NaN);
        var gp = GpRegress(x, y, cand, l2, noise);
        var mean = gp.Mean;
        var varArr = gp.Variance;
        var best = 0;
        var bv = -1e300;
        for (var i = 0; i < cand.Length; i++)
        {
            var u = mean[i] + kappa * Math.Sqrt(varArr[i] > 0.0 ? varArr[i] : 0.0);
            if (u > bv) { bv = u; best = i; }
        }
        return (best, bv);
    }

    public static double GamblerRuinProb(double i, double N, double p)
    {
        if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return double.NaN;
        var q = 1.0 - p;
        if (i == 0.0) return 1.0;
        if (i == N) return 0.0;
        if (Math.Abs(p - 0.5) < 1e-12) return 1.0 - i / N;
        var r = q / p;
        return (Math.Pow(r, i) - Math.Pow(r, N)) / (1.0 - Math.Pow(r, N));
    }

    public static double GamblerRuinDuration(double i, double N, double p)
    {
        if (N <= 0.0 || i < 0.0 || i > N || p < 0.0 || p > 1.0) return double.NaN;
        var q = 1.0 - p;
        if (Math.Abs(p - 0.5) < 1e-12) return i * (N - i);
        var r = q / p;
        return i / (q - p) - (N / (q - p)) * (1.0 - Math.Pow(r, i)) / (1.0 - Math.Pow(r, N));
    }

    public static double AteIpw(double[] t, double[] y, double[] ps)
    {
        var n = Math.Min(t.Length, Math.Min(y.Length, ps.Length));
        if (n < 1) return double.NaN;
        double s1 = 0.0, s0 = 0.0;
        for (var i = 0; i < n; i++)
        {
            var e = ps[i] < 1e-6 ? 1e-6 : (ps[i] > 1.0 - 1e-6 ? 1.0 - 1e-6 : ps[i]);
            s1 += t[i] * y[i] / e;
            s0 += (1.0 - t[i]) * y[i] / (1.0 - e);
        }
        return s1 / n - s0 / n;
    }

    public static double AteMatching(double[] t, double[] y, double[] ps)
    {
        var n = Math.Min(t.Length, Math.Min(y.Length, ps.Length));
        if (n < 2) return double.NaN;
        int i, j, nt = 0, nc = 0;
        double att = 0.0, atc = 0.0;
        for (i = 0; i < n; i++)
        {
            var best = -1;
            var bd = 1e300;
            for (j = 0; j < n; j++)
            {
                if (t[j] == t[i]) continue;
                var dd = Math.Abs(ps[i] - ps[j]);
                if (dd < bd) { bd = dd; best = j; }
            }
            if (best < 0) continue;
            if (t[i] > 0.5) { att += y[i] - y[best]; nt++; }
            else { atc += y[best] - y[i]; nc++; }
        }
        att = nt > 0 ? att / nt : 0.0;
        atc = nc > 0 ? atc / nc : 0.0;
        if (nt > 0 && nc > 0) return 0.5 * (att + atc);
        return nt > 0 ? att : atc;
    }
}
