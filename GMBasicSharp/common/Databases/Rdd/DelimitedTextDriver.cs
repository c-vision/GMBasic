// FILE: Basic.Databases.Rdd.DelimitedTextDriver.cs
// Base comune per CSV/DELIM: file testo delimitato con quoting RFC 4180.
// Modello: ogni riga fisica = un record; l'INDICE VIRTUALE coincide con il numero
// di riga (1-based). Supporta manipolazione con write-back (salva al Close se dirty).

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Basic.Databases.Rdd;

public abstract class DelimitedTextDriver : BaseDriver
{
    protected string _separator = ",";
    protected bool _hasHeader = true;
    protected List<string?[]> _rows = new();
    private readonly HashSet<int> _deletedRows = new();
    private readonly List<int> _sortOrder = new();
    private bool _dirty;
    private int _rowIndex = -1;
    protected string? _orderField;

    /// <summary>Indice RIGA (in _rows) alla posizione corrente dell'ordine attivo.</summary>
    private int PhysicalRow()
    {
        if (_orderField != null && _sortOrder.Count > 0 && _rowIndex >= 0 && _rowIndex < _sortOrder.Count)
            return _sortOrder[_rowIndex];
        return _rowIndex;
    }

    protected string Separator => _separator;
    protected bool HasHeader => _hasHeader;

    public override void Open(DbOpenInfo info)
    {
        _fileName = info.FileName;
        _alias = info.Alias;
        _rows = new List<string?[]>();
        _deletedRows.Clear();
        _sortOrder.Clear();
        _dirty = false;
        _rowIndex = -1;
        _orderField = null;

        if (!File.Exists(_fileName))
        {
            // Nuovo file: crea vuoto (writeable) — serve per COPY TO / SORT TO / creazione.
            File.WriteAllText(_fileName, _hasHeader ? "" : "");
            _fields = Array.Empty<RddFieldInfo>();
            _rows = new List<string?[]>();
            _recCount = 0;
            _isOpen = true;
            _eof = true;
            _bof = true;
            return;
        }

        var lines = File.ReadAllLines(_fileName);
        if (lines.Length == 0)
        {
            _recCount = 0;
            _fields = Array.Empty<RddFieldInfo>();
            _isOpen = true;
            return;
        }

        int dataStart;
        if (_hasHeader)
        {
            var header = ParseLine(lines[0]);
            _fields = new RddFieldInfo[header.Length];
            for (int i = 0; i < header.Length; i++)
                _fields[i] = new RddFieldInfo(header[i]?.Trim() ?? $"F{i + 1}", new StringType(255), 255, i, i);
            dataStart = 1;
        }
        else
        {
            var sample = ParseLine(lines[0]);
            _fields = new RddFieldInfo[sample.Length];
            for (int i = 0; i < sample.Length; i++)
                _fields[i] = new RddFieldInfo($"FLD{i + 1}", new StringType(255), 255, i, i);
            dataStart = 0;
        }

        for (int i = dataStart; i < lines.Length; i++)
            _rows.Add(NormalizeRow(ParseLine(lines[i])));

        _recCount = _rows.Count;
        _isOpen = true;
        _eof = _rows.Count == 0;
        _bof = true;
    }

    public override void SetFields(RddFieldInfo[] fields)
    {
        _fields = fields;
        if (_hasHeader) _dirty = true; // il header va scritto al Save dal nuovo schema
    }

    public override void Close()
    {
        if (_dirty) Save();
        _rows.Clear();
        _deletedRows.Clear();
        _isOpen = false;
    }

    private string?[] NormalizeRow(string?[] parts)
    {
        var row = new string?[_fields.Length];
        for (int j = 0; j < _fields.Length && j < parts.Length; j++) row[j] = parts[j];
        return row;
    }

    // ═══ Parsing RFC 4180 ═══

