using MathNet.Numerics.Statistics;

namespace CommonLib.Trading;

/// <summary>
/// Motore di backtesting per strategie di investimento. Esegue simulazioni storiche
/// su una matrice di rendimenti applicando strategie di ribilanciamento periodico o momentum.
///
/// Metodi:
/// - <see cref="RunBacktest"/>: esegue il backtest completo e restituisce rendimenti, pesi finali e metriche.
/// </summary>
public static class Backtest
{
    /// <summary>
    /// Esegue un backtest sulla matrice dei rendimenti storici applicando la strategia di ribilanciamento specificata.
    /// Simula l'evoluzione del portafoglio periodo per periodo, applicando il ribilanciamento secondo la strategia
    /// e calcolando le metriche di performance finali.
    /// </summary>
    /// <param name="historicalReturns">Matrice bidimensionale [periodi × asset] dei rendimenti storici.</param>
    /// <param name="strategy">Strategia di ribilanciamento da applicare (tipo, periodo, finestra lookback).</param>
    /// <param name="initialWeights">Pesi iniziali del portafoglio. Se null, viene usata la ponderazione equa (1/N).</param>
    /// <returns>Un oggetto <see cref="BacktestResult"/> contenente la serie dei rendimenti, i pesi finali e le metriche.</returns>
    /// <exception cref="ArgumentException">Sollevata se la matrice dei rendimenti è vuota.</exception>
    /// <example>
    /// <code>
    /// var rendimenti = new double[12, 3] { /* dati mensili su 3 asset */ };
    /// var strategia = new RebalanceStrategy("PERIODIC", 3, 0, 0, 0);
    /// var risultato = Backtest.RunBacktest(rendimenti, strategia, null);
    /// Console.WriteLine($"Sharpe: {risultato.Metrics.SharpeRatio}");
    /// </code>
    /// </example>
    public static BacktestResult RunBacktest(double[,] historicalReturns, RebalanceStrategy strategy, double[]? initialWeights)
    {
        var numPeriods = historicalReturns.GetLength(0);
        var numAssets = historicalReturns.GetLength(1);

        if (numPeriods == 0 || numAssets == 0)
            throw new ArgumentException("La matrice dei rendimenti storici non può essere vuota.");

        var portfolioReturns = new List<double>();
        var currentWeights = new double[numAssets];

        if (initialWeights != null && initialWeights.Length == numAssets)
        {
            Array.Copy(initialWeights, currentWeights, numAssets);
        }
        else
        {
            for (var i = 0; i < numAssets; i++) currentWeights[i] = 1.0 / numAssets;
        }

        for (var t = 0; t < numPeriods; t++)
        {
            var periodReturn = 0.0;
            for (var i = 0; i < numAssets; i++)
            {
                periodReturn += currentWeights[i] * historicalReturns[t, i];
            }
            portfolioReturns.Add(periodReturn);

            var marketValues = new double[numAssets];
            var totalMarketValue = 0.0;
            for (var i = 0; i < numAssets; i++)
            {
                marketValues[i] = currentWeights[i] * (1 + historicalReturns[t, i]);
                totalMarketValue += marketValues[i];
            }
            if (totalMarketValue > 0)
            {
                for (var i = 0; i < numAssets; i++)
                {
                    currentWeights[i] = marketValues[i] / totalMarketValue;
                }
            }

            var shouldRebalance = false;
            switch (strategy.Type.ToUpperInvariant())
            {
                case "PERIODIC":
                    if (strategy.Period > 0 && (t + 1) % strategy.Period == 0)
                    {
                        shouldRebalance = true;
                    }
                    break;
                case "MOMENTUM":
                    if (strategy.Period > 0 && (t + 1) % strategy.Period == 0 && t >= strategy.LookbackWindow)
                    {
                        shouldRebalance = true;
                    }
                    break;
            }

            if (shouldRebalance)
            {
                switch (strategy.Type.ToUpperInvariant())
                {
                    case "PERIODIC":
                        if (initialWeights != null)
                        {
                            Array.Copy(initialWeights, currentWeights, numAssets);
                        }
                        break;
                    case "MOMENTUM":
                        var lookbackReturns = new double[numAssets];
                        for (var i = 0; i < numAssets; i++)
                        {
                            var cumulativeReturn = 1.0;
                            for (var lb = 0; lb < strategy.LookbackWindow; lb++)
                            {
                                cumulativeReturn *= (1 + historicalReturns[t - lb, i]);
                            }
                            lookbackReturns[i] = cumulativeReturn - 1.0;
                        }

                        var rankedAssets = lookbackReturns
                            .Select((ret, index) => new { Return = ret, Index = index })
                            .OrderByDescending(x => x.Return)
                            .ToList();

                        var newTargetWeights = new double[numAssets];
                        var topAssets = rankedAssets.Take(strategy.NumAssets);
                        var weightPerAsset = 1.0 / strategy.NumAssets;

                        foreach (var asset in topAssets)
                        {
                            newTargetWeights[asset.Index] = weightPerAsset;
                        }
                        currentWeights = newTargetWeights;
                        break;
                }
            }
        }

        var finalReturnsArray = portfolioReturns.ToArray();

        var periodsPerYear = strategy.Period > 0 ? (int)Math.Round(12.0 / strategy.Period) : 12;
        if (periodsPerYear == 0) periodsPerYear = 1;

        var metrics = new PortfolioMetrics(
            Cagr: Metrics.AnnualizedReturn(finalReturnsArray, periodsPerYear),
            SharpeRatio: Metrics.SharpeRatio(finalReturnsArray, 0, periodsPerYear),
            MaxDrawdown: Metrics.MaxDrawdown(finalReturnsArray),
            Volatility: finalReturnsArray.StandardDeviation() * Math.Sqrt(periodsPerYear)
        );

        return new BacktestResult(finalReturnsArray, currentWeights, metrics);
    }
}
