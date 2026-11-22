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

    public static StatisticalTestResult Kruskal(double[][] groups)
    {
        var k = groups.Length;
        if (k < 2) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var total = groups.Sum(g => g.Length);
        if (total == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var flat = new double[total];
        var sizes = new int[k];
        var pos = 0;
        for (var i = 0; i < k; i++)
        {
            sizes[i] = groups[i].Length;
            Array.Copy(groups[i], 0, flat, pos, sizes[i]);
            pos += sizes[i];
        }
        var ranks = RankData(flat, total);
        double h = 0;
        var off = 0;
        for (var g = 0; g < k; g++)
        {
            double rsum = 0;
            for (var i = 0; i < sizes[g]; i++) rsum += ranks[off++];
            if (sizes[g] > 0) h += rsum * rsum / sizes[g];
        }
        h = 12.0 / (total * (double)(total + 1)) * h - 3.0 * (total + 1);
        var df = k - 1d;
        var p = 1.0 - ChiSquaredCdf(df, h);
        return new StatisticalTestResult(h, p, df);
    }

    public static StatisticalTestResult KendallTau(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        int concordant = 0, discordant = 0, tiesX = 0, tiesY = 0;
        for (var i = 0; i < n; i++)
            for (var j = i + 1; j < n; j++)
            {
                var dx = x[j] - x[i];
                var dy = y[j] - y[i];
                var prod = dx * dy;
                if (prod > 0) concordant++;
                else if (prod < 0) discordant++;
                if (dx == 0) tiesX++;
                if (dy == 0) tiesY++;
            }
        var n0 = n * (n - 1) / 2;
        var den = Math.Sqrt((double)(n0 - tiesX) * (n0 - tiesY));
        var tau = den > 0 ? (concordant - discordant) / den : 0.0;
        var variance = 2.0 * (2 * n + 5) / (9.0 * n * (n - 1));
        var p = variance > 0 ? 2.0 * NormalCdf(0, 1, -Math.Abs(tau / Math.Sqrt(variance))) : double.NaN;
        return new StatisticalTestResult(tau, p, double.NaN);
    }

    public static StatisticalTestResult FTest(double[] x, double[] y)
    {
        if (x.Length < 2 || y.Length < 2)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var m1 = x.Average();
        var m2 = y.Average();
        var v1 = x.Sum(v => (v - m1) * (v - m1)) / (x.Length - 1);
        var v2 = y.Sum(v => (v - m2) * (v - m2)) / (y.Length - 1);
        if (v1 <= 0 || v2 <= 0)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var f = v1 / v2;
        var df1 = x.Length - 1d;
        var df2 = y.Length - 1d;
        var cdf = FCdf(df1, df2, f);
        var p = 2.0 * Math.Min(cdf, 1.0 - cdf);
        return new StatisticalTestResult(f, p, df1);
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
        double total = 0;
        for (var i = 0; i < rows; i++)
            for (var j = 0; j < cols; j++) total += table[i][j];
        if (total == 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var flat = new double[rows * cols];
        for (var i = 0; i < rows; i++)
            for (var j = 0; j < cols; j++) flat[i * cols + j] = table[i][j];
        return HypothesisNative.Chi2Contingency(flat, rows, cols);
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
        return StatsDistNative.GammaPdf(shape, scale, x);
    }

    /// <summary>scipy.stats.gamma.cdf — incomplete gamma regolarizzata P(a, x/s).</summary>
    public static double GammaCdf(double shape, double scale, double x)
    {
        if (x < 0) return 0;
        return StatsDistNative.GammaCdf(shape, scale, x);
    }
    /// <summary>Funzione di sopravvivenza Gamma (1 - CDF).</summary>
    public static double GammaSf(double shape, double scale, double x) => StatsDistNative.GammaSf(shape, scale, x);

    /// <summary>scipy.stats.gamma.ppf — inversa della CDF (kernel C, gammaincinv).</summary>
    public static double GammaPpf(double shape, double scale, double p)
    {
        if (p <= 0) return 0;
        if (p >= 1) return double.PositiveInfinity;
        return StatsDistNative.GammaPpf(shape, scale, p);
    }

    /// <summary>scipy.stats.gamma.isf — 1 − ppf(p).</summary>
    public static double GammaIsf(double shape, double scale, double p) => StatsDistNative.GammaIsf(shape, scale, p);

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
        return HypothesisNative.SkewTest(data);
    }

    /// <summary>scipy.stats.kurtosistest — test sulla curtosi. Riusa la stessa trasformata Z
    /// precisa di <see cref="NormalityTest"/> (KurtosisTestZ in HypothesisTests.cs), stesso
    /// motivo di <see cref="SkewTest"/>.</summary>
    public static StatisticalTestResult KurtosisTest(double[] data)
    {
        var n = data.Length;
        if (n < 4) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        return HypothesisNative.KurtosisTest(data);
    }

    /// <summary>scipy.stats.jarque_bera — JB = n/6·(S² + (K−3)²/4), p da chi²(2).</summary>
    public static StatisticalTestResult JarqueBeraTest(double[] data)
    {
        var n = data.Length;
        if (n < 3) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        return HypothesisNative.JarqueBera(data);
    }

    // ========================================================================
    // Descrittive: mode, moment, iqr, describe
    // ========================================================================

    /// <summary>scipy.stats.mode — valore più frequente (pareggi → il più piccolo).</summary>
    public static double Mode(double[] data)
    {
        if (data.Length == 0) return double.NaN;
        return StatsDistNative.Mode(data, data.Length);
    }

    /// <summary>scipy.stats.moment — momento centrale di ordine k (attorno alla media).</summary>
    public static double Moment(double[] data, int order)
    {
        var n = data.Length;
        if (n == 0) return double.NaN;
        return StatsDistNative.Moment(data, n, order);
    }

    /// <summary>scipy.stats.iqr — Q3 − Q1 (numpy.percentile, interpolazione lineare).</summary>
    public static double Iqr(double[] data) => StatsDistNative.Iqr(data, data.Length);

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
