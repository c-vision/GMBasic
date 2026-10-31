// FILE: Basic.Databases.Rdd.BaseDriver.cs
// Implementazione base di IDatabaseDriver con stato comune
// (BoF, EoF, Found, RecNo, RecCount, campi, lock).
// I driver concreti ereditano da questa classe.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Classe base astratta per driver database.
/// Fornisce lo stato condiviso (cursore, campi, lock).</summary>
public abstract class BaseDriver : IDatabaseDriver
{
    /// <summary>Costruisce il contesto campo→valore del record CORRENTE di un driver — usato
    /// per valutare espressioni di relazione (SET RELATION) composte tramite
    /// <see cref="Basic.Databases.KeyExpressionPCodeHook.CompileFactory"/> (stesso hook già
    /// usato da IsamDriver/DatabaseService.Commercial per le espressioni chiave SEEK/INDEX ON —
    /// nessun nuovo meccanismo di iniezione: quello esiste già e IlBackend lo imposta una volta
    /// via DbFilterHookInit, [ModuleInitializer]).</summary>
    protected static Dictionary<string, object?> BuildFieldContext(IDatabaseDriver driver)
    {
        var ctx = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        var fields = driver.Fields;
        for (var i = 0; i < fields.Length; i++)
            ctx[fields[i].Name] = driver.GetFieldValue(i);
        return ctx;
    }

    // ═══════════════════════════════════════════════════════════
    // Stato condiviso
    // ═══════════════════════════════════════════════════════════
    protected bool _eof;
    protected bool _bof;
    protected bool _found;
    protected long _recNo;
    protected long _recCount;
    protected RddFieldInfo[] _fields = [];
    protected string _alias = "";
    protected string _fileName = "";
    protected bool _isOpen;
    protected bool _readOnly;
    protected string _filterExpression = "";
    protected string? _activeOrder;
    protected IDatabaseDriver? _parentRelation;
    protected string? _relationKeyExpression;

    // ═══════════════════════════════════════════════════════════
    // IDatabaseDriver — Proprietà
    // ═══════════════════════════════════════════════════════════
    public bool Eof => _eof;
    public bool Bof => _bof;
    public bool Found => _found;
    public long RecNo => _recNo;
    public long RecCount => _recCount;
    public RddFieldInfo[] Fields => _fields;

    public virtual void SetFields(RddFieldInfo[] fields) => _fields = fields;
    public int FieldCount => _fields.Length;
    public string? Alias => _alias;
    public string FileName => _fileName;
    public bool IsOpen => _isOpen;
    public bool IsReadOnly => _readOnly;
    public abstract string DriverName { get; }

    // ═══════════════════════════════════════════════════════════
    // IDatabaseDriver — Astratti (ogni driver implementa)
    // ═══════════════════════════════════════════════════════════
    public abstract void Open(DbOpenInfo info);
    public abstract void Close();

    public abstract void GoTop();
    public abstract void GoBottom();
    public abstract void GoTo(long recordNumber);
    public abstract void Skip(int count);
    public abstract void SkipRaw(int count);

    public abstract long Append(Dictionary<int, object?> fields);
    public abstract void Delete();
    public abstract void Recall();
    public abstract void Pack();
    public abstract void Zap();
    public abstract Dictionary<int, object?>? GetCurrentRecord();
    public abstract void PutCurrentRecord(Dictionary<int, object?> fields);

    public abstract void CreateIndex(DbOrderInfo info);
    public abstract void OpenIndex(string name, string filePath);
    public abstract void CloseIndex(string name);
    public abstract void SetOrder(string name);
    public abstract bool Seek(DbSeekInfo info);
    public abstract string? CurrentKey();
    public abstract int CurrentKeyNo();

    public abstract bool Lock(long recordNumber);
    public abstract void Unlock(long recordNumber);

    // ═══════════════════════════════════════════════════════════
    // IDatabaseDriver — Implementazioni default
    // ═══════════════════════════════════════════════════════════
    public virtual bool FileLock() { return false; }
    public virtual void FileUnlock() { }

    public virtual int GetFieldIndex(string name)
    {
        for (int i = 0; i < _fields.Length; i++)
            if (string.Equals(_fields[i].Name, name, StringComparison.OrdinalIgnoreCase))
                return i;
        // DBF tronca i nomi campo a 10 caratteri su disco: un nome BASIC più lungo (dichiarato
        // nel TYPE, es. "Descrizione") non trova mai il match esatto contro "Descrizion" —
        // stessa regola già applicata da IsamExportImport.LookupFieldValue per lo stesso motivo.
        if (name.Length > 10)
        {
            var truncated = name[..10];
            for (int i = 0; i < _fields.Length; i++)
                if (string.Equals(_fields[i].Name, truncated, StringComparison.OrdinalIgnoreCase))
                    return i;
        }
        return -1;
    }

