/* FILE: native/gmb_db_c/src/Databases/RuntimeExpressionEvaluator.c */
/* Port 1:1 di common/Databases/RuntimeExpressionEvaluator.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.PCodeVm.RuntimeExpressionEvaluator.cs
//// Valutatore runtime di espressioni di filtro per ISAM SETFILTER.
//// Supporta sintassi: field OP value [AND/OR field OP value ...]
//// Operatori: =, ==, >, <, >=, <=, <>, !=
//// Valori: numerici, stringa tra apici, TRUE/FALSE
//
//using System.Collections.Generic;
//using System.Globalization;
//
//namespace Basic.Databases.Rdd;
//
///// <summary>Valutatore runtime di espressioni filtro per ISAM SETFILTER.</summary>
//public static class RuntimeExpressionEvaluator
//{
//    /// <summary>Delegato per valutazione runtime di espressioni filtro.</summary>
//    public delegate bool ExpressionEvaluator(string? expr, System.Collections.Generic.IReadOnlyDictionary<string, object?>? ctx);
//
//    /// <summary>Crea un evaluator da una stringa di espressione filtro.</summary>
//    public static ExpressionEvaluator? CreateEvaluator(string? expression)
//    {
//        if (string.IsNullOrWhiteSpace(expression))
//            return null;
//
//        var parts = ParseExpression(expression);
//        if (parts == null) return null;
//
//        return (_, ctx) =>
//        {
//            if (ctx == null) return false;
//            try { return Evaluate(parts, ctx); }
//            catch { return false; }
//        };
//    }
//
//    private sealed class FilterNode
//    {
//        public string? Field;
//        public string? Op;
//        public object? Value;
//        public List<FilterNode>? Children; // per AND/OR
//        public bool IsOr;
//    }
//
//    private static FilterNode? ParseExpression(string expr)
//    {
//        // Split su OR (priorità più bassa)
//        var orParts = SplitRespectingQuotes(expr, " OR ");
//        if (orParts.Count > 1)
//        {
//            var node = new FilterNode { Children = new List<FilterNode>(), IsOr = true };
//            foreach (var p in orParts)
//            {
//                var child = ParseExpression(p);
//                if (child != null) node.Children.Add(child);
//            }
//            return node.Children.Count > 0 ? node : null;
//        }
//
//        // Split su AND
//        var andParts = SplitRespectingQuotes(expr, " AND ");
//        if (andParts.Count > 1)
//        {
//            var node = new FilterNode { Children = new List<FilterNode>(), IsOr = false };
//            foreach (var p in andParts)
//            {
//                var child = ParseCondition(p.Trim());
//                if (child != null) node.Children.Add(child);
//            }
//            return node.Children.Count > 0 ? node : null;
//        }
//
//        return ParseCondition(expr.Trim());
//    }
//
//    private static FilterNode? ParseCondition(string cond)
//    {
//        if (string.IsNullOrEmpty(cond)) return null;
//
//        // Cerca operatore
//        string[] ops = { ">=", "<=", "<>", "!=", "=", "==", ">", "<" };
//        foreach (var op in ops)
//        {
//            var idx = cond.IndexOf(op, StringComparison.Ordinal);
//            if (idx > 0)
//            {
//                var field = cond[..idx].Trim();
//                var valStr = cond[(idx + op.Length)..].Trim();
//                object? val = ParseValue(valStr);
//                return new FilterNode { Field = field, Op = op, Value = val };
//            }
//        }
//
//        // Nessun operatore: deve essere un'espressione booleana o un campo
//        return new FilterNode { Field = cond.Trim(), Op = "!=", Value = false };
//    }
//
//    private static object? ParseValue(string s)
//    {
//        if (s.StartsWith("'") && s.EndsWith("'")) return s[1..^1];
//        if (s.StartsWith("\"") && s.EndsWith("\"")) return s[1..^1];
//        if (s.Equals("TRUE", StringComparison.OrdinalIgnoreCase)) return true;
//        if (s.Equals("FALSE", StringComparison.OrdinalIgnoreCase)) return false;
//        if (s.Equals("NIL", StringComparison.OrdinalIgnoreCase)) return null;
//        if (double.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, out var d)) return d;
//        return s; // stringa senza apici
//    }
//
//    private static List<string> SplitRespectingQuotes(string expr, string separator)
//    {
//        var result = new List<string>();
//        var inSingle = false;
//        var inDouble = false;
//        var start = 0;
//        var i = 0;
//
//        while (i <= expr.Length - separator.Length)
//        {
//            var c = expr[i];
//            if (c == '\'' && !inDouble) inSingle = !inSingle;
//            if (c == '"' && !inSingle) inDouble = !inDouble;
//
//            if (!inSingle && !inDouble && expr.AsSpan(i, separator.Length).SequenceEqual(separator))
//            {
//                result.Add(expr[start..i].Trim());
//                start = i + separator.Length;
//                i = start;
//                continue;
//            }
//            i++;
//        }
//        result.Add(expr[start..].Trim());
//        return result;
//    }
//
//    private static bool Evaluate(FilterNode node, IReadOnlyDictionary<string, object?> ctx)
//    {
//        if (node.Children != null)
//        {
//            foreach (var child in node.Children)
//            {
//                bool childResult = Evaluate(child, ctx);
//                if (node.IsOr && childResult) return true;
//                if (!node.IsOr && !childResult) return false;
//            }
//            return !node.IsOr; // AND: all true → true; OR: none true → false
//        }
//
//        if (node.Field == null) return false;
//        if (!ctx.TryGetValue(node.Field, out var fieldVal)) return false;
//
//        var cmp = CompareValues(fieldVal, node.Value);
//        return node.Op switch
//        {
//            "=" or "==" => cmp == 0,
//            ">" => cmp > 0,
//            "<" => cmp < 0,
//            ">=" => cmp >= 0,
//            "<=" => cmp <= 0,
//            "<>" or "!=" => cmp != 0,
//            _ => false
//        };
//    }
//
//    private static int CompareValues(object? a, object? b)
//    {
//        if (a == null && b == null) return 0;
//        if (a == null) return -1;
//        if (b == null) return 1;
//
//        if (a is string sa && b is string sb) return string.Compare(sa, sb, StringComparison.OrdinalIgnoreCase);
//        if (a is bool ba && b is bool bb) return ba.CompareTo(bb);
//
//        try
//        {
//            double da = Convert.ToDouble(a, CultureInfo.InvariantCulture);
//            double db = Convert.ToDouble(b, CultureInfo.InvariantCulture);
//            return da.CompareTo(db);
//        }
//        catch
//        {
//            return string.Compare(a.ToString(), b.ToString(), StringComparison.OrdinalIgnoreCase);
//        }
//    }
//}

/* --- C: SET FILTER / ISAM / SPEED — stesso gmb_expr dei macro `&` / EVAL. --- */

