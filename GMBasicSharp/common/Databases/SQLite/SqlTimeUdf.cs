// FILE: Basic.SQLite.SqlTimeUdf
using Microsoft.Data.Sqlite;

namespace Basic.SQLite;

/// <summary>UDF temporali stile Polars (parità C gmb_sql.c §0bis): TIMEBUCKET scalare +
/// FIRST_TS/LAST_TS. Semantica bloccata (stessa nei due mondi, vedi guida SQL):
/// TIMEBUCKET(ts, secs) = FLOOR(ts/secs)*secs (INTEGER+INTEGER → INTEGER con floor vera anche
/// su ts negativi, altrimenti REAL; ts non numerico o secs NULL/&lt;=0 → NULL);
/// FIRST_TS(x, ts)/LAST_TS(x, ts) = x della riga con ts minimo/massimo (a pari ts vince la
/// prima riga; righe con ts NULL/non-numerico o x NULL saltate; vuoto → NULL; tipo di x
/// preservato). Registrate su ogni connessione SQLite aperta da SqlDriver.</summary>
public static class SqlTimeUdf
{
    public static void Register(SqliteConnection connection)
    {
        ArgumentNullException.ThrowIfNull(connection);
        connection.CreateFunction("TIMEBUCKET", (object? ts, object? secs) => TimeBucket(ts, secs), isDeterministic: true);
        connection.CreateAggregate<object?, object?, FirstLastAcc, object?>("FIRST_TS",
            new FirstLastAcc(false, 0, null),
            (acc, x, ts) => acc.Step(x, ts, true),
            acc => acc.Val,
            isDeterministic: true);
        connection.CreateAggregate<object?, object?, FirstLastAcc, object?>("LAST_TS",
            new FirstLastAcc(false, 0, null),
            (acc, x, ts) => acc.Step(x, ts, false),
            acc => acc.Val,
            isDeterministic: true);
    }

    internal static object? TimeBucket(object? ts, object? secs)
    {
        if (ts is not (long or double) || secs is not (long or double))
            return null;
        var s = Convert.ToDouble(secs);
        if (!(s > 0))
            return null;
        if (ts is long lts && secs is long ls)
        {
            var r = lts % ls;
            if (r < 0) r += ls;
            return lts - r;
        }
        return Math.Floor(Convert.ToDouble(ts) / s) * s;
    }

    internal sealed record FirstLastAcc(bool Has, double Ts, object? Val)
    {
        public FirstLastAcc Step(object? x, object? tsIn, bool wantFirst)
        {
            if (x is null || x is DBNull || tsIn is not (long or double))
                return this;
            var t = Convert.ToDouble(tsIn);
            if (!Has || (wantFirst ? t < Ts : t > Ts))
                return this with { Has = true, Ts = t, Val = x };
            return this;
        }
    }
}
