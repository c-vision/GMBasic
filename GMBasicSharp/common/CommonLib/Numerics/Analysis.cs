// FILE: CommonLib.Numerics.Analysis.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.LinearAlgebra;
// ReSharper disable InconsistentNaming

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>
    /// Test di Dickey-Fuller Aumentato (ADF) per la stazionarietà di una serie temporale.
    /// Regressione OLS: Δy_t = α + γ·y_{t-1} + δ·Δy_{t-1} + ε_t (un lag di aumento fisso).
    /// H0: la serie ha una radice unitaria (non stazionaria). Uno statistic molto negativo e un
    /// p-value basso rigettano H0 (la serie è stazionaria).
    /// </summary>
    /// <param name="series">Serie temporale di input (almeno 5 punti).</param>
    /// <returns>Tupla <c>(statistic, pValue)</c> — lo statistic t di γ e il p-value approssimato
    /// (interpolazione sui valori critici di MacKinnon 1994, caso "costante, nessun trend" — non
    /// un calcolo esatto tramite superficie di risposta completa, ma un'approssimazione
    /// ragionevole e monotona per uso dimostrativo/didattico).</returns>
    /// <example>var (stat, p) = Numerics.AdfTest(series);</example>
    public static (double statistic, double pValue) AdfTest(double[] series)
    {
        var n = series.Length;
        if (n < 5) throw new ArgumentException("Serie troppo corta per il test ADF (minimo 5 punti).");

        // Δy_t per t = 1..n-1 (dy[i] = series[i+1] - series[i])
        var dy = new double[n - 1];
        for (var t = 1; t < n; t++) dy[t - 1] = series[t] - series[t - 1];

        // Righe di regressione: dy[t] = alpha + gamma*y[t-1] + delta*dy[t-1] + eps, per t = 2..n-1
        // (serve y[t-1] e dy[t-1] = y[t-1]-y[t-2], quindi la prima riga utile è t=2).
        var rows = n - 2;
        if (rows < 4) throw new ArgumentException("Serie troppo corta per il test ADF aumentato.");

        var A = Matrix<double>.Build.Dense(rows, 3);
        var b = Vector<double>.Build.Dense(rows);
        for (var i = 0; i < rows; i++)
        {
            var t = i + 2;
            A[i, 0] = 1.0;
            A[i, 1] = series[t - 1];
            A[i, 2] = dy[t - 2];
            b[i] = dy[t - 1];
        }

        var coeffs = A.Solve(b);
        var gamma = coeffs[1];

        var fitted = A * coeffs;
        var resid = b - fitted;
        var rss = resid.DotProduct(resid);
        var dof = rows - 3;
        var sigma2 = dof > 0 ? rss / dof : 0.0;

        var AtA = A.TransposeThisAndMultiply(A);
        var AtAInv = AtA.Inverse();
        var seGamma = Math.Sqrt(Math.Max(0.0, sigma2 * AtAInv[1, 1]));

        var statistic = seGamma > 0 ? gamma / seGamma : 0.0;
        return (statistic, AdfPValue(statistic));
    }

    /// <summary>
    /// Approssima il p-value ADF per interpolazione lineare fra punti di ancoraggio noti
    /// (valori critici standard di Dickey-Fuller/MacKinnon per il caso "costante, nessun
    /// trend", più due code asintotiche). Monotona, continua, non richiede la superficie di
    /// risposta completa di MacKinnon (1994) — sufficiente per un test dimostrativo.
    /// </summary>
    private static double AdfPValue(double tau)
    {
        var points = new (double Stat, double P)[]
        {
            (-4.5, 0.0001), (-3.43, 0.01), (-2.86, 0.05), (-2.57, 0.10),
            (-1.6, 0.30), (-0.5, 0.60), (0.5, 0.90), (2.0, 0.999),
        };
        if (tau <= points[0].Stat) return points[0].P;
        if (tau >= points[^1].Stat) return points[^1].P;
        for (var i = 0; i < points.Length - 1; i++)
        {
            if (tau < points[i].Stat || tau > points[i + 1].Stat) continue;
            var frac = (tau - points[i].Stat) / (points[i + 1].Stat - points[i].Stat);
            return points[i].P + frac * (points[i + 1].P - points[i].P);
        }
        return 0.5;
    }

    /// <summary>
    /// Clustering K-Means (algoritmo di Lloyd) su dati monodimensionali.
    /// </summary>
    /// <param name="data">Punti da raggruppare.</param>
    /// <param name="k">Numero di cluster desiderati.</param>
    /// <param name="maxIterations">Limite di iterazioni (default 100).</param>
    /// <param name="seed">Seme per l'inizializzazione casuale dei centroidi (opzionale,
    /// per riproducibilità nei test).</param>
    /// <returns>I <paramref name="k"/> centroidi finali, ordinati in modo crescente (solo per
    /// leggibilità/riproducibilità dell'output — l'algoritmo stesso non impone un ordine).</returns>
    /// <example>var centroids = Numerics.KMeansCluster(data, 2);</example>
    public static double[] KMeansCluster(double[] data, int k, int maxIterations = 100, int? seed = null)
    {
        if (k <= 0) throw new ArgumentException("k deve essere positivo.");
        if (data.Length < k) throw new ArgumentException("Numero di punti insufficiente per k cluster.");

        var rng = seed.HasValue ? new Random(seed.Value) : new Random();
        var centroids = Enumerable.Range(0, data.Length).OrderBy(_ => rng.Next()).Take(k)
            .Select(i => data[i]).ToArray();

        var assignments = new int[data.Length];
        for (var iter = 0; iter < maxIterations; iter++)
        {
            var changed = false;
            for (var i = 0; i < data.Length; i++)
            {
                var best = 0;
                var bestDist = Math.Abs(data[i] - centroids[0]);
                for (var c = 1; c < k; c++)
                {
                    var dist = Math.Abs(data[i] - centroids[c]);
                    if (dist >= bestDist) continue;
                    bestDist = dist;
                    best = c;
                }
                if (assignments[i] == best) continue;
                assignments[i] = best;
                changed = true;
            }

            var sums = new double[k];
            var counts = new int[k];
            for (var i = 0; i < data.Length; i++)
            {
                sums[assignments[i]] += data[i];
                counts[assignments[i]]++;
            }
            for (var c = 0; c < k; c++)
                if (counts[c] > 0) centroids[c] = sums[c] / counts[c];
                // Cluster rimasto vuoto: mantiene il vecchio centroide (evita NaN da 0/0).

            if (!changed) break;
        }

        Array.Sort(centroids);
        return centroids;
    }

    /// <summary>
    /// Fitting di un modello ARIMA(p,d,q) tramite il metodo in due passi di Hannan-Rissanen:
    /// (1) fit di un AR lungo sulla serie differenziata per stimare i residui (proxy delle
    /// innovazioni MA), (2) regressione OLS finale su p lag autoregressivi + q lag dei residui
    /// stimati al passo 1. Non è una stima a massima verosimiglianza esatta (richiederebbe un
    /// filtro di Kalman o un'ottimizzazione non lineare) ma un metodo standard, veloce e
    /// numericamente stabile — adeguato per un uso dimostrativo/didattico.
    /// </summary>
    /// <param name="series">Serie temporale di input.</param>
    /// <param name="p">Ordine autoregressivo (AR).</param>
    /// <param name="d">Ordine di differenziazione (I).</param>
    /// <param name="q">Ordine media mobile (MA).</param>
    /// <returns>Tupla <c>(arCoeffs, maCoeffs, aic, fitted)</c>: coefficienti AR, coefficienti MA,
    /// Akaike Information Criterion del fit, e i valori fitted sulla serie differenziata
    /// (lunghezza pari alla serie dopo le <paramref name="d"/> differenziazioni, zero dove non
    /// c'è ancora abbastanza storia per calcolare il fit).</returns>
    /// <example>var (ar, ma, aic, fitted) = Numerics.ArimaFit(series, 1, 1, 1);</example>
    public static (double[] arCoeffs, double[] maCoeffs, double aic, double[] fitted) ArimaFit(
        double[] series, int p, int d, int q)
    {
        // Implementazione ALLINEATA al runtime C condiviso (native/gmb_runtime_c/src/gmb_math.c
        // gmb_arima_fit): stessa algebra (equazioni normali + eliminazione di Gauss a pivot
        // parziale). Serve la PARITA' bit-exact tra IL/PCode e i backend C (vmc/c99/c99n).
        // NON MathNet: il suo QR LAPACK, su questo sistema rank-deficient (il design matrix
        // AR-long del segnale sinusoidale ha R[k,k] ~ 1e-15), dava un AIC diverso
        // (-135.75 vs -138.06) e non è replicabile bit-exact in C.
        if (series == null) series = Array.Empty<double>();
        var arCoeffs = new double[p];
        var maCoeffs = new double[q];
        var diff = (double[])series.Clone();
        int nd = diff.Length;
        for (var k = 0; k < d; k++)
        {
            if (nd < 2) return (arCoeffs, maCoeffs, 0.0, new double[0]);
            for (var i = 0; i < nd - 1; i++) diff[i] = diff[i + 1] - diff[i];
            nd--;
        }
        var fitted = new double[nd];
        if (nd < p + q + 5) return (arCoeffs, maCoeffs, 0.0, fitted);

        var residuals = new double[nd];
        var m = p + q + 2; if (m < 8) m = 8; if (m > nd / 3) m = nd / 3;
        if (m > 0 && nd > m + 1)
        {
            var rn = nd - m;
            var ata = new double[m * m];
            var atb = new double[m];
            for (var i = 0; i < rn; i++)
            {
                var t = i + m;
                for (var lag = 0; lag < m; lag++)
                {
                    var col = diff[t - 1 - lag];
                    atb[lag] += col * diff[t];
                    for (var lag2 = 0; lag2 < m; lag2++) ata[lag * m + lag2] += col * diff[t - 1 - lag2];
                }
            }
            var aug = new double[m * (m + 1)];
            for (var r = 0; r < m; r++)
            {
                for (var c = 0; c < m; c++) aug[r * (m + 1) + c] = ata[r * m + c];
                aug[r * (m + 1) + m] = atb[r];
            }
            var arLong = GaussSolve(aug, m);
            if (arLong != null)
                for (var i = 0; i < rn; i++)
                {
                    var t = i + m;
                    var pred = 0.0;
                    for (var lag = 0; lag < m; lag++) pred += arLong[lag] * diff[t - 1 - lag];
                    residuals[t] = diff[t] - pred;
                }
        }

        var start = p; if (q > start) start = q; if (m > start) start = m;
        var rows = nd - start;
        if (rows < p + q + 1) return (arCoeffs, maCoeffs, 0.0, fitted);
        var cols = p + q;
        var rss = 0.0;
        if (cols == 0)
        {
            for (var i = start; i < nd; i++) rss += diff[i] * diff[i];
        }
        else
        {
            var ata = new double[cols * cols];
            var atb = new double[cols];
            for (var i = 0; i < rows; i++)
            {
                var t = i + start;
                var rcol = new double[cols];
                var c = 0;
                for (var lag = 1; lag <= p; lag++) rcol[c++] = diff[t - lag];
                for (var lag = 1; lag <= q; lag++) rcol[c++] = residuals[t - lag];
                for (var r = 0; r < cols; r++)
                {
                    atb[r] += rcol[r] * diff[t];
                    for (var c2 = 0; c2 < cols; c2++) ata[r * cols + c2] += rcol[r] * rcol[c2];
                }
            }
            var trace = 0.0;
            for (var i = 0; i < cols; i++) trace += ata[i * cols + i];
            var lambda = 1e-8 * (trace / cols);
            for (var i = 0; i < cols; i++) ata[i * cols + i] += lambda;
            var aug = new double[cols * (cols + 1)];
            for (var r = 0; r < cols; r++)
            {
                for (var c = 0; c < cols; c++) aug[r * (cols + 1) + c] = ata[r * cols + c];
                aug[r * (cols + 1) + cols] = atb[r];
            }
            var coeffs = GaussSolve(aug, cols);
            if (coeffs != null)
            {
                for (var i = 0; i < p; i++) arCoeffs[i] = coeffs[i];
                for (var i = 0; i < q; i++) maCoeffs[i] = coeffs[p + i];
                for (var i = 0; i < rows; i++)
                {
                    var t = i + start;
                    var pred = 0.0;
                    var c = 0;
                    for (var lag = 1; lag <= p; lag++) pred += coeffs[c++] * diff[t - lag];
                    for (var lag = 1; lag <= q; lag++) pred += coeffs[c++] * residuals[t - lag];
                    var e = diff[t] - pred;
                    rss += e * e;
                    fitted[t] = pred;
                }
            }
        }
        var kk = p + q + 1;
        var sigma2 = rows > 0 ? rss / rows : 0.0;
        var aic = rows > 0 && sigma2 > 0 ? rows * Math.Log(sigma2) + 2 * kk : 0.0;
        return (arCoeffs, maCoeffs, aic, fitted);
    }

    /// <summary>Eliminazione di Gauss a pivot parziale su matrice aumentata n×(n+1)
    /// (row-major, come `gmb_dense_solve` del runtime C). Ritorna x (n) o null se singolare.</summary>
    private static double[]? GaussSolve(double[] aug, int n)
    {
        for (var col = 0; col < n; col++)
        {
            var piv = col;
            for (var r = col + 1; r < n; r++)
                if (Math.Abs(aug[r * (n + 1) + col]) > Math.Abs(aug[piv * (n + 1) + col])) piv = r;
            if (piv != col)
                for (var j = col; j <= n; j++)
                {
                    var tmp = aug[col * (n + 1) + j]; aug[col * (n + 1) + j] = aug[piv * (n + 1) + j]; aug[piv * (n + 1) + j] = tmp;
                }
            for (var r = col + 1; r < n; r++)
            {
                var f = aug[r * (n + 1) + col] / aug[col * (n + 1) + col];
                for (var j = col; j <= n; j++) aug[r * (n + 1) + j] -= f * aug[col * (n + 1) + j];
            }
        }
        var x = new double[n];
        for (var r = n - 1; r >= 0; r--)
        {
            var s = aug[r * (n + 1) + n];
            for (var j = r + 1; j < n; j++) s -= aug[r * (n + 1) + j] * x[j];
            x[r] = s / aug[r * (n + 1) + r];
        }
        return x;
    }
}
