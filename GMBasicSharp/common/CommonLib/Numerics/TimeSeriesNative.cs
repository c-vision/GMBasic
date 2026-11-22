// FILE: CommonLib.Numerics.TimeSeriesNative.cs
// TIMESERIES: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. Le funzioni di analisi di serie temporali erano calcolate in C# (TimeSeries.cs) mentre
// VM/C e C99Native usano `gmb_timeseries.c`: due implementazioni indipendenti della stessa
// formula. Qui il managed chiama la lib `gmb_ts_flat` (inoltro puro alle funzioni del runtime
// condiviso, come `gmb_matrix_flat` per i kernel matriciali): una sola fonte numerica,
// risultati identici sulle 4 gambe.
//
// ONDA 1: MOVINGAVERAGE, EMA, DIFFERENCE, RETURNS, VOLATILITY, AUTOCORRELATION.
// ONDA 2: FRACDIFF, KALMAN1D, STATIONARYBOOTSTRAP, SAMPLE/APPROX/PERM ENTROPY, LZCOMPLEXITY,
//         LYAPUNOV, BMAWEIGHTS, HMMFORWARD, HMMVITERBI, COVARIANCEMATRIX.
//
// OWNERSHIP. Le funzioni C allocano il buffer (malloc/calloc): qui si copia e si libera con `free`.
// ERRORI. Su errore il kernel ritorna NULL e il messaggio in `gmb_tsf_last_error()`; la
// chiamata solleva `InvalidOperationException` con QUEL messaggio (nessun fallback silenzioso).

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

internal static class TimeSeriesNative
{
    private const string Lib = "gmb_ts_flat";

