// FILE: Basic.Databases.Rdd.SpeedDriver.cs
using System.Collections.Generic;
using System.Linq;
using Basic.Databases.Speed;

namespace Basic.Databases.Rdd;

[DatabaseDriver("SPEED", Extensions = new[] { ".speed" }, Description = "SPEED LSM-tree key-value store")]
public class SpeedDriver : BaseDriver
{
    internal Database? _db;
    internal int _tableId;
    private byte[]? _currentKey;
    private byte[]? _currentValue;
    private Config.SpeedSchema? _schema;

    /// <summary>Accesso al motore SPEED per le operazioni di configurazione (durabilità).</summary>
    public Database? Db => _db;

    /// <summary>ID della tabella corrente (per SETMANY/bulk).</summary>
    public int TableId => _tableId;

    public override string DriverName => "SPEED";

    public SpeedDriver() { }
    public SpeedDriver(Database db, string alias)
    {
        _db = db;
        _alias = alias;
        _fileName = alias;
        _isOpen = true;
        SyncCursorState();
    }

    /// <summary>Carica lo schema della tabella dal manifest e popola _fields (nomi+ordine).
    /// Necessario per serializzare i record nel formato NATIVO del motore (SpeedMarshaller),
    /// non JSON — altrimenti Delete/indici non riescono a deserializzare il vecchio record.</summary>
    private void LoadSchema()
    {
        if (_db == null || _schema != null || _tableId <= 0) return;
        if (_db.Manifest.TryGetTable(_tableId, out var meta) && meta != null)
        {
            _schema = meta.Schema;
            _fields = [.. meta.Schema.Fields.Select((f, i) => new RddFieldInfo(f.Name, f.DataType, f.Length, i, i))];
        }
    }

    /// <summary>
    /// Punta questo driver (già costruito su un `Database` condiviso via
    /// <see cref="SpeedDriver(Database, string)"/>) a una tabella SPECIFICA per ID — usato da
    /// `SPEED USE tableName FROM dbAlias OF Type ALIAS tableAlias INDEXED BY key` per legare
    /// l'alias della TABELLA (non del database) alla tabella corretta con lo schema derivato dal
    /// tipo UDT. Prima, `SPEED USE` chiamava solo `SpeedOpen(dbAlias, tableAlias)` — ignorava
    /// completamente `tableName`/lo schema UDT/`INDEXED BY`, riaprendo lo stesso database sotto
    /// un alias diverso: `_tableId` restava 0, `Open()` sceglieva la prima tabella del manifest
    /// (o ne creava una "KV" di default) invece della tabella richiesta, e senza schema
    /// registrato `BaseDriver.GetFieldValue` si fermava dopo pochissimi campi (o zero) — bug
    /// trovato dal vivo isolando un crash SIGSEGV in SPEED GET, causa radice distinta e più a
    /// monte (vedi Bugs/ILBackend-DbSpeedFullTests-Silent-Hang.md).
    /// </summary>
    public void UseTable(int tableId)
    {
        _tableId = tableId;
        _schema = null;
        LoadSchema();
        SyncCursorState();
    }

    public override void Open(DbOpenInfo info)
    {
        _db = new Database(info.FileName);
        _alias = info.Alias;
        _fileName = info.FileName;
        _isOpen = true;
        // RDD USE su un database SPEED (senza la clausola SPEED USE ... FROM che setta la
        // tabella): usa la PRIMA tabella del manifest (o crea "KV" se vuoto).
        if (_tableId <= 0)
        {
            var first = _db.Manifest.GetAllTables().OrderBy(t => t.Id).FirstOrDefault();
            _tableId = first != null ? first.Id : _db.CreateTable(new Config.SpeedSchema("KV",
                [
                    new Config.SpeedFieldSchema("KEY", new Basic.StringType(), 32),
                    new Config.SpeedFieldSchema("VALUE", new Basic.StringType(), 32),
                ],
                new Config.SpeedIndexDefinition("PK", "KEY", new Basic.StringType(), 32, true, false),
                [])).Id;
            LoadSchema();
        }
        SyncCursorState();
    }

    public override void Close() { _db?.Dispose(); _db = null; _isOpen = false; }

    // ═══ Navigazione ═══

    private void FetchCurrentValue()
    {
        if (_db == null || _tableId <= 0) return;
        var cursor = _db.GetCursor(_tableId);
        _currentKey = cursor.CurrentIndexKey ?? cursor.CurrentPrimaryKey;
        _currentValue = _db.GetCurrent(_tableId);
    }

