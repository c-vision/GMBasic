/* FILE: native/gmb_runtime_c/tests/expr_selfcheck.c
 * Golden del valutatore & / EVAL (sottoinsieme v1). */
#include "gmb_expr.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int g_fail;

static void fail_msg(const char* name, const char* detail)
{
    fprintf(stderr, "FAIL %s: %s\n", name, detail);
    g_fail = 1;
}

static int nearly(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

static void expect_num(const char* name, const char* src, double want)
{
    gmb_val_t v = gmb_val_nil();
    int rc = gmb_expr_eval(src, NULL, NULL, NULL, &v);
    if (rc != GMB_EXPR_OK) { fail_msg(name, "eval rc"); return; }
    if (!nearly(gmb_val_to_double(v), want)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "got %g want %g", gmb_val_to_double(v), want);
        fail_msg(name, buf);
    }
}

static void expect_str(const char* name, const char* src, const char* want)
{
    gmb_val_t v = gmb_val_nil();
    int rc = gmb_expr_eval(src, NULL, NULL, NULL, &v);
    if (rc != GMB_EXPR_OK) { fail_msg(name, "eval rc"); return; }
    if (v.kind != GMB_TYPE_STRING || !v.v_string.data || strcmp(v.v_string.data, want) != 0)
        fail_msg(name, v.kind == GMB_TYPE_STRING && v.v_string.data ? v.v_string.data : "not string");
}

static void expect_bool(const char* name, const char* src, int want)
{
    gmb_val_t v = gmb_val_nil();
    int rc = gmb_expr_eval(src, NULL, NULL, NULL, &v);
    if (rc != GMB_EXPR_OK) { fail_msg(name, "eval rc"); return; }
    int got = 0;
    switch (v.kind) {
        case GMB_TYPE_BOOL: got = v.v_bool ? 1 : 0; break;
        case GMB_TYPE_INT: got = v.v_int != 0; break;
        case GMB_TYPE_DOUBLE: got = v.v_double != 0.0; break;
        default: fail_msg(name, "not boolish"); return;
    }
    if (got != want) fail_msg(name, got ? "true" : "false");
}

static int host_double(void* env, const char* name, const gmb_val_t* args, int nargs, gmb_val_t* out)
{
    (void)env;
    if (!name || !out || strcasecmp(name, "DOUBLE") != 0 || nargs < 1) return -1;
    *out = gmb_make_val_double(2.0 * gmb_val_to_double(args[0]));
    return 0;
}

