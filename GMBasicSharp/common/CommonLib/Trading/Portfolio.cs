using MathNet.Numerics.LinearAlgebra;
using QLNet;
// ReSharper disable InconsistentNaming

namespace CommonLib.Trading;

/// <summary>
/// Ottimizzazione e analisi di portafoglio. Implementa mean-variance (Markowitz),
/// criterio di Kelly, Risk Parity, Black-Litterman, Min-CVaR, Min-CDaR,
/// e metriche multi-asset come VaR e CVaR di portafoglio.
///
/// Metodi:
/// - <see cref="PortfolioExpectedReturn"/>: rendimento atteso di un portafoglio.
/// - <see cref="PortfolioStDev"/>: deviazione standard di un portafoglio.
/// - <see cref="PortfolioSharpeRatio"/>: indice di Sharpe di un portafoglio.
/// - <see cref="OptimizeMeanVariance"/>: ottimizzazione mean-variance (max Sharpe, target return, min varianza).
/// - <see cref="KellyCriterion"/>: ottimizzazione col criterio di Kelly.
/// - <see cref="RiskParity"/>: portafoglio a parità di rischio.
/// - <see cref="EquallyWeighted"/>: portafoglio equamente ponderato.
/// - <see cref="MaxReturn"/>: portafoglio concentrato sull'asset a massimo rendimento.
/// - <see cref="BlackLitterman"/>: modello Black-Litterman con view discrete.
/// - <see cref="MinCVaR"/>: minimizzazione del Conditional VaR.
/// - <see cref="MinCDaR"/>: minimizzazione del Conditional Drawdown at Risk.
/// - <see cref="PortfolioMultiVaR"/>: VaR di un portafoglio multi-asset.
/// - <see cref="PortfolioMultiCVaR"/>: CVaR di un portafoglio multi-asset.
/// - <see cref="IsLongOnlyPortfolio"/>: verifica se un portafoglio è long-only.
/// </summary>
public static class Portfolio
{
    /// <summary>
    /// Calcola il rendimento atteso di un portafoglio dato il vettore dei pesi e dei rendimenti medi.
    /// </summary>
    /// <param name="weights">Pesi del portafoglio (devono sommare a 1).</param>
    /// <param name="meanReturns">Rendimenti medi attesi per ciascun asset.</param>
    /// <returns>Rendimento atteso del portafoglio.</returns>
    /// <exception cref="ArgumentException">Sollevata se weights e meanReturns hanno lunghezze diverse.</exception>
    /// <example>
    /// <code>
    /// var r = Portfolio.PortfolioExpectedReturn(new[] { 0.5, 0.5 }, new[] { 0.08, 0.12 });
    /// </code>
    /// </example>
    public static double PortfolioExpectedReturn(double[] weights, double[] meanReturns)
    {
        if (weights.Length != meanReturns.Length)
            throw new ArgumentException("Il numero di pesi deve corrispondere al numero di rendimenti.");

        var w = PortfolioMath.ToQlNetVector(weights);
        var r = PortfolioMath.ToQlNetVector(meanReturns);
        return PortfolioMath.PortfolioReturn(w, r);
    }

    /// <summary>
    /// Calcola la deviazione standard (rischio) di un portafoglio data la matrice di covarianza.
    /// </summary>
    /// <param name="weights">Pesi del portafoglio.</param>
    /// <param name="covarianceMatrix">Matrice di covarianza tra gli asset.</param>
    /// <returns>Deviazione standard del portafoglio.</returns>
    /// <exception cref="ArgumentException">Sollevata se la dimensione dei pesi non è compatibile.</exception>
    /// <example>
    /// <code>
    /// var cov = new double[,] { { 0.04, 0.02 }, { 0.02, 0.09 } };
    /// var sd = Portfolio.PortfolioStDev(new[] { 0.5, 0.5 }, cov);
    /// </code>
    /// </example>
    public static double PortfolioStDev(double[] weights, double[,] covarianceMatrix)
    {
        if (weights.Length != covarianceMatrix.GetLength(0))
            throw new ArgumentException("La dimensione dei pesi non è compatibile con la matrice di covarianza.");

        var w = PortfolioMath.ToQlNetVector(weights);
        var cov = PortfolioMath.ToQlNetMatrix(covarianceMatrix);
        return PortfolioMath.PortfolioRisk(w, cov).StDev;
    }

