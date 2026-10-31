/* FILE: native/gmb_db_c/src/ISAM/IsamKeyExpressionEvaluator.c */
/* Port 1:1 di modules/Database/ISAM/IsamKeyExpressionEvaluator.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamKeyExpressionEvaluator
//
//using System.Text.Json;
//using Basic.Databases;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Valutatore built-in per espressioni chiave indice ISAM (engine-side, senza GPM).
///// Supporta UPPER, JSON_VALUE, COMPOSITE; estendibile via <see cref="KeyExpressionEvaluatorCallback"/> esterno.
///// </summary>
//internal static class IsamKeyExpressionEvaluator
//{
//    private const char CompositeSeparator = '\x1F';
//
//    /// <summary>
//    /// Tenta di valutare un'espressione chiave built-in. Restituisce false se serve il callback GPM/UDF.
//    /// </summary>
//    public static bool TryEvaluateBuiltIn(
//        string expressionString,
//        IReadOnlyDictionary<string, object?> recordContext,
//        DataType targetType,
//        out object? value)
//    {
//        value = null;
//        var expr = expressionString.Trim();
//
//        if (TryEvaluateUpper(expr, recordContext, out value))
//        {
//            return true;
//        }
//
//        if (TryEvaluateJsonValue(expr, recordContext, out value))
//        {
//            return true;
//        }
//
//        if (TryEvaluateComposite(expr, recordContext, out value))
//        {
//            return true;
//        }
//
//        if (recordContext.TryGetValue(expr, out value))
//        {
//            return true;
//        }
//
//        return false;
//    }
//
//    /// <summary>
//    /// Catena built-in → callback utente (hook UDF / espressioni BASIC unsafe).
//    /// </summary>
//    public static KeyExpressionEvaluatorCallback Chain(KeyExpressionEvaluatorCallback? userCallback) =>
//        (expression, context, targetType) =>
//        {
//            if (TryEvaluateBuiltIn(expression, context, targetType, out var builtIn))
//            {
//                return builtIn;
//            }
//
//            if (userCallback != null)
//            {
//                return userCallback(expression, context, targetType);
//            }
//
//            throw new InvalidOperationException(
//                $"Espressione chiave ISAM non risolvibile: '{expression}'. Usare IsUnsafe=true e un KeyExpressionEvaluatorCallback GPM.");
//        };
//
//    private static bool TryEvaluateUpper(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
//    {
//        value = null;
//        if (!expr.StartsWith("UPPER(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
//        {
//            return false;
//        }
//
//        var inner = expr[6..^1].Trim();
//        if (!ctx.TryGetValue(inner, out var raw))
//        {
//            return false;
//        }
//
//        value = raw?.ToString()?.ToUpperInvariant() ?? string.Empty;
//        return true;
//    }
//
//    private static bool TryEvaluateJsonValue(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
//    {
//        value = null;
//        if (!expr.StartsWith("JSON_VALUE(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
//        {
//            return false;
//        }
//
//        var inner = expr[11..^1];
//        var comma = inner.IndexOf(',');
//        if (comma <= 0)
//        {
//            return false;
//        }
//
//        var fieldName = inner[..comma].Trim();
//        var pathToken = inner[(comma + 1)..].Trim().Trim('\'', '"');
//        if (!pathToken.StartsWith('$'))
//        {
//            pathToken = "$." + pathToken.TrimStart('.');
//        }
//
//        if (!ctx.TryGetValue(fieldName, out var jsonRaw) || jsonRaw is null)
//        {
//            value = string.Empty;
//            return true;
//        }
//
//        value = ExtractJsonPath(jsonRaw.ToString() ?? "{}", pathToken) ?? string.Empty;
//        return true;
//    }
//
//    private static bool TryEvaluateComposite(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
//    {
//        value = null;
//        if (!expr.StartsWith("COMPOSITE(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
//        {
//            return false;
//        }
//
//        var inner = expr[10..^1];
//        var parts = inner.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
//        if (parts.Length < 2)
//        {
//            return false;
//        }
//
//        var segments = new string[parts.Length];
//        for (var i = 0; i < parts.Length; i++)
//        {
//            if (!ctx.TryGetValue(parts[i], out var partVal))
//            {
//                segments[i] = string.Empty;
//            }
//            else
//            {
//                segments[i] = partVal?.ToString() ?? string.Empty;
//            }
//        }
//
//        value = string.Join(CompositeSeparator, segments);
//        return true;
//    }
//
//    internal static string? ExtractJsonPath(string json, string jsonPath)
//    {
//        if (string.IsNullOrWhiteSpace(json))
//        {
//            return null;
//        }
//
//        try
//        {
//            using var doc = JsonDocument.Parse(json);
//            var element = doc.RootElement;
//            var segments = jsonPath.TrimStart('$', '.').Split('.', StringSplitOptions.RemoveEmptyEntries);
//            foreach (var segment in segments)
//            {
//                if (element.ValueKind != JsonValueKind.Object ||
//                    !element.TryGetProperty(segment, out element))
//                {
//                    return null;
//                }
//            }
//
//            return element.ValueKind switch
//            {
//                JsonValueKind.String => element.GetString(),
//                JsonValueKind.Number => element.GetRawText(),
//                JsonValueKind.True => "true",
//                JsonValueKind.False => "false",
//                JsonValueKind.Null => null,
//                _ => element.GetRawText(),
//            };
//        }
//        catch (JsonException)
//        {
//            return null;
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static int ke_field_idx(const IsamSchema* s, const char* name)
{
    int i;
    if (!s || !name) return -1;
    for (i = 0; i < s->FieldCount; i++) {
        if (gmb_strcasecmp(s->Fields[i].Name, name) == 0) return i;
    }
    return -1;
}

static int ke_is_text(const IsamSchema* s, int i)
{
    return i >= 0 && strcmp(s->Fields[i].DataTypeCode, "StringType") == 0;
}

static void ke_field_text(const IsamFieldValue* v, int isText, char* out, int cap)
{
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!v) return;
    if (isText)
        snprintf(out, (size_t)cap, "%s", v->text ? v->text : (v->textOut ? v->textOut : ""));
    else
        snprintf(out, (size_t)cap, "%lld", (long long)v->i64);
}

static int ke_trim_copy(char* dst, int cap, const char* s, int n)
{
    while (n > 0 && (*s == ' ' || *s == '\t')) { s++; n--; }
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(dst, s, (size_t)n);
    dst[n] = 0;
    return n;
}

static int ke_json_path(const char* json, const char* path, char* out, int cap)
{
    char key[64], pat[80];
    const char *p, *q;
    int n;
    if (!out || cap <= 0) return 0;
    out[0] = 0;
    if (!json || !path) return 0;
    while (*path == '$' || *path == '.') path++;
    ke_trim_copy(key, (int)sizeof(key), path, (int)strlen(path));
    if (!key[0]) return 0;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        p++;
        q = p;
        while (*q && *q != '"') q++;
        n = (int)(q - p);
        if (n >= cap) n = cap - 1;
        memcpy(out, p, (size_t)n);
        out[n] = 0;
        return 1;
    }
    q = p;
    while (*q && *q != ',' && *q != '}' && *q != ' ' && *q != '\n') q++;
    n = (int)(q - p);
    if (n >= cap) n = cap - 1;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    return 1;
}

int IsamKeyExpressionEvaluator_TryEvaluateBuiltIn(
    const char* expression, const IsamSchema* schema, const IsamFieldValue* fields,
    char* outText, int textCap, int64_t* outI64, int* isText)
{
    char expr[256], inner[256];
    int n, fi, i, text;
    if (isText) *isText = 1;
    if (outI64) *outI64 = 0;
    if (outText && textCap > 0) outText[0] = 0;
    if (!expression || !schema || !fields) return 0;
    ke_trim_copy(expr, (int)sizeof(expr), expression, (int)strlen(expression));
    n = (int)strlen(expr);
    if (n >= 7 && gmb_strncasecmp(expr, "UPPER(", 6) == 0 && expr[n - 1] == ')') {
        ke_trim_copy(inner, (int)sizeof(inner), expr + 6, n - 7);
        fi = ke_field_idx(schema, inner);
        if (fi < 0) return 0;
        ke_field_text(&fields[fi], ke_is_text(schema, fi), outText ? outText : inner,
                      outText ? textCap : (int)sizeof(inner));
        if (outText) {
            for (i = 0; outText[i]; i++)
                outText[i] = (char)toupper((unsigned char)outText[i]);
        }
        if (isText) *isText = 1;
        return 1;
    }
    if (n >= 12 && gmb_strncasecmp(expr, "JSON_VALUE(", 11) == 0 && expr[n - 1] == ')') {
        char fieldName[64], path[64], json[512];
        const char* comma;
        ke_trim_copy(inner, (int)sizeof(inner), expr + 11, n - 12);
        comma = strchr(inner, ',');
        if (!comma) return 0;
        ke_trim_copy(fieldName, (int)sizeof(fieldName), inner, (int)(comma - inner));
        ke_trim_copy(path, (int)sizeof(path), comma + 1, (int)strlen(comma + 1));
        {
            int plen = (int)strlen(path);
            if (plen >= 2 && (path[0] == '\'' || path[0] == '"') && path[0] == path[plen - 1]) {
                memmove(path, path + 1, (size_t)(plen - 2));
                path[plen - 2] = 0;
            }
        }
        fi = ke_field_idx(schema, fieldName);
        if (fi < 0) return 0;
        ke_field_text(&fields[fi], 1, json, (int)sizeof(json));
        ke_json_path(json, path, outText ? outText : inner, outText ? textCap : (int)sizeof(inner));
        if (isText) *isText = 1;
        return 1;
    }
    {
        char names[IsamFilterPushdown_MaxEq][IsamFilterPushdown_NameCap];
        int nNames = 0;
        if (IsamFilterPushdown_TryGetKeyFieldNames(expr, names, IsamFilterPushdown_MaxEq, &nNames) && nNames >= 2) {
            int pos = 0;
            if (!outText || textCap <= 0) return 0;
            outText[0] = 0;
            for (i = 0; i < nNames; i++) {
                char piece[64];
                fi = ke_field_idx(schema, names[i]);
                if (i) {
                    if (pos < textCap - 1) outText[pos++] = '\x1F';
                }
                ke_field_text(fi >= 0 ? &fields[fi] : NULL, fi >= 0 && ke_is_text(schema, fi), piece, (int)sizeof(piece));
                n = (int)strlen(piece);
                if (pos + n >= textCap) n = textCap - 1 - pos;
                if (n > 0) { memcpy(outText + pos, piece, (size_t)n); pos += n; }
                outText[pos] = 0;
            }
            if (isText) *isText = 1;
            return 1;
        }
    }
    fi = ke_field_idx(schema, expr);
    if (fi < 0) return 0;
    text = ke_is_text(schema, fi);
    if (isText) *isText = text;
    if (text)
        ke_field_text(&fields[fi], 1, outText, textCap);
    else if (outI64)
        *outI64 = fields[fi].i64;
    return 1;
}
