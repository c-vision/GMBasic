/* FILE: native/gmb_db_c/src/DBF/DbfListFormatter.c */
/* Port 1:1 di modules/Database/DBF/DbfListFormatter.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfListFormatter
//
//using System.Globalization;
//using System.Text;
//
//namespace Basic.DBF;
//
///// <summary>
///// Formats DBF records for LIST output (xBase-style: record number + space-separated values).
///// </summary>
//public static class DbfListFormatter
//{
//    /// <summary>
//    /// Formats a single field value for LIST display.
//    /// </summary>
//    public static string FormatValue(object? value) =>
//        value switch
//        {
//            null => string.Empty,
//            bool b => b ? ".T." : ".F.",
//            DateTime dt => dt.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture),
//            IFormattable f => f.ToString(null, CultureInfo.InvariantCulture) ?? string.Empty,
//            _ => value.ToString()?.Trim() ?? string.Empty
//        };
//
//    /// <summary>
//    /// Formats one LIST line: padded record number followed by column values.
//    /// </summary>
//    public static string FormatLine(long recordNumber, IReadOnlyList<object?> values)
//    {
//        var sb = new StringBuilder();
//        sb.Append(recordNumber.ToString(CultureInfo.InvariantCulture).PadLeft(8));
//        foreach (var value in values)
//        {
//            sb.Append(' ');
//            sb.Append(FormatValue(value));
//        }
//
//        return sb.ToString();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public static class DbfListFormatter */
void DbfListFormatter_FormatValue(const char* valueUtf8, int isNull, int isBool, int boolVal,
                                  char* out, int cap)
{
    if (isNull) { out[0] = 0; return; }
    if (isBool) { snprintf(out, (size_t)cap, "%s", boolVal ? ".T." : ".F."); return; }
    snprintf(out, (size_t)cap, "%s", valueUtf8 ? valueUtf8 : "");
}

void DbfListFormatter_FormatLine(int64_t recordNumber, const char* const* values, int nValues,
                                 char* out, int cap)
{
    int n, i;
    n = snprintf(out, (size_t)cap, "%8lld", (long long)recordNumber);
    for (i = 0; i < nValues && n < cap - 1; i++)
        n += snprintf(out + n, (size_t)(cap - n), " %s", values[i] ? values[i] : "");
}

