// FILE: CommonLib.Numerics.SignalFilters.cs
// Design e applicazione di filtri IIR (scipy.signal): le formule vivono nel runtime C
// (`gmb_filters.c`) e qui si chiamano via P/Invoke (`gmb_filter_flat`), così le 4 gambe
// usano la stessa implementazione. Questo file conserva solo l'API managed e la gestione
// dell'handle.

namespace CommonLib;

/// <summary>
/// Filtro IIR in forma (b, a) — risultato del design scipy.signal.
/// Il calcolo vero e proprio usa l'handle del runtime C: la classe lo crea alla prima
/// applicazione (o al design) e lo rilascia in <see cref="Dispose"/> / nel finalizzatore.
/// </summary>
public sealed class SignalFilter : IDisposable
{
    public double[] B { get; init; } = [1.0];
    public double[] A { get; init; } = [1.0];

    /// <summary>Handle nella tabella del runtime (0 = non ancora registrato).</summary>
    internal long Handle { get; set; }

    /// <summary>Handle del runtime, registrando B/A alla prima richiesta.</summary>
    internal long EnsureHandle()
    {
        if (Handle > 0) return Handle;
        Handle = SignalFiltersNative.FromBa(B, A);
        if (Handle <= 0)
            throw new InvalidOperationException(
                "SIGNAL: impossibile registrare il filtro nel runtime (coefficienti non validi o tabella piena).");
        return Handle;
    }

    public void Dispose()
    {
        if (Handle > 0)
        {
            SignalFiltersNative.Free(Handle);
            Handle = 0;
        }
        GC.SuppressFinalize(this);
    }

    ~SignalFilter()
    {
        try { Dispose(); } catch { /* nel finalizzatore non si propaga nulla */ }
    }
}

/// <summary>
/// Design e applicazione di filtri IIR con parità a scipy.signal (iirfilter):
/// Butterworth, Chebyshev I/II e ellittico, più lfilter (direct form II transposed),
/// filtfilt (zero-phase, padding odd) e freqz (risposta in ampiezza).
/// </summary>
public static partial class Numerics
{
    // ============================ design (parity scipy) ============================

    public static SignalFilter Butter(int n, double[] wn, string btype)
        => FromNative(SignalFiltersNative.Butter(n, wn, wn.Length, btype));

    public static SignalFilter Cheby1(int n, double rp, double[] wn, string btype)
        => FromNative(SignalFiltersNative.Cheby1(n, rp, wn, wn.Length, btype));

    public static SignalFilter Cheby2(int n, double rs, double[] wn, string btype)
        => FromNative(SignalFiltersNative.Cheby2(n, rs, wn, wn.Length, btype));

    public static SignalFilter Ellip(int n, double rp, double rs, double[] wn, string btype)
        => FromNative(SignalFiltersNative.Ellip(n, rp, rs, wn, wn.Length, btype));

    /// <summary>Costruisce il SignalFilter leggendo i coefficienti dal registro del runtime.</summary>
    private static SignalFilter FromNative(long handle)
    {
        if (handle <= 0)
            throw new InvalidOperationException("SIGNAL: design del filtro fallito (parametri non validi).");
        if (SignalFiltersNative.Taps(handle, out var nb, out var na) == 0)
        {
            SignalFiltersNative.Free(handle);
            throw new InvalidOperationException("SIGNAL: handle del filtro non valido.");
        }
        var b = new double[nb];
        var a = new double[na];
        if (SignalFiltersNative.Coeffs(handle, b, nb, a, na) == 0)
        {
            SignalFiltersNative.Free(handle);
            throw new InvalidOperationException("SIGNAL: lettura dei coefficienti fallita.");
        }
        return new SignalFilter { B = b, A = a, Handle = handle };
    }

    // ============================ elliptic functions ============================

    /// <summary>Integrale ellittico completo K(m) (scipy special.ellipk) via AGM.</summary>
    public static double EllipticK(double m) => SignalFiltersNative.EllipticK(m);

    public static double EllipSn(double u, double m) => SignalFiltersNative.EllipSn(u, m);

    /// <summary>sn/cn/dn di Jacobi via trasformazione di Landen discendente (scipy
    /// special.ellipj per u reale in [0,K]); cn/dn dalle relazioni cn²=1-sn², dn²=1-m·sn².</summary>
    public static void EllipSnCnDn(double u, double m, out double sn, out double cn, out double dn, out double ph)
    {
        SignalFiltersNative.EllipSnCnDn(u, m, out sn, out cn, out dn);
        ph = Math.Asin(sn);
    }

    // ============================ applicazione ============================

    /// <summary>Filtro causale direct form II transposed (scipy lfilter, ba).</summary>
    public static double[] LFilter(SignalFilter f, double[] x)
    {
        var p = SignalFiltersNative.LFilter(f.EnsureHandle(), x, out var n);
        return SignalFiltersNative.Take(p, n, "LFILTER");
    }

    /// <summary>Filtraggio zero-phase (scipy filtfilt, method='pad').</summary>
    public static double[] FiltFilt(SignalFilter f, double[] x)
    {
        if (x.Length == 0) return Array.Empty<double>();
        var p = SignalFiltersNative.FiltFilt(f.EnsureHandle(), x, out var n);
        return SignalFiltersNative.Take(p, n, "FILTFILT");
    }

    /// <summary>Risposta in ampiezza |H(e^{jw})| a n punti w=0..π (endpoint escluso).</summary>
    public static double[] Freqz(SignalFilter f, int n)
    {
        var p = SignalFiltersNative.Freqz(f.EnsureHandle(), n, out var len);
        return SignalFiltersNative.Take(p, len, "FREQZ");
    }
}
