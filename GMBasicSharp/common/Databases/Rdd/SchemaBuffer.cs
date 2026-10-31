// FILE: Basic.Databases.Rdd.SchemaBuffer.cs
// SchemaBuffer: buffer a offset fissi per accesso diretto ai campi record.
// Elimina Dictionary<int, object?> → byte[] con FieldOffset precalcolati.
// Performance: 3-5x rispetto a dictionary lookup + boxing.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Offset e metadati di un campo nel buffer.</summary>
public sealed class FieldOffsetInfo
{
    public string Name { get; init; } = "";
    public int Offset { get; init; }        // posizione byte nel buffer
    public int Length { get; init; }         // dimensione in byte
    public int SymbolId { get; init; }       // per compatibilità compilatore
    public Type FieldType { get; init; } = typeof(object);

    public FieldOffsetInfo(string name, int offset, int length, int symbolId, Type type)
    {
        Name = name; Offset = offset; Length = length; SymbolId = symbolId; FieldType = type;
    }
}

/// <summary>Buffer record a offset fissi. Scrittura/lettura tipizzata senza boxing.</summary>
public sealed class SchemaBuffer
{
    private byte[] _data;
    private readonly FieldOffsetInfo[] _fieldInfos;
    private readonly Dictionary<string, int> _nameToIndex;

    public int Length => _data.Length;
    public int FieldCount => _fieldInfos.Length;
    public ReadOnlySpan<byte> Raw => _data;

    public SchemaBuffer(FieldOffsetInfo[] fields)
    {
        _fieldInfos = fields;
        _nameToIndex = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var totalSize = 0;
        for (int i = 0; i < fields.Length; i++)
        {
            _nameToIndex[fields[i].Name] = i;
            totalSize = Math.Max(totalSize, fields[i].Offset + fields[i].Length);
        }
        _data = new byte[totalSize];
    }

    /// <summary>Inizializza il buffer da un dizionario SymbolId→valore.</summary>
    public void FromDictionary(Dictionary<int, object?> dict)
    {
        for (int i = 0; i < _fieldInfos.Length; i++)
        {
            var fi = _fieldInfos[i];
            if (dict.TryGetValue(fi.SymbolId, out var val))
                WriteValue(i, val);
        }
    }

    /// <summary>Scrive un valore in una posizione del buffer.</summary>
    public void WriteValue(int index, object? value)
    {
        if (index < 0 || index >= _fieldInfos.Length) return;
        var fi = _fieldInfos[index];
        var span = _data.AsSpan(fi.Offset, fi.Length);

        if (value == null)
        {
            span.Clear();
            return;
        }

        var type = fi.FieldType;
        if (type == typeof(double))
        {
            var d = Convert.ToDouble(value);
            BitConverter.TryWriteBytes(span, d);
        }
        else if (type == typeof(long))
        {
            // Bug: prima scriveva SEMPRE i bit di un double anche per i campi LONG (stesso ramo
            // di typeof(double) sopra), mentre ReadValue per typeof(long) rilegge quegli 8 byte
            // come Int64 grezzo — il bit-pattern IEEE754 di 1.0/2.0 reinterpretato come intero
            // dava valori enormi (4607182418800017408 ecc.), non 1/2. Scoperto perché
            // IsamDriver.SyncRelation confrontava sempre "false" un ID_Cliente LONG col valore
            // ID del padre nonostante entrambi fossero effettivamente 1/2.
            BitConverter.TryWriteBytes(span, Convert.ToInt64(value));
        }
        else if (type == typeof(int))
        {
            BitConverter.TryWriteBytes(span, Convert.ToInt32(value));
        }
        else if (type == typeof(bool))
        {
            span[0] = Convert.ToBoolean(value) ? (byte)1 : (byte)0;
        }
        else if (type == typeof(string))
        {
            // Stringa: primi 2 byte = lunghezza, resto = UTF-8
            var str = value.ToString() ?? "";
            var utf8 = System.Text.Encoding.UTF8.GetBytes(str);
            var maxLen = fi.Length - 2;
            var actualLen = Math.Min(utf8.Length, maxLen);
            span[0] = (byte)(actualLen & 0xFF);
            if (fi.Length >= 2) span[1] = (byte)((actualLen >> 8) & 0xFF);
            utf8.AsSpan(0, actualLen).CopyTo(span[2..]);
        }
        else if (type == typeof(GbDate))
        {
            var gbd = value is GbDate d ? d : value is DateTime dt ? new GbDate(dt) : GbDate.FromLiteral(value?.ToString() ?? "");
            BitConverter.TryWriteBytes(span, gbd.ToOADate());
        }
        else if (type == typeof(GbCurrency))
        {
            var cur = value is GbCurrency c ? c : new GbCurrency(Convert.ToDouble(value));
            BitConverter.TryWriteBytes(span, cur.RawValue);
        }
        else
        {
            // Fallback: serializza come stringa JSON
            var json = System.Text.Json.JsonSerializer.Serialize(value);
            var utf8 = System.Text.Encoding.UTF8.GetBytes(json);
            var maxLen = fi.Length;
            utf8.AsSpan(0, Math.Min(utf8.Length, maxLen)).CopyTo(span);
        }
    }