    /// <summary>
    /// Calcola l'indice di Sharpe di un portafoglio dati rendimento, deviazione standard e tasso privo di rischio.
    /// </summary>
    /// <param name="portfolioReturn">Rendimento del portafoglio.</param>
    /// <param name="portfolioStDev">Deviazione standard del portafoglio.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Indice di Sharpe.</returns>
    /// <example>
    /// <code>var sharpe = Portfolio.PortfolioSharpeRatio(0.10, 0.15, 0.02);</code>
    /// </example>
    public static double PortfolioSharpeRatio(double portfolioReturn, double portfolioStDev, double riskFreeRate)
    {
        return PortfolioMath.SharpeRatio(portfolioReturn, portfolioStDev, riskFreeRate);
    }

    /// <summary>
    /// Ottimizza un portafoglio secondo il criterio mean-variance (Markowitz).
    /// Supporta tre modalità: "MAXSHARPE" (massimizzazione Sharpe), "MINVARIANCE" (varianza minima),
    /// "TARGETRETURN" (varianza minima con rendimento target).
    /// </summary>
    /// <param name="meanReturns">Rendimenti medi attesi per ciascun asset.</param>
    /// <param name="covarianceMatrix">Matrice di covarianza tra gli asset.</param>
    /// <param name="optimizationType">Tipo di ottimizzazione: "MAXSHARPE", "MINVARIANCE", "TARGETRETURN".</param>
    /// <param name="riskFreeRate">Tasso privo di rischio (usato per MAXSHARPE).</param>
    /// <param name="constraints">Vincoli opzionali (rendimento target, bound inferiori/superiori).</param>
    /// <returns>Risultato dell'ottimizzazione con pesi, rendimento, rischio e Sharpe.</returns>
    /// <exception cref="ArgumentException">Sollevata se TARGETRETURN è richiesto ma TargetReturn è null.</exception>
    /// <exception cref="InvalidOperationException">Sollevata se il solutore QLNet non converge.</exception>
    /// <example>
    /// <code>
    /// var result = Portfolio.OptimizeMeanVariance(meanReturns, cov, "MAXSHARPE", 0.02, null);
    /// </code>
    /// </example>
    public static PortfolioResult OptimizeMeanVariance(
        double[] meanReturns,
        double[,] covarianceMatrix,
        string optimizationType,
        double riskFreeRate,
        PortfolioConstraints? constraints)
    {
        if (optimizationType.Equals("MAXSHARPE", StringComparison.InvariantCultureIgnoreCase))
        {
            return OptimizeMaxSharpe(meanReturns, covarianceMatrix, riskFreeRate);
        }

        var qlCov = PortfolioMath.ToQlNetMatrix(covarianceMatrix);
        var qlReturns = PortfolioMath.ToQlNetVector(meanReturns);
        var numAssets = meanReturns.Length;

        var costFunction = new MarkowitzCostFunction(qlCov);

        var activeConstraints = new List<Constraint> { new OneSumConstraint() };

        var lowerBounds = constraints?.LowerBounds ?? Enumerable.Repeat(0.0, numAssets).ToArray();
        var upperBounds = constraints?.UpperBounds ?? Enumerable.Repeat(1.0, numAssets).ToArray();
        activeConstraints.Add(new BoundaryConstraint(lowerBounds, upperBounds));

        if (optimizationType.Equals("TARGETRETURN", StringComparison.InvariantCultureIgnoreCase))
        {
            if (constraints?.TargetReturn is null)
                throw new ArgumentException("'TARGETRETURN' richiede un valore per TargetReturn nei vincoli.");
            activeConstraints.Add(new TargetReturnConstraint(qlReturns, constraints.TargetReturn.Value));
        }

        if (activeConstraints.Count == 0)
            throw new InvalidOperationException("Nessun vincolo specificato per l'ottimizzazione.");

        var compositeConstraint = activeConstraints[0];
        for (var i = 1; i < activeConstraints.Count; i++)
        {
            compositeConstraint = new CompositeConstraint(compositeConstraint, activeConstraints[i]);
        }

        var problem = new Problem(costFunction, compositeConstraint, new Vector(numAssets, 1.0 / numAssets));
        var endCriteria = new EndCriteria(1000, 100, 1e-8, 1e-8, 1e-8);

        EndCriteria.Type endResult;
        if (optimizationType.Equals("MINVOLATILITY", StringComparison.InvariantCultureIgnoreCase))
        {
            var solver = new Simplex(0.1);
            endResult = solver.minimize(problem, endCriteria);
        }
        else
        {
            var solver = new LevenbergMarquardt();
            endResult = solver.minimize(problem, endCriteria);
        }

        if (endResult is EndCriteria.Type.None or EndCriteria.Type.Unknown or EndCriteria.Type.MaxIterations)
        {
            throw new InvalidOperationException(
                "Il solutore QLNet non è riuscito a convergere. I vincoli potrebbero essere infattibili.");
        }

        var qlOptimalWeights = problem.currentValue();
        var optimalWeights = new double[qlOptimalWeights.size()];
        for (var i = 0; i < qlOptimalWeights.size(); i++)
        {
            optimalWeights[i] = qlOptimalWeights[i];
        }

        var wFinal = PortfolioMath.ToQlNetVector(optimalWeights);
        var finalReturn = PortfolioMath.PortfolioReturn(wFinal, qlReturns);
        var finalStDev = PortfolioMath.PortfolioRisk(wFinal, qlCov).StDev;
        var finalSharpe = PortfolioMath.SharpeRatio(finalReturn, finalStDev, riskFreeRate);

        return new PortfolioResult(optimalWeights, finalReturn, finalStDev, finalSharpe);
    }

