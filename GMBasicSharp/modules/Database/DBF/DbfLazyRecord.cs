// FILE: Basic.DBF.DbfLazyRecord.cs
// Lazy field parsing per DBF: parsa i campi solo quando vengono acceduti.
// Riduce allocazioni e CPU per record con molti campi quando solo un subset è usato.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;

namespace Basic.DBF;

/// <summary>Record DBF con parsing lazy dei campi. Implementa IReadOnlyDictionary per trasparenza.</summary>
internal sealed class DbfLazyRecord : IReadOnlyDictionary<string, object?>
{
    private readonly byte[] _rawData;
    private readonly IReadOnlyList<DbfField> _fields;
    private readonly IFieldParser _parser;
    private readonly IMemoFile? _memoFile;
    private readonly Encoding _encoding;
    private readonly Dictionary<string, object?> _cache;
    private readonly Dictionary<string, int> _nameToIndex;
    private readonly int[] _offsets;

    public DbfLazyRecord(byte[] rawData, IReadOnlyList<DbfField> fields, IFieldParser parser,
        IMemoFile? memoFile, Encoding encoding)
    {
        _rawData = rawData;
        _fields = fields;
        _parser = parser;
        _memoFile = memoFile;
        _encoding = encoding;
        _cache = new Dictionary<string, object?>(fields.Count, StringComparer.OrdinalIgnoreCase);
        _nameToIndex = new Dictionary<string, int>(fields.Count, StringComparer.OrdinalIgnoreCase);
        _offsets = new int[fields.Count];

        int offset = 0;
        for (int i = 0; i < fields.Count; i++)
        {
            _offsets[i] = offset;
            _nameToIndex[fields[i].Name] = i;
            offset += fields[i].Length;
        }
    }

    /// <summary>Forza il parsing di tutti i campi (per compatibilità).</summary>
    public Dictionary<string, object?> Materialize()
    {
        for (int i = 0; i < _fields.Count; i++)
        {
            if (!_cache.ContainsKey(_fields[i].Name))
                _cache[_fields[i].Name] = ParseField(i);
        }
        return _cache;
    }

    private object? ParseField(int index)
    {
        var field = _fields[index];
        var fieldData = _rawData.AsSpan(_offsets[index], field.Length);
        return _parser.Parse(field, fieldData, _memoFile, _encoding);
    }

    // ═══ IReadOnlyDictionary implementation ═══

    public object? this[string key]
    {
        get
        {
            if (_cache.TryGetValue(key, out var val)) return val;
            if (_nameToIndex.TryGetValue(key, out var idx))
            {
                val = ParseField(idx);
                _cache[key] = val;
                return val;
            }
            return null;
        }
    }

    public IEnumerable<string> Keys => _fields.Select(f => f.Name);
    public IEnumerable<object?> Values
    {
        get
        {
            for (int i = 0; i < _fields.Count; i++)
                yield return this[_fields[i].Name];
        }
    }

    public int Count => _fields.Count;
    public bool ContainsKey(string key) => _nameToIndex.ContainsKey(key);
    
    public bool TryGetValue(string key, out object? value)
    {
        if (_cache.TryGetValue(key, out value)) return true;
        if (_nameToIndex.TryGetValue(key, out var idx))
        {
            value = ParseField(idx);
            _cache[key] = value;
            return true;
        }
        value = null;
        return false;
    }

    public IEnumerator<KeyValuePair<string, object?>> GetEnumerator()
    {
        for (int i = 0; i < _fields.Count; i++)
            yield return new KeyValuePair<string, object?>(_fields[i].Name, this[_fields[i].Name]);
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
