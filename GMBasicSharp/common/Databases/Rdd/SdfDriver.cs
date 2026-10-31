// FILE: Basic.Databases.Rdd.SdfDriver.cs
// Driver SDF (System Data Format - fixed-width text, Clipper standard).
// Ogni riga ha larghezza fissa. NON c'è header nel file.
// Modello: riga = record, indice virtuale = numero di riga, manipolazione con write-back
// (padding/troncamento alla lunghezza fissa al salvataggio).

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Basic.Databases.Rdd;

/// <summary>Driver per file SDF (System Data Format - fixed-width).</summary>
[DatabaseDriver("SDF", Extensions = new[] { ".sdf", ".txt" })]
public class SdfDriver : BaseDriver
{
    private List<string> _lines = new();
    private readonly HashSet<int> _deletedRows = new();
    private readonly List<int> _sortOrder = new();
    private bool _dirty;
    private int _rowIndex = -1;
    private readonly int _recordLength;
    private string? _orderField;

    private int PhysicalRow()
    {
        if (_orderField != null && _sortOrder.Count > 0 && _rowIndex >= 0 && _rowIndex < _sortOrder.Count)
            return _sortOrder[_rowIndex];
        return _rowIndex;
    }
    private int OrderCount() => _orderField != null ? _sortOrder.Count : _lines.Count;
    private int PhysicalAt(int position) =>
        _orderField != null && _sortOrder.Count > 0 && position >= 0 && position < _sortOrder.Count
            ? _sortOrder[position] : position;
    private void RebuildSort()
    {
        _sortOrder.Clear();
        if (string.IsNullOrEmpty(_orderField)) return;
        _sortOrder.AddRange(Enumerable.Range(0, _lines.Count));
        _sortOrder.Sort((a, b) => StringComparer.Ordinal.Compare(_lines[a], _lines[b]));
    }

    public override string DriverName => "SDF";

    public SdfDriver() { _recordLength = 80; } // Default 80 colonne
    public SdfDriver(int recordLength) { _recordLength = recordLength; }

    public override void Open(DbOpenInfo info)
    {
        _fileName = info.FileName;
        _alias = info.Alias;
        _lines = new List<string>();
        _deletedRows.Clear();
        _sortOrder.Clear();
        _dirty = false;
        _rowIndex = -1;
        _orderField = null;

        if (!File.Exists(_fileName))
        {
            // Nuovo file: crea vuoto (writeable) — serve per COPY TO / creazione.
            File.WriteAllText(_fileName, "");
            _lines = new List<string>();
            _fields = new RddFieldInfo[] { new RddFieldInfo("FLD1", new StringType(_recordLength), _recordLength, 0, 0) };
            _recCount = 0;
            _isOpen = true;
            _eof = true;
            _bof = true;
            return;
        }

        _lines = File.ReadAllLines(_fileName).ToList();
        _recCount = _lines.Count;

        // Campo generico: FLD1 (larghezza intera riga)
        _fields = new RddFieldInfo[]
        {
            new RddFieldInfo("FLD1", new StringType(_recordLength), _recordLength, 0, 0)
        };

        _isOpen = true;
        _eof = _lines.Count == 0;
        _bof = true;
    }

    public override void SetFields(RddFieldInfo[] fields)
    {
        _fields = fields.Length > 0 ? fields : _fields;
    }

    public override void Close()
    {
        if (_dirty) Save();
        _lines.Clear();
        _deletedRows.Clear();
        _isOpen = false;
    }

    private void Save()
    {
        var outLines = new List<string>(_lines.Count);
        foreach (var line in _lines)
            outLines.Add(Pad(line));
        File.WriteAllLines(_fileName, outLines);
        _dirty = false;
    }

    private string Pad(string s) =>
        s.Length >= _recordLength ? s[.._recordLength] : s.PadRight(_recordLength);

    // ═══ Navigazione ═══