    /// <summary>
    /// Ottimizza il portafoglio massimizzando l'indice di Sharpe, scandagliando la frontiera efficiente
    /// su 20 livelli di rendimento target.
    /// </summary>
    private static PortfolioResult OptimizeMaxSharpe(double[] meanReturns, double[,] covarianceMatrix,
        double riskFreeRate)
    {
        var bestSharpe = double.MinValue;
        var bestWeights = new double[meanReturns.Length];
        var solutionFound = false;

        var minReturn = meanReturns.Min();
        var maxReturn = meanReturns.Max();

        for (var i = 0; i <= 20; i++)
        {
            var targetReturn = minReturn + (maxReturn - minReturn) * i / 20.0;
            try
            {
                var constraints = new PortfolioConstraints(targetReturn, null, null);
                var result = OptimizeMeanVariance(meanReturns, covarianceMatrix, "TARGETRETURN", riskFreeRate,
                    constraints);

                if (result.SharpeRatio > bestSharpe)
                {
                    bestSharpe = result.SharpeRatio;
                    bestWeights = result.Weights;
                    solutionFound = true;
                }
            }
            catch (InvalidOperationException)
            {
            }
            catch (ArgumentException)
            {
                // QLNet/LevenbergMarquardt lancia ArgumentException ("less functions than
                // available variables") per un target infeasible/degenere: va trattato come
                // "target non valido" e si prosegue (prima propagava e faceva fallire tutta
                // l'ottimizzazione, es. BLACKLITTERMAN con posterior degenere).
            }
        }

        if (!solutionFound)
            throw new InvalidOperationException(
                "Impossibile trovare un portafoglio Max Sharpe. Controllare i dati di input, potrebbero essere infattibili.");

        var wFinal = PortfolioMath.ToQlNetVector(bestWeights);
        var qlReturns = PortfolioMath.ToQlNetVector(meanReturns);
        var qlCov = PortfolioMath.ToQlNetMatrix(covarianceMatrix);

        var finalReturn = PortfolioMath.PortfolioReturn(wFinal, qlReturns);
        var finalStDev = PortfolioMath.PortfolioRisk(wFinal, qlCov).StDev;
        var finalSharpe = PortfolioMath.SharpeRatio(finalReturn, finalStDev, riskFreeRate);

        return new PortfolioResult(bestWeights, finalReturn, finalStDev, finalSharpe);
    }

