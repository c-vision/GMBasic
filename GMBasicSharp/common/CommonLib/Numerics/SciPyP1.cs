// FILE: CommonLib.Numerics.SciPyP1.cs
// P1 scipy: signal (welch — resta builtin, ritorna un handle di matrice).
// splrep/splev e rfft/irfft sono stati spostati nei pacchetti FFI (2026-11-23):
// `INTERP` nel pacchetto `math`, `SIGNAL` nel pacchetto `signal` (fuori dalla superficie canonica).
// NOTA: spatial.distance/ConvexHull sono migrati a shared/libs/spatial (FFI).

namespace CommonLib;

public static partial class Numerics
{
    // ========================================================================
    // scipy.signal: welch, savgol_filter, resample, decimate
    // ========================================================================

    /// <summary>Finestra di Hann simmetrica (0.5-0.5·cos(2πi/(n-1))), usata da <see cref="Welch"/>.
    /// Vive qui perché la finestra del dominio SIGNAL è nella lib FFI `signal`; WELCH resta builtin
    /// (ritorna un handle di matrice) e non può dipendere dalla lib.</summary>
    internal static double[] WelchWindow(int n)
    {
        if (n <= 1) return new[] { 1.0 };
        var w = new double[n];
        for (var i = 0; i < n; i++) w[i] = 0.5 - 0.5 * Math.Cos(2.0 * Math.PI * i / (n - 1));
        return w;
    }

    /// <summary>scipy.signal.welch — densità spettrale (periodogramma di Welch).
    /// Ritorna matrice [nfreqs, 2]: col0 = frequenze, col1 = PSD.
    /// Il kernel C (`gmb_welch`) usa la STESSA finestra di Hann e la stessa FFT (nessuna perdita).</summary>
    public static double[,] Welch(double[] x, int nperseg, double fs)
    {
        var n = x.Length;
        if (nperseg <= 0) nperseg = Math.Min(256, Math.Max(8, n));
        if (nperseg > n) nperseg = n;
        return AnalysisNative.Welch(x, nperseg, fs);
    }
}
