// FILE: StdLib.Numerics.cs
// ReSharper disable CheckNamespace

namespace CommonLib;

/// <summary>
/// Fornisce operazioni numeriche fondamentali per il runtime BASIC.
/// Contiene i seguenti metodi:
/// <list type="bullet">
///   <item><description><c>Add</c> — Addizione tra due valori double.</description></item>
///   <item><description><c>Sub</c> — Sottrazione tra due valori double.</description></item>
///   <item><description><c>Mul</c> — Moltiplicazione tra due valori double.</description></item>
///   <item><description><c>Div</c> — Divisione tra due valori double con protezione da divisione per zero.</description></item>
///   <item><description><c>Mod</c> — Modulo tra due valori double con protezione da divisione per zero.</description></item>
///   <item><description><c>Pow</c> — Potenza con base ed esponente double.</description></item>
///   <item><description><c>Neg</c> — Negazione di un valore double.</description></item>
///   <item><description><c>Compare</c> — Confronto tra due valori double.</description></item>
///   <item><description><c>IsZero</c> — Verifica se un valore double è circa zero entro una tolleranza.</description></item>
///   <item><description><c>Clamp</c> — Vincola un valore tra minimo e massimo.</description></item>
///   <item><description><c>Floor</c> — Arrotondamento per difetto con risultato long.</description></item>
///   <item><description><c>Ceiling</c> — Arrotondamento per eccesso con risultato long.</description></item>
///   <item><description><c>Round</c> — Arrotondamento aritmetico con risultato long.</description></item>
///   <item><description><c>Truncate</c> — Troncamento della parte decimale con risultato long.</description></item>
///   <item><description><c>Abs</c> — Valore assoluto.</description></item>
///   <item><description><c>Sqrt</c> — Radice quadrata.</description></item>
///   <item><description><c>Sin</c> — Seno (radianti).</description></item>
///   <item><description><c>Cos</c> — Coseno (radianti).</description></item>
///   <item><description><c>Tan</c> — Tangente (radianti).</description></item>
///   <item><description><c>Atan2</c> — Arcotangente con segno dei quadranti.</description></item>
///   <item><description><c>Log</c> — Logaritmo naturale.</description></item>
///   <item><description><c>Log10</c> — Logaritmo in base 10.</description></item>
///   <item><description><c>Exp</c> — Esponenziale in base e.</description></item>
///   <item><description><c>Min</c> — Minimo tra due valori.</description></item>
///   <item><description><c>Max</c> — Massimo tra due valori.</description></item>
///   <item><description><c>DegreesToRadians</c> — Conversione da gradi a radianti.</description></item>
///   <item><description><c>RadiansToDegrees</c> — Conversione da radianti a gradi.</description></item>
///   <item><description><c>GreatestCommonDivisor</c> — Massimo comune divisore (MCD).</description></item>
///   <item><description><c>LeastCommonMultiple</c> — Minimo comune multiplo (mcm).</description></item>
///   <item><description><c>Average</c> — Media aritmetica di un array.</description></item>
///   <item><description><c>Sum</c> — Somma degli elementi di un array.</description></item>
///   <item><description><c>Factorial</c> — Fattoriale di un numero intero (double).</description></item>
/// </list>
/// </summary>
public static partial class Numerics
{
    /// <summary>
    /// Restituisce la somma di due numeri double.
    /// </summary>
    /// <param name="a">Primo addendo.</param>
    /// <param name="b">Secondo addendo.</param>
    /// <returns>La somma di a e b.</returns>
    /// <example>double result = Numerics.Add(3.5, 2.1); // restituisce 5.6</example>
    public static double Add(double a, double b) => a + b;

    /// <summary>
    /// Restituisce la differenza tra due numeri double.
    /// </summary>
    /// <param name="a">Minuendo.</param>
    /// <param name="b">Sottraendo.</param>
    /// <returns>La differenza a - b.</returns>
    /// <example>double result = Numerics.Sub(10.0, 3.5); // restituisce 6.5</example>
    public static double Sub(double a, double b) => a - b;

    /// <summary>
    /// Restituisce il prodotto di due numeri double.
    /// </summary>
    /// <param name="a">Primo fattore.</param>
    /// <param name="b">Secondo fattore.</param>
    /// <returns>Il prodotto di a e b.</returns>
    /// <example>double result = Numerics.Mul(4.0, 2.5); // restituisce 10.0</example>
    public static double Mul(double a, double b) => a * b;