    /// <summary>
    /// Ottimizza il portafoglio utilizzando il criterio di Kelly, che massimizza la crescita attesa
    /// del capitale nel lungo periodo bilanciando rendimento atteso e varianza.
    /// </summary>
    /// <param name="historicalReturns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Portafoglio ottimale secondo il criterio di Kelly.</returns>
    /// <exception cref="InvalidOperationException">Sollevata se il solutore QLNet non converge.</exception>
    /// <example>
    /// <code>
    /// var kelly = Portfolio.KellyCriterion(rendimentiStorici, 0.02);
    /// </code>
    /// </example>
    public static PortfolioResult KellyCriterion(double[,] historicalReturns, double riskFreeRate)
    {
        var returnsMatrix = Matrix<double>.Build.DenseOfArray(historicalReturns);
        var numAssets = returnsMatrix.ColumnCount;
        var numPeriods = returnsMatrix.RowCount;

        var meanReturns = new double[numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            meanReturns[i] = returnsMatrix.Column(i).Average();
        }

        var cov = new double[numAssets, numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            for (var j = 0; j < numAssets; j++)
            {
                var colI = returnsMatrix.Column(i);
                var colJ = returnsMatrix.Column(j);
                var meanI = meanReturns[i];
                var meanJ = meanReturns[j];

                var covarianceSum = 0.0;
                for (var t = 0; t < numPeriods; t++)
                {
                    covarianceSum += (colI[t] - meanI) * (colJ[t] - meanJ);
                }

                cov[i, j] = covarianceSum / (numPeriods - 1);
            }
        }

        var costFunction =
            new KellyCostFunction(PortfolioMath.ToQlNetMatrix(cov), PortfolioMath.ToQlNetVector(meanReturns));

        var constraint1 = new OneSumConstraint();
        var constraint2 = new NonNegativeConstraint();
        var constraints = new CompositeConstraint(constraint1, constraint2);

        var problem = new Problem(costFunction, constraints, new Vector(numAssets, 1.0 / numAssets));
        var solver = new LevenbergMarquardt();
        var endCriteria = new EndCriteria(1000, 100, 1e-8, 1e-8, 1e-8);
        var endResult = solver.minimize(problem, endCriteria);

        if (endResult is EndCriteria.Type.None or EndCriteria.Type.Unknown or EndCriteria.Type.MaxIterations)
        {
            throw new InvalidOperationException("Il solutore QLNet per Kelly Criterion non è riuscito a convergere.");
        }

        var qlOptimalWeights = problem.currentValue();
        var optimalWeights = new double[qlOptimalWeights.size()];
        for (var i = 0; i < qlOptimalWeights.size(); i++)
        {
            optimalWeights[i] = qlOptimalWeights[i];
        }

        var wFinal = qlOptimalWeights;
        var qlReturns = PortfolioMath.ToQlNetVector(meanReturns);
        var qlCov = PortfolioMath.ToQlNetMatrix(cov);
        var finalReturn = PortfolioMath.PortfolioReturn(wFinal, qlReturns);
        var finalStDev = PortfolioMath.PortfolioRisk(wFinal, qlCov).StDev;
        var finalSharpe = PortfolioMath.SharpeRatio(finalReturn, finalStDev, riskFreeRate);

        return new PortfolioResult(optimalWeights, finalReturn, finalStDev, finalSharpe);
    }

