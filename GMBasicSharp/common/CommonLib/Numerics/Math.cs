// FILE: StdLib.Numerics.Math.cs
// ReSharper disable CheckNamespace

using System.Globalization;
using System.Numerics;

namespace CommonLib;

/// <summary>
/// Fornisce funzioni matematiche avanzate per il runtime BASIC, oltre alle operazioni di base.
/// Contiene i seguenti metodi:
/// <list type="bullet">
///   <item><description><c>Sqr</c> — Radice quadrata con controllo argomento negativo.</description></item>
///   <item><description><c>Asin</c> — Arcoseno con controllo del dominio.</description></item>
///   <item><description><c>Acos</c> — Arcocoseno con controllo del dominio.</description></item>
///   <item><description><c>IsNumeric</c> — Verifica se un oggetto è un valore numerico.</description></item>
///   <item><description><c>Round</c> — Arrotondamento decimale di un valore decimal (banker's rounding).</description></item>
///   <item><description><c>Trunc</c> — Troncamento decimale con precisione specificata.</description></item>
///   <item><description><c>Fix</c> — Troncamento della parte decimale (equivalente a INT).</description></item>
///   <item><description><c>Count</c> — Conteggio elementi in una lista.</description></item>
///   <item><description><c>Sum</c> — Somma di una lista di oggetti convertibili in decimal.</description></item>
///   <item><description><c>Avg</c> — Media aritmetica di una lista di oggetti.</description></item>
///   <item><description><c>MinInList</c> — Valore minimo in una lista di oggetti comparabili.</description></item>
///   <item><description><c>MaxInList</c> — Valore massimo in una lista di oggetti comparabili.</description></item>
///   <item><description><c>Clamp</c> — Vincola un oggetto IComparable tra minimo e massimo.</description></item>
///   <item><description><c>Factorial</c> — Fattoriale esatto (ulong, limite n ≤ 20).</description></item>
///   <item><description><c>Perm</c> — Permutazioni (nPk).</description></item>
///   <item><description><c>Comb</c> — Combinazioni (nCk).</description></item>
///   <item><description><c>Hypot</c> — Ipotenusa (norma euclidea 2D).</description></item>
///   <item><description><c>Dist2d</c> — Distanza euclidea tra due punti 2D.</description></item>
///   <item><description><c>Dist3d</c> — Distanza euclidea tra due punti 3D.</description></item>
///   <item><description><c>PolarX</c> — Coordinata X da coordinate polari.</description></item>
///   <item><description><c>PolarY</c> — Coordinata Y da coordinate polari.</description></item>
///   <item><description><c>Angle2d</c> — Angolo in gradi tra due punti 2D.</description></item>
///   <item><description><c>ComputeModMulInverse</c> — Inverso moltiplicativo modulare (long).</description></item>
///   <item><description><c>ComputeModMulInverse</c> — Inverso moltiplicativo modulare (BigInteger).</description></item>
/// </list>
/// </summary>
public static partial class Numerics
{
    /// <summary>
    /// Calcola la radice quadrata del valore specificato.
    /// Genera un'eccezione se il valore è negativo (compatibilità BASIC).
    /// </summary>
    /// <param name="value">Valore di cui calcolare la radice quadrata. Deve essere maggiore o uguale a zero.</param>
    /// <returns>La radice quadrata del valore di input.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se value è negativo.</exception>
    /// <example>double result = Numerics.Sqr(9); // restituisce 3.0</example>
    public static double Sqr(double value)
    {
        if (value < 0)
            throw new ArgumentOutOfRangeException(nameof(value), "L'argomento di SQR non può essere negativo.");
        return Math.Sqrt(value);
    }

    /// <summary>
    /// Calcola l'arcoseno di un valore double.
    /// Genera un'eccezione se il valore è al di fuori dell'intervallo [-1, 1].
    /// </summary>
    /// <param name="value">Valore compreso tra -1 e 1 di cui calcolare l'arcoseno.</param>
    /// <returns>L'arcoseno in radianti.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se value è minore di -1 o maggiore di 1.</exception>
    /// <example>double result = Numerics.Asin(0.5); // restituisce circa 0.5236</example>
    public static double Asin(double value)
    {
        if (value is < -1 or > 1)
            throw new ArgumentOutOfRangeException(nameof(value), "L'argomento di ASIN deve essere tra -1 e 1.");
        return Math.Asin(value);
    }

