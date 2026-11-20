// FILE: StdLib.Numerics.Advanced.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.LinearAlgebra;
// ReSharper disable InconsistentNaming

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>
    /// Calcola la distanza euclidea quadratica tra due punti nello spazio n-dimensionale.
    /// Utile come metrica di similarità in algoritmi di clustering e nearest neighbor.
    /// </summary>
    /// <param name="p1">Primo punto (array di coordinate).</param>
    /// <param name="p2">Secondo punto (stessa dimensionalità di <paramref name="p1"/>).</param>
    /// <returns>Distanza quadratica Σ(p₁ᵢ − p₂ᵢ)².</returns>
    /// <exception cref="ArgumentException">Se i punti hanno dimensionalità diversa.</exception>
    /// <example>
    /// double d = Numerics.EuclideanDistanceSquared(
    ///     new[] { 1.0, 2.0 },
    ///     new[] { 4.0, 6.0 });
    /// // d = (1-4)² + (2-6)² = 9 + 16 = 25
    /// </example>
    public static double EuclideanDistanceSquared(double[] p1, double[] p2)
    {
        if (p1.Length != p2.Length)
            throw new ArgumentException("I punti devono avere la stessa dimensionalità.");
        return AnalysisNative.EuclideanDistanceSquared(p1, p2);
    }

    /// <summary>
    /// Esegue il fitting e la previsione con il modello Holt-Winters
    /// (smoothing esponenziale con trend e stagionalità).
    /// </summary>
    /// <param name="series">Serie storica di input.</param>
    /// <param name="seasonalPeriod">Lunghezza del periodo stagionale (es. 12 per dati mensili).</param>
    /// <param name="alpha">Parametro di smoothing del livello (0-1).</param>
    /// <param name="beta">Parametro di smoothing del trend (0-1).</param>
    /// <param name="gamma">Parametro di smoothing della stagionalità (0-1).</param>
    /// <param name="horizon">Numero di passi futuri da prevedere (default: 0).</param>
    /// <returns>Tupla <c>(fitted, forecast, mse)</c>: <c>fitted</c> è il fit in-sample sull'intera
    /// serie storica (lunghezza <c>series.Length</c>, zero per gli indici prima di
    /// <paramref name="seasonalPeriod"/>, dove il modello non ha ancora abbastanza storia per
    /// stimare la componente stagionale); <c>forecast</c> è un array di lunghezza
    /// <paramref name="horizon"/> con le previsioni FUTURE oltre la fine della serie; <c>mse</c>
    /// è l'errore quadratico medio sul periodo di fitting.</returns>
    /// <example>
    /// var (fitted, forecast, mse) = Numerics.HoltWintersFit(
    ///     series, seasonalPeriod: 12,
    ///     alpha: 0.5, beta: 0.1, gamma: 0.3,
    ///     horizon: 6);
    /// </example>
    public static (double[] fitted, double[] forecast, double mse) HoltWintersFit(
        double[] series, int seasonalPeriod,
        double alpha, double beta, double gamma, int horizon = 0)
    {
        var n = series.Length;

        var level = new double[n];
        var trend = new double[n];
        var seasonal = new double[n];
        var forecasts = new double[n + horizon];

        level[0] = series[0];
        trend[0] = series[1] - series[0];
        for (var i = 0; i < seasonalPeriod && i < n; i++)
        {
            seasonal[i] = series[i] - level[0];
        }

        for (var t = seasonalPeriod; t < n; t++)
        {
            var prevLevel = level[t - 1];
            var prevTrend = trend[t - 1];
            var prevSeasonal = seasonal[t - seasonalPeriod];

            level[t] = alpha * (series[t] - prevSeasonal) + (1 - alpha) * (prevLevel + prevTrend);
            trend[t] = beta * (level[t] - prevLevel) + (1 - beta) * prevTrend;
            seasonal[t] = gamma * (series[t] - level[t] - trend[t]) + (1 - gamma) * prevSeasonal;

            forecasts[t] = prevLevel + prevTrend + prevSeasonal;
        }

        for (var t = n; t < n + horizon; t++)
        {
            var p = t - n;
            var seasonalIndex = t - seasonalPeriod;
            while (seasonalIndex < 0) seasonalIndex += seasonalPeriod;
            forecasts[t] = level[n - 1] + (p + 1) * trend[n - 1] + seasonal[seasonalIndex];
        }

        var mse = 0.0;
        var countMse = 0;
        for (var t = seasonalPeriod; t < n; t++)
        {
            mse += Math.Pow(series[t] - forecasts[t], 2);
            countMse++;
        }
        mse = countMse > 0 ? mse / countMse : 0;

        // "forecasts" qui sopra copre [0, n+horizon): [0, n) è il fit in-sample (zero prima di
        // seasonalPeriod), [n, n+horizon) sono le previsioni future — separati esplicitamente
        // nel valore di ritorno, prima erano fusi in un unico array e SOLO la coda futura veniva
        // restituita (forecasts.Skip(n).Take(horizon)): con horizon=0 (default, nessun parametro
        // BASIC lo espone — vedi StdLib.Analysis.cs) il chiamante riceveva SEMPRE un array vuoto,
        // qualunque fosse la serie in ingresso (bug scoperto testando AnalysisTests.bas: la
        // sintassi BASIC HOLTWINTERS(serie, alpha, beta, gamma, period) non ha uno slot per
        // horizon, quindi HwRes.Forecast(i) va inteso come il fit in-sample, non una previsione
        // futura mai richiesta).
        return (forecasts.Take(n).ToArray(), forecasts.Skip(n).Take(horizon).ToArray(), mse);
    }

    /// <summary>
    /// Calcola la funzione di autocorrelazione (ACF) di una serie fino a un lag massimo.
    /// </summary>
    /// <param name="series">Serie temporale di input.</param>
    /// <param name="maxLag">Numero massimo di lag da calcolare.</param>
    /// <returns>Array di coefficienti di autocorrelazione per lag 0..maxLag.</returns>
    /// <example>
    /// double[] acf = Numerics.AutoCorrelation(series, maxLag: 20);
    /// </example>
    public static double[] AutoCorrelation(double[] series, int maxLag)
    {
        if (series.Length <= 1 || maxLag < 0) return [];
        return MathNet.Numerics.Statistics.Correlation.Auto(series, Math.Min(maxLag, series.Length - 1));
    }

    /// <summary>
    /// Risolve le equazioni di Yule-Walker per stimare i coefficienti
    /// di un modello autoregressivo AR(p) a partire dall'ACF.
    /// </summary>
    /// <param name="acf">Funzione di autocorrelazione (lag 0..p).</param>
    /// <param name="p">Ordine del modello AR.</param>
    /// <returns>Array di lunghezza <paramref name="p"/> con i coefficienti AR stimati.</returns>
    /// <exception cref="ArgumentException">Se l'ACF è troppo corta per l'ordine richiesto.</exception>
    /// <exception cref="InvalidOperationException">Se il sistema di equazioni non può essere risolto.</exception>
    /// <example>
    /// double[] arCoeffs = Numerics.YuleWalker(acf, p: 2);
    /// </example>
    public static double[] YuleWalker(double[] acf, int p)
    {
        if (p == 0) return [];
        if (acf == null || acf.Length < p + 1)
            throw new ArgumentException("ACF troppo corto per l'ordine AR specificato.");

        return AnalysisNative.YuleWalker(acf, p);
    }

    /// <summary>
    /// Stima i coefficienti di un modello a media mobile MA(q) a partire
    /// dall'autocorrelazione dei residui.
    /// </summary>
    /// <param name="residuals">Array dei residui del modello.</param>
    /// <param name="q">Ordine del modello MA.</param>
    /// <returns>Array di lunghezza <paramref name="q"/> con i coefficienti MA stimati.</returns>
    /// <example>
    /// double[] maCoeffs = Numerics.MaCoefficients(residuals, q: 2);
    /// </example>
    public static double[] MaCoefficients(double[] residuals, int q)
    {
        if (q == 0 || residuals.Length <= q) return [];
        return AnalysisNative.MaCoefficients(residuals, q);
    }

    /// <summary>
    /// Sottrae un vettore riga da ogni riga di una matrice.
    /// Operazione equivalente a centrare i dati sottraendo la media.
    /// </summary>
    /// <param name="matrix">Matrice di input <c>m × n</c>.</param>
    /// <param name="rowVector">Vettore riga di lunghezza <c>n</c> da sottrarre.</param>
    /// <returns>Nuova matrice con lo stesso numero di righe e colonne, dove ogni riga è stata ridotta di <paramref name="rowVector"/>.</returns>
    /// <exception cref="ArgumentException">Se il numero di colonne della matrice non corrisponde alla lunghezza del vettore.</exception>
    /// <example>
    /// var centered = Numerics.SubtractRowVector(dataMatrix, meanVector);
    /// </example>
    public static Matrix<double> SubtractRowVector(Matrix<double> matrix, Vector<double> rowVector)
    {
        if (matrix.ColumnCount != rowVector.Count)
        {
            throw new ArgumentException(
                "Il numero di colonne della matrice deve corrispondere alla lunghezza del vettore da sottrarre.");
        }
        var result = Matrix<double>.Build.Dense(matrix.RowCount, matrix.ColumnCount);
        for (var r = 0; r < matrix.RowCount; r++)
        {
            for (var c = 0; c < matrix.ColumnCount; c++)
            {
                result[r, c] = matrix[r, c] - rowVector[c];
            }
        }
        return result;
    }


    // ── Portfolio / Risk ──
    public static double MaxDrawdown(double[] returns)
    {
        double peak = returns[0], maxDD = 0;
        for (int i = 0; i < returns.Length; i++) {
            if (returns[i] > peak) peak = returns[i];
            double dd = (peak - returns[i]) / peak;
            if (dd > maxDD) maxDD = dd;
        }
        return maxDD;
    }

    public static double MaxReturn(double[] returns) => returns.Length > 0 ? returns.Max() : 0;

    public static double MinSemiVariance(double[] returns, double threshold = 0)
    {
        return AnalysisNative.MinSemiVariance(returns, threshold);
    }

    // ── Outliers (Z-score method) ──
    public static double[] Outliers(double[] data, double threshold = 3.0)
    {
        double mean = Mean(data), std = StdDev(data);
        var result = new List<double>();
        for (int i = 0; i < data.Length; i++)
            if (Math.Abs(data[i] - mean) / std > threshold)
                result.Add(data[i]);
        return result.ToArray();
    }

    // ── Linear Regression ──
    public static (double slope, double intercept, double rSquared) LinearRegression(double[] x, double[] y)
    {
        int n = Math.Min(x.Length, y.Length);
        return AnalysisNative.LinearRegression(x, y, n);
    }

    // ── Rolling Regression ──
    public static double[] RollingRegression(double[] x, double[] y, int window)
    {
        int n = Math.Min(x.Length, y.Length);
        var slopes = new double[n];
        for (int i = window - 1; i < n; i++) {
            var (s, _, _) = LinearRegression(x[(i - window + 1)..(i + 1)], y[(i - window + 1)..(i + 1)]);
            slopes[i] = s;
        }
        return slopes;
    }

    // ── PCA (simplified: first component via power iteration) ──
    public static (double[] eigenvalues, double[,] eigenvectors) PCA(double[,] data)
    {
        int m = data.GetLength(0), n = data.GetLength(1);
        // Center the data
        var means = new double[n];
        for (int j = 0; j < n; j++) { for (int i = 0; i < m; i++) means[j] += data[i, j]; means[j] /= m; }
        var centered = new double[m, n];
        for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) centered[i, j] = data[i, j] - means[j];
        // Compute covariance matrix
        var cov = new double[n, n];
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                double s = 0;
                for (int k = 0; k < m; k++) s += centered[k, i] * centered[k, j];
                cov[i, j] = s / (m - 1);
            }
        // Power iteration for eigenvalues/vectors
        var eigenvals = new double[n];
        var eigenvecs = new double[n, n];
        for (int p = 0; p < n; p++) {
            var v = new double[n]; v[p % n] = 1;
            for (int iter = 0; iter < 100; iter++) {
                var av = new double[n];
                for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) av[i] += cov[i, j] * v[j];
                double norm = Math.Sqrt(av.Sum(x => x * x));
                if (norm < 1e-10) break;
                for (int i = 0; i < n; i++) av[i] /= norm;
                if (av.Zip(v, (a, b) => Math.Abs(a - b)).Max() < 1e-8) { v = av; break; }
                v = av;
            }
            eigenvals[p] = 0;
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) eigenvals[p] += v[i] * cov[i, j] * v[j];
            for (int i = 0; i < n; i++) eigenvecs[i, p] = v[i];
            // Deflate
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) cov[i, j] -= eigenvals[p] * v[i] * v[j];
        }
        return (eigenvals, eigenvecs);
    }

    // ── ANOVA (one-way) ──
    public static (double fStat, double pValue, double ssBetween, double ssWithin) Anova(double[][] groups)
    {
        ArgumentNullException.ThrowIfNull(groups);
        if (groups.Length < 2 || Array.Exists(groups, static group => group is null || group.Length == 0))
            throw new ArgumentException("ANOVA richiede almeno due gruppi non vuoti.", nameof(groups));

        var groupCount = groups.Length;
        var sampleCount = groups.Sum(static group => group.Length);
        if (sampleCount <= groupCount)
            throw new ArgumentException("ANOVA richiede almeno un grado di libertà interno ai gruppi.", nameof(groups));

        var grandMean = groups.SelectMany(static group => group).Average();
        var ssBetween = groups.Sum(group => group.Length * Math.Pow(group.Average() - grandMean, 2));
        var ssWithin = groups.Sum(group =>
        {
            var groupMean = group.Average();
            return group.Sum(value => Math.Pow(value - groupMean, 2));
        });
        var degreesBetween = groupCount - 1;
        var degreesWithin = sampleCount - groupCount;
        var meanSquareBetween = ssBetween / degreesBetween;
        var meanSquareWithin = ssWithin / degreesWithin;
        var fStatistic = meanSquareWithin == 0d
            ? meanSquareBetween == 0d ? double.NaN : double.PositiveInfinity
            : meanSquareBetween / meanSquareWithin;
        var pValue = double.IsNaN(fStatistic)
            ? double.NaN
            : 1d - MathNet.Numerics.Distributions.FisherSnedecor.CDF(
                degreesBetween, degreesWithin, fStatistic);
        return (fStatistic, pValue, ssBetween, ssWithin);
    }

    static double BetaRegularized(double a, double b, double x)
    {
        if (x < 0 || x > 1) return 0;
        // Continued fraction approximation
        double apb = a + b;
        double front = Math.Exp(GammaLn(a + b) - GammaLn(a) - GammaLn(b) + a * Math.Log(x) + b * Math.Log(1 - x));
        double c = 1, d = 1 - (apb) * x / (a + 1);
        if (Math.Abs(d) < 1e-30) d = 1e-30;
        d = 1 / d; double h = d;
        for (int m = 1; m <= 100; m++) {
            int m2 = 2 * m;
            double aa = m * (b - m) * x / ((a + m2 - 1) * (a + m2));
            d = 1 + aa * d; if (Math.Abs(d) < 1e-30) d = 1e-30;
            c = 1 + aa / c; if (Math.Abs(c) < 1e-30) c = 1e-30;
            d = 1 / d; h *= d * c;
            aa = -(a + m) * (apb + m) * x / ((a + m2) * (a + m2 + 1));
            d = 1 + aa * d; if (Math.Abs(d) < 1e-30) d = 1e-30;
            c = 1 + aa / c; if (Math.Abs(c) < 1e-30) c = 1e-30;
            d = 1 / d; double del = d * c; h *= del;
            if (Math.Abs(del - 1) < 3e-7) break;
        }
        return front * (h - 1) / a;
    }

    static double GammaLn(double xx)
    {
        double[] cof = { 76.18009172947146, -86.50532032941677, 24.01409824083091, -1.231739572450155, 0.1208650973866179e-2, -0.5395239384953e-5 };
        double y = xx, x = xx, tmp = x + 5.5;
        tmp -= (x + 0.5) * Math.Log(tmp);
        double ser = 1.000000000190015;
        for (int j = 0; j < 6; j++) ser += cof[j] / ++y;
        return -tmp + Math.Log(2.5066282746310005 * ser / x);
    }


    // ── Cholesky Decomposition & Solve ──
    /// <summary>
    /// Risolve Ax = b con A simmetrica definita positiva usando decomposizione di Cholesky.
    /// </summary>
    public static double[]? CholeskySolve(double[,] A, double[] b)
    {
        int n = A.GetLength(0);
        if (n != A.GetLength(1) || n != b.Length) return null;
        var L = new double[n, n];
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++)
            {
                double sum = 0;
                for (int k = 0; k < j; k++) sum += L[i, k] * L[j, k];
                if (i == j)
                {
                    double d = A[i, i] - sum;
                    if (d <= 1e-12) return null;
                    L[i, i] = Math.Sqrt(d);
                }
                else L[i, j] = (A[i, j] - sum) / L[j, j];
            }
        }
        var y = new double[n];
        for (int i = 0; i < n; i++)
        {
            double sum = 0;
            for (int j = 0; j < i; j++) sum += L[i, j] * y[j];
            y[i] = (b[i] - sum) / L[i, i];
        }
        var x = new double[n];
        for (int i = n - 1; i >= 0; i--)
        {
            double sum = 0;
            for (int j = i + 1; j < n; j++) sum += L[j, i] * x[j];
            x[i] = (y[i] - sum) / L[i, i];
        }
        return x;
    }

    // ── Conditional Value at Risk ──
    /// <summary>Calcola il Conditional VaR (Expected Shortfall).</summary>
    public static double ConditionalVaR(double[] returns, double confidence = 0.95)
    {
        if (returns.Length == 0) return 0;
        var sorted = (double[])returns.Clone();
        Array.Sort(sorted);
        int cutoff = (int)((1 - confidence) * sorted.Length);
        if (cutoff <= 0) cutoff = 1;
        double sum = 0;
        for (int i = 0; i < cutoff; i++) sum += sorted[i];
        double cvar = sum / cutoff;
        return cvar < 0 ? -cvar : cvar;
    }
}
