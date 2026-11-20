// FILE: CommonLib.Numerics.SignalFiltersNative.cs
// FILTRI IIR: le gambe MANAGED (IL/PCodeVM) chiamano il runtime C via P/Invoke.
//
// PERCHÉ. `SignalFilters.cs` implementava in C# design (prototipo analogico + trasformazioni di
// banda + bilineare) e applicazione (lfilter/filtfilt/freqz) degli stessi filtri che
// `gmb_filters.c` implementa per VM/C e C99Native. Qui il managed usa la lib `gmb_filter_flat`
// (inoltro puro): una sola sorgente numerica.
//
// HANDLE. Il runtime tiene una tabella di filtri: il design ritorna un handle, `FromBa` lo crea
// da coefficienti già calcolati, `Free` rilascia lo slot (prima le wrapper FILTER.* consumavano
// uno slot per chiamata, per sempre).
//
// OWNERSHIP. I buffer di lfilter/filtfilt/freqz sono allocati dal runtime: `Take` copia e libera.

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

internal static class SignalFiltersNative
{
    private const string Lib = "gmb_filter_flat";

    [DllImport(Lib, EntryPoint = "gmb_flf_butter", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long Butter(long n, double[] wn, int nwn, [MarshalAs(UnmanagedType.LPStr)] string btype);
    [DllImport(Lib, EntryPoint = "gmb_flf_cheby1", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long Cheby1(long n, double rp, double[] wn, int nwn, [MarshalAs(UnmanagedType.LPStr)] string btype);
    [DllImport(Lib, EntryPoint = "gmb_flf_cheby2", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long Cheby2(long n, double rs, double[] wn, int nwn, [MarshalAs(UnmanagedType.LPStr)] string btype);
    [DllImport(Lib, EntryPoint = "gmb_flf_ellip", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long Ellip(long n, double rp, double rs, double[] wn, int nwn, [MarshalAs(UnmanagedType.LPStr)] string btype);
    [DllImport(Lib, EntryPoint = "gmb_flf_lfilter", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr LFilter(long h, double[] x, int nx, out long outn);
    [DllImport(Lib, EntryPoint = "gmb_flf_filtfilt", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr FiltFilt(long h, double[] x, int nx, out long outn);
    [DllImport(Lib, EntryPoint = "gmb_flf_freqz", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr Freqz(long h, int npts, out long outn);
    [DllImport(Lib, EntryPoint = "gmb_flf_from_ba", CallingConvention = CallingConvention.Cdecl)]
    internal static extern long FromBa(double[] b, int nb, double[] a, int na);
    [DllImport(Lib, EntryPoint = "gmb_flf_taps", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Taps(long h, out int nb, out int na);
    [DllImport(Lib, EntryPoint = "gmb_flf_coeffs", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int Coeffs(long h, double[] b, int nb, double[] a, int na);
    [DllImport(Lib, EntryPoint = "gmb_flf_free", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void Free(long h);
    [DllImport(Lib, EntryPoint = "gmb_flf_ellipk", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double EllipticK(double m);
    [DllImport(Lib, EntryPoint = "gmb_flf_ellip_sn", CallingConvention = CallingConvention.Cdecl)]
    internal static extern double EllipSn(double u, double m);
    [DllImport(Lib, EntryPoint = "gmb_flf_ellip_scndn", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void EllipSnCnDn(double u, double m, out double sn, out double cn, out double dn);

    /* i buffer dei risultati sono allocati dal runtime: vanno liberati con `free` */
    [DllImport("libc", EntryPoint = "free", CallingConvention = CallingConvention.Cdecl)]
    private static extern void libc_free(IntPtr p);

    /* overload comodi: la lunghezza è quella dell'array */
    internal static long FromBa(double[] b, double[] a) => FromBa(b, b.Length, a, a.Length);
    internal static IntPtr LFilter(long h, double[] x, out long outn) => LFilter(h, x, x.Length, out outn);
    internal static IntPtr FiltFilt(long h, double[] x, out long outn) => FiltFilt(h, x, x.Length, out outn);

    internal static double[] Take(IntPtr p, long len, string what)
    {
        try
        {
            if (p == IntPtr.Zero)
                throw new InvalidOperationException("SIGNAL: il kernel C non ha prodotto " + what + ".");
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
}
