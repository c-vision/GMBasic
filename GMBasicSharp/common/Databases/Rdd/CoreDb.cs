// FILE: Basic.Databases.Rdd.CoreDb.cs
// CoreDb: dispatcher centralizzato per operazioni database.
// Ogni operazione DB passa da qui → DataSession → IDatabaseDriver.
// Pattern XSharp: CoreDb.GoTop() risolve l'area corrente e chiama il driver.

using System;

namespace Basic.Databases.Rdd;

/// <summary>Dispatcher centrale per tutte le operazioni database via RDD.</summary>
public static partial class CoreDb
{
    /// <summary>Esegue un'operazione sul driver dell'area corrente.</summary>
    public static T Execute<T>(DataSession session, int area, Func<IDatabaseDriver, T> action)
    {
        var wa = area > 0 ? session.GetWorkArea(area) : null;
        var driver = wa?.Driver ?? session.CurrentDriver;
        if (driver == null)
            throw new InvalidOperationException($"No database open in area {area}.");
        return action(driver);
    }

    /// <summary>Esegue un'azione senza ritorno.</summary>
    public static void Execute(DataSession session, int area, Action<IDatabaseDriver> action)
    {
        var wa = area > 0 ? session.GetWorkArea(area) : null;
        var driver = wa?.Driver ?? session.CurrentDriver;
        if (driver == null)
            throw new InvalidOperationException($"No database open in area {area}.");
        action(driver);
    }

    // ═══ Navigazione ═══
    public static void GoTop(DataSession s, int area)    => Execute(s, area, d => d.GoTop());
    public static void GoBottom(DataSession s, int area) => Execute(s, area, d => d.GoBottom());
    public static void GoTo(DataSession s, int area, long n) => Execute(s, area, d => d.GoTo(n));
    public static void Skip(DataSession s, int area, int n) => Execute(s, area, d => d.Skip(n));

    // ═══ CRUD ═══
    public static long Append(DataSession s, int area, System.Collections.Generic.Dictionary<int, object?> fields)
        => Execute(s, area, d => d.Append(fields));
    public static void Delete(DataSession s, int area)    => Execute(s, area, d => d.Delete());
    public static void Recall(DataSession s, int area)    => Execute(s, area, d => d.Recall());
    public static void Pack(DataSession s, int area)      => Execute(s, area, d => d.Pack());
    public static void Zap(DataSession s, int area)       => Execute(s, area, d => d.Zap());

    // ═══ Indici ═══
    public static bool Seek(DataSession s, int area, DbSeekInfo info)
        => Execute(s, area, d => d.Seek(info));
    public static void SetOrder(DataSession s, int area, string name)
        => Execute(s, area, d => d.SetOrder(name));

    // ═══ Lock ═══
    public static bool Lock(DataSession s, int area, long rec)
        => Execute(s, area, d => d.Lock(rec));
    public static void Unlock(DataSession s, int area, long rec)
        => Execute(s, area, d => d.Unlock(rec));

    // ═══ Info ═══
    public static object? Info(DataSession s, int area, int infoType, object? param)
        => Execute(s, area, d => d.Info(infoType, param));

    // ═══ Filtri ═══
    public static void SetFilter(DataSession s, int area, string? expr)
        => Execute(s, area, d => d.SetFilter(expr));
    public static void ClearFilter(DataSession s, int area)
        => Execute(s, area, d => d.ClearFilter());
}