    public override void GoTop()
    {
        _db?.GoTop(_tableId);
        FetchCurrentValue();
        SyncCursorState();
        while (!_eof && !PassesFilter()) { _db?.Skip(_tableId, 1); FetchCurrentValue(); SyncCursorState(); }
    }

    public override void GoBottom()
    {
        _db?.GoBottom(_tableId);
        FetchCurrentValue();
        SyncCursorState();
    }

    public override void GoTo(long n)
    {
        if (_db == null || n <= 1) { GoTop(); return; }
        GoTop();
        for (long i = 1; i < n; i++)
        {
            _db.Skip(_tableId, 1);
            FetchCurrentValue();
            SyncCursorState();
            if (_eof) break;
        }
    }

    public override void Skip(int n)
    {
        for (var i = 0; i < Math.Abs(n); i++)
        {
            _db?.Skip(_tableId, Math.Sign(n));
            FetchCurrentValue();
            SyncCursorState();
            if (_eof) break;
            if (_filterEval != null)
            {
                while (!_eof && !PassesFilter()) { _db?.Skip(_tableId, Math.Sign(n)); FetchCurrentValue(); SyncCursorState(); }
            }
        }
    }

    public override void SkipRaw(int n)
    {
        _db?.Skip(_tableId, n);
        FetchCurrentValue();
        SyncCursorState();
    }

    // ═══ CRUD ═══

    public override long Append(Dictionary<int, object?> fields)
    {
        if (_db == null) return 0;
        LoadSchema();
        if (_schema == null) return 0;
        var byName = FieldsToSchemaDict(fields);
        var val = SpeedMarshaller.ToBytes(byName, _schema);
        // Convenzione SPEED: il primo campo del record è la chiave primaria (USE INDEXED BY).
        var keyVal = fields.Count > 0
            ? fields.OrderBy(kv => kv.Key).First().Value?.ToString()
            : null;
        var key = System.Text.Encoding.UTF8.GetBytes(string.IsNullOrEmpty(keyVal) ? _recCount.ToString() : keyVal);
        _db.Set(_tableId, key, val);
        SyncCursorState();
        return _recNo;
    }

    private Dictionary<string, object?> FieldsToSchemaDict(Dictionary<int, object?> fields)
    {
        var byName = new Dictionary<string, object?>(System.StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < _fields.Length && i < _schema!.Fields.Count; i++)
            if (fields.TryGetValue(_fields[i].SymbolId, out var v)) byName[_fields[i].Name] = v;
        return byName;
    }

    public override void Delete()
    {
        if (_currentKey != null)
            _db?.Delete(_tableId, _currentKey);
        SyncCursorState();
    }

    public override void Recall()
    {
        // SPEED è un KV store: DELETE rimuove FISICAMENTE la coppia (nessun tombstone).
        // Non c'è un "undelete" recuperabile a posteriori → errore esplicito, non no-op.
        throw new NotSupportedException("SPEED: DELETE è fisico (KV), RECALL non è applicabile.");
    }
    public override void Pack()
    {
        // In un LSM la compattazione è il corrispettivo reale del PACK: riunisce le run e
        // recupera spazio. La coppia eliminata è già rimossa fisicamente dalla Delete.
        if (_db == null) return;
        try
        {
            if (_db.Manifest.TryGetTable(_tableId, out var meta) && meta != null)
                _db.ForceCompactTable(meta.Name);
        }
        catch { }
        SyncCursorState();
    }
    public override void Zap()
    {
        // Rimuove TUTTE le coppie chiave→valore della tabella (Scan + Delete).
        if (_db == null) return;
        foreach (var kv in _db.Scan(_tableId).ToList())
            _db.Delete(_tableId, kv.Key);
        _currentKey = null;
        _currentValue = null;
        SyncCursorState();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        if (_currentValue == null) return null;
        LoadSchema();
        if (_schema == null) return null;
        try
        {
            var byName = SpeedMarshaller.FromBytes(_currentValue, _schema) as IReadOnlyDictionary<string, object?>;
            if (byName is null) return null;
            var result = new Dictionary<int, object?>();
            for (var i = 0; i < _fields.Length; i++)
                if (byName.TryGetValue(_fields[i].Name, out var v)) result[i] = v;
            return result;
        }
        catch { return null; }
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        if (_db == null || _currentKey == null) return;
        LoadSchema();
        if (_schema == null) return;
        var byName = FieldsToSchemaDict(fields);
        _currentValue = SpeedMarshaller.ToBytes(byName, _schema);
        _db.Set(_tableId, _currentKey, _currentValue);
        SyncCursorState();
    }

