// FILE: StdLib.Numerics.OptimizeFit.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.LinearAlgebra;

namespace CommonLib;

/// <summary>
/// scipy.optimize: curve_fit (fit non lineare ai minimi quadrati, riusa il motore
/// Levenberg-Marquardt) e differential_evolution (ottimizzazione globale, DE/rand/1/bin).
/// </summary>
public static partial class Numerics
{
    /// <summary>
    /// Fit non lineare del modello y = model(p, x) ai dati (x, y), minimizzando la somma dei
    /// quadrati dei residui (scipy.optimize.curve_fit, popt). Ripartendo da p0.
    /// </summary>
    public static double[] CurveFit(Func<double[], double, double> model,
        double[] x, double[] y, double[] p0,
        double tolerance = 1e-10, int maxIterations = 200)
    {
        if (model == null) throw new ArgumentNullException(nameof(model));
        if (x == null || y == null || p0 == null) throw new ArgumentNullException();
        if (x.Length != y.Length) throw new ArgumentException("x e y devono avere la stessa lunghezza.");
        var xv = Vector<double>.Build.Dense(x);
        var yv = Vector<double>.Build.Dense(y);
        var p0v = Vector<double>.Build.Dense(p0);
        var result = LevenbergMarquardt((p, xi) => model(p.ToArray(), xi), p0v, xv, yv, tolerance, maxIterations);
        return result.ToArray();
    }

    /// <summary>
    /// Ottimizzazione globale con Differential Evolution (scipy.optimize.differential_evolution,
    /// strategia rand/1/bin): minimizza fn(p) sui bounds per parametro. Deterministico (seed fisso).
    /// </summary>
    public static double[] DifferentialEvolution(Func<double[], double> fn, double[,] bounds,
        int popsize = 0, int maxiter = 1000, double tol = 1e-6)
    {
        if (fn == null) throw new ArgumentNullException(nameof(fn));
        var n = bounds.GetLength(0);
        var lo = new double[n];
        var hi = new double[n];
        for (var i = 0; i < n; i++) { lo[i] = bounds[i, 0]; hi[i] = bounds[i, 1]; }
        var np = popsize > 0 ? popsize : Math.Max(10 * n, 20);
        var rnd = new Random(0);
        var pop = new double[np][];
        var fit = new double[np];
        for (var i = 0; i < np; i++)
        {
            pop[i] = new double[n];
            for (var j = 0; j < n; j++) pop[i][j] = lo[j] + rnd.NextDouble() * (hi[j] - lo[j]);
            fit[i] = fn(pop[i]);
        }
        const double fScale = 0.8, cr = 0.9;
        for (var gen = 0; gen < maxiter; gen++)
        {
            var bestNow = double.PositiveInfinity;
            for (var i = 0; i < np; i++) if (fit[i] < bestNow) bestNow = fit[i];
            if (bestNow < tol) break;
            for (var i = 0; i < np; i++)
            {
                int r1, r2, r3;
                do { r1 = rnd.Next(np); } while (r1 == i);
                do { r2 = rnd.Next(np); } while (r2 == i || r2 == r1);
                do { r3 = rnd.Next(np); } while (r3 == i || r3 == r1 || r3 == r2);
                var trial = new double[n];
                var jrand = rnd.Next(n);
                for (var j = 0; j < n; j++)
                {
                    var u = (rnd.NextDouble() < cr || j == jrand)
                        ? pop[r1][j] + fScale * (pop[r2][j] - pop[r3][j])
                        : pop[i][j];
                    if (u < lo[j]) u = lo[j];
                    if (u > hi[j]) u = hi[j];
                    trial[j] = u;
                }
                var tf = fn(trial);
                if (tf <= fit[i]) { pop[i] = trial; fit[i] = tf; }
            }
        }
        var bestF = double.PositiveInfinity;
        var best = (double[])lo.Clone();
        for (var i = 0; i < np; i++)
            if (fit[i] < bestF) { bestF = fit[i]; best = pop[i]; }
        return best;
    }
}
