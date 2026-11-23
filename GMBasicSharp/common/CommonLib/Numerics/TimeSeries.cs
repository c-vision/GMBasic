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
        return TimeSeriesNative.OutliersIqr(data, threshold ?? 1.5);
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
        var flat = TimeSeriesNative.DecomposeMatrix(data, seasonLength);
        var result = new double[3, n];
        for (var j = 0; j < n; j++)
        {
            result[0, j] = flat[j];
            result[1, j] = flat[n + j];
            result[2, j] = flat[2 * n + j];
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
        return TimeSeriesNative.MinMax(series);
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
        return TimeSeriesNative.RollingDelta(data, window, order);
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
        return TimeSeriesNative.RollingSortino(returns, window, riskFreeRate, targetReturn);
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





    public static (double[] EigenValues, double[] TraceStatistic, double[] MaxStatistic, int Observations, int Lags)
        Johansen(double[][] Y, int lags)
    {
        var nObs = Y.Length;
        var k = nObs > 0 ? Y[0].Length : 0;
        var rows = nObs - lags - 1;
        var cc = k * lags + 1;
        if (k < 1 || k > 32 || lags < 0 || rows <= k + cc)
            return (Array.Empty<double>(), Array.Empty<double>(), Array.Empty<double>(), 0, lags);
        return TimeSeriesNative.Johansen(Y, lags);
    }

    // ── nucleo ADF + OLS (parità gs_infer.c) ────────────────────────────────────────




    public static (double Statistic, int Lags, int Observations, int Deterministic, double AlphaBar)
        Dfgls(double[] series, int lags, int deterministic)
    {
        var n = series.Length;
        var nd = deterministic == 1 ? 2 : 1;
        if (n <= nd + lags + 4)
            return (double.NaN, lags, 0, deterministic, double.NaN);
        return TimeSeriesNative.Dfgls(series, lags, deterministic);
    }

    public static (double Statistic, int Lags, int Observations, int NRegressors, int WithConstant)
        EngleGranger(double[][] Y, int dependent, int lags, int withConstant)
    {
        var n = Y.Length;
        var k = n > 0 ? Y[0].Length : 0;
        var cols = (k - 1) + (withConstant != 0 ? 1 : 0);
        if (n <= cols + lags + 2 || k < 2 || dependent < 0 || dependent >= k)
            return (double.NaN, lags, 0, cols, withConstant);
        return TimeSeriesNative.EngleGranger(Y, dependent, lags, withConstant);
    }

    // ── KPSS e QLR (parità gs_infer.c) ─────────────────────────────────────────────

    public static (double Statistic, int Bandwidth, int Observations, int Deterministic,
        double LongRunVariance, double Critical10, double Critical5, double Critical25, double Critical1)
        Kpss(double[] series, int bandwidth, int deterministic)
    {
        var n = series.Length;
        if (n <= 1)
            return (double.NaN, 0, n, deterministic, double.NaN, double.NaN, double.NaN, double.NaN, double.NaN);
        return TimeSeriesNative.Kpss(series, bandwidth, deterministic);
    }

    public static (double QlrT, double QlrTilde, int BestIndex) Qlr(double l0, double[] lBeta, double kappaHat)
    {
        if (lBeta == null || lBeta.Length == 0) return (double.NaN, double.NaN, -1);
        return TimeSeriesNative.Qlr(l0, lBeta, kappaHat);
    }

    // ── Hidden Markov Model gaussiano (parità gs_hmm.c) ────────────────────────────

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
        return TimeSeriesNative.HmmFit(X, K, nIter);
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
        return TimeSeriesNative.OuFit(x, dt);
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
        return TimeSeriesNative.Garch11(r);
    }


    public static StatisticalTestResult Granger(double[] x, double[] y, int p)
    {
        var n = Math.Min(x.Length, y.Length);
        if (p < 1 || n < 3 * p + 2)
            return new StatisticalTestResult(double.NaN, double.NaN, double.NaN);
        var (stat, pv, df) = TimeSeriesNative.Granger(x, y, p);
        return new StatisticalTestResult(stat, pv, df);
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
    // Condivisi con gli altri file della classe partial (Stats.cs, Fitting.cs, SciPyP1.cs).
    /// <summary>Risolve A·x = b con il solver del runtime (soglia 1e-300). A/b non sono modificati.</summary>
    private static bool InferSolve(int n, double[] A, double[] b, double[] x)
    {
        return MatrixNativeKernels.SolveLinear(A, b, x, 1e-300);
    }


    /// <summary>Risolve A·x = b con il solver del runtime (soglia 1e-14); singolare → eccezione.</summary>
    private static double[] SolveLinear(int n, double[] a, double[] b)
    {
        var x = new double[n];
        if (!MatrixNativeKernels.SolveLinear(a, b, x, 1e-14))
            throw new InvalidOperationException("singolare");
        return x;
    }

}
