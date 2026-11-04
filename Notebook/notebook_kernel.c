/* Notebook/notebook_kernel.c — kernel di una sessione notebook (C).
 *
 * Esecuzione di una cella: P-CODE. Il frammento BASIC è compilato dal COMPILATORE C
 * (gmb_compile_snippet, dentro libgmb_eval) in chunk A1/GMBC ed eseguito dalla VM/C
 * (`gmb_vm_eval_text_standalone`): stessa pipeline di `gmb`/REPL/VM, nessun valutatore
 * di espressioni separato e nessun `gmb_expr`. Lo stato della sessione sono le variabili
 * del dict, lette/scritte dal frammento via `nb_env_get`/`nb_env_set` (BYREF write-through):
 * le celle successive vedono i valori precedenti.
 */
#include "notebook_kernel.h"

#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

/* gmb_vm_eval.h (native/gmb_vm_c): firma identica. Il notebook linka SOLO la dylib del
 * valutatore p-code (libgmb_eval = compilatore C + VM/C), non l'intera VM, quindi non si
 * include quel header (tira dentro gmb_gpm.h/gmb_chunk.h); le due funzioni usate sono
 * dichiarate qui e sono quelle esportate dalla dylib (gmb_vm_eval.c). */
int gmb_vm_eval_text_standalone(const char* src, int as_stmt,
                                gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                                void* env, gmb_eval_call_fn call,
                                gmb_eval_print_str_fn pstr, gmb_eval_print_num_fn pnum,
                                gmb_eval_print_nl_fn pnl, void* pctx, gmb_val_t* out);
const char* gmb_vm_eval_last_error(void);

#define NB_MAX_CELLS 256

struct nb_kernel {
    gmb_dict_t env;
    int cells;                         /* celle eseguite (contatore storico) */
    nb_cell_t list[NB_MAX_CELLS];      /* celle per id (ordine di prima esecuzione) */
    int nlist;
};

/* ── JSON ────────────────────────────────────────────────────────────────── */

typedef struct {
    char* p;
    size_t cap;
    size_t len;
} jbuf_t;

static void jb_init(jbuf_t* b, size_t cap)
{
    b->p = (char*)malloc(cap ? cap : 256);
    b->cap = cap ? cap : 256;
    b->len = 0;
    if (b->p) b->p[0] = 0;
}

static void jb_putn(jbuf_t* b, const char* s, size_t n)
{
    if (!b->p) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2 + n + 1;
        char* np = (char*)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

static void jb_puts(jbuf_t* b, const char* s) { jb_putn(b, s, strlen(s)); }

static void jb_escape(jbuf_t* b, const char* s)
{
    if (!s) return;
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch (*p) {
            case '"':  jb_puts(b, "\\\""); break;
            case '\\': jb_puts(b, "\\\\"); break;
            case '\n': jb_puts(b, "\\n"); break;
            case '\r': jb_puts(b, "\\r"); break;
            case '\t': jb_puts(b, "\\t"); break;
            default:
                if (*p < 0x20) {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)*p);
                    jb_puts(b, tmp);
                } else {
                    jb_putn(b, (const char*)p, 1);
                }
        }
    }
}

/* Valore BASIC → JSON. Numeri/bool/nil nativi; tutto il resto come stringa
 * (stessa resa del PRINT, via gmb_val_to_string). */
static void val_to_json(gmb_val_t v, jbuf_t* b)
{
    char tmp[64];
    switch (v.kind) {
        case GMB_TYPE_NIL:   jb_puts(b, "null"); break;
        case GMB_TYPE_BOOL:  jb_puts(b, v.v_bool ? "true" : "false"); break;
        case GMB_TYPE_INT:
            snprintf(tmp, sizeof(tmp), "%lld", (long long)v.v_int);
            jb_puts(b, tmp);
            break;
        case GMB_TYPE_DOUBLE:
            snprintf(tmp, sizeof(tmp), "%.15g", (double)v.v_double);
            jb_puts(b, tmp);
            break;
        default: {
            gmb_string_t s = gmb_val_to_string(v);
            jb_puts(b, "\"");
            jb_escape(b, s.data ? s.data : "");
            jb_puts(b, "\"");
            break;
        }
    }
}