    /// <summary>
    /// Calcola il portafoglio Risk Parity (a parità di rischio): i pesi sono determinati
    /// in modo che ciascun asset contribuisca equamente al rischio totale del portafoglio.
    /// </summary>
    /// <param name="covarianceMatrix">Matrice di covarianza tra gli asset.</param>
    /// <returns>Portafoglio Risk Parity (rendimento e Sharpe non calcolati).</returns>
    /// <example>
    /// <code>var rp = Portfolio.RiskParity(covMatrix);</code>
    /// </example>
    public static PortfolioResult RiskParity(double[,] covarianceMatrix)
    {
        var covMnet = Matrix<double>.Build.DenseOfArray(covarianceMatrix);
        var w = Vector<double>.Build.Dense(covMnet.ColumnCount, 1.0 / covMnet.ColumnCount);

        for (var iter = 0; iter < 1000; iter++)
        {
            var portfolioVariance = w.ToRowMatrix().Multiply(covMnet).Multiply(w.ToColumnMatrix())[0, 0];
            var marginalContributions = covMnet * w;
            var riskContributions = w.PointwiseMultiply(marginalContributions) / portfolioVariance;

            var newW = w.PointwiseDivide(riskContributions);
            newW /= newW.Sum();

            if ((newW - w).L2Norm() < 1e-6) break;
            w = newW;
        }

        var wArray = w.ToArray();
        var qlW = PortfolioMath.ToQlNetVector(wArray);
        var qlCov = PortfolioMath.ToQlNetMatrix(covarianceMatrix);
        var finalStDev = PortfolioMath.PortfolioRisk(qlW, qlCov).StDev;

        return new PortfolioResult(wArray, double.NaN, finalStDev, double.NaN);
    }

    /// <summary>
    /// Crea un portafoglio equamente ponderato (1/N).
    /// </summary>
    /// <param name="numAssets">Numero di asset.</param>
    /// <returns>Portafoglio con pesi uniformi.</returns>
    /// <exception cref="ArgumentException">Sollevata se numAssets &lt;= 0.</exception>
    /// <example>
    /// <code>var ew = Portfolio.EquallyWeighted(5);</code>
    /// </example>
    public static PortfolioResult EquallyWeighted(int numAssets)
    {
        if (numAssets <= 0) throw new ArgumentException("Il numero di asset deve essere positivo.");
        var weights = new double[numAssets];
        Array.Fill(weights, 1.0 / numAssets);
        return new PortfolioResult(weights, double.NaN, double.NaN, double.NaN);
    }

    /// <summary>
    /// Crea un portafoglio concentrato al 100% sull'asset con il rendimento atteso massimo.
    /// </summary>
    /// <param name="meanReturns">Rendimenti medi attesi per ciascun asset.</param>
    /// <returns>Portafoglio concentrato sull'asset migliore.</returns>
    /// <example>
    /// <code>var mr = Portfolio.MaxReturn(new[] { 0.05, 0.08, 0.06 });</code>
    /// </example>
    public static PortfolioResult MaxReturn(double[] meanReturns)
    {
        var numAssets = meanReturns.Length;
        var maxReturn = meanReturns.Max();
        var maxIndex = Array.IndexOf(meanReturns, maxReturn);

        var weights = new double[numAssets];
        weights[maxIndex] = 1.0;

        return new PortfolioResult(weights, maxReturn, double.NaN, double.NaN);
    }

