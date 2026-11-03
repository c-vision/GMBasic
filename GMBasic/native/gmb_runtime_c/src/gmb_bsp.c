/* ============================================================================
 * gmb_bsp.c — HTML / <%= expr %> / <% code %> → PRINT e statement BASIC.
 * Direttive `<%@ IMPORT ... %>` tradotte in IMPORT (prima erano ignorate: le pagine che
 * importano AI/JSON non compilavano). Preambolo minimo: IMPORT HTTP + STRINGS.
 * ============================================================================ */
#include "gmb_bsp.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Renderer VM registrato da gmb_vm_c (pagine precompilate in chunk A1). */
static gmb_bsp_vm_render_fn g_vm_renderer = 0;

void gmb_bsp_set_vm_renderer(gmb_bsp_vm_render_fn fn)
{
    g_vm_renderer = fn;
}

typedef struct {
    char* p;
    size_t n;
    size_t cap;
} buf;

static int buf_grow(buf* b, size_t extra)
{
    if (b->n + extra + 1 <= b->cap) return 1;
    size_t nc = b->cap ? b->cap * 2 : 256;
    while (nc < b->n + extra + 1) nc *= 2;
    char* p = (char*)realloc(b->p, nc);
    if (!p) return 0;
    b->p = p;
    b->cap = nc;
    return 1;
}

static void buf_app(buf* b, const char* s)
{
    if (!b || !s) return;
    size_t k = strlen(s);
    if (!buf_grow(b, k)) return;
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = 0;
}

static void buf_napp(buf* b, const char* s, size_t k)
{
    if (!b || !s || !k) return;
    if (!buf_grow(b, k)) return;
    memcpy(b->p + b->n, s, k);
    b->n += k;
    b->p[b->n] = 0;
}

static void buf_ch(buf* b, char ch)
{
    if (!buf_grow(b, 1)) return;
    b->p[b->n++] = ch;
    b->p[b->n] = 0;
}

static int html_blank(const char* s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!isspace((unsigned char)s[i])) return 0;
    return 1;
}

static void emit_html(buf* b, const char* s, size_t n)
{
    if (!s || !n) return;
    /* OGNI riga sorgente diventa `PRINT "..."` SENZA `;`: il newline va preservato.
     * Con `;` le righe venivano concatenate: dentro <script> i commenti `//` si
     * mangiavano il resto del codice (SyntaxError "Unexpected end of input") e i
     * <pre> perdevano le righe. Anche le righe vuote emettono il loro newline. */
    const char* line = s;
    const char* end = s + n;
    while (line < end) {
        const char* nl = (const char*)memchr(line, '\n', (size_t)(end - line));
        const char* le = nl ? nl : end;
        size_t ln = (size_t)(le - line);
        if (ln && line[ln - 1] == '\r') ln--;
        buf_app(b, "PRINT \"");
        for (size_t i = 0; i < ln; i++) {
            if (line[i] == '"') buf_app(b, "\"\"");
            else buf_ch(b, line[i]);
        }
        /* `;` solo sull'ULTIMO segmento senza newline (es. HTML prima di <%= %>):
         * il testo deve restare attaccato. Con newline: PRINT senza `;` → riga
         * preservata (commenti // dentro <script>, <pre>, ecc.). */
        buf_app(b, nl ? "\"\n" : "\";\n");
        line = nl ? nl + 1 : end;
    }
}

/* Le pagine BSP contengono commenti BASIC `' ...` dentro i blocchi <% %>. Il motore
 * di espressioni C (gmb_expr) usa `'` come delimitatore di STRINGA (idioma
 * Clipper/SQL), quindi un commento verrebbe letto come stringa non chiusa →
 * "expected expression". Qui si tolgono i commenti riga rispettando le stringhe
 * "..." (un apostrofo DENTRO una stringa resta). Il testo HTML resta intatto. */