/* ── valore dell'espressione ─────────────────────────────────────────────── */

/* Una cella è "espressione" se è una sola riga e non inizia con una keyword di
 * statement. Serve solo a decidere se restituire anche `value`. */
static int looks_like_expression(const char* src)
{
    if (!src) return 0;
    while (*src && isspace((unsigned char)*src)) src++;
    if (!*src) return 0;
    for (const char* p = src; *p; p++)
        if (*p == '\n' || *p == '\r') return 0;

    static const char* kw[] = {
        "PRINT", "?", "LET", "FOR", "NEXT", "WHILE", "WEND", "DO", "LOOP", "IF",
        "THEN", "ELSE", "ELSEIF", "ENDIF", "END", "SELECT", "CASE", "DIM", "REDIM",
        "ERASE", "SWAP", "INPUT", "LINE", "OPEN", "CLOSE", "GOSUB", "RETURN", "STOP",
        "EXIT", "ON", "DATA", "READ", "RESTORE", "DEF", "DECLARE", "GLOBAL", "LOCAL",
        "SLEEP", "RANDOMIZE", "ERROR", "RESUME", "TRY", "CATCH", "FINALLY", "RAISE",
        "THROW", "IMPORT", "REQUIRE", "NAMESPACE", "CLASS", "SUB", "FUNCTION",
        "GOTO", "WIDTH", "COLOR", "LOCATE", "CLS", NULL
    };
    char word[32];
    size_t n = 0;
    while (src[n] && (isalnum((unsigned char)src[n]) || src[n] == '_') && n < sizeof(word) - 1) {
        word[n] = (char)toupper((unsigned char)src[n]);
        n++;
    }
    word[n] = 0;
    for (int i = 0; kw[i]; i++)
        if (strcmp(word, kw[i]) == 0) return 0;
    return 1;
}

/* ── API ─────────────────────────────────────────────────────────────────── */

nb_kernel_t* nb_kernel_new(void)
{
    nb_kernel_t* k = (nb_kernel_t*)calloc(1, sizeof(*k));
    if (!k) return NULL;
    k->env = gmb_dict_new();
    return k;
}

void nb_kernel_free(nb_kernel_t* k)
{
    if (!k) return;
    for (int i = 0; i < k->nlist; i++) {
        free(k->list[i].id);
        free(k->list[i].source);
        free(k->list[i].output);
    }
    gmb_dict_free(&k->env);
    free(k);
}

static nb_cell_t* nb_cell_find(nb_kernel_t* k, const char* id)
{
    if (!id || !*id) return NULL;
    for (int i = 0; i < k->nlist; i++)
        if (k->list[i].id && !strcmp(k->list[i].id, id)) return &k->list[i];
    return NULL;
}

static void nb_cell_store(nb_kernel_t* k, const char* id, const char* source, const nb_result_t* r)
{
    if (!id || !*id) return;
    nb_cell_t* c = nb_cell_find(k, id);
    if (!c) {
        if (k->nlist >= NB_MAX_CELLS) return;
        c = &k->list[k->nlist++];
        c->id = strdup(id);
    }
    free(c->source);
    free(c->output);
    c->source = strdup(source ? source : "");
    c->output = strdup(r->output ? r->output : "");
    c->ok = r->ok;
}

/* Celle magiche della vecchia implementazione C#: %md è puro passthrough; le altre
 * richiedono SQLite/LLM/VectorDB e non esistono nel notebook C (errore ESPLICITO). */
static int nb_magic(const char* src, const char** out_text)
{
    const char* p = src;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '%') return 0;
    if (!strncasecmp(p, "%md", 3) && (!p[3] || isspace((unsigned char)p[3]))) {
        *out_text = p + 3;
        while (**out_text == ' ' || **out_text == '\t') (*out_text)++;
        return 1;
    }
    *out_text = NULL;
    return 2;   /* magia non supportata */
}

