// FILE: CommonLib.Numerics.Signal.cs
// Nucleo scipy.signal per GMBasic (runtime C#): convoluzione/correlazione 1D e
// finestre classiche. Nessun duplicato: verificato che ANALYSIS/TIMESERIES non
// espongono convolve/correlate/finestre (solo FFT/IFFT).

using System;

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>Convoluzione 1D 'full' (scipy: np.convolve(x, k, 'full')).</summary>
    public static double[] SignalConvolve(double[] x, double[] k)
    {
        if (x == null || k == null || x.Length == 0 || k.Length == 0) return [];
        var n = x.Length + k.Length - 1;
        var r = new double[n];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < k.Length; j++)
            {
                var xi = i - j;
                if (xi >= 0 && xi < x.Length) r[i] += x[xi] * k[j];
            }
        return r;
    }

    /// <summary>Correlazione 1D 'full' (scipy: np.correlate(x, k, 'full')).</summary>
    public static double[] SignalCorrelate(double[] x, double[] k)
    {
        if (x == null || k == null || x.Length == 0 || k.Length == 0) return [];
        var n = x.Length + k.Length - 1;
        var r = new double[n];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < k.Length; j++)
            {
                var xi = i - (k.Length - 1) + j;
                if (xi >= 0 && xi < x.Length) r[i] += x[xi] * k[j];
            }
        return r;
    }

    /// <summary>Finestra di Hann simmetrica (scipy.signal.windows.hann). w(n)=0.5(1−cos(2πn/(N−1))).</summary>
    public static double[] SignalHann(int n)
    {
        if (n <= 0) return [];
        var r = new double[n];
        if (n == 1) { r[0] = 1.0; return r; }
        for (var i = 0; i < n; i++)
            r[i] = 0.5 * (1.0 - Math.Cos(2.0 * Math.PI * i / (n - 1)));
        return r;
    }

    /// <summary>Finestra di Hamming simmetrica (scipy.signal.windows.hamming). w(n)=0.54−0.46cos(2πn/(N−1)).</summary>
    public static double[] SignalHamming(int n)
    {
        if (n <= 0) return [];
        var r = new double[n];
        if (n == 1) { r[0] = 1.0; return r; }
        for (var i = 0; i < n; i++)
            r[i] = 0.54 - 0.46 * Math.Cos(2.0 * Math.PI * i / (n - 1));
        return r;
    }

    /// <summary>Finestra di Blackman simmetrica (scipy.signal.windows.blackman).
    /// w(n)=0.42−0.5cos(2πn/(N−1))+0.08cos(4πn/(N−1)).</summary>
    public static double[] SignalBlackman(int n)
    {
        if (n <= 0) return [];
        var r = new double[n];
        if (n == 1) { r[0] = 1.0; return r; }
        var t = 2.0 * Math.PI / (n - 1);
        for (var i = 0; i < n; i++)
            r[i] = 0.42 - 0.5 * Math.Cos(t * i) + 0.08 * Math.Cos(2.0 * t * i);
        return r;
    }
    /// <summary>scipy.signal.firwin — design FIR a finestra (lowpass/highpass/bandpass/bandstop).
    /// Cutoff normalizzati (0..0.5, Nyquist=0.5), finestra hann/hamming/blackman/rect.</summary>
    public static double[] Firwin(int numtaps, double[] cutoff, string btype, string window)
    {
        if (numtaps < 1) return Array.Empty<double>();
        var m = (numtaps - 1) / 2.0;
        var taps = new double[numtaps];
        var win = SignalWindow(numtaps, window);
        var type = btype.ToLowerInvariant();
        for (var n = 0; n < numtaps; n++)
        {
            var x = n - m;
            var h = 0.0;
            if (type == "highpass")
            {
                // δ[n-M] − lowpass(fc)
                h = x == 0 ? 1.0 : 0.0;
                h -= IdealLowpass(cutoff[0], x);
            }
            else if (type is "bandpass" or "bandpass" or "band")
            {
                if (cutoff.Length >= 2)
                    h = IdealLowpass(cutoff[1], x) - IdealLowpass(cutoff[0], x);
                else
                    h = IdealLowpass(cutoff[0], x);
            }
            else if (type is "bandstop" or "stop")
            {
                if (cutoff.Length >= 2)
                    h = (x == 0 ? 1.0 : 0.0) - (IdealLowpass(cutoff[1], x) - IdealLowpass(cutoff[0], x));
                else
                    h = (x == 0 ? 1.0 : 0.0) - IdealLowpass(cutoff[0], x);
            }
            else
            {
                h = IdealLowpass(cutoff[0], x); // lowpass
            }
            taps[n] = h * win[n];
        }
        return taps;
    }

    private static double IdealLowpass(double fc, double x)
        => x == 0 ? 2 * fc : 2 * fc * Math.Sin(2 * Math.PI * fc * x) / (2 * Math.PI * fc * x);

    private static double[] SignalWindow(int n, string window)
    {
        switch (window.ToLowerInvariant())
        {
            case "hamming": return SignalHamming(n);
            case "blackman": return SignalBlackman(n);
            case "rect" or "boxcar": return Enumerable.Repeat(1.0, n).ToArray();
            default: return SignalHann(n); // hann default (scipy default)
        }
    }

    /// <summary>scipy.signal.find_peaks — picchi locali con distanza minima.</summary>
    public static double[] FindPeaks(double[] x, double distance)
    {
        var n = x.Length;
        var idx = new System.Collections.Generic.List<double>();
        var heights = new System.Collections.Generic.List<double>();
        for (var i = 1; i < n - 1; i++)
        {
            if (x[i] >= x[i - 1] && x[i] >= x[i + 1])
            {
                // escludi i punti interni di un plateau (il primo è il picco)
                if (i > 1 && x[i] == x[i - 1] && x[i - 1] == x[i - 2]) continue;
                idx.Add(i);
                heights.Add(x[i]);
            }
        }
        var d = Math.Max(1.0, distance);
        var order = Enumerable.Range(0, idx.Count).OrderByDescending(k => heights[k]).ToArray();
        var kept = new System.Collections.Generic.List<double>();
        foreach (var k in order)
        {
            var ok = true;
            foreach (var pk in kept)
                if (Math.Abs(pk - idx[k]) < d) { ok = false; break; }
            if (ok) kept.Add(idx[k]);
        }
        kept.Sort();
        return kept.ToArray();
    }
}