    /// <summary>
    /// Applica il modello Black-Litterman per combinare i rendimenti di equilibrio di mercato
    /// con view discrete dell'investitore, producendo rendimenti attesi posteriori
    /// che vengono poi ottimizzati mean-variance.
    /// </summary>
    /// <param name="marketCaps">Capitalizzazioni di mercato degli asset.</param>
    /// <param name="covarianceMatrix">Matrice di covarianza tra gli asset.</param>
    /// <param name="riskAversion">Coefficiente di avversione al rischio di mercato.</param>
    /// <param name="tau">Parametro di scala per l'incertezza dei rendimenti di equilibrio.</param>
    /// <param name="P">Matrice delle view (k × n, dove k = numero di view, n = numero di asset).</param>
    /// <param name="Q">Vettore dei rendimenti attesi delle view (k elementi).</param>
    /// <param name="omega">Matrice di covarianza dell'incertezza delle view (k × k).</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Portafoglio ottimale secondo Black-Litterman (max Sharpe).</returns>
    /// <example>
    /// <code>
    /// var bl = Portfolio.BlackLitterman(mktCaps, cov, 2.5, 0.05, P, Q, omega, 0.02);
    /// </code>
    /// </example>
    public static PortfolioResult BlackLitterman(double[] marketCaps, double[,] covarianceMatrix,
        double riskAversion, double tau, double[,] P, double[] Q, double[,] omega, double riskFreeRate)
    {
        var Σ = Matrix<double>.Build.DenseOfArray(covarianceMatrix);
        var P_m = Matrix<double>.Build.DenseOfArray(P);
        var Q_v = Vector<double>.Build.DenseOfArray(Q);
        var Ω = Matrix<double>.Build.DenseOfArray(omega);

        var totalMarketCap = marketCaps.Sum();
        var w_mkt = Vector<double>.Build.DenseOfArray(marketCaps.Select(c => c / totalMarketCap).ToArray());

        var Π = riskAversion * Σ * w_mkt;

        var tsum_inv = (tau * Σ).Inverse();
        var P_t = P_m.Transpose();
        var Ω_inv = Ω.Inverse();

        var part1 = (tsum_inv + P_t * Ω_inv * P_m).Inverse();
        var part2 = tsum_inv * Π + P_t * Ω_inv * Q_v;

        var posteriorReturns = part1 * part2;

        // MAXSHARPE analitico (portafoglio tangente, w = Σ⁻¹(r − rf) normalizzato): robusto e
        // allineato a IlRuntime.MaxSharpeDict / gmb_port_max_sharpe. Prima si passava da
        // OptimizeMeanVariance("MAXSHARPE") → QLNet/LevenbergMarquardt, che lancia
        // ArgumentException ("less functions than available variables") e faceva fallire tutta
        // la Black-Litterman.
        return AnalyticMaxSharpe(posteriorReturns.ToArray(), covarianceMatrix, riskFreeRate);
    }

    /// <summary>Max-Sharpe analitico: w = Σ⁻¹(r − rf), normalizzato a somma 1 (portafoglio
    /// tangente). Usato da BlackLitterman (parità IlRuntime.MaxSharpeDict / gmb_port_max_sharpe).</summary>
    private static PortfolioResult AnalyticMaxSharpe(double[] returns, double[,] cov, double rf)
    {
        var n = returns.Length;
        if (n == 0) return new PortfolioResult([], 0, 0, 0);
        var Σ = Matrix<double>.Build.DenseOfArray(cov);
        var r = Vector<double>.Build.DenseOfArray(returns);
        Vector<double> w;
        try
        {
            var invΣ = Σ.Inverse();
            var wRaw = invΣ * (r - Vector<double>.Build.Dense(n, rf));
            var sum = wRaw.Sum();
            w = sum != 0 ? wRaw / sum : Vector<double>.Build.Dense(n, 1.0 / n);
        }
        catch
        {
            w = Vector<double>.Build.Dense(n, 1.0 / n);
        }
        var ret = w.DotProduct(r);
        var variance = w.DotProduct(Σ * w);
        var sd = Math.Sqrt(Math.Max(0, variance));
        var sharpe = sd > 0 ? (ret - rf) / sd : 0;
        return new PortfolioResult(w.ToArray(), ret, sd, sharpe);
    }

