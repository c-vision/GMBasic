/* FILE: native/gmb_runtime_c/tests/gmb_ffi_selfcheck.c
 * Vertical slice #1 — stringhe (\0 embedded, UTF-8, >64K) + UAF copy-out.
 * Verifica il contratto copy-in/copy-out di gmb_ffi.h:
 *   - copia-in : input validi SOLO durante la chiamata (niente puntatori conservati);
 *   - copia-out: gmb_ffi_make_string[_len] COPIA, quindi il ritorno resta valido
 *                anche se il modulo libera il proprio buffer (niente UAF lato host);
 *   - BYREF multi-out, errori -> gmb_ffi_last_error, liste gmb_list_t.
 */
#include "gmb_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void fail_msg(const char* name, const char* detail)
{
    fprintf(stderr, "FAIL %s: %s\n", name, detail);
    g_fail = 1;
}

/* ============================================================================
 * Modulo di test (come lo scriverebbe l'autore di un pacchetto gmb-native)
 * ============================================================================ */

/* Copia fedele (con \0 embedded) — usa _len per non troncare. */
static gmb_ffi_val_t fn_echo(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("echo: 1 arg"); return gmb_ffi_make_nil(); }
    const char* s; size_t n;
    s = gmb_ffi_get_string(&args[0], &n);
    return gmb_ffi_make_string_len(s, n);
}

/* Lunghezza in BYTE (non strlen) — valida \0 embedded. */
static gmb_ffi_val_t fn_buflen(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("buflen: 1 arg"); return gmb_ffi_make_nil(); }
    size_t n = 0;
    (void)gmb_ffi_get_string(&args[0], &n);
    return gmb_ffi_make_int((gmb_ffi_int_t)n);
}

/* Concatenazione di 2 stringhe (copy-in multi-arg). */
static gmb_ffi_val_t fn_concat(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 2) { gmb_ffi_error("concat: 2 args"); return gmb_ffi_make_nil(); }
    const char* a; size_t an;
    const char* b; size_t bn;
    a = gmb_ffi_get_string(&args[0], &an);
    b = gmb_ffi_get_string(&args[1], &bn);
    char* buf = (char*)malloc(an + bn + 1);
    if (buf == NULL) { gmb_ffi_error("concat: OOM"); return gmb_ffi_make_nil(); }
    memcpy(buf, a, an);
    memcpy(buf + an, b, bn);
    buf[an + bn] = '\0';
    gmb_ffi_val_t out = gmb_ffi_make_string_len(buf, an + bn);
    free(buf);
    return out;
}

/* Ritorna una stringa >64K (70000 byte) — stress copy-out. */
static gmb_ffi_val_t fn_big(const gmb_ffi_val_t* args, size_t argc)
{
    (void)args; (void)argc;
    const size_t N = 70000;
    char* buf = (char*)malloc(N + 1);
    if (buf == NULL) { gmb_ffi_error("big: OOM"); return gmb_ffi_make_nil(); }
    memset(buf, 'A', N);
    buf[N] = '\0';
    gmb_ffi_val_t out = gmb_ffi_make_string_len(buf, N);
    free(buf); /* il ritorno DEVE restare valido: copia-out */
    return out;
}

/* UAF: copia l'input in un buffer suo e lo LIBERA subito dopo la make_string_len.
 * Se il contratto non copiasse, il return sarebbe un puntatore dangling -> segfault. */
static gmb_ffi_val_t fn_free_after(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("free_after: 1 arg"); return gmb_ffi_make_nil(); }
    const char* s; size_t n;
    s = gmb_ffi_get_string(&args[0], &n);
    char* buf = (char*)malloc(n + 1);
    if (buf == NULL) { gmb_ffi_error("free_after: OOM"); return gmb_ffi_make_nil(); }
    memcpy(buf, s, n);
    buf[n] = '\0';
    gmb_ffi_val_t out = gmb_ffi_make_string_len(buf, n); /* COPIA */
    free(buf);                                            /* UAF se non copiasse */
    return out;
}

/* Multi-out BYREF: DECLARE SUB SumOut LIB "m" (x AS LONG, y AS LONG, BYREF sx AS LONG, BYREF sy AS DOUBLE)
 * Il runtime passa gli argomenti BYREF come gmb_ffi_val_t con v_ptr = puntatore alla variabile. */
