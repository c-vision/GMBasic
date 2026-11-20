// FILE: StdLib.Numerics.Interpolation.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

public static partial class Numerics
{
    /// <summary>
    /// Interpola il valore <paramref name="targetX"/> utilizzando un'interpolazione lineare a tratti.
    /// Richiede almeno 2 punti.
    /// </summary>
    /// <param name="xPoints">Coordinate X dei punti noti (ordinate crescenti, senza duplicati).</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <returns>Valore Y interpolato linearmente in <paramref name="targetX"/>.</returns>
    /// <exception cref="ArgumentNullException">Se uno degli array è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa, meno di 2 punti, non ordinati o con duplicati.</exception>
    /// <example>
    /// double y = Numerics.InterpolateLinear(
    ///     new[] { 0.0, 1.0, 2.0 },
    ///     new[] { 0.0, 2.0, 4.0 },
    ///     targetX: 0.5);
    /// // y = 1.0
    /// </example>
    public static double InterpolateLinear(double[] xPoints, double[] yPoints, double targetX)
    {
        ValidateInterpolationInputs(xPoints, yPoints, minCount: 2);
        if (!IsSortedAscending(xPoints))
            throw new ArgumentException("L'array xPoints deve essere ordinato in modo crescente.");
        if (HasDuplicates(xPoints))
            throw new ArgumentException("L'array xPoints non deve contenere duplicati.");
        return InterpolationNative.Linear(xPoints, yPoints, xPoints.Length, targetX);
    }

    /// <summary>
    /// Interpola il valore <paramref name="targetX"/> utilizzando una spline cubica di Akima.
    /// Richiede almeno 3 punti.
    /// </summary>
    /// <param name="xPoints">Coordinate X dei punti noti (ordinate crescenti, senza duplicati).</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <returns>Valore Y interpolato in <paramref name="targetX"/>.</returns>
    /// <exception cref="ArgumentNullException">Se uno degli array è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa, meno di 3 punti, non ordinati o con duplicati.</exception>
    /// <example>
    /// double y = Numerics.InterpolateSpline(
    ///     new[] { 0.0, 1.0, 2.0, 3.0 },
    ///     new[] { 0.0, 0.5, 2.0, 1.5 },
    ///     targetX: 1.5);
    /// </example>
    public static double InterpolateSpline(double[] xPoints, double[] yPoints, double targetX)
    {
        ValidateInterpolationInputs(xPoints, yPoints, minCount: 3);
        if (!IsSortedAscending(xPoints))
            throw new ArgumentException("L'array xPoints deve essere ordinato in modo crescente.");
        if (HasDuplicates(xPoints))
            throw new ArgumentException("L'array xPoints non deve contenere duplicati.");
        return InterpolationNative.Akima(xPoints, yPoints, xPoints.Length, targetX);
    }

    /// <summary>
    /// Interpola il valore <paramref name="targetX"/> utilizzando il metodo polinomiale
    /// barycentric Lagrange. Funziona con qualsiasi numero di punti.
    /// </summary>
    /// <param name="x">Coordinate X dei punti noti.</param>
    /// <param name="y">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <returns>Valore Y interpolato.</returns>
    /// <example>
    /// double y = Numerics.InterpolatePolynomial(
    ///     new[] { 0.0, 1.0, 2.0 },
    ///     new[] { 1.0, 2.0, 4.0 },
    ///     targetX: 1.5);
    /// </example>
    public static double InterpolatePolynomial(double[] x, double[] y, double targetX)
    {
        // n == 0 dava NaN (0/0) nel calcolo barycentric: mantenuto esplicitamente.
        if (x.Length == 0) return double.NaN;
        return InterpolationNative.Polynomial(x, y, x.Length, targetX);
    }