    /// <summary>
    /// Minimizza il Conditional Value at Risk (CVaR / Expected Shortfall) di un portafoglio
    /// utilizzando l'ottimizzazione PSO (Particle Swarm Optimization).
    /// </summary>
    /// <param name="historicalReturns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <param name="confidenceLevel">Livello di confidenza per il CVaR (es. 0.95).</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Portafoglio a CVaR minimo con metriche di performance.</returns>
    /// <example>
    /// <code>var mincvar = Portfolio.MinCVaR(rendimenti, 0.95, 0.02);</code>
    /// </example>
    public static PortfolioResult MinCVaR(double[,] historicalReturns, double confidenceLevel,
        double riskFreeRate)
    {
        var numAssets = historicalReturns.GetLength(1);
        var numPeriods = historicalReturns.GetLength(0);
        var alpha = 1.0 - confidenceLevel;

        var optimalWeights = PsoOptimizer.Minimize(historicalReturns, alpha, PsoObjective.CVaR);

        var meanReturns = new double[numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            double sum = 0;
            for (var t = 0; t < numPeriods; t++)
            {
                sum += historicalReturns[t, i];
            }

            meanReturns[i] = sum / numPeriods;
        }

        var covMatrixValues = new double[numAssets, numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            for (var j = 0; j < numAssets; j++)
            {
                double covSum = 0;
                for (var t = 0; t < numPeriods; t++)
                {
                    covSum += (historicalReturns[t, i] - meanReturns[i]) * (historicalReturns[t, j] - meanReturns[j]);
                }

                covMatrixValues[i, j] = covSum / (numPeriods - 1);
            }
        }

        var wFinal = PortfolioMath.ToQlNetVector(optimalWeights);
        var qlReturns = PortfolioMath.ToQlNetVector(meanReturns);
        var qlCov = PortfolioMath.ToQlNetMatrix(covMatrixValues);

        var finalReturn = PortfolioMath.PortfolioReturn(wFinal, qlReturns);
        var finalStDev = PortfolioMath.PortfolioRisk(wFinal, qlCov).StDev;
        var finalSharpe = PortfolioMath.SharpeRatio(finalReturn, finalStDev, riskFreeRate);

        return new PortfolioResult(optimalWeights, finalReturn, finalStDev, finalSharpe);
    }

    /// <summary>
    /// Minimizza il Conditional Drawdown at Risk (CDaR) di un portafoglio
    /// utilizzando l'ottimizzazione PSO (Particle Swarm Optimization).
    /// </summary>
    /// <param name="historicalReturns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <param name="confidenceLevel">Livello di confidenza per il CDaR (es. 0.95).</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Portafoglio a CDaR minimo con metriche di performance.</returns>
    /// <example>
    /// <code>var mincdar = Portfolio.MinCDaR(rendimenti, 0.95, 0.02);</code>
    /// </example>
    public static PortfolioResult MinCDaR(double[,] historicalReturns, double confidenceLevel,
        double riskFreeRate)
    {
        var numAssets = historicalReturns.GetLength(1);
        var numPeriods = historicalReturns.GetLength(0);
        var alpha = 1.0 - confidenceLevel;

        var optimalWeights = PsoOptimizer.Minimize(historicalReturns, alpha, PsoObjective.CDaR);

        var meanReturns = new double[numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            double sum = 0;
            for (var t = 0; t < numPeriods; t++)
            {
                sum += historicalReturns[t, i];
            }

            meanReturns[i] = sum / numPeriods;
        }

        var covMatrixValues = new double[numAssets, numAssets];
        for (var i = 0; i < numAssets; i++)
        {
            for (var j = 0; j < numAssets; j++)
            {
                double covSum = 0;
                for (var t = 0; t < numPeriods; t++)
                {
                    covSum += (historicalReturns[t, i] - meanReturns[i]) * (historicalReturns[t, j] - meanReturns[j]);
                }

                covMatrixValues[i, j] = covSum / (numPeriods - 1);
            }
        }

        var wFinal = PortfolioMath.ToQlNetVector(optimalWeights);
        var qlReturns = PortfolioMath.ToQlNetVector(meanReturns);
        var qlCov = PortfolioMath.ToQlNetMatrix(covMatrixValues);

        var finalReturn = PortfolioMath.PortfolioReturn(wFinal, qlReturns);
        var finalStDev = PortfolioMath.PortfolioRisk(wFinal, qlCov).StDev;
        var finalSharpe = PortfolioMath.SharpeRatio(finalReturn, finalStDev, riskFreeRate);

        return new PortfolioResult(optimalWeights, finalReturn, finalStDev, finalSharpe);
    }

