namespace CommonLib.Trading;

/// <summary>
/// Obiettivo della funzione fitness per l'ottimizzazione PSO.
/// - <see cref="CVaR"/>: minimizzazione del Conditional Value at Risk.
/// - <see cref="CDaR"/>: minimizzazione del Conditional Drawdown at Risk.
/// </summary>
public enum PsoObjective
{
    /// <summary>Minimizzazione del Conditional Value at Risk (Expected Shortfall).</summary>
    CVaR,
    /// <summary>Minimizzazione del Conditional Drawdown at Risk.</summary>
    CDaR
}

/// <summary>
/// Ottimizzazione di portafoglio tramite Particle Swarm Optimization (PSO).
/// Supporta la minimizzazione del CVaR e CDaR con vincolo di portafoglio long-only.
/// Utilizza pesi di inerzia e coefficienti di accelerazione standard (Clerc &amp; Kennedy 2002).
///
/// Metodi:
/// - <see cref="Minimize"/>: esegue l'ottimizzazione PSO.
/// </summary>
public static class PsoOptimizer
{
    /// <summary>
    /// Esegue l'ottimizzazione PSO per minimizzare il rischio di portafoglio (CVaR o CDaR).
    /// Inizializza uno sciame di particelle con posizioni casuali normalizzate a somma 1,
    /// e iterativamente aggiorna velocità e posizioni secondo la dinamica PSO standard.
    /// </summary>
    /// <param name="returns">Matrice [periodi × asset] dei rendimenti storici.</param>
    /// <param name="alpha">Parametro di coda: 1 - livello di confidenza (es. 0.05 per 95% confidenza).</param>
    /// <param name="objective">Obiettivo da minimizzare: CVaR o CDaR.</param>
    /// <param name="swarmSize">Dimensione dello sciame (default 50).</param>
    /// <param name="maxIter">Numero massimo di iterazioni (default 200).</param>
    /// <returns>Vettore dei pesi ottimali del portafoglio (somma = 1, non negativi).</returns>
    /// <exception cref="ArgumentException">Sollevata se swarmSize non è positivo.</exception>
    /// <exception cref="ArgumentOutOfRangeException">Sollevata se l'obiettivo PSO non è valido.</exception>
    /// <example>
    /// <code>
    /// var pesi = PsoOptimizer.Minimize(rendimenti, 0.05, PsoObjective.CVaR);
    /// </code>
    /// </example>
    public static double[] Minimize(double[,] returns, double alpha, PsoObjective objective, int swarmSize = 50,
        int maxIter = 200)
    {
        var nAssets = returns.GetLength(1);
        var rand = new Random();

        if (swarmSize <= 0)
        {
            throw new ArgumentException("La dimensione dello sciame (swarmSize) deve essere positiva.",
                nameof(swarmSize));
        }

        var wInertia = 0.729;
        var c1 = 1.494;
        var c2 = 1.494;

        var particles = new List<double[]>(swarmSize);
        var velocities = new List<double[]>(swarmSize);
        var bestPositions = new List<double[]>(swarmSize);
        var bestScores = new List<double>(swarmSize);

        var firstParticle = new double[nAssets];
        for (var j = 0; j < nAssets; j++) firstParticle[j] = rand.NextDouble();
        Normalize(firstParticle);

        particles.Add(firstParticle.ToArray());
        bestPositions.Add(firstParticle.ToArray());

        var firstVelocity = new double[nAssets];
        for (var j = 0; j < nAssets; j++) firstVelocity[j] = (rand.NextDouble() - 0.5) * 0.1;
        velocities.Add(firstVelocity);

        var firstScore = CalculateObjective(firstParticle, returns, alpha, objective);
        bestScores.Add(firstScore);

        var globalBestPosition = firstParticle.ToArray();
        var globalBestScore = firstScore;

        for (var s = 1; s < swarmSize; s++)
        {
            var p = new double[nAssets];
            for (var j = 0; j < nAssets; j++) p[j] = rand.NextDouble();
            Normalize(p);
            particles.Add(p.ToArray());

            var v = new double[nAssets];
            for (var j = 0; j < nAssets; j++) v[j] = (rand.NextDouble() - 0.5) * 0.1;
            velocities.Add(v);

            var score = CalculateObjective(p, returns, alpha, objective);

            bestPositions.Add(p.ToArray());
            bestScores.Add(score);

            if (score < globalBestScore)
            {
                globalBestScore = score;
                Array.Copy(p, globalBestPosition, nAssets);
            }
        }

        for (var iter = 0; iter < maxIter; iter++)
        {
            for (var s = 0; s < swarmSize; s++)
            {
                for (var j = 0; j < nAssets; j++)
                {
                    velocities[s][j] = wInertia * velocities[s][j]
                                       + c1 * rand.NextDouble() * (bestPositions[s][j] - particles[s][j])
                                       + c2 * rand.NextDouble() * (globalBestPosition[j] - particles[s][j]);
                }

                for (var j = 0; j < nAssets; j++)
                    particles[s][j] += velocities[s][j];

                Normalize(particles[s]);

                var score = CalculateObjective(particles[s], returns, alpha, objective);

                if (score < bestScores[s])
                {
                    bestScores[s] = score;
                    Array.Copy(particles[s], bestPositions[s], nAssets);

                    if (score < globalBestScore)
                    {
                        globalBestScore = score;
                        Array.Copy(particles[s], globalBestPosition, nAssets);
                    }
                }
            }
        }

        return globalBestPosition;
    }

