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

    internal static StatisticalTestResult JarqueBera(double[] x)
    {
        var ok = NativeJarqueBera(x, x.Length, out var stat, out var p, out var df);
        return Result(ok, stat, p, df);
    }
}
