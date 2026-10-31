// FILE: Basic.Databases.Rdd.ArrayDriver.cs
// ArrayDriver — database in memoria (XSharp ARRAY RDD).
// Record stored in List<Dictionary<int, object?>>.
// Zero I/O, massima velocità. Perfetto per temp table e testing.

using System;
using System.Collections.Generic;
using System.Linq;

namespace Basic.Databases.Rdd;

[DatabaseDriver("ARRAY", Extensions = new[] { ":memory:" }, Description = "In-memory database (ARRAY RDD)")]
public class ArrayDriver : BaseDriver
{
    private readonly List<Dictionary<int, object?>> _records = new();
    private int _currentIndex = -1;
    private string? _orderField;                 // campo attivo di ordinamento (SET ORDER)
    private readonly List<int> _sortOrder = [];  // indici FISICI in ordine di _orderField
    private readonly Dictionary<string, string> _indexes = new(StringComparer.OrdinalIgnoreCase); // nome -> campo

    public override string DriverName => "ARRAY";

    // ═══ Schema: definisci i campi prima di usare il driver ═══
    public override void SetFields(RddFieldInfo[] fields) => _fields = fields;

    public ArrayDriver() { _isOpen = true; _alias = "ARRAY"; }
    public ArrayDriver(string alias) { _isOpen = true; _alias = alias; }

    public override void Open(DbOpenInfo info)
    {
        _alias = info.Alias;
        _fileName = info.FileName;
        _isOpen = true;
        _records.Clear();
        _currentIndex = -1;
    }

    public override void Close()
    {
        _records.Clear();
        _currentIndex = -1;
        _isOpen = false;
    }

    // ═══ Navigazione (rispetta l'ordine attivo) ═══

    /// <summary>Indice FISICO alla posizione corrente dell'ordine attivo.</summary>
    private int PhysicalIndex() =>
        _orderField != null && _sortOrder.Count > 0 && _currentIndex >= 0 && _currentIndex < _sortOrder.Count
            ? _sortOrder[_currentIndex]
            : _currentIndex;

    public override void GoTop()
    {
        var n = _orderField != null ? _sortOrder.Count : _records.Count;
        _currentIndex = n > 0 ? 0 : -1;
        SyncState();
    }

    public override void GoBottom()
    {
        var n = _orderField != null ? _sortOrder.Count : _records.Count;
        _currentIndex = n > 0 ? n - 1 : -1;
        SyncState();
    }

    public override void GoTo(long n)
    {
        var count = _orderField != null ? _sortOrder.Count : _records.Count;
        if (n >= 1 && n <= count) _currentIndex = (int)n - 1;
        SyncState();
    }

    public override void Skip(int n)
    {
        _currentIndex += n;
        SyncState();
    }

    public override void SkipRaw(int n) => Skip(n);

    // ═══ CRUD ═══

    public override long Append(Dictionary<int, object?> fields)
    {
        _records.Add(new Dictionary<int, object?>(fields));
        if (_orderField != null) RebuildSort();
        _currentIndex = _records.Count - 1;
        SyncState();
        return _recNo;
    }

    public override void Delete()
    {
        var p = PhysicalIndex();
        if (p >= 0 && p < _records.Count) _records[p][-1] = true;
        SyncState();
    }

    public override void Recall()
    {
        var p = PhysicalIndex();
        if (p >= 0 && p < _records.Count) _records[p].Remove(-1);
        SyncState();
    }

    public override void Pack()
    {
        _records.RemoveAll(r => r.ContainsKey(-1));
        if (_orderField != null) RebuildSort();
        if (_currentIndex >= _sortOrder.Count && _orderField == null) _currentIndex = _records.Count - 1;
        SyncState();
    }

    public override void Zap()
    {
        _records.Clear();
        _sortOrder.Clear();
        _currentIndex = -1;
        SyncState();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        var p = PhysicalIndex();
        if (p < 0 || p >= _records.Count) return null;
        return new Dictionary<int, object?>(_records[p]);
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        var p = PhysicalIndex();
        if (p < 0 || p >= _records.Count) return;
        var rec = _records[p];
        foreach (var kv in fields) rec[kv.Key] = kv.Value;
        if (_orderField != null) RebuildSort();
        SyncState();
    }

    // ═══ Indici (in-memory: ordine di campo + SEEK binario) ═══

