using MathNet.Numerics.Statistics;

namespace CommonLib.Trading;

/// <summary>
/// Metriche di performance e rischio per portafogli e strategie di investimento.
/// Fornisce calcoli per rendimenti, indici di Sharpe/Sortino/Calmar/Treynor,
/// Value at Risk (VaR), Conditional VaR (CVaR), drawdown, volatilità e test statistici.
///
/// Metodi:
/// - <see cref="TotalReturn"/>: rendimento totale composto.
/// - <see cref="AnnualizedReturn"/>: rendimento annualizzato.
/// - <see cref="SharpeRatio"/>: indice di Sharpe.
/// - <see cref="SortinoRatio"/>: indice di Sortino.
/// - <see cref="CalmarRatio"/>: indice di Calmar.
/// - <see cref="InformationRatio"/>: information ratio.
/// - <see cref="TreynorRatio"/>: indice di Treynor.
/// - <see cref="MaxDrawdown"/>: massimo drawdown.
/// - <see cref="DownsideDeviation"/>: deviazione downside.
/// - <see cref="ValueAtRisk"/>: Value at Risk (VaR).
/// - <see cref="ConditionalValueAtRisk"/>: Conditional VaR (CVaR / Expected Shortfall).
/// - <see cref="Beta"/>: beta di mercato.
/// - <see cref="Alpha"/>: alpha di Jensen.
/// - <see cref="ReturnsSkewness"/>: asimmetria dei rendimenti.
/// - <see cref="ReturnsKurtosis"/>: curtosi dei rendimenti.
/// - <see cref="JarqueBeraTest"/>: test di normalità di Jarque-Bera.
/// - <see cref="AnnualizedVolatility"/>: volatilità annualizzata.
/// - <see cref="AnnualizedTrackingError"/>: tracking error annualizzato.
/// - <see cref="AnnualizedDownsideDeviation"/>: deviazione downside annualizzata.
/// - <see cref="OmegaRatio"/>: omega ratio.
/// - <see cref="UlcerIndex"/>: ulcer index.
/// </summary>
public static class Metrics
{
    /// <summary>
    /// Calcola il rendimento totale composto di una serie di rendimenti.
    /// Formula: prod(1 + r_i) - 1.
    /// </summary>
    /// <param name="returns">Array dei rendimenti periodici.</param>
    /// <returns>Rendimento totale cumulativo.</returns>
    /// <example>
    /// <code>var tr = Metrics.TotalReturn(new[] { 0.01, -0.005, 0.02 });</code>
    /// </example>
    public static double TotalReturn(double[] returns)
    {
        if (returns.Length == 0) return 0.0;
        return returns.Select(r => 1 + r).Aggregate(1.0, (a, b) => a * b) - 1.0;
    }

    /// <summary>
    /// Calcola il rendimento annualizzato a partire da una serie di rendimenti periodici.
    /// </summary>
    /// <param name="returns">Array dei rendimenti periodici.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno (es. 12 per mensile, 252 per giornaliero).</param>
    /// <returns>Rendimento annualizzato.</returns>
    /// <example>
    /// <code>var ar = Metrics.AnnualizedReturn(rendimentiMensili, 12);</code>
    /// </example>
    public static double AnnualizedReturn(double[] returns, int periodsPerYear)
    {
        if (returns.Length == 0) return 0.0;
        var totalReturn = TotalReturn(returns);
        return Math.Pow(1 + totalReturn, (double)periodsPerYear / returns.Length) - 1.0;
    }

    /// <summary>
    /// Calcola l'indice di Sharpe: rendimento in eccesso per unità di rischio totale.
    /// Formula: sqrt(periodsPerYear) × media(excessReturns) / std(excessReturns).
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio per periodo.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Indice di Sharpe annualizzato.</returns>
    /// <example>
    /// <code>var sharpe = Metrics.SharpeRatio(rendimenti, 0.0, 12);</code>
    /// </example>
    public static double SharpeRatio(double[] returns, double riskFreeRate, int periodsPerYear)
    {
        if (returns.Length < 2) return 0.0;
        var excessReturns = returns.Select(r => r - riskFreeRate).ToArray();
        var stdDev = excessReturns.StandardDeviation();
        if (stdDev == 0) return 0.0;
        return Math.Sqrt(periodsPerYear) * excessReturns.Mean() / stdDev;
    }