int nb_kernel_exec_cell(nb_kernel_t* k, const char* id, const char* source, nb_result_t* r)
{
    const char* text = NULL;
    int magic = nb_magic(source ? source : "", &text);
    if (magic == 1) {
        memset(r, 0, sizeof(*r));
        r->ok = 1;
        r->output = strdup(text ? text : "");
        r->exec_ms = 0;
        k->cells++;
        nb_cell_store(k, id, source, r);
        return 0;
    }
    if (magic == 2) {
        memset(r, 0, sizeof(*r));
        r->output = strdup("");
        r->ok = 0;
        r->error = strdup("cella magica non supportata nel notebook C: usa il codice BASIC "
                          "oppure la superficie C/engine (gmb, gmbcs)");
        nb_cell_store(k, id, source, r);
        return 1;
    }
    int rc = nb_kernel_exec(k, source, r);
    nb_cell_store(k, id, source, r);
    return rc;
}

char* nb_kernel_cells_json(nb_kernel_t* k)
{
    jbuf_t b;
    jb_init(&b, 256);
    jb_puts(&b, "{\"cells\":[");
    for (int i = 0; i < k->nlist; i++) {
        if (i) jb_puts(&b, ",");
        jb_puts(&b, "{\"cellId\":\"");
        jb_escape(&b, k->list[i].id ? k->list[i].id : "");
        jb_puts(&b, "\",\"source\":\"");
        jb_escape(&b, k->list[i].source ? k->list[i].source : "");
        jb_puts(&b, "\",\"output\":\"");
        jb_escape(&b, k->list[i].output ? k->list[i].output : "");
        jb_puts(&b, k->list[i].ok ? "\",\"ok\":true}" : "\",\"ok\":false}");
    }
    jb_puts(&b, "]}");
    return b.p;
}

char* nb_kernel_order_json(nb_kernel_t* k)
{
    jbuf_t b;
    jb_init(&b, 128);
    jb_puts(&b, "{\"order\":[");
    for (int i = 0; i < k->nlist; i++) {
        if (i) jb_puts(&b, ",");
        jb_puts(&b, "\"");
        jb_escape(&b, k->list[i].id ? k->list[i].id : "");
        jb_puts(&b, "\"");
    }
    jb_puts(&b, "]}");
    return b.p;
}

char* nb_kernel_runall(nb_kernel_t* k)
{
    jbuf_t b;
    jb_init(&b, 256);
    jb_puts(&b, "{\"ok\":true,\"results\":[");
    gmb_dict_clear(&k->env);
    for (int i = 0; i < k->nlist; i++) {
        nb_result_t r;
        nb_kernel_exec(k, k->list[i].source ? k->list[i].source : "", &r);
        free(k->list[i].output);
        k->list[i].output = strdup(r.output ? r.output : "");
        k->list[i].ok = r.ok;
        if (i) jb_puts(&b, ",");
        jb_puts(&b, "{\"cellId\":\"");
        jb_escape(&b, k->list[i].id ? k->list[i].id : "");
        jb_puts(&b, "\",\"ok\":");
        jb_puts(&b, r.ok ? "true" : "false");
        jb_puts(&b, ",\"output\":\"");
        jb_escape(&b, r.output ? r.output : "");
        jb_puts(&b, "\"}");
        nb_result_free(&r);
    }
    jb_puts(&b, "]}");
    return b.p;
}

static double ms_since(const struct timespec* t0)
{
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0->tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0->tv_nsec) / 1e6;
}

/* ── esecuzione P-CODE ────────────────────────────────────────────────────
 * Buffer di cattura dell'output del frammento (hook PRINT della VM). */

typedef struct {
    char* p;
    size_t cap;
    size_t len;
} nb_cap_t;

static void nb_cap_putn(nb_cap_t* c, const char* s, size_t n)
{
    if (!s) return;
    if (c->len + n + 1 > c->cap) {
        size_t nc = c->cap * 2 + n + 64;
        char* np = (char*)realloc(c->p, nc);
        if (!np) return;
        c->p = np;
        c->cap = nc;
    }
    memcpy(c->p + c->len, s, n);
    c->len += n;
    c->p[c->len] = 0;
}

static void nb_cap_str(void* ctx, const char* s, size_t len)
{
    nb_cap_putn((nb_cap_t*)ctx, s, len);
}

