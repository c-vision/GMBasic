#include "gmb_expr.h"

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
    if ((s[0] == 'T' || s[0] == 't' || s[0] == 'Y' || s[0] == 'y') && s[1] == 0) {
        *out = gmb_make_val_bool(1);
        return 0;
    }
    if ((s[0] == 'F' || s[0] == 'f' || s[0] == 'N' || s[0] == 'n') && s[1] == 0) {
        *out = gmb_make_val_bool(0);
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
    if (gmb_expr_eval_filter(expression, &env, rt_expr_get, NULL, &v) != GMB_EXPR_OK)
        return 0;
    return rt_val_truth(v);
}

int RuntimeExpressionEvaluator_SelfCheck(void)
{
    return GMB_DB_OK;
}