    /// <summary>
    /// Restituisce il quoziente della divisione tra due numeri double.
    /// Se il divisore è zero, restituisce zero invece di generare un'eccezione.
    /// </summary>
    /// <param name="a">Dividendo.</param>
    /// <param name="b">Divisore.</param>
    /// <returns>Il quoziente a / b, oppure 0 se b è zero.</returns>
    /// <example>double result = Numerics.Div(10.0, 3.0); // restituisce 3.333...</example>
    public static double Div(double a, double b) => b != 0 ? a / b : 0;

    /// <summary>
    /// Restituisce il resto della divisione tra due numeri double (modulo).
    /// Se il divisore è zero, restituisce zero invece di generare un'eccezione.
    /// </summary>
    /// <param name="a">Dividendo.</param>
    /// <param name="b">Divisore.</param>
    /// <returns>Il resto a % b, oppure 0 se b è zero.</returns>
    /// <example>double result = Numerics.Mod(10.0, 3.0); // restituisce 1.0</example>
    public static double Mod(double a, double b) => b != 0 ? a % b : 0;

    /// <summary>
    /// Eleva un numero a una potenza.
    /// </summary>
    /// <param name="a">Base.</param>
    /// <param name="b">Esponente.</param>
    /// <returns>Il valore di a elevato alla potenza b.</returns>
    /// <example>double result = Numerics.Pow(2.0, 3.0); // restituisce 8.0</example>
    public static double Pow(double a, double b) => Math.Pow(a, b);

    /// <summary>
    /// Restituisce l'opposto (negazione) di un valore double.
    /// </summary>
    /// <param name="a">Valore da negare.</param>
    /// <returns>Il valore negato (-a).</returns>
    /// <example>double result = Numerics.Neg(5.0); // restituisce -5.0</example>
    public static double Neg(double a) => -a;

    /// <summary>
    /// Confronta due valori double e restituisce un indicatore del loro ordinamento relativo.
    /// </summary>
    /// <param name="a">Primo valore da confrontare.</param>
    /// <param name="b">Secondo valore da confrontare.</param>
    /// <returns>-1 se a è minore di b, 0 se a è uguale a b, 1 se a è maggiore di b.</returns>
    /// <example>int result = Numerics.Compare(5.0, 3.0); // restituisce 1</example>
    public static int Compare(double a, double b) =>
        a < b ? -1 : a > b ? 1 : 0;

    /// <summary>
    /// Verifica se un valore double è circa zero entro una tolleranza di 1e-15.
    /// </summary>
    /// <param name="v">Valore da verificare.</param>
    /// <returns>true se il valore assoluto è minore di 1e-15; altrimenti false.</returns>
    /// <example>bool result = Numerics.IsZero(1e-16); // restituisce true</example>
    public static bool IsZero(double v) => Math.Abs(v) < 1e-15;

    /// <summary>
    /// Vincola un valore double all'intervallo specificato.
    /// </summary>
    /// <param name="v">Valore da vincolare.</param>
    /// <param name="min">Limite inferiore dell'intervallo.</param>
    /// <param name="max">Limite superiore dell'intervallo.</param>
    /// <returns>min se v è minore di min, max se v è maggiore di max, altrimenti v.</returns>
    /// <example>double result = Numerics.Clamp(15.0, 0.0, 10.0); // restituisce 10.0</example>
    public static double Clamp(double v, double min, double max) =>
        v < min ? min : v > max ? max : v;

    /// <summary>
    /// Arrotonda un valore double per difetto e restituisce il risultato come long.
    /// </summary>
    /// <param name="v">Valore da arrotondare.</param>
    /// <returns>Il più grande intero long minore o uguale a v.</returns>
    /// <example>long result = Numerics.Floor(3.9); // restituisce 3</example>
    public static long Floor(double v) => (long)Math.Floor(v);

    /// <summary>
    /// Arrotonda un valore double per eccesso e restituisce il risultato come long.
    /// </summary>
    /// <param name="v">Valore da arrotondare.</param>
    /// <returns>Il più piccolo intero long maggiore o uguale a v.</returns>
    /// <example>long result = Numerics.Ceiling(3.1); // restituisce 4</example>
    public static long Ceiling(double v) => (long)Math.Ceiling(v);

