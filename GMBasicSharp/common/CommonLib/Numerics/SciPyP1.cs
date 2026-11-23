// FILE: CommonLib.Numerics.SciPyP1.cs
// P1 scipy: signal (welch — resta builtin), interpolate
// (splrep/splev), fft (rfft/irfft). Reference: scipy 1.18.
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

    /// <summary>scipy.signal.savgol_filter — smoothing Savitzky-Golay (polinomio least-squares locale).</summary>

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

    /// <summary>scipy.signal.decimate — filtra passa-basso FIR + downsampling di q.</summary>

    // ========================================================================
    // scipy.interpolate: splrep / splev (cubic spline not-a-knot, k=3)
    // ========================================================================

    /// <summary>Rappresentazione spline cubica (scipy splrep).</summary>
    public sealed class CubicSplineRep
    {
        public double[] X { get; }
        public double[] Coeff { get; }
        /// <summary>Array combinato del runtime: [n, x…, coeff…] (usato da SPLEV).</summary>
        internal double[] Flat { get; }
        internal CubicSplineRep(double[] x, double[] c)
            : this(x, c, BuildFlat(x, c)) { }
        private CubicSplineRep(double[] x, double[] c, double[] flat)
        {
            X = x;
            Coeff = c;
            Flat = flat;
        }

        private static double[] BuildFlat(double[] x, double[] c)
        {
            var n = x.Length;
            var flat = new double[1 + n + c.Length];
            flat[0] = n;
            for (var i = 0; i < n; i++) flat[1 + i] = x[i];
            for (var i = 0; i < c.Length; i++) flat[1 + n + i] = c[i];
            return flat;
        }
    }

    /// <summary>scipy.interpolate.splrep — spline cubica interpolante (naturale, k=3).
    /// RESTA MANAGED: il kernel C `gmb_splrep` costruisce un sistema DENSE n×n, mentre qui il
    /// not-a-knot è risolto con Thomas O(n) (un input da 200k punti in C richiederebbe 320 GB).
    /// Migrarlo sarebbe una PERDITA computazionale: prima va portato il Thomas nel kernel.</summary>
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

    /// <summary>scipy.interpolate.splev — valuta la spline sui punti xq (kernel C `gmb_splev`).</summary>
    public static double[] Splev(CubicSplineRep spline, double[] xq)
    {
        // n < 3 è una rappresentazione degenere che il kernel non copre (assume n >= 3):
        // stessa formula inline, così il comportamento resta identico a prima.
        if (spline.X.Length < 3) return SplevSmall(spline, xq);
        return InterpolationNative.Splev(spline.Flat, xq);
    }

    private static double[] SplevSmall(CubicSplineRep spline, double[] xq)
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
            var dx = t - x[i];
            result[k] = c[4 * i] + dx * (c[4 * i + 1] + dx * (c[4 * i + 2] + dx * c[4 * i + 3]));
        }
        return result;
    }


    // ========================================================================
    // scipy.fft: rfft / irfft
    // ========================================================================

    /// <summary>scipy.fft.rfft — FFT reale (componenti a frequenza non negativa).
    /// Il kernel C (`gmb_rfft`) fa la FFT NON scalata come scipy (nessuna perdita).</summary>
    public static object[,] Rfft(double[] x)
    {
        var interleaved = AnalysisNative.Rfft(x);
        var m = interleaved.Length / 2;
        var result = new object[m, 2];
        for (var k = 0; k < m; k++)
        {
            result[k, 0] = interleaved[2 * k];
            result[k, 1] = interleaved[2 * k + 1];
        }
        return result;
    }

    /// <summary>scipy.fft.irfft — inversa reale: il kernel C (`gmb_irfft`) fa l'inversa NON
    /// scalata e divide per n, quindi il round-trip è esatto come in scipy.</summary>
    public static double[] Irfft(double[] re, double[] im)
    {
        return AnalysisNative.Irfft(re, im);
    }


    /// <summary>Risolve <c>a·x = b</c> con il solver del runtime (Gauss a pivot parziale).
    /// Ritorna <c>null</c> se la matrice è singolare (|pivot| &lt; 1e-14).</summary>
    private static double[]? SolveLinear(double[,] a, double[] b)
    {
        var n = b.Length;
        var flat = new double[n * n];
        for (var i = 0; i < n; i++)
            for (var j = 0; j < n; j++) flat[i * n + j] = a[i, j];
        var x = new double[n];
        return MatrixNativeKernels.SolveLinear(flat, b, x, 1e-14) ? x : null;
    }
}
