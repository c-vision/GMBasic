/* FILE: native/gmb_vm_c/tests/expr_compile_selfcheck.c
 * Golden Fase 8: compile AST → snippet opcode (senza dispatcher VM). */
#include "gmb_vm_expr_compile.h"
#include "gmb_adapter.h"
#include "gmb_gpm.h"
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

static int nearly(double a, double b) { return fabs(a - b) < 1e-9; }

static gmb_task_t* make_task(gmb_chunk_t* chunk, const char* xname, double xval)
{
    gmb_task_t* t = gmb_task_create_unregistered(0);
    if (!t) return NULL;
    gmb_task_ensure_globals(t, 8);
    t->owns_globals = 1;
    memset(chunk, 0, sizeof(*chunk));
    if (xname) {
        chunk->global_names = (char**)calloc(1, sizeof(char*));
        chunk->global_names[0] = strdup(xname);
        chunk->global_name_count = 1;
        gmb_stack_entry_t e;
        memset(&e, 0, sizeof(e));
        e.type = 1;
        e.num = xval;
        e.basic_type = GMB_BT_NUMBER;
        gmb_task_store_global(t, 0, e);
    }
    return t;
}

static void free_task(gmb_task_t* t, gmb_chunk_t* chunk)
{
    if (chunk && chunk->global_names) {
        free(chunk->global_names[0]);
        free(chunk->global_names);
        memset(chunk, 0, sizeof(*chunk));
    }
    gmb_task_free(t);
}

static void expect_compiled_num(const char* name, const char* src, int as_stmt,
                                const char* xname, double xval, double want)
{
    gmb_chunk_t chunk;
    gmb_task_t* t = make_task(&chunk, xname, xval);
    if (!t) { fail_msg(name, "task"); return; }
    gmb_val_t v = gmb_val_nil();
    int rc = gmb_vm_expr_try_compiled(t, xname ? &chunk : NULL, src, as_stmt, &v);
    if (rc != 0) {
        char buf[128];
        snprintf(buf, sizeof buf, "try_compiled rc=%d (want 0)", rc);
        fail_msg(name, buf);
        free_task(t, &chunk);
        return;
    }
    if (!nearly(gmb_val_to_double(v), want)) {
        char buf[128];
        snprintf(buf, sizeof buf, "got %g want %g", gmb_val_to_double(v), want);
        fail_msg(name, buf);
    }
    free_task(t, &chunk);
}

static void expect_fallback(const char* name, const char* src)
{
    gmb_chunk_t chunk;
    gmb_task_t* t = make_task(&chunk, NULL, 0);
    if (!t) { fail_msg(name, "task"); return; }
    int rc = gmb_vm_expr_try_compiled(t, NULL, src, 0, NULL);
    if (rc != 1) {
        char buf[64];
        snprintf(buf, sizeof buf, "rc=%d want 1 (fallback)", rc);
        fail_msg(name, buf);
    }
    free_task(t, &chunk);
}

static void expect_assign(const char* name, const char* src, double start, double want)
{
    gmb_chunk_t chunk;
    gmb_task_t* t = make_task(&chunk, "X", start);
    if (!t) { fail_msg(name, "task"); return; }
    gmb_val_t v = gmb_val_nil();
    int rc = gmb_vm_expr_try_compiled(t, &chunk, src, 1, &v);
    if (rc != 0) { fail_msg(name, "compile/run"); free_task(t, &chunk); return; }
    gmb_stack_entry_t e = gmb_task_load_global(t, 0);
    if (!nearly(e.num, want)) {
        char buf[128];
        snprintf(buf, sizeof buf, "X=%g want %g", e.num, want);
        fail_msg(name, buf);
    }
    free_task(t, &chunk);
}

int main(void)
{
    expect_compiled_num("arith", "1+2*3", 0, NULL, 0, 7);
    expect_compiled_num("paren", "(1+2)*3", 0, NULL, 0, 9);
    expect_compiled_num("pow", "2^3", 0, NULL, 0, 8);
    expect_compiled_num("mod", "10%3", 0, NULL, 0, 1);
    expect_compiled_num("neg", "-4+1", 0, NULL, 0, -3);
    expect_compiled_num("eq", "1=1", 0, NULL, 0, 1);
    expect_compiled_num("and", "1 AND 2", 0, NULL, 0, 1);
    expect_compiled_num("and0", "0 AND 1", 0, NULL, 0, 0);
    expect_compiled_num("or", "0 OR 2", 0, NULL, 0, 1);
    expect_compiled_num("global", "X+1", 0, "X", 10, 11);
    expect_compiled_num("if", "IF 1 THEN 4 ELSE 5", 1, NULL, 0, 4);
    expect_assign("let", "X = X + 5", 10, 15);
    expect_fallback("call", "LEN(\"ab\")");
    expect_fallback("amp", "&(\"1+2\")");

    if (g_fail) {
        fprintf(stderr, "expr_compile_selfcheck FAILED\n");
        return 1;
    }
    printf("expr_compile_selfcheck OK\n");
    return 0;
}