static void buf_app_code(buf* b, const char* s, size_t n)
{
    size_t i = 0;
    int in_str = 0;
    while (i < n) {
        char ch = s[i];
        if (in_str) {
            buf_napp(b, &ch, 1);
            if (ch == '"') {
                if (i + 1 < n && s[i + 1] == '"') { buf_napp(b, &s[i + 1], 1); i += 2; continue; }
                in_str = 0;
            }
            i++;
            continue;
        }
        if (ch == '"') { in_str = 1; buf_napp(b, &ch, 1); i++; continue; }
        if (ch == '\'') {                       /* commento fino a fine riga */
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        buf_napp(b, &ch, 1);
        i++;
    }
}

static const char* trim_start(const char* s, size_t* n)
{
    while (*n && isspace((unsigned char)*s)) { s++; (*n)--; }
    while (*n && isspace((unsigned char)s[*n - 1])) (*n)--;
    return s;
}

int gmb_bsp_compile(const char* src, char** out_bas)
{
    if (!out_bas) return GMB_EXPR_ERR_PARSE;
    *out_bas = NULL;
    if (!src) src = "";
    buf b;
    memset(&b, 0, sizeof(b));
    /* NIENTE IMPORT nel programma per gmb_expr: il valutatore non ha namespace e
     * `IMPORT HTTP` verrebbe letto come identificatore `IMPORT` → "undefined identifier"
     * (canvas.bsp non si renderizzava). Gli identificatori sono risolti da get/scope;
     * le chiamate dal callback `call`. Le direttive <%@ ... %> sono quindi solo
     * consumate (nessun codice emesso). */
    const char* p = src;
    while (*p) {
        const char* tag = strstr(p, "<%");
        if (!tag) {
            emit_html(&b, p, strlen(p));
            break;
        }
        if (tag > p) emit_html(&b, p, (size_t)(tag - p));
        p = tag + 2;
        if (*p == '@') {
            p++;
            const char* e = strstr(p, "%>");
            const char* end = e ? e : p + strlen(p);
            /* direttiva <%@ ... %>: solo consumata (IMPORT inclusi) — vedi sopra. */
            p = e ? e + 2 : end;
            continue;
        }
        if (p[0] == '-' && p[1] == '-') {
            const char* e = strstr(p, "--%>");
            p = e ? e + 4 : p + strlen(p);
            continue;
        }
        if (*p == '=') {
            p++;
            const char* e = strstr(p, "%>");
            size_t n = e ? (size_t)(e - p) : strlen(p);
            const char* expr = trim_start(p, &n);
            buf_app(&b, "PRINT ");
            if (n) buf_napp(&b, expr, n);
            buf_app(&b, ";\n");
            p = e ? e + 2 : p + n;
            continue;
        }
        const char* e = strstr(p, "%>");
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n) {
            buf_app_code(&b, p, n);
            if (p[n - 1] != '\n') buf_app(&b, "\n");
        }
        p = e ? e + 2 : p + n;
    }
    if (!b.p) {
        b.p = (char*)calloc(1, 1);
        if (!b.p) return GMB_EXPR_ERR_PARSE;
    }
    *out_bas = b.p;
    return GMB_EXPR_OK;
}

int gmb_bsp_render(const char* src, void* env,
                   gmb_expr_get_fn get, gmb_expr_set_fn set,
                   gmb_expr_call_fn call, char** out_html)
{
    char* bas = NULL;
    int rc = gmb_bsp_compile(src, &bas);
    if (rc != GMB_EXPR_OK) {
        free(bas);
        if (out_html) *out_html = NULL;
        return rc;
    }
    rc = gmb_expr_exec_capture(bas, env, get, set, call, out_html);
    free(bas);
    return rc;
}

