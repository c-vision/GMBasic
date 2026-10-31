/* FILE: native/gmb_db_c/src/DBF/DbfFieldWriter.c */
/* Port 1:1 di modules/Database/DBF/DbfFieldWriter.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfFieldWriter.cs
//
//using System.Globalization;
//using System.Text;
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic.DBF;
//
///// <summary>
///// Fornisce la logica per convertire valori .NET nel formato di byte grezzi
///// per la scrittura nei campi di un file DBF.
///// </summary>
//internal static class DbfFieldWriter
//{
//    /// <summary>
//    /// Scrive il valore formattato in uno span di destinazione.
//    /// </summary>
//    public static void Write(Span<byte> destination, DbfField field, object? value, Encoding encoding, IMemoFile? memoFile = null)
//    {
//        switch (field.Type)
//        {
//            case DbfFieldType.Character:
//                WriteCharacter(destination, value, encoding);
//                break;
//            case DbfFieldType.Numeric:
//                WriteNumeric(destination, value, field.Length, field.DecimalCount);
//                break;
//            case DbfFieldType.Date:
//                WriteDate(destination, value);
//                break;
//            case DbfFieldType.Logical:
//                WriteLogical(destination, value);
//                break;
//            case DbfFieldType.Float:
//                WriteFloat(destination, value, field.DecimalCount);
//                break;
//            case DbfFieldType.Integer:
//                WriteInteger(destination, value);
//                break;
//            case DbfFieldType.DateTime:
//                WriteDateTime(destination, value);
//                break;
//            case DbfFieldType.Currency:
//                WriteCurrency(destination, value);
//                break;
//            case DbfFieldType.Double:
//                WriteDouble(destination, value);
//                break;
//            case DbfFieldType.Memo:
//            case DbfFieldType.General:
//                WriteMemo(destination, field, value, encoding, memoFile);
//                break;
//            default:
//                destination.Clear();
//                break;
//        }
//    }
//
//    private static void WriteCharacter(Span<byte> destination, object? value, Encoding encoding)
//    {
//        destination.Fill((byte)' ');
//        var strValue = value?.ToString() ?? string.Empty;
//        var bytes = encoding.GetBytes(strValue);
//        bytes.AsSpan(0, Math.Min(bytes.Length, destination.Length)).CopyTo(destination);
//    }
//
//    private static void WriteNumeric(Span<byte> destination, object? value, int length, int decimals)
//    {
//        destination.Fill((byte)' ');
//        if (value == null) return;
//        var numStr = Convert.ToDecimal(value).ToString($"F{decimals}", CultureInfo.InvariantCulture);
//        var bytes = Encoding.ASCII.GetBytes(numStr);
//        if (bytes.Length <= destination.Length)
//            bytes.CopyTo(destination[^bytes.Length..]);
//    }
//
//    private static void WriteDate(Span<byte> destination, object? value)
//    {
//        var dateStr = "        ";
//        if (value is DateTime dt) dateStr = dt.ToString("yyyyMMdd");
//        Encoding.ASCII.GetBytes(dateStr).CopyTo(destination);
//    }
//
//    private static void WriteLogical(Span<byte> destination, object? value)
//    {
//        if (value is bool b) destination[0] = b ? (byte)'T' : (byte)'F';
//        else destination[0] = (byte)'?';
//    }
//    
//    private static void WriteFloat(Span<byte> destination, object? value, int decimals)
//    {
//        destination.Fill((byte)' ');
//        if (value == null) return;
//        var numStr = Convert.ToSingle(value).ToString($"F{decimals}", CultureInfo.InvariantCulture);
//        var bytes = Encoding.ASCII.GetBytes(numStr);
//        if (bytes.Length <= destination.Length)
//            bytes.CopyTo(destination[^bytes.Length..]);
//    }
//
//    private static void WriteInteger(Span<byte> destination, object? value)
//    {
//        if (destination.Length == 4 && value != null)
//            BitConverter.GetBytes(Convert.ToInt32(value)).CopyTo(destination);
//        else
//            destination.Clear();
//    }
//
//    private static void WriteDateTime(Span<byte> destination, object? value)
//    {
//        destination.Clear();
//        if (value is DateTime dt && destination.Length >= 8)
//        {
//            var oaDate = dt.ToOADate();
//            var julianDay = (uint)(oaDate + 1721425);
//            var milliseconds = (uint)dt.TimeOfDay.TotalMilliseconds;
//            
//            BitConverter.GetBytes(julianDay).CopyTo(destination[..4]);
//            BitConverter.GetBytes(milliseconds).CopyTo(destination.Slice(4, 4));
//        }
//    }
//
//    private static void WriteCurrency(Span<byte> destination, object? value)
//    {
//        if (destination.Length == 8 && value != null)
//        {
//            var currencyValue = (long)(Convert.ToDecimal(value) * 10000);
//            BitConverter.GetBytes(currencyValue).CopyTo(destination);
//        }
//        else
//        {
//            destination.Clear();
//        }
//    }
//
//    private static void WriteDouble(Span<byte> destination, object? value)
//    {
//        if (destination.Length == 8 && value != null)
//        {
//            BitConverter.GetBytes(Convert.ToDouble(value)).CopyTo(destination);
//        }
//        else
//        {
//            destination.Clear();
//        }
//    }
//
//    private static void WriteMemo(
//        Span<byte> destination,
//        DbfField field,
//        object? value,
//        Encoding encoding,
//        IMemoFile? memoFile)
//    {
//        destination.Fill((byte)' ');
//        if (value is null || memoFile is null or NullMemoFile)
//        {
//            return;
//        }
//
//        ReadOnlySpan<byte> payload = value switch
//        {
//            string s => encoding.GetBytes(s),
//            byte[] bytes => bytes,
//            _ => encoding.GetBytes(value.ToString() ?? string.Empty),
//        };
//
//        if (payload.IsEmpty)
//        {
//            return;
//        }
//
//        var blockIndex = memoFile.WriteMemo(payload, isText: field.Type != DbfFieldType.General);
//        if (blockIndex <= 0)
//        {
//            return;
//        }
//
//        WriteMemoIndex(destination, blockIndex);
//    }
//
//    private static void WriteMemoIndex(Span<byte> destination, int blockIndex)
//    {
//        if (destination.Length == 4)
//        {
//            BitConverter.TryWriteBytes(destination, blockIndex);
//            return;
//        }
//
//        var text = blockIndex.ToString(CultureInfo.InvariantCulture);
//        var bytes = Encoding.ASCII.GetBytes(text);
//        destination.Fill((byte)' ');
//        bytes.AsSpan(0, Math.Min(bytes.Length, destination.Length))
//            .CopyTo(destination[(destination.Length - bytes.Length)..]);
//    }
//    
//    /// <summary>
//    /// Restituisce il valore .NET "vuoto" (blank) standard per un dato tipo di campo DBF.
//    /// Questi sono i valori che un record appena creato con APPEND BLANK dovrebbe contenere.
//    /// </summary>
//    /// <param name="fieldType">Il tipo di campo DBF.</param>
//    /// <returns>Il valore di default corrispondente (es. stringa vuota, 0, false).</returns>
//    public static object? GetBlankValue(DbfFieldType fieldType)
//    {
//        return fieldType switch
//        {
//            DbfFieldType.Character => string.Empty,
//            DbfFieldType.Numeric => 0.0m,
//            DbfFieldType.Float => 0.0f,
//            DbfFieldType.Double => 0.0d,
//            DbfFieldType.Integer => 0,
//            DbfFieldType.Currency => 0.0m,
//            DbfFieldType.Logical => false,
//            DbfFieldType.Date => new DateTime(1, 1, 1), // Rappresenta una data vuota/nulla
//            DbfFieldType.DateTime => new DateTime(1, 1, 1), // Rappresenta una data/ora vuota/nulla
//            DbfFieldType.Memo => string.Empty,
//            DbfFieldType.General => null, // I campi General non hanno un valore blank standard
//            _ => null
//        };
//    }
//    
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <time.h>

/* C#: DbfFieldWriter — valore come stringa (REPLACE/APPEND). Byte-for-byte vs C# per C/N/D/L/F/I/T/Y/B/M. */

