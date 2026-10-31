using QLNet;

namespace CommonLib.Trading;

/// <summary>
/// Metodi di utility per l'interoperabilità con la libreria QLNet (quantitative finance).
/// Converte array e matrici .NET nei tipi Vector/Matrix di QLNet e calcola
/// rendimento, rischio e Sharpe di portafoglio utilizzando le strutture QLNet.
///
/// Metodi:
/// - <see cref="ToQlNetVector"/>: converte double[] in QLNet Vector.
/// - <see cref="ToQlNetMatrix"/>: converte double[,] in QLNet Matrix.
/// - <see cref="PortfolioReturn"/>: prodotto scalare pesi × rendimenti.
/// - <see cref="PortfolioRisk"/>: varianza e deviazione standard.
/// - <see cref="SharpeRatio"/>: indice di Sharpe.
/// </summary>
public static class PortfolioMath
{
    /// <summary>
    /// Converte un array di double in un vettore QLNet Vector.
    /// </summary>
    /// <param name="vector">Array di double da convertire.</param>
    /// <returns>Nuovo QLNet Vector con gli stessi valori.</returns>
    /// <example>
    /// <code>
    /// var qlVec = PortfolioMath.ToQlNetVector(new[] { 0.5, 0.5 });
    /// </code>
    /// </example>
    public static Vector ToQlNetVector(double[] vector)
    {
        var qlVector = new Vector(vector.Length);
        for (var i = 0; i < vector.Length; i++)
        {
            qlVector[i] = vector[i];
        }
        return qlVector;
    }

    /// <summary>
    /// Converte una matrice bidimensionale di double in una matrice QLNet Matrix.
    /// </summary>
    /// <param name="matrix">Matrice double[,] da convertire.</param>
    /// <returns>Nuova QLNet Matrix con gli stessi valori.</returns>
    /// <example>
    /// <code>
    /// var qlMat = PortfolioMath.ToQlNetMatrix(new double[,] { { 0.04, 0.02 }, { 0.02, 0.09 } });
    /// </code>
    /// </example>
    public static Matrix ToQlNetMatrix(double[,] matrix)
    {
        var rows = matrix.GetLength(0);
        var cols = matrix.GetLength(1);
        var qlMatrix = new Matrix(rows, cols);
        for (var i = 0; i < rows; i++)
        {
            for (var j = 0; j < cols; j++)
            {
                qlMatrix[i, j] = matrix[i, j];
            }
        }
        return qlMatrix;
    }

    /// <summary>
    /// Calcola il rendimento atteso di un portafoglio come prodotto scalare
    /// tra il vettore dei pesi e il vettore dei rendimenti medi.
    /// </summary>
    /// <param name="weights">Pesi del portafoglio (QLNet Vector).</param>
    /// <param name="meanReturns">Rendimenti medi attesi (QLNet Vector).</param>
    /// <returns>Rendimento atteso del portafoglio (prodotto scalare).</returns>
    /// <example>
    /// <code>
    /// var ret = PortfolioMath.PortfolioReturn(weights, meanReturns);
    /// </code>
    /// </example>
    public static double PortfolioReturn(Vector weights, Vector meanReturns)
    {
        return weights * meanReturns;
    }

    /// <summary>
    /// Calcola la varianza e la deviazione standard (rischio) di un portafoglio
    /// dati i pesi e la matrice di covarianza.
    /// La varianza è calcolata come w' × Σ × w.
    /// </summary>
    /// <param name="weights">Pesi del portafoglio (QLNet Vector).</param>
    /// <param name="covariance">Matrice di covarianza (QLNet Matrix).</param>
    /// <returns>Tuple con Varianza e Deviazione Standard (StDev).</returns>
    /// <example>
    /// <code>
    /// var (var, sd) = PortfolioMath.PortfolioRisk(weights, covMatrix);
    /// </code>
    /// </example>
    public static (double Variance, double StDev) PortfolioRisk(Vector weights, Matrix covariance)
    {
        var variance = weights * (covariance * weights);
        if (variance < 0) variance = 0;
        return (variance, Math.Sqrt(variance));
    }

    /// <summary>
    /// Calcola l'indice di Sharpe: (rendimento del portafoglio - tasso privo di rischio) / deviazione standard.
    /// Restituisce 0.0 se la deviazione standard è trascurabile (&lt;= 1e-9).
    /// </summary>
    /// <param name="portfolioReturn">Rendimento del portafoglio.</param>
    /// <param name="portfolioStDev">Deviazione standard del portafoglio.</param>
    /// <param name="riskFreeRate">Tasso privo di rischio.</param>
    /// <returns>Indice di Sharpe.</returns>
    /// <example>
    /// <code>
    /// var sharpe = PortfolioMath.SharpeRatio(0.10, 0.15, 0.02);
    /// </code>
    /// </example>
    public static double SharpeRatio(double portfolioReturn, double portfolioStDev, double riskFreeRate)
    {
        return portfolioStDev > 1e-9 ? (portfolioReturn - riskFreeRate) / portfolioStDev : 0.0;
    }
}

#region QLNet Optimization Helpers

/// <summary>
/// Classe astratta di base per le funzioni di costo nell'ottimizzazione di portafoglio con QLNet.
/// Estende <see cref="CostFunction"/> e implementa il wrapping del valore scalare in un vettore.
/// </summary>
public abstract class BasePortfolioCostFunction : CostFunction
{
    /// <summary>
    /// Restituisce la funzione obiettivo come vettore di un elemento.
    /// </summary>
    public override Vector values(Vector w) => new Vector(1, value(w));
    /// <summary>
    /// Calcola il valore della funzione obiettivo (da implementare nelle sottoclassi).
    /// </summary>
    public abstract override double value(Vector w);
}

