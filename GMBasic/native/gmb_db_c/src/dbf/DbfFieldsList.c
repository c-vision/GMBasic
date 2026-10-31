/* FILE: native/gmb_db_c/src/DBF/DbfFieldsList.c */
/* Port 1:1 di modules/Database/DBF/DbfFieldsList.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfFieldsList
//
//namespace Basic.DBF;
//
///// <summary>
///// Parses comma-separated DBF field name lists (e.g. from Clipper <c>FIELDS &amp;macro</c> expansion).
///// </summary>
//public static class DbfFieldsList
//{
//    /// <summary>
//    /// Splits a comma-separated field list into trimmed, non-empty names.
//    /// </summary>
//    /// <param name="fieldList">Raw list such as <c>"Type, Shape"</c>.</param>
//    /// <returns>Individual field names in source order.</returns>
//    public static IReadOnlyList<string> SplitCommaSeparatedNames(string? fieldList)
//    {
//        if (string.IsNullOrWhiteSpace(fieldList))
//        {
//            return [];
//        }
//
//        var names = new List<string>();
//        var start = 0;
//        for (var i = 0; i <= fieldList.Length; i++)
//        {
//            if (i < fieldList.Length && fieldList[i] != ',')
//            {
//                continue;
//            }
//
//            var segment = fieldList[start..i].Trim();
//            if (segment.Length > 0)
//            {
//                names.Add(segment);
//            }
//
//            start = i + 1;
//        }
//
//        return names;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public static class DbfFieldsList */
/* Splits a comma-separated field list into trimmed, non-empty names. */
int DbfFieldsList_SplitCommaSeparatedNames(const char* fieldList, char names[][64], int maxNames)
{
    int n = 0, start = 0, i, len;
    if (!fieldList) return 0;
    len = (int)strlen(fieldList);
    for (i = 0; i <= len; i++)
    {
        if (i < len && fieldList[i] != ',') continue;
        {
            int a = start, b = i;
            while (a < b && (fieldList[a] == ' ' || fieldList[a] == '\t')) a++;
            while (b > a && (fieldList[b - 1] == ' ' || fieldList[b - 1] == '\t')) b--;
            if (b > a && n < maxNames)
            {
                int k = b - a;
                if (k > 63) k = 63;
                memcpy(names[n], fieldList + a, (size_t)k);
                names[n][k] = 0;
                n++;
            }
        }
        start = i + 1;
    }
    return n;
}