static void fw_fill_space(uint8_t* dest, int destLen)
{
    if (dest && destLen > 0) memset(dest, ' ', (size_t)destLen);
}

static void fw_fill_zero(uint8_t* dest, int destLen)
{
    if (dest && destLen > 0) memset(dest, 0, (size_t)destLen);
}

static void fw_put_i32_le(uint8_t* dest, int32_t v)
{
    dest[0] = (uint8_t)v;
    dest[1] = (uint8_t)((uint32_t)v >> 8);
    dest[2] = (uint8_t)((uint32_t)v >> 16);
    dest[3] = (uint8_t)((uint32_t)v >> 24);
}

static void fw_put_i64_le(uint8_t* dest, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++) dest[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static void fw_put_f64_le(uint8_t* dest, double v)
{
    memcpy(dest, &v, 8);
}

static void fw_right_ascii(uint8_t* dest, int destLen, const char* text)
{
    int n;
    fw_fill_space(dest, destLen);
    if (!text) return;
    n = (int)strlen(text);
    if (n > destLen) return; /* C#: se non entra, lascia spazi */
    memcpy(dest + (destLen - n), text, (size_t)n);
}

void DbfFieldWriter_WriteCharacter(uint8_t* dest, int destLen, const char* value)
{
    int n;
    fw_fill_space(dest, destLen);
    if (!value) value = "";
    n = (int)strlen(value);
    if (n > destLen) n = destLen;
    if (n > 0) memcpy(dest, value, (size_t)n);
}

void DbfFieldWriter_WriteNumeric(uint8_t* dest, int destLen, const char* value, int decimals)
{
    char num[64];
    double v;
    char* end = NULL;
    fw_fill_space(dest, destLen);
    if (!value || !*value) return;
    v = strtod(value, &end);
    if (end == value) return;
    snprintf(num, sizeof(num), "%.*f", decimals < 0 ? 0 : decimals, v);
    fw_right_ascii(dest, destLen, num);
}

void DbfFieldWriter_WriteDate(uint8_t* dest, int destLen, const char* value)
{
    char ymd[9];
    int n, i, digits = 0;
    if (destLen < 8) {
        fw_fill_space(dest, destLen);
        return;
    }
    memset(ymd, ' ', 8);
    ymd[8] = 0;
    if (value) {
        n = (int)strlen(value);
        for (i = 0; i < n && digits < 8; i++) {
            if (value[i] >= '0' && value[i] <= '9') ymd[digits++] = value[i];
        }
        if (digits != 8) memset(ymd, ' ', 8);
    }
    memcpy(dest, ymd, 8);
    if (destLen > 8) memset(dest + 8, ' ', (size_t)(destLen - 8));
}

void DbfFieldWriter_WriteLogical(uint8_t* dest, int destLen, const char* value)
{
    char c;
    if (destLen <= 0) return;
    if (!value || !*value) {
        dest[0] = '?';
    } else {
        /* Valore NUMERICO (convenzione VMBASIC: TRUE = -1, FALSE = 0) — es.
         * `REPLACE Attivo WITH TRUE` arriva come "-1". Prima si guardava solo il PRIMO
         * carattere: '-' non era riconosciuto → '?' (campo vuoto, poi letto 0). */
        char* endp = NULL;
        long num = strtol(value, &endp, 10);
        if (endp != value && *endp == 0) {
            dest[0] = num != 0 ? 'T' : 'F';
        } else {
            c = value[0];
            if (c == 'T' || c == 't' || c == 'Y' || c == 'y' || c == '1') dest[0] = 'T';
            else if (c == 'F' || c == 'f' || c == 'N' || c == 'n' || c == '0') dest[0] = 'F';
            else dest[0] = '?';
        }
    }
    if (destLen > 1) memset(dest + 1, ' ', (size_t)(destLen - 1));
}

void DbfFieldWriter_WriteFloat(uint8_t* dest, int destLen, const char* value, int decimals)
{
    DbfFieldWriter_WriteNumeric(dest, destLen, value, decimals);
}

void DbfFieldWriter_WriteInteger(uint8_t* dest, int destLen, const char* value)
{
    int32_t v;
    fw_fill_zero(dest, destLen);
    if (destLen != 4 || !value || !*value) return;
    v = (int32_t)strtol(value, NULL, 10);
    fw_put_i32_le(dest, v);
}

static int32_t fw_civil_days(int y, int m, int d)
{
    /* Howard Hinnant days_from_civil — OADate è giorni da 1899-12-30. */
    y -= m <= 2;
    {
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
        unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097 + (int)doe - 719468;
    }
}

static uint32_t fw_oa_julian(int y, int m, int d)
{
    int oa = fw_civil_days(y, m, d) - fw_civil_days(1899, 12, 30);
    return (uint32_t)(oa + 1721425);
}

void DbfFieldWriter_WriteDateTime(uint8_t* dest, int destLen, const char* value)
{
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0, ms = 0;
    uint32_t julian, milli;
    fw_fill_zero(dest, destLen);
    if (destLen < 8 || !value || !*value) return;
    if (sscanf(value, "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &hh, &mi, &ss) < 3
        && sscanf(value, "%4d%2d%2d%2d%2d%2d", &y, &mo, &d, &hh, &mi, &ss) < 3
        && sscanf(value, "%4d%2d%2d", &y, &mo, &d) < 3)
        return;
    julian = fw_oa_julian(y, mo, d);
    milli = (uint32_t)(((hh * 60 + mi) * 60 + ss) * 1000 + ms);
    fw_put_i32_le(dest, (int32_t)julian);
    fw_put_i32_le(dest + 4, (int32_t)milli);
}

void DbfFieldWriter_WriteCurrency(uint8_t* dest, int destLen, const char* value)
{
    double v;
    int64_t scaled;
    fw_fill_zero(dest, destLen);
    if (destLen != 8 || !value || !*value) return;
    v = strtod(value, NULL);
    scaled = (int64_t)(v * 10000.0);
    fw_put_i64_le(dest, scaled);
}

void DbfFieldWriter_WriteDouble(uint8_t* dest, int destLen, const char* value)
{
    double v;
    fw_fill_zero(dest, destLen);
    if (destLen != 8 || !value || !*value) return;
    v = strtod(value, NULL);
    fw_put_f64_le(dest, v);
}

void DbfFieldWriter_WriteMemoIndex(uint8_t* dest, int destLen, int blockIndex)
{
    char text[32];
    if (destLen == 4) {
        fw_put_i32_le(dest, blockIndex);
        return;
    }
    snprintf(text, sizeof(text), "%d", blockIndex);
    fw_fill_space(dest, destLen);
    fw_right_ascii(dest, destLen, text);
}

void DbfFieldWriter_WriteMemo(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile)
{
    int block;
    fw_fill_space(dest, destLen);
    if (!value || !*value || !memoFile || MemoFile_IsNull(memoFile)) return;
    block = MemoFile_WriteMemo(memoFile, (const uint8_t*)value, (int)strlen(value),
                               field && field->Type != (uint8_t)DbfFieldType_General);
    if (block <= 0) return;
    DbfFieldWriter_WriteMemoIndex(dest, destLen, block);
}

void DbfFieldWriter_Write(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile)
{
    uint8_t t;
    if (!dest || destLen <= 0 || !field) return;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
        DbfFieldWriter_WriteCharacter(dest, destLen, value);
        break;
    case DbfFieldType_Numeric:
        DbfFieldWriter_WriteNumeric(dest, destLen, value, field->DecimalCount);
        break;
    case DbfFieldType_Date:
        DbfFieldWriter_WriteDate(dest, destLen, value);
        break;
    case DbfFieldType_Logical:
        DbfFieldWriter_WriteLogical(dest, destLen, value);
        break;
    case DbfFieldType_Float:
        DbfFieldWriter_WriteFloat(dest, destLen, value, field->DecimalCount);
        break;
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        DbfFieldWriter_WriteInteger(dest, destLen, value);
        break;
    case DbfFieldType_DateTime:
        DbfFieldWriter_WriteDateTime(dest, destLen, value);
        break;
    case DbfFieldType_Currency:
        DbfFieldWriter_WriteCurrency(dest, destLen, value);
        break;
    case DbfFieldType_Double:
        DbfFieldWriter_WriteDouble(dest, destLen, value);
        break;
    case DbfFieldType_Memo:
    case DbfFieldType_General:
    case DbfFieldType_Varbinary:
    case DbfFieldType_Blob:
        DbfFieldWriter_WriteMemo(dest, destLen, field, value, memoFile);
        break;
    default:
        fw_fill_zero(dest, destLen);
        break;
    }
}

void DbfFieldWriter_WriteBlank(uint8_t* dest, int destLen, const DbfField* field, MemoFile* memoFile)
{
    uint8_t t;
    if (!dest || destLen <= 0 || !field) return;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
    case DbfFieldType_Memo:
        DbfFieldWriter_WriteCharacter(dest, destLen, "");
        break;
    case DbfFieldType_Numeric:
        DbfFieldWriter_WriteNumeric(dest, destLen, "0", field->DecimalCount);
        break;
    case DbfFieldType_Float:
        DbfFieldWriter_WriteFloat(dest, destLen, "0", field->DecimalCount);
        break;
    case DbfFieldType_Double:
        DbfFieldWriter_WriteDouble(dest, destLen, "0");
        break;
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        DbfFieldWriter_WriteInteger(dest, destLen, "0");
        break;
    case DbfFieldType_Currency:
        DbfFieldWriter_WriteCurrency(dest, destLen, "0");
        break;
    case DbfFieldType_Logical:
        DbfFieldWriter_WriteLogical(dest, destLen, "F");
        break;
    case DbfFieldType_Date:
        DbfFieldWriter_WriteDate(dest, destLen, "00010101");
        break;
    case DbfFieldType_DateTime:
        DbfFieldWriter_WriteDateTime(dest, destLen, "00010101");
        break;
    default:
        DbfFieldWriter_Write(dest, destLen, field, NULL, memoFile);
        break;
    }
}