static gmb_ffi_val_t fn_sum_out(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 4) { gmb_ffi_error("sum_out: 4 args"); return gmb_ffi_make_nil(); }
    gmb_ffi_int_t x = gmb_ffi_get_int(&args[0], 0);
    gmb_ffi_int_t y = gmb_ffi_get_int(&args[1], 0);
    gmb_ffi_set_int_out((gmb_ffi_int_t*)args[2].v_ptr, x + y);
    gmb_ffi_set_double_out((double*)args[3].v_ptr, (gmb_ffi_double_t)x + 0.5);
    return gmb_ffi_make_int(x + y);
}

/* Errore controllato -> gmb_ffi_last_error + nil. */
static gmb_ffi_val_t fn_err(const gmb_ffi_val_t* args, size_t argc)
{
    (void)args; (void)argc;
    gmb_ffi_error("simulated module failure");
    return gmb_ffi_make_nil();
}

/* Lista: somma gli elementi numerici (gmb_list_t copy-in). */
static gmb_ffi_val_t fn_list_sum(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("list_sum: 1 arg"); return gmb_ffi_make_nil(); }
    gmb_ffi_list_t l = gmb_ffi_get_list(&args[0]);
    gmb_ffi_double_t acc = 0.0;
    for (size_t i = 0; i < (size_t)l.count; i++)
        acc += gmb_val_to_double(l.items[i]);
    gmb_list_free(&l);
    return gmb_ffi_make_double(acc);
}

/* --- Test #4: handle opaco (LONG) + ciclo vita / leak check.
 *   InitEngine()->LONG, StepEngine(handle), DestroyEngine(handle) pattern. */
#define GMB_FFI_MAX_HANDLES 64
static void* g_handles[GMB_FFI_MAX_HANDLES];
static int g_handle_live;
static int g_handle_next = 1;

static int handle_alloc(double v)
{
    for (int i = 0; i < GMB_FFI_MAX_HANDLES; i++)
        if (g_handles[i] == NULL) {
            double* p = (double*)malloc(sizeof(double));
            if (p == NULL) return 0;
            *p = v;
            g_handles[i] = p;
            g_handle_live++;
            return i + 1; /* handle 1-based, id opaco */
        }
    return 0;
}
static double* handle_get(int id)
{
    if (id < 1 || id > GMB_FFI_MAX_HANDLES) return NULL;
    return (double*)g_handles[id - 1];
}
static void handle_free(int id)
{
    if (id < 1 || id > GMB_FFI_MAX_HANDLES) return;
    if (g_handles[id - 1] != NULL) { free(g_handles[id - 1]); g_handles[id - 1] = NULL; g_handle_live--; }
}

static gmb_ffi_val_t fn_handle_new(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("handle_new: 1 arg"); return gmb_ffi_make_nil(); }
    return gmb_ffi_make_int(handle_alloc(gmb_ffi_get_double(&args[0], 0.0)));
}
static gmb_ffi_val_t fn_handle_get(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("handle_get: 1 arg"); return gmb_ffi_make_nil(); }
    double* p = handle_get((int)gmb_ffi_get_int(&args[0], 0));
    return p != NULL ? gmb_ffi_make_double(*p) : gmb_ffi_make_nil();
}
static gmb_ffi_val_t fn_handle_free(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 1) { gmb_ffi_error("handle_free: 1 arg"); return gmb_ffi_make_nil(); }
    handle_free((int)gmb_ffi_get_int(&args[0], 0));
    return gmb_ffi_make_int(0);
}
static gmb_ffi_val_t fn_handle_live(const gmb_ffi_val_t* args, size_t argc)
{
    (void)args; (void)argc;
    return gmb_ffi_make_int(g_handle_live);
}

/* --- Test #2: callback (closure BASIC → C, convenzione cb(ctx, ...)).
 *   Map  : (closure, list) -> list di cb(ctx, elem)          [AMAP-style]
 *   Fold : (closure, list, init) -> cb(ctx, acc, elem)       [AREDUCE-style] */

