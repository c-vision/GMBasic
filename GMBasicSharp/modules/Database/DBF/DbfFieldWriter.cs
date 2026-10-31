// FILE: Basic.DBF.DbfFieldWriter.cs

using System.Globalization;
using System.Text;

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

/// <summary>
/// Fornisce la logica per convertire valori .NET nel formato di byte grezzi
/// per la scrittura nei campi di un file DBF.
/// </summary>
internal static class DbfFieldWriter
{
    /// <summary>
    /// Scrive il valore formattato in uno span di destinazione.
    /// </summary>
    public static void Write(Span<byte> destination, DbfField field, object? value, Encoding encoding, IMemoFile? memoFile = null)
    {
        switch (field.Type)
        {
            case DbfFieldType.Character:
                WriteCharacter(destination, value, encoding);
                break;
            case DbfFieldType.Numeric:
                WriteNumeric(destination, value, field.Length, field.DecimalCount);
                break;
            case DbfFieldType.Date:
                WriteDate(destination, value);
                break;
            case DbfFieldType.Logical:
                WriteLogical(destination, value);
                break;
            case DbfFieldType.Float:
                WriteFloat(destination, value, field.DecimalCount);
                break;
            case DbfFieldType.Integer:
                WriteInteger(destination, value);
                break;
            case DbfFieldType.DateTime:
                WriteDateTime(destination, value);
                break;
            case DbfFieldType.Currency:
                WriteCurrency(destination, value);
                break;
            case DbfFieldType.Double:
                WriteDouble(destination, value);
                break;
            case DbfFieldType.Memo:
            case DbfFieldType.General:
                WriteMemo(destination, field, value, encoding, memoFile);
                break;
            default:
                destination.Clear();
                break;
        }
    }

    private static void WriteCharacter(Span<byte> destination, object? value, Encoding encoding)
    {
        destination.Fill((byte)' ');
        var strValue = value?.ToString() ?? string.Empty;
        var bytes = encoding.GetBytes(strValue);
        bytes.AsSpan(0, Math.Min(bytes.Length, destination.Length)).CopyTo(destination);
    }

    private static void WriteNumeric(Span<byte> destination, object? value, int length, int decimals)
    {
        destination.Fill((byte)' ');
        if (value == null) return;
        var numStr = Convert.ToDecimal(value).ToString($"F{decimals}", CultureInfo.InvariantCulture);
        var bytes = Encoding.ASCII.GetBytes(numStr);
        if (bytes.Length <= destination.Length)
            bytes.CopyTo(destination[^bytes.Length..]);
    }

    private static void WriteDate(Span<byte> destination, object? value)
    {
        var dateStr = "        ";
        if (value is DateTime dt) dateStr = dt.ToString("yyyyMMdd");
        Encoding.ASCII.GetBytes(dateStr).CopyTo(destination);
    }

    private static void WriteLogical(Span<byte> destination, object? value)
    {
        if (value is bool b) destination[0] = b ? (byte)'T' : (byte)'F';
        else destination[0] = (byte)'?';
    }
    
    private static void WriteFloat(Span<byte> destination, object? value, int decimals)
    {
        destination.Fill((byte)' ');
        if (value == null) return;
        var numStr = Convert.ToSingle(value).ToString($"F{decimals}", CultureInfo.InvariantCulture);
        var bytes = Encoding.ASCII.GetBytes(numStr);
        if (bytes.Length <= destination.Length)
            bytes.CopyTo(destination[^bytes.Length..]);
    }

    private static void WriteInteger(Span<byte> destination, object? value)
    {
        if (destination.Length == 4 && value != null)
            BitConverter.GetBytes(Convert.ToInt32(value)).CopyTo(destination);
        else
            destination.Clear();
    }

    private static void WriteDateTime(Span<byte> destination, object? value)
    {
        destination.Clear();
        if (value is DateTime dt && destination.Length >= 8)
        {
            var oaDate = dt.ToOADate();
            var julianDay = (uint)(oaDate + 1721425);
            var milliseconds = (uint)dt.TimeOfDay.TotalMilliseconds;
            
            BitConverter.GetBytes(julianDay).CopyTo(destination[..4]);
            BitConverter.GetBytes(milliseconds).CopyTo(destination.Slice(4, 4));
        }
    }

    private static void WriteCurrency(Span<byte> destination, object? value)
    {
        if (destination.Length == 8 && value != null)
        {
            var currencyValue = (long)(Convert.ToDecimal(value) * 10000);
            BitConverter.GetBytes(currencyValue).CopyTo(destination);
        }
        else
        {
            destination.Clear();
        }
    }

    private static void WriteDouble(Span<byte> destination, object? value)
    {
        if (destination.Length == 8 && value != null)
        {
            BitConverter.GetBytes(Convert.ToDouble(value)).CopyTo(destination);
        }
        else
        {
            destination.Clear();
        }
    }

    private static void WriteMemo(
        Span<byte> destination,
        DbfField field,
        object? value,
        Encoding encoding,
        IMemoFile? memoFile)
    {
        destination.Fill((byte)' ');
        if (value is null || memoFile is null or NullMemoFile)
        {
            return;
        }

        ReadOnlySpan<byte> payload = value switch
        {
            string s => encoding.GetBytes(s),
            byte[] bytes => bytes,
            _ => encoding.GetBytes(value.ToString() ?? string.Empty),
        };

        if (payload.IsEmpty)
        {
            return;
        }

        var blockIndex = memoFile.WriteMemo(payload, isText: field.Type != DbfFieldType.General);
        if (blockIndex <= 0)
        {
            return;
        }

        WriteMemoIndex(destination, blockIndex);
    }

    private static void WriteMemoIndex(Span<byte> destination, int blockIndex)
    {
        if (destination.Length == 4)
        {
            BitConverter.TryWriteBytes(destination, blockIndex);
            return;
        }

        var text = blockIndex.ToString(CultureInfo.InvariantCulture);
        var bytes = Encoding.ASCII.GetBytes(text);
        destination.Fill((byte)' ');
        bytes.AsSpan(0, Math.Min(bytes.Length, destination.Length))
            .CopyTo(destination[(destination.Length - bytes.Length)..]);
    }
    
    /// <summary>
    /// Restituisce il valore .NET "vuoto" (blank) standard per un dato tipo di campo DBF.
    /// Questi sono i valori che un record appena creato con APPEND BLANK dovrebbe contenere.
    /// </summary>
    /// <param name="fieldType">Il tipo di campo DBF.</param>
    /// <returns>Il valore di default corrispondente (es. stringa vuota, 0, false).</returns>
    public static object? GetBlankValue(DbfFieldType fieldType)
    {
        return fieldType switch
        {
            DbfFieldType.Character => string.Empty,
            DbfFieldType.Numeric => 0.0m,
            DbfFieldType.Float => 0.0f,
            DbfFieldType.Double => 0.0d,
            DbfFieldType.Integer => 0,
            DbfFieldType.Currency => 0.0m,
            DbfFieldType.Logical => false,
            DbfFieldType.Date => new DateTime(1, 1, 1), // Rappresenta una data vuota/nulla
            DbfFieldType.DateTime => new DateTime(1, 1, 1), // Rappresenta una data/ora vuota/nulla
            DbfFieldType.Memo => string.Empty,
            DbfFieldType.General => null, // I campi General non hanno un valore blank standard
            _ => null
        };
    }
    
}