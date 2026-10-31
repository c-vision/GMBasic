// FILE: Basic.DBF.DbfFieldParser.cs

using System.Globalization;
using System.Text;

// ReSharper disable UnusedParameter.Local

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

/// <summary>
/// Implementazione di IFieldParser che supporta i tipi di campo DBF standard.
/// </summary>
internal class FieldParser : IFieldParser
{
    private static readonly Dictionary<DbfFieldType, Func<FieldParser, DbfField, ReadOnlySpan<byte>, IMemoFile?, Encoding, object?>> Parsers;

    static FieldParser()
    {
        Parsers = new Dictionary<DbfFieldType, Func<FieldParser, DbfField, ReadOnlySpan<byte>, IMemoFile?, Encoding, object?>>
        {
            { DbfFieldType.Character, (_, f, d, _, e) => ParseCharacter(f, d, e) },
            { DbfFieldType.Date, (_, f, d, _, _) => ParseDate(f, d) },
            { DbfFieldType.Float, (_, f, d, _, _) => ParseFloat(f, d) },
            { DbfFieldType.Integer, (_, f, d, _, _) => ParseInteger(f, d) },
            { DbfFieldType.Logical, (_, f, d, _, _) => ParseLogical(f, d) },
            { DbfFieldType.Memo, (_, f, d, m, e) => ParseMemo(f, d, m, e) },
            { DbfFieldType.Numeric, (_, f, d, _, _) => ParseNumeric(f, d) },
            { DbfFieldType.Double, (_, f, d, _, _) => ParseDouble(f, d) },
            { DbfFieldType.DateTime, (_, f, d, _, _) => ParseTimestamp(f, d) },
            { DbfFieldType.Currency, (_, f, d, _, _) => ParseCurrency(f, d) },
            { DbfFieldType.General, (_, f, d, m, e) => ParseMemo(f, d, m, e) },
            { DbfFieldType.Varchar, (_, f, d, _, e) => ParseCharacter(f, d, e) },
            { DbfFieldType.Varbinary, (_, f, d, m, _) => ParseMemo(f, d, m, System.Text.Encoding.Latin1) },
            { DbfFieldType.Blob, (_, f, d, m, _) => ParseMemo(f, d, m, System.Text.Encoding.Latin1) },
            { DbfFieldType.AutoIncrement, (_, f, d, _, _) => ParseInteger(f, d) },
        };
    }

    /// <summary>
    /// Converte i dati grezzi di un campo in un valore .NET.
    /// </summary>
    public object? Parse(DbfField field, ReadOnlySpan<byte> data, IMemoFile? memoFile, Encoding encoding)
    {
        try
        {
            if (Parsers.TryGetValue(field.Type, out var parser))
            {
                return parser(this, field, data, memoFile, encoding);
            }
            // Se il tipo non è nel dizionario, restituisce i dati grezzi
            return new InvalidValue(data, field);
        }
        catch (Exception)
        {
            return new InvalidValue(data, field);
        }
    }

    #region Private Parsing Methods

    private static string ParseCharacter(DbfField field, ReadOnlySpan<byte> data, Encoding encoding)
    {
        return encoding.GetString(TrimEnd(data));
    }

    private static DateTime? ParseDate(DbfField field, ReadOnlySpan<byte> data)
    {
        if (IsNullValue(data)) return null;
        var dateString = Encoding.ASCII.GetString(data);
        return DateTime.TryParseExact(dateString, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out var date) ? date : null;
    }

    private static float? ParseFloat(DbfField field, ReadOnlySpan<byte> data)
    {
        var text = GetNumericString(data);
        return float.TryParse(text, NumberStyles.Any, CultureInfo.InvariantCulture, out var result) ? result : null;
    }

    private static int? ParseInteger(DbfField field, ReadOnlySpan<byte> data)
    {
        return data.Length == 4 ? BitConverter.ToInt32(data) : null;
    }

    private static bool? ParseLogical(DbfField field, ReadOnlySpan<byte> data)
    {
        return data.Length > 0 ? (data[0] switch
        {
            (byte)'T' or (byte)'t' or (byte)'Y' or (byte)'y' => true,
            (byte)'F' or (byte)'f' or (byte)'N' or (byte)'n' => false,
            _ => null,
        }) : null;
    }

    private static object? ParseMemo(DbfField field, ReadOnlySpan<byte> data, IMemoFile? memoFile, Encoding encoding)
    {
        var memoIndex = ParseMemoIndex(data);
        if (memoIndex == 0 || memoFile == null) return null;
        
        var memoData = memoFile.GetMemo(memoIndex);
        if (memoData is TextMemo textMemo) return encoding.GetString(textMemo.Data.Span).TrimEnd('\0');
        if (memoData is BinaryMemo binaryMemo) return binaryMemo.Data.ToArray();
        return null;
    }

    private static decimal? ParseNumeric(DbfField field, ReadOnlySpan<byte> data)
    {
        var text = GetNumericString(data);
        if (string.IsNullOrEmpty(text)) return null;
        return decimal.TryParse(text, NumberStyles.Any, CultureInfo.InvariantCulture, out var result) ? result : null;
    }

    private static double? ParseDouble(DbfField field, ReadOnlySpan<byte> data)
    {
        return data.Length == 8 ? BitConverter.ToDouble(data) : null;
    }
    
    private static DateTime? ParseTimestamp(DbfField field, ReadOnlySpan<byte> data)
    {
        if (data.Length != 8 || IsNullValue(data)) return null;
        var julianDay = BitConverter.ToUInt32(data[..4]);
        var milliseconds = BitConverter.ToUInt32(data.Slice(4, 4));
        if (julianDay == 0) return null;
        try
        {
            // L'offset per la conversione da Giorno Giuliano a OADate di .NET
            return DateTime.FromOADate(julianDay - 1721425).AddMilliseconds(milliseconds);
        }
        catch { return null; }
    }

    private static decimal? ParseCurrency(DbfField field, ReadOnlySpan<byte> data)
    {
        return data.Length == 8 ? BitConverter.ToInt64(data) / 10000m : null;
    }

    #endregion

    #region Private Helper Methods
    private static ReadOnlySpan<byte> TrimEnd(ReadOnlySpan<byte> data)
    {
        var end = data.Length;
        while (end > 0 && (data[end - 1] == 0 || data[end - 1] == 32)) end--;
        return data[..end];
    }

    private static string GetNumericString(ReadOnlySpan<byte> data)
    {
        return Encoding.ASCII.GetString(TrimEnd(data)).Trim();
    }

    private static bool IsNullValue(ReadOnlySpan<byte> data)
    {
        foreach (var b in data) if (b != 0 && b != 32) return false;
        return true;
    }
    
    private static int ParseMemoIndex(ReadOnlySpan<byte> data)
    {
        // ponytail: VFP/FPT memo fields are 4-byte LE block numbers; dBase III uses ASCII in wider fields
        if (data.Length == 4)
        {
            return BitConverter.ToInt32(data);
        }

        var text = Encoding.ASCII.GetString(TrimEnd(data)).Trim();
        return int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var result) ? result : 0;
    }
    #endregion
}