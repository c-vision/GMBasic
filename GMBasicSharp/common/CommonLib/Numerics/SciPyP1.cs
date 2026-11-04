// FILE: CommonLib.Numerics.SciPyP1.cs
// P1 scipy: signal (welch, savgol_filter, resample, decimate), interpolate
// (splrep/splev), spatial.distance (pdist, cdist, distance_matrix), fft
// (rfft/irfft), spatial (ConvexHull). Reference: scipy 1.18.

using System.Numerics;
using MathNet.Numerics;
using MathNet.Numerics.IntegralTransforms;
using MathNet.Numerics.LinearAlgebra;

namespace CommonLib;

public static partial class Numerics
{
    // ========================================================================
    // scipy.signal: welch, savgol_filter, resample, decimate
    // ========================================================================

    /// <summary>scipy.signal.welch — densità spettrale (periodogramma di Welch).
    /// Ritorna matrice [nfreqs, 2]: col0 = frequenze, col1 = PSD.</summary>
    public static double[,] Welch(double[] x, int nperseg, double fs)
    {
        var n = x.Length;
        if (nperseg <= 0) nperseg = Math.Min(256, Math.Max(8, n));
        if (nperseg > n) nperseg = n;
        var noverlap = nperseg / 2;
        var step = nperseg - noverlap;
        var nSeg = Math.Max(1, (n - noverlap) / step);
        var win = SignalHann(nperseg);
        var winSum2 = 0.0;
        foreach (var w in win) winSum2 += w * w;
        var nfreqs = nperseg / 2 + 1;
        var psd = new double[nfreqs];
        var scale = 1.0 / (fs * (winSum2 > 0 ? winSum2 : 1.0));
        for (var s = 0; s < nSeg; s++)
        {
            var start = s * step;
            var c = new Complex[nperseg];
            for (var i = 0; i < nperseg; i++)
                c[i] = new Complex(x[start + i] * win[i], 0);
            Fourier.Forward(c, FourierOptions.Default);
            for (var k = 0; k < nfreqs; k++)
            {
                var m = c[k].Magnitude;
                psd[k] += m * m * scale;
            }
        }
        for (var k = 0; k < nfreqs; k++) psd[k] /= nSeg;
        var result = new double[nfreqs, 2];
        for (var k = 0; k < nfreqs; k++)
        {
            result[k, 0] = k * fs / nperseg;
            result[k, 1] = psd[k];
        }
        return result;
    }

    /// <summary>scipy.signal.savgol_filter — smoothing Savitzky-Golay (polinomio least-squares locale).</summary>
    public static double[] SavgolFilter(double[] x, int windowLength, int polyorder)
    {
        var n = x.Length;
        if (windowLength > n) windowLength = n;
        if (windowLength % 2 == 0) windowLength++;
        if (windowLength < 3) return (double[])x.Clone();
        var half = (windowLength - 1) / 2;
        var coeffs = SavgolCoeffs(half, polyorder);
        // estende i bordi per estrapolazione polinomiale (scipy mode='interp')
        var ext = new double[n + 2 * half];
        var pl = PolyFit(x, 0, Math.Min(windowLength, n), polyorder);
        var pr = PolyFit(x, Math.Max(0, n - windowLength), n, polyorder);
        var wl = Math.Min(windowLength, n);
        for (var i = 0; i < half; i++) ext[i] = EvalPoly(pl, i - half);
        for (var i = 0; i < n; i++) ext[half + i] = x[i];
        for (var i = 0; i < half; i++) ext[half + n + i] = EvalPoly(pr, wl + i);
        var result = new double[n];
        for (var i = 0; i < n; i++)
        {
            double s = 0;
            for (var j = 0; j < windowLength; j++) s += coeffs[j] * ext[i + j];
            result[i] = s;
        }
        return result;
    }

    private static double[] PolyFit(double[] x, int start, int end, int order)
    {
        var len = end - start;
        var ata = new double[order + 1, order + 1];
        var atb = new double[order + 1];
        for (var i = 0; i < len; i++)
        {
            for (var p = 0; p <= order; p++)
            {
                atb[p] += Math.Pow(i, p) * x[start + i];
                for (var q = 0; q <= order; q++)
                    ata[p, q] += Math.Pow(i, p + q);
            }
        }
        return SolveLinear(ata, atb) ?? new double[order + 1];
    }