    /// <summary>Ricostruisce l'ordine fisico in base al campo attivo (_orderField).</summary>
    private void RebuildSort()
    {
        _sortOrder.Clear();
        if (string.IsNullOrEmpty(_orderField)) return;
        var fi = GetFieldIndex(_orderField);
        if (fi < 0) return;
        _sortOrder.AddRange(Enumerable.Range(0, _records.Count));
        _sortOrder.Sort((a, b) =>
        {
            _records[a].TryGetValue(fi, out var va);
            _records[b].TryGetValue(fi, out var vb);
            if (va is null && vb is null) return 0;
            if (va is null) return -1;
            if (vb is null) return 1;
            return Comparer<object>.Default.Compare(va, vb);
        });
    }

    public override void CreateIndex(DbOrderInfo info)
    {
        if (string.IsNullOrEmpty(info.Name)) return;
        _indexes[info.Name] = string.IsNullOrEmpty(info.KeyExpression) ? info.Name : info.KeyExpression;
        if (_orderField == null) SetOrder(info.Name);
    }
    public override void OpenIndex(string name, string filePath)
    {
        // ARRAY non ha file indice: OpenIndex attiva l'ordine per nome (se registrato)
        // o per campo (il filePath è interpretato come nome campo).
        if (_indexes.ContainsKey(name)) SetOrder(name);
        else if (!string.IsNullOrEmpty(filePath)) SetOrder(filePath);
        else SetOrder(name);
    }
    public override void CloseIndex(string name)
    {
        string? field = _indexes.TryGetValue(name, out var f) ? f : name;
        _indexes.Remove(name);
        // Se l'ordine attivo proveniva da questo indice, torna all'ordine fisico.
        if (_orderField != null && _orderField.Equals(field, StringComparison.OrdinalIgnoreCase))
            SetOrder("");
    }
    public override void SetOrder(string name)
    {
        // Nome vuoto → ordine fisico; altrimenti campo (o indice registrato → suo campo).
        if (string.IsNullOrEmpty(name))
        {
            _orderField = null;
            _sortOrder.Clear();
            _currentIndex = _records.Count > 0 ? 0 : -1;
            SyncState();
            return;
        }
        var field = _indexes.TryGetValue(name, out var f) ? f : name;
        _orderField = field;
        RebuildSort();
        _currentIndex = _sortOrder.Count > 0 ? 0 : -1;
        SyncState();
    }

    public override bool Seek(DbSeekInfo info)
    {
        if (info.Key == null) return false;
        // Con ordine attivo: cerca nel campo ordinato (lineare su _sortOrder — in-memory).
        if (_orderField != null)
        {
            var fi = GetFieldIndex(_orderField);
            for (var i = 0; i < _sortOrder.Count; i++)
            {
                var rec = _records[_sortOrder[i]];
                if (rec.TryGetValue(fi, out var v) && Equals(v, info.Key))
                {
                    _currentIndex = i;
                    _found = true;
                    SyncState();
                    return true;
                }
            }
            _found = false;
            return false;
        }
        for (int i = 0; i < _records.Count; i++)
        {
            foreach (var kv in _records[i])
            {
                if (Equals(kv.Value, info.Key))
                {
                    _currentIndex = i;
                    _found = true;
                    SyncState();
                    return true;
                }
            }
        }
        _found = false;
        return false;
    }

    public override string? CurrentKey() => null;
    public override int CurrentKeyNo() => _currentIndex;

    // ═══ Lock (advisory: array è sempre in-memory e locale, nessuna concorrenza) ═══

    public override bool Lock(long rec) => true;
    public override void Unlock(long rec) { /* advisory — array in-memory locale */ }
    public override bool FileLock() => true;
    public override void FileUnlock() { /* advisory — array in-memory locale */ }

    // ═══ Info ═══

    public override object? Info(int infoType, object? param) => infoType switch
    {
        1 => _recNo,
        2 => _recCount,
        3 => _fields.Length,
        4 => _alias,
        5 => ":memory:",
        6 => _isOpen,
        _ => null
    };

    // ═══ Helpers ═══

    private void SyncState()
    {
        var orderCount = _orderField != null ? _sortOrder.Count : _records.Count;
        _eof = _currentIndex < 0 || _currentIndex >= orderCount;
        _bof = _currentIndex < 0;
        _found = _currentIndex >= 0 && _currentIndex < orderCount;
        _recNo = _currentIndex + 1;
        _recCount = _records.Count;
    }
}