    /// <summary>
    /// Calcola l'indice di Sortino: come lo Sharpe ma utilizzando la sola deviazione downside.
    /// Penalizza solo la volatilità negativa.
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio per periodo.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Indice di Sortino annualizzato.</returns>
    /// <example>
    /// <code>var sortino = Metrics.SortinoRatio(rendimenti, 0.0, 12);</code>
    /// </example>
    public static double SortinoRatio(double[] returns, double riskFreeRate, int periodsPerYear)
    {
        if (returns.Length < 2) return 0.0;
        var excessReturns = returns.Select(r => r - riskFreeRate).ToArray();
        var downsideDev = DownsideDeviation(excessReturns, 0);
        if (downsideDev == 0) return 0.0;
        return Math.Sqrt(periodsPerYear) * excessReturns.Mean() / downsideDev;
    }

    /// <summary>
    /// Calcola l'indice di Calmar: rapporto tra rendimento annualizzato e massimo drawdown in valore assoluto.
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Indice di Calmar annualizzato.</returns>
    /// <example>
    /// <code>var calmar = Metrics.CalmarRatio(rendimenti, 12);</code>
    /// </example>
    public static double CalmarRatio(double[] returns, int periodsPerYear)
    {
        if (returns.Length == 0) return 0.0;
        var maxDrawdown = MaxDrawdown(returns);
        if (maxDrawdown == 0) return 0.0;
        return AnnualizedReturn(returns, periodsPerYear) / Math.Abs(maxDrawdown);
    }

    /// <summary>
    /// Calcola l'Information Ratio: rendimento attivo (rispetto al benchmark) diviso per il tracking error.
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="benchmarkReturns">Array dei rendimenti del benchmark.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Information ratio annualizzato.</returns>
    /// <example>
    /// <code>var ir = Metrics.InformationRatio(ptfRend, bmkRend, 12);</code>
    /// </example>
    public static double InformationRatio(double[] returns, double[] benchmarkReturns, int periodsPerYear)
    {
        if (returns.Length != benchmarkReturns.Length || returns.Length < 2) return 0.0;
        var activeReturns = returns.Zip(benchmarkReturns, (r, b) => r - b).ToArray();
        var trackingError = activeReturns.StandardDeviation();
        if (trackingError == 0) return 0.0;
        return Math.Sqrt(periodsPerYear) * activeReturns.Mean() / trackingError;
    }

    /// <summary>
    /// Calcola l'indice di Treynor: rendimento in eccesso per unità di rischio sistematico (beta).
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="benchmarkReturns">Array dei rendimenti del benchmark di mercato.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio per periodo.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Indice di Treynor annualizzato.</returns>
    /// <example>
    /// <code>var treynor = Metrics.TreynorRatio(ptfRend, bmkRend, 0.0, 12);</code>
    /// </example>
    public static double TreynorRatio(double[] returns, double[] benchmarkReturns, double riskFreeRate,
        int periodsPerYear)
    {
        if (returns.Length < 2) return 0.0;
        var beta = Beta(returns, benchmarkReturns);
        if (Math.Abs(beta) < 1e-9) return 0.0;
        var annualizedExcessReturn = AnnualizedReturn(returns, periodsPerYear) -
                                      AnnualizedReturn(Enumerable.Repeat(riskFreeRate, returns.Length).ToArray(),
                                          periodsPerYear);
        return annualizedExcessReturn / beta;
    }

