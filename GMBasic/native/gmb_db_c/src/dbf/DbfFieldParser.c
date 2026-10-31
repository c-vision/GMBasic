/* FILE: native/gmb_db_c/src/DBF/DbfFieldParser.c */
/* Port 1:1 di modules/Database/DBF/DbfFieldParser.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfFieldParser.cs
//
//using System.Globalization;
//using System.Text;
//
//// ReSharper disable UnusedParameter.Local
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic.DBF;
//
///// <summary>
///// Implementazione di IFieldParser che supporta i tipi di campo DBF standard.
///// </summary>
//internal class FieldParser : IFieldParser
//{
//    private static readonly Dictionary<DbfFieldType, Func<FieldParser, DbfField, ReadOnlySpan<byte>, IMemoFile?, Encoding, object?>> Parsers;
//
//    static FieldParser()
//    {
//        Parsers = new Dictionary<DbfFieldType, Func<FieldParser, DbfField, ReadOnlySpan<byte>, IMemoFile?, Encoding, object?>>
//        {
//            { DbfFieldType.Character, (_, f, d, _, e) => ParseCharacter(f, d, e) },
//            { DbfFieldType.Date, (_, f, d, _, _) => ParseDate(f, d) },
//            { DbfFieldType.Float, (_, f, d, _, _) => ParseFloat(f, d) },
//            { DbfFieldType.Integer, (_, f, d, _, _) => ParseInteger(f, d) },
//            { DbfFieldType.Logical, (_, f, d, _, _) => ParseLogical(f, d) },
//            { DbfFieldType.Memo, (_, f, d, m, e) => ParseMemo(f, d, m, e) },
//            { DbfFieldType.Numeric, (_, f, d, _, _) => ParseNumeric(f, d) },
//            { DbfFieldType.Double, (_, f, d, _, _) => ParseDouble(f, d) },
//            { DbfFieldType.DateTime, (_, f, d, _, _) => ParseTimestamp(f, d) },
//            { DbfFieldType.Currency, (_, f, d, _, _) => ParseCurrency(f, d) },
//            { DbfFieldType.General, (_, f, d, m, e) => ParseMemo(f, d, m, e) },
//            { DbfFieldType.Varchar, (_, f, d, _, e) => ParseCharacter(f, d, e) },
//            { DbfFieldType.Varbinary, (_, f, d, m, _) => ParseMemo(f, d, m, System.Text.Encoding.Latin1) },
//            { DbfFieldType.Blob, (_, f, d, m, _) => ParseMemo(f, d, m, System.Text.Encoding.Latin1) },
//            { DbfFieldType.AutoIncrement, (_, f, d, _, _) => ParseInteger(f, d) },
//        };
//    }
//
//    /// <summary>
//    /// Converte i dati grezzi di un campo in un valore .NET.
//    /// </summary>
//    public object? Parse(DbfField field, ReadOnlySpan<byte> data, IMemoFile? memoFile, Encoding encoding)
//    {
//        try
//        {
//            if (Parsers.TryGetValue(field.Type, out var parser))
//            {
//                return parser(this, field, data, memoFile, encoding);
//            }
//            // Se il tipo non è nel dizionario, restituisce i dati grezzi
//            return new InvalidValue(data, field);
//        }
//        catch (Exception)
//        {
//            return new InvalidValue(data, field);
//        }
//    }
//
//    #region Private Parsing Methods
//
//    private static string ParseCharacter(DbfField field, ReadOnlySpan<byte> data, Encoding encoding)
//    {
//        return encoding.GetString(TrimEnd(data));
//    }
//
//    private static DateTime? ParseDate(DbfField field, ReadOnlySpan<byte> data)
//    {
//        if (IsNullValue(data)) return null;
//        var dateString = Encoding.ASCII.GetString(data);
//        return DateTime.TryParseExact(dateString, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out var date) ? date : null;
//    }
//
//    private static float? ParseFloat(DbfField field, ReadOnlySpan<byte> data)
//    {
//        var text = GetNumericString(data);
//        return float.TryParse(text, NumberStyles.Any, CultureInfo.InvariantCulture, out var result) ? result : null;
//    }
//
//    private static int? ParseInteger(DbfField field, ReadOnlySpan<byte> data)
//    {
//        return data.Length == 4 ? BitConverter.ToInt32(data) : null;
//    }
//
//    private static bool? ParseLogical(DbfField field, ReadOnlySpan<byte> data)
//    {
//        return data.Length > 0 ? (data[0] switch
//        {
//            (byte)'T' or (byte)'t' or (byte)'Y' or (byte)'y' => true,
//            (byte)'F' or (byte)'f' or (byte)'N' or (byte)'n' => false,
//            _ => null,
//        }) : null;
//    }
//
//    private static object? ParseMemo(DbfField field, ReadOnlySpan<byte> data, IMemoFile? memoFile, Encoding encoding)
//    {
//        var memoIndex = ParseMemoIndex(data);
//        if (memoIndex == 0 || memoFile == null) return null;
//        
//        var memoData = memoFile.GetMemo(memoIndex);
//        if (memoData is TextMemo textMemo) return encoding.GetString(textMemo.Data.Span).TrimEnd('\0');
//        if (memoData is BinaryMemo binaryMemo) return binaryMemo.Data.ToArray();
//        return null;
//    }
//
//    private static decimal? ParseNumeric(DbfField field, ReadOnlySpan<byte> data)
//    {
//        var text = GetNumericString(data);
//        if (string.IsNullOrEmpty(text)) return null;
//        return decimal.TryParse(text, NumberStyles.Any, CultureInfo.InvariantCulture, out var result) ? result : null;
//    }
//
//    private static double? ParseDouble(DbfField field, ReadOnlySpan<byte> data)
//    {
//        return data.Length == 8 ? BitConverter.ToDouble(data) : null;
//    }
//    
//    private static DateTime? ParseTimestamp(DbfField field, ReadOnlySpan<byte> data)
//    {
//        if (data.Length != 8 || IsNullValue(data)) return null;
//        var julianDay = BitConverter.ToUInt32(data[..4]);
//        var milliseconds = BitConverter.ToUInt32(data.Slice(4, 4));
//        if (julianDay == 0) return null;
//        try
//        {
//            // L'offset per la conversione da Giorno Giuliano a OADate di .NET
//            return DateTime.FromOADate(julianDay - 1721425).AddMilliseconds(milliseconds);
//        }
//        catch { return null; }
//    }
//
//    private static decimal? ParseCurrency(DbfField field, ReadOnlySpan<byte> data)
//    {
//        return data.Length == 8 ? BitConverter.ToInt64(data) / 10000m : null;
//    }
//
//    #endregion
//
//    #region Private Helper Methods
//    private static ReadOnlySpan<byte> TrimEnd(ReadOnlySpan<byte> data)
//    {
//        var end = data.Length;
//        while (end > 0 && (data[end - 1] == 0 || data[end - 1] == 32)) end--;
//        return data[..end];
//    }
//
//    private static string GetNumericString(ReadOnlySpan<byte> data)
//    {
//        return Encoding.ASCII.GetString(TrimEnd(data)).Trim();
//    }
//
//    private static bool IsNullValue(ReadOnlySpan<byte> data)
//    {
//        foreach (var b in data) if (b != 0 && b != 32) return false;
//        return true;
//    }
//    
//    private static int ParseMemoIndex(ReadOnlySpan<byte> data)
//    {
//        // ponytail: VFP/FPT memo fields are 4-byte LE block numbers; dBase III uses ASCII in wider fields
//        if (data.Length == 4)
//        {
//            return BitConverter.ToInt32(data);
//        }
//
//        var text = Encoding.ASCII.GetString(TrimEnd(data)).Trim();
//        return int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var result) ? result : 0;
//    }
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>

/* C#: FieldParser — raw field bytes → testo (ABI/REPLACE roundtrip). */

