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
        var result = new double[data.Length - window + 1];
        var sum = 0.0;
        for (var i = 0; i < window; i++) sum += data[i];
        result[0] = sum / window;
        for (var i = window; i < data.Length; i++)
        {
            sum += data[i] - data[i - window];
            result[i - window + 1] = sum / window;
        }
        return result;
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
        var ema = new double[data.Length];
        ema[0] = data[0];
        for (var i = 1; i < data.Length; i++)
            ema[i] = alpha * data[i] + (1 - alpha) * ema[i - 1];
        return ema;
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
        var outlierIdx = new List<int>();
        if (method == "Z")
        {
            var th = threshold ?? 3.0;
            var mean = data.Average();
            var std = Math.Sqrt(data.Average(v => (v - mean) * (v - mean)));
            for (var i = 0; i < data.Length; i++)
            {
                var z = std > 0 ? Math.Abs((data[i] - mean) / std) : 0.0;
                if (z > th) outlierIdx.Add(i);
            }
        }
        else if (method == "IQR")
        {
            var th = threshold ?? 1.5;
            var sorted = data.OrderBy(x => x).ToArray();
            var n = sorted.Length;
            var Q1 = sorted[n / 4];
            var Q3 = sorted[(3 * n) / 4];
            var iqr = Q3 - Q1;
            var low = Q1 - th * iqr;
            var high = Q3 + th * iqr;
            for (var i = 0; i < data.Length; i++)
                if (data[i] < low || data[i] > high)
                    outlierIdx.Add(i);
        }
        else
        {
            throw new ArgumentException("Metodo non supportato: " + method);
        }
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
        var result = data.ToArray();
        for (var k = 0; k < order; k++)
        {
            if (result.Length < 2) return [];
            var diff = new double[result.Length - 1];
            for (var i = 1; i < result.Length; i++)
                diff[i - 1] = result[i] - result[i - 1];
            result = diff;
        }
        return result;
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
        var result = new double[data.Length - 1];
        for (var i = 1; i < data.Length; i++)
        {
            if (log)
                result[i - 1] = Math.Log(data[i] / data[i - 1]);
            else
                result[i - 1] = (data[i] - data[i - 1]) / data[i - 1];
        }
        return result;
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
        var returns = Returns(data, type);
        if (returns.Length == 0) return 0.0;
        var mean = returns.Average();
        var var = returns.Average(x => (x - mean) * (x - mean));
        return Math.Sqrt(var);
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
        var mean = data.Average();
        double num = 0.0, den = 0.0;
        for (var i = lag; i < data.Length; i++)
            num += (data[i] - mean) * (data[i - lag] - mean);
        foreach (var t in data)
            den += (t - mean) * (t - mean);
        return den != 0.0 ? num / den : 0.0;
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
        var season = new double[data.Length];
        var half = seasonLength / 2;
        for (var i = 0; i < data.Length; i++)
        {
            var left = Math.Max(0, i - half);
            var right = Math.Min(data.Length - 1, i + half);
            double sum = 0;
            var count = 0;
            for (var j = left; j <= right; j++)
            {
                sum += data[j];
                count++;
            }
            season[i] = sum / count;
        }
        var destag = new double[data.Length];
        for (var i = 0; i < data.Length; i++)
            destag[i] = data[i] - season[i];
        return destag;
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
        var n = data.Length;
        var m = data[0].Length;
        var means = new double[m];
        for (var j = 0; j < m; j++)
            means[j] = data.Average(row => row[j]);
        var cov = new double[m, m];
        for (var i = 0; i < m; i++)
        for (var j = 0; j < m; j++)
        {
            double sum = 0;
            for (var k = 0; k < n; k++)
                sum += (data[k][i] - means[i]) * (data[k][j] - means[j]);
            cov[i, j] = sum / (n - 1);
        }
        return cov;
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
        var result = new double[data.Length - window + 1];
        var sum = 0.0;
        for (var i = 0; i < window; i++) sum += data[i];
        result[0] = sum / window;
        for (var i = window; i < data.Length; i++)
        {
            sum += data[i] - data[i - window];
            result[i - window + 1] = sum / window;
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i <= data.Length - window; i++)
        {
            var mean = 0.0;
            for (var j = 0; j < window; j++) mean += data[i + j];
            mean /= window;
            var var = 0.0;
            for (var j = 0; j < window; j++) var += (data[i + j] - mean) * (data[i + j] - mean);
            var /= window;
            result[i] = Math.Sqrt(var);
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var mean = slice.Average();
            double n = slice.Length;
            var std = Math.Sqrt(slice.Sum(x => (x - mean) * (x - mean)) / (n - 1));
            if (std < 1e-12)
            {
                result[i] = 0.0;
                continue;
            }
            var sum3 = slice.Sum(x => Math.Pow(x - mean, 3));
            result[i] = (n / ((n - 1) * (n - 2))) * (sum3 / Math.Pow(std, 3));
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var mean = slice.Average();
            double n = slice.Length;
            var std2 = slice.Sum(x => (x - mean) * (x - mean)) / (n - 1);
            if (std2 < 1e-12)
            {
                result[i] = 0.0;
                continue;
            }
            var sum4 = slice.Sum(x => Math.Pow(x - mean, 4));
            var kurtosis = (n * (n + 1) * sum4) / ((n - 1) * (n - 2) * (n - 3) * Math.Pow(std2, 2))
                           - (3 * Math.Pow(n - 1, 2)) / ((n - 2) * (n - 3));
            result[i] = kurtosis;
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            Array.Sort(slice);
            var mid = window / 2;
            result[i] = (window % 2 == 0) ? (slice[mid - 1] + slice[mid]) / 2.0 : slice[mid];
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            Array.Sort(slice);
            var idx = quantile * (window - 1);
            var idxLow = (int)Math.Floor(idx);
            var idxHigh = (int)Math.Ceiling(idx);
            if (idxLow == idxHigh) result[i] = slice[idxLow];
            else result[i] = slice[idxLow] + (slice[idxHigh] - slice[idxLow]) * (idx - idxLow);
        }
        return result;
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
        var result = new double[returns.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(returns, i, window).ToArray();
            var mean = slice.Average() - riskFreeRate;
            var std = Math.Sqrt(slice.Average(r => Math.Pow(r - slice.Average(), 2)));
            result[i] = std != 0.0 ? mean / std : 0.0;
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var median = GetMedian(slice);
            var absDev = slice.Select(x => Math.Abs(x - median)).ToArray();
            result[i] = GetMedian(absDev);
        }
        return result;
        static double GetMedian(double[] arr)
        {
            Array.Sort(arr);
            var mid = arr.Length / 2;
            if (arr.Length % 2 == 0)
                return (arr[mid - 1] + arr[mid]) / 2.0;
            return arr[mid];
        }
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var mean = slice.Average();
            var std = Math.Sqrt(slice.Average(x => (x - mean) * (x - mean)));
            result[i] = std < 1e-12 ? 0.0 : (slice.Last() - mean) / std;
        }
        return result;
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
        var result = new double[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(data, i, window).ToArray();
            var mean = slice.Average();
            double num = 0.0, den = 0.0;
            for (var j = lag; j < window; j++)
                num += (slice[j] - mean) * (slice[j - lag] - mean);
            for (var j = 0; j < window; j++)
                den += (slice[j] - mean) * (slice[j] - mean);
            result[i] = den != 0.0 ? num / den : 0.0;
        }
        return result;
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
        var result = new double[x.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var xw = new ArraySegment<double>(x, i, window).ToArray();
            var yw = new ArraySegment<double>(y, i, window).ToArray();
            var xm = xw.Average();
            var ym = yw.Average();
            var sum = 0.0;
            for (var j = 0; j < window; j++)
                sum += (xw[j] - xm) * (yw[j] - ym);
            result[i] = sum / (window - 1);
        }
        return result;
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
        var result = new (double slope, double intercept)[y.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            var slice = new ArraySegment<double>(y, i, window).ToArray();
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (var j = 0; j < window; j++)
            {
                sx += j;
                sy += slice[j];
                sxx += j * j;
                sxy += j * slice[j];
            }
            var denom = window * sxx - sx * sx;
            var slope = denom == 0 ? 0.0 : (window * sxy - sx * sy) / denom;
            var intercept = window == 0 ? 0.0 : (sy - slope * sx) / window;
            result[i] = (slope, intercept);
        }
        return result;
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
        var n = Math.Min(x.Length, y.Length);
        var result = new (double slope, double intercept)[n - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (var j = 0; j < window; j++)
            {
                double xv = x[i + j], yv = y[i + j];
                sx += xv; sy += yv; sxx += xv * xv; sxy += xv * yv;
            }
            var denom = window * sxx - sx * sx;
            var slope = denom == 0 ? 0.0 : (window * sxy - sx * sy) / denom;
            var intercept = window == 0 ? 0.0 : (sy - slope * sx) / window;
            result[i] = (slope, intercept);
        }
        return result;
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
        var result = new (double min, double max)[data.Length - window + 1];
        for (var i = 0; i < result.Length; i++)
        {
            double min = double.MaxValue, max = double.MinValue;
            for (var j = 0; j < window; j++)
            {
                var v = data[i + j];
                if (v < min) min = v;
                if (v > max) max = v;
            }
            result[i] = (min, max);
        }
        return result;
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
        var n = x.Length - window + 1;
        var result = new double[n];
        for (var i = 0; i < n; i++)
        {
            var xw = new ArraySegment<double>(x, i, window).ToArray();
            var yw = new ArraySegment<double>(y, i, window).ToArray();
            result[i] = MathNet.Numerics.Statistics.Correlation.Pearson(xw, yw);
        }
        return result;
    }

    /// <summary>Hurst exponent via DFA (gmbstats gs_hurst_dfa).</summary>
    public static double HurstDfa(double[] x, int minBox = 4)
    {
        var n = x.Length;
        if (n < 20 || minBox < 2) return double.NaN;
        var y = new double[n];
        var mean = x.Average();
        y[0] = x[0] - mean;
        for (var i = 1; i < n; i++) y[i] = y[i - 1] + (x[i] - mean);
        var ls = new List<double>();
        var lx = new List<double>();
        for (var t = minBox; t <= n / 2; t *= 2)
        {
            var nboxes = n / t;
            double f = 0;
            for (var j = 0; j < nboxes; j++)
            {
                double mx = 0, my = 0, sxy = 0, sxx = 0;
                for (var k = 0; k < t; k++) { mx += k; my += y[j * t + k]; }
                mx /= t; my /= t;
                for (var k = 0; k < t; k++)
                {
                    var dx = k - mx;
                    var dy = y[j * t + k] - my;
                    sxy += dx * dy; sxx += dx * dx;
                }
                var slope = sxx > 0 ? sxy / sxx : 0;
                var inter = my - slope * mx;
                for (var k = 0; k < t; k++)
                {
                    var r = y[j * t + k] - (slope * k + inter);
                    f += r * r;
                }
            }
            f = Math.Sqrt(f / (nboxes * t));
            ls.Add(Math.Log(t));
            lx.Add(Math.Log(f));
        }
        var m1 = ls.Average();
        var m2 = lx.Average();
        double cov = 0, var1 = 0;
        for (var i = 0; i < ls.Count; i++)
        {
            cov += (ls[i] - m1) * (lx[i] - m2);
            var1 += (ls[i] - m1) * (ls[i] - m1);
        }
        return var1 > 0 ? cov / var1 : double.NaN;
    }
}