static gmb_ffi_val_t fn_map(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 2) { gmb_ffi_error("map: 2 args"); return gmb_ffi_make_nil(); }
    gmb_ffi_closure_t cl = gmb_ffi_get_closure(&args[0]);
    gmb_ffi_list_t l = gmb_ffi_get_list(&args[1]);
    gmb_ffi_list_t out = gmb_list_new();
    if (cl.fn != NULL)
    {
        gmb_double_t (*cb)(void*, gmb_double_t) =
            (gmb_double_t(*)(void*, gmb_double_t))cl.fn;
        for (size_t i = 0; i < (size_t)l.count; i++)
            gmb_list_add(&out, gmb_make_val_double(cb(cl.ctx, gmb_val_to_double(l.items[i]))));
    }
    gmb_list_free(&l);
    gmb_ffi_val_t res = gmb_make_val_list(out);
    gmb_list_free(&out);
    return res;
}

static gmb_ffi_val_t fn_fold(const gmb_ffi_val_t* args, size_t argc)
{
    if (argc < 3) { gmb_ffi_error("fold: 3 args"); return gmb_ffi_make_nil(); }
    gmb_ffi_closure_t cl = gmb_ffi_get_closure(&args[0]);
    gmb_ffi_list_t l = gmb_ffi_get_list(&args[1]);
    gmb_double_t init = gmb_ffi_get_double(&args[2], 0.0);
    if (cl.fn != NULL)
    {
        gmb_double_t (*cb)(void*, gmb_double_t, gmb_double_t) =
            (gmb_double_t(*)(void*, gmb_double_t, gmb_double_t))cl.fn;
        for (size_t i = 0; i < (size_t)l.count; i++)
            init = cb(cl.ctx, init, gmb_val_to_double(l.items[i]));
    }
    gmb_list_free(&l);
    return gmb_ffi_make_double(init);
}

static const gmb_ffi_fn_descr_t g_fns[] = {
    { "Echo",        fn_echo,      1, 1, "S" },
    { "BufLen",      fn_buflen,    1, 1, "S" },
    { "Concat",      fn_concat,    2, 2, "S,S" },
    { "Big",         fn_big,       0, 0, ""  },
    { "FreeAfter",   fn_free_after,1, 1, "S" },
    { "SumOut",      fn_sum_out,   4, 4, "L,L,P,P" },
    { "Err",         fn_err,       0, 0, ""  },
    { "ListSum",     fn_list_sum,  1, 1, "A" },
    { "Map",         fn_map,       2, 2, "C,A" },
    { "Fold",        fn_fold,      3, 3, "C,A,D" },
    { "HandleNew",   fn_handle_new,  1, 1, "D" },
    { "HandleGet",   fn_handle_get,  1, 1, "L" },
    { "HandleFree",  fn_handle_free, 1, 1, "L" },
    { "HandleLive",  fn_handle_live, 0, 0, "" },
    { NULL, NULL, 0, 0, NULL }
};

static gmb_ffi_module_t g_mod = { "vec", GMB_FFI_VERSION, g_fns, NULL, NULL };
gmb_ffi_module_t* gmb_ffi_module_entry(void) { return &g_mod; }

/* ============================================================================
 * Helper di verifica
 * ============================================================================ */

static void expect_str_len(const char* name, gmb_ffi_val_t v, const char* want, size_t want_len)
{
    if (v.kind != GMB_TYPE_STRING) { fail_msg(name, "not string"); return; }
    if (v.v_string.length != want_len) {
        char buf[96];
        snprintf(buf, sizeof(buf), "len %zu want %zu", v.v_string.length, want_len);
        fail_msg(name, buf);
        return;
    }
    if (memcmp(v.v_string.data, want, want_len) != 0)
        fail_msg(name, "bytes differ");
}

/* Callback C che il "host" passerebbe al modulo (simula FUNCTION BASIC): x*2. */
static gmb_double_t cb_double(void* ctx, gmb_double_t x)
{
    (void)ctx;
    return x * 2.0;
}

/* Callback con CONTESTO: moltiplica per il fattore in ctx (simula cattura upvalue). */
static gmb_double_t cb_scale(void* ctx, gmb_double_t x)
{
    gmb_double_t factor = *(gmb_double_t*)ctx;
    return x * factor;
}

/* Callback per Fold (AREDUCE-style): acc + elem. */
static gmb_double_t cb_sum(void* ctx, gmb_double_t acc, gmb_double_t elem)
{
    (void)ctx;
    return acc + elem;
}

static int nearly_eq(double a, double b) { return (a - b) < 1e-9 && (b - a) < 1e-9; }

/* ============================================================================
 * main
 * ============================================================================ */