    /// <summary>
    /// Calcola la funzione obiettivo per una data soluzione (pesi) in base all'obiettivo PSO selezionato.
    /// </summary>
    private static double CalculateObjective(double[] weights, double[,] returns, double alpha,
        PsoObjective objective)
    {
        return objective switch
        {
            PsoObjective.CVaR => CalculateCvarInternal(weights, returns, alpha),
            PsoObjective.CDaR => CalculateCdarInternal(weights, returns, alpha),
            _ => throw new ArgumentOutOfRangeException(nameof(objective), "Obiettivo PSO non valido.")
        };
    }

    /// <summary>
    /// Calcola il Conditional Value at Risk (CVaR) interno per una data combinazione di pesi.
    /// </summary>
    private static double CalculateCvarInternal(double[] weights, double[,] returns, double alpha)
    {
        var nSim = returns.GetLength(0);
        var portfolioReturns = new double[nSim];
        for (var i = 0; i < nSim; i++)
        {
            double sum = 0;
            for (var j = 0; j < weights.Length; j++) sum += returns[i, j] * weights[j];
            portfolioReturns[i] = sum;
        }

        Array.Sort(portfolioReturns);
        var varIndex = (int)Math.Floor(nSim * alpha);
        var varValue = portfolioReturns[varIndex];

        return -portfolioReturns.Where(r => r <= varValue).Average();
    }

    /// <summary>
    /// Calcola il Conditional Drawdown at Risk (CDaR) interno per una data combinazione di pesi.
    /// </summary>
    private static double CalculateCdarInternal(double[] weights, double[,] returns, double alpha)
    {
        var nSim = returns.GetLength(0);
        var portfolioReturns = new double[nSim];
        for (var i = 0; i < nSim; i++)
        {
            double sum = 0;
            for (var j = 0; j < weights.Length; j++) sum += returns[i, j] * weights[j];
            portfolioReturns[i] = sum;
        }

        var cumReturns = new double[nSim];
        cumReturns[0] = 1 + portfolioReturns[0];
        for (var i = 1; i < nSim; i++) cumReturns[i] = cumReturns[i - 1] * (1 + portfolioReturns[i]);

        var peak = new double[nSim];
        peak[0] = Math.Max(1.0, cumReturns[0]);
        for (var i = 1; i < nSim; i++) peak[i] = Math.Max(peak[i - 1], cumReturns[i]);

        var drawdowns = new double[nSim];
        for (var i = 0; i < nSim; i++) drawdowns[i] = (peak[i] - cumReturns[i]) / peak[i];

        Array.Sort(drawdowns);
        var varIndex = (int)Math.Floor(nSim * (1 - alpha));
        var varValue = drawdowns[varIndex];

        return drawdowns.Where(d => d >= varValue).Average();
    }

    /// <summary>
    /// Normalizza un vettore di pesi: azzera i valori negativi e scala a somma 1.
    /// Se la somma è nulla (tutti zero), assegna pesi uniformi 1/N.
    /// </summary>
    private static void Normalize(double[] weights)
    {
        for (var i = 0; i < weights.Length; i++)
            weights[i] = Math.Max(0, weights[i]);
        var sum = weights.Sum();
        if (sum > 1e-9)
        {
            for (var i = 0; i < weights.Length; i++)
                weights[i] /= sum;
        }
        else
        {
            for (var i = 0; i < weights.Length; i++)
                weights[i] = 1.0 / weights.Length;
        }
    }
}
