/* FILE: native/gmb_db_c/src/ISAM/IsamQueryPredicate.c */
/* Port 1:1 di modules/Database/ISAM/IsamQueryPredicate.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamQueryPredicate
//namespace Basic.ISAM;
//
///// <summary>
///// Parser MVP per predicati stringa IsamQuery (uguaglianza campo = letterale).
///// </summary>
//internal static class IsamQueryPredicate
//{
//    /// <summary>
//    /// Prova a interpretare <c>Field == value</c> o <c>Field = value</c>.
//    /// </summary>
//    public static bool TryParseEquality(string predicate, out string fieldName, out object? value)
//    {
//        fieldName = string.Empty;
//        value = null;
//        var trimmed = predicate.Trim();
//        foreach (var op in new[] { "==", "=" })
//        {
//            var idx = trimmed.IndexOf(op, StringComparison.Ordinal);
//            if (idx <= 0)
//            {
//                continue;
//            }
//
//            fieldName = trimmed[..idx].Trim();
//            if (string.IsNullOrEmpty(fieldName))
//            {
//                return false;
//            }
//
//            value = ParseLiteral(trimmed[(idx + op.Length)..].Trim());
//            return true;
//        }
//
//        return false;
//    }
//
//    /// <summary>
//    /// Espressione chiave indice MVP pushdown: identificatore semplice uguale al campo filtro.
//    /// </summary>
//    public static bool IsSimpleFieldKeyExpression(string keyExpression, string fieldName) =>
//        keyExpression.Trim().Equals(fieldName, StringComparison.OrdinalIgnoreCase);
//
//    private static object? ParseLiteral(string rhs)
//    {
//        if (rhs.Length >= 2 &&
//            ((rhs[0] == '"' && rhs[^1] == '"') || (rhs[0] == '\'' && rhs[^1] == '\'')))
//        {
//            return rhs[1..^1];
//        }
//
//        if (long.TryParse(rhs, out var number))
//        {
//            return number;
//        }
//
//        return rhs;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static void qp_trim_copy(char* dst, int cap, const char* s, int n)
{
    while (n > 0 && isspace((unsigned char)*s)) { s++; n--; }
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(dst, s, (size_t)n);
    dst[n] = 0;
}

static void qp_parse_literal(const char* rhs, char* value, int valueCap, int* isNum)
{
    int n;
    char* end = NULL;
    *isNum = 0;
    n = (int)strlen(rhs);
    if (n >= 2 && ((rhs[0] == '"' && rhs[n - 1] == '"') || (rhs[0] == '\'' && rhs[n - 1] == '\''))) {
        qp_trim_copy(value, valueCap, rhs + 1, n - 2);
        return;
    }
    strtoll(rhs, &end, 10);
    if (end && end != rhs && *end == 0) *isNum = 1;
    qp_trim_copy(value, valueCap, rhs, n);
}

int IsamQueryPredicate_TryParseEquality(const char* predicate, char* fieldName, int fieldCap,
                                        char* value, int valueCap, int* isNum)
{
    const char* ops[] = { "==", "=", NULL };
    char trimmed[256];
    int oi, n;
    const char* p;
    if (!predicate || !fieldName || !value) return 0;
    fieldName[0] = 0;
    value[0] = 0;
    if (isNum) *isNum = 0;
    qp_trim_copy(trimmed, (int)sizeof(trimmed), predicate, (int)strlen(predicate));
    for (oi = 0; ops[oi]; oi++) {
        p = strstr(trimmed, ops[oi]);
        if (!p || p <= trimmed) continue;
        qp_trim_copy(fieldName, fieldCap, trimmed, (int)(p - trimmed));
        if (!fieldName[0]) return 0;
        n = (int)strlen(ops[oi]);
        qp_parse_literal(p + n, value, valueCap, isNum ? isNum : &n);
        return 1;
    }
    return 0;
}

int IsamQueryPredicate_IsSimpleFieldKeyExpression(const char* keyExpression, const char* fieldName)
{
    char expr[64];
    if (!keyExpression || !fieldName) return 0;
    qp_trim_copy(expr, (int)sizeof(expr), keyExpression, (int)strlen(keyExpression));
    return gmb_strcasecmp(expr, fieldName) == 0;
}