    protected string?[] ParseLine(string line)
    {
        if (string.IsNullOrEmpty(line)) return [];
        var fields = new List<string?>();
        var sb = new StringBuilder();
        var inQuotes = false;
        for (int i = 0; i < line.Length; i++)
        {
            var c = line[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < line.Length && line[i + 1] == '"') { sb.Append('"'); i++; }
                    else inQuotes = false;
                }
                else sb.Append(c);
            }
            else if (c == '"')
            {
                inQuotes = true;
            }
            else if (_separator.Length > 0 && c == _separator[0])
            {
                fields.Add(sb.ToString());
                sb.Clear();
            }
            else sb.Append(c);
        }
        fields.Add(sb.ToString());
        return [.. fields];
    }

    protected string FormatLine(string?[] fields)
    {
        var parts = new List<string>(fields.Length);
        foreach (var f in fields)
        {
            var s = f ?? "";
            var needQuote = s.IndexOf(_separator) >= 0 || s.IndexOf('"') >= 0
                || s.IndexOf('\n') >= 0 || s.IndexOf('\r') >= 0;
            parts.Add(needQuote ? "\"" + s.Replace("\"", "\"\"") + "\"" : s);
        }
        return string.Join(_separator, parts);
    }

    private void Save()
    {
        var lines = new List<string>(_rows.Count + (_hasHeader ? 1 : 0));
        if (_hasHeader && _fields.Length > 0)
            lines.Add(FormatLine([.. _fields.Select(f => f.Name)]));
        foreach (var row in _rows)
            lines.Add(FormatLine(row));
        File.WriteAllLines(_fileName, lines);
        _dirty = false;
    }

    // ═══ Navigazione (rispetta l'ordine attivo) ═══

    private int OrderCount() => _orderField != null ? _sortOrder.Count : _rows.Count;

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
        // Chiave per SymbolId (_fields[i].SymbolId), non per indice posizionale grezzo `i` —
        // stessa convenzione di BaseDriver.GetFieldValue/tutti gli altri driver (Isam/Dbf/
        // Speed). Per i campi auto-generati da Open() (header CSV) SymbolId == indice, quindi
        // nessuna differenza in un round-trip CSV-su-CSV; ma quando SetFields() importa lo
        // schema di un ALTRO driver (es. CoreDb.CopyTo ISAM→CSV per EXPORT ... AS CSV), i
        // SymbolId sono quelli ORIGINALI del driver sorgente (non 0,1,2...) — con la chiave
        // posizionale, ogni TryGetValue(i) falliva sempre e la riga veniva scritta vuota.
        var row = new string?[_fields.Length];
        for (var i = 0; i < _fields.Length; i++)
            if (fields.TryGetValue(_fields[i].SymbolId, out var v)) row[i] = v?.ToString();
        _rows.Add(row);
        if (_orderField != null) RebuildSort();
        _rowIndex = _rows.Count - 1;
        _recNo = _rowIndex + 1;
        _dirty = true;
        _eof = false; _bof = false;
        return _recNo;
    }

    public override void Delete()
    {
        var p = PhysicalRow();
        if (p >= 0 && p < _rows.Count) { _deletedRows.Add(p); _dirty = true; }
    }

    public override void Recall()
    {
        var p = PhysicalRow();
        if (p >= 0) { _deletedRows.Remove(p); _dirty = true; }
    }

    public override void Pack()
    {
        if (_deletedRows.Count == 0) return;
        var keep = new List<string?[]>();
        for (int i = 0; i < _rows.Count; i++)
            if (!_deletedRows.Contains(i)) keep.Add(_rows[i]);
        _rows = keep;
        _deletedRows.Clear();
        if (_orderField != null) RebuildSort();
        _rowIndex = -1;
        _dirty = true;
        Save();
    }

    public override void Zap()
    {
        _rows.Clear();
        _deletedRows.Clear();
        _sortOrder.Clear();
        _rowIndex = -1;
        _eof = true; _bof = true;
        _dirty = true;
        Save();
    }

    public override Dictionary<int, object?>? GetCurrentRecord()
    {
        // Chiave per SymbolId (vedi Append per il motivo) — non l'indice posizionale.
        var p = PhysicalRow();
        if (p < 0 || p >= _rows.Count) return null;
        var dict = new Dictionary<int, object?>();
        var row = _rows[p];
        for (int i = 0; i < _fields.Length; i++)
            dict[_fields[i].SymbolId] = i < row.Length ? row[i] : null;
        if (_deletedRows.Contains(p)) dict[-1] = true;
        return dict;
    }

    public override void PutCurrentRecord(Dictionary<int, object?> fields)
    {
        var p = PhysicalRow();
        if (p < 0 || p >= _rows.Count) return;
        var row = _rows[p];
        for (var i = 0; i < _fields.Length; i++)
            if (fields.TryGetValue(_fields[i].SymbolId, out var v)) row[i] = v?.ToString();
        if (_orderField != null) RebuildSort();
        _dirty = true;
    }

    // ═══ Indici (virtuale = numero di riga) + SEEK + SORT reale ═══

    /// <summary>Ricostruisce l'ordine delle righe in base al campo attivo.</summary>
    private void RebuildSort()
    {
        _sortOrder.Clear();
        if (string.IsNullOrEmpty(_orderField)) return;
        var fi = GetFieldIndex(_orderField);
        if (fi < 0) return;
        _sortOrder.AddRange(Enumerable.Range(0, _rows.Count));
        _sortOrder.Sort((a, b) =>
        {
            var va = a < _rows.Count && fi < _rows[a].Length ? _rows[a][fi] : null;
            var vb = b < _rows.Count && fi < _rows[b].Length ? _rows[b][fi] : null;
            if (va is null && vb is null) return 0;
            if (va is null) return -1;
            if (vb is null) return 1;
            return StringComparer.Ordinal.Compare(va, vb);
        });
    }

    public override void CreateIndex(DbOrderInfo info)
    {
        // File testo: l'indice virtuale è il numero di riga; SORT per campo per SEEK/ordine.
        if (!string.IsNullOrEmpty(info.KeyExpression)) _orderField = info.KeyExpression;
        else if (!string.IsNullOrEmpty(info.Name)) _orderField = info.Name;
        RebuildSort();
    }
    public override void OpenIndex(string name, string filePath)
    {
        if (!string.IsNullOrEmpty(name)) _orderField = name;
        else if (!string.IsNullOrEmpty(filePath)) _orderField = filePath;
        RebuildSort();
    }
    public override void CloseIndex(string name) { _orderField = null; _sortOrder.Clear(); }
    public override void SetOrder(string name)
    {
        _orderField = string.IsNullOrEmpty(name) ? null : name;
        RebuildSort();
    }

    public override bool Seek(DbSeekInfo info)
    {
        if (info.Key == null || _rows.Count == 0) return false;
        // Indice virtuale: se la chiave è un numero entro il range → posiziona sulla riga.
        if (info.Key is double d && d >= 1 && d <= OrderCount())
        {
            _rowIndex = (int)d - 1;
            _recNo = (int)d;
            _found = true; _eof = false; _bof = false;
            return true;
        }
        // Altrimenti cerca per valore nel campo ordinato (o in tutti i campi).
        var fi = _orderField != null ? GetFieldIndex(_orderField) : -1;
        int n = OrderCount();
        for (int i = 0; i < n; i++)
        {
            var row = _rows[PhysicalAt(i)];
            if (fi >= 0)
            {
                if (fi < row.Length && Equals(row[fi], info.Key.ToString()))
                {
                    _rowIndex = i; _recNo = i + 1; _found = true; _eof = false; _bof = false;
                    return true;
                }
            }
            else
            {
                foreach (var f in row)
                    if (Equals(f, info.Key.ToString())) { _rowIndex = i; _recNo = i + 1; _found = true; _eof = false; _bof = false; return true; }
            }
        }
        _found = false;
        return false;
    }

    private int PhysicalAt(int position) =>
        _orderField != null && _sortOrder.Count > 0 && position >= 0 && position < _sortOrder.Count
            ? _sortOrder[position] : position;

    public override string? CurrentKey() => _rowIndex >= 0 ? (_rowIndex + 1).ToString(System.Globalization.CultureInfo.InvariantCulture) : null;
    public override int CurrentKeyNo() => _rowIndex + 1;

    // ═══ Lock (advisory: file locale, nessuna concorrenza) ═══

    public override bool Lock(long rec) => true;
    public override void Unlock(long rec) { /* advisory */ }
    public override bool FileLock() => true;
    public override void FileUnlock() { /* advisory */ }
}