int DbfFieldWriter_SelfCheck(void)
{
    uint8_t buf[16];
    DbfField f;
    memset(&f, 0, sizeof(f));
    f.Type = (uint8_t)DbfFieldType_Character;
    f.Length = 10;
    DbfFieldWriter_Write(buf, 10, &f, "HI", NULL);
    if (memcmp(buf, "HI        ", 10) != 0) return -1;
    f.Type = (uint8_t)DbfFieldType_Numeric;
    f.Length = 6;
    f.DecimalCount = 2;
    DbfFieldWriter_Write(buf, 6, &f, "12.5", NULL);
    if (memcmp(buf, " 12.50", 6) != 0) return -2;
    f.Type = (uint8_t)DbfFieldType_Date;
    f.Length = 8;
    DbfFieldWriter_Write(buf, 8, &f, "2024-01-15", NULL);
    if (memcmp(buf, "20240115", 8) != 0) return -3;
    f.Type = (uint8_t)DbfFieldType_Logical;
    f.Length = 1;
    DbfFieldWriter_Write(buf, 1, &f, "Y", NULL);
    if (buf[0] != 'T') return -4;
    f.Type = (uint8_t)DbfFieldType_Integer;
    f.Length = 4;
    DbfFieldWriter_Write(buf, 4, &f, "42", NULL);
    if (buf[0] != 42 || buf[1] != 0 || buf[2] != 0 || buf[3] != 0) return -5;
    return GMB_DB_OK;
}