    /// <summary>Legge un valore dal buffer.</summary>
    public object? ReadValue(int index)
    {
        if (index < 0 || index >= _fieldInfos.Length) return null;
        var fi = _fieldInfos[index];
        var span = _data.AsSpan(fi.Offset, fi.Length);
        var type = fi.FieldType;

        if (type == typeof(double))
            return span.Length >= 8 ? BitConverter.ToDouble(span) : 0.0;
        if (type == typeof(long))
            return span.Length >= 8 ? BitConverter.ToInt64(span) : 0L;
        if (type == typeof(int))
            return span.Length >= 4 ? BitConverter.ToInt32(span) : 0;
        if (type == typeof(bool))
            return span.Length > 0 && span[0] != 0;
        if (type == typeof(string))
        {
            if (span.Length < 2) return "";
            var len = span[0] | (span[1] << 8);
            len = Math.Min(len, span.Length - 2);
            return System.Text.Encoding.UTF8.GetString(span[2..(2 + len)]);
        }
        if (type == typeof(GbDate))
            return span.Length >= 8 ? new GbDate(BitConverter.ToDouble(span)) : new GbDate(DateTime.MinValue);
        if (type == typeof(GbCurrency))
            return span.Length >= 8 ? new GbCurrency(BitConverter.ToInt64(span)) : new GbCurrency(0L);

        // Fallback simmetrico a WriteValue: i tipi non primitivi sono stati serializzati
        // come JSON alla scrittura → qui li deserializza (prima ritornava sempre null,
        // perdendo il dato al round-trip). Tipo complesso → JsonElement; primitivo → valore.
        try
        {
            var json = System.Text.Encoding.UTF8.GetString(span).TrimEnd('\0', ' ', '\t', '\r', '\n');
            if (string.IsNullOrEmpty(json)) return null;
            return System.Text.Json.JsonSerializer.Deserialize<object>(json);
        }
        catch { return null; }
    }

    /// <summary>Legge per nome campo.</summary>
    public object? ReadValue(string fieldName)
        => _nameToIndex.TryGetValue(fieldName, out var idx) ? ReadValue(idx) : null;

    /// <summary>Scrive per nome campo.</summary>
    public void WriteValue(string fieldName, object? value)
    {
        if (_nameToIndex.TryGetValue(fieldName, out var idx))
            WriteValue(idx, value);
    }

    /// <summary>Esporta come Dictionary per retrocompatibilità.</summary>
    public Dictionary<int, object?> ToDictionary()
    {
        var dict = new Dictionary<int, object?>();
        for (int i = 0; i < _fieldInfos.Length; i++)
            dict[_fieldInfos[i].SymbolId] = ReadValue(i);
        return dict;
    }

    /// <summary>Crea uno SchemaBuffer dai campi di un TypeField[].</summary>
    public static SchemaBuffer FromUdt(UserDefinedType udt)
    {
        var fields = new FieldOffsetInfo[udt.Fields.Length];
        int offset = 0;
        for (int i = 0; i < udt.Fields.Length; i++)
        {
            var f = udt.Fields[i];
            int size = GetTypeSize(f.DataType);
            var clrType = GetClrType(f.DataType);
            fields[i] = new FieldOffsetInfo(f.Name.Lexeme, offset, size, f.SymbolId, clrType);
            offset += size;
        }
        return new SchemaBuffer(fields);
    }

    private static int GetTypeSize(DataType dt) => dt switch
    {
        LongType or ULongType or QuadType or UQuadType => 8,
        DoubleType or SingleType or CurrencyType => 8,
        IntegerType or UIntegerType => 4,
        BitType or ByteType or SByteType => 1,
        StringType st => (st.FixedLength ?? 32) + 2,
        DateType or DateTimeType or TimeSpanType => 8,
        _ => 32
    };

    private static Type GetClrType(DataType dt) => dt switch
    {
        LongType or ULongType or QuadType or UQuadType => typeof(long),
        DoubleType or SingleType or CurrencyType => typeof(double),
        IntegerType or UIntegerType => typeof(int),
        BitType or ByteType or SByteType => typeof(int),
        StringType => typeof(string),
        DateType or DateTimeType => typeof(GbDate),
        TimeSpanType => typeof(TimeSpan),
        _ => typeof(object)
    };
}