    /// <summary>
    /// Calcola l'arcocoseno di un valore double.
    /// Genera un'eccezione se il valore è al di fuori dell'intervallo [-1, 1].
    /// </summary>
    /// <param name="value">Valore compreso tra -1 e 1 di cui calcolare l'arcocoseno.</param>
    /// <returns>L'arcocoseno in radianti.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se value è minore di -1 o maggiore di 1.</exception>
    /// <example>double result = Numerics.Acos(0.5); // restituisce circa 1.0472</example>
    public static double Acos(double value)
    {
        if (value is < -1 or > 1)
            throw new ArgumentOutOfRangeException(nameof(value), "L'argomento di ACOS deve essere tra -1 e 1.");
        return Math.Acos(value);
    }

    /// <summary>
    /// Verifica se un oggetto è un valore numerico (intero, decimale, float, double, ecc.)
    /// o una stringa convertibile in numero decimale.
    /// </summary>
    /// <param name="value">Oggetto da verificare.</param>
    /// <returns>true se l'oggetto è un tipo numerico o una stringa numerica valida; altrimenti false.</returns>
    /// <example>bool result = Numerics.IsNumeric("123.45"); // restituisce true</example>
    public static bool IsNumeric(object? value)
    {
        if (value is string s)
            return decimal.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, out _);
        return value is sbyte or byte or short or ushort or int or uint or long or ulong or float or double or decimal;
    }

    /// <summary>
    /// Arrotonda un valore decimal al numero specificato di cifre decimali,
    /// utilizzando l'arrotondamento AwayFromZero (standard BASIC).
    /// </summary>
    /// <param name="number">Valore decimal da arrotondare.</param>
    /// <param name="decimals">Numero di cifre decimali (0-28).</param>
    /// <returns>Il valore arrotondato al numero specificato di decimali.</returns>
    /// <example>decimal result = Numerics.Round(3.14159m, 2); // restituisce 3.14m</example>
    public static decimal Round(decimal number, int decimals)
    {
        return Math.Round(number, decimals, MidpointRounding.AwayFromZero);
    }

    /// <summary>
    /// Tronca un valore decimal al numero specificato di cifre decimali.
    /// </summary>
    /// <param name="number">Valore decimal da troncare.</param>
    /// <param name="decimals">Numero di cifre decimali da mantenere. Deve essere maggiore o uguale a zero.</param>
    /// <returns>Il valore troncato al numero specificato di decimali.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se decimals è negativo.</exception>
    /// <example>decimal result = Numerics.Trunc(3.14159m, 2); // restituisce 3.14m</example>
    public static decimal Trunc(decimal number, int decimals)
    {
        if (decimals < 0)
            throw new ArgumentOutOfRangeException(nameof(decimals), "I decimali non possono essere negativi.");
        if (decimals == 0)
            return Math.Truncate(number);
        var factor = (decimal)Math.Pow(10, decimals);
        return Math.Truncate(number * factor) / factor;
    }

    /// <summary>
    /// Restituisce la parte intera di un valore double, troncando la parte decimale.
    /// Equivalente alla funzione INT/FIX del BASIC.
    /// </summary>
    /// <param name="value">Valore double da troncare.</param>
    /// <returns>La parte intera di value come double.</returns>
    /// <example>double result = Numerics.Fix(3.9); // restituisce 3.0</example>
    public static double Fix(double value) => Math.Truncate(value);

    /// <summary>
    /// Restituisce il numero di elementi in una lista di sola lettura.
    /// </summary>
    /// <param name="values">Lista di oggetti.</param>
    /// <returns>Il numero di elementi nella lista.</returns>
    /// <example>int result = Numerics.Count(new List&lt;object&gt; {1, 2, 3}); // restituisce 3</example>
    public static int Count(IReadOnlyList<object?> values) => values.Count;

    /// <summary>
    /// Calcola la somma di una lista di oggetti convertibili in decimal.
    /// Gli elementi null vengono ignorati.
    /// </summary>
    /// <param name="values">Lista di oggetti convertibili in decimal.</param>
    /// <returns>La somma dei valori, oppure 0 se la lista è vuota.</returns>
    /// <exception cref="InvalidCastException">Se un elemento non è convertibile in decimal.</exception>
    /// <example>decimal result = Numerics.Sum(new List&lt;object&gt; {1.5m, 2.5m}); // restituisce 4.0m</example>
    public static decimal Sum(IReadOnlyList<object?> values)
    {
        if (values.Count == 0) return 0;
        decimal sum = 0;
        foreach (var value in values)
        {
            if (value == null) continue;
            sum += Convert.ToDecimal(value, CultureInfo.InvariantCulture);
        }
        return sum;
    }

    /// <summary>
    /// Calcola la media aritmetica di una lista di oggetti convertibili in decimal.
    /// </summary>
    /// <param name="values">Lista di oggetti convertibili in decimal.</param>
    /// <returns>La media aritmetica, oppure 0 se la lista è vuota.</returns>
    /// <example>decimal result = Numerics.Avg(new List&lt;object&gt; {1.0m, 2.0m, 3.0m}); // restituisce 2.0m</example>
    public static decimal Avg(IReadOnlyList<object?> values)
    {
        if (values.Count == 0) return 0;
        return Sum(values) / values.Count;
    }

    /// <summary>
    /// Restituisce il valore minimo da una lista di oggetti IComparable.
    /// </summary>
    /// <param name="values">Lista di oggetti che implementano IComparable.</param>
    /// <returns>Il valore minimo presente nella lista.</returns>
    /// <exception cref="ArgumentException">Se la lista è vuota o se non tutti gli elementi sono comparabili.</exception>
    /// <example>object result = Numerics.MinInList(new List&lt;object&gt; {3, 1, 2}); // restituisce 1</example>
    public static object MinInList(IReadOnlyList<object?> values)
    {
        if (values.Count == 0) throw new ArgumentException("La lista non può essere vuota.");
        var comparables = values.OfType<IComparable>().ToList();
        if (comparables.Count != values.Count)
            throw new ArgumentException("Tutti gli elementi devono essere comparabili.");
        var result = comparables[0];
        for (var i = 1; i < comparables.Count; i++)
            if (comparables[i].CompareTo(result) < 0)
                result = comparables[i];
        return result;
    }

    /// <summary>
    /// Restituisce il valore massimo da una lista di oggetti IComparable.
    /// </summary>
    /// <param name="values">Lista di oggetti che implementano IComparable.</param>
    /// <returns>Il valore massimo presente nella lista.</returns>
    /// <exception cref="ArgumentException">Se la lista è vuota o se non tutti gli elementi sono comparabili.</exception>
    /// <example>object result = Numerics.MaxInList(new List&lt;object&gt; {3, 1, 2}); // restituisce 3</example>
    public static object MaxInList(IReadOnlyList<object?> values)
    {
        if (values.Count == 0) throw new ArgumentException("La lista non può essere vuota.");
        var comparables = values.OfType<IComparable>().ToList();
        if (comparables.Count != values.Count)
            throw new ArgumentException("Tutti gli elementi devono essere comparabili.");
        var result = comparables[0];
        for (var i = 1; i < comparables.Count; i++)
            if (comparables[i].CompareTo(result) > 0)
                result = comparables[i];
        return result;
    }

    /// <summary>
    /// Vincola un oggetto IComparable tra un valore minimo e massimo.
    /// </summary>
    /// <param name="valueObj">Valore da vincolare.</param>
    /// <param name="minObj">Limite inferiore.</param>
    /// <param name="maxObj">Limite superiore.</param>
    /// <returns>Il valore vincolato all'intervallo [min, max].</returns>
    /// <exception cref="ArgumentException">Se uno degli argomenti non è IComparable, o se min è maggiore di max.</exception>
    /// <example>object result = Numerics.Clamp(15, 0, 10); // restituisce 10</example>
    public static object Clamp(object? valueObj, object? minObj, object? maxObj)
    {
        if (valueObj is not IComparable value || minObj is not IComparable min || maxObj is not IComparable max)
            throw new ArgumentException("Tutti gli argomenti devono essere comparabili.");
        if (min.CompareTo(max) > 0)
            throw new ArgumentException("Il minimo non può essere maggiore del massimo.");
        if (value.CompareTo(min) < 0) return min;
        if (value.CompareTo(max) > 0) return max;
        return value;
    }

    /// <summary>
    /// Calcola il fattoriale esatto di un intero come ulong.
    /// Supporta valori fino a 20 (limite di overflow per ulong).
    /// </summary>
    /// <param name="n">Numero intero non negativo. Massimo 20.</param>
    /// <returns>Il fattoriale n! come ulong.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Se n è negativo.</exception>
    /// <exception cref="OverflowException">Se n è maggiore di 20.</exception>
    /// <example>ulong result = Numerics.Factorial(5); // restituisce 120</example>
    public static ulong Factorial(int n)
    {
        if (n < 0) throw new ArgumentOutOfRangeException(nameof(n), "L'argomento non può essere negativo.");
        if (n > 20) throw new OverflowException("Overflow: l'argomento massimo è 20.");
        if (n == 0) return 1UL;
        var result = 1UL;
        for (var i = 2; i <= n; i++)
            result *= (ulong)i;
        return result;
    }

    /// <summary>
    /// Calcola il numero di permutazioni di n elementi presi k alla volta (nPk).
    /// </summary>
    /// <param name="n">Numero totale di elementi. Deve essere non negativo.</param>
    /// <param name="k">Numero di elementi da selezionare. Deve essere compreso tra 0 e n.</param>
    /// <returns>Il numero di permutazioni nPk come double.</returns>
    /// <exception cref="ArgumentException">Se n o k sono negativi, o se k è maggiore di n.</exception>
    /// <example>double result = Numerics.Perm(5, 2); // restituisce 20.0</example>
    public static double Perm(int n, int k)
    {
        if (n < 0 || k < 0 || k > n)
            throw new ArgumentException("Argomenti non validi: n, k devono essere non negativi e k <= n.");
        if (n > 170) return double.PositiveInfinity;
        if (k == 0) return 1.0;
        var result = 1.0;
        for (var i = 0; i < k; i++) result *= (n - i);
        return result;
    }

    /// <summary>
    /// Calcola il numero di combinazioni di n elementi presi k alla volta (nCk).
    /// Utilizza l'ottimizzazione k ≤ n/2 per minimizzare le iterazioni.
    /// </summary>
    /// <param name="n">Numero totale di elementi. Deve essere non negativo.</param>
    /// <param name="k">Numero di elementi da selezionare. Deve essere compreso tra 0 e n.</param>
    /// <returns>Il numero di combinazioni nCk come double.</returns>
    /// <exception cref="ArgumentException">Se n o k sono negativi, o se k è maggiore di n.</exception>
    /// <example>double result = Numerics.Comb(5, 2); // restituisce 10.0</example>
    public static double Comb(int n, int k)
    {
        if (n < 0 || k < 0 || k > n)
            throw new ArgumentException("Argomenti non validi: n, k devono essere non negativi e k <= n.");
        if (k == 0 || k == n) return 1.0;
        if (k > n / 2) k = n - k;
        var result = 1.0;
        for (var i = 1; i <= k; i++) result = result * (n - i + 1) / i;
        return result;
    }

    /// <summary>
    /// Calcola l'ipotenusa (norma euclidea) di un triangolo rettangolo: sqrt(x² + y²).
    /// </summary>
    /// <param name="x">Primo cateto.</param>
    /// <param name="y">Secondo cateto.</param>
    /// <returns>L'ipotenusa sqrt(x² + y²).</returns>
    /// <example>double result = Numerics.Hypot(3.0, 4.0); // restituisce 5.0</example>
    public static double Hypot(double x, double y) => Math.Sqrt(x * x + y * y);

    /// <summary>
    /// Calcola la distanza euclidea tra due punti nello spazio bidimensionale.
    /// </summary>
    /// <param name="x1">Coordinata X del primo punto.</param>
    /// <param name="y1">Coordinata Y del primo punto.</param>
    /// <param name="x2">Coordinata X del secondo punto.</param>
    /// <param name="y2">Coordinata Y del secondo punto.</param>
    /// <returns>La distanza euclidea tra i due punti.</returns>
    /// <example>double result = Numerics.Dist2d(0, 0, 3, 4); // restituisce 5.0</example>
    public static double Dist2d(double x1, double y1, double x2, double y2)
    {
        var dx = x2 - x1;
        var dy = y2 - y1;
        return Math.Sqrt(dx * dx + dy * dy);
    }

    /// <summary>
    /// Calcola la distanza euclidea tra due punti nello spazio tridimensionale.
    /// </summary>
    /// <param name="x1">Coordinata X del primo punto.</param>
    /// <param name="y1">Coordinata Y del primo punto.</param>
    /// <param name="z1">Coordinata Z del primo punto.</param>
    /// <param name="x2">Coordinata X del secondo punto.</param>
    /// <param name="y2">Coordinata Y del secondo punto.</param>
    /// <param name="z2">Coordinata Z del secondo punto.</param>
    /// <returns>La distanza euclidea tra i due punti.</returns>
    /// <example>double result = Numerics.Dist3d(0, 0, 0, 1, 1, 1); // restituisce circa 1.732</example>
    public static double Dist3d(double x1, double y1, double z1, double x2, double y2, double z2)
    {
        var dx = x2 - x1;
        var dy = y2 - y1;
        var dz = z2 - z1;
        return Math.Sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// <summary>
    /// Calcola la coordinata X (orizzontale) dalle coordinate polari (raggio, angolo).
    /// </summary>
    /// <param name="radius">Raggio (distanza dall'origine).</param>
    /// <param name="angleDegrees">Angolo in gradi.</param>
    /// <returns>La coordinata X = raggio * cos(angolo).</returns>
    /// <example>double result = Numerics.PolarX(5.0, 0.0); // restituisce 5.0</example>
    public static double PolarX(double radius, double angleDegrees)
    {
        var angleRad = angleDegrees * (Math.PI / 180.0);
        return radius * Math.Cos(angleRad);
    }

    /// <summary>
    /// Calcola la coordinata Y (verticale) dalle coordinate polari (raggio, angolo).
    /// </summary>
    /// <param name="radius">Raggio (distanza dall'origine).</param>
    /// <param name="angleDegrees">Angolo in gradi.</param>
    /// <returns>La coordinata Y = raggio * sin(angolo).</returns>
    /// <example>double result = Numerics.PolarY(5.0, 90.0); // restituisce 5.0</example>
    public static double PolarY(double radius, double angleDegrees)
    {
        var angleRad = angleDegrees * (Math.PI / 180.0);
        return radius * Math.Sin(angleRad);
    }

    /// <summary>
    /// Calcola l'angolo in gradi tra due punti nel piano bidimensionale, utilizzando Atan2.
    /// </summary>
    /// <param name="x1">Coordinata X del primo punto.</param>
    /// <param name="y1">Coordinata Y del primo punto.</param>
    /// <param name="x2">Coordinata X del secondo punto.</param>
    /// <param name="y2">Coordinata Y del secondo punto.</param>
    /// <returns>L'angolo in gradi tra i due punti, misurato dall'asse X positivo.</returns>
    /// <example>double result = Numerics.Angle2d(0, 0, 1, 1); // restituisce 45.0</example>
    public static double Angle2d(double x1, double y1, double x2, double y2)
    {
        var dx = x2 - x1;
        var dy = y2 - y1;
        var angleRad = Math.Atan2(dy, dx);
        return angleRad * (180.0 / Math.PI);
    }

    /// <summary>
    /// Calcola l'inverso moltiplicativo modulare di 'a' modulo 'n' per interi long.
    /// Richiede che a e n siano coprimi (MCD = 1).
    /// </summary>
    /// <param name="a">Il numero di cui calcolare l'inverso modulare.</param>
    /// <param name="n">Il modulo.</param>
    /// <returns>L'inverso moltiplicativo di a in Z/nZ.</returns>
    /// <exception cref="ArithmeticException">Se a non è invertibile modulo n (MCD ≠ 1).</exception>
    /// <example>long result = Numerics.ComputeModMulInverse(3, 7); // restituisce 5 (3*5=15≡1 mod7)</example>
    public static long ComputeModMulInverse(long a, long n)
    {
        var eea = ComputeGcd(a, n);
        if (eea.Gcd != 1)
            throw new ArithmeticException($"{a} is not invertible in Z/{n}Z.");
        var inverse = eea.BezoutA;
        if (inverse < 0) inverse += n;
        return inverse;
    }

    /// <summary>
    /// Calcola l'inverso moltiplicativo modulare di 'a' modulo 'n' per BigInteger.
    /// Richiede che a e n siano coprimi (MCD = 1).
    /// </summary>
    /// <param name="a">Il numero di cui calcolare l'inverso modulare.</param>
    /// <param name="n">Il modulo.</param>
    /// <returns>L'inverso moltiplicativo di a in Z/nZ.</returns>
    /// <exception cref="ArithmeticException">Se a non è invertibile modulo n (MCD ≠ 1).</exception>
    /// <example>BigInteger result = Numerics.ComputeModMulInverse(BigInteger.Parse("3"), BigInteger.Parse("7")); // restituisce 5</example>
    public static BigInteger ComputeModMulInverse(BigInteger a, BigInteger n)
    {
        var eea = ComputeGcd(a, n);
        if (eea.Gcd != 1)
            throw new ArithmeticException($"{a} is not invertible in Z/{n}Z.");
        var inverse = eea.BezoutA;
        if (inverse < 0) inverse += n;
        return inverse;
    }

    /// <summary>
    /// Calcola il massimo comune divisore (MCD) esteso di due interi long (algoritmo di Euclide esteso).
    /// Restituisce i coefficienti di Bézout e il MCD.
    /// </summary>
    /// <param name="a">Primo intero.</param>
    /// <param name="b">Secondo intero.</param>
    /// <returns>Una tupla (BezoutA, BezoutB, Gcd) dove BezoutA * a + BezoutB * b = Gcd.</returns>
    private static (long BezoutA, long BezoutB, long Gcd) ComputeGcd(long a, long b)
    {
        var s = 0L;
        var bezoutOfA = 1L;
        var r = b;
        var gcd = a;
        var bezoutOfB = 0L;
        while (r != 0)
        {
            var quotient = gcd / r;
            var tmp = gcd;
            gcd = r;
            r = tmp - quotient * r;
            tmp = bezoutOfA;
            bezoutOfA = s;
            s = tmp - quotient * s;
        }
        if (b != 0)
            bezoutOfB = (gcd - bezoutOfA * a) / b;
        return (bezoutOfA, bezoutOfB, gcd);
    }

    /// <summary>
    /// Calcola il massimo comune divisore (MCD) esteso di due BigInteger (algoritmo di Euclide esteso).
    /// Restituisce i coefficienti di Bézout e il MCD.
    /// </summary>
    /// <param name="a">Primo BigInteger.</param>
    /// <param name="b">Secondo BigInteger.</param>
    /// <returns>Una tupla (BezoutA, BezoutB, Gcd) dove BezoutA * a + BezoutB * b = Gcd.</returns>
    private static (BigInteger BezoutA, BigInteger BezoutB, BigInteger Gcd) ComputeGcd(BigInteger a, BigInteger b)
    {
        var s = BigInteger.Zero;
        var bezoutOfA = BigInteger.One;
        var r = b;
        var gcd = a;
        var bezoutOfB = BigInteger.Zero;
        while (r != 0)
        {
            var quotient = gcd / r;
            var tmp = gcd;
            gcd = r;
            r = tmp - quotient * r;
            tmp = bezoutOfA;
            bezoutOfA = s;
            s = tmp - quotient * s;
        }
        if (b != 0)
            bezoutOfB = (gcd - bezoutOfA * a) / b;
        return (bezoutOfA, bezoutOfB, gcd);
    }
}