/// <summary>
/// Funzione di costo per l'ottimizzazione mean-variance (Markowitz).
/// Minimizza la varianza del portafoglio: w' × Σ × w.
/// </summary>
/// <param name="covariance">Matrice di covarianza tra gli asset.</param>
public class MarkowitzCostFunction(Matrix covariance) : BasePortfolioCostFunction
{
    /// <summary>
    /// Calcola la varianza del portafoglio dati i pesi.
    /// </summary>
    /// <param name="w">Pesi del portafoglio.</param>
    /// <returns>Varianza del portafoglio.</returns>
    public override double value(Vector w) => PortfolioMath.PortfolioRisk(w, covariance).Variance;
}

/// <summary>
/// Funzione di costo per l'ottimizzazione secondo il criterio di Kelly.
/// Minimizza 0.5 × varianza - rendimento atteso (massimizza la crescita attesa).
/// </summary>
/// <param name="covariance">Matrice di covarianza tra gli asset.</param>
/// <param name="meanReturns">Rendimenti medi attesi.</param>
public class KellyCostFunction(Matrix covariance, Vector meanReturns) : BasePortfolioCostFunction
{
    /// <summary>
    /// Calcola il valore della funzione obiettivo di Kelly: 0.5 × w'Σw - w'μ.
    /// </summary>
    /// <param name="w">Pesi del portafoglio.</param>
    /// <returns>Valore della funzione obiettivo (da minimizzare).</returns>
    public override double value(Vector w) => 0.5 * PortfolioMath.PortfolioRisk(w, covariance).Variance -
                                              PortfolioMath.PortfolioReturn(w, meanReturns);
}

/// <summary>
/// Classe astratta di base per i vincoli di ottimizzazione di portafoglio con QLNet.
/// Estende <see cref="IConstraint"/> e fornisce implementazioni predefinite
/// per i bound inferiori e superiori.
/// </summary>
public abstract class BasePortfolioConstraint : IConstraint
{
    /// <summary>
    /// Verifica se un punto soddisfa il vincolo.
    /// </summary>
    public abstract bool test(Vector p);

    /// <summary>
    /// Restituisce il bound superiore per ciascuna variabile (default MaxValue).
    /// </summary>
    public virtual Vector upperBound(Vector p) => new Vector(p.size(), double.MaxValue);

    /// <summary>
    /// Restituisce il bound inferiore per ciascuna variabile (default MinValue).
    /// </summary>
    public virtual Vector lowerBound(Vector p) => new Vector(p.size(), double.MinValue);
}

/// <summary>
/// Vincolo che impone che la somma dei pesi sia esattamente 1.0 (portafoglio fully invested).
/// w₁ + w₂ + ... + wₙ = 1.0 (con tolleranza 1e-9).
/// </summary>
public class OneSumConstraint() : Constraint(new Impl())
{
    private class Impl : BasePortfolioConstraint
    {
        public override bool test(Vector p) => Math.Abs(p.Sum() - 1.0) < 1.0e-9;
    }
}

/// <summary>
/// Vincolo di non negatività: tutti i pesi devono essere >= 0 (portafoglio long-only).
/// Il bound inferiore è impostato a 0 per tutte le variabili.
/// </summary>
public class NonNegativeConstraint() : Constraint(new Impl())
{
    private class Impl : BasePortfolioConstraint
    {
        public override bool test(Vector p) => p.All(val => val >= -1.0e-9);

        public override Vector lowerBound(Vector p) => new Vector(p.size(), 0.0);
    }
}

/// <summary>
/// Vincolo che impone un rendimento minimo target: w' × μ >= target.
/// Utilizzato nell'ottimizzazione mean-variance con rendimento target.
/// </summary>
/// <param name="returns">Vettore dei rendimenti medi attesi.</param>
/// <param name="target">Rendimento minimo richiesto.</param>
public class TargetReturnConstraint(Vector returns, double target) : Constraint(new Impl(returns, target))
{
    private class Impl(Vector returns, double target) : BasePortfolioConstraint
    {
        public override bool test(Vector p) => PortfolioMath.PortfolioReturn(p, returns) >= target - 1.0e-9;
    }
}

/// <summary>
/// Vincolo che impone limiti inferiore e superiore per ciascun peso del portafoglio.
/// I limiti vengono convertiti in QLNet Vector per l'uso con il solutore.
/// </summary>
/// <param name="lower">Array dei limiti inferiori per ciascun asset.</param>
/// <param name="upper">Array dei limiti superiori per ciascun asset.</param>
public class BoundaryConstraint(double[] lower, double[] upper) : Constraint(new Impl(lower, upper))
{
    private class Impl(double[] lower, double[] upper) : BasePortfolioConstraint
    {
        private readonly Vector _lower = PortfolioMath.ToQlNetVector(lower);
        private readonly Vector _upper = PortfolioMath.ToQlNetVector(upper);

        public override bool test(Vector p) => p.All(val => val >= -1.0e-9) &&
                                               (p - _lower).All(val => val >= -1.0e-9) &&
                                               (_upper - p).All(val => val >= -1.0e-9);

        public override Vector lowerBound(Vector p) => _lower;

        public override Vector upperBound(Vector p) => _upper;
    }
}

#endregion