    private static double EvalPoly(double[] coeff, double t)
    {
        double s = 0;
        for (var p = coeff.Length - 1; p >= 0; p--) s = s * t + coeff[p];
        return s;
    }

    private static double[] SavgolCoeffs(int half, int polyorder)
    {
        var w = 2 * half + 1;
        // matrice A: colonne = potenze di (j), righe = j in [-half, half]
        var A = new double[w, polyorder + 1];
        for (var j = -half; j <= half; j++)
            for (var p = 0; p <= polyorder; p++)
                A[j + half, p] = Math.Pow(j, p);
        // At·A
        var ata = new double[polyorder + 1, polyorder + 1];
        var atb = new double[polyorder + 1];
        for (var r = 0; r <= polyorder; r++)
            for (var c = 0; c <= polyorder; c++)
            {
                double s = 0;
                for (var j = 0; j < w; j++) s += A[j, r] * A[j, c];
                ata[r, c] = s;
            }
        // b: vettore con 1 al centro (derivata 0) → atb[r] = Σ_j A[j,r]*δ(j=0) = A[half, r]
        for (var r = 0; r <= polyorder; r++) atb[r] = A[half, r];
        var b = SolveLinear(ata, atb) ?? new double[polyorder + 1];
        var coeffs = new double[w];
        for (var j = 0; j < w; j++)
        {
            double s = 0;
            for (var p = 0; p <= polyorder; p++) s += A[j, p] * b[p];
            coeffs[j] = s;
        }
        return coeffs;
    }

    /// <summary>scipy.signal.resample — ricampionamento via FFT (zero-pad/truncate nello spettro).</summary>
    public static double[] Resample(double[] x, int num)
    {
        var n = x.Length;
        if (n == 0 || num <= 0) return Array.Empty<double>();
        if (num == n) return (double[])x.Clone();
        var c = new Complex[n];
        for (var i = 0; i < n; i++) c[i] = new Complex(x[i], 0);
        Fourier.Forward(c, FourierOptions.NoScaling);
        var outN = num;
        var spec = new Complex[outN];
        var halfN = n / 2;
        var keep = Math.Min(halfN, outN / 2);
        spec[0] = c[0];
        for (var k = 1; k < keep; k++) spec[k] = c[k];
        if (outN % 2 == 0 && outN / 2 < n)
            spec[outN / 2] = c[Math.Min(outN / 2, n - 1)]; /* Nyquist */
        for (var k = 1; k < keep; k++)
            if (outN - k >= 0 && n - k >= 0) spec[outN - k] = c[n - k];
        Fourier.Inverse(spec, FourierOptions.NoScaling);
        var result = new double[outN];
        for (var i = 0; i < outN; i++) result[i] = spec[i].Real / n;
        return result;
    }

    /// <summary>scipy.signal.decimate — filtra passa-basso FIR + downsampling di q.</summary>
    public static double[] Decimate(double[] x, int q)
    {
        if (q <= 1) return (double[])x.Clone();
        var n = x.Length;
        var fc = 0.8 / q;
        // FIR lowpass (taps dispari, finestra hann)
        var nt = Math.Max(3, 2 * (int)Math.Ceiling(4.0 / fc) + 1);
        if (nt > n) nt = n % 2 == 0 ? n - 1 : n;
        var cutoff = new[] { fc };
        var taps = Firwin(nt, cutoff, "lowpass", "hann");
        var smoothed = new double[n];
        var half = nt / 2;
        for (var i = 0; i < n; i++)
        {
            double s = 0;
            for (var j = 0; j < nt; j++)
            {
                var idx = i + j - half;
                if (idx >= 0 && idx < n) s += taps[j] * x[idx];
            }
            smoothed[i] = s;
        }
        var m = (n + q - 1) / q;
        var result = new double[m];
        for (var i = 0; i < m; i++) result[i] = smoothed[i * q];
        return result;
    }

    // ========================================================================
    // scipy.interpolate: splrep / splev (cubic spline not-a-knot, k=3)
    // ========================================================================

    /// <summary>Rappresentazione spline cubica (scipy splrep).</summary>
    public sealed class CubicSplineRep
    {
        public double[] X { get; }
        public double[] Coeff { get; }
        internal CubicSplineRep(double[] x, double[] c) { X = x; Coeff = c; }
    }