    [DllImport(Lib, EntryPoint = "gmb_tsf_moving_average", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeMovingAverage(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_ema", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeEma(double[] data, long n, double alpha, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_returns", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeReturns(double[] data, long n, int logReturns, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_difference", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeDifference(double[] data, long n, long order, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_volatility", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeVolatility(double[] data, long n, int logReturns);
    [DllImport(Lib, EntryPoint = "gmb_tsf_autocorrelation", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeAutocorrelation(double[] data, long n, long lag);

    // ── Onda 2: firme flat (int), identiche ai kernel di gmb_timeseries.c ────────────────────
    [DllImport(Lib, EntryPoint = "gmb_tsf_fracdiff", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeFracDiff(double[] x, int n, double d, out int outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_kalman1d", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeKalman1D(double[] z, int n, double a, double h, double q, double r, out int outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_stationary_bootstrap", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeStationaryBootstrap(double[] x, int n, int window, int seed, out int outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_sample_entropy", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeSampleEntropy(double[] x, int n, int m, double r);
    [DllImport(Lib, EntryPoint = "gmb_tsf_approx_entropy", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeApproxEntropy(double[] x, int n, int m, double r);
    [DllImport(Lib, EntryPoint = "gmb_tsf_perm_entropy", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativePermEntropy(double[] x, int n, int m, int lag);
    [DllImport(Lib, EntryPoint = "gmb_tsf_lz_complexity", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeLzComplexity(double[] x, int n);
    [DllImport(Lib, EntryPoint = "gmb_tsf_lyapunov", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeLyapunov(double[] x, int n, int m, int tau);
    [DllImport(Lib, EntryPoint = "gmb_tsf_bma_weights", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeBmaWeights(double[] ic, int n, out int outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_hmm_forward", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeHmmForward(double[] lpi, double[] lA, double[] lE, int k, int n);
    [DllImport(Lib, EntryPoint = "gmb_tsf_hmm_viterbi", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeHmmViterbi(double[] lpi, double[] lA, double[] lE, int k, int n, out int outN);
    [DllImport(Lib, EntryPoint = "gmb_tsf_covariance_obs", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeCovarianceObs(IntPtr[] obs, int nObs, int nVars);

    // ── Onda 3: destagionalizzazione, outlier, Hurst, ROLLING* ──────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_tsf_deseasonalize", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeDeseasonalize(double[] data, long n, long season, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_outliers", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeOutliers(double[] data, long n, double threshold, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_hurst_dfa", CallingConvention = CallingConvention.Cdecl)]
    private static extern double NativeHurstDfa(double[] x, int n, int minBox);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_mean", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingMean(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_stddev", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingStdDev(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_skewness", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingSkewness(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_kurtosis", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingKurtosis(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_median", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingMedian(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_quantile", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingQuantile(double[] data, long n, long window, double q, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_sharpe", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingSharpe(double[] data, long n, long window, double rf, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_mad", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingMad(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_zscore", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingZScore(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_autocorr", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingAutocorrelation(double[] data, long n, long window, long lag, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_cov", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingCovariance(double[] x, double[] y, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_corr", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingCorrelation(double[] x, double[] y, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_minmax", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingMinMax(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_regression", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingRegressionPairs(double[] x, double[] y, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_regression_single", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingRegressionSingle(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolldelta", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingDelta(double[] data, long n, long window, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolldelta_order", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingDeltaOrder(double[] data, long n, long window, long order, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_outliers_iqr", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeOutliersIqr(double[] data, long n, double threshold, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_decompose_matrix", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeDecomposeMatrix(double[] data, long n, long period, out long outLen);
    [DllImport(Lib, EntryPoint = "gmb_tsf_rolling_sortino", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeRollingSortino(double[] data, long n, long window, double rf, double target, out long outLen);

    // ── Onda 4: kernel che ritornano gmb_val_t (dict) ──────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gmb_tsf_garch11", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeGarch11(double[] r, int n, out double omega, out double alpha, out double beta);
    [DllImport(Lib, EntryPoint = "gmb_tsf_qlr", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeQlr(double[] lBeta, int nBeta, double l0, double kappaHat,
        out double qlrT, out double qlrTilde, out int bestIndex);
    [DllImport(Lib, EntryPoint = "gmb_tsf_ou_fit", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeOuFit(double[] x, int n, double dt,
        out double theta, out double mu, out double sigma, out double phi, out double halfLife);
    [DllImport(Lib, EntryPoint = "gmb_tsf_granger", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeGranger(double[] x, int nx, double[] y, int ny, int p,
        out double stat, out double pvalue, out double df);
    [DllImport(Lib, EntryPoint = "gmb_tsf_kpss", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeKpss(double[] series, int n, int bandwidth, int deterministic,
        out double stat, out int bw, out int observations, out int det, out double lrv,
        out double c10, out double c5, out double c25, out double c1);
    [DllImport(Lib, EntryPoint = "gmb_tsf_dfgls", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeDfgls(double[] series, int n, int lags, int deterministic,
        out double stat, out int lagsOut, out int observations, out int det, out double alphaBar);
    [DllImport(Lib, EntryPoint = "gmb_tsf_engle_granger", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeEngleGranger(double[] yFlat, int n, int k, int dependent, int lags,
        int withConstant, out double stat, out int lagsOut, out int observations, out int nRegressors,
        out int wc);
    [DllImport(Lib, EntryPoint = "gmb_tsf_johansen", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeJohansen(double[] yFlat, int nObs, int k, int lags,
        double[] eigenValues, double[] traceStatistic, double[] maxStatistic, out int observations);
    [DllImport(Lib, EntryPoint = "gmb_tsf_hmm_fit", CallingConvention = CallingConvention.Cdecl)]
    private static extern int NativeHmmFit(double[] xFlat, int n, int d, int k, int nIter,
        double[] pi, double[] a, double[] means, double[] covs, out double logLikelihood);

    [DllImport(Lib, EntryPoint = "gmb_tsf_last_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr NativeLastError();
    [DllImport(Lib, EntryPoint = "gmb_tsf_clear_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern void NativeClearError();

    /* il buffer dei risultati è allocato dal runtime: va liberato con `free` */
    [DllImport("libc", EntryPoint = "free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void libc_free(IntPtr p);

    private static string LastError()
    {
        try
        {
            var p = NativeLastError();
            var s = p == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(p);
            return string.IsNullOrEmpty(s) ? "TIMESERIES: errore nel kernel C" : s!;
        }
        catch
        {
            return "TIMESERIES: errore nel kernel C";
        }
    }

    private static double[] Take(IntPtr p, long len)
    {
        try
        {
            if (p == IntPtr.Zero)
                throw new InvalidOperationException(LastError());
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

    private static void Begin() { try { NativeClearError(); } catch { /* non fatale */ } }

    internal static double[] MovingAverage(double[] data, int window)
    {
        Begin();
        return Take(NativeMovingAverage(data, data.Length, window, out var n), n);
    }

    internal static double[] Ema(double[] data, double alpha)
    {
        Begin();
        return Take(NativeEma(data, data.Length, alpha, out var n), n);
    }

    internal static double[] Returns(double[] data, bool logReturns)
    {
        Begin();
        return Take(NativeReturns(data, data.Length, logReturns ? 1 : 0, out var n), n);
    }

    internal static double[] Difference(double[] data, int order)
    {
        Begin();
        return Take(NativeDifference(data, data.Length, order, out var n), n);
    }

    internal static double Volatility(double[] data, bool logReturns)
    {
        Begin();
        return NativeVolatility(data, data.Length, logReturns ? 1 : 0);
    }

    internal static double Autocorrelation(double[] data, int lag)
    {
        Begin();
        return NativeAutocorrelation(data, data.Length, lag);
    }

    // ── Onda 2 ──────────────────────────────────────────────────────────────────────────────

    internal static double[] FracDiff(double[] x, double d)
    {
        Begin();
        return Take(NativeFracDiff(x, x.Length, d, out var n), n);
    }

    internal static double[] Kalman1D(double[] z, double a, double h, double q, double r)
    {
        Begin();
        return Take(NativeKalman1D(z, z.Length, a, h, q, r, out var n), n);
    }

    internal static double[] StationaryBootstrap(double[] x, int window, int seed)
    {
        Begin();
        return Take(NativeStationaryBootstrap(x, x.Length, window, seed, out var n), n);
    }

    internal static double SampleEntropy(double[] x, int m, double r)
    {
        Begin();
        return NativeSampleEntropy(x, x.Length, m, r);
    }

    internal static double ApproxEntropy(double[] x, int m, double r)
    {
        Begin();
        return NativeApproxEntropy(x, x.Length, m, r);
    }

    internal static double PermEntropy(double[] x, int m, int lag)
    {
        Begin();
        return NativePermEntropy(x, x.Length, m, lag);
    }

    internal static double LzComplexity(double[] x)
    {
        Begin();
        return NativeLzComplexity(x, x.Length);
    }

    internal static double Lyapunov(double[] x, int m, int tau)
    {
        Begin();
        return NativeLyapunov(x, x.Length, m, tau);
    }

    internal static double[] BmaWeights(double[] ic)
    {
        Begin();
        return Take(NativeBmaWeights(ic, ic.Length, out var n), n);
    }

    internal static double HmmForward(double[] lpi, double[] lA, double[] lE, int k, int n)
    {
        Begin();
        return NativeHmmForward(lpi, lA, lE, k, n);
    }

    internal static double[] HmmViterbi(double[] lpi, double[] lA, double[] lE, int k, int n)
    {
        Begin();
        return Take(NativeHmmViterbi(lpi, lA, lE, k, n, out var len), len);
    }

    /// <summary>
    /// Covarianza campionaria (Bessel, n-1) di <paramref name="data"/> = n osservazioni × m
    /// variabili. Il kernel C ritorna un jagged `row[i][j]`: qui si copia in una matrice e si
    /// liberano le righe + il vettore dei puntatori.
    /// </summary>
    internal static double[,] CovarianceMatrix(double[][] data)
    {
        var nObs = data.Length;
        var nVars = data[0].Length;
        var rows = new IntPtr[nObs];
        var handles = new GCHandle[nObs];
        try
        {
            for (var i = 0; i < nObs; i++)
            {
                handles[i] = GCHandle.Alloc(data[i], GCHandleType.Pinned);
                rows[i] = handles[i].AddrOfPinnedObject();
            }
            Begin();
            var jag = NativeCovarianceObs(rows, nObs, nVars);
            if (jag == IntPtr.Zero) throw new InvalidOperationException(LastError());
            try
            {
                var m = new double[nVars, nVars];
                for (var i = 0; i < nVars; i++)
                {
                    var row = Marshal.ReadIntPtr(jag, i * IntPtr.Size);
                    if (row == IntPtr.Zero) continue;   // alloc fallita nel kernel: riga a zero
                    var tmp = new double[nVars];
                    Marshal.Copy(row, tmp, 0, nVars);
                    for (var j = 0; j < nVars; j++) m[i, j] = tmp[j];
                    libc_free(row);
                }
                return m;
            }
            finally
            {
                libc_free(jag);
            }
        }
        finally
        {
            for (var i = 0; i < nObs; i++)
                if (handles[i].IsAllocated) handles[i].Free();
        }
    }

    // ── Onda 3 ──────────────────────────────────────────────────────────────────────────────

    internal static double[] Deseasonalize(double[] data, int season)
    {
        Begin();
        return Take(NativeDeseasonalize(data, data.Length, season, out var n), n);
    }

    /// <summary>Indici (metodo Z) come interi. Il kernel ritorna NULL quando non ci sono
    /// outlier o la serie è troppo corta: qui significa lista vuota, non errore.</summary>
    internal static int[] OutliersZ(double[] data, double threshold)
    {
        Begin();
        var p = NativeOutliers(data, data.Length, threshold, out var len);
        if (p == IntPtr.Zero) return [];
        var buf = Take(p, len);
        var res = new int[buf.Length];
        for (var i = 0; i < buf.Length; i++) res[i] = (int)buf[i];
        return res;
    }

    internal static double HurstDfa(double[] x, int minBox) => NativeHurstDfa(x, x.Length, minBox);

    internal static double[] RollingMean(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingMean(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingStdDev(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingStdDev(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingSkewness(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingSkewness(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingKurtosis(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingKurtosis(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingMedian(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingMedian(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingQuantile(double[] data, int window, double quantile)
    {
        Begin();
        return Take(NativeRollingQuantile(data, data.Length, window, quantile, out var n), n);
    }

    internal static double[] RollingSharpe(double[] data, int window, double riskFreeRate)
    {
        Begin();
        return Take(NativeRollingSharpe(data, data.Length, window, riskFreeRate, out var n), n);
    }

    internal static double[] RollingMad(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingMad(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingZScore(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingZScore(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingAutocorrelation(double[] data, int window, int lag)
    {
        Begin();
        return Take(NativeRollingAutocorrelation(data, data.Length, window, lag, out var n), n);
    }

    internal static double[] RollingCovariance(double[] x, double[] y, int window)
    {
        Begin();
        return Take(NativeRollingCovariance(x, y, Math.Min(x.Length, y.Length), window, out var n), n);
    }

    internal static double[] RollingCorrelation(double[] x, double[] y, int window)
    {
        Begin();
        return Take(NativeRollingCorrelation(x, y, Math.Min(x.Length, y.Length), window, out var n), n);
    }

    /// <summary>Kernel piatto [min,max] per finestra → array di tuple.</summary>
    internal static (double min, double max)[] RollingMinMax(double[] data, int window)
    {
        Begin();
        var flat = Take(NativeRollingMinMax(data, data.Length, window, out var n), n);
        var res = new (double min, double max)[flat.Length / 2];
        for (var i = 0; i < res.Length; i++) res[i] = (flat[2 * i], flat[2 * i + 1]);
        return res;
    }

    /// <summary>Kernel piatto [slope,intercept] per finestra → array di tuple.</summary>
    internal static (double slope, double intercept)[] RollingRegression(double[] y, int window)
    {
        Begin();
        var flat = Take(NativeRollingRegressionSingle(y, y.Length, window, out var n), n);
        var res = new (double slope, double intercept)[flat.Length / 2];
        for (var i = 0; i < res.Length; i++) res[i] = (flat[2 * i], flat[2 * i + 1]);
        return res;
    }

    /// <summary>Kernel piatto [slope,intercept] per finestra → array di tuple.</summary>
    internal static (double slope, double intercept)[] RollingRegressionPairs(double[] x, double[] y, int window)
    {
        Begin();
        var flat = Take(NativeRollingRegressionPairs(x, y, Math.Min(x.Length, y.Length), window, out var n), n);
        var res = new (double slope, double intercept)[flat.Length / 2];
        for (var i = 0; i < res.Length; i++) res[i] = (flat[2 * i], flat[2 * i + 1]);
        return res;
    }

    internal static double[] RollingDelta(double[] data, int window)
    {
        Begin();
        return Take(NativeRollingDelta(data, data.Length, window, out var n), n);
    }

    internal static double[] RollingDelta(double[] data, int window, int order)
    {
        Begin();
        return Take(NativeRollingDeltaOrder(data, data.Length, window, order, out var n), n);
    }

    /// <summary>Indici (metodo IQR) come interi; kernel NULL = nessun outlier, non errore.</summary>
    internal static int[] OutliersIqr(double[] data, double threshold)
    {
        Begin();
        var p = NativeOutliersIqr(data, data.Length, threshold, out var len);
        if (p == IntPtr.Zero) return [];
        var buf = Take(p, len);
        var res = new int[buf.Length];
        for (var i = 0; i < buf.Length; i++) res[i] = (int)buf[i];
        return res;
    }

    /// <summary>Decomposizione additiva: matrice 3×n appiattita in [trend, seasonal, residual].</summary>
    internal static double[] DecomposeMatrix(double[] data, int period)
    {
        Begin();
        return Take(NativeDecomposeMatrix(data, data.Length, period, out var n), n);
    }

    internal static double[] RollingSortino(double[] data, int window, double rf, double target)
    {
        Begin();
        return Take(NativeRollingSortino(data, data.Length, window, rf, target, out var n), n);
    }

    // ── Onda 4: dict → tuple/array ──────────────────────────────────────────────────────────

    private static void Require(int ok, string what)
    {
        if (ok == 0) throw new InvalidOperationException("TIMESERIES: il kernel C ha rifiutato l'input di " + what + ".");
    }

    /// <summary>Appiattisce righe jagged (n × cols) nel layout row-major atteso dai kernel C.</summary>
    private static double[] Flatten(double[][] rows, int n, int cols)
    {
        var flat = new double[(long)n * cols];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < cols; j++) flat[i * cols + j] = rows[i][j];
        return flat;
    }

    internal static (double Omega, double Alpha, double Beta) Garch11(double[] r)
    {
        Begin();
        var ok = NativeGarch11(r, r.Length, out var omega, out var alpha, out var beta);
        Require(ok, "GARCH11");
        return (omega, alpha, beta);
    }

    internal static (double QlrT, double QlrTilde, int BestIndex) Qlr(double l0, double[] lBeta, double kappaHat)
    {
        Begin();
        var ok = NativeQlr(lBeta, lBeta.Length, l0, kappaHat, out var t, out var tilde, out var best);
        Require(ok, "QLR");
        return (t, tilde, best);
    }

    internal static (double Theta, double Mu, double Sigma, double Phi, double HalfLife) OuFit(double[] x, double dt)
    {
        Begin();
        var ok = NativeOuFit(x, x.Length, dt, out var theta, out var mu, out var sigma, out var phi, out var hl);
        Require(ok, "OUFIT");
        return (theta, mu, sigma, phi, hl);
    }

    internal static (double Statistic, double PValue, double DegreesOfFreedom) Granger(double[] x, double[] y, int p)
    {
        Begin();
        var ok = NativeGranger(x, x.Length, y, y.Length, p, out var stat, out var pv, out var df);
        Require(ok, "GRANGER");
        return (stat, pv, df);
    }

    internal static (double Statistic, int Bandwidth, int Observations, int Deterministic,
        double LongRunVariance, double Critical10, double Critical5, double Critical25, double Critical1)
        Kpss(double[] series, int bandwidth, int deterministic)
    {
        Begin();
        var ok = NativeKpss(series, series.Length, bandwidth, deterministic,
            out var stat, out var bw, out var obs, out var det, out var lrv,
            out var c10, out var c5, out var c25, out var c1);
        Require(ok, "KPSS");
        return (stat, bw, obs, det, lrv, c10, c5, c25, c1);
    }

    internal static (double Statistic, int Lags, int Observations, int Deterministic, double AlphaBar)
        Dfgls(double[] series, int lags, int deterministic)
    {
        Begin();
        var ok = NativeDfgls(series, series.Length, lags, deterministic,
            out var stat, out var lagsOut, out var obs, out var det, out var alphaBar);
        Require(ok, "DFGLS");
        return (stat, lagsOut, obs, det, alphaBar);
    }

    internal static (double Statistic, int Lags, int Observations, int NRegressors, int WithConstant)
        EngleGranger(double[][] y, int dependent, int lags, int withConstant)
    {
        var n = y.Length;
        var k = n > 0 ? y[0].Length : 0;
        Begin();
        var ok = NativeEngleGranger(Flatten(y, n, k), n, k, dependent, lags, withConstant,
            out var stat, out var lagsOut, out var obs, out var nreg, out var wc);
        Require(ok, "ENGLEGRANGER");
        return (stat, lagsOut, obs, nreg, wc);
    }

    internal static (double[] EigenValues, double[] TraceStatistic, double[] MaxStatistic, int Observations, int Lags)
        Johansen(double[][] y, int lags)
    {
        var nObs = y.Length;
        var k = nObs > 0 ? y[0].Length : 0;
        var eval = new double[k];
        var trace = new double[k];
        var maxs = new double[k];
        Begin();
        var ok = NativeJohansen(Flatten(y, nObs, k), nObs, k, lags, eval, trace, maxs, out var obs);
        Require(ok, "JOHANSEN");
        return (eval, trace, maxs, obs, lags);
    }

    internal static (double[] Pi, double[] A, double[] Means, double[] Covs, double LogLikelihood)
        HmmFit(double[][] x, int nStates, int nIter)
    {
        var n = x.Length;
        var d = n > 0 ? x[0].Length : 0;
        var k = nStates;
        var pi = new double[k];
        var a = new double[k * k];
        var means = new double[k * d];
        var covs = new double[k * d * d];
        Begin();
        var ok = NativeHmmFit(Flatten(x, n, d), n, d, k, nIter, pi, a, means, covs, out var ll);
        Require(ok, "HMMFIT");
        return (pi, a, means, covs, ll);
    }
}
