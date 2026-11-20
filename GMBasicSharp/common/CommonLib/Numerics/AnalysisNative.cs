// FILE: CommonLib.Numerics.AnalysisNative.cs
// ANALISI / REGRESSIONE: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `Advanced.cs` calcolava in C# (MathNet o formule inline) le stesse grandezze che
// `gmb_math.c` calcola per VM/C e C99Native. Qui il managed chiama la lib `gmb_analysis_flat`
// (inoltro puro ai kernel): una sola fonte numerica, stessi valori sulle 4 gambe.
//
// OWNERSHIP. `gmb_anf_yule_walker`/`gmb_anf_ma_coefficients` allocano nel runtime: qui si copia e
// si libera con `free`. ERRORI. Le validazioni restano nel chiamante; lib assente →
// `DllNotFoundException` (nessun fallback al C#).

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

internal static class AnalysisNative
{
    private const string Lib = "gmb_analysis_flat";

    [DllImport(Lib, EntryPoint = "gmb_anf_euclidean_distance_squared", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeEuclideanDistanceSquared(double[] a, double[] b, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_linear_regression", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeLinearRegression(double[] x, double[] y, int n,
        out double slope, out double intercept, out double r2);
    [DllImport(Lib, EntryPoint = "gmb_anf_yule_walker", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeYuleWalker(double[] acf, int n, int p, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_anf_ma_coefficients", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeMaCoefficients(double[] residuals, int n, int q, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_anf_min_semivariance", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMinSemiVariance(double[] returns, int n, double threshold);

    /* i buffer dei risultati sono allocati dal runtime: vanno liberati con `free` */
    [DllImport("libc", EntryPoint = "free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void libc_free(IntPtr p);

    private static double[] Take(IntPtr p, long len, string what)
    {
        try
        {
            if (p == IntPtr.Zero)
                throw new InvalidOperationException("ANALYSIS: il kernel C non ha prodotto " + what + ".");
            if (len <= 0) return [];
            var outArr = new double[len];
            Marshal.Copy(p, outArr, 0, (int)len);
            return outArr;
        }
        finally
        {
            if (p != IntPtr.Zero)
            {
                try { libc_free(p); } catch { /* piattaforma senza libc: buffer perso, non fatale */ }
            }
        }
    }

    internal static double EuclideanDistanceSquared(double[] a, double[] b)
        => NativeEuclideanDistanceSquared(a, b, a.Length);

    internal static (double slope, double intercept, double rSquared) LinearRegression(double[] x, double[] y, int n)
    {
        NativeLinearRegression(x, y, n, out var slope, out var intercept, out var r2);
        return (slope, intercept, r2);
    }

    internal static double[] YuleWalker(double[] acf, int p)
        => Take(NativeYuleWalker(acf, acf.Length, p, out var len), len, "i coefficienti AR");

    internal static double[] MaCoefficients(double[] residuals, int q)
        => Take(NativeMaCoefficients(residuals, residuals.Length, q, out var len), len, "i coefficienti MA");

    internal static double MinSemiVariance(double[] returns, double threshold)
        => NativeMinSemiVariance(returns, returns.Length, threshold);
}