    /// <summary>
    /// Calcola il massimo drawdown (perdita massima dal picco al minimo) di una serie di rendimenti.
    /// </summary>
    /// <param name="returns">Array dei rendimenti periodici.</param>
    /// <returns>Massimo drawdown (valore negativo, es. -0.25 per -25%).</returns>
    /// <example>
    /// <code>var mdd = Metrics.MaxDrawdown(rendimenti);</code>
    /// </example>
    public static double MaxDrawdown(double[] returns)
    {
        if (returns.Length == 0) return 0.0;
        var cumulative = new double[returns.Length];
        var peak = new double[returns.Length];
        cumulative[0] = 1 + returns[0];
        peak[0] = cumulative[0];
        for (var i = 1; i < returns.Length; i++)
        {
            cumulative[i] = cumulative[i - 1] * (1 + returns[i]);
            peak[i] = Math.Max(peak[i - 1], cumulative[i]);
        }

        var drawdown = cumulative.Zip(peak, (c, p) => (c - p) / p).ToArray();
        return drawdown.Min();
    }

    /// <summary>
    /// Calcola la deviazione downside (semi-devianza) dei rendimenti al di sotto di una soglia.
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <param name="threshold">Soglia (tipicamente 0 per rendimenti negativi).</param>
    /// <returns>Deviazione downside.</returns>
    /// <example>
    /// <code>var dd = Metrics.DownsideDeviation(rendimenti, 0);</code>
    /// </example>
    public static double DownsideDeviation(double[] returns, double threshold)
    {
        if (returns.Length == 0) return 0.0;
        var downsideReturns = returns.Where(r => r < threshold).Select(r => Math.Pow(r - threshold, 2)).ToArray();
        return downsideReturns.Length > 0 ? Math.Sqrt(downsideReturns.Average()) : 0.0;
    }

    /// <summary>
    /// Calcola il Value at Risk (VaR) a un dato livello di confidenza.
    /// Il VaR rappresenta la perdita massima attesa con una probabilità pari al livello di confidenza.
    /// </summary>
    /// <param name="returns">Array dei rendimenti storici o simulati.</param>
    /// <param name="confidenceLevel">Livello di confidenza (es. 0.95 per 95%).</param>
    /// <returns>Value at Risk (valore negativo, es. -0.02 per -2%).</returns>
    /// <example>
    /// <code>var var95 = Metrics.ValueAtRisk(rendimenti, 0.95);</code>
    /// </example>
    public static double ValueAtRisk(double[] returns, double confidenceLevel)
    {
        if (returns.Length == 0) return 0.0;
        var percentileTarget = (int)Math.Round((1 - confidenceLevel) * 100);
        var sortedReturns = returns.OrderBy(x => x).ToArray();
        return sortedReturns.Percentile(percentileTarget);
    }

    /// <summary>
    /// Calcola il Conditional Value at Risk (CVaR / Expected Shortfall):
    /// media delle perdite oltre la soglia del VaR.
    /// </summary>
    /// <param name="returns">Array dei rendimenti storici o simulati.</param>
    /// <param name="confidenceLevel">Livello di confidenza (es. 0.95 per 95%).</param>
    /// <returns>Expected Shortfall (valore negativo).</returns>
    /// <example>
    /// <code>var cvar95 = Metrics.ConditionalValueAtRisk(rendimenti, 0.95);</code>
    /// </example>
    public static double ConditionalValueAtRisk(double[] returns, double confidenceLevel)
    {
        if (returns.Length == 0) return 0.0;
        var var = ValueAtRisk(returns, confidenceLevel);
        var tailLosses = returns.Where(r => r <= var).ToArray();
        return tailLosses.Length > 0 ? tailLosses.Mean() : 0.0;
    }

    /// <summary>
    /// Calcola il beta di un portafoglio/asset rispetto a un benchmark di mercato.
    /// Il beta misura la sensibilità dei rendimenti dell'asset ai movimenti del mercato.
    /// </summary>
    /// <param name="returns">Array dei rendimenti dell'asset/portafoglio.</param>
    /// <param name="benchmarkReturns">Array dei rendimenti del benchmark.</param>
    /// <returns>Coefficiente beta.</returns>
    /// <example>
    /// <code>var beta = Metrics.Beta(rendimentiAsset, rendimentiMercato);</code>
    /// </example>
    public static double Beta(double[] returns, double[] benchmarkReturns)
    {
        if (returns.Length != benchmarkReturns.Length || returns.Length < 2) return 0.0;
        var covariance = returns.Covariance(benchmarkReturns);
        var benchmarkVariance = benchmarkReturns.Variance();
        return benchmarkVariance == 0 ? 0.0 : covariance / benchmarkVariance;
    }

