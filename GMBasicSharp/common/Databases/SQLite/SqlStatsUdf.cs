// FILE: Basic.SQLite.SqlStatsUdf
using System.Collections.Immutable;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite;

/// <summary>Aggregati statistici SQLite (gap vs Polars/DuckDB/PostgreSQL) — parità C gmb_sql.c
/// §0bis. Semantica bloccata: NULL/non-numerici SALTATI; insieme vuoto → NULL; VAR_SAMP/
/// STDDEV_SAMP/VARIANCE/STDDEV richiedono n≥2, altrimenti NULL. Var/Std via Welford (stesso
/// ordine d'iterazione del C → stessi bit); MEDIAN ordina i valori in final. Registrati su ogni
/// connessione SQLite aperta da SqlDriver (il mondo C li registra in gmb_sql_register_udfs).</summary>
public static class SqlStatsUdf
{
    public static void Register(SqliteConnection connection)
    {
        ArgumentNullException.ThrowIfNull(connection);
        connection.CreateAggregate<object?, StatsAcc, object?>("VAR_POP", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 1 ? null : acc.VarPop, isDeterministic: true);
        connection.CreateAggregate<object?, StatsAcc, object?>("VAR_SAMP", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 2 ? null : acc.VarSamp, isDeterministic: true);
        connection.CreateAggregate<object?, StatsAcc, object?>("VARIANCE", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 2 ? null : acc.VarSamp, isDeterministic: true);
        connection.CreateAggregate<object?, StatsAcc, object?>("STDDEV_POP", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 1 ? null : Math.Sqrt(acc.VarPop), isDeterministic: true);
        connection.CreateAggregate<object?, StatsAcc, object?>("STDDEV_SAMP", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 2 ? null : Math.Sqrt(acc.VarSamp), isDeterministic: true);
        connection.CreateAggregate<object?, StatsAcc, object?>("STDDEV", StatsAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Count < 2 ? null : Math.Sqrt(acc.VarSamp), isDeterministic: true);
        connection.CreateAggregate<object?, MedianAcc, object?>("MEDIAN", MedianAcc.Empty,
            (acc, x) => Add(acc, x), acc => acc.Median(), isDeterministic: true);
        connection.CreateAggregate<object?, ModeAcc, object?>("MODE", ModeAcc.Empty,
            (acc, x) => AddMode(acc, x), acc => acc.Mode(), isDeterministic: true);
        connection.CreateAggregate<object?, object?, QuantileAcc, object?>("QUANTILE", QuantileAcc.Empty,
            (acc, x, p) => AddQuantile(acc, x, p), acc => acc.Quantile(), isDeterministic: true);
        connection.CreateAggregate<object?, object?, CorrAcc, object?>("COVAR_POP", CorrAcc.Empty,
            (acc, y, x) => AddYX(acc, y, x), acc => acc.CovarPop, isDeterministic: true);
        connection.CreateAggregate<object?, object?, CorrAcc, object?>("COVAR_SAMP", CorrAcc.Empty,
            (acc, y, x) => AddYX(acc, y, x), acc => acc.CovarSamp, isDeterministic: true);
        connection.CreateAggregate<object?, object?, CorrAcc, object?>("CORR", CorrAcc.Empty,
            (acc, y, x) => AddYX(acc, y, x), acc => acc.Corr, isDeterministic: true);
    }

    private static StatsAcc Add(StatsAcc acc, object? x) =>
        x is long or double ? acc.Add(Convert.ToDouble(x, System.Globalization.CultureInfo.InvariantCulture)) : acc;

    private static MedianAcc Add(MedianAcc acc, object? x) =>
        x is long or double ? acc.Add(Convert.ToDouble(x, System.Globalization.CultureInfo.InvariantCulture)) : acc;

    private static ModeAcc AddMode(ModeAcc acc, object? x) =>
        x is long or double ? acc.Add(Convert.ToDouble(x, System.Globalization.CultureInfo.InvariantCulture)) : acc;

    private static QuantileAcc AddQuantile(QuantileAcc acc, object? x, object? p)
    {
        var pv = p is long or double ? Convert.ToDouble(p, System.Globalization.CultureInfo.InvariantCulture) : (double?)null;
        return x is long or double
            ? acc.Add(Convert.ToDouble(x, System.Globalization.CultureInfo.InvariantCulture), pv)
            : acc.SetP(pv);
    }

