using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Globalization;

namespace ClipperLib.Clipper;

/// <summary>
/// Funzioni numeriche di Clipper 5.3: ABS, INT, ROUND, SQRT, EXP, LOG, LOG10,
/// MAX, MIN, MOD.
/// </summary>
public static class ClipperNumbers
{
    /// <summary>ABS(n) — valore assoluto.</summary>
    public static double Abs(double n) => Math.Abs(n);
    public static int Abs(int n) => Math.Abs(n);
    public static decimal Abs(decimal n) => Math.Abs(n);

    /// <summary>INT(n) — parte intera (tronca verso zero).</summary>
    public static int Int(double n) => n >= 0 ? (int)n : (int)Math.Ceiling(n);
    public static long Int(decimal n) => (long)decimal.Truncate(n);
    public static int Int(int n) => n;

    /// <summary>ROUND(n, nDecimals) — arrotondamento.</summary>
    public static double Round(double n, int decimals) =>
        Math.Round(n, Math.Clamp(decimals, 0, 15), MidpointRounding.AwayFromZero);

    /// <summary>ROUND(n) — arrotondamento a intero.</summary>
    public static double Round(double n) => Round(n, 0);

    /// <summary>SQRT(n) — radice quadrata.</summary>
    public static double Sqrt(double n) => Math.Sqrt(n);

    /// <summary>EXP(n) — esponenziale (e^n).</summary>
    public static double Exp(double n) => Math.Exp(n);

    /// <summary>LOG(n) — logaritmo naturale.</summary>
    public static double Log(double n) => Math.Log(n);

    /// <summary>LOG10(n) — logaritmo base 10.</summary>
    public static double Log10(double n) => Math.Log10(n);

    /// <summary>MAX(n1, n2, …) — il maggiore.</summary>
    public static double Max(params double[] values) => values.Length == 0 ? 0 : values.Max();
    public static int Max(params int[] values) => values.Length == 0 ? 0 : values.Max();
    public static long Max(params long[] values) => values.Length == 0 ? 0 : values.Max();
    public static decimal Max(params decimal[] values) => values.Length == 0 ? 0 : values.Max();

    /// <summary>MAX(d1, d2) — la data maggiore.</summary>
    public static DateTime Max(DateTime a, DateTime b) => a > b ? a : b;

    /// <summary>MIN(n1, n2, …) — il minore.</summary>
    public static double Min(params double[] values) => values.Length == 0 ? 0 : values.Min();
    public static int Min(params int[] values) => values.Length == 0 ? 0 : values.Min();
    public static long Min(params long[] values) => values.Length == 0 ? 0 : values.Min();
    public static decimal Min(params decimal[] values) => values.Length == 0 ? 0 : values.Min();

    /// <summary>MIN(d1, d2) — la data minore.</summary>
    public static DateTime Min(DateTime a, DateTime b) => a < b ? a : b;

    /// <summary>
    /// MOD(nDividend, nDivisor) — resto con segno del divisore (semantica Clipper %).
    /// </summary>
    public static double Mod(double dividend, double divisor)
    {
        if (divisor == 0) return 0;
        var r = dividend % divisor;
        return r != 0 && (r < 0) != (divisor < 0) ? r + divisor : r;
    }

    /// <summary>MOD(nDividend, nDivisor) — resto intero.</summary>
    public static int Mod(int dividend, int divisor)
    {
        if (divisor == 0) return 0;
        var r = dividend % divisor;
        return r != 0 && (r < 0) != (divisor < 0) ? r + divisor : r;
    }

    /// <summary>Sign(n) — -1, 0 o 1 (utilità .NET, non Clipper).</summary>
    public static int Sign(double n) => Math.Sign(n);
}