    /// <summary>
    /// Calcola l'alpha di Jensen: rendimento in eccesso rispetto al CAPM.
    /// Un alpha positivo indica sovraperformance rispetto al rischio assunto.
    /// </summary>
    /// <param name="returns">Array dei rendimenti dell'asset/portafoglio.</param>
    /// <param name="benchmarkReturns">Array dei rendimenti del benchmark.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio per periodo.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Alpha annualizzato.</returns>
    /// <example>
    /// <code>var alpha = Metrics.Alpha(ptfRend, bmkRend, 0.0, 12);</code>
    /// </example>
    public static double Alpha(double[] returns, double[] benchmarkReturns, double riskFreeRate,
        int periodsPerYear)
    {
        if (returns.Length != benchmarkReturns.Length || returns.Length < 2) return 0.0;
        var beta = Beta(returns, benchmarkReturns);
        var annualizedReturn = AnnualizedReturn(returns, periodsPerYear);
        var annualizedBenchmarkReturn = AnnualizedReturn(benchmarkReturns, periodsPerYear);
        var annualizedRiskFree = Math.Pow(1 + riskFreeRate, periodsPerYear) - 1;
        return annualizedReturn - (annualizedRiskFree + beta * (annualizedBenchmarkReturn - annualizedRiskFree));
    }

    /// <summary>
    /// Calcola l'asimmetria (skewness) della distribuzione dei rendimenti.
    /// Skewness negativa indica una coda sinistra più pesante (rischio di perdite estreme).
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <returns>Skewness campionaria.</returns>
    /// <example>
    /// <code>var skew = Metrics.ReturnsSkewness(rendimenti);</code>
    /// </example>
    public static double ReturnsSkewness(double[] returns)
    {
        if (returns.Length < 3) return 0.0;
        return returns.Skewness();
    }

    /// <summary>
    /// Calcola la curtosi (kurtosis) della distribuzione dei rendimenti.
    /// Curtosi maggiore di 3 indica code pesanti (rischio di eventi estremi).
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <returns>Curtosi campionaria (eccesso rispetto alla normale).</returns>
    /// <example>
    /// <code>var kurt = Metrics.ReturnsKurtosis(rendimenti);</code>
    /// </example>
    public static double ReturnsKurtosis(double[] returns)
    {
        if (returns.Length < 4) return 0.0;
        return returns.Kurtosis();
    }

    /// <summary>
    /// Esegue il test di normalità di Jarque-Bera sulla distribuzione dei rendimenti.
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <returns>Array di due elementi: [statistica JB, p-value].</returns>
    /// <example>
    /// <code>
    /// var jb = Metrics.JarqueBeraTest(rendimenti);
    /// Console.WriteLine($"JB={jb[0]}, p-value={jb[1]}");
    /// </code>
    /// </example>
    public static double[] JarqueBeraTest(double[] returns)
    {
        if (returns.Length < 4)
            return [0.0, 1.0];
        var n = returns.Length;
        var s = returns.Skewness();
        var k = returns.Kurtosis() + 3;
        var jbStatistic = (n / 6.0) * (Math.Pow(s, 2) + (Math.Pow(k - 3, 2) / 4.0));
        var pValue = 1.0 - MathNet.Numerics.Distributions.ChiSquared.CDF(2, jbStatistic);
        return [jbStatistic, pValue];
    }