int main(void)
{
    /* 1. \0 embedded: "ab\0cd" (5 byte). */
    {
        gmb_ffi_val_t in = gmb_ffi_make_string_len("ab\0cd", 5);
        gmb_ffi_val_t out = fn_echo(&in, 1);
        expect_str_len("echo_nul", out, "ab\0cd", 5);
        gmb_ffi_val_t len = fn_buflen(&in, 1);
        if (gmb_ffi_get_int(&len, -1) != 5) fail_msg("buflen_nul", "expected 5 (strlen=2)");
    }

    /* 2. UTF-8 multibyte: "héllo→wörld". */
    {
        const char* utf = "héllo→wörld";
        size_t n = strlen(utf);
        gmb_ffi_val_t in = gmb_ffi_make_string(utf);
        gmb_ffi_val_t out = fn_echo(&in, 1);
        expect_str_len("echo_utf8", out, utf, n);
        gmb_ffi_val_t len = fn_buflen(&in, 1);
        if (gmb_ffi_get_int(&len, -1) != (gmb_ffi_int_t)n) fail_msg("buflen_utf8", "byte length mismatch");
    }

    /* 3. Concat 2 stringhe (copy-in multi-arg). */
    {
        gmb_ffi_val_t a = gmb_ffi_make_string("foo");
        gmb_ffi_val_t b = gmb_ffi_make_string_len("bar\0!", 5);
        gmb_ffi_val_t args[2] = { a, b };
        gmb_ffi_val_t out = fn_concat(args, 2);
        expect_str_len("concat", out, "foobar\0!", 8);
    }

    /* 4. >64K (70000 byte) copy-out, modulo libera dopo. */
    {
        gmb_ffi_val_t out = fn_big(NULL, 0);
        if (out.kind != GMB_TYPE_STRING) { fail_msg("big", "not string"); }
        else {
            if (out.v_string.length != 70000) fail_msg("big", "len != 70000");
            else if (out.v_string.data[0] != 'A' || out.v_string.data[69999] != 'A')
                fail_msg("big", "content");
        }
    }

    /* 5. UAF: return valido dopo free del buffer del modulo. */
    {
        gmb_ffi_val_t in = gmb_ffi_make_string("uaf-safe");
        gmb_ffi_val_t out = fn_free_after(&in, 1);
        expect_str_len("uaf", out, "uaf-safe", 8);
    }

    /* 6. Errore controllato -> last_error + nil. */
    {
        gmb_ffi_val_t out = fn_err(NULL, 0);
        if (out.kind != GMB_TYPE_NIL) fail_msg("err", "not nil");
        if (gmb_ffi_last_error() == NULL || strlen(gmb_ffi_last_error()) == 0)
            fail_msg("err", "last_error empty");
        if (strcmp(gmb_ffi_last_error(), "simulated module failure") != 0)
            fail_msg("err", "last_error mismatch");
    }

    /* 7. Multi-out BYREF: SumOut(2,3,&sx,&sy) -> sx=5, sy=2.5. */
    {
        gmb_ffi_int_t sx = 0;
        double sy = 0.0;
        gmb_ffi_val_t args[4];
        args[0] = gmb_ffi_make_int(2);
        args[1] = gmb_ffi_make_int(3);
        args[2].kind = GMB_TYPE_OBJECT; args[2].v_ptr = &sx;
        args[3].kind = GMB_TYPE_OBJECT; args[3].v_ptr = &sy;
        gmb_ffi_val_t out = fn_sum_out(args, 4);
        if (gmb_ffi_get_int(&out, -1) != 5) fail_msg("sum_out", "return != 5");
        if (sx != 5) fail_msg("sum_out", "sx != 5");
        if (sy != 2.5) fail_msg("sum_out", "sy != 2.5");
    }

    /* 8. Lista (gmb_list_t): somma [1.0, 2.5, 3.5] = 7.0. */
    {
        gmb_ffi_list_t l = gmb_list_new();
        gmb_list_add(&l, gmb_make_val_double(1.0));
        gmb_list_add(&l, gmb_make_val_double(2.5));
        gmb_list_add(&l, gmb_make_val_double(3.5));
        gmb_ffi_val_t v = gmb_make_val_list(l);
        gmb_ffi_val_t out = fn_list_sum(&v, 1);
        if (gmb_ffi_get_double(&out, -1.0) != 7.0) fail_msg("list_sum", "expected 7.0");
    }

    /* 9. Callback Map: cb_double su [1,2,3] -> [2,4,6]. */
    {
        gmb_ffi_closure_t cl = { (void*)cb_double, NULL };
        gmb_ffi_list_t l = gmb_list_new();
        gmb_list_add(&l, gmb_make_val_double(1.0));
        gmb_list_add(&l, gmb_make_val_double(2.0));
        gmb_list_add(&l, gmb_make_val_double(3.0));
        gmb_ffi_val_t vcl = gmb_ffi_make_closure(cl);
        gmb_ffi_val_t vlist = gmb_make_val_list(l);
        gmb_ffi_val_t args[2] = { vcl, vlist };
        gmb_ffi_val_t out = fn_map(args, 2);
        gmb_ffi_list_t r = gmb_ffi_get_list(&out);
        if (r.count != 3) fail_msg("map", "count != 3");
        else {
            if (!nearly_eq(gmb_val_to_double(r.items[0]), 2.0)) fail_msg("map", "r[0] != 2");
            if (!nearly_eq(gmb_val_to_double(r.items[1]), 4.0)) fail_msg("map", "r[1] != 4");
            if (!nearly_eq(gmb_val_to_double(r.items[2]), 6.0)) fail_msg("map", "r[2] != 6");
        }
        gmb_list_free(&r);
    }

    /* 10. Callback con CTX (cattura upvalue): factor=10 su [1,2] -> [10,20]. */
    {
        gmb_double_t factor = 10.0;
        gmb_ffi_closure_t cl = { (void*)cb_scale, &factor };
        gmb_ffi_list_t l = gmb_list_new();
        gmb_list_add(&l, gmb_make_val_double(1.0));
        gmb_list_add(&l, gmb_make_val_double(2.0));
        gmb_ffi_val_t vcl = gmb_ffi_make_closure(cl);
        gmb_ffi_val_t vlist = gmb_make_val_list(l);
        gmb_ffi_val_t args[2] = { vcl, vlist };
        gmb_ffi_val_t out = fn_map(args, 2);
        gmb_ffi_list_t r = gmb_ffi_get_list(&out);
        if (r.count != 2) fail_msg("map_ctx", "count != 2");
        else {
            if (!nearly_eq(gmb_val_to_double(r.items[0]), 10.0)) fail_msg("map_ctx", "r[0] != 10");
            if (!nearly_eq(gmb_val_to_double(r.items[1]), 20.0)) fail_msg("map_ctx", "r[1] != 20");
        }
        gmb_list_free(&r);
    }

    /* 11. Callback Fold (AREDUCE-style): cb_sum, init 10 + [1,2,3] = 16. */
    {
        gmb_ffi_closure_t cl = { (void*)cb_sum, NULL };
        gmb_ffi_list_t l = gmb_list_new();
        gmb_list_add(&l, gmb_make_val_double(1.0));
        gmb_list_add(&l, gmb_make_val_double(2.0));
        gmb_list_add(&l, gmb_make_val_double(3.0));
        gmb_ffi_val_t vcl = gmb_ffi_make_closure(cl);
        gmb_ffi_val_t vlist = gmb_make_val_list(l);
        gmb_ffi_val_t vinit = gmb_ffi_make_double(10.0);
        gmb_ffi_val_t args[3] = { vcl, vlist, vinit };
        gmb_ffi_val_t out = fn_fold(args, 3);
        if (!nearly_eq(gmb_ffi_get_double(&out, -1.0), 16.0)) fail_msg("fold", "expected 16");
    }

    /* 12-14. PACKED struct layout: BASIC `PACKED TYPE` emette [StructLayout(Pack=1)]
     * (IlProgramCompiler.cs:769) → il C deve usare `#pragma pack(1)` e passare la
     * struct come buffer BYREF grezzo (gmb_ffi.h NON serve struct nativo). */
    {
#pragma pack(push, 1)
        struct PackedPnt { int16_t x; int16_t y; double f; };
#pragma pack(pop)
        struct PackedPnt p;
        if (sizeof(struct PackedPnt) != 12) {
            char buf[80]; snprintf(buf, sizeof(buf), "sizeof %zu want 12", sizeof(struct PackedPnt));
            fail_msg("packed_sizeof", buf);
        }
        /* offsetof: x=0, y=2, f=4 (packed, niente padding) */
        {
            size_t oy = (size_t)((char*)&p.y - (char*)&p);
            size_t of = (size_t)((char*)&p.f - (char*)&p);
            if (oy != 2 || of != 4) { fail_msg("packed_offset", "offset y!=2 o f!=4"); }
        }
        /* roundtrip su buffer BYREF: scrive/legge i campi */
        p.x = 10; p.y = 20; p.f = 3.5;
        unsigned char buf[12];
        memcpy(buf, &p, sizeof(p));
        struct PackedPnt q;
        memcpy(&q, buf, sizeof(q));
        if (q.x != 10 || q.y != 20 || q.f != 3.5) fail_msg("packed_roundtrip", "roundtrip mismatch");
    }
    {
        /* Confronto con layout NATURALE: su arm64 x8 → padding a 16. */
        struct NatPnt { int16_t x; int16_t y; double f; };
        if (sizeof(struct NatPnt) != 16) {
            char buf[80]; snprintf(buf, sizeof(buf), "natural sizeof %zu want 16", sizeof(struct NatPnt));
            fail_msg("natural_sizeof", buf);
        }
    }

    /* 15-16. Handle opaco + leak check (InitEngine/Step/Destroy pattern). */
    {
        gmb_ffi_val_t a = gmb_ffi_make_double(10.0);
        gmb_ffi_val_t b = gmb_ffi_make_double(20.0);
        gmb_ffi_val_t c = gmb_ffi_make_double(30.0);
        gmb_ffi_val_t r1 = fn_handle_new(&a, 1);
        gmb_ffi_val_t r2 = fn_handle_new(&b, 1);
        gmb_ffi_val_t r3 = fn_handle_new(&c, 1);
        gmb_ffi_int_t h1 = gmb_ffi_get_int(&r1, 0);
        gmb_ffi_int_t h2 = gmb_ffi_get_int(&r2, 0);
        gmb_ffi_int_t h3 = gmb_ffi_get_int(&r3, 0);
        if (g_handle_live != 3) fail_msg("handle_live3", "live != 3");
        if (h1 == h2 || h1 == h3) fail_msg("handle_id", "handle collisioni");
        gmb_ffi_val_t got = fn_handle_get(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h2 }, 1);
        if (gmb_ffi_get_double(&got, -1.0) != 20.0) fail_msg("handle_get", "h2 != 20");
        fn_handle_free(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h1 }, 1);
        fn_handle_free(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h3 }, 1);
        if (g_handle_live != 1) fail_msg("handle_free2", "live != 1 dopo 2 free");
        /* il handle liberato non è più leggibile (nil) */
        gmb_ffi_val_t gone = fn_handle_get(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h1 }, 1);
        if (gone.kind != GMB_TYPE_NIL) fail_msg("handle_uaf", "handle liberato ancora leggibile");
        /* il sopravvissuto resta valido */
        got = fn_handle_get(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h2 }, 1);
        if (gmb_ffi_get_double(&got, -1.0) != 20.0) fail_msg("handle_survivor", "h2 non valido dopo free altri");
        fn_handle_free(&(gmb_ffi_val_t){ .kind=GMB_TYPE_INT, .v_int=h2 }, 1);
        if (g_handle_live != 0) fail_msg("handle_leak", "live != 0 a fine test (leak)");
    }

    /* 17. gmb_ffi_dl_* — dlopen/dlsym condiviso (VM/C e C99): apertura libm per nome,
     * risoluzione di `sin` e invocazione; preload per basename (match pacchetti). */
    {
        gmb_ffi_dl_preload("libm.dylib");
        void* h = gmb_ffi_dl_open("m");   /* libm: estensione .dylib o match preload */
        if (h == NULL)
        {
            h = gmb_ffi_dl_open("libm");
        }
        if (h == NULL) fail_msg("dl_open_libm", "impossibile aprire libm");
        else
        {
            void* p = gmb_ffi_dl_sym(h, "sin");
            if (p == NULL) fail_msg("dl_sym_sin", "simbolo sin non trovato in libm");
            else
            {
                double (*f)(double) = (double (*)(double))p;
                double v = f(0.5);
                if (v < 0.47 || v > 0.48) fail_msg("dl_call_sin", "sin(0.5) fuori range");
            }
        }
    }

    if (g_fail) { fprintf(stderr, "GMB_FFI_SELFCHECK: FAIL\n"); return 1; }
    printf("GMB_FFI_SELFCHECK: PASS (17 gruppi)\n");
    return 0;
}