    /// <summary>scipy.interpolate.splrep — spline cubica interpolante (naturale, k=3).</summary>
    public static CubicSplineRep Splrep(double[] x, double[] y)
    {
        var n = x.Length;
        if (n < 3) return new CubicSplineRep((double[])x.Clone(), (double[])y.Clone());
        var h = new double[n - 1];
        for (var i = 0; i < n - 1; i++) h[i] = x[i + 1] - x[i];
        // c_i (seconda derivata / 2): sistema not-a-knot risolto con Thomas O(n).
        // La vecchia costruzione densa double[n,n] richiedeva O(n^2) memoria (esplodeva
        // su input grandi: 200k punti = 320 GB) — bug pre-esistente esposto dal parking.
        var c = n < 4 ? SolveNotAKnotDense(x, y, h) : SolveNotAKnotTridiagonal(y, h);
        var coeff = new double[4 * (n - 1)];
        for (var i = 0; i < n - 1; i++)
        {
            coeff[4 * i] = y[i];
            coeff[4 * i + 1] = (y[i + 1] - y[i]) / h[i] - h[i] * (2 * c[i] + c[i + 1]) / 3.0;
            coeff[4 * i + 2] = c[i];
            coeff[4 * i + 3] = (c[i + 1] - c[i]) / (3.0 * h[i]);
        }
        return new CubicSplineRep((double[])x.Clone(), coeff);
    }

