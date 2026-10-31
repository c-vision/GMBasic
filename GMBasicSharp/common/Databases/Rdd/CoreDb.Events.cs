// FILE: Basic.Databases.Rdd.CoreDb.Events.cs
// CoreDb event hooks: ogni operazione scatena Before/After eventi.

using System;

namespace Basic.Databases.Rdd;

public static partial class CoreDb
{
    /// <summary>Scatena un evento Before/After per un'operazione di navigazione.</summary>
    private static bool FireBeforeNavigate(DataSession s, int area)
    {
        var wa = s.GetWorkArea(area); var d = wa?.Driver;
        return s.Events.Fire(DbEventType.BeforeNavigate,
            new DbEventArgs(DbEventType.BeforeNavigate, area, d?.Alias, d?.RecNo ?? 0));
    }

    private static void FireAfterNavigate(DataSession s, int area)
    {
        var wa = s.GetWorkArea(area); var d = wa?.Driver;
        s.Events.FireAfter(DbEventType.AfterNavigate,
            new DbEventArgs(DbEventType.AfterNavigate, area, d?.Alias, d?.RecNo ?? 0));
    }

    // ═══ Navigazione con eventi ═══

    public static void GoTopEv(DataSession s, int area)
    {
        if (FireBeforeNavigate(s, area)) { Execute(s, area, d => d.GoTop()); FireAfterNavigate(s, area); }
    }

    public static void GoBottomEv(DataSession s, int area)
    {
        if (FireBeforeNavigate(s, area)) { Execute(s, area, d => d.GoBottom()); FireAfterNavigate(s, area); }
    }

    public static void SkipEv(DataSession s, int area, int n)
    {
        if (FireBeforeNavigate(s, area)) { Execute(s, area, d => d.Skip(n)); FireAfterNavigate(s, area); }
    }

    public static bool SeekEv(DataSession s, int area, DbSeekInfo info)
    {
        if (!FireBeforeNavigate(s, area)) return false;
        var result = Execute(s, area, d => d.Seek(info));
        FireAfterNavigate(s, area);
        return result;
    }

    // ═══ CRUD con eventi ═══

    public static long AppendEv(DataSession s, int area, System.Collections.Generic.Dictionary<int, object?> fields)
    {
        var wa = s.GetWorkArea(area);
        if (!s.Events.Fire(DbEventType.BeforeAppend,
            new DbEventArgs(DbEventType.BeforeAppend, area, wa?.Driver?.Alias, 0, fields))) return 0;
        var recNo = Execute(s, area, d => d.Append(fields));
        s.Events.FireAfter(DbEventType.AfterAppend,
            new DbEventArgs(DbEventType.AfterAppend, area, wa?.Driver?.Alias, recNo));
        return recNo;
    }

    public static void DeleteEv(DataSession s, int area)
    {
        var wa = s.GetWorkArea(area);
        var record = wa?.Driver?.GetCurrentRecord();
        if (!s.Events.Fire(DbEventType.BeforeDelete,
            new DbEventArgs(DbEventType.BeforeDelete, area, wa?.Driver?.Alias,
                wa?.Driver?.RecNo ?? 0, record))) return;
        Execute(s, area, d => d.Delete());
        s.Events.FireAfter(DbEventType.AfterDelete,
            new DbEventArgs(DbEventType.AfterDelete, area, wa?.Driver?.Alias));
    }

    public static void UpdateEv(DataSession s, int area, System.Collections.Generic.Dictionary<int, object?> fields)
    {
        var wa = s.GetWorkArea(area);
        if (!s.Events.Fire(DbEventType.BeforeUpdate,
            new DbEventArgs(DbEventType.BeforeUpdate, area, wa?.Driver?.Alias,
                wa?.Driver?.RecNo ?? 0, fields))) return;
        Execute(s, area, d => d.PutCurrentRecord(fields));
        s.Events.FireAfter(DbEventType.AfterUpdate,
            new DbEventArgs(DbEventType.AfterUpdate, area, wa?.Driver?.Alias, wa?.Driver?.RecNo ?? 0));
    }

    public static void PackEv(DataSession s, int area)
    {
        var wa = s.GetWorkArea(area);
        if (s.Events.Fire(DbEventType.BeforePack,
            new DbEventArgs(DbEventType.BeforePack, area, wa?.Driver?.Alias)))
        {
            Execute(s, area, d => d.Pack());
            s.Events.FireAfter(DbEventType.AfterPack,
                new DbEventArgs(DbEventType.AfterPack, area, wa?.Driver?.Alias));
        }
    }

    public static void ZapEv(DataSession s, int area)
    {
        var wa = s.GetWorkArea(area);
        if (s.Events.Fire(DbEventType.BeforeZap,
            new DbEventArgs(DbEventType.BeforeZap, area, wa?.Driver?.Alias)))
        {
            Execute(s, area, d => d.Zap());
            s.Events.FireAfter(DbEventType.AfterZap,
                new DbEventArgs(DbEventType.AfterZap, area, wa?.Driver?.Alias));
        }
    }
}
