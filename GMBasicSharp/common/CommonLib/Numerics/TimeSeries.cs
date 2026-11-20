// FILE: StdLib.Numerics.TimeSeries.cs
// ReSharper disable CheckNamespace

// ReSharper disable InconsistentNaming
namespace CommonLib;

/// <summary>
/// Metodi per l'analisi di serie temporali finanziarie e statistiche.
/// Contiene tecniche di smoothing, rilevamento outlier, trasformazioni,
/// decomposizione e finestre mobili (rolling).
/// </summary>
/// <remarks>
/// Metodi disponibili:
/// <list type="bullet">
///   <item><description>MovingAverage — media mobile semplice</description></item>
///   <item><description>Ema — media mobile esponenziale</description></item>
///   <item><description>Outliers — rilevamento outlier (Z-score / IQR)</description></item>
///   <item><description>Difference — differenziazione</description></item>
///   <item><description>Returns — rendimenti semplici o logaritmici</description></item>
///   <item><description>Volatility — volatilità storica</description></item>
///   <item><description>Autocorrelation — autocorrelazione a lag fissato</description></item>
///   <item><description>Deseasonalize — destagionalizzazione</description></item>
///   <item><description>Decompose — decomposizione trend-stagionalità-residuo</description></item>
///   <item><description>CovarianceMatrix — matrice di covarianza</description></item>
///   <item><description>MinMax — minimo e massimo</description></item>
///   <item><description>RollingMean, RollingStdDev, RollingSkewness, RollingKurtosis, RollingMedian, RollingQuantile — statistiche mobili</description></item>
///   <item><description>RollingSharpe, RollingSortino — indici risk-adjusted mobili</description></item>
///   <item><description>RollingMad, RollingZScore — dispersioni mobili</description></item>
///   <item><description>RollingAutocorrelation, RollingCovariance, RollingCorrelation — relazioni mobili</description></item>
///   <item><description>RollingRegression — regressione lineare mobile</description></item>
///   <item><description>RollingMinMax — range mobile</description></item>
///   <item><description>RollingDelta — differenza mobile</description></item>
/// </list>
/// </remarks>
public static partial class Numerics
{
    public static double[] MovingAverage(double[] data, int window)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (window < 1) throw new ArgumentException("La finestra deve essere almeno 1.", nameof(window));
        if (data.Length < window) throw new ArgumentException("La serie è troppo corta per la finestra richiesta.");
        return TimeSeriesNative.MovingAverage(data, window);
    }

    /// <summary>
    /// Calcola la media mobile esponenziale (EMA) di una serie temporale.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="alpha">Fattore di smoothing nell'intervallo (0, 1].</param>
    /// <returns>Array della stessa lunghezza di <paramref name="data"/> con i valori EMA.</returns>
    /// <exception cref="ArgumentException">
    /// Sollevata se <paramref name="data"/> è vuoto o se <paramref name="alpha"/> non è in (0, 1].
    /// </exception>
    /// <example>
    /// double[] ema = Numerics.Ema(new[] { 1.0, 2.0, 3.0 }, alpha: 0.3);
    /// </example>
    public static double[] Ema(double[] data, double alpha)
    {
        if (data == null || data.Length == 0) throw new ArgumentException("Dati vuoti.");
        if (alpha is <= 0 or > 1) throw new ArgumentException("Alpha deve essere in (0,1].");
        return TimeSeriesNative.Ema(data, alpha);
    }

    /// <summary>
    /// Individua gli indici degli outlier in una serie numerica.
    /// Supporta il metodo Z-score (default) o IQR (scatola e baffi).
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="method">Metodo da usare: "Z" per Z-score, "IQR" per intervallo interquartile.</param>
    /// <param name="threshold">Soglia oltre la quale un punto è considerato outlier (default: 3.0 per Z, 1.5 per IQR).</param>
    /// <returns>Array di indici corrispondenti agli outlier.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="method"/> non è supportato.</exception>
    /// <example>
    /// int[] idx = Numerics.Outliers(new[] { 1.0, 2.0, 100.0, 3.0 }, method: "Z", threshold: 2.0);
    /// </example>
    public static int[] Outliers(double[] data, string method = "Z", double? threshold = null)
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (data.Length < 3) return [];
        method = method.ToUpperInvariant();
        if (method == "Z")
            return TimeSeriesNative.OutliersZ(data, threshold ?? 3.0);
        if (method != "IQR")
            throw new ArgumentException("Metodo non supportato: " + method);
        // IQR: nessun kernel C equivalente (`gmb_ts_outliers` copre solo il metodo Z) → managed.
        var th = threshold ?? 1.5;
        var sorted = data.OrderBy(x => x).ToArray();
        var n = sorted.Length;
        var Q1 = sorted[n / 4];
        var Q3 = sorted[(3 * n) / 4];
        var iqr = Q3 - Q1;
        var low = Q1 - th * iqr;
        var high = Q3 + th * iqr;
        var outlierIdx = new List<int>();
        for (var i = 0; i < data.Length; i++)
            if (data[i] < low || data[i] > high)
                outlierIdx.Add(i);
        return outlierIdx.ToArray();
    }

    /// <summary>
    /// Applica una differenziazione di ordine <paramref name="order"/> alla serie.
    /// Utile per rendere stazionaria una serie temporale.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="order">Numero di differenziazioni successive (default: 1).</param>
    /// <returns>Serie differenziata, di lunghezza <c>n - order</c>.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="order"/> è minore di 1.</exception>
    /// <example>
    /// double[] diff = Numerics.Difference(new[] { 5.0, 7.0, 10.0 }, order: 1);
    /// // diff = [2.0, 3.0]
    /// </example>
    public static double[] Difference(double[] data, int order = 1)
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (order < 1) throw new ArgumentException("L'ordine deve essere >= 1.");
        return TimeSeriesNative.Difference(data, order);
    }

    /// <summary>
    /// Calcola i rendimenti di una serie di prezzi.
    /// Supporta rendimenti semplici (default) o logaritmici.
    /// </summary>
    /// <param name="data">Serie di prezzi consecutivi.</param>
    /// <param name="type">Tipo rendimento: "SIMPLE" (default) per rendimenti percentuali, "LOG" per log-rendimenti.</param>
    /// <returns>Array di lunghezza <c>n - 1</c> con i rendimenti.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <example>
    /// double[] ret = Numerics.Returns(new[] { 100.0, 102.0, 101.0 }, type: "LOG");
    /// </example>
    public static double[] Returns(double[] data, string type = "SIMPLE")
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (data.Length < 2) return [];
        var log = type.Equals("LOG", StringComparison.InvariantCultureIgnoreCase);
        return TimeSeriesNative.Returns(data, log);
    }

    /// <summary>
    /// Calcola la volatilità storica (deviazione standard dei rendimenti) di una serie.
    /// </summary>
    /// <param name="data">Serie di prezzi.</param>
    /// <param name="type">Tipo rendimento per il calcolo ("SIMPLE" o "LOG").</param>
    /// <returns>Volatilità come deviazione standard dei rendimenti.</returns>
    /// <example>
    /// double vol = Numerics.Volatility(new[] { 100.0, 102.0, 98.0, 103.0 });
    /// </example>
    public static double Volatility(double[] data, string type = "SIMPLE")
    {
        if (data == null || data.Length < 2) return 0.0;
        var log = type.Equals("LOG", StringComparison.InvariantCultureIgnoreCase);
        return TimeSeriesNative.Volatility(data, log);
    }

    /// <summary>
    /// Calcola l'autocorrelazione di una serie a un dato lag.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="lag">Ritardo (lag) per il calcolo dell'autocorrelazione.</param>
    /// <returns>Coefficiente di autocorrelazione nell'intervallo [-1, 1].</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="lag"/> è inferiore a 1 o maggiore/uguale alla lunghezza dei dati.</exception>
    /// <example>
    /// double ac = Numerics.Autocorrelation(new[] { 1.0, 2.0, 3.0, 4.0 }, lag: 1);
    /// </example>
    public static double Autocorrelation(double[] data, int lag = 1)
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (lag < 1 || lag >= data.Length) throw new ArgumentException("Lag non valido.");
        return TimeSeriesNative.Autocorrelation(data, lag);
    }

    /// <summary>
    /// Destagionalizza una serie temporale sottraendo una media mobile centrata di periodo <paramref name="seasonLength"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="seasonLength">Lunghezza del periodo stagionale (es. 12 per mensile, 4 per trimestrale).</param>
    /// <returns>Serie destagionalizzata della stessa lunghezza.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="seasonLength"/> è minore di 2 o maggiore di metà della lunghezza dei dati.</exception>
    /// <example>
    /// double[] ds = Numerics.Deseasonalize(data, seasonLength: 12);
    /// </example>
    public static double[] Deseasonalize(double[] data, int seasonLength)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (seasonLength < 2 || seasonLength > data.Length / 2)
            throw new ArgumentException("Periodo non valido per la destagionalizzazione.");
        return TimeSeriesNative.Deseasonalize(data, seasonLength);
    }

    /// <summary>
    /// Decompone una serie temporale in trend, stagionalità e residuo secondo il modello additivo.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="seasonLength">Lunghezza del periodo stagionale.</param>
    /// <returns>Matrice 3×n dove la riga 0 è il trend, la riga 1 la stagionalità e la riga 2 il residuo.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="seasonLength"/> è minore di 2 o maggiore di metà della lunghezza dei dati.</exception>
    /// <example>
    /// var decomp = Numerics.Decompose(data, seasonLength: 12);
    /// double[] trend = decomp.GetRow(0);
    /// double[] season = decomp.GetRow(1);
    /// double[] resid = decomp.GetRow(2);
    /// </example>
    public static double[,] Decompose(double[] data, int seasonLength)
    {
        ArgumentNullException.ThrowIfNull(data);
        if (seasonLength < 2 || seasonLength > data.Length / 2)
            throw new ArgumentException("Periodo non valido per la decomposizione.");
        var n = data.Length;
        var trend = new double[n];
        var season = new double[n];
        var resid = new double[n];
        var half = seasonLength / 2;
        for (var i = 0; i < n; i++)
        {
            var left = Math.Max(0, i - half);
            var right = Math.Min(n - 1, i + half);
            double sum = 0;
            var count = 0;
            for (var j = left; j <= right; j++)
            {
                sum += data[j];
                count++;
            }
            trend[i] = sum / count;
        }
        var seasonMeans = new double[seasonLength];
        var seasonCounts = new int[seasonLength];
        for (var i = 0; i < n; i++)
        {
            var idx = i % seasonLength;
            seasonMeans[idx] += data[i] - trend[i];
            seasonCounts[idx]++;
        }
        for (var i = 0; i < seasonLength; i++)
            if (seasonCounts[i] > 0)
                seasonMeans[i] /= seasonCounts[i];
        for (var i = 0; i < n; i++)
            season[i] = seasonMeans[i % seasonLength];
        for (var i = 0; i < n; i++)
            resid[i] = data[i] - trend[i] - season[i];
        var result = new double[3, n];
        for (var j = 0; j < n; j++)
        {
            result[0, j] = trend[j];
            result[1, j] = season[j];
            result[2, j] = resid[j];
        }
        return result;
    }

    /// <summary>
    /// Calcola la matrice di covarianza campionaria per un insieme di serie multiple.
    /// </summary>
    /// <param name="data">Array di vettori, dove ogni vettore è un'osservazione con <c>m</c> variabili.</param>
    /// <returns>Matrice di covarianza <c>m × m</c> con correzione di Bessel (n − 1).</returns>
    /// <exception cref="ArgumentException">Se i dati sono null o contengono meno di 2 osservazioni.</exception>
    /// <example>
    /// double[,] cov = Numerics.CovarianceMatrix(new[] { new[] { 1.0, 2.0 }, new[] { 3.0, 4.0 } });
    /// </example>
    public static double[,] CovarianceMatrix(double[][] data)
    {
        if (data == null || data.Length < 2)
            throw new ArgumentException("Dati insufficienti");
        return TimeSeriesNative.CovarianceMatrix(data);
    }

    /// <summary>
    /// Trova il valore minimo e massimo in una serie numerica.
    /// </summary>
    /// <param name="series">Serie di input.</param>
    /// <returns>Tupla <c>(min, max)</c> contenente minimo e massimo.</returns>
    /// <exception cref="ArgumentException">Se la serie è vuota.</exception>
    /// <example>
    /// var (min, max) = Numerics.MinMax(new[] { 3.0, 1.0, 7.0, 2.0 });
    /// // min = 1.0, max = 7.0
    /// </example>
    public static (double min, double max) MinMax(double[] series)
    {
        if (series == null || series.Length == 0)
            throw new ArgumentException("Serie vuota.");
        double min = double.MaxValue, max = double.MinValue;
        foreach (var v in series)
        {
            if (v < min) min = v;
            if (v > max) max = v;
        }
        return (min, max);
    }

    /// <summary>
    /// Calcola la media mobile semplice su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con le medie mobili.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="window"/> è minore di 1 o superiore alla lunghezza dei dati.</exception>
    /// <example>
    /// double[] ma = Numerics.RollingMean(new[] { 1.0, 2.0, 3.0, 4.0 }, window: 2);
    /// </example>
    public static double[] RollingMean(double[] data, int window)
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (window < 1 || data.Length < window) throw new ArgumentException("Finestra non valida.");
        return TimeSeriesNative.RollingMean(data, window);
    }

    /// <summary>
    /// Calcola la deviazione standard mobile su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con le deviazioni standard mobili.</returns>
    /// <exception cref="ArgumentNullException">Se <paramref name="data"/> è null.</exception>
    /// <exception cref="ArgumentException">Se <paramref name="window"/> non è valido.</exception>
    /// <example>
    /// double[] std = Numerics.RollingStdDev(new[] { 1.0, 2.0, 3.0, 4.0 }, window: 3);
    /// </example>
    public static double[] RollingStdDev(double[] data, int window)
    {
        if (data == null) throw new ArgumentNullException(nameof(data));
        if (window < 1 || data.Length < window) throw new ArgumentException("Finestra non valida.");
        return TimeSeriesNative.RollingStdDev(data, window);
    }

    /// <summary>
    /// Calcola l'asimmetria (skewness) mobile su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 3).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con l'asimmetria campionaria.</returns>
    /// <exception cref="ArgumentException">Se <paramref name="data"/> è null o i parametri non sono validi.</exception>
    /// <example>
    /// double[] skew = Numerics.RollingSkewness(data, window: 30);
    /// </example>
    public static double[] RollingSkewness(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 3)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingSkewness(data, window);
    }

    /// <summary>
    /// Calcola la curtosi mobile (eccesso di curtosi) su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 4).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con la curtosi campionaria.</returns>
    /// <exception cref="ArgumentException">Se <paramref name="data"/> è null o i parametri non sono validi.</exception>
    /// <example>
    /// double[] kurt = Numerics.RollingKurtosis(data, window: 30);
    /// </example>
    public static double[] RollingKurtosis(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 4)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingKurtosis(data, window);
    }

    /// <summary>
    /// Calcola la mediana mobile su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con le mediane mobili.</returns>
    /// <exception cref="ArgumentException">Se <paramref name="data"/> è null o i parametri non sono validi.</exception>
    /// <example>
    /// double[] med = Numerics.RollingMedian(data, window: 5);
    /// </example>
    public static double[] RollingMedian(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 1)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingMedian(data, window);
    }

    /// <summary>
    /// Calcola un quantile mobile su finestra di lunghezza <paramref name="window"/>.
    /// Supporta interpolazione lineare tra i due valori più vicini.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <param name="quantile">Quantile richiesto nell'intervallo [0, 1] (es. 0.25 per Q1, 0.75 per Q3).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con i quantili mobili.</returns>
    /// <exception cref="ArgumentException">Se i parametri non sono validi o il quantile è fuori intervallo.</exception>
    /// <example>
    /// double[] q75 = Numerics.RollingQuantile(data, window: 20, quantile: 0.75);
    /// </example>
    public static double[] RollingQuantile(double[] data, int window, double quantile)
    {
        if (data == null || data.Length < window || window < 1)
            throw new ArgumentException("Dati o finestra non validi.");
        if (quantile is < 0 or > 1) throw new ArgumentException("Quantile fuori intervallo [0,1]");
        return TimeSeriesNative.RollingQuantile(data, window, quantile);
    }

    /// <summary>
    /// Calcola l'indice di Sharpe mobile (risk-adjusted return) su finestra di rendimenti.
    /// </summary>
    /// <param name="returns">Array di rendimenti periodici.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <param name="riskFreeRate">Tasso privo di rischio annuo da sottrarre (default: 0.0).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con gli Sharpe ratio mobili.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// double[] sharpe = Numerics.RollingSharpe(returns, window: 252);
    /// </example>
    public static double[] RollingSharpe(double[] returns, int window, double riskFreeRate = 0.0)
    {
        if (returns == null || returns.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingSharpe(returns, window, riskFreeRate);
    }

    /// <summary>
    /// Calcola la deviazione assoluta mediana (MAD) mobile.
    /// La MAD è una misura robusta della dispersione statistica.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con i valori MAD mobili.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// double[] mad = Numerics.RollingMad(data, window: 20);
    /// </example>
    public static double[] RollingMad(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 1)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingMad(data, window);
    }

    /// <summary>
    /// Calcola lo Z-score mobile dell'ultimo punto di ogni finestra.
    /// Misura di quanto l'ultimo valore si discosta dalla media della finestra in deviazioni standard.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con gli Z-score mobili.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// double[] zs = Numerics.RollingZScore(data, window: 20);
    /// </example>
    public static double[] RollingZScore(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingZScore(data, window);
    }

    /// <summary>
    /// Calcola l'autocorrelazione mobile a un dato lag su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <param name="lag">Ritardo per l'autocorrelazione.</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con le autocorrelazioni mobili.</returns>
    /// <exception cref="ArgumentException">Se i parametri non sono validi.</exception>
    /// <example>
    /// double[] ac = Numerics.RollingAutocorrelation(data, window: 50, lag: 1);
    /// </example>
    public static double[] RollingAutocorrelation(double[] data, int window, int lag)
    {
        if (data == null || data.Length < window || window < lag + 1 || lag < 1)
            throw new ArgumentException("Dati, finestra o lag non validi.");
        return TimeSeriesNative.RollingAutocorrelation(data, window, lag);
    }

    /// <summary>
    /// Calcola la covarianza mobile tra due serie <paramref name="x"/> e <paramref name="y"/>.
    /// </summary>
    /// <param name="x">Prima serie.</param>
    /// <param name="y">Seconda serie (stessa lunghezza di <paramref name="x"/>).</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con le covarianze mobili.</returns>
    /// <exception cref="ArgumentException">Se le serie hanno lunghezza diversa o la finestra non è valida.</exception>
    /// <example>
    /// double[] cov = Numerics.RollingCovariance(x, y, window: 20);
    /// </example>
    public static double[] RollingCovariance(double[] x, double[] y, int window)
    {
        if (x == null || y == null || x.Length != y.Length || x.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingCovariance(x, y, window);
    }

    /// <summary>
    /// Stima la regressione lineare semplice mobile (pendenza e intercetta) su finestra di lunghezza <paramref name="window"/>.
    /// La variabile indipendente implicita è l'indice temporale <c>j = 0, 1, ..., window−1</c>.
    /// </summary>
    /// <param name="y">Serie dipendente (valori osservati).</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <returns>Array di tuple <c>(slope, intercept)</c> di lunghezza <c>n − window + 1</c>.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// var reg = Numerics.RollingRegression(y, window: 20);
    /// foreach (var (slope, intercept) in reg) { ... }
    /// </example>
    public static (double slope, double intercept)[] RollingRegression(double[] y, int window)
    {
        if (y == null || y.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingRegression(y, window);
    }

    /// <summary>Regressione lineare mobile su DUE serie (y in funzione di x) — stesso contratto
    /// della runtime C (gmb_ts_rolling_regression): per ogni finestra una coppia
    /// (slope, intercept) = (beta, alpha). Allineata alla semantica del campione TSROLLREGRESSION
    /// che passa entrambe le serie. Nota: esiste già RollingRegression(double[],double[],int) in
    /// Advanced.cs (solo slope) — qui la versione a COPPIE con nome dedicato per overload.</summary>
    public static (double slope, double intercept)[] RollingRegressionPairs(double[] x, double[] y, int window)
    {
        if (x == null || y == null || x.Length < window || y.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingRegressionPairs(x, y, window);
    }

    /// <summary>
    /// Calcola la differenza mobile (delta) su finestra, con supporto per differenziazione di ordine superiore.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <param name="order">Ordine di differenziazione (default: 1).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con i delta mobili.</returns>
    /// <exception cref="ArgumentException">Se i parametri non sono validi.</exception>
    /// <example>
    /// double[] delta = Numerics.RollingDelta(data, window: 5, order: 1);
    /// </example>
    public static double[] RollingDelta(double[] data, int window, int order = 1)
    {
        if (data == null || data.Length < window || window < 2 || order < 1)
            throw new ArgumentException("Dati, finestra o ordine non validi.");
        // Il kernel C copre l'ordine 1 (`gmb_ts_rolldelta`); ordini superiori → managed.
        if (order == 1) return TimeSeriesNative.RollingDelta(data, window);
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var value = slice.Last() - slice.First();
            for (var o = 1; o < order; o++)
            {
                for (var j = 1; j < slice.Length; j++)
                    slice[j - 1] = slice[j] - slice[j - 1];
                value = slice[^order];
            }
            result[i] = value;
        }
        return result;
    }

    /// <summary>
    /// Calcola l'indice di Sortino mobile, che considera solo la volatilità negativa (downside deviation).
    /// </summary>
    /// <param name="returns">Array di rendimenti periodici.</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <param name="riskFreeRate">Tasso privo di rischio da sottrarre (default: 0.0).</param>
    /// <param name="targetReturn">Rendimento obiettivo minimo accettabile (default: 0.0).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con i Sortino ratio mobili.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// double[] sortino = Numerics.RollingSortino(returns, window: 252);
    /// </example>
    public static double[] RollingSortino(double[] returns, int window, double riskFreeRate = 0.0,
        double targetReturn = 0.0)
    {
        if (returns == null || returns.Length < window || window < 2)
            throw new ArgumentException("Dati o finestra non validi.");
        var result = new double[returns.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(returns, i, window).ToArray();
            var mean = slice.Average() - riskFreeRate;
            var downside = slice.Where(r => r < targetReturn).Select(r => Math.Pow(r - targetReturn, 2));
            var enumerable = downside.ToList();
            var downsideDeviation = enumerable.Count != 0 ? Math.Sqrt(enumerable.Average()) : 0.0;
            result[i] = downsideDeviation > 0.0 ? mean / downsideDeviation : double.PositiveInfinity;
        }
        return result;
    }

    /// <summary>
    /// Calcola il minimo e il massimo mobili su finestra di lunghezza <paramref name="window"/>.
    /// </summary>
    /// <param name="data">Serie di input.</param>
    /// <param name="window">Ampiezza della finestra mobile.</param>
    /// <returns>Array di tuple <c>(min, max)</c> di lunghezza <c>n − window + 1</c>.</returns>
    /// <exception cref="ArgumentException">Se i dati o la finestra non sono validi.</exception>
    /// <example>
    /// var mm = Numerics.RollingMinMax(data, window: 14);
    /// </example>
    public static (double min, double max)[] RollingMinMax(double[] data, int window)
    {
        if (data == null || data.Length < window || window < 1)
            throw new ArgumentException("Dati o finestra non validi.");
        return TimeSeriesNative.RollingMinMax(data, window);
    }

    /// <summary>
    /// Calcola la correlazione di Pearson mobile tra due serie <paramref name="x"/> e <paramref name="y"/>.
    /// </summary>
    /// <param name="x">Prima serie.</param>
    /// <param name="y">Seconda serie (stessa lunghezza di <paramref name="x"/>).</param>
    /// <param name="window">Ampiezza della finestra mobile (minimo 2).</param>
    /// <returns>Array di lunghezza <c>n − window + 1</c> con i coefficienti di correlazione mobili.</returns>
    /// <exception cref="ArgumentException">Se le serie hanno lunghezza diversa o la finestra non è valida.</exception>
    /// <example>
    /// double[] corr = Numerics.RollingCorrelation(x, y, window: 20);
    /// </example>
    public static double[] RollingCorrelation(double[] x, double[] y, int window)
    {
        if (x == null || y == null || x.Length != y.Length)
            throw new ArgumentException("Le serie devono avere la stessa lunghezza.");
        if (window < 2 || window > x.Length)
            throw new ArgumentException("Finestra non valida.");
        return TimeSeriesNative.RollingCorrelation(x, y, window);
    }

    /// <summary>Entropia campionaria (gmbstats gs_entropy_sample).</summary>
    public static double SampleEntropy(double[] x, int m, double r)
    {
        return TimeSeriesNative.SampleEntropy(x, m, r);
    }

    public static double ApproxEntropy(double[] x, int m, double r)
    {
        return TimeSeriesNative.ApproxEntropy(x, m, r);
    }

    public static double PermEntropy(double[] x, int m, int lag)
    {
        return TimeSeriesNative.PermEntropy(x, m, lag);
    }

    public static double Lyapunov(double[] x, int m, int tau)
    {
        return TimeSeriesNative.Lyapunov(x, m, tau);
    }

    // ── Johansen (parità gs_infer.c, eigensolver Jacobi) ────────────────────────────

    private static bool JohCholesky(int n, double[] A, double[] L)
    {
        int i, j, k;
        Array.Clear(L);
        for (i = 0; i < n; i++)
            for (j = 0; j <= i; j++)
            {
                var s = A[i * n + j];
                for (k = 0; k < j; k++) s -= L[i * n + k] * L[j * n + k];
                if (i == j) { if (s <= 0) return false; L[i * n + j] = Math.Sqrt(s); }
                else L[i * n + j] = s / L[j * n + j];
            }
        return true;
    }

    private static void JohLowerSolve(int n, int ncol, double[] L, double[] B, double[] X)
    {
        int i, j, c;
        for (c = 0; c < ncol; c++)
            for (i = 0; i < n; i++)
            {
                var s = B[i * ncol + c];
                for (j = 0; j < i; j++) s -= L[i * n + j] * X[j * ncol + c];
                X[i * ncol + c] = s / L[i * n + i];
            }
    }

    private static void JohEigSym(int n, double[] A, double[] eval)
    {
        int i, j, k, sweep;
        for (sweep = 0; sweep < 100; sweep++)
        {
            double off = 0.0, diag = 0.0;
            for (i = 0; i < n; i++)
            {
                diag += A[i * n + i] * A[i * n + i];
                for (j = i + 1; j < n; j++) off += A[i * n + j] * A[i * n + j];
            }
            if (off <= 1e-30 * (diag + 1e-300)) break;
            for (i = 0; i < n; i++)
                for (j = i + 1; j < n; j++)
                {
                    var apq = A[i * n + j];
                    if (Math.Abs(apq) < 1e-300) continue;
                    var app = A[i * n + i];
                    var aqq = A[j * n + j];
                    var tau = (aqq - app) / (2.0 * apq);
                    var t = (tau >= 0 ? 1.0 : -1.0) / (Math.Abs(tau) + Math.Sqrt(1.0 + tau * tau));
                    var c = 1.0 / Math.Sqrt(1.0 + t * t);
                    var sn = t * c;
                    for (k = 0; k < n; k++)
                    {
                        var aik = A[i * n + k];
                        var ajk = A[j * n + k];
                        A[i * n + k] = c * aik - sn * ajk;
                        A[j * n + k] = sn * aik + c * ajk;
                    }
                    for (k = 0; k < n; k++)
                    {
                        var aki = A[k * n + i];
                        var akj = A[k * n + j];
                        A[k * n + i] = c * aki - sn * akj;
                        A[k * n + j] = sn * aki + c * akj;
                    }
                    A[i * n + j] = 0.0; A[j * n + i] = 0.0;
                }
        }
        for (i = 0; i < n; i++) eval[i] = A[i * n + i];
        for (i = 1; i < n; i++)
        {
            var v = eval[i];
            j = i - 1;
            while (j >= 0 && eval[j] > v) { eval[j + 1] = eval[j]; j--; }
            eval[j + 1] = v;
        }
    }

    private static void JohResidualize(int rows, int ncol, int nreg, double[] Y, double[] X, double[] R)
    {
        int c, i, j;
        var beta = new double[nreg];
        var Xc = new double[rows * nreg];
        var yc = new double[rows];
        for (c = 0; c < ncol; c++)
        {
            for (i = 0; i < rows; i++)
            {
                for (j = 0; j < nreg; j++) Xc[i * nreg + j] = X[i * nreg + j];
                yc[i] = Y[i * ncol + c];
            }
            if (!InferOls(rows, nreg, Xc, yc, beta, out _, -1, out _)) { for (i = 0; i < rows; i++) R[i * ncol + c] = 0; continue; }
            for (i = 0; i < rows; i++)
            {
                double f = 0.0;
                for (j = 0; j < nreg; j++) f += X[i * nreg + j] * beta[j];
                R[i * ncol + c] = Y[i * ncol + c] - f;
            }
        }
    }

    public static (double[] EigenValues, double[] TraceStatistic, double[] MaxStatistic, int Observations, int Lags)
        Johansen(double[][] Y, int lags)
    {
        var nObs = Y.Length;
        var k = nObs > 0 ? Y[0].Length : 0;
        var rows = nObs - lags - 1;
        var cc = k * lags + 1;
        if (k < 1 || k > 32 || lags < 0 || rows <= k + cc)
            return (Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), 0, lags);
        var D = new double[rows * k];
        var Lv = new double[rows * k];
        var COV = new double[rows * cc];
        var Dr = new double[rows * k];
        var Lr = new double[rows * k];
        var S00 = new double[k * k];
        var S01 = new double[k * k];
        var S11 = new double[k * k];
        var W = new double[k * k];
        var P = new double[k * k];
        var Lc = new double[k * k];
        var half = new double[k * k];
        var halfT = new double[k * k];
        var sym = new double[k * k];
        var eval = new double[k];
        int i, j, r, row, l, v;
        for (row = 0; row < rows; row++)
        {
            var t = lags + 1 + row;
            for (v = 0; v < k; v++)
            {
                D[row * k + v] = Y[t][v] - Y[t - 1][v];
                Lv[row * k + v] = Y[t - 1][v];
            }
            for (l = 1; l <= lags; l++)
                for (v = 0; v < k; v++)
                    COV[row * cc + (l - 1) * k + v] = Y[t - l][v] - Y[t - l - 1][v];
            COV[row * cc + cc - 1] = 1.0;
        }
        JohResidualize(rows, k, cc, D, COV, Dr);
        JohResidualize(rows, k, cc, Lv, COV, Lr);
        for (i = 0; i < k; i++)
            for (j = 0; j < k; j++)
            {
                double s00 = 0, s01 = 0, s11 = 0;
                for (row = 0; row < rows; row++)
                {
                    s00 += Dr[row * k + i] * Dr[row * k + j];
                    s01 += Dr[row * k + i] * Lr[row * k + j];
                    s11 += Lr[row * k + i] * Lr[row * k + j];
                }
                S00[i * k + j] = s00 / rows;
                S01[i * k + j] = s01 / rows;
                S11[i * k + j] = s11 / rows;
            }
        var col = new double[k];
        var sol = new double[k];
        for (j = 0; j < k; j++)
        {
            var cm = (double[])S00.Clone();
            for (i = 0; i < k; i++) col[i] = S01[i * k + j];
            if (!InferSolve(k, cm, col, sol))
                return (Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), 0, lags);
            for (i = 0; i < k; i++) W[i * k + j] = sol[i];
        }
        for (i = 0; i < k; i++)
            for (j = 0; j < k; j++)
            {
                double s = 0.0;
                for (v = 0; v < k; v++) s += S01[v * k + i] * W[v * k + j];
                P[i * k + j] = s;
            }
        Array.Copy(S11, Lc, k * k);
        if (!JohCholesky(k, S11, Lc))
            return (Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), 0, lags);
        JohLowerSolve(k, k, Lc, P, half);
        for (i = 0; i < k; i++) for (j = 0; j < k; j++) halfT[i * k + j] = half[j * k + i];
        JohLowerSolve(k, k, Lc, halfT, sym);
        JohEigSym(k, sym, eval);
        for (i = 0; i < k; i++)
        {
            var lam = eval[i];
            if (lam < 0.0) lam = 0.0;
            if (lam > 1.0 - 1e-15) lam = 1.0 - 1e-15;
            eval[i] = lam;
        }
        var traceS = new double[k];
        var maxS = new double[k];
        for (r = 0; r < k; r++)
        {
            double tot = 0.0;
            for (i = r; i < k; i++) tot += -Math.Log(1.0 - eval[i]);
            traceS[r] = rows * tot;
            maxS[r] = rows * -Math.Log(1.0 - eval[r]);
        }
        return (eval, traceS, maxS, rows, lags);
    }

    // ── nucleo ADF + OLS (parità gs_infer.c) ────────────────────────────────────────

    private static bool InferSolve(int n, double[] A, double[] b, double[] x)
    {
        int i, j, k, piv;
        for (i = 0; i < n; i++)
        {
            piv = i;
            for (j = i + 1; j < n; j++) if (Math.Abs(A[j * n + i]) > Math.Abs(A[piv * n + i])) piv = j;
            if (Math.Abs(A[piv * n + i]) < 1e-300) return false;
            if (piv != i)
            {
                for (k = 0; k < n; k++) { (A[i * n + k], A[piv * n + k]) = (A[piv * n + k], A[i * n + k]); }
                (b[i], b[piv]) = (b[piv], b[i]);
            }
            for (j = i + 1; j < n; j++)
            {
                var f = A[j * n + i] / A[i * n + i];
                for (k = i; k < n; k++) A[j * n + k] -= f * A[i * n + k];
                b[j] -= f * b[i];
            }
        }
        for (i = n - 1; i >= 0; i--)
        {
            var s = b[i];
            for (j = i + 1; j < n; j++) s -= A[i * n + j] * x[j];
            x[i] = s / A[i * n + i];
        }
        return true;
    }

    private static bool InferOls(int rows, int cols, double[] X, double[] y,
        double[] beta, out double ssr, int leverageCol, out double leverageOut)
    {
        ssr = 0.0;
        leverageOut = 0.0;
        if (rows <= cols || cols > 64) return false;
        var XtX = new double[64 * 64];
        var Xty = new double[64];
        var z = new double[64];
        var e = new double[64];
        int i, j, k;
        for (i = 0; i < rows; i++)
            for (j = 0; j < cols; j++)
            {
                Xty[j] += X[i * cols + j] * y[i];
                for (k = 0; k < cols; k++) XtX[j * cols + k] += X[i * cols + j] * X[i * cols + k];
            }
        var XtX0 = (double[])XtX.Clone();
        if (!InferSolve(cols, XtX, Xty, beta)) return false;
        for (i = 0; i < rows; i++)
        {
            double f = 0.0;
            for (j = 0; j < cols; j++) f += X[i * cols + j] * beta[j];
            var r = y[i] - f;
            ssr += r * r;
        }
        if (leverageCol >= 0)
        {
            for (j = 0; j < cols; j++) e[j] = j == leverageCol ? 1.0 : 0.0;
            Array.Copy(XtX0, XtX, cols * cols);
            if (!InferSolve(cols, XtX, e, z)) return false;
            leverageOut = z[leverageCol];
        }
        return true;
    }

    private static (double Statistic, int Lags, int Observations)? InferAdfCore(
        int n, double[] series, int lags, int firstObs, int deterministic)
    {
        var nd = deterministic;
        var cols = nd + 1 + lags;
        var levelCol = nd;
        var rows = n - firstObs;
        if (rows <= cols || firstObs < 1 + lags) return null;
        var X = new double[rows * cols];
        var y = new double[rows];
        var beta = new double[cols];
        for (var row = 0; row < rows; row++)
        {
            var t = firstObs + row;
            y[row] = series[t] - series[t - 1];
            if (nd >= 1) X[row * cols + 0] = 1.0;
            if (nd >= 2) X[row * cols + 1] = t + 1;
            X[row * cols + levelCol] = series[t - 1];
            for (var l = 1; l <= lags; l++)
                X[row * cols + levelCol + l] = series[t - l] - series[t - l - 1];
        }
        if (!InferOls(rows, cols, X, y, beta, out var ssr, levelCol, out var lev)) return null;
        var varr = ssr / (rows - cols);
        var se = Math.Sqrt(varr * lev);
        return (beta[levelCol] / se, lags, rows);
    }

    public static (double Statistic, int Lags, int Observations, int Deterministic, double AlphaBar)
        Dfgls(double[] series, int lags, int deterministic)
    {
        var n = series.Length;
        var nd = deterministic == 1 ? 2 : 1;
        var cBar = nd == 2 ? 13.5 : 7.0;
        var alphaBar = 1.0 - cBar / n;
        if (n <= nd + lags + 4)
            return (double.NaN, lags, 0, deterministic, double.NaN);
        var qdDet = new double[n * nd];
        var qdSer = new double[n];
        var det = new double[n * nd];
        var psi = new double[nd];
        var detrended = new double[n];
        int t, j;
        for (t = 0; t < n; t++)
        {
            det[t * nd + 0] = 1.0;
            if (nd == 2) det[t * nd + 1] = t + 1;
        }
        for (t = 0; t < n; t++)
        {
            for (j = 0; j < nd; j++)
                qdDet[t * nd + j] = t == 0 ? det[j] : det[t * nd + j] - alphaBar * det[(t - 1) * nd + j];
            qdSer[t] = t == 0 ? series[0] : series[t] - alphaBar * series[t - 1];
        }
        if (!InferOls(n, nd, qdDet, qdSer, psi, out _, -1, out _))
            return (double.NaN, lags, 0, deterministic, double.NaN);
        for (t = 0; t < n; t++)
        {
            double fit = 0.0;
            for (j = 0; j < nd; j++) fit += psi[j] * det[t * nd + j];
            detrended[t] = series[t] - fit;
        }
        var ar = InferAdfCore(n, detrended, lags, 1 + lags, 0);
        if (ar is null) return (double.NaN, lags, 0, deterministic, double.NaN);
        return (ar.Value.Statistic, ar.Value.Lags, ar.Value.Observations, deterministic, alphaBar);
    }

    public static (double Statistic, int Lags, int Observations, int NRegressors, int WithConstant)
        EngleGranger(double[][] Y, int dependent, int lags, int withConstant)
    {
        var n = Y.Length;
        var k = n > 0 ? Y[0].Length : 0;
        var cols = (k - 1) + (withConstant != 0 ? 1 : 0);
        if (n <= cols + lags + 2 || k < 2 || dependent < 0 || dependent >= k)
            return (double.NaN, lags, 0, cols, withConstant);
        var X = new double[n * cols];
        var resid = new double[n];
        var beta = new double[cols];
        var yc = new double[n];
        int i, j, row;
        for (i = 0; i < n; i++)
        {
            var c = 0;
            if (withConstant != 0) X[i * cols + c++] = 1.0;
            for (j = 0; j < k; j++) if (j != dependent) X[i * cols + c++] = Y[i][j];
            yc[i] = Y[i][dependent];
        }
        if (!InferOls(n, cols, X, yc, beta, out _, -1, out _))
            return (double.NaN, lags, 0, cols, withConstant);
        for (row = 0; row < n; row++)
        {
            double f = 0.0;
            for (j = 0; j < cols; j++) f += X[row * cols + j] * beta[j];
            resid[row] = Y[row][dependent] - f;
        }
        var ar = InferAdfCore(n, resid, lags, 1 + lags, withConstant != 0 ? 1 : 0);
        if (ar is null) return (double.NaN, lags, 0, cols, withConstant);
        return (ar.Value.Statistic, ar.Value.Lags, ar.Value.Observations, cols, withConstant);
    }

    // ── KPSS e QLR (parità gs_infer.c) ─────────────────────────────────────────────

    private static double BartlettLrv(double[] e, int n, int bw)
    {
        double total = 0.0;
        int t, lag;
        for (t = 0; t < n; t++) total += e[t] * e[t];
        total /= n;
        for (lag = 1; lag <= bw; lag++)
        {
            double cov = 0.0;
            for (t = lag; t < n; t++) cov += e[t] * e[t - lag];
            cov /= n;
            total += 2.0 * (1.0 - (double)lag / (bw + 1.0)) * cov;
        }
        return total;
    }

    public static (double Statistic, int Bandwidth, int Observations, int Deterministic,
        double LongRunVariance, double Critical10, double Critical5, double Critical25, double Critical1)
        Kpss(double[] series, int bandwidth, int deterministic)
    {
        int n = series.Length, t;
        if (n <= 1)
            return (double.NaN, 0, n, deterministic, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        if (bandwidth < 0) bandwidth = (int)Math.Floor(4.0 * Math.Pow((double)n / 100.0, 0.25));
        if (bandwidth >= n) bandwidth = n - 1;
        var e = new double[n];
        if (deterministic == 1)
        {
            double st = 0, sy = 0, stt = 0, sty = 0;
            for (t = 0; t < n; t++) { double tt = t + 1; st += tt; sy += series[t]; stt += tt * tt; sty += tt * series[t]; }
            var b = (n * sty - st * sy) / (n * stt - st * st);
            var a = (sy - b * st) / n;
            for (t = 0; t < n; t++) e[t] = series[t] - a - b * (t + 1);
        }
        else
        {
            double mean = 0;
            for (t = 0; t < n; t++) mean += series[t];
            mean /= n;
            for (t = 0; t < n; t++) e[t] = series[t] - mean;
        }
        double partial = 0, ss = 0;
        for (t = 0; t < n; t++) { partial += e[t]; ss += partial * partial; }
        var lrv = BartlettLrv(e, n, bandwidth);
        var stat = lrv > 0.0 ? ss / ((double)n * n * lrv) : double.NaN;
        if (deterministic == 1)
            return (stat, bandwidth, n, deterministic, lrv, 0.119, 0.146, 0.176, 0.216);
        return (stat, bandwidth, n, deterministic, lrv, 0.347, 0.463, 0.574, 0.739);
    }

    public static (double QlrT, double QlrTilde, int BestIndex) Qlr(double l0, double[] lBeta, double kappaHat)
    {
        if (lBeta == null || lBeta.Length == 0) return (double.NaN, double.NaN, -1);
        var best = 0;
        for (var i = 1; i < lBeta.Length; i++)
            if (lBeta[i] > lBeta[best]) best = i;
        var qlrT = 2.0 * (lBeta[best] - l0);
        var qlrTilde = kappaHat != 0.0 ? qlrT / kappaHat : qlrT;
        return (qlrT, qlrTilde, best);
    }

    // ── Hidden Markov Model gaussiano (parità gs_hmm.c) ────────────────────────────

    private static double HmmLogAdd(double a, double b)
    {
        if (double.IsNegativeInfinity(a)) return b;
        if (double.IsNegativeInfinity(b)) return a;
        var m = a > b ? a : b;
        return m + Math.Log(Math.Exp(a - m) + Math.Exp(b - m));
    }

    public static double HmmForward(double[] lpi, double[] lA, double[] lE)
    {
        var K = lpi.Length;
        if (K < 1 || lE.Length % K != 0 || lA.Length < K * K) return double.NaN;
        var n = lE.Length / K;
        if (n < 1) return double.NaN;
        return TimeSeriesNative.HmmForward(lpi, lA, lE, K, n);
    }

    public static double[] HmmViterbi(double[] lpi, double[] lA, double[] lE)
    {
        var K = lpi.Length;
        if (K < 1 || lE.Length % K != 0 || lA.Length < K * K) return Array.Empty<double>();
        var n = lE.Length / K;
        if (n < 1) return Array.Empty<double>();
        return TimeSeriesNative.HmmViterbi(lpi, lA, lE, K, n);
    }

    public static (double[] Pi, double[] A, double[] Means, double[] Covs, double LogLikelihood)
        HmmFit(double[][] X, int nStates, int nIter)
    {
        var n = X.Length;
        var d = n > 0 ? X[0].Length : 0;
        var K = nStates;
        if (X == null || n < K || d < 1 || K < 1 || K > 32 || d > 32)
            return (Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), double.NaN);
        if (nIter < 1) nIter = 100;
        var logE = new double[n * K];
        var lpi = new double[K];
        var lA = new double[K * K];
        var al = new double[n * K];
        var be = new double[n * K];
        var gam = new double[n * K];
        var xi = new double[n * K * K];
        var pi = new double[K];
        var A = new double[K * K];
        var means = new double[K * d];
        var covs = new double[K * d * d];
        var gmean = new double[d];
        var gcov = new double[d * d];
        int t, i, j, it, k;

        for (i = 0; i < K; i++) { pi[i] = 1.0 / K; for (j = 0; j < K; j++) A[i * K + j] = 1.0 / K; }
        for (k = 0; k < d; k++) { double s = 0; for (t = 0; t < n; t++) s += X[t][k]; gmean[k] = s / n; }
        for (k = 0; k < d; k++)
            for (j = 0; j < d; j++)
            {
                double s = 0;
                for (t = 0; t < n; t++) s += (X[t][k] - gmean[k]) * (X[t][j] - gmean[j]);
                gcov[k * d + j] = s / n;
            }
        for (i = 0; i < K; i++)
        {
            var t0 = (int)((long)i * n / K);
            var t1 = (int)((long)(i + 1) * n / K);
            if (t1 <= t0) t1 = t0 + 1;
            if (t1 > n) t1 = n;
            for (k = 0; k < d; k++)
            {
                double s = 0;
                for (t = t0; t < t1; t++) s += X[t][k];
                means[i * d + k] = s / (t1 - t0);
            }
            for (k = 0; k < d * d; k++) covs[i * d * d + k] = gcov[k];
            for (k = 0; k < d; k++) covs[i * d * d + k * d + k] += 1e-6;
        }

        for (t = 0; t < n; t++)
            for (i = 0; i < K; i++)
                logE[t * K + i] = MvGaussLogPdf(X[t], Slice(means, i * d, d), Slice(covs, i * d * d, d * d));

        for (it = 0; it < nIter; it++)
        {
            for (i = 0; i < K; i++) lpi[i] = Math.Log(pi[i] > 0 ? pi[i] : 1e-300);
            for (i = 0; i < K; i++)
                for (j = 0; j < K; j++)
                    lA[i * K + j] = Math.Log(A[i * K + j] > 0 ? A[i * K + j] : 1e-300);
            for (i = 0; i < K; i++) al[i] = lpi[i] + logE[i];
            for (t = 1; t < n; t++)
                for (j = 0; j < K; j++)
                {
                    var bj = double.NegativeInfinity;
                    for (i = 0; i < K; i++) bj = HmmLogAdd(bj, al[(t - 1) * K + i] + lA[i * K + j]);
                    al[t * K + j] = bj + logE[t * K + j];
                }
            for (i = 0; i < K; i++) be[(n - 1) * K + i] = 0;
            for (t = n - 2; t >= 0; t--)
                for (i = 0; i < K; i++)
                {
                    var bj = double.NegativeInfinity;
                    for (j = 0; j < K; j++)
                        bj = HmmLogAdd(bj, lA[i * K + j] + logE[(t + 1) * K + j] + be[(t + 1) * K + j]);
                    be[t * K + i] = bj;
                }
            for (t = 0; t < n; t++)
            {
                var s = double.NegativeInfinity;
                for (i = 0; i < K; i++) s = HmmLogAdd(s, al[t * K + i] + be[t * K + i]);
                for (i = 0; i < K; i++) gam[t * K + i] = Math.Exp(al[t * K + i] + be[t * K + i] - s);
            }
            for (t = 0; t < n - 1; t++)
            {
                var s = double.NegativeInfinity;
                for (i = 0; i < K; i++)
                    for (j = 0; j < K; j++)
                    {
                        xi[(t * K + i) * K + j] = al[t * K + i] + lA[i * K + j] + logE[(t + 1) * K + j] + be[(t + 1) * K + j];
                        s = HmmLogAdd(s, xi[(t * K + i) * K + j]);
                    }
                for (i = 0; i < K; i++)
                    for (j = 0; j < K; j++)
                        xi[(t * K + i) * K + j] = Math.Exp(xi[(t * K + i) * K + j] - s);
            }
            var newpi = new double[K];
            var newA = new double[K * K];
            var newmeans = new double[K * d];
            var newcovs = new double[K * d * d];
            for (i = 0; i < K; i++)
            {
                double g0 = 0;
                for (t = 0; t < n; t++) g0 += gam[t * K + i];
                newpi[i] = gam[i];
                for (j = 0; j < K; j++)
                {
                    double s = 0;
                    for (t = 0; t < n - 1; t++) s += xi[(t * K + i) * K + j];
                    newA[i * K + j] = s;
                }
                for (k = 0; k < d; k++)
                {
                    double s = 0;
                    for (t = 0; t < n; t++) s += gam[t * K + i] * X[t][k];
                    newmeans[i * d + k] = g0 > 0 ? s / g0 : 0;
                }
                for (k = 0; k < d * d; k++)
                {
                    double s = 0;
                    var kk = k / d;
                    var ll = k % d;
                    for (t = 0; t < n; t++)
                        s += gam[t * K + i] * (X[t][kk] - newmeans[i * d + kk]) * (X[t][ll] - newmeans[i * d + ll]);
                    newcovs[i * d * d + k] = g0 > 0 ? s / g0 + 1e-6 : 1e-6;
                }
            }
            {
                double s = 0;
                for (i = 0; i < K; i++) s += newpi[i];
                for (i = 0; i < K; i++) newpi[i] = s > 0 ? newpi[i] / s : 1.0 / K;
                for (i = 0; i < K; i++)
                {
                    double s2 = 0;
                    for (j = 0; j < K; j++) s2 += newA[i * K + j];
                    for (j = 0; j < K; j++) newA[i * K + j] = s2 > 0 ? newA[i * K + j] / s2 : 1.0 / K;
                }
            }
            Array.Copy(newpi, pi, K);
            Array.Copy(newA, A, K * K);
            Array.Copy(newmeans, means, K * d);
            Array.Copy(newcovs, covs, K * d * d);
            for (t = 0; t < n; t++)
                for (i = 0; i < K; i++)
                    logE[t * K + i] = MvGaussLogPdf(X[t], Slice(means, i * d, d), Slice(covs, i * d * d, d * d));
        }

        for (i = 0; i < K; i++) lpi[i] = Math.Log(pi[i] > 0 ? pi[i] : 1e-300);
        for (i = 0; i < K; i++)
            for (j = 0; j < K; j++)
                lA[i * K + j] = Math.Log(A[i * K + j] > 0 ? A[i * K + j] : 1e-300);
        var loglik = HmmForward(lpi, lA, logE);
        return (pi, A, means, covs, loglik);
    }

    private static double[] Slice(double[] src, int start, int len)
    {
        var r = new double[len];
        Array.Copy(src, start, r, 0, len);
        return r;
    }

    public static double[] BmaWeights(double[] ic)
    {
        var n = ic.Length;
        if (n == 0) return Array.Empty<double>();
        return TimeSeriesNative.BmaWeights(ic);
    }

    public static double LzComplexity(double[] x)
    {
        return TimeSeriesNative.LzComplexity(x);
    }

    public static (double Theta, double Mu, double Sigma, double Phi, double HalfLife) OuFit(double[] x, double dt)
    {
        var n = x.Length;
        if (n < 3 || dt <= 0) return (double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        double meanX = 0, meanY = 0, sxx = 0, sxy = 0, sse = 0;
        var cnt = 0;
        for (var i = 1; i < n; i++)
        {
            double xv = x[i - 1], yv = x[i];
            cnt++;
            var dx = xv - meanX;
            var dy = yv - meanY;
            var inv = 1.0 / cnt;
            meanX += dx * inv; meanY += dy * inv;
            sxx += dx * (xv - meanX); sxy += dx * (yv - meanY);
        }
        var phi = sxx > 0 ? sxy / sxx : double.NaN;
        var alpha = meanY - phi * meanX;
        var logphi = phi > 0 && phi < 1 ? Math.Log(phi) : double.NaN;
        var omphi = -(Math.Exp(logphi) - 1.0);
        var mu = alpha / omphi;
        var theta = -logphi / dt;
        for (var i = 1; i < n; i++) { var r = x[i] - alpha - phi * x[i - 1]; sse += r * r; }
        var vf = -(Math.Exp(2 * logphi) - 1.0) / (2 * theta);
        var sigma = Math.Sqrt(sse / cnt / vf);
        return (theta, mu, sigma, phi, Math.Log(2.0) / theta);
    }

    public static double[] StationaryBootstrap(double[] x, int window, int seed)
    {
        var n = x.Length;
        if (n == 0 || window < 1) return Array.Empty<double>();
        return TimeSeriesNative.StationaryBootstrap(x, window, seed);
    }

    public static double[] Kalman1D(double[] z, double a, double h, double q, double r)
    {
        var n = z.Length;
        if (n == 0) return Array.Empty<double>();
        return TimeSeriesNative.Kalman1D(z, a, h, q, r);
    }

    public static (double Omega, double Alpha, double Beta) Garch11(double[] r)
    {
        var n = r.Length;
        if (n < 5) return (double.NaN, double.NaN, double.NaN);
        double Nll(double[] p)
        {
            double w = p[0], al = p[1], be = p[2], s2 = r[0] * r[0], ll = 0;
            if (s2 <= 0) s2 = 1e-6;
            for (var t = 1; t < n; t++)
            {
                s2 = w + al * r[t - 1] * r[t - 1] + be * s2;
                if (s2 < 1e-12) s2 = 1e-12;
                ll += Math.Log(s2) + r[t] * r[t] / s2;
            }
            return 0.5 * ll;
        }
        var p = new double[3];
        var v = r.Sum(x => x * x) / n;
        p[0] = 0.1 * v; p[1] = 0.1; p[2] = 0.8;
        const double h = 1e-5;
        var step = 0.05;
        for (var it = 0; it < 2000; it++)
        {
            var f0 = Nll(p);
            var grad = new double[3];
            double gn = 0;
            for (var i = 0; i < 3; i++)
            {
                var save = p[i];
                p[i] = save + h;
                grad[i] = (Nll(p) - f0) / h;
                p[i] = save;
                gn += grad[i] * grad[i];
            }
            gn = Math.Sqrt(gn);
            if (gn < 1e-10) break;
            var np = new double[3];
            for (var i = 0; i < 3; i++) np[i] = p[i] - step * grad[i] / gn;
            if (np[0] < 1e-8) np[0] = 1e-8;
            if (np[1] < 0) np[1] = 0;
            if (np[2] < 0) np[2] = 0;
            if (np[1] + np[2] > 0.999) { var ss = 0.999 / (np[1] + np[2]); np[1] *= ss; np[2] *= ss; }
            if (Nll(np) < f0) { p[0] = np[0]; p[1] = np[1]; p[2] = np[2]; step *= 1.05; }
            else step *= 0.5;
            if (step < 1e-12) break;
        }
        return (p[0], p[1], p[2]);
    }

    private static double[] SolveLinear(int n, double[] a, double[] b)
    {
        for (var i = 0; i < n; i++)
        {
            var piv = i;
            for (var r = i + 1; r < n; r++)
                if (Math.Abs(a[r * n + i]) > Math.Abs(a[piv * n + i])) piv = r;
            if (Math.Abs(a[piv * n + i]) < 1e-14) throw new InvalidOperationException("singolare");
            if (piv != i)
            {
                for (var c = i; c < n; c++) (a[i * n + c], a[piv * n + c]) = (a[piv * n + c], a[i * n + c]);
                (b[i], b[piv]) = (b[piv], b[i]);
            }
            var d = a[i * n + i];
            for (var r = i + 1; r < n; r++)
            {
                var f = a[r * n + i] / d;
                for (var c = i; c < n; c++) a[r * n + c] -= f * a[i * n + c];
                b[r] -= f * b[i];
            }
        }
        var x = new double[n];
        for (var i = n - 1; i >= 0; i--)
        {
            var sum = b[i];
            for (var c = i + 1; c < n; c++) sum -= a[i * n + c] * x[c];
            x[i] = sum / a[i * n + i];
        }
        return x;
    }

    public static StatisticalTestResult Granger(double[] x, double[] y, int p)
    {
        var n = Math.Min(x.Length, y.Length);
        if (p < 1 || n < 3 * p + 2)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var m = n - p;
        var nr = p + 1;
        var nu = 2 * p + 1;
        var xr = new double[m * nr];
        var xu = new double[m * nu];
        for (var i = p; i < n; i++)
        {
            var r = i - p;
            xr[r * nr] = 1;
            xu[r * nu] = 1;
            for (var j = 1; j <= p; j++)
            {
                xr[r * nr + j] = x[i - j];
                xu[r * nu + j] = x[i - j];
                xu[r * nu + p + j] = y[i - j];
            }
        }
        double[] Rss(double[] xm, int cols)
        {
            var ata = new double[cols * cols];
            var atb = new double[cols];
            for (var i = 0; i < m; i++)
                for (var j = 0; j < cols; j++)
                {
                    for (var k = 0; k < cols; k++) ata[j * cols + k] += xm[i * cols + j] * xm[i * cols + k];
                    atb[j] += xm[i * cols + j] * x[i + p];
                }
            var coef = SolveLinear(cols, ata, atb);
            var rss = 0.0;
            for (var i = 0; i < m; i++)
            {
                var r = x[i + p];
                for (var j = 0; j < cols; j++) r -= coef[j] * xm[i * cols + j];
                rss += r * r;
            }
            return [rss];
        }
        var rssR = Rss(xr, nr)[0];
        var rssU = Rss(xu, nu)[0];
        if (rssU <= 0) return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var df1 = (double)p;
        var df2 = m - nu;
        var f = ((rssR - rssU) / df1) / (rssU / df2);
        var xb = df1 * f / (df1 * f + df2);
        var pv = 1.0 - BetaCdf(df1 / 2.0, df2 / 2.0, xb);
        return new StatisticalTestResult(f, pv, df1);
    }

    public static double[] FracDiff(double[] x, double d)
    {
        if (d < 0) return Array.Empty<double>();
        return TimeSeriesNative.FracDiff(x, d);
    }

    public static double HurstDfa(double[] x, int minBox = 4)
    {
        var n = x.Length;
        if (n < 20 || minBox < 2) return double.NaN;
        return TimeSeriesNative.HurstDfa(x, minBox);
    }
}