    /// <summary>
    /// Interpola il valore <paramref name="targetX"/> utilizzando PCHIP (Piecewise Cubic Hermite
    /// Interpolating Polynomial), che preserva la monotonia dei dati.
    /// Richiede almeno 3 punti.
    /// </summary>
    /// <param name="xPoints">Coordinate X dei punti noti (ordinate crescenti, senza duplicati).</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <returns>Valore Y interpolato in <paramref name="targetX"/>.</returns>
    /// <exception cref="ArgumentNullException">Se uno degli array è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa, meno di 3 punti, non ordinati o con duplicati.</exception>
    /// <example>
    /// double y = Numerics.InterpolatePchip(xPoints, yPoints, targetX: 2.5);
    /// </example>
    public static double InterpolatePchip(double[] xPoints, double[] yPoints, double targetX)
    {
        ValidateInterpolationInputs(xPoints, yPoints, minCount: 3);
        if (!IsSortedAscending(xPoints))
            throw new ArgumentException("L'array xPoints deve essere ordinato in modo crescente.");
        if (HasDuplicates(xPoints))
            throw new ArgumentException("L'array xPoints non deve contenere duplicati.");
        return InterpolationNative.Pchip(xPoints, yPoints, xPoints.Length, targetX);
    }

    /// <summary>
    /// Interpola il valore <paramref name="targetX"/> con una funzione a gradino (step).
    /// Restituisce il valore Y del punto noto immediatamente precedente a <paramref name="targetX"/>.
    /// </summary>
    /// <param name="xPoints">Coordinate X dei punti noti (ordinate crescenti).</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <returns>Valore Y del gradino in <paramref name="targetX"/>.</returns>
    /// <exception cref="ArgumentNullException">Se uno degli array è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa o meno di 2 punti.</exception>
    /// <example>
    /// double y = Numerics.InterpolateStep(
    ///     new[] { 0.0, 2.0, 5.0 },
    ///     new[] { 1.0, 3.0, 4.0 },
    ///     targetX: 3.0);
    /// // y = 3.0 (gradino fino a x=5)
    /// </example>
    public static double InterpolateStep(double[] xPoints, double[] yPoints, double targetX)
    {
        ValidateInterpolationInputs(xPoints, yPoints, minCount: 2);
        if (!IsSortedAscending(xPoints))
            throw new ArgumentException("L'array xPoints deve essere ordinato in modo crescente.");
        if (HasDuplicates(xPoints))
            throw new ArgumentException("L'array xPoints non deve contenere duplicati.");
        return InterpolationNative.Step(xPoints, yPoints, xPoints.Length, targetX);
    }

    /// <summary>
    /// Esegue l'interpolazione con clamping: se <paramref name="targetX"/> è fuori dal range
    /// dei punti noti, restituisce il valore Y del bordo più vicino.
    /// </summary>
    /// <param name="xPoints">Coordinate X dei punti noti.</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <param name="interpolationFunc">Funzione di interpolazione da usare per i punti interni.</param>
    /// <returns>Valore Y in <paramref name="targetX"/>, clampato ai bordi se necessario.</returns>
    /// <exception cref="ArgumentNullException">Se uno degli array è null o <paramref name="interpolationFunc"/> è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa o meno di 2 punti.</exception>
    /// <example>
    /// double y = Numerics.InterpolateClamp(x, y, 10.0, Numerics.InterpolateLinear);
    /// </example>
    public static double InterpolateClamp(double[] xPoints, double[] yPoints, double targetX,
        Func<double[], double[], double, double> interpolationFunc)
    {
        ValidateInterpolationInputs(xPoints, yPoints, minCount: 2);
        if (targetX < xPoints[0]) return yPoints[0];
        return targetX > xPoints[^1] ? yPoints[^1] : interpolationFunc(xPoints, yPoints, targetX);
    }