    public virtual object? GetFieldValue(int index)
    {
        if (index < 0 || index >= _fields.Length) return null;
        var record = GetCurrentRecord();
        if (record != null && record.TryGetValue(_fields[index].SymbolId, out var val))
            return val;
        return null;
    }

    public virtual void SetFieldValue(int index, object? value)
    {
        if (index < 0 || index >= _fields.Length) return;
        var record = GetCurrentRecord();
        if (record != null)
        {
            record[_fields[index].SymbolId] = value;
            PutCurrentRecord(record);
        }
    }

    public virtual void SetFilter(string? expression)
    {
        /* Prima si pulisce e POI si imposta: invertito, ClearFilter() azzerava subito
         * l'espressione appena scritta → DBF SET FILTER TO <expr> non aveva effetto e
         * DBF.DBFILTER() restava vuoto (§ MiniTests/dbf/dbf_lock_filter.bas). */
        ClearFilter();
        _filterExpression = expression ?? "";
    }

    public virtual void ClearFilter()
    {
        _filterExpression = "";
    }

    /// <summary>Espressione del filtro attivo (stringa vuota se nessun filtro).</summary>
    public string FilterExpression => _filterExpression;

    protected bool _deleted = false;
    public virtual void SetDeleted(bool on) => _deleted = on;

    public virtual void SetRelation(IDatabaseDriver parent, string keyExpression)
    {
        _parentRelation = parent;
        _relationKeyExpression = keyExpression;
    }

    public virtual void ClearRelation()
    {
        _parentRelation = null;
        _relationKeyExpression = null;
    }

    /// <summary>Vero se questo driver ha una relazione master-detail attiva (impostata da
    /// SetRelation con parent non nullo).</summary>
    public virtual bool HasActiveRelation => _parentRelation != null;

    /// <summary>Il driver padre impostato dall'ultima SetRelation (null se nessuna relazione
    /// attiva) — vedi IDatabaseDriver.ParentRelationDriver.</summary>
    public IDatabaseDriver? ParentRelationDriver => _parentRelation;

    /// <inheritdoc />
    public string? RelationKeyExpression => _relationKeyExpression;

    /// <summary>Riposiziona questo driver (figlio) sul record corrispondente al valore corrente
    /// del campo chiave nel driver padre, se una relazione e' attiva. Implementazione di base:
    /// nessun posizionamento (nessuno stato di relazione da sincronizzare per driver che non la
    /// supportano). Override in IsamDriver per la vera logica di posizionamento.</summary>
    public virtual bool SyncRelation() => _found;

    public virtual object? Info(int infoType, object? param) => null;

    // ═══ SQL Raw — default NotSupported ═══
    private static NotSupportedException RawNotSupported(string method) =>
        new NotSupportedException($"{method} non supportato da questo driver. Usare un driver SQL (SQLite/SQL Server).");

    public virtual long ExecRaw(string sql, IReadOnlyList<object?>? p, IReadOnlyDictionary<string, object?>? n) => throw RawNotSupported(nameof(ExecRaw));
    public virtual int QueryRaw(string sql, IReadOnlyList<object?>? p, IReadOnlyDictionary<string, object?>? n) => throw RawNotSupported(nameof(QueryRaw));
    public virtual bool Fetch(int rsId) => throw RawNotSupported(nameof(Fetch));
    public virtual IReadOnlyDictionary<string, object?>? GetCurrentRow(int rsId) => throw RawNotSupported(nameof(GetCurrentRow));
    public virtual void CloseResultSet(int rsId) => throw RawNotSupported(nameof(CloseResultSet));
    public virtual void Attach(string path, string? schema) => throw RawNotSupported(nameof(Attach));
    public virtual void Detach(string? schema) => throw RawNotSupported(nameof(Detach));
    public virtual void BeginTransaction() => throw RawNotSupported(nameof(BeginTransaction));
    public virtual void Commit() => throw RawNotSupported(nameof(Commit));
    public virtual void Rollback() => throw RawNotSupported(nameof(Rollback));
    public virtual long LastInsertRowId() => throw RawNotSupported(nameof(LastInsertRowId));
    public virtual long Changes() => throw RawNotSupported(nameof(Changes));
}