#include "gmb_eval_scope.h"

typedef struct {
    RtFilterGetField get;
    void* ctx;
} RtExprEnv;

static int rt_text_to_val(const char* s, gmb_val_t* out)
{
    char* end = NULL;
    double d;
    *out = gmb_val_nil();
    if (!s || !*s) return 0;
    /* Booleani: forme corte (T/F/Y/N) E parole intere TRUE/FALSE/YES/NO (il testo di una
     * condizione lambda `rec.Attivo = TRUE` emette "TRUE", non "T"). Valore NUMERICO -1/0
     * (convenzione VMBASIC) così che il confronto con un campo BIT letto come "-1"/"0" matchi
     * (con bool(1) `Attivo = TRUE` non trovava mai — §5 filtro Qualified). */
    if (((s[0] == 'T' || s[0] == 't' || s[0] == 'Y' || s[0] == 'y') && s[1] == 0)
        || strcasecmp(s, "TRUE") == 0 || strcasecmp(s, "YES") == 0) {
        *out = gmb_make_val_int(-1);
        return 0;
    }
    if (((s[0] == 'F' || s[0] == 'f' || s[0] == 'N' || s[0] == 'n') && s[1] == 0)
        || strcasecmp(s, "FALSE") == 0 || strcasecmp(s, "NO") == 0) {
        *out = gmb_make_val_int(0);
        return 0;
    }
    d = strtod(s, &end);
    if (end && end != s && *end == 0) {
        gmb_int_t i = (gmb_int_t)d;
        if ((double)i == d) *out = gmb_make_val_int(i);
        else *out = gmb_make_val_double(d);
        return 0;
    }
    *out = gmb_make_val_string(s);
    return 0;
}

static int rt_expr_get(void* env, const char* name, gmb_val_t* out)
{
    RtExprEnv* e = (RtExprEnv*)env;
    char buf[512];
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!e || !e->get || !name) return 0;
    if (e->get(e->ctx, name, buf, (int)sizeof(buf)) != GMB_DB_OK)
        return 0;
    return rt_text_to_val(buf, out);
}

static int rt_val_truth(gmb_val_t v)
{
    switch (v.kind) {
        case GMB_TYPE_NIL: return 0;
        case GMB_TYPE_BOOL: return v.v_bool ? 1 : 0;
        case GMB_TYPE_INT: return v.v_int != 0;
        case GMB_TYPE_DOUBLE: return v.v_double != 0.0;
        case GMB_TYPE_STRING:
            return v.v_string.data && v.v_string.data[0] != 0;
        default: return 0;
    }
}

int RuntimeExpressionEvaluator_Eval(const char* expression, RtFilterGetField get, void* ctx)
{
    RtExprEnv env;
    gmb_val_t v = gmb_val_nil();
    env.get = get;
    env.ctx = ctx;
    /* Motore compilatore+VM (in-process o libgmb_eval on-demand); fallback al
     * valutatore runtime finché la gamba senza motore non è completa. */
    int rc = gmb_eval_filter_env(expression, rt_expr_get, NULL, &env, NULL,
                                 NULL, NULL, NULL, NULL, &v);
    if (rc != 0)
        return 0;
    return rt_val_truth(v);
}

/* ---- Parità C# (DbExprPCodeEvaluator → PCodeVM): campo TIPIZZATO + funzioni utente. ---- */

typedef struct {
    RtFilterGetFieldVal get;
    gmb_expr_call_fn call;
    void* ctx;
} RtExprValEnv;

/* Passa direttamente il gmb_val_t prodotto dal callback motore: il tipo del campo
 * (INT/DOUBLE/BOOL/STRING) è preservato, come il Dictionary<string,object?> di C#. */
static int rt_get_val(void* env, const char* name, gmb_val_t* out)
{
    RtExprValEnv* e = (RtExprValEnv*)env;
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!e || !e->get || !name) return 0;
    return e->get(e->ctx, name, out);
}

int RuntimeExpressionEvaluator_EvalVal(const char* expression, RtFilterGetFieldVal get, void* ctx,
                                       gmb_expr_call_fn call)
{
    RtExprValEnv env;
    gmb_val_t v = gmb_val_nil();
    env.get = get;
    env.call = call;
    env.ctx = ctx;
    int rc = gmb_eval_filter_env(expression, rt_get_val, NULL, &env, call,
                                 NULL, NULL, NULL, NULL, &v);
    if (rc != 0)
        return 0;
    return rt_val_truth(v);
}

int RuntimeExpressionEvaluator_SelfCheck(void)
{
    return GMB_DB_OK;
}

