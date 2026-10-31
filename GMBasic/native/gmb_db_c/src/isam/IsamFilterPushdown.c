/* FILE: native/gmb_db_c/src/ISAM/IsamFilterPushdown.c */
/* Port 1:1 di modules/Database/ISAM/IsamFilterPushdown.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamFilterPushdown
//
//using System.Text;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Rileva filtri SET FILTER con uguaglianze su campi indicizzati (seek pushdown).
///// ponytail: AND di <c>field = literal</c> su prefisso chiave indice attivo (SET ORDER);
///// OR, funzioni, campi non in chiave restano su scan sequenziale.
///// </summary>
//internal static class IsamFilterPushdown
//{
//    /// <summary>
//    /// Prova a estrarre <c>field = literal</c> da un AST filtro.
//    /// </summary>
//    public static bool TryParseEquality(Expr? expression, out string fieldName, out object? value)
//    {
//        fieldName = string.Empty;
//        value = null;
//        if (expression is not BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.ExactlyEqual } binary)
//        {
//            return false;
//        }
//
//        if (binary.Left is not QualifiedNameExpr { Parts.Length: 1 } qualified)
//        {
//            return false;
//        }
//
//        fieldName = qualified.Parts[0].Lexeme;
//        if (string.IsNullOrEmpty(fieldName))
//        {
//            return false;
//        }
//
//        switch (binary.Right)
//        {
//            case NumberExpr number:
//                value = number.Value == Math.Truncate(number.Value)
//                    ? (object)(long)number.Value
//                    : number.Value;
//                return true;
//            case StringExpr str:
//                value = str.Value;
//                return true;
//            default:
//                return false;
//        }
//    }
//
//    /// <summary>
//    /// Raccoglie uguaglianze da <c>field = literal</c> o AND di tali clausole.
//    /// </summary>
//    public static bool TryCollectEqualities(Expr? expression, Dictionary<string, object?> equalities)
//    {
//        equalities.Clear();
//        if (expression is null)
//        {
//            return false;
//        }
//
//        return CollectEqualitiesRecursive(expression, equalities) && equalities.Count > 0;
//    }
//
//    /// <summary>
//    /// Campi record referenziati dall'espressione chiave indice (semplice, UPPER, COMPOSITE).
//    /// </summary>
//    public static bool TryGetKeyFieldNames(string keyExpression, out string[] fieldNames)
//    {
//        fieldNames = [];
//        var expr = keyExpression.Trim();
//
//        if (expr.StartsWith("COMPOSITE(", StringComparison.OrdinalIgnoreCase) && expr.EndsWith(')'))
//        {
//            var inner = expr[10..^1];
//            fieldNames = inner.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
//            return fieldNames.Length >= 2;
//        }
//
//        if (expr.StartsWith("UPPER(", StringComparison.OrdinalIgnoreCase) && expr.EndsWith(')'))
//        {
//            var inner = expr[6..^1].Trim();
//            if (string.IsNullOrEmpty(inner))
//            {
//                return false;
//            }
//
//            fieldNames = [inner];
//            return true;
//        }
//
//        if (string.IsNullOrEmpty(expr))
//        {
//            return false;
//        }
//
//        fieldNames = [expr];
//        return true;
//    }
//
//    /// <summary>
//    /// Quanti campi iniziali della chiave sono coperti da uguaglianze nel filtro (prefisso).
//    /// </summary>
//    public static int CountMatchingPrefixFields(
//        string[] keyFieldNames,
//        IReadOnlyDictionary<string, object?> equalities)
//    {
//        var count = 0;
//        foreach (var field in keyFieldNames)
//        {
//            if (!equalities.ContainsKey(field))
//            {
//                break;
//            }
//
//            count++;
//        }
//
//        return count;
//    }
//
//    /// <summary>
//    /// Lunghezza byte (UTF-8 upper) da confrontare per pushdown prefisso; -1 = chiave intera serializzata.
//    /// </summary>
//    public static int ComputeMatchLength(object? keyValue, bool isFullKeyMatch)
//    {
//        if (isFullKeyMatch)
//        {
//            return -1;
//        }
//
//        return Encoding.UTF8.GetByteCount((keyValue?.ToString() ?? string.Empty).ToUpperInvariant());
//    }
//
//    private static bool CollectEqualitiesRecursive(Expr expression, Dictionary<string, object?> equalities)
//    {
//        if (TryParseEquality(expression, out var fieldName, out var value))
//        {
//            equalities[fieldName] = value;
//            return true;
//        }
//
//        if (expression is BinaryExpr { Op.Kind: TokenKind.And } andExpr)
//        {
//            return CollectEqualitiesRecursive(andExpr.Left, equalities)
//                   && CollectEqualitiesRecursive(andExpr.Right, equalities);
//        }
//
//        return false;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <ctype.h>
#include <string.h>

static void fp_trim_copy(char* dst, int cap, const char* s, int n)
{
    while (n > 0 && isspace((unsigned char)*s)) {
        s++;
        n--;
    }
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(dst, s, (size_t)n);
    dst[n] = 0;
}

static int fp_in_token(const char* s, const char* at, int tokLen)
{
    if (at > s && !isspace((unsigned char)at[-1])) return 0;
    return at[tokLen] == 0 || isspace((unsigned char)at[tokLen]);
}

static int fp_has_or(const char* expr)
{
    int inS = 0, inD = 0;
    const char* s = expr;
    if (!s) return 0;
    for (; *s; s++) {
        if (*s == '\'' && !inD) inS = !inS;
        else if (*s == '"' && !inS) inD = !inD;
        else if (!inS && !inD && (s[0] == 'O' || s[0] == 'o') && (s[1] == 'R' || s[1] == 'r')
                 && fp_in_token(expr, s, 2))
            return 1;
    }
    return 0;
}

static int fp_split_and(const char* expr, char parts[][IsamFilterPushdown_ValCap], int maxParts)
{
    int n = 0, start = 0, i = 0, inS = 0, inD = 0, elen;
    if (!expr) return 0;
    elen = (int)strlen(expr);
    while (i <= elen - 5) {
        char c = expr[i];
        if (c == '\'' && !inD) inS = !inS;
        else if (c == '"' && !inS) inD = !inD;
        else if (!inS && !inD && (expr[i] == 'A' || expr[i] == 'a')
                 && (expr[i + 1] == 'N' || expr[i + 1] == 'n')
                 && (expr[i + 2] == 'D' || expr[i + 2] == 'd')
                 && fp_in_token(expr, expr + i, 3)) {
            if (n < maxParts)
                fp_trim_copy(parts[n++], IsamFilterPushdown_ValCap, expr + start, i - start);
            i += 3;
            while (expr[i] && isspace((unsigned char)expr[i])) i++;
            start = i;
            continue;
        }
        i++;
    }
    if (n < maxParts) fp_trim_copy(parts[n++], IsamFilterPushdown_ValCap, expr + start, elen - start);
    return n;
}

int IsamFilterPushdown_TryParseEquality(const char* expression, char* fieldName, int fieldCap,
                                        char* value, int valueCap)
{
    static const char* ops[] = { ">=", "<=", "<>", "!=", "==", "=", ">", "<", NULL };
    const char* op = NULL;
    const char* p;
    int oi, oplen = 0, n;
    char raw[IsamFilterPushdown_ValCap];
    if (!expression || !fieldName || !value) return 0;
    fieldName[0] = 0;
    value[0] = 0;
    for (oi = 0; ops[oi]; oi++) {
        p = strstr(expression, ops[oi]);
        if (p && p > expression) {
            op = ops[oi];
            oplen = (int)strlen(op);
            break;
        }
    }
    if (!op || (strcmp(op, "=") != 0 && strcmp(op, "==") != 0)) return 0;
    fp_trim_copy(fieldName, fieldCap, expression, (int)(strstr(expression, op) - expression));
    if (!fieldName[0]) return 0;
    fp_trim_copy(raw, (int)sizeof(raw), strstr(expression, op) + oplen,
                 (int)strlen(strstr(expression, op) + oplen));
    n = (int)strlen(raw);
    if (n >= 2 && ((raw[0] == '\'' && raw[n - 1] == '\'') || (raw[0] == '"' && raw[n - 1] == '"')))
        fp_trim_copy(value, valueCap, raw + 1, n - 2);
    else
        fp_trim_copy(value, valueCap, raw, n);
    return fieldName[0] != 0;
}

int IsamFilterPushdown_TryCollectEqualities(const char* expression, IsamFilterEquality* eqs, int maxEq,
                                            int* count)
{
    char parts[IsamFilterPushdown_MaxEq][IsamFilterPushdown_ValCap];
    int n, i;
    if (count) *count = 0;
    if (!expression || !eqs || maxEq <= 0) return 0;
    if (!*expression || fp_has_or(expression)) return 0;
    n = fp_split_and(expression, parts, IsamFilterPushdown_MaxEq);
    if (n > maxEq) n = maxEq;
    for (i = 0; i < n; i++) {
        if (!parts[i][0]) continue;
        if (!IsamFilterPushdown_TryParseEquality(parts[i], eqs[i].Name, (int)sizeof(eqs[i].Name),
                                                 eqs[i].Value, (int)sizeof(eqs[i].Value)))
            return 0;
    }
    if (count) *count = n;
    return n > 0;
}

int IsamFilterPushdown_TryGetKeyFieldNames(const char* keyExpression, char names[][IsamFilterPushdown_NameCap],
                                           int maxNames, int* count)
{
    char expr[256], inner[256];
    int n, i, start;
    if (count) *count = 0;
    if (!keyExpression || !names || maxNames <= 0) return 0;
    fp_trim_copy(expr, (int)sizeof(expr), keyExpression, (int)strlen(keyExpression));
    n = (int)strlen(expr);
    if (n >= 11 && gmb_strncasecmp(expr, "COMPOSITE(", 10) == 0 && expr[n - 1] == ')') {
        int nFields = 0;
        fp_trim_copy(inner, (int)sizeof(inner), expr + 10, n - 11);
        start = 0;
        for (i = 0; ; i++) {
            if (inner[i] == ',' || inner[i] == 0) {
                if (nFields < maxNames)
                    fp_trim_copy(names[nFields], IsamFilterPushdown_NameCap, inner + start, i - start);
                if (names[nFields][0]) nFields++;
                start = i + 1;
                if (!inner[i]) break;
            }
        }
        if (count) *count = nFields;
        return nFields >= 2;
    }
    if (n >= 7 && gmb_strncasecmp(expr, "UPPER(", 6) == 0 && expr[n - 1] == ')') {
        fp_trim_copy(names[0], IsamFilterPushdown_NameCap, expr + 6, n - 7);
        if (!names[0][0]) return 0;
        if (count) *count = 1;
        return 1;
    }
    if (!expr[0]) return 0;
    fp_trim_copy(names[0], IsamFilterPushdown_NameCap, expr, n);
    if (count) *count = 1;
    return 1;
}

int IsamFilterPushdown_CountMatchingPrefixFields(const char names[][IsamFilterPushdown_NameCap], int nNames,
                                                 const IsamFilterEquality* eqs, int nEq)
{
    int count = 0, i, j, found;
    for (i = 0; i < nNames; i++) {
        found = 0;
        for (j = 0; j < nEq; j++) {
            if (gmb_strcasecmp(names[i], eqs[j].Name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) break;
        count++;
    }
    return count;
}

int IsamFilterPushdown_ComputeMatchLength(const char* keyValue, int isFullKeyMatch)
{
    int n = 0;
    if (isFullKeyMatch) return -1;
    if (!keyValue) return 0;
    while (keyValue[n]) n++;
    return n;
}

int IsamFilterPushdown_FindEquality(const IsamFilterEquality* eqs, int nEq, const char* fieldName)
{
    int i;
    if (!eqs || !fieldName) return -1;
    for (i = 0; i < nEq; i++) {
        if (gmb_strcasecmp(eqs[i].Name, fieldName) == 0) return i;
    }
    return -1;
}