    /// <summary>
    /// Dispatcher generico per interpolazione. Seleziona il metodo in base al parametro <paramref name="type"/>
    /// e opzionalmente applica clamping.
    /// </summary>
    /// <param name="type">Tipo di interpolazione: "LINEAR", "SPLINE", "POLY"/"POLYNOMIAL", "PCHIP", "STEP".</param>
    /// <param name="xPoints">Coordinate X dei punti noti.</param>
    /// <param name="yPoints">Coordinate Y corrispondenti.</param>
    /// <param name="targetX">Punto X in cui calcolare il valore interpolato.</param>
    /// <param name="clamp">Se <c>true</c>, applica clamping ai bordi (default: false).</param>
    /// <returns>Valore Y interpolato in <paramref name="targetX"/>.</returns>
    /// <exception cref="ArgumentException">Se <paramref name="type"/> non è supportato.</exception>
    /// <example>
    /// double y = Numerics.InterpolateGeneric("SPLINE", x, y, 1.5, clamp: true);
    /// </example>
    public static double InterpolateGeneric(string type, double[] xPoints, double[] yPoints, double targetX,
        bool clamp = false)
    {
        Func<double[], double[], double, double> interpFunc = type.ToUpperInvariant() switch
        {
            "LINEAR" => InterpolateLinear,
            "SPLINE" => InterpolateSpline,
            "POLY" or "POLYNOMIAL" => InterpolatePolynomial,
            "PCHIP" => InterpolatePchip,
            "STEP" => InterpolateStep,
            _ => throw new ArgumentException($"Tipo di interpolazione non supportato: {type}")
        };
        if (clamp)
            return InterpolateClamp(xPoints, yPoints, targetX, interpFunc);
        return interpFunc(xPoints, yPoints, targetX);
    }

    /// <summary>
    /// Interpola con una spline cubica not-a-knot (default di scipy.interpolate.CubicSpline,
    /// bc_type="not-a-knot"): le derivate terze sono continue ai nodi interni x[1] e x[n-2].
    /// </summary>
    public static double InterpolateCubicSpline(double[] x, double[] y, double targetX)
    {
        ValidateInterpolationInputs(x, y, minCount: 3);
        if (!IsSortedAscending(x))
            throw new ArgumentException("L'array xPoints deve essere ordinato in modo crescente.");
        if (HasDuplicates(x))
            throw new ArgumentException("L'array xPoints non deve contenere duplicati.");
        return InterpolationNative.CubicSpline(x, y, x.Length, targetX);
    }

    /// <summary>Eliminazione di Gauss con pivot parziale per un sistema denso n×n.</summary>


    /// <summary>
    /// Valida gli input per le funzioni di interpolazione.
    /// Verifica che gli array non siano null, abbiano la stessa lunghezza e almeno <paramref name="minCount"/> punti.
    /// </summary>
    /// <param name="x">Array delle coordinate X.</param>
    /// <param name="y">Array delle coordinate Y.</param>
    /// <param name="minCount">Numero minimo di punti richiesto.</param>
    /// <exception cref="ArgumentNullException">Se <paramref name="x"/> o <paramref name="y"/> è null.</exception>
    /// <exception cref="ArgumentException">Se gli array hanno lunghezza diversa o sono troppo corti.</exception>
    private static void ValidateInterpolationInputs(double[] x, double[] y, int minCount)
    {
        if (x == null)
            throw new ArgumentNullException(nameof(x), "L'array dei punti X non può essere null.");
        if (y == null)
            throw new ArgumentNullException(nameof(y), "L'array dei punti Y non può essere null.");
        if (x.Length != y.Length)
            throw new ArgumentException("Gli array di punti X e Y devono avere la stessa lunghezza.");
        if (x.Length < minCount)
            throw new ArgumentException($"Servono almeno {minCount} punti per questo tipo di interpolazione.");
    }

    /// <summary>
    /// Verifica che l'array sia ordinato in senso strettamente crescente.
    /// </summary>
    /// <param name="arr">Array da verificare.</param>
    /// <returns><c>true</c> se l'array è strettamente crescente, altrimenti <c>false</c>.</returns>
    private static bool IsSortedAscending(double[] arr)
    {
        for (var i = 1; i < arr.Length; i++)
            if (arr[i] <= arr[i - 1])
                return false;
        return true;
    }

    /// <summary>
    /// Verifica se l'array contiene valori duplicati.
    /// </summary>
    /// <param name="arr">Array da verificare.</param>
    /// <returns><c>true</c> se sono presenti duplicati, altrimenti <c>false</c>.</returns>
    private static bool HasDuplicates(double[] arr)
    {
        var set = new HashSet<double>();
        foreach (var x in arr)
            if (!set.Add(x))
                return true;
        return false;
    }
}
