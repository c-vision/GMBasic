// FILE: CommonLib.Numerics.HypothesisNative.cs
// TEST DI IPOTESI: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `HypothesisTests.cs` calcolava in C# (MathNet) gli stessi test che `gmb_stats.c`
// implementa per VM/C e C99Native. La lib `gmb_stats_flat` fa il marshalling (liste del runtime)
// e restituisce Statistic/PValue/DegreesOfFreedom; la formula resta UNA (quella del kernel).
//
// ERRORI. Ritorno 0 del kernel = input rifiutato → `InvalidOperationException` esplicita;
// lib assente → `DllNotFoundException`. Nessun fallback al C#.

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

internal static class HypothesisNative
{
    private const string Lib = "gmb_stats_flat";

    [DllImport(Lib, EntryPoint = "gmb_stf_ttest", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeTTest(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_ttest_paired", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeTTestPaired(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_ttest_1samp", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeTTestOneSample(double[] x, int nx, double mu,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_chi2", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeChi2(double[] observed, int nobs, double[] expected, int nexp,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_fisher", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeFisher(long a, long b, long c, long d,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_ks", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeKs(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_shapiro", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeShapiro(double[] x, int n,
        out double stat, out double p, out double df);

    // ── Test compositi (dict) ──────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_stf_chi2con", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeChi2Con(double[] table, int rows, int cols,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_skewtest", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSkewTest(double[] x, int n,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_kurtosistest", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeKurtosisTest(double[] x, int n,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_jarquebera", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeJarqueBera(double[] x, int n,
        out double stat, out double p, out double df);

    // ── Test inferenziali ─────────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_stf_pearsonr", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativePearsonR(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_spearmanr", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSpearmanR(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_kendalltau", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeKendallTau(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_ftest", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeFTest(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_mannwhitney", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeMannWhitney(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_wilcoxon", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeWilcoxon(double[] x, int nx, double[] y, int ny,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_kruskal", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeKruskal(double[] data, int[] sizes, int k,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_linregress", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeLinRegress(double[] x, int nx, double[] y, int ny,
        out double slope, out double intercept, out double r, out double pvalue, out double stderr);
    [DllImport(Lib, EntryPoint = "gmb_stf_normaltest", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeNormalityTest(double[] x, int n,
        out double stat, out double p, out double df);
    [DllImport(Lib, EntryPoint = "gmb_stf_gp_regress", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeGpRegress(double[] x, double[] y, int n, double[] xs, int m,
        double l2, double noise, double[] mean, double[] var);
    [DllImport(Lib, EntryPoint = "gmb_stf_gmm_em1d", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeGmmEm1d(double[] x, int n, int k, int iters,
        double[] mu, double[] var, double[] weight);

    [DllImport(Lib, EntryPoint = "gmb_stf_describe", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeDescribe(double[] a, int n,
        out double nobs, out double vmin, out double vmax, out double mean,
        out double variance, out double skewness, out double kurtosis);

    private static StatisticalTestResult Result(int ok, double stat, double p, double df)
    {
        if (ok == 0)
            throw new InvalidOperationException("STATS: il kernel C ha rifiutato l'input del test.");
        return new StatisticalTestResult(stat, p, df);
    }

    internal static StatisticalTestResult TTestIndependent(double[] x, double[] y)
    {
        var ok = NativeTTest(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult TTestPaired(double[] x, double[] y)
    {
        var ok = NativeTTestPaired(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult TTestOneSample(double[] x, double mu)
    {
        var ok = NativeTTestOneSample(x, x.Length, mu, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult Chi2(double[] observed, double[] expected)
    {
        var ok = NativeChi2(observed, observed.Length, expected, expected.Length,
            out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult Fisher(int a, int b, int c, int d)
    {
        var ok = NativeFisher(a, b, c, d, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult KolmogorovSmirnov(double[] x, double[] y)
    {
        var ok = NativeKs(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult ShapiroWilk(double[] x)
    {
        var ok = NativeShapiro(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    /// <summary>`table` è appiattita row-major (rows × cols).</summary>
    internal static StatisticalTestResult Chi2Contingency(double[] table, int rows, int cols)
    {
        var ok = NativeChi2Con(table, rows, cols, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult SkewTest(double[] x)
    {
        var ok = NativeSkewTest(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult KurtosisTest(double[] x)
    {
        var ok = NativeKurtosisTest(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static (double[] Mean, double[] Variance) GpRegress(double[] x, double[] y, double[] xs, double l2, double noise)
    {
        var mean = new double[xs.Length];
        var variance = new double[xs.Length];
        var ok = NativeGpRegress(x, y, x.Length, xs, xs.Length, l2, noise, mean, variance);
        if (ok == 0) throw new InvalidOperationException("STATS: il kernel C ha rifiutato l'input di GP regression.");
        return (mean, variance);
    }

    internal static (double[] Mu, double[] Variance, double[] Weight) GmmEm1D(double[] x, int k, int iters)
    {
        var mu = new double[k];
        var variance = new double[k];
        var weight = new double[k];
        var ok = NativeGmmEm1d(x, x.Length, k, iters, mu, variance, weight);
        if (ok == 0) throw new InvalidOperationException("STATS: il kernel C ha rifiutato l'input di GMM EM 1D.");
        return (mu, variance, weight);
    }

    internal static StatisticalTestResult NormalityTest(double[] x)

    {
        var ok = NativeNormalityTest(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult JarqueBera(double[] x)
    {
        var ok = NativeJarqueBera(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult PearsonR(double[] x, double[] y)
    {
        var ok = NativePearsonR(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult SpearmanR(double[] x, double[] y)
    {
        var ok = NativeSpearmanR(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult KendallTau(double[] x, double[] y)
    {
        var ok = NativeKendallTau(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult FTest(double[] x, double[] y)
    {
        var ok = NativeFTest(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult MannWhitney(double[] x, double[] y)
    {
        var ok = NativeMannWhitney(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult Wilcoxon(double[] x, double[] y)
    {
        var ok = NativeWilcoxon(x, x.Length, y, y.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static StatisticalTestResult Kruskal(double[] flat, int[] sizes)
    {
        var ok = NativeKruskal(flat, sizes, sizes.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }

    internal static (double Slope, double Intercept, double R, double PValue, double Stderr) LinRegress(double[] x, double[] y)
    {
        var ok = NativeLinRegress(x, x.Length, y, y.Length,
            out var slope, out var intercept, out var r, out var p, out var stderr);
        if (ok == 0)
            throw new InvalidOperationException("STATS: il kernel C ha rifiutato l'input di LINREGRESS.");
        return (slope, intercept, r, p, stderr);
    }

    internal static (double Nobs, double Min, double Max, double Mean, double Variance, double Skewness, double Kurtosis)
        Describe(double[] a)
    {
        var ok = NativeDescribe(a, a.Length, out var nobs, out var vmin, out var vmax,
            out var mean, out var variance, out var skewness, out var kurtosis);
        if (ok == 0)
            throw new InvalidOperationException("STATS: il kernel C ha rifiutato l'input di DESCRIBE.");
        return (nobs, vmin, vmax, mean, variance, skewness, kurtosis);
    }
}
