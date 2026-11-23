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

    // ── Descrittive (Stats.cs) ─────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_anf_mean", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMean(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_median", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMedian(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_variance", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeVariance(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_stddev", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeStdDev(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_skewness", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeSkewness(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_kurtosis", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeKurtosis(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_correlation", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeCorrelation(double[] x, double[] y, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_min", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMin(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_max", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMax(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_percentile", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativePercentile(double[] a, int n, double percent);
    [DllImport(Lib, EntryPoint = "gmb_anf_mad", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeMad(double[] a, int n);

    // ── Fitting ────────────────────────────────────────────────────────────────────────────
    internal static double ChebEval(double x, double[] coef) => NativeChebEval(x, coef, coef.Length);
    internal static double PadeEval(double x, double[] a, double[] b) => NativePadeEval(x, a, a.Length, b, b.Length);
    internal static double PolyEval(double x, double[] coef) => NativePolyEval(x, coef, coef.Length - 1);
    internal static double PolyLs(double[] x, double[] y, int m, double[] coef)
        => NativePolyLs(x, y, Math.Min(x.Length, y.Length), m, coef);
    internal static (double R2, double Angle) RSquared(double[] data)
    {
        NativeRSquared(data, data.Length, out var r2, out var angle);
        return (r2, angle);
    }
    internal static double RSquaredTest(double[] data) => NativeRSquaredTest(data, data.Length);

    // ── signal/fft ─────────────────────────────────────────────────────────────────────────
    /// <summary>Welch: matrice [nfreqs, 2] (frequenza, PSD).</summary>
    internal static double[,] Welch(double[] x, int nperseg, double fs)
    {
        var p = NativeWelch(x, x.Length, nperseg, fs, out var rows);
        try
        {
            if (p == IntPtr.Zero || rows <= 0)
                throw new InvalidOperationException("SIGNAL: il kernel C non ha prodotto WELCH.");
            var flat = new double[rows * 2];
            Marshal.Copy(p, flat, 0, rows * 2);
            var res = new double[rows, 2];
            for (var k = 0; k < rows; k++) { res[k, 0] = flat[2 * k]; res[k, 1] = flat[2 * k + 1]; }
            return res;
        }
        finally { if (p != IntPtr.Zero) { try { libc_free(p); } catch { } } }
    }

    /// <summary>RFFT: componenti a frequenza non negativa, interleaved [re0, im0, re1, im1, …].</summary>
    internal static double[] Rfft(double[] x)
    {
        var p = NativeRfft(x, x.Length, out var len);
        try
        {
            if (p == IntPtr.Zero) throw new InvalidOperationException("FFT: il kernel C non ha prodotto RFFT.");
            if (len <= 0) return [];
            var res = new double[len * 2];
            Marshal.Copy(p, res, 0, (int)(len * 2));
            return res;
        }
        finally { if (p != IntPtr.Zero) { try { libc_free(p); } catch { } } }
    }

    internal static double[] Irfft(double[] re, double[] im)
    {
        var p = NativeIrfft(re, im, re.Length, out var len);
        try
        {
            if (p == IntPtr.Zero) throw new InvalidOperationException("FFT: il kernel C non ha prodotto IRFFT.");
            if (len <= 0) return [];
            var res = new double[len];
            Marshal.Copy(p, res, 0, (int)len);
            return res;
        }
        finally { if (p != IntPtr.Zero) { try { libc_free(p); } catch { } } }
    }

    // ── Import delle funzioni native (Fitting, signal/fft, RMT/SVM) ────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_anf_cheb_eval", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeChebEval(double x, double[] coef, int m);
    [DllImport(Lib, EntryPoint = "gmb_anf_pade_eval", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativePadeEval(double x, double[] a, int na, double[] b, int nb);
    [DllImport(Lib, EntryPoint = "gmb_anf_poly_eval", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativePolyEval(double x, double[] coef, int m);
    [DllImport(Lib, EntryPoint = "gmb_anf_poly_ls", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativePolyLs(double[] x, double[] y, int n, int m, double[] coef);
    [DllImport(Lib, EntryPoint = "gmb_anf_rsquared", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeRSquared(double[] data, int n, out double r2, out double angle);
    [DllImport(Lib, EntryPoint = "gmb_anf_rsquared_test", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRSquaredTest(double[] data, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_welch", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeWelch(double[] x, int n, int nperseg, double fs, out int outRows);
    [DllImport(Lib, EntryPoint = "gmb_anf_rfft", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRfft(double[] x, int n, out long outn);
    [DllImport(Lib, EntryPoint = "gmb_anf_irfft", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeIrfft(double[] re, double[] im, int m, out long outn);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_lambda_plus", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtLambdaPlus(int n, int T, double sigma2);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_lambda_minus", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtLambdaMinus(int n, int T, double sigma2);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_coherence", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtCoherence(double[] v, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_spectral_norm", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtSpectralNorm(double[] a, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_davis_kahan", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtDavisKahan(double eNorm, double gap);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_principal_angle", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtPrincipalAngle(double[] u, double[] v, int n);
    [DllImport(Lib, EntryPoint = "gmb_anf_rmt_tightness", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeRmtTightness(double observed, double bound);
    [DllImport(Lib, EntryPoint = "gmb_anf_svm_fit", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeSvmFit(double[] x, double[] y, int n, int d, int k,
        double c, int maxIter, double[] w);
    [DllImport(Lib, EntryPoint = "gmb_anf_svm_predict", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeSvmPredict(double[] w, double[] x, int d, int k);

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

    // ── Descrittive (Stats.cs) ─────────────────────────────────────────────────────────────

    internal static double Mean(double[] a) => NativeMean(a, a.Length);
    internal static double Median(double[] a) => NativeMedian(a, a.Length);
    internal static double Variance(double[] a) => NativeVariance(a, a.Length);
    internal static double StdDev(double[] a) => NativeStdDev(a, a.Length);
    internal static double Skewness(double[] a) => NativeSkewness(a, a.Length);
    internal static double Kurtosis(double[] a) => NativeKurtosis(a, a.Length);
    internal static double Correlation(double[] x, double[] y) => NativeCorrelation(x, y, x.Length);
    internal static double Min(double[] a) => NativeMin(a, a.Length);
    internal static double Max(double[] a) => NativeMax(a, a.Length);
    /// <summary>`percent` è in percentuale 0..100 (il chiamante arrotonda la frazione come prima).</summary>
    internal static double PercentilePercent(double[] a, double percent) => NativePercentile(a, a.Length, percent);
    internal static double Mad(double[] a) => NativeMad(a, a.Length);

    // ── RMT e SVM ──────────────────────────────────────────────────────────────────────────
    internal static double RmtLambdaPlus(int n, int t, double sigma2) => NativeRmtLambdaPlus(n, t, sigma2);
    internal static double RmtLambdaMinus(int n, int t, double sigma2) => NativeRmtLambdaMinus(n, t, sigma2);
    internal static double RmtCoherence(double[] v) => NativeRmtCoherence(v, v.Length);
    internal static double RmtSpectralNorm(double[] a, int n) => NativeRmtSpectralNorm(a, n);
    internal static double RmtDavisKahan(double eNorm, double gap) => NativeRmtDavisKahan(eNorm, gap);
    internal static double RmtPrincipalAngle(double[] u, double[] v) => NativeRmtPrincipalAngle(u, v, Math.Min(u.Length, v.Length));
    internal static double RmtTightness(double observed, double bound) => NativeRmtTightness(observed, bound);

    /// <summary>Perceptron multi-classe: `w` (k·d) è preallocato dal chiamante.</summary>
    internal static void SvmFit(double[] xFlat, double[] y, int n, int d, int k, double c, int maxIter, double[] w)
        => NativeSvmFit(xFlat, y, n, d, k, c, maxIter, w);

    internal static int SvmPredict(double[] w, double[] x, int d, int k) => NativeSvmPredict(w, x, d, k);
}