    private static double[] SolveNotAKnotDense(double[] x, double[] y, double[] h)
    {
        var n = x.Length;
        var A = new double[n, n];
        var b = new double[n];
        A[0, 0] = h[1];
        A[0, 1] = -(h[0] + h[1]);
        A[0, 2] = h[0];
        for (var i = 1; i < n - 1; i++)
        {
            A[i, i - 1] = h[i - 1];
            A[i, i] = 2 * (h[i - 1] + h[i]);
            A[i, i + 1] = h[i];
            b[i] = 3 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);
        }
        A[n - 1, n - 3] = h[n - 2];
        A[n - 1, n - 2] = -(h[n - 3] + h[n - 2]);
        A[n - 1, n - 1] = h[n - 3];
        return SolveLinear(A, b) ?? new double[n];
    }

    private static double[] SolveNotAKnotTridiagonal(double[] y, double[] h)
    {
        var n = y.Length;
        for (var i = 0; i < n - 1; i++)
            if (h[i] == 0) return new double[n]; // x non strettamente crescente: fallback come il denso singolare
        var m = n - 2; // incognite c_1..c_{n-2}
        var b = new double[n];
        for (var i = 1; i < n - 1; i++)
            b[i] = 3 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);

        var lo = new double[m];
        var diag = new double[m];
        var up = new double[m];
        var rhs = new double[m];

        diag[0] = h[0] * (h[0] + h[1]) / h[1] + 2 * (h[0] + h[1]);
        up[0] = h[1] - h[0] * h[0] / h[1];
        rhs[0] = b[1];
        for (var i = 1; i < m - 1; i++)
        {
            var k = i + 1;
            lo[i] = h[k - 1];
            diag[i] = 2 * (h[k - 1] + h[k]);
            up[i] = h[k];
            rhs[i] = b[k];
        }
        var last = m - 1;
        lo[last] = h[n - 3] - h[n - 2] * h[n - 2] / h[n - 3];
        diag[last] = 2 * (h[n - 3] + h[n - 2]) + h[n - 2] * (h[n - 3] + h[n - 2]) / h[n - 3];
        rhs[last] = b[n - 2];

        var cprime = new double[m];
        var rprime = new double[m];
        cprime[0] = up[0] / diag[0];
        rprime[0] = rhs[0] / diag[0];
        for (var i = 1; i < m; i++)
        {
            var denom = diag[i] - lo[i] * cprime[i - 1];
            cprime[i] = up[i] / denom;
            rprime[i] = (rhs[i] - lo[i] * rprime[i - 1]) / denom;
        }

        var ci = new double[m];
        ci[m - 1] = rprime[m - 1];
        for (var i = m - 2; i >= 0; i--)
            ci[i] = rprime[i] - cprime[i] * ci[i + 1];

        var result = new double[n];
        for (var i = 0; i < m; i++) result[i + 1] = ci[i];
        result[0] = ((h[0] + h[1]) * result[1] - h[0] * result[2]) / h[1];
        result[n - 1] = ((h[n - 3] + h[n - 2]) * result[n - 2] - h[n - 2] * result[n - 3]) / h[n - 3];
        return result;
    }

    /// <summary>scipy.interpolate.splev — valuta la spline sui punti xq.</summary>
    public static double[] Splev(CubicSplineRep spline, double[] xq)
    {
        var x = spline.X;
        var c = spline.Coeff;
        var n = x.Length;
        var result = new double[xq.Length];
        for (var k = 0; k < xq.Length; k++)
        {
            var t = xq[k];
            if (t <= x[0]) { result[k] = c[0]; continue; }
            if (t >= x[n - 1]) { result[k] = c[4 * (n - 2)]; continue; }
            var i = 0;
            while (i < n - 2 && t > x[i + 1]) i++;
            var dx = t - x[i];
            result[k] = c[4 * i] + dx * (c[4 * i + 1] + dx * (c[4 * i + 2] + dx * c[4 * i + 3]));
        }
        return result;
    }

    // ========================================================================
    // scipy.spatial.distance: pdist, cdist, distance_matrix
    // ========================================================================

    private static double DistMetric(double[] a, double[] b, string metric, double p)
    {
        switch (metric.ToLowerInvariant())
        {
            case "cityblock" or "manhattan":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s += Math.Abs(a[i] - b[i]);
                return s;
            }
            case "chebyshev":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s = Math.Max(s, Math.Abs(a[i] - b[i]));
                return s;
            }
            case "cosine":
            {
                double ab = 0, aa = 0, bb = 0;
                for (var i = 0; i < a.Length; i++) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
                var denom = Math.Sqrt(aa * bb);
                return denom > 0 ? 1.0 - ab / denom : 0;
            }
            case "minkowski":
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) s += Math.Pow(Math.Abs(a[i] - b[i]), p);
                return Math.Pow(s, 1.0 / p);
            }
            default:
            {
                double s = 0;
                for (var i = 0; i < a.Length; i++) { var d = a[i] - b[i]; s += d * d; }
                return Math.Sqrt(s);
            }
        }
    }

    /// <summary>scipy.spatial.distance.pdist — distanze a coppie condensate (triangolo superiore, i&lt;j).</summary>
    public static double[] Pdist(double[][] X, string metric, double p)
    {
        var n = X.Length;
        var m = n * (n - 1) / 2;
        var result = new double[m];
        var k = 0;
        for (var i = 0; i < n; i++)
            for (var j = i + 1; j < n; j++)
                result[k++] = DistMetric(X[i], X[j], metric, p);
        return result;
    }

    /// <summary>scipy.spatial.distance.cdist — distanze tra ogni riga di X e ogni riga di Y.</summary>
    public static double[,] Cdist(double[][] X, double[][] Y, string metric, double p)
    {
        var result = new double[X.Length, Y.Length];
        for (var i = 0; i < X.Length; i++)
            for (var j = 0; j < Y.Length; j++)
                result[i, j] = DistMetric(X[i], Y[j], metric, p);
        return result;
    }

    /// <summary>scipy.spatial.distance_matrix — matrice di distanze euclidee.</summary>
    public static double[,] DistanceMatrix(double[][] X, double[][] Y)
    {
        var result = new double[X.Length, Y.Length];
        for (var i = 0; i < X.Length; i++)
            for (var j = 0; j < Y.Length; j++)
            {
                double s = 0;
                for (var k = 0; k < X[i].Length; k++)
                {
                    var d = X[i][k] - Y[j][k];
                    s += d * d;
                }
                result[i, j] = Math.Sqrt(s);
            }
        return result;
    }

    // ========================================================================
    // scipy.fft: rfft / irfft
    // ========================================================================

    /// <summary>scipy.fft.rfft — FFT reale (componenti a frequenza non negativa).</summary>
    public static object[,] Rfft(double[] x)
    {
        var n = x.Length;
        var c = new Complex[n];
        for (var i = 0; i < n; i++) c[i] = new Complex(x[i], 0);
        // scipy.fft.rfft: FFT NON scalata (X[k]=Σx[j]e^{-...}); MathNet Default usa 1/√N.
        Fourier.Forward(c, FourierOptions.NoScaling);
        var m = n / 2 + 1;
        var result = new object[m, 2];
        for (var k = 0; k < m; k++)
        {
            result[k, 0] = c[k].Real;
            result[k, 1] = c[k].Imaginary;
        }
        return result;
    }

    /// <summary>scipy.fft.irfft — inversa reale: ricostruisce il segnale reale dalle componenti non negative.</summary>
    public static double[] Irfft(double[] re, double[] im)
    {
        var m = re.Length;
        var n = 2 * (m - 1);
        var c = new Complex[n];
        for (var k = 0; k < m; k++) c[k] = new Complex(re[k], im[k]);
        for (var k = 1; k < m - 1; k++) c[n - k] = new Complex(re[k], -im[k]);
        // scipy.fft.irfft: inversa NON scalata con divisione per n (round-trip esatto).
        Fourier.Inverse(c, FourierOptions.NoScaling);
        var result = new double[n];
        for (var i = 0; i < n; i++) result[i] = c[i].Real / n;
        return result;
    }

    // ========================================================================
    // scipy.spatial: ConvexHull (Andrew's monotone chain)
    // ========================================================================

    /// <summary>scipy.spatial.ConvexHull — indici dei vertici dell'inviluppo convesso (2D).</summary>
    public static double[] ConvexHull(double[][] points)
    {
        var n = points.Length;
        var idx = Enumerable.Range(0, n).ToArray();
        Array.Sort(idx, (a, b) => points[a][0] != points[b][0] ? points[a][0].CompareTo(points[b][0]) : points[a][1].CompareTo(points[b][1]));
        double Cross(int o, int a, int b)
        {
            var (x1, y1) = (points[idx[a]][0] - points[idx[o]][0], points[idx[a]][1] - points[idx[o]][1]);
            var (x2, y2) = (points[idx[b]][0] - points[idx[o]][0], points[idx[b]][1] - points[idx[o]][1]);
            return x1 * y2 - y1 * x2;
        }
        var lower = new List<int>();
        for (var i = 0; i < n; i++)
        {
            while (lower.Count >= 2 && Cross(lower[^2], lower[^1], i) <= 0) lower.RemoveAt(lower.Count - 1);
            lower.Add(i);
        }
        var upper = new List<int>();
        for (var i = n - 1; i >= 0; i--)
        {
            while (upper.Count >= 2 && Cross(upper[^2], upper[^1], i) <= 0) upper.RemoveAt(upper.Count - 1);
            upper.Add(i);
        }
        lower.RemoveAt(lower.Count - 1);
        upper.RemoveAt(upper.Count - 1);
        lower.AddRange(upper);
        return lower.Select(k => (double)idx[k]).ToArray();
    }

    /// <summary>Risolve <c>a·x = b</c> con eliminazione di Gauss e pivot parziale.
    /// Ritorna <c>null</c> se la matrice è singolare (|pivot| &lt; 1e-14).</summary>
    private static double[]? SolveLinear(double[,] a, double[] b)
    {
        var n = b.Length;
        var m = new double[n, n + 1];
        for (var i = 0; i < n; i++)
        {
            for (var j = 0; j < n; j++) m[i, j] = a[i, j];
            m[i, n] = b[i];
        }
        for (var col = 0; col < n; col++)
        {
            var piv = col;
            for (var r = col + 1; r < n; r++)
                if (Math.Abs(m[r, col]) > Math.Abs(m[piv, col])) piv = r;
            if (Math.Abs(m[piv, col]) < 1e-14) return null;
            if (piv != col)
                for (var j = 0; j <= n; j++) (m[col, j], m[piv, j]) = (m[piv, j], m[col, j]);
            for (var r = col + 1; r < n; r++)
            {
                var f = m[r, col] / m[col, col];
                for (var j = col; j <= n; j++) m[r, j] -= f * m[col, j];
            }
        }
        var x = new double[n];
        for (var i = n - 1; i >= 0; i--)
        {
            double s = m[i, n];
            for (var j = i + 1; j < n; j++) s -= m[i, j] * x[j];
            x[i] = s / m[i, i];
        }
        return x;
    }
}