    /// <summary>
    /// Arrotonda un valore double all'intero più vicino e restituisce il risultato come long.
    /// </summary>
    /// <param name="v">Valore da arrotondare.</param>
    /// <returns>Il valore long arrotondato più vicino a v.</returns>
    /// <example>long result = Numerics.Round(3.5); // restituisce 4</example>
    public static long Round(double v) => (long)Math.Round(v);

    /// <summary>
    /// Tronca la parte decimale di un valore double e restituisce la parte intera come long.
    /// </summary>
    /// <param name="v">Valore da troncare.</param>
    /// <returns>La parte intera di v, scartando le cifre decimali.</returns>
    /// <example>long result = Numerics.Truncate(3.9); // restituisce 3</example>
    public static long Truncate(double v) => (long)Math.Truncate(v);

    /// <summary>
    /// Restituisce il valore assoluto di un numero double.
    /// </summary>
    /// <param name="v">Valore di input.</param>
    /// <returns>Il valore assoluto (non negativo) di v.</returns>
    /// <example>double result = Numerics.Abs(-5.0); // restituisce 5.0</example>
    public static double Abs(double v) => Math.Abs(v);

    /// <summary>
    /// Calcola la radice quadrata di un numero double.
    /// </summary>
    /// <param name="v">Valore di cui calcolare la radice quadrata. Deve essere maggiore o uguale a zero.</param>
    /// <returns>La radice quadrata di v.</returns>
    /// <example>double result = Numerics.Sqrt(9.0); // restituisce 3.0</example>
    public static double Sqrt(double v) => Math.Sqrt(v);

    /// <summary>
    /// Calcola il seno di un angolo espresso in radianti.
    /// </summary>
    /// <param name="v">Angolo in radianti.</param>
    /// <returns>Il seno dell'angolo specificato.</returns>
    /// <example>double result = Numerics.Sin(Math.PI / 2); // restituisce 1.0</example>
    public static double Sin(double v) => Math.Sin(v);

    /// <summary>
    /// Calcola il coseno di un angolo espresso in radianti.
    /// </summary>
    /// <param name="v">Angolo in radianti.</param>
    /// <returns>Il coseno dell'angolo specificato.</returns>
    /// <example>double result = Numerics.Cos(0); // restituisce 1.0</example>
    public static double Cos(double v) => Math.Cos(v);

    /// <summary>
    /// Calcola la tangente di un angolo espresso in radianti.
    /// </summary>
    /// <param name="v">Angolo in radianti.</param>
    /// <returns>La tangente dell'angolo specificato.</returns>
    /// <example>double result = Numerics.Tan(Math.PI / 4); // restituisce circa 1.0</example>
    public static double Tan(double v) => Math.Tan(v);

    /// <summary>
    /// Calcola l'arcotangente del rapporto y/x, restituendo l'angolo in radianti.
    /// </summary>
    /// <param name="y">Coordinata y (verticale).</param>
    /// <param name="x">Coordinata x (orizzontale).</param>
    /// <returns>L'angolo θ in radianti tale che tan(θ) = y/x, nel quadrante corretto.</returns>
    /// <example>double result = Numerics.Atan2(1.0, 1.0); // restituisce circa 0.7854 (π/4)</example>
    public static double Atan2(double y, double x) => Math.Atan2(y, x);

    /// <summary>
    /// Calcola il logaritmo naturale (base e) di un numero.
    /// </summary>
    /// <param name="v">Valore di cui calcolare il logaritmo. Deve essere maggiore di zero.</param>
    /// <returns>Il logaritmo naturale di v.</returns>
    /// <example>double result = Numerics.Log(Math.E); // restituisce 1.0</example>
    public static double Log(double v) => Math.Log(v);

    /// <summary>
    /// Calcola il logaritmo in base 10 di un numero.
    /// </summary>
    /// <param name="v">Valore di cui calcolare il logaritmo. Deve essere maggiore di zero.</param>
    /// <returns>Il logaritmo in base 10 di v.</returns>
    /// <example>double result = Numerics.Log10(100.0); // restituisce 2.0</example>
    public static double Log10(double v) => Math.Log10(v);

    /// <summary>
    /// Calcola l'esponenziale in base e (e^v).
    /// </summary>
    /// <param name="v">Esponente.</param>
    /// <returns>Il valore di e elevato alla potenza v.</returns>
    /// <example>double result = Numerics.Exp(1.0); // restituisce circa 2.71828</example>
    public static double Exp(double v) => Math.Exp(v);