    private static CorrAcc AddYX(CorrAcc acc, object? y, object? x) =>
        y is long or double && x is long or double
            ? acc.Add(Convert.ToDouble(y, System.Globalization.CultureInfo.InvariantCulture),
                      Convert.ToDouble(x, System.Globalization.CultureInfo.InvariantCulture))
            : acc;

    // Welford: immutabile (il seed è condiviso tra i gruppi — mai mutare).
    internal readonly record struct StatsAcc(long Count, double Mean, double M2)
    {
        public static readonly StatsAcc Empty = new(0, 0, 0);
        public StatsAcc Add(double x)
        {
            var n = Count + 1;
            var d = x - Mean;
            var mean = Mean + d / n;
            var m2 = M2 + d * (x - mean);
            return new StatsAcc(n, mean, m2);
        }
        public double VarPop => M2 / Count;
        public double VarSamp => M2 / (Count - 1);
    }

    // MEDIAN: accumula i valori in una lista immutabile (seed condiviso, niente mutazione).
    internal sealed record MedianAcc(ImmutableList<double> Vals)
    {
        public static readonly MedianAcc Empty = new(ImmutableList<double>.Empty);
        public MedianAcc Add(double x) => this with { Vals = Vals.Add(x) };
        public object? Median()
        {
            if (Vals.Count == 0) return null;
            var sorted = Vals.OrderBy(v => v).ToArray();
            var n = sorted.Length;
            return n % 2 == 1
                ? sorted[n / 2]
                : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
        }
    }

    // MODE: valore più frequente; a parità il più piccolo (ordine crescente → primo massimo).
    internal sealed record ModeAcc(ImmutableList<double> Vals)
    {
        public static readonly ModeAcc Empty = new(ImmutableList<double>.Empty);
        public ModeAcc Add(double x) => this with { Vals = Vals.Add(x) };
        public object? Mode()
        {
            if (Vals.Count == 0) return null;
            var s = Vals.OrderBy(v => v).ToArray();
            double bestV = 0;
            long bestC = 0;
            var i = 0;
            while (i < s.Length)
            {
                var j = i + 1;
                while (j < s.Length && s[j] == s[i]) j++;
                long c = j - i;
                if (c > bestC) { bestC = c; bestV = s[i]; }
                i = j;
            }
            return bestV;
        }
    }

    // QUANTILE(x, p): p ∈ [0,1] interpolazione lineare (come numpy/pandas). p fuori range/non dato → null.
    internal sealed record QuantileAcc(ImmutableList<double> Vals, double P, bool HasP)
    {
        public static readonly QuantileAcc Empty = new(ImmutableList<double>.Empty, 0, false);
        public QuantileAcc SetP(double? p) => p.HasValue ? this with { P = p.Value, HasP = true } : this;
        public QuantileAcc Add(double x, double? p)
        {
            var a = p.HasValue ? this with { P = p.Value, HasP = true } : this;
            return a with { Vals = a.Vals.Add(x) };
        }
        public object? Quantile()
        {
            if (Vals.Count == 0 || !HasP || P < 0 || P > 1) return null;
            var s = Vals.OrderBy(v => v).ToArray();
            if (s.Length == 1) return s[0];
            var pos = P * (s.Length - 1);
            var lo = (int)Math.Floor(pos);
            var hi = Math.Min(lo + 1, s.Length - 1);
            var frac = pos - lo;
            return s[lo] + (s[hi] - s[lo]) * frac;
        }
    }

    // COVAR/CORR (2 argomenti: y, x). Somme grezze → COVAR_POP/SAMP e Pearson r.
    internal readonly record struct CorrAcc(long N, double Sy, double Sx, double Syy, double Sxx, double Sxy)
    {
        public static readonly CorrAcc Empty = new(0, 0, 0, 0, 0, 0);
        public CorrAcc Add(double y, double x) =>
            new(N + 1, Sy + y, Sx + x, Syy + y * y, Sxx + x * x, Sxy + y * x);
        public object? CovarPop => N < 1 ? null : (Sxy - Sy * Sx / N) / N;
        public object? CovarSamp => N < 2 ? null : (Sxy - Sy * Sx / N) / (N - 1);
        public object? Corr
        {
            get
            {
                if (N < 2) return null;
                var syy = Syy - Sy * Sy / N;
                var sxx = Sxx - Sx * Sx / N;
                var sxy = Sxy - Sy * Sx / N;
                if (syy <= 0 || sxx <= 0) return null;
                return sxy / Math.Sqrt(syy * sxx);
            }
        }
    }
}
