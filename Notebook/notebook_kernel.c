/* Notebook/notebook_kernel.c — kernel di una sessione notebook (C).
 *
 * Esecuzione di una cella: `gmb_expr_exec_capture` del runtime C (valutatore BASIC
 * condiviso con le macro &/EVAL): supporta assegnazioni, PRINT, IF/THEN/ELSE, FOR/NEXT,
 * WHILE/WEND, LET, chiamate a funzione. Lo stato della sessione è il dict passato come
 * ambiente (`gmb_expr_dict_get/set`): le celle successive vedono le variabili precedenti.
 *
 * Limite dichiarato (Fase 1): niente SUB/FUNCTION definite in cella (il valutatore è un
 * interprete di statement, non un compilatore di moduli). Per un'espressione pura il
 * server restituisce anche `value` (valutata di nuovo: nessun effetto collaterale).
 */
#include "notebook_kernel.h"

#include "gmb_runtime.h"
#include "gmb_expr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

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

int nb_kernel_exec(nb_kernel_t* k, const char* source, nb_result_t* r)
{
    memset(r, 0, sizeof(*r));
    r->output = strdup("");

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    char* cap = NULL;
    int rc = gmb_expr_exec_capture(source ? source : "", &k->env,
                                   gmb_expr_dict_get, gmb_expr_dict_set, NULL, &cap);
    r->exec_ms = ms_since(&t0);

    if (rc != GMB_EXPR_OK) {
        const char* err = gmb_expr_last_error();
        r->ok = 0;
        r->error = strdup(err && *err ? err : "errore di esecuzione");
        free(cap);
        return 1;
    }

    free(r->output);
    r->output = cap ? cap : strdup("");
    r->ok = 1;
    k->cells++;

    if (looks_like_expression(source)) {
        gmb_val_t v;
        if (gmb_expr_eval(source, &k->env, gmb_expr_dict_get, gmb_expr_dict_set, &v) == GMB_EXPR_OK) {
            jbuf_t b;
            jb_init(&b, 128);
            val_to_json(v, &b);
            r->value_json = b.p;
        }
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