    /// <summary>
    /// Restituisce il valore minimo tra due numeri double.
    /// </summary>
    /// <param name="a">Primo valore.</param>
    /// <param name="b">Secondo valore.</param>
    /// <returns>Il più piccolo tra a e b.</returns>
    /// <example>double result = Numerics.Min(5.0, 3.0); // restituisce 3.0</example>
    public static double Min(double a, double b) => Math.Min(a, b);

    /// <summary>
    /// Restituisce il valore massimo tra due numeri double.
    /// </summary>
    /// <param name="a">Primo valore.</param>
    /// <param name="b">Secondo valore.</param>
    /// <returns>Il più grande tra a e b.</returns>
    /// <example>double result = Numerics.Max(5.0, 3.0); // restituisce 5.0</example>
    public static double Max(double a, double b) => Math.Max(a, b);

    /// <summary>
    /// Converte un angolo da gradi a radianti.
    /// </summary>
    /// <param name="deg">Angolo in gradi.</param>
    /// <returns>L'angolo convertito in radianti.</returns>
    /// <example>double result = Numerics.DegreesToRadians(180.0); // restituisce circa 3.14159 (π)</example>
    public static double DegreesToRadians(double deg) => deg * Math.PI / 180.0;

    /// <summary>
    /// Converte un angolo da radianti a gradi.
    /// </summary>
    /// <param name="rad">Angolo in radianti.</param>
    /// <returns>L'angolo convertito in gradi.</returns>
    /// <example>double result = Numerics.RadiansToDegrees(Math.PI); // restituisce 180.0</example>
    public static double RadiansToDegrees(double rad) => rad * 180.0 / Math.PI;

    /// <summary>
    /// Calcola il massimo comune divisore (MCD) di due interi long.
    /// Utilizza l'algoritmo di Euclide.
    /// </summary>
    /// <param name="a">Primo numero intero.</param>
    /// <param name="b">Secondo numero intero.</param>
    /// <returns>Il massimo comune divisore positivo di a e b.</returns>
    /// <example>long result = Numerics.GreatestCommonDivisor(12, 18); // restituisce 6</example>
    public static long GreatestCommonDivisor(long a, long b)
    {
        a = Math.Abs(a); b = Math.Abs(b);
        while (b != 0) { var t = b; b = a % b; a = t; }
        return a;
    }

    /// <summary>
    /// Calcola il minimo comune multiplo (mcm) di due interi long.
    /// </summary>
    /// <param name="a">Primo numero intero.</param>
    /// <param name="b">Secondo numero intero.</param>
    /// <returns>Il minimo comune multiplo positivo di a e b, oppure 0 se uno dei due è zero.</returns>
    /// <example>long result = Numerics.LeastCommonMultiple(6, 8); // restituisce 24</example>
    public static long LeastCommonMultiple(long a, long b) =>
        a == 0 || b == 0 ? 0 : Math.Abs(a) / GreatestCommonDivisor(a, b) * Math.Abs(b);

    /// <summary>
    /// Calcola la media aritmetica di un array di valori double.
    /// </summary>
    /// <param name="values">Array di valori di cui calcolare la media.</param>
    /// <returns>La media aritmetica, oppure 0 se l'array è vuoto.</returns>
    /// <example>double result = Numerics.Average(new double[] {1.0, 2.0, 3.0}); // restituisce 2.0</example>
    public static double Average(double[] values) =>
        values.Length == 0 ? 0 : values.Sum() / values.Length;

    /// <summary>
    /// Calcola la somma degli elementi di un array di valori double.
    /// </summary>
    /// <param name="values">Array di valori da sommare.</param>
    /// <returns>La somma di tutti gli elementi.</returns>
    /// <example>double result = Numerics.Sum(new double[] {1.0, 2.0, 3.0}); // restituisce 6.0</example>
    public static double Sum(double[] values) => values.Sum();

    /// <summary>
    /// Calcola il fattoriale di un numero intero long come valore double.
    /// Se n è negativo, restituisce 0.
    /// </summary>
    /// <param name="n">Numero intero di cui calcolare il fattoriale.</param>
    /// <returns>Il fattoriale n! come double, oppure 0 se n è negativo.</returns>
    /// <example>double result = Numerics.Factorial(5); // restituisce 120.0</example>
    public static double Factorial(long n)
    {
        if (n < 0) return 0;
        double result = 1;
        for (long i = 2; i <= n; i++) result *= i;
        return result;
    }
}
