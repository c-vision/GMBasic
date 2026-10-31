// FILE: StdLib.Numerics.Optimization.cs
// ReSharper disable CheckNamespace

using MathNet.Numerics.LinearAlgebra;
using MathNet.Numerics.Optimization;
// ReSharper disable InconsistentNaming

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>
    /// Risolve un problema di programmazione lineare usando l'algoritmo del simplesso.
    /// Supporta sia massimizzazione che minimizzazione.
    /// </summary>
    /// <param name="tableau">Tabella del simplesso (matrice dei coefficienti con l'ultima riga come funzione obiettivo e l'ultima colonna come termini noti).</param>
    /// <param name="maximize"><c>true</c> per massimizzare, <c>false</c> per minimizzare.</param>
    /// <returns>Array di lunghezza <c>numVariabili + 1</c> dove le prime <c>numVariabili</c> posizioni sono la soluzione e l'ultima è il valore ottimo.</returns>
    /// <exception cref="InvalidOperationException">Se il problema non è limitato (soluzione infinita).</exception>
    /// <example>
    /// double[,] tableau = {
    ///     { 2.0, 1.0, 0.0, 0.0, 10.0 },
    ///     { 1.0, 2.0, 1.0, 0.0, 8.0 },
    ///     { -3.0, -2.0, 0.0, 1.0, 0.0 }
    /// };
    /// double[] sol = Numerics.Simplex(tableau, maximize: true);
    /// </example>
    public static double[] Simplex(double[,] tableau, bool maximize)
    {
        var numRows = tableau.GetLength(0);
        var numCols = tableau.GetLength(1);
        const double epsilon = 1e-9;

        if (!maximize)
        {
            for (var j = 0; j < numCols - 1; j++)
                tableau[numRows - 1, j] *= -1;
        }

        while (true)
        {
            var pivotCol = -1;
            var maxVal = epsilon;
            for (var j = 0; j < numCols - 1; j++)
            {
                if (tableau[numRows - 1, j] > maxVal)
                {
                    maxVal = tableau[numRows - 1, j];
                    pivotCol = j;
                }
            }
            if (pivotCol == -1) break;

            var pivotRow = -1;
            var minRatio = double.MaxValue;
            for (var i = 0; i < numRows - 1; i++)
            {
                if (tableau[i, pivotCol] > epsilon)
                {
                    var ratio = tableau[i, numCols - 1] / tableau[i, pivotCol];
                    if (ratio < minRatio)
                    {
                        minRatio = ratio;
                        pivotRow = i;
                    }
                }
            }
            if (pivotRow == -1) throw new InvalidOperationException("Il problema non è limitato (soluzione infinita).");

            var pivotElement = tableau[pivotRow, pivotCol];
            for (var j = 0; j < numCols; j++)
                tableau[pivotRow, j] /= pivotElement;

            for (var i = 0; i < numRows; i++)
            {
                if (i == pivotRow) continue;
                var multiplier = tableau[i, pivotCol];
                for (var j = 0; j < numCols; j++)
                    tableau[i, j] -= multiplier * tableau[pivotRow, j];
            }
        }

        var numVars = numCols - numRows;
        var solution = new double[numVars + 1];
        for (var j = 0; j < numVars; j++)
        {
            var isBasic = false;
            for (var i = 0; i < numRows - 1; i++)
            {
                if (Math.Abs(tableau[i, j] - 1.0) < epsilon && !Enumerable.Range(0, numRows - 1)
                        .Any(ii => ii != i && Math.Abs(tableau[ii, j]) > epsilon))
                {
                    solution[j] = tableau[i, numCols - 1];
                    isBasic = true;
                    break;
                }
            }
            if (!isBasic) solution[j] = 0;
        }
        solution[numVars] = tableau[numRows - 1, numCols - 1] * (maximize ? 1 : -1);
        return solution;
    }

    /// <summary>
    /// Trova la radice di una funzione usando il metodo di Newton-Raphson.
    /// </summary>
    /// <param name="f_and_df">Funzione che restituisce una tupla (f(x), f'(x)).</param>
    /// <param name="initialGuess">Stima iniziale per la radice.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Radice trovata, o <c>double.NaN</c> se la convergenza fallisce.</returns>
    /// <example>
    /// double root = Numerics.NewtonRaphson(
    ///     x => (x * x - 2.0, 2.0 * x),
    ///     initialGuess: 1.5,
    ///     tolerance: 1e-8,
    ///     maxIterations: 100);
    /// // root ≈ 1.4142 (radice di 2)
    /// </example>
    public static double NewtonRaphson(Func<double, (double, double)> f_and_df, double initialGuess,
        double tolerance, int maxIterations)
    {
        if (MathNet.Numerics.RootFinding.NewtonRaphson.TryFindRoot(Func, Df, initialGuess, double.MinValue,
                double.MaxValue, tolerance,
                maxIterations, out double root))
        {
            return root;
        }
        return double.NaN;

        double Df(double x) => f_and_df(x).Item2;
        double Func(double x) => f_and_df(x).Item1;
    }

    /// <summary>
    /// Trova la radice di una funzione usando il metodo di Brent.
    /// Combina bisezione, secante e interpolazione quadratica inversa.
    /// Richiede che <paramref name="lowerBound"/> e <paramref name="upperBound"/> abbiano segno opposto.
    /// </summary>
    /// <param name="f">Funzione di cui trovare la radice.</param>
    /// <param name="lowerBound">Estremo inferiore dell'intervallo di ricerca.</param>
    /// <param name="upperBound">Estremo superiore dell'intervallo di ricerca.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Radice trovata, o <c>double.NaN</c> se la convergenza fallisce.</returns>
    /// <example>
    /// double root = Numerics.Brent(x => x * x - 2, 0.0, 2.0, 1e-8, 100);
    /// </example>
    public static double Brent(Func<double, double> f, double lowerBound, double upperBound, double tolerance,
        int maxIterations)
    {
        if (MathNet.Numerics.RootFinding.Brent.TryFindRoot(f, lowerBound, upperBound, tolerance, maxIterations,
                out double root))
            return root;
        return double.NaN;
    }

    /// <summary>
    /// Trova la radice di un sistema di equazioni non lineari usando il metodo di Broyden
    /// (approssimazione quasi-Newton). Esegue la minimizzazione della norma L2 del vettore funzione.
    /// </summary>
    /// <param name="f">Funzione vettoriale di cui trovare la radice.</param>
    /// <param name="initialGuess">Vettore di stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Vettore radice, o un vettore di <c>NaN</c> se la convergenza fallisce.</returns>
    /// <example>
    /// var root = Numerics.Broyden(
    ///     v => CreateVector.Dense(new[] { v[0]*v[0] + v[1] - 1.0, v[1]*v[1] - v[0] }),
    ///     CreateVector.Dense(new[] { 0.5, 0.5 }),
    ///     1e-8, 100);
    /// </example>
    public static Vector<double> Broyden(Func<Vector<double>, Vector<double>> f, Vector<double> initialGuess,
        double tolerance, int maxIterations)
    {
        var point = initialGuess.Clone();
        var value = f(point);
        if (value.Count != point.Count)
            return Vector<double>.Build.Dense(initialGuess.Count, double.NaN);
        if (value.L2Norm() <= tolerance)
            return point;

        var dimension = point.Count;
        var jacobian = Matrix<double>.Build.Dense(dimension, dimension);
        var stepScale = Math.Sqrt(Math.BitIncrement(1d) - 1d);
        for (var column = 0; column < dimension; column++)
        {
            var probe = point.Clone();
            var step = stepScale * Math.Max(1d, Math.Abs(point[column]));
            probe[column] += step;
            var probeValue = f(probe);
            if (probeValue.Count != dimension)
                return Vector<double>.Build.Dense(dimension, double.NaN);
            jacobian.SetColumn(column, (probeValue - value) / step);
        }

        for (var iteration = 0; iteration < maxIterations; iteration++)
        {
            Vector<double> delta;
            try
            {
                delta = jacobian.Solve(-value);
            }
            catch (ArgumentException)
            {
                return Vector<double>.Build.Dense(dimension, double.NaN);
            }

            var nextPoint = point + delta;
            var nextValue = f(nextPoint);
            if (nextValue.Count != dimension)
                return Vector<double>.Build.Dense(dimension, double.NaN);
            if (nextValue.L2Norm() <= tolerance || delta.L2Norm() <= tolerance)
                return nextPoint;

            var valueDelta = nextValue - value;
            var denominator = delta.DotProduct(delta);
            if (denominator <= double.Epsilon)
                return nextPoint;
            jacobian += (valueDelta - jacobian * delta).OuterProduct(delta) / denominator;
            point = nextPoint;
            value = nextValue;
        }

        return Vector<double>.Build.Dense(dimension, double.NaN);
    }

    /// <summary>
    /// Minimizza una funzione multivariata usando il metodo Nelder-Mead (simplex diretto).
    /// Non richiede il calcolo del gradiente.
    /// </summary>
    /// <param name="f">Funzione obiettivo da minimizzare.</param>
    /// <param name="initialGuess">Vettore di stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Risultato della minimizzazione (<c>MinimizationResult</c>).</returns>
    /// <example>
    /// var result = Numerics.NelderMead(
    ///     v => v[0]*v[0] + v[1]*v[1],
    ///     CreateVector.Dense(new[] { 1.0, 1.0 }),
    ///     1e-6, 500);
    /// </example>
    public static MinimizationResult NelderMead(Func<Vector<double>, double> f, Vector<double> initialGuess,
        double tolerance, int maxIterations)
    {
        var objectiveFunction = ObjectiveFunction.Value(f);
        var result = NelderMeadSimplex.Minimum(objectiveFunction, initialGuess, tolerance, maxIterations);
        return result;
    }

    /// <summary>
    /// Trova la radice di una funzione usando il metodo di bisezione.
    /// Richiede che <paramref name="lowerBound"/> e <paramref name="upperBound"/> abbiano segno opposto.
    /// </summary>
    /// <param name="f">Funzione di cui trovare la radice.</param>
    /// <param name="lowerBound">Estremo inferiore dell'intervallo di ricerca.</param>
    /// <param name="upperBound">Estremo superiore dell'intervallo di ricerca.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Radice trovata, o <c>double.NaN</c> se la convergenza fallisce.</returns>
    /// <example>
    /// double root = Numerics.Bisection(x => x * x - 2, 0.0, 2.0, 1e-8, 100);
    /// </example>
    public static double Bisection(Func<double, double> f, double lowerBound, double upperBound, double tolerance,
        int maxIterations)
    {
        if (MathNet.Numerics.RootFinding.Bisection.TryFindRoot(f, lowerBound, upperBound, tolerance, maxIterations,
                out double root))
            return root;
        return double.NaN;
    }

    /// <summary>
    /// Trova la radice di una funzione usando il metodo delle secanti.
    /// Non richiede il calcolo della derivata.
    /// </summary>
    /// <param name="f">Funzione di cui trovare la radice.</param>
    /// <param name="x0">Prima stima iniziale.</param>
    /// <param name="x1">Seconda stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Radice trovata, o <c>double.NaN</c> se la convergenza fallisce.</returns>
    /// <example>
    /// double root = Numerics.Secant(x => x * x - 2, 1.0, 2.0, 1e-8, 100);
    /// </example>
    public static double Secant(Func<double, double> f, double x0, double x1, double tolerance, int maxIterations)
    {
        var previous = x0;
        var current = x1;
        var previousValue = f(previous);
        var currentValue = f(current);

        for (var iteration = 0; iteration < maxIterations; iteration++)
        {
            if (Math.Abs(currentValue) <= tolerance)
                return current;

            var denominator = currentValue - previousValue;
            if (Math.Abs(denominator) <= double.Epsilon)
                return double.NaN;

            var next = current - currentValue * (current - previous) / denominator;
            if (!double.IsFinite(next))
                return double.NaN;
            if (Math.Abs(next - current) <= tolerance)
                return next;

            previous = current;
            previousValue = currentValue;
            current = next;
            currentValue = f(current);
        }

        return double.NaN;
    }

    /// <summary>
    /// Trova il minimo di una funzione unimodale usando il metodo della sezione aurea.
    /// </summary>
    /// <param name="f">Funzione obiettivo unimodale da minimizzare.</param>
    /// <param name="lower">Estremo inferiore dell'intervallo di ricerca.</param>
    /// <param name="upper">Estremo superiore dell'intervallo di ricerca.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Punto X del minimo trovato nell'intervallo.</returns>
    /// <example>
    /// double min = Numerics.GoldenSection(x => (x - 2.0) * (x - 2.0), -10.0, 10.0, 1e-6, 100);
    /// // min ≈ 2.0
    /// </example>
    public static double GoldenSection(Func<double, double> f, double lower, double upper, double tolerance,
        int maxIterations)
    {
        const double phi = 0.618033988749895;
        double a = lower, b = upper;
        var x1 = b - phi * (b - a);
        var x2 = a + phi * (b - a);
        var f1 = f(x1);
        var f2 = f(x2);
        var iter = 0;
        while (b - a > tolerance && iter < maxIterations)
        {
            if (f1 < f2)
            {
                b = x2;
                x2 = x1;
                f2 = f1;
                x1 = b - phi * (b - a);
                f1 = f(x1);
            }
            else
            {
                a = x1;
                x1 = x2;
                f1 = f2;
                x2 = a + phi * (b - a);
                f2 = f(x2);
            }
            iter++;
        }
        return (a + b) / 2;
    }

    /// <summary>
    /// Minimizza una funzione multivariata usando l'algoritmo BFGS a memoria piena.
    /// </summary>
    /// <param name="f">Funzione obiettivo da minimizzare.</param>
    /// <param name="initialGuess">Vettore di stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Risultato della minimizzazione (<c>MinimizationResult</c>).</returns>
    /// <example>
    /// var result = Numerics.Bfgs(v => v[0]*v[0] + v[1]*v[1], CreateVector.Dense(new[] { 1.0, 1.0 }), 1e-6, 100);
    /// </example>
    public static MinimizationResult Bfgs(Func<Vector<double>, double> f, Vector<double> initialGuess,
        double tolerance, int maxIterations)
    {
        var objectiveFunction = new NumericalGradientObjective(f);
        var solver = new BfgsMinimizer(tolerance, tolerance, tolerance, maxIterations);
        return solver.FindMinimum(objectiveFunction, initialGuess);
    }

    /// <summary>
    /// Minimizza una funzione multivariata usando l'algoritmo L-BFGS a memoria limitata.
    /// Adatto a problemi con un gran numero di parametri.
    /// </summary>
    /// <param name="f">Funzione obiettivo da minimizzare.</param>
    /// <param name="initialGuess">Vettore di stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Risultato della minimizzazione (<c>MinimizationResult</c>).</returns>
    /// <example>
    /// var result = Numerics.LBfgs(v => v[0]*v[0] + v[1]*v[1], CreateVector.Dense(new[] { 1.0, 1.0 }), 1e-6, 100);
    /// </example>
    public static MinimizationResult LBfgs(Func<Vector<double>, double> f, Vector<double> initialGuess,
        double tolerance, int maxIterations)
    {
        var objectiveFunction = new NumericalGradientObjective(f);
        var solver = new LimitedMemoryBfgsMinimizer(tolerance, tolerance, tolerance, maxIterations);
        return solver.FindMinimum(objectiveFunction, initialGuess);
    }

    /// <summary>
    /// Minimizza una funzione multivariata usando il metodo del gradiente coniugato non lineare.
    /// </summary>
    /// <param name="f">Funzione obiettivo da minimizzare.</param>
    /// <param name="initialGuess">Vettore di stima iniziale.</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Risultato della minimizzazione (<c>MinimizationResult</c>).</returns>
    /// <example>
    /// var result = Numerics.ConjugateGradient(v => v[0]*v[0] + v[1]*v[1], CreateVector.Dense(new[] { 1.0, 1.0 }), 1e-6, 100);
    /// </example>
    public static MinimizationResult ConjugateGradient(Func<Vector<double>, double> f,
        Vector<double> initialGuess, double tolerance, int maxIterations)
    {
        var objectiveFunction = new NumericalGradientObjective(f);
        var solver = new ConjugateGradientMinimizer(tolerance, maxIterations);
        return solver.FindMinimum(objectiveFunction, initialGuess);
    }

    /// <summary>
    /// Esegue la minimizzazione non lineare ai minimi quadrati con l'algoritmo di Levenberg-Marquardt.
    /// </summary>
    /// <param name="f">Funzione modello non lineare: f(parametri, input) → previsione.</param>
    /// <param name="initialGuess">Vettore di stima iniziale dei parametri.</param>
    /// <param name="inputs">Vettore degli input.</param>
    /// <param name="targets">Vettore dei target (valori osservati).</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Vettore dei parametri ottimizzati.</returns>
    /// <example>
    /// var result = Numerics.LevenbergMarquardt(
    ///     (p, x) => p[0] * Math.Exp(-p[1] * x),
    ///     CreateVector.Dense(new[] { 1.0, 0.5 }),
    ///     inputs, targets, 1e-8, 100);
    /// </example>
    public static Vector<double> LevenbergMarquardt(
        Func<Vector<double>, double, double> f,
        Vector<double> initialGuess,
        Vector<double> inputs,
        Vector<double> targets,
        double tolerance, int maxIterations)
    {
        // Bug pre-esistente: gli argomenti erano passati nell'ordine sbagliato rispetto alla
        // vera firma di ObjectiveFunction.NonlinearModel(function, observedX, observedY, weight,
        // accuracyOrder) — initialGuess finiva in observedX, inputs in observedY, targets come
        // weight, e inputs.Count (potenzialmente enorme) come ordine di accuratezza delle
        // differenze finite invece di un piccolo intero. Corretto: observedX=inputs,
        // observedY=targets, weight=vettore di 1 (nessuna ponderazione), accuracyOrder=2
        // (schema centrale standard).
        var solver = new LevenbergMarquardtMinimizer(maxIterations, tolerance, tolerance, tolerance);
        var weights = Vector<double>.Build.Dense(inputs.Count, 1.0);
        var obj = ObjectiveFunction.NonlinearModel(f, inputs, targets, weights, 2);
        var result = solver.FindMinimum(obj, initialGuess);
        return result.MinimizingPoint;
    }

    /// <summary>
    /// Esegue la minimizzazione non lineare ai minimi quadrati con Levenberg-Marquardt,
    /// dove il modello restituisce un vettore invece di uno scalare.
    /// </summary>
    /// <param name="f">Funzione modello vettoriale: f(parametri, input) → vettore previsione.</param>
    /// <param name="initialGuess">Vettore di stima iniziale dei parametri.</param>
    /// <param name="inputs">Input del modello (vettore).</param>
    /// <param name="targets">Target osservati (vettore).</param>
    /// <param name="tolerance">Tolleranza per la convergenza.</param>
    /// <param name="maxIterations">Numero massimo di iterazioni.</param>
    /// <returns>Vettore dei parametri ottimizzati.</returns>
    /// <example>
    /// var result = Numerics.LevenbergMarquardtVec(
    ///     (p, x) => CreateVector.Dense(new[] { p[0] * x[0], p[1] * x[1] }),
    ///     CreateVector.Dense(new[] { 1.0, 1.0 }),
    ///     inputs, targets, 1e-8, 100);
    /// </example>
    public static Vector<double> LevenbergMarquardtVec(
        Func<Vector<double>, Vector<double>, Vector<double>> f,
        Vector<double> initialGuess,
        Vector<double> inputs,
        Vector<double> targets,
        double tolerance, int maxIterations)
    {
        // Stesso bug di LevenbergMarquardt sopra: argomenti nell'ordine sbagliato rispetto alla
        // vera firma di NonlinearModel.
        var solver = new LevenbergMarquardtMinimizer(maxIterations, tolerance, tolerance, tolerance);
        var weights = Vector<double>.Build.Dense(inputs.Count, 1.0);
        var obj = ObjectiveFunction.NonlinearModel(f, inputs, targets, weights, 2);
        var result = solver.FindMinimum(obj, initialGuess);
        return result.MinimizingPoint;
    }

    /// <summary>
    /// Risolve il sistema lineare Ax = b nel senso dei minimi quadrati.
    /// </summary>
    /// <param name="A">Matrice dei coefficienti.</param>
    /// <param name="b">Vettore dei termini noti.</param>
    /// <returns>Vettore soluzione x che minimizza ||Ax − b||².</returns>
    /// <example>
    /// var x = Numerics.LeastSquares(A, b);
    /// </example>
    public static Vector<double> LeastSquares(Matrix<double> A, Vector<double> b)
    {
        return A.Solve(b);
    }

    /// <summary>
    /// Risolve il sistema lineare Ax = b usando la decomposizione di Cholesky.
    /// La matrice A deve essere simmetrica e definita positiva.
    /// </summary>
    /// <param name="A">Matrice simmetrica definita positiva.</param>
    /// <param name="b">Vettore dei termini noti.</param>
    /// <returns>Vettore soluzione x.</returns>
    /// <exception cref="ArgumentException">Se A non è definita positiva.</exception>
    /// <example>
    /// var x = Numerics.CholeskySolve(A, b);
    /// </example>
    public static Vector<double> CholeskySolve(Matrix<double> A, Vector<double> b)
    {
        var cholesky = A.Cholesky();
        return cholesky.Solve(b);
    }
}