static int bsp_hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void bsp_url_decode(char* s)
{
    char* r = s;
    char* w = s;
    while (*r) {
        if (*r == '+') {
            *w++ = ' ';
            r++;
        } else if (*r == '%' && bsp_hex((unsigned char)r[1]) >= 0 && bsp_hex((unsigned char)r[2]) >= 0) {
            *w++ = (char)((bsp_hex((unsigned char)r[1]) << 4) | bsp_hex((unsigned char)r[2]));
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

static int bsp_ends_dot_bsp(const char* p)
{
    size_t n;
    const char* e;
    if (!p) return 0;
    n = strlen(p);
    if (n < 4) return 0;
    e = p + n - 4;
    return e[0] == '.'
        && (e[1] == 'b' || e[1] == 'B')
        && (e[2] == 's' || e[2] == 'S')
        && (e[3] == 'p' || e[3] == 'P');
}

static void bsp_seed_query(gmb_dict_t* env, const char* query)
{
    char* buf;
    char* p;
    if (!env || !query || !*query) return;
    buf = (char*)malloc(strlen(query) + 1);
    if (!buf) return;
    memcpy(buf, query, strlen(query) + 1);
    p = buf;
    while (*p) {
        char* amp = strchr(p, '&');
        char* eq;
        char* val;
        if (amp) *amp = 0;
        eq = strchr(p, '=');
        val = (char*)"";
        if (eq) {
            *eq = 0;
            val = eq + 1;
        }
        bsp_url_decode(p);
        if (eq) bsp_url_decode(val);
        if (*p) gmb_dict_set(env, p, gmb_make_val_string(val));
        if (!amp) break;
        p = amp + 1;
    }
    free(buf);
}

static char* bsp_read_file(const char* path)
{
    FILE* f;
    long n;
    char* b;
    size_t r;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 1048576L) { fclose(f); return NULL; }
    rewind(f);
    b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    r = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[r] = 0;
    return b;
}

static char* bsp_dup_lit(const char* s)
{
    size_t n = strlen(s);
    char* b = (char*)malloc(n + 1);
    if (!b) return NULL;
    memcpy(b, s, n + 1);
    return b;
}

/* ── Dispatcher delle chiamate BASIC usate dalle pagine BSP ─────────────────────
 * Il valutatore di espressioni non conosce i namespace: le chiamate `AI.*`/`MEMORY.*`
 * delle pagine arrivano qui e delegano alle STESSE funzioni del runtime condiviso
 * (§0bis), senza reimplementare nulla. `DASHBOARD.*` non è qui: sono FUNZIONI BASIC
 * dell'include e restano eseguibili solo dal compilatore completo (le pagine servite
 * dal motore C non le usano). */
static gmb_val_t bsp_str_val(const char* s) { return gmb_make_val_string(s ? s : ""); }

static gmb_val_t bsp_names_list(char names[][64], int n)
{
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_push(&l, bsp_str_val(names[i]));
    return gmb_make_val_list_take(&l);
}

static const char* bsp_arg_cstr(const gmb_val_t* a, int n, int i)
{
    if (i >= n || !a) return "";
    if (a[i].kind == GMB_TYPE_STRING && a[i].v_string.data) return a[i].v_string.data;
    return "";
}

static int bsp_runtime_call(void* env, const char* name, const gmb_val_t* args, int nargs,
                            gmb_val_t* out)
{
    (void)env;
    if (!name || !out) return -1;
    if (!strcasecmp(name, "AI.AGENTLIST")) {
        char names[64][64];
        *out = bsp_names_list(names, gmb_agent_list(names, 64));
        return 0;
    }
    if (!strcasecmp(name, "AI.TEAMLIST")) {
        char names[64][64];
        *out = bsp_names_list(names, gmb_team_list(names, 64));
        return 0;
    }
    if (!strcasecmp(name, "AI.AGENTEVENTS")) {
        char evs[64][256];
        int n = gmb_agent_events(evs, 64);
        gmb_list_t l = gmb_list_new();
        for (int i = 0; i < n; i++) gmb_list_push(&l, bsp_str_val(evs[i]));
        *out = gmb_make_val_list_take(&l);
        return 0;
    }
    if (!strcasecmp(name, "AI.AGENTGET")) {
        *out = bsp_str_val(gmb_agent_get(bsp_arg_cstr(args, nargs, 0), bsp_arg_cstr(args, nargs, 1)));
        return 0;
    }
    if (!strcasecmp(name, "AI.WORKFLOWLIST")) {
        gmb_string_t s = gmb_workflow_list();
        *out = bsp_str_val(s.data);
        return 0;
    }
    if (!strcasecmp(name, "AI.WORKFLOWSTATUS")) {
        gmb_string_t s = gmb_workflow_status(bsp_arg_cstr(args, nargs, 0));
        *out = bsp_str_val(s.data);
        return 0;
    }
    if (!strcasecmp(name, "AI.WORKFLOWSTEPS")) {
        gmb_string_t s = gmb_workflow_steps(bsp_arg_cstr(args, nargs, 0));
        *out = bsp_str_val(s.data);
        return 0;
    }
    if (!strcasecmp(name, "MEMORY.LIST")) {
        char* p = gmb_mem_list();
        *out = bsp_str_val(p);
        free(p);
        return 0;
    }
    return -1;
}

/* Fallback STATICO di una pagina BSP: markup HTML/CSS/JS senza i blocchi <% %>.
 * Serve quando il valutatore C non esegue un costrutto della pagina (DIM array,
 * chiamate a FUNCTION dell'include, stato del runtime...): la UI resta viva perché
 * carica i dati dagli endpoint /api/*. NON è SSR: il client lo legge nel commento
 * HTML in testa (nessun inganno) e stderr riporta la causa. */
static char* bsp_static_shell(const char* src)
{
    buf b;
    memset(&b, 0, sizeof(b));
    buf_app(&b, "<!-- BSP: SSR non eseguito dal motore C per questa pagina; "
                "la UI carica i dati via /api/* -->\n");
    const char* p = src ? src : "";
    while (*p) {
        const char* tag = strstr(p, "<%");
        if (tag) {
            if (tag > p) buf_napp(&b, p, (size_t)(tag - p));
            const char* e = strstr(tag + 2, "%>");
            p = e ? e + 2 : tag + strlen(tag);
            continue;
        }
        buf_app(&b, p);
        break;
    }
    if (!b.p) return strdup("");
    return b.p;
}

int gmb_bsp_try_serve(const char* dir, const char* url_path, const char* query,
                      const char* host, char** out_html, int* out_status)
{
    const char* rel;
    char path[1024];
    char* src;
    gmb_dict_t env;
    int rc;

    if (out_html) *out_html = NULL;
    if (out_status) *out_status = 0;
    if (!dir || !*dir || !url_path) return 0;
    if (!bsp_ends_dot_bsp(url_path)) return 0;

    rel = url_path;
    while (*rel == '/') rel++;
    if (!*rel) return 0;
    if (strstr(rel, "..")) {
        if (out_status) *out_status = 403;
        if (out_html) *out_html = bsp_dup_lit("Forbidden");
        return 1;
    }

    snprintf(path, sizeof(path), "%s/%s", dir, rel);

    /* Renderer VM (gmb_vm_c): pagina precompilata in chunk A1 eseguita dalla VM
     * completa (FETCH/JSON/metodi). 0 = sidecar assente/non fresco → fallback gmb_expr. */
    if (g_vm_renderer && g_vm_renderer(path, url_path, query, host, out_html, out_status))
        return 1;

    src = bsp_read_file(path);
    if (!src) return 0;

    env = gmb_dict_new();
    bsp_seed_query(&env, query);
    rc = gmb_bsp_render(src, &env, gmb_expr_dict_get, gmb_expr_dict_set, bsp_runtime_call, out_html);
    gmb_dict_free(&env);
    if (rc != GMB_EXPR_OK) {
        /* Il motore di espressioni C non è un interprete BASIC completo: una pagina che
         * usa costrutti fuori dal suo sottoinsieme non si può renderizzare qui. Si serve
         * il MARKUP STATICO (stessa UI, dati via /api/*) invece del 500 "BSP error", che
         * rendeva il dashboard inutilizzabile. Causa SEMPRE su stderr, nota nel commento
         * HTML: non è un successo silenzioso. */
        fprintf(stderr, "[bsp] %s: motore C non esegue la pagina (%s) → fallback statico\n",
                path, gmb_expr_last_error());
        if (out_html && *out_html) { free(*out_html); *out_html = NULL; }
        if (out_html) *out_html = bsp_static_shell(src);
        if (out_status) *out_status = 200;
        free(src);
        return 1;
    }
    free(src);
    if (out_status) *out_status = 200;
    return 1;
}
