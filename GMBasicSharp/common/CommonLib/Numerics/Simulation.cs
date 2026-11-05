// FILE: CommonLib.Numerics.Simulation.cs
// SIMULATION: RNG deterministico (SplitMix64) — parità bit-exact con il runtime C.
namespace CommonLib;

public static partial class Numerics
{
    private static ulong _simState = 0x9E3779B97F4A7C15UL;

    public static double SimSeed(double seed)
    {
        var s = (ulong)seed;
        _simState = s != 0 ? s : 0x9E3779B97F4A7C15UL;
        return 0.0;
    }

    private static ulong SimNextU64()
    {
        unchecked
        {
            _simState += 0x9E3779B97F4A7C15UL;
            var z = _simState;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
            return z ^ (z >> 31);
        }
    }

    private static double SimNextDouble()
        => (double)(SimNextU64() >> 11) * (1.0 / 9007199254740992.0);

    public static double SimUniform() => SimNextDouble();

    public static double SimNormal()
    {
        double u, v, s;
        do
        {
            u = 2.0 * SimNextDouble() - 1.0;
            v = 2.0 * SimNextDouble() - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        return u * Math.Sqrt(-2.0 * Math.Log(s) / s);
    }

    public static double SimExponential(double rate)
        => rate <= 0.0 ? double.NaN : -Math.Log(1.0 - SimNextDouble()) / rate;

    public static double SimBernoulli(double p) => SimNextDouble() < p ? 1.0 : 0.0;

    public static double SimBinomial(double nIn, double p)
    {
        var n = (int)nIn;
        if (n < 0) return double.NaN;
        double c = 0.0;
        for (var i = 0; i < n; i++) if (SimNextDouble() < p) c += 1.0;
        return c;
    }

    private static int SimInt(int n)
    {
        if (n <= 1) return 0;
        var v = (int)(SimNextDouble() * n);
        if (v < 0) v = 0;
        if (v >= n) v = n - 1;
        return v;
    }

    private static double TpeLogPdf(double x, double mu, double sd)
    {
        var d = x - mu;
        return -0.5 * d * d / (sd * sd) - Math.Log(sd);
    }

    public static double[] Tpe(Func<double[], double> f, double[] lo, double[] hi,
        int nIter, int nInit, double seed)
    {
        const double LNORM = 0.91893853320467274178;
        var d = lo.Length;
        if (f is null || d < 1 || nInit < 2 || nIter < 1) return Array.Empty<double>();
        var m = nInit + nIter;
        var X = new double[m * d];
        var Y = new double[m];
        var sigma = new double[d];
        var cand = new double[d];
        var bestx = new double[d];
        var ord = new int[m];
        var count = 0;
        int i, k, it;
        SimSeed(seed);
        for (count = 0; count < nInit; count++)
        {
            var row = new double[d];
            for (k = 0; k < d; k++) { row[k] = lo[k] + (hi[k] - lo[k]) * SimUniform(); X[count * d + k] = row[k]; }
            Y[count] = f(row);
        }
        for (k = 0; k < d; k++) sigma[k] = 0.2 * (hi[k] - lo[k]);
        for (it = 0; it < nIter; it++)
        {
            var bestRatio = -1e300;
            for (i = 0; i < count; i++) ord[i] = i;
            for (i = 0; i < count; i++)
                for (var g = i + 1; g < count; g++)
                    if (Y[ord[g]] < Y[ord[i]]) { var t = ord[i]; ord[i] = ord[g]; ord[g] = t; }
            var nGood = (int)(0.25 * count);
            if (nGood < 1) nGood = 1;
            if (nGood >= count) nGood = count - 1;
            var nb = count - nGood;
            for (var c = 0; c < 24; c++)
            {
                var src = ord[SimInt(nGood)];
                double lg, gb;
                for (k = 0; k < d; k++)
                {
                    var xc = X[src * d + k] + sigma[k] * SimNormal();
                    if (xc < lo[k]) xc = lo[k];
                    if (xc > hi[k]) xc = hi[k];
                    cand[k] = xc;
                }
                {
                    double acc = 0.0;
                    for (var g = 0; g < nGood; g++)
                    {
                        var lp = -LNORM;
                        var id = ord[g];
                        for (k = 0; k < d; k++) lp += TpeLogPdf(cand[k], X[id * d + k], sigma[k]);
                        acc += Math.Exp(lp);
                    }
                    lg = Math.Log(acc / nGood + 1e-300);
                }
                if (nb > 0)
                {
                    double acc = 0.0;
                    for (var g = nGood; g < count; g++)
                    {
                        var lp = -LNORM;
                        var id = ord[g];
                        for (k = 0; k < d; k++) lp += TpeLogPdf(cand[k], X[id * d + k], sigma[k]);
                        acc += Math.Exp(lp);
                    }
                    gb = Math.Log(acc / nb + 1e-300);
                }
                else gb = -1e300;
                var ratio = lg - gb;
                if (ratio > bestRatio) { bestRatio = ratio; for (k = 0; k < d; k++) bestx[k] = cand[k]; }
            }
            for (k = 0; k < d; k++) X[count * d + k] = bestx[k];
            Y[count] = f((double[])bestx.Clone());
            count++;
        }
        var best = 0;
        for (i = 1; i < count; i++) if (Y[i] < Y[best]) best = i;
        var outp = new double[d + 1];
        for (k = 0; k < d; k++) outp[k] = X[best * d + k];
        outp[d] = Y[best];
        return outp;
    }

    public static (double[] Mean, double[] Variance) ParticleFilter(double[] obs, double a,
        double q, double r, int nParticles, double seed)
    {
        var T = obs.Length;
        if (T < 1 || nParticles < 2)
            return (Array.Empty<double>(), Array.Empty<double>());
        SimSeed(seed);
        var part = new double[nParticles];
        var w = new double[nParticles];
        var cdf = new double[nParticles];
        var mean = new double[T];
        var varArr = new double[T];
        int t, i;
        for (i = 0; i < nParticles; i++) part[i] = obs[0] + Math.Sqrt(r) * SimNormal();
        for (t = 0; t < T; t++)
        {
            double wsum = 0.0;
            for (i = 0; i < nParticles; i++)
            {
                if (t > 0) part[i] = a * part[i] + Math.Sqrt(q) * SimNormal();
                var dd = obs[t] - part[i];
                w[i] = Math.Exp(-0.5 * dd * dd / r);
                wsum += w[i];
            }
            if (wsum <= 0.0) wsum = 1e-300;
            for (i = 0; i < nParticles; i++) w[i] /= wsum;
            double m = 0.0;
            for (i = 0; i < nParticles; i++) m += w[i] * part[i];
            double v = 0.0;
            for (i = 0; i < nParticles; i++) { var dd = part[i] - m; v += w[i] * dd * dd; }
            mean[t] = m; varArr[t] = v;
            {
                double acc = 0.0;
                for (i = 0; i < nParticles; i++) { acc += w[i]; cdf[i] = acc; }
                if (acc > 0.0) for (i = 0; i < nParticles; i++) cdf[i] /= acc;
            }
            {
                var np = new double[nParticles];
                for (i = 0; i < nParticles; i++)
                {
                    var u = SimUniform();
                    int lo = 0, hi = nParticles - 1, mid;
                    while (lo < hi) { mid = (lo + hi) / 2; if (cdf[mid] < u) lo = mid + 1; else hi = mid; }
                    np[i] = part[lo];
                }
                Array.Copy(np, part, nParticles);
            }
        }
        return (mean, varArr);
    }
}