    // ═══ Indici ═══

    public override void CreateIndex(DbOrderInfo info)
    {
        if (_db == null || string.IsNullOrEmpty(info.Name)) return;
        var def = new Basic.Databases.Speed.Config.SpeedIndexDefinition(
            info.Name, info.KeyExpression, new Basic.StringType(),
            info.KeyLength > 0 ? info.KeyLength : 32, info.Unique, false);
        try { _db.CreateSecondaryIndex(_tableId, def); } catch { }
    }
    public override void OpenIndex(string name, string filePath)
    {
        // SPEED: gli indici secondari sono tabelle DENTRO il database (nessun file esterno)
        // → attiva l'ordine per nome se esiste.
        SetOrder(name);
    }
    public override void CloseIndex(string name) => _db?.DropIndex(_tableId, name);
    public override void SetOrder(string name) => _db?.SetOrder(_tableId, name);

    public override bool Seek(DbSeekInfo info)
    {
        if (_db == null || info.Key == null) return false;
        var key = System.Text.Encoding.UTF8.GetBytes(info.Key.ToString()!);
        _currentValue = _db.Get(_tableId, key);
        _currentKey = key;
        _found = _currentValue != null;
        _recNo = 1;
        return _found;
    }

    /// <summary>
    /// Riposiziona questo driver (figlio) con SEEK sulla chiave primaria, usando il valore
    /// corrente del campo/espressione chiave nel driver padre — stesso contratto di
    /// DbfDriver.SyncRelation. Prima dell'introduzione di questo override, SPEED non aveva
    /// alcun supporto per SET RELATION (BaseDriver.SyncRelation di default è un no-op): `SPEED
    /// SET RELATION` restava un puro no-op runtime anche una volta collegata la sintassi.
    /// </summary>
    public override bool SyncRelation()
    {
        if (_parentRelation == null || string.IsNullOrEmpty(_relationKeyExpression)) return _found;

        object? parentValue;
        var parentFieldIndex = _parentRelation.GetFieldIndex(_relationKeyExpression);
        if (parentFieldIndex >= 0)
        {
            parentValue = _parentRelation.GetFieldValue(parentFieldIndex);
        }
        else if (Basic.Databases.KeyExpressionPCodeHook.CompileFactory?.Invoke(_relationKeyExpression) is { } compiled)
        {
            parentValue = compiled(BuildFieldContext(_parentRelation));
        }
        else
        {
            _found = false;
            return false;
        }
        if (parentValue == null) { _found = false; return false; }
        return Seek(new DbSeekInfo { Key = parentValue });
    }

    private RuntimeExpressionEvaluator.ExpressionEvaluator? _filterEval;
    public override void SetFilter(string? expression)
    {
        _filterEval = RuntimeExpressionEvaluator.CreateEvaluator(expression);
    }

    private bool PassesFilter()
    {
        if (_filterEval == null || _currentValue == null) return true;
        try
        {
            var ctx = GetCurrentRecord() is { } rec
                ? rec.ToDictionary(kv => _fields.FirstOrDefault(f => f.SymbolId == kv.Key)?.Name ?? kv.Key.ToString(), kv => kv.Value, StringComparer.OrdinalIgnoreCase)
                : new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            return _filterEval("", ctx);
        }
        catch { return true; }
    }

    public void DropIndex(string indexName)
    {
        _db?.DropIndex(_tableId, indexName);
    }

    public override string? CurrentKey() => _currentKey != null
        ? System.Text.Encoding.UTF8.GetString(_currentKey) : null;
    public override int CurrentKeyNo() => 0;

    // ═══ Lock ═══

    public override bool Lock(long rec) => false;
    public override void Unlock(long rec) { }

    // ═══ Info ═══

    public override object? Info(int infoType, object? param) => infoType switch
    {
        1 => _recNo,
        2 => _recCount,
        4 => _alias,
        5 => _fileName,
        6 => _isOpen,
        _ => null
    };

    // ═══ Helpers ═══

    private void SyncCursorState()
    {
        if (_db == null || _tableId <= 0) return;
        var cursor = _db.GetCursor(_tableId);
        _eof = cursor.Eof;
        _bof = cursor.Bof;
        _recNo = cursor.Eof ? 0 : 1;
        _recCount = 0;
    }
}