static int fp_is_null(const uint8_t* data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (data[i] != 0 && data[i] != 32) return 0;
    }
    return 1;
}

static int fp_trim_end(const uint8_t* data, int len)
{
    while (len > 0 && (data[len - 1] == 0 || data[len - 1] == 32)) len--;
    return len;
}

static int32_t fp_i32_le(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int64_t fp_i64_le(const uint8_t* p)
{
    int i;
    uint64_t u = 0;
    for (i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

static int fp_copy_out(char* out, int cap, const char* s)
{
    if (!out || cap <= 0) return GMB_DB_ARG;
    snprintf(out, (size_t)cap, "%s", s ? s : "");
    return GMB_DB_OK;
}

static int fp_parse_character(const uint8_t* data, int len, char* out, int cap)
{
    int n = fp_trim_end(data, len);
    if (!out || cap <= 0) return GMB_DB_ARG;
    if (n >= cap) n = cap - 1;
    memcpy(out, data, (size_t)n);
    out[n] = 0;
    return GMB_DB_OK;
}

static int fp_parse_numeric(const uint8_t* data, int len, char* out, int cap)
{
    int n = fp_trim_end(data, len);
    const uint8_t* p = data;
    int i;
    while (n > 0 && (*p == ' ' || *p == '\t')) {
        p++;
        n--;
    }
    if (!out || cap <= 0) return GMB_DB_ARG;
    if (n <= 0) {
        out[0] = 0;
        return GMB_DB_OK;
    }
    if (n >= cap) n = cap - 1;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    for (i = 0; out[i]; i++) {
        if (out[i] == ',') out[i] = '.';
    }
    return GMB_DB_OK;
}

static int fp_parse_date(const uint8_t* data, int len, char* out, int cap)
{
    if (fp_is_null(data, len)) return fp_copy_out(out, cap, "");
    return fp_parse_character(data, len < 8 ? len : 8, out, cap);
}

static int fp_parse_logical(const uint8_t* data, int len, char* out, int cap)
{
    if (len <= 0) return fp_copy_out(out, cap, "");
    switch (data[0]) {
    case 'T':
    case 't':
    case 'Y':
    case 'y':
        return fp_copy_out(out, cap, "T");
    case 'F':
    case 'f':
    case 'N':
    case 'n':
        return fp_copy_out(out, cap, "F");
    default:
        return fp_copy_out(out, cap, "");
    }
}

static int fp_parse_integer(const uint8_t* data, int len, char* out, int cap)
{
    char tmp[32];
    if (len != 4) return fp_copy_out(out, cap, "");
    snprintf(tmp, sizeof(tmp), "%d", (int)fp_i32_le(data));
    return fp_copy_out(out, cap, tmp);
}

static int fp_parse_double(const uint8_t* data, int len, char* out, int cap)
{
    double v;
    char tmp[64];
    if (len != 8) return fp_copy_out(out, cap, "");
    memcpy(&v, data, 8);
    snprintf(tmp, sizeof(tmp), "%.15g", v);
    return fp_copy_out(out, cap, tmp);
}

static int fp_parse_currency(const uint8_t* data, int len, char* out, int cap)
{
    double v;
    char tmp[64];
    int n;
    if (len != 8) return fp_copy_out(out, cap, "");
    v = (double)fp_i64_le(data) / 10000.0;
    snprintf(tmp, sizeof(tmp), "%.4f", v);
    /* Canone INVARIANTE con ZERI FINALI TOLTI (parità con `Convert.ToString(decimal,
     * InvariantCulture)` del C# — prima il C# usava la cultura corrente → virgola).
     * "12.3400" → "12.34", "10.0000" → "10", "-0.0000" → "0". */
    n = (int)strlen(tmp);
    while (n > 0 && tmp[n - 1] == '0') n--;
    if (n > 0 && tmp[n - 1] == '.') n--;
    if (n <= 0 || (n == 1 && tmp[0] == '-')) { tmp[0] = '0'; n = 1; }
    tmp[n] = 0;
    return fp_copy_out(out, cap, tmp);
}

static int32_t fp_civil_days(int y, int m, int d)
{
    y -= m <= 2;
    {
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
        unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097 + (int)doe - 719468;
    }
}

static void fp_civil_from_days(int z, int* y, int* m, int* d)
{
    z += 719468;
    {
        int era = (z >= 0 ? z : z - 146096) / 146097;
        unsigned doe = (unsigned)(z - era * 146097);
        unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
        int yy = (int)yoe + era * 400;
        unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
        unsigned mp = (5u * doy + 2u) / 153u;
        unsigned dd = doy - (153u * mp + 2u) / 5u + 1u;
        unsigned mm = mp + (mp < 10 ? 3u : -9u);
        yy += (mm <= 2);
        *y = yy;
        *m = (int)mm;
        *d = (int)dd;
    }
}

static int fp_parse_timestamp(const uint8_t* data, int len, char* out, int cap)
{
    uint32_t julian, milli;
    int oa, y, mo, d, hh, mi, ss;
    char tmp[32];
    if (len != 8 || fp_is_null(data, len)) return fp_copy_out(out, cap, "");
    julian = (uint32_t)fp_i32_le(data);
    milli = (uint32_t)fp_i32_le(data + 4);
    if (julian == 0) return fp_copy_out(out, cap, "");
    oa = (int)julian - 1721425;
    fp_civil_from_days(oa + fp_civil_days(1899, 12, 30), &y, &mo, &d);
    hh = (int)(milli / 3600000u);
    milli %= 3600000u;
    mi = (int)(milli / 60000u);
    milli %= 60000u;
    ss = (int)(milli / 1000u);
    snprintf(tmp, sizeof(tmp), "%04d%02d%02d%02d%02d%02d", y, mo, d, hh, mi, ss);
    return fp_copy_out(out, cap, tmp);
}

int DbfFieldParser_ParseMemoIndex(const uint8_t* data, int len)
{
    char tmp[32];
    int n, i;
    if (!data || len <= 0) return 0;
    if (len == 4) return (int)fp_i32_le(data);
    n = fp_trim_end(data, len);
    if (n <= 0 || n >= (int)sizeof(tmp)) return 0;
    memcpy(tmp, data, (size_t)n);
    tmp[n] = 0;
    for (i = 0; tmp[i] == ' '; i++) { }
    return (int)strtol(tmp + i, NULL, 10);
}

static int fp_parse_memo(const uint8_t* data, int len, MemoFile* memo, char* out, int cap)
{
    int idx;
    MemoData md;
    int n, rc;
    if (!out || cap <= 0) return GMB_DB_ARG;
    out[0] = 0;
    idx = DbfFieldParser_ParseMemoIndex(data, len);
    if (idx == 0 || !memo) return GMB_DB_OK;
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(memo, idx, &md) != GMB_DB_OK) return GMB_DB_OK;
    n = md.Length;
    while (n > 0 && md.Data && md.Data[n - 1] == 0) n--;
    if (n >= cap) n = cap - 1;
    if (n > 0 && md.Data) memcpy(out, md.Data, (size_t)n);
    out[n] = 0;
    MemoData_Dispose(&md);
    rc = GMB_DB_OK;
    return rc;
}

int DbfFieldParser_Parse(const DbfField* field, const uint8_t* data, int len, MemoFile* memo, char* out, int cap)
{
    uint8_t t;
    if (!field || !data || !out || cap <= 0) return GMB_DB_ARG;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
        return fp_parse_character(data, len, out, cap);
    case DbfFieldType_Date:
        return fp_parse_date(data, len, out, cap);
    case DbfFieldType_Float:
    case DbfFieldType_Numeric:
        return fp_parse_numeric(data, len, out, cap);
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        return fp_parse_integer(data, len, out, cap);
    case DbfFieldType_Logical:
        return fp_parse_logical(data, len, out, cap);
    case DbfFieldType_Memo:
    case DbfFieldType_General:
    case DbfFieldType_Varbinary:
    case DbfFieldType_Blob:
        return fp_parse_memo(data, len, memo, out, cap);
    case DbfFieldType_Double:
        return fp_parse_double(data, len, out, cap);
    case DbfFieldType_DateTime:
        return fp_parse_timestamp(data, len, out, cap);
    case DbfFieldType_Currency:
        return fp_parse_currency(data, len, out, cap);
    default:
        return fp_parse_character(data, len, out, cap);
    }
}

int DbfFieldParser_SelfCheck(void)
{
    uint8_t buf[16];
    char out[32];
    DbfField f;
    memset(&f, 0, sizeof(f));
    f.Type = (uint8_t)DbfFieldType_Character;
    f.Length = 10;
    memcpy(buf, "HI        ", 10);
    if (DbfFieldParser_Parse(&f, buf, 10, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "HI") != 0)
        return -1;
    f.Type = (uint8_t)DbfFieldType_Numeric;
    memcpy(buf, " 12.50", 6);
    if (DbfFieldParser_Parse(&f, buf, 6, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "12.50") != 0)
        return -2;
    f.Type = (uint8_t)DbfFieldType_Logical;
    buf[0] = 'Y';
    if (DbfFieldParser_Parse(&f, buf, 1, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "T") != 0)
        return -3;
    return GMB_DB_OK;
}
