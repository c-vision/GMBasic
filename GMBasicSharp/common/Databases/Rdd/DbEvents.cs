// FILE: Basic.Databases.Rdd.DbEvents.cs
// Database Events/Triggers — stile XSharp.
// Eventi: Before/After per Navigate, Append, Delete, Update, Open, Close.
// Registrabili da BASIC via RDD.ONEVENT().

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Tipo di evento database.</summary>
public enum DbEventType
{
    BeforeOpen, AfterOpen,
    BeforeClose, AfterClose,
    BeforeNavigate, AfterNavigate,   // GoTop, GoBottom, GoTo, Skip, Seek
    BeforeAppend, AfterAppend,
    BeforeDelete, AfterDelete,
    BeforeUpdate, AfterUpdate,        // PutCurrentRecord
    BeforePack, AfterPack,
    BeforeZap, AfterZap
}

/// <summary>Argomenti evento database.</summary>
public sealed class DbEventArgs : EventArgs
{
    public DbEventType Type { get; init; }
    public int Area { get; init; }
    public string? Alias { get; init; }
    public long RecNo { get; init; }
    public Dictionary<int, object?>? RecordData { get; init; }
    public bool Cancel { get; set; }  // per eventi Before*

    public DbEventArgs(DbEventType type, int area, string? alias = null, long recNo = 0,
        Dictionary<int, object?>? record = null)
    {
        Type = type; Area = area; Alias = alias; RecNo = recNo; RecordData = record;
    }
}

/// <summary>Delegato per handler eventi database.</summary>
public delegate void DbEventHandler(DbEventArgs e);

/// <summary>Gestore eventi per DataSession.</summary>
public sealed class DbEventManager
{
    private readonly Dictionary<DbEventType, List<DbEventHandler>> _handlers = new();
    private readonly object _mutex = new();

    /// <summary>Registra un handler per un tipo di evento.</summary>
    public void On(DbEventType eventType, DbEventHandler handler)
    {
        lock (_mutex)
        {
            if (!_handlers.TryGetValue(eventType, out var list))
                _handlers[eventType] = list = new List<DbEventHandler>();
            list.Add(handler);
        }
    }

    /// <summary>Rimuove tutti gli handler per un tipo di evento.</summary>
    public void Off(DbEventType eventType)
    {
        lock (_mutex) { _handlers.Remove(eventType); }
    }

    /// <summary>Rimuove tutti gli handler.</summary>
    public void Clear()
    {
        lock (_mutex) { _handlers.Clear(); }
    }

    /// <summary>Scatena un evento. Restituisce false se cancellato da un handler Before*.</summary>
    public bool Fire(DbEventType eventType, DbEventArgs args)
    {
        List<DbEventHandler>? list;
        lock (_mutex)
        {
            if (!_handlers.TryGetValue(eventType, out list) || list.Count == 0)
                return true;
        }

        foreach (var handler in list)
        {
            try { handler(args); }
            catch { /* handler non deve interrompere altri handler */ }
            if (args.Cancel) return false;
        }
        return true;
    }

    /// <summary>Scatena un evento in modo fire-and-forget (per After*).</summary>
    public void FireAfter(DbEventType eventType, DbEventArgs args)
    {
        Fire(eventType, args); // After* ignora Cancel
    }

    /// <summary>Verifica se ci sono handler registrati per un tipo di evento.</summary>
    public bool HasHandlers(DbEventType eventType)
    {
        lock (_mutex) { return _handlers.TryGetValue(eventType, out var list) && list.Count > 0; }
    }
}