    public override void GoTop() { if (OrderCount() == 0) { _eof = true; _bof = true; return; } _rowIndex = 0; _recNo = 1; _eof = false; _bof = false; }
    public override void GoBottom() { if (OrderCount() == 0) { _eof = true; _bof = true; return; } _rowIndex = OrderCount() - 1; _recNo = OrderCount(); _eof = false; _bof = false; }
    public override void GoTo(long n) { if (n < 1 || n > OrderCount()) { _eof = true; return; } _rowIndex = (int)n - 1; _recNo = n; _eof = false; _bof = false; }
    public override void Skip(int n)
    {
        _rowIndex += n;
        if (_rowIndex < 0) { _rowIndex = -1; _bof = true; _recNo = 0; return; }
        if (_rowIndex >= OrderCount()) { _eof = true; _recNo = OrderCount() + 1; return; }
        _recNo = _rowIndex + 1; _eof = false; _bof = false;
    }
    public override void SkipRaw(int n) => Skip(n);

    // ═══ CRUD con write-back ═══

    public override long Append(Dictionary<int, object?> fields)
    {
        var val = fields.TryGetValue(0, out var v) ? v?.ToString() : "";
        _lines.Add(val ?? "");
        if (_orderField != null) RebuildSort();
        _rowIndex = _lines.Count - 1;
        _recNo = _rowIndex + 1;
        _dirty = true;
        _eof = false; _bof = false;
        return _recNo;
    }

    public override void Delete()
    {
        var p = PhysicalRow();
        if (p >= 0 && p < _lines.Count) { _deletedRows.Add(p); _dirty = true; }
    }

    public override void Recall()
    {
        var p = PhysicalRow();
        if (p >= 0) { _deletedRows.Remove(p); _dirty = true; }
    }

    public override void Pack()
    {
        if (_deletedRows.Count == 0) return;
        _lines = _lines.Where((_, i) => !_deletedRows.Contains(i)).ToList();
        _deletedRows.Clear();
        if (_orderField != null) RebuildSort();
        _rowIndex = -1;
        _dirty = true;
        Save();
    }

    public override void Zap()
    {
        _lines.Clear();
        _deletedRows.Clear();
        _sortOrder.Clear();
        _rowIndex = -1;
        _eof = true; _bof = true;
        _dirty = true;
        Save();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        var p = PhysicalRow();
        if (p < 0 || p >= _lines.Count) return null;
        var dict = new Dictionary<int, object?> { [0] = _lines[p] };
        if (_deletedRows.Contains(p)) dict[-1] = true;
        return dict;
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        var p = PhysicalRow();
        if (p < 0 || p >= _lines.Count) return;
        if (fields.TryGetValue(0, out var v)) _lines[p] = v?.ToString() ?? "";
        if (_orderField != null) RebuildSort();
        _dirty = true;
    }

    // ═══ Indici (virtuale = riga) + SEEK ═══

    public override void CreateIndex(DbOrderInfo info) { _orderField = "FLD1"; RebuildSort(); }
    public override void OpenIndex(string name, string filePath) { _orderField = "FLD1"; RebuildSort(); }
    public override void CloseIndex(string name) { _orderField = null; _sortOrder.Clear(); }
    public override void SetOrder(string name) { _orderField = string.IsNullOrEmpty(name) ? null : "FLD1"; RebuildSort(); }

    public override bool Seek(DbSeekInfo info)
    {
        if (info.Key == null || _lines.Count == 0) return false;
        if (info.Key is double d && d >= 1 && d <= OrderCount())
        {
            _rowIndex = (int)d - 1;
            _recNo = (int)d;
            _found = true; _eof = false; _bof = false;
            return true;
        }
        int n = OrderCount();
        for (int i = 0; i < n; i++)
            if (Equals(_lines[PhysicalAt(i)].Trim(), info.Key.ToString()))
            {
                _rowIndex = i; _recNo = i + 1; _found = true; _eof = false; _bof = false;
                return true;
            }
        _found = false;
        return false;
    }

    public override string? CurrentKey() => _rowIndex >= 0 ? (_rowIndex + 1).ToString(System.Globalization.CultureInfo.InvariantCulture) : null;
    public override int CurrentKeyNo() => _rowIndex + 1;

    // ═══ Lock (advisory) ═══

    public override bool Lock(long rec) => true;
    public override void Unlock(long rec) { /* advisory */ }
    public override bool FileLock() => true;
    public override void FileUnlock() { /* advisory */ }
}
