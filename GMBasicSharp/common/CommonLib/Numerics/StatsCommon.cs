// FILE: CommonLib.Numerics.StatsCommon.cs
// P0 scipy.stats: correlazione (pearsonr/spearmanr), linregress, test
// (chi2_contingency, mannwhitneyu, wilcoxon), distribuzione Gamma, test di
// normalità (skewtest/kurtosistest/jarque_bera), descrittive (mode/moment/iqr/describe).
// Reference: scipy.stats 1.18.

using MathNet.Numerics;
using MathNet.Numerics.Distributions;
using MathNet.Numerics.Statistics;

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>Risultato di scipy.stats.linregress.</summary>
    public sealed record LinRegressResult(double Slope, double Intercept, double R, double PValue, double Stderr);

    /// <summary>Risultato di scipy.stats.describe.</summary>
    public sealed record DescribeResult(double Nobs, double Min, double Max, double Mean, double Variance, double Skewness, double Kurtosis);

    // ========================================================================
    // Correlazione e regressione lineare
    // ========================================================================

    /// <summary>scipy.stats.pearsonr — coefficiente di correlazione di Pearson e p-value.</summary>
    public static StatisticalTestResult PearsonR(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        double mx = 0, my = 0;
        for (var i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
        mx /= n;
        my /= n;
        double sxx = 0, syy = 0, sxy = 0;
        for (var i = 0; i < n; i++)
        {
            var dx = x[i] - mx;
            var dy = y[i] - my;
            sxx += dx * dx;
            syy += dy * dy;
            sxy += dx * dy;
        }
        if (sxx == 0 || syy == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var r = sxy / Math.Sqrt(sxx * syy);
        var p = PearsonPValue(r, n);
        return new StatisticalTestResult(r, p, n - 2);
    }

    private static double PearsonPValue(double r, int n)
    {
        if (Math.Abs(r) >= 1.0) return 0.0;
        double t = r * Math.Sqrt((n - 2) / (1 - r * r));
        var df = n - 2;
        return 2.0 * StudentT.CDF(0.0, 1.0, df, -Math.Abs(t));
    }

    /// <summary>scipy.stats.spearmanr — correlazione per ranghi di Spearman e p-value.</summary>
    public static StatisticalTestResult SpearmanR(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var rx = RankData(x, n);
        var ry = RankData(y, n);
        return PearsonR(rx, ry);
    }

    private static double[] RankData(double[] data, int n)
    {
        var idx = Enumerable.Range(0, n).ToArray();
        Array.Sort(idx, (a, b) => data[a].CompareTo(data[b]));
        var ranks = new double[n];
        var i = 0;
        while (i < n)
        {
            var j = i;
            while (j + 1 < n && data[idx[j + 1]] == data[idx[i]]) j++;
            var avg = 1.0 + (i + j) / 2.0;
            for (var k = i; k <= j; k++) ranks[idx[k]] = avg;
            i = j + 1;
        }
        return ranks;
    }

    /// <summary>scipy.stats.linregress — regressione lineare OLS (slope, intercept, r, p, se).</summary>
    public static LinRegressResult LinRegress(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new LinRegressResult(double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        double mx = 0, my = 0;
        for (var i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
        mx /= n;
        my /= n;
        double sxx = 0, sxy = 0;
        for (var i = 0; i < n; i++)
        {
            var dx = x[i] - mx;
            sxx += dx * dx;
            sxy += dx * (y[i] - my);
        }
        if (sxx == 0) return new LinRegressResult(double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        var slope = sxy / sxx;
        var intercept = my - slope * mx;
        double ss = 0;
        for (var i = 0; i < n; i++)
        {
            var e = y[i] - (intercept + slope * x[i]);
            ss += e * e;
        }
        var df = n - 2;
        var stderr = Math.Sqrt(ss / df / sxx);
        double t = 0;
        if (stderr > 0) t = slope / stderr;
        var p = 2.0 * StudentT.CDF(0.0, 1.0, df, -Math.Abs(t));
        // r: segno = segno slope, modulo = sqrt(R²) dove R² = 1 - ss/sstot
        double sstot = 0;
        for (var i = 0; i < n; i++) sstot += (y[i] - my) * (y[i] - my);
        var r = sstot > 0 ? Math.Sign(slope) * Math.Sqrt(Math.Max(0, 1 - ss / sstot)) : 0.0;
        return new LinRegressResult(slope, intercept, r, p, stderr);
    }

    // ========================================================================
    // Test: chi2_contingency, mannwhitneyu, wilcoxon
    // ========================================================================

    /// <summary>scipy.stats.chi2_contingency — test chi-quadrato di indipendenza su tabella di contingenza.</summary>
    public static StatisticalTestResult Chi2Contingency(double[][] table)
    {
        var rows = table.Length;
        if (rows == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var cols = table[0].Length;
        var rowSum = new double[rows];
        var colSum = new double[cols];
        double total = 0;
        for (var i = 0; i < rows; i++)
            for (var j = 0; j < cols; j++)
            {
                rowSum[i] += table[i][j];
                colSum[j] += table[i][j];
                total += table[i][j];
            }
        if (total == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        double chi2 = 0;
        for (var i = 0; i < rows; i++)
            for (var j = 0; j < cols; j++)
            {
                var expected = rowSum[i] * colSum[j] / total;
                if (expected > 0)
                {
                    var d = table[i][j] - expected;
                    chi2 += d * d / expected;
                }
            }
        var dof = (rows - 1.0) * (cols - 1.0);
        var p = 1.0 - ChiSquared.CDF(dof, chi2);
        return new StatisticalTestResult(chi2, p, dof);
    }

    /// <summary>scipy.stats.mannwhitneyu — test U di Mann-Whitney (approssimazione normale).</summary>
    public static StatisticalTestResult MannWhitneyU(double[] x, double[] y)
    {
        var nx = x.Length;
        var ny = y.Length;
        if (nx == 0 || ny == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        double u = 0;
        foreach (var xi in x)
            foreach (var yj in y)
                if (xi > yj) u += 1.0;
                else if (xi == yj) u += 0.5;
        var mean = nx * ny / 2.0;
        var n = nx + ny;
        // varianza con correzione per i pareggi
        double ties = 0;
        var all = x.Concat(y).OrderBy(v => v).ToArray();
        var i = 0;
        while (i < n)
        {
            var j = i;
            while (j + 1 < n && all[j + 1] == all[i]) j++;
            var t = j - i + 1;
            ties += t * (t - 1) * (t + 1);
            i = j + 1;
        }
        var varU = (nx * ny / 12.0) * ((n + 1.0) - ties / (n * (n - 1.0)));
        if (varU <= 0) return new StatisticalTestResult(u, double.NaN, double.NaN);
        var z = (u - mean) / Math.Sqrt(varU);
        var p = 2.0 * Normal.CDF(0.0, 1.0, -Math.Abs(z));
        return new StatisticalTestResult(u, p, double.NaN);
    }

    /// <summary>scipy.stats.wilcoxon — test dei ranghi con segno (approssimazione normale).</summary>
    public static StatisticalTestResult Wilcoxon(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        var diffs = new List<double>();
        for (var i = 0; i < n; i++)
        {
            var d = x[i] - y[i];
            if (d != 0) diffs.Add(d);
        }
        if (diffs.Count == 0) return new StatisticalTestResult(0.0, 1.0, double.NaN);
        var abs = diffs.Select(Math.Abs).ToArray();
        var ranks = RankData(abs, abs.Length);
        double w = 0;
        for (var i = 0; i < diffs.Count; i++)
            if (diffs[i] > 0) w += ranks[i];
        var m = diffs.Count;
        var mean = m * (m + 1.0) / 4.0;
        var varW = m * (m + 1.0) * (2 * m + 1.0) / 24.0;
        if (varW <= 0) return new StatisticalTestResult(w, double.NaN, double.NaN);
        var z = (w - mean) / Math.Sqrt(varW);
        var p = 2.0 * Normal.CDF(0.0, 1.0, -Math.Abs(z));
        return new StatisticalTestResult(w, p, double.NaN);
    }

    // ========================================================================
    // Distribuzione Gamma (scipy.stats.gamma)
    // ========================================================================

    /// <summary>scipy.stats.gamma.pdf — shape a, scale s.</summary>
    public static double GammaPdf(double shape, double scale, double x)
    {
        if (x < 0 || shape <= 0 || scale <= 0) return 0;
        return Math.Pow(x / scale, shape - 1) * Math.Exp(-x / scale) / (scale * SpecialFunctions.Gamma(shape));
    }

    /// <summary>scipy.stats.gamma.cdf — incomplete gamma regolarizzata P(a, x/s).</summary>
    public static double GammaCdf(double shape, double scale, double x)
    {
        if (x < 0) return 0;
        return SpecialFunctions.GammaLowerRegularized(shape, x / scale);
    }
    /// <summary>Funzione di sopravvivenza Gamma (1 - CDF).</summary>
    public static double GammaSf(double shape, double scale, double x) => 1.0 - GammaCdf(shape, scale, x);

    /// <summary>scipy.stats.gamma.ppf — inversa della CDF via bisezione robusta.</summary>
    public static double GammaPpf(double shape, double scale, double p)
    {
        if (p <= 0) return 0;
        if (p >= 1) return double.PositiveInfinity;
        var lo = 0.0;
        var hi = 1.0;
        while (GammaCdf(shape, scale, hi) < p) hi *= 2;
        for (var it = 0; it < 200; it++)
        {
            var mid = 0.5 * (lo + hi);
            if (GammaCdf(shape, scale, mid) < p) lo = mid;
            else hi = mid;
            if (hi - lo < 1e-10 * Math.Max(1, hi)) break;
        }
        return 0.5 * (lo + hi);
    }

    /// <summary>scipy.stats.gamma.isf — 1 − ppf(p).</summary>
    public static double GammaIsf(double shape, double scale, double p) => GammaPpf(shape, scale, 1.0 - p);

    // ========================================================================
    // Test di normalità: skewtest, kurtosistest, jarque_bera
    // ========================================================================

    /// <summary>scipy.stats.skewtest — test D'Agostino sull'asimmetria. Riusa la stessa
    /// trasformata Z precisa di <see cref="NormalityTest"/> (SkewTestZ in HypothesisTests.cs)
    /// — prima era un'approssimazione grezza (`skew/sqrt(6/n)`, formula diversa da scipy e
    /// dalla propria STATS.NORMALTEST: violava la regola di un'unica implementazione e non
    /// replicava scipy nemmeno approssimativamente per campioni non banali).</summary>
    public static StatisticalTestResult SkewTest(double[] data)
    {
        var n = data.Length;
        if (n < 3) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var z = SkewTestZ(data);
        var p = 2.0 * Normal.CDF(0.0, 1.0, -Math.Abs(z));
        return new StatisticalTestResult(z, p, double.NaN);
    }

    /// <summary>scipy.stats.kurtosistest — test sulla curtosi. Riusa la stessa trasformata Z
    /// precisa di <see cref="NormalityTest"/> (KurtosisTestZ in HypothesisTests.cs), stesso
    /// motivo di <see cref="SkewTest"/>.</summary>
    public static StatisticalTestResult KurtosisTest(double[] data)
    {
        var n = data.Length;
        if (n < 4) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var z = KurtosisTestZ(data);
        var p = 2.0 * Normal.CDF(0.0, 1.0, -Math.Abs(z));
        return new StatisticalTestResult(z, p, double.NaN);
    }

    /// <summary>scipy.stats.jarque_bera — JB = n/6·(S² + (K−3)²/4), p da chi²(2).</summary>
    public static StatisticalTestResult JarqueBeraTest(double[] data)
    {
        var n = data.Length;
        if (n < 3) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        // scipy.stats.jarque_bera: S = m3/m2^1.5 (skew grezza), K = m4/m2^2 (kurtosi grezza),
        // JB = n/6·(S² + (K−3)²), p = chi2.sf(JB, 2).
        var mean = Mean(data);
        double m2 = 0, m3 = 0, m4 = 0;
        for (var i = 0; i < n; i++)
        {
            var d = data[i] - mean;
            m2 += d * d; m3 += d * d * d; m4 += d * d * d * d;
        }
        m2 /= n; m3 /= n; m4 /= n;
        var s = m2 > 0 ? m3 / Math.Pow(m2, 1.5) : 0;
        var k = m2 > 0 ? m4 / (m2 * m2) : 0;
        var jb = (n / 6.0) * (s * s + (k - 3.0) * (k - 3.0));
        var p = 1.0 - ChiSquared.CDF(2.0, jb);
        return new StatisticalTestResult(jb, p, double.NaN);
    }

    // ========================================================================
    // Descrittive: mode, moment, iqr, describe
    // ========================================================================

    /// <summary>scipy.stats.mode — valore più frequente (pareggi → il più piccolo).</summary>
    public static double Mode(double[] data)
    {
        if (data.Length == 0) return double.NaN;
        return data.GroupBy(v => v).OrderByDescending(g => g.Count()).ThenBy(g => g.Key).First().Key;
    }

    /// <summary>scipy.stats.moment — momento centrale di ordine k (attorno alla media).</summary>
    public static double Moment(double[] data, int order)
    {
        var n = data.Length;
        if (n == 0) return double.NaN;
        var m = Mean(data);
        double s = 0;
        for (var i = 0; i < n; i++) s += Math.Pow(data[i] - m, order);
        return s / n;
    }

    /// <summary>scipy.stats.iqr — Q3 − Q1 (numpy.percentile, interpolazione lineare).</summary>
    public static double Iqr(double[] data)
    {
        var q1 = NumpyPercentile(data, 0.25);
        var q3 = NumpyPercentile(data, 0.75);
        return q3 - q1;
    }

    private static double NumpyPercentile(double[] data, double q)
    {
        var sorted = (double[])data.Clone();
        Array.Sort(sorted);
        var pos = q * (sorted.Length - 1);
        var lo = (int)Math.Floor(pos);
        var frac = pos - lo;
        if (lo + 1 >= sorted.Length) return sorted[^1];
        return sorted[lo] + frac * (sorted[lo + 1] - sorted[lo]);
    }

    /// <summary>scipy.stats.describe — statistiche descrittive complete.</summary>
    public static DescribeResult Describe(double[] data)
    {
        var n = data.Length;
        if (n == 0) return new DescribeResult(0, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        var mean = Mean(data);
        var variance = n > 1 ? Variance(data) : 0.0;
        return new DescribeResult(n, Min(data), Max(data), mean, variance, Skewness(data), Kurtosis(data));
    }
}