/* Stessa resa del PRINT dell'host VM/C (gmb_vm_host: "%.17g"). */
static void nb_cap_num(void* ctx, double v)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%.17g", v);
    nb_cap_putn((nb_cap_t*)ctx, tmp, strlen(tmp));
}

static void nb_cap_nl(void* ctx)
{
    nb_cap_putn((nb_cap_t*)ctx, "\n", 1);
}

/* Ambiente della sessione: i nomi ignoti del frammento sono le variabili del dict
 * (contratto: valore BORROWED in lettura, 0 = ok; assente = nil). */
static int nb_env_get(void* env, const char* name, gmb_val_t* out)
{
    nb_kernel_t* k = (nb_kernel_t*)env;
    *out = gmb_dict_get_ci(k->env, name);
    return 0;
}

static int nb_env_set(void* env, const char* name, gmb_val_t in)
{
    nb_kernel_t* k = (nb_kernel_t*)env;
    gmb_dict_set(&k->env, name, in);
    return 0;
}

int nb_kernel_exec(nb_kernel_t* k, const char* source, nb_result_t* r)
{
    memset(r, 0, sizeof(*r));
    r->output = strdup("");
    const char* src = source ? source : "";

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    nb_cap_t cap;
    memset(&cap, 0, sizeof cap);
    gmb_val_t val = gmb_val_nil();
    int as_stmt = looks_like_expression(src) ? 0 : 1;
    int rc = gmb_vm_eval_text_standalone(src, as_stmt, nb_env_get, nb_env_set, k, NULL,
                                         nb_cap_str, nb_cap_num, nb_cap_nl, &cap, &val);
    /* `x = 5` non è un'espressione valida: si riesegue come STATEMENT. Il tentativo
     * fallito si ferma in compilazione (nessuna esecuzione, nessun output). */
    if (rc != 0 && as_stmt == 0 && cap.len == 0) {
        as_stmt = 1;
        rc = gmb_vm_eval_text_standalone(src, 1, nb_env_get, nb_env_set, k, NULL,
                                         nb_cap_str, nb_cap_num, nb_cap_nl, &cap, &val);
    }
    r->exec_ms = ms_since(&t0);

    if (rc != 0) {
        const char* err = gmb_vm_eval_last_error();
        r->ok = 0;
        r->error = strdup(err && *err ? err : "errore di esecuzione");
        free(cap.p);
        return 1;
    }

    free(r->output);
    r->output = cap.p ? cap.p : strdup("");
    r->ok = 1;
    k->cells++;

    if (!as_stmt) {
        jbuf_t b;
        jb_init(&b, 128);
        val_to_json(val, &b);
        r->value_json = b.p;
        if (val.kind == GMB_TYPE_STRING) gmb_string_free(&val.v_string);
    }
    return 0;
}

void nb_result_free(nb_result_t* r)
{
    if (!r) return;
    free(r->output);
    free(r->value_json);
    free(r->error);
    memset(r, 0, sizeof(*r));
}

char* nb_kernel_vars_json(nb_kernel_t* k)
{
    jbuf_t b;
    jb_init(&b, 256);
    jb_puts(&b, "{\"vars\":[");
    gmb_list_t keys = gmb_dict_keys_sorted(k->env);
    int n = (int)gmb_list_count(keys);
    int first = 1;
    for (int i = 0; i < n; i++) {
        gmb_val_t kv = gmb_list_get(keys, i);
        gmb_string_t ks = gmb_val_to_string(kv);
        const char* name = ks.data ? ks.data : "";
        if (!name[0]) continue;
        if (!first) jb_puts(&b, ",");
        first = 0;
        jb_puts(&b, "{\"name\":\"");
        jb_escape(&b, name);
        jb_puts(&b, "\",\"value\":");
        val_to_json(gmb_dict_get(k->env, name), &b);
        jb_puts(&b, "}");
    }
    jb_puts(&b, "]}");
    return b.p;
}

void nb_kernel_reset(nb_kernel_t* k)
{
    if (!k) return;
    gmb_dict_clear(&k->env);
}

int nb_kernel_cell_count(nb_kernel_t* k) { return k ? k->cells : 0; }