int main(void)
{
    expect_num("add", "1+2", 3);
    expect_num("muladd", "2*3+4", 10);
    expect_num("paren", "(1+2)*3", 9);
    expect_num("pow", "2^3", 8);
    expect_num("unary", "-5+2", -3);
    expect_str("concat", "\"A\"+\"B\"", "AB");
    expect_bool("eq_t", "1=1", 1);
    expect_bool("eq_f", "1=0", 0);
    expect_bool("ne", "1<>0", 1);
    expect_bool("and_f", "1 AND 0", 0);
    expect_bool("or_t", "1 OR 0", 1);
    expect_bool("not", "NOT 0", 1);
    expect_bool("dot_and", ".T. .AND. .T.", 1);
    expect_num("len", "LEN(\"abc\")", 3);
    expect_num("val", "VAL(\"12\")", 12);
    expect_num("abs", "ABS(-5)", 5);
    expect_num("sqr", "SQR(9)", 3);
    expect_num("int", "INT(3.7)", 3);
    expect_str("upper", "UPPER(\"ab\")", "AB");
    expect_str("single_quote", "'ab'", "ab");
    expect_bool("eqeq", "1==1", 1);
    expect_num("eval_nested", "EVAL(\"1+2\")", 3);
    expect_num("iif", "IIF(1, 10, 20)", 10);
    expect_num("if_fn", "IF(0, 10, 20)", 20);
    expect_num("iif_lazy", "IIF(1, 5, 1/0)", 5);
    expect_str("type_n", "TYPE(\"1+2\")", "N");
    expect_str("type_c", "TYPE(\"'ab'\")", "C");
    expect_str("type_l", "TYPE(\"1=1\")", "L");
    expect_str("type_u", "TYPE(\"nope\")", "U");
    expect_num("amp_nested", "&('1+2')", 3);
    expect_num("eval_if", "IF 1 THEN 3 ELSE 4", 3);

    gmb_dict_t env = gmb_dict_new();
    gmb_dict_set(&env, "X", gmb_make_val_double(10));
    gmb_val_t v = gmb_val_nil();
    if (gmb_expr_eval("X+5", &env, gmb_expr_dict_get, gmb_expr_dict_set, &v) != GMB_EXPR_OK
        || !nearly(gmb_val_to_double(v), 15))
        fail_msg("env_read", "X+5");
    if (gmb_expr_eval("X = X + 1", &env, gmb_expr_dict_get, gmb_expr_dict_set, &v) != GMB_EXPR_OK)
        fail_msg("env_assign_rc", "X = X + 1");
    gmb_val_t x = gmb_dict_get(env, "X");
    if (!nearly(gmb_val_to_double(x), 11))
        fail_msg("env_assign", "X not 11");

    if (gmb_expr_exec("LET Y = 7", &env, gmb_expr_dict_get, gmb_expr_dict_set) != GMB_EXPR_OK)
        fail_msg("let", "rc");
    gmb_val_t y = gmb_dict_get(env, "Y");
    if (!nearly(gmb_val_to_double(y), 7))
        fail_msg("let_val", "Y not 7");

    /* Filtro: `=` è uguaglianza, non LET. */
    gmb_val_t fv = gmb_val_nil();
    if (gmb_expr_eval_filter("X = 11", &env, gmb_expr_dict_get, gmb_expr_dict_set, &fv) != GMB_EXPR_OK
        || !nearly(gmb_val_to_double(fv), 1))
        fail_msg("filter_eq", "X = 11");
    gmb_val_t x2 = gmb_dict_get(env, "X");
    if (!nearly(gmb_val_to_double(x2), 11))
        fail_msg("filter_no_assign", "X mutated");
    gmb_dict_set(&env, "Name", gmb_make_val_string("alice"));
    if (gmb_expr_eval_filter("Name = 'alice'", &env, gmb_expr_dict_get, gmb_expr_dict_set, &fv) != GMB_EXPR_OK
        || !nearly(gmb_val_to_double(fv), 1))
        fail_msg("filter_sq", "Name = 'alice'");

    if (gmb_expr_eval("TYPE(\"X\")", &env, gmb_expr_dict_get, gmb_expr_dict_set, &v) != GMB_EXPR_OK
        || v.kind != GMB_TYPE_STRING || !v.v_string.data || strcmp(v.v_string.data, "N") != 0)
        fail_msg("type_var", "TYPE(X)");

    if (gmb_expr_exec("IF 1 THEN Z = 3 ELSE Z = 9", &env, gmb_expr_dict_get, gmb_expr_dict_set) != GMB_EXPR_OK)
        fail_msg("if_stmt_rc", "IF THEN");
    gmb_val_t z = gmb_dict_get(env, "Z");
    if (!nearly(gmb_val_to_double(z), 3))
        fail_msg("if_stmt", "Z not 3");

    if (gmb_expr_eval_ex("DOUBLE(21)", &env, gmb_expr_dict_get, gmb_expr_dict_set, host_double, &v) != GMB_EXPR_OK
        || !nearly(gmb_val_to_double(v), 42))
        fail_msg("user_call", "DOUBLE(21)");

    {
        char* cap = NULL;
        if (gmb_expr_exec_capture("PRINT 1; PRINT 2;", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &cap) != GMB_EXPR_OK
            || !cap || strcmp(cap, "12") != 0)
            fail_msg("print_semi", cap ? cap : "null");
        free(cap);
        cap = NULL;
        if (gmb_expr_exec_capture("FOR i = 1 TO 3\nPRINT i;\nNEXT", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &cap) != GMB_EXPR_OK
            || !cap || strcmp(cap, "123") != 0)
            fail_msg("for", cap ? cap : "null");
        free(cap);
        cap = NULL;
        if (gmb_expr_exec_capture("i = 0\nWHILE i < 3\ni = i + 1\nPRINT i;\nWEND", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &cap) != GMB_EXPR_OK
            || !cap || strcmp(cap, "123") != 0)
            fail_msg("while", cap ? cap : "null");
        free(cap);
        cap = NULL;
        if (gmb_expr_exec_capture("IF 1 THEN\nPRINT 7;\nELSE\nPRINT 9;\nENDIF", &env, gmb_expr_dict_get, gmb_expr_dict_set, NULL, &cap) != GMB_EXPR_OK
            || !cap || strcmp(cap, "7") != 0)
            fail_msg("if_block", cap ? cap : "null");
        free(cap);
    }

    if (g_fail) {
        fprintf(stderr, "gmb_expr_selfcheck FAILED\n");
        return 1;
    }
    puts("gmb_expr_selfcheck OK");
    return 0;
}
