// FILE: CommonLib.Numerics.TimeSeriesNative.cs
// TIMESERIES: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. MOVINGAVERAGE/EMA/DIFFERENCE/RETURNS/VOLATILITY/AUTOCORRELATION erano calcolate in
// C# (TimeSeries.cs) mentre VM/C e C99Native usano `gmb_timeseries.c`: due implementazioni
// indipendenti della stessa formula. Qui il managed chiama la lib `gmb_ts_flat` (inoltro puro
// alle funzioni del runtime condiviso, come `gmb_matrix_flat` per i kernel matriciali): una
// sola fonte numerica, risultati identici sulle 4 gambe.
//
// OWNERSHIP. Le funzioni C allocano il buffer (calloc): qui si copia e si libera con `free`.
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
}
