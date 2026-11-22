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
        return HypothesisNative.PearsonR(x, y);
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
        return HypothesisNative.SpearmanR(x, y);
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
        return HypothesisNative.Kruskal(flat, sizes);
    }

    public static StatisticalTestResult KendallTau(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        return HypothesisNative.KendallTau(x, y);
    }

    public static StatisticalTestResult FTest(double[] x, double[] y)
    {
        if (x.Length < 2 || y.Length < 2)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        return HypothesisNative.FTest(x, y);
    }

    /// <summary>scipy.stats.linregress — regressione lineare OLS (slope, intercept, r, p, se).</summary>
    public static LinRegressResult LinRegress(double[] x, double[] y)
    {
        var n = Math.Min(x.Length, y.Length);
        if (n < 2) return new LinRegressResult(double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        var (slope, intercept, r, p, stderr) = HypothesisNative.LinRegress(x, y);
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
        if (x.Length == 0 || y.Length == 0)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        return HypothesisNative.MannWhitney(x, y);
    }

    /// <summary>scipy.stats.wilcoxon — test dei ranghi con segno (approssimazione normale).</summary>
    public static StatisticalTestResult Wilcoxon(double[] x, double[] y)
    {
        return HypothesisNative.Wilcoxon(x, y);
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
        if (data.Length == 0) return new DescribeResult(0, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        var (nobs, vmin, vmax, mean, variance, skewness, kurtosis) = HypothesisNative.Describe(data);
        return new DescribeResult(nobs, vmin, vmax, mean, variance, skewness, kurtosis);
    }
}