    /// <summary>
    /// Calcola il Value at Risk (VaR) storico per un portafoglio multi-asset.
    /// </summary>
    /// <param name="historicalReturns">Matrice [simulazioni × asset] dei rendimenti.</param>
    /// <param name="weights">Pesi del portafoglio.</param>
    /// <param name="confidenceLevel">Livello di confidenza (es. 0.95).</param>
    /// <returns>VaR del portafoglio al livello di confidenza specificato.</returns>
    /// <example>
    /// <code>
    /// var var95 = Portfolio.PortfolioMultiVaR(rendimenti, pesi, 0.95);
    /// </code>
    /// </example>
    public static double PortfolioMultiVaR(double[,] historicalReturns, double[] weights, double confidenceLevel)
    {
        var nSim = historicalReturns.GetLength(0);
        var ptfReturns = new double[nSim];
        for (var i = 0; i < nSim; i++)
        {
            double sum = 0;
            for (var j = 0; j < weights.Length; j++)
                sum += historicalReturns[i, j] * weights[j];
            ptfReturns[i] = sum;
        }

        Array.Sort(ptfReturns);
        var varIndex = (int)Math.Floor(nSim * (1.0 - confidenceLevel));
        return ptfReturns[varIndex];
    }

    /// <summary>
    /// Calcola il Conditional Value at Risk (CVaR / Expected Shortfall) per un portafoglio multi-asset.
    /// </summary>
    /// <param name="historicalReturns">Matrice [simulazioni × asset] dei rendimenti.</param>
    /// <param name="weights">Pesi del portafoglio.</param>
    /// <param name="confidenceLevel">Livello di confidenza (es. 0.95).</param>
    /// <returns>CVaR del portafoglio (media delle perdite oltre il VaR).</returns>
    /// <example>
    /// <code>
    /// var cvar95 = Portfolio.PortfolioMultiCVaR(rendimenti, pesi, 0.95);
    /// </code>
    /// </example>
    public static double PortfolioMultiCVaR(double[,] historicalReturns, double[] weights, double confidenceLevel)
    {
        var nSim = historicalReturns.GetLength(0);
        var ptfReturns = new double[nSim];
        for (var i = 0; i < nSim; i++)
        {
            double sum = 0;
            for (var j = 0; j < weights.Length; j++)
                sum += historicalReturns[i, j] * weights[j];
            ptfReturns[i] = sum;
        }

        Array.Sort(ptfReturns);
        var varIndex = (int)Math.Floor(nSim * (1.0 - confidenceLevel));
        var cut = ptfReturns.Take(varIndex + 1).ToArray();
        return cut.Average();
    }

    /// <summary>
    /// Verifica se un portafoglio è long-only: tutti i pesi sono non negativi
    /// e la somma è esattamente 1.0 (con tolleranza 1e-6).
    /// </summary>
    /// <param name="weights">Pesi del portafoglio da verificare.</param>
    /// <returns>True se il portafoglio è long-only, False altrimenti.</returns>
    /// <example>
    /// <code>var isLongOnly = Portfolio.IsLongOnlyPortfolio(new[] { 0.5, 0.5 });</code>
    /// </example>
    public static bool IsLongOnlyPortfolio(double[] weights)
    {
        if (weights.Any(w => w < -1e-9)) return false;
        var sum = weights.Sum();
        return Math.Abs(sum - 1.0) < 1e-6;
    }
}