    /// <summary>
    /// Calcola la volatilità annualizzata: deviazione standard dei rendimenti annualizzata.
    /// </summary>
    /// <param name="returns">Array dei rendimenti periodici.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Volatilità annualizzata.</returns>
    /// <example>
    /// <code>var vol = Metrics.AnnualizedVolatility(rendimentiMensili, 12);</code>
    /// </example>
    public static double AnnualizedVolatility(double[] returns, int periodsPerYear)
    {
        if (returns.Length < 2) return 0.0;
        return Math.Sqrt(periodsPerYear) * returns.StandardDeviation();
    }

    /// <summary>
    /// Calcola il tracking error annualizzato: deviazione standard annualizzata della differenza
    /// tra rendimenti del portafoglio e rendimenti del benchmark.
    /// </summary>
    /// <param name="returns">Array dei rendimenti del portafoglio.</param>
    /// <param name="benchmarkReturns">Array dei rendimenti del benchmark.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Tracking error annualizzato.</returns>
    /// <example>
    /// <code>var te = Metrics.AnnualizedTrackingError(ptfRend, bmkRend, 12);</code>
    /// </example>
    public static double AnnualizedTrackingError(double[] returns, double[] benchmarkReturns, int periodsPerYear)
    {
        if (returns.Length != benchmarkReturns.Length || returns.Length < 2) return 0.0;
        var diff = returns.Zip(benchmarkReturns, (r, b) => r - b).ToArray();
        return Math.Sqrt(periodsPerYear) * diff.StandardDeviation();
    }

    /// <summary>
    /// Calcola la deviazione downside annualizzata.
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <param name="threshold">Soglia per la definizione di downside.</param>
    /// <param name="periodsPerYear">Numero di periodi per anno.</param>
    /// <returns>Deviazione downside annualizzata.</returns>
    /// <example>
    /// <code>var add = Metrics.AnnualizedDownsideDeviation(rendimenti, 0, 12);</code>
    /// </example>
    public static double AnnualizedDownsideDeviation(double[] returns, double threshold, int periodsPerYear)
    {
        return Math.Sqrt(periodsPerYear) * DownsideDeviation(returns, threshold);
    }

    /// <summary>
    /// Calcola l'Omega Ratio: rapporto tra guadagni e perdite al di sopra/sotto di una soglia.
    /// Un valore maggiore di 1 indica prevalenza di guadagni sulle perdite.
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <param name="threshold">Soglia (tipicamente 0).</param>
    /// <returns>Omega ratio. PositiveInfinity se non ci sono perdite.</returns>
    /// <example>
    /// <code>var omega = Metrics.OmegaRatio(rendimenti, 0);</code>
    /// </example>
    public static double OmegaRatio(double[] returns, double threshold)
    {
        if (returns.Length == 0) return 0.0;
        var gains = returns.Where(r => r > threshold).Select(r => r - threshold).Sum();
        var losses = -returns.Where(r => r < threshold).Select(r => r - threshold).Sum();
        return losses == 0 ? double.PositiveInfinity : gains / losses;
    }

    /// <summary>
    /// Calcola l'Ulcer Index: misura della profondità e durata dei drawdown.
    /// Radice quadrata della media dei drawdown quadratici dal picco.
    /// </summary>
    /// <param name="returns">Array dei rendimenti.</param>
    /// <returns>Ulcer index (più alto = drawdown più profondi e prolungati).</returns>
    /// <example>
    /// <code>var ui = Metrics.UlcerIndex(rendimenti);</code>
    /// </example>
    public static double UlcerIndex(double[] returns)
    {
        if (returns.Length == 0) return 0.0;
        var cumulative = new double[returns.Length];
        cumulative[0] = 1 + returns[0];
        for (var i = 1; i < returns.Length; i++)
            cumulative[i] = cumulative[i - 1] * (1 + returns[i]);
        var maxSoFar = cumulative[0];
        var sumSq = 0.0;
        var count = 0;
        for (var i = 0; i < returns.Length; i++)
        {
            if (cumulative[i] > maxSoFar) maxSoFar = cumulative[i];
            var dd = (cumulative[i] - maxSoFar) / maxSoFar * 100.0;
            sumSq += dd * dd;
            count++;
        }
        return count > 0 ? Math.Sqrt(sumSq / count) : 0.0;
    }
}
