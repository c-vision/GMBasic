/* native/gmb_runtime_c/src/gmb_agent_http.c — server HTTP agent (AI AGENT SERVE).
 *
 * Core INTERAMENTE in C (il programmatore BASIC customizza solo le route). Thread server
 * dedicato: accetta connessioni, legge la richiesta HTTP, fa il routing via TABELLA di
 * route (niente switch monolitico — §0quater) e ritorna JSON.
 *
 * Convenzione: NON invoca SUB BASIC direttamente (regola GPM §0ter): le route sono
 * servite qui in C sul runtime condiviso (gmb_agent_* / gmb_mem_* / gmb_brain_* /
 * gmb_workflow_* / gmb_enrich) — la logica di dominio resta nel runtime (§0bis).
 *
 * Superficie (porting 2026-09-30, parità IlRuntime.HandleAgentHttpRequestAsync):
 *   legacy agent : /chat /execute /memory /workflow
 *   webapp       : /api/memory /api/memory/search /api/brain/<route> /api/enrich
 *                  /api/workflows[/{n}[/steps|/execute]] /api/agent/{n}/prompt
 *                  /api/team/{n}/broadcast
 *   non portate  : /api/agents /api/teams /api/teams/{n} /api/events → 501 ESPLICITO
 *                  (servono gli aggregatori JSON di AgentStatusManager/AgentEventLog,
 *                  assenti nel runtime C: vedi Todo/(open) - Webapp-HTTP-Surface-C-Port §2.1)
 */
#include "gmb_runtime.h"
#include "gmb_network.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static pthread_t g_http_thread;
static int64_t   g_http_listener = -1;
static volatile int g_http_running = 0;

/* ============================================================================
 * Richiesta / risposta
 * ========================================================================== */
typedef struct {
    char method[16];
    char path[256];
    char query[1024];
    char* body;          /* malloc, o NULL */
} http_req_t;

typedef struct {
    int status;
    char* json;          /* malloc */
} http_res_t;

static void res_set(http_res_t* r, int status, char* json_owned)
{
    free(r->json);
    r->status = status;
    r->json = json_owned;
}

static void res_err(http_res_t* r, int status, const char* msg)
{
    size_t mlen = msg ? strlen(msg) : 0;
    size_t cap = mlen * 6 + 32;
    char* b = (char*)malloc(cap);
    if (!b) { r->status = status; r->json = NULL; return; }
    size_t n = 0;
    n += (size_t)snprintf(b + n, cap - n, "{\"error\":\"");
    /* gmb_json_escape non NUL-termina: scrivo nel buffer di destinazione. */
    n += gmb_json_escape(b + n, cap - n, msg ? msg : "");
    snprintf(b + n, cap - n, "\"}");
    res_set(r, status, b);
}

/* ============================================================================
 * Buffer JSON growable
 * ========================================================================== */
typedef struct { char* p; size_t n, cap; } rbuf_t;

static void rb_reserve(rbuf_t* b, size_t extra)
{
    if (b->n + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 256;
    while (b->n + extra + 1 > nc) nc *= 2;
    char* np = (char*)realloc(b->p, nc);
    if (!np) return;
    b->p = np;
    b->cap = nc;
}

static void rb_add(rbuf_t* b, const char* s, size_t len)
{
    if (!s || !len) return;
    rb_reserve(b, len);
    if (!b->p) return;
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void rb_adds(rbuf_t* b, const char* s) { if (s) rb_add(b, s, strlen(s)); }

static void rb_addf(rbuf_t* b, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    int k = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (k < 0) return;
    if ((size_t)k < sizeof(tmp)) { rb_add(b, tmp, (size_t)k); return; }
    char* big = (char*)malloc((size_t)k + 1);
    if (!big) return;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)k + 1, fmt, ap);
    va_end(ap);
    rb_add(b, big, (size_t)k);
    free(big);
}

/* Accoda il testo con escape JSON DIRETTAMENTE nel buffer (gmb_json_escape non
 * NUL-termina: va usata scrivendo a destinazione e facendo avanzare l'offset,
 * convenzione di gmb_rag_search.c). */
static void rb_add_escaped(rbuf_t* b, const char* s)
{
    if (!s) return;
    size_t need = strlen(s) * 6 + 8;
    rb_reserve(b, need);
    if (!b->p) return;
    b->n += gmb_json_escape(b->p + b->n, b->cap - b->n, s);
    b->p[b->n] = '\0';
}

/* {"k":"<escaped>"} — apre un oggetto con UNA chiave stringa. */
static void rb_open_kv(rbuf_t* b, const char* key, const char* val)
{
    rb_addf(b, "\"%s\":\"", key);
    rb_add_escaped(b, val ? val : "");
    rb_adds(b, "\"");
}

/* JSON array di stringhe, da un buffer "\n"-joinato (parità split('\n')).
 * Escape su un buffer temporaneo (mai in place: sovrapposizione di buffer). */
static void rb_add_lines_array(rbuf_t* b, const char* joined)
{
    char line[8192];
    rb_adds(b, "[");
    if (joined && *joined) {
        const char* p = joined;
        int first = 1;
        while (*p) {
            const char* e = strchr(p, '\n');
            size_t len = e ? (size_t)(e - p) : strlen(p);
            if (len >= sizeof(line)) len = sizeof(line) - 1;
            memcpy(line, p, len);
            line[len] = '\0';
            rb_adds(b, first ? "\"" : ",\"");
            rb_add_escaped(b, line);
            rb_adds(b, "\"");
            first = 0;
            if (!e) break;
            p = e + 1;
        }
    }
    rb_adds(b, "]");
}

static char* rb_take(rbuf_t* b)
{
    if (!b->p) return strdup("{}");
    char* out = b->p;
    out[b->n] = '\0';
    return out;
}

/* ============================================================================
 * Parsing: query-string e campo JSON di primo livello
 * ========================================================================== */

/* Valore del parametro di query `name` (URL-decoded minimamente: %XX e '+'). */
static int query_get(const http_req_t* q, const char* name, char* out, size_t cap)
{
    if (!q->query[0]) return 0;
    size_t nl = strlen(name);
    const char* p = q->query;
    while (*p) {
        const char* amp = strchr(p, '&');
        size_t seglen = amp ? (size_t)(amp - p) : strlen(p);
        if (seglen > nl && p[nl] == '=' && strncmp(p, name, nl) == 0) {
            const char* v = p + nl + 1;
            size_t vlen = seglen - nl - 1;
            if (vlen >= cap) vlen = cap - 1;
            size_t w = 0;
            for (size_t i = 0; i < vlen; i++) {
                char ch = v[i];
                if (ch == '+') ch = ' ';
                else if (ch == '%' && i + 2 < vlen) {
                    char h[3] = { v[i + 1], v[i + 2], 0 };
                    char* end = NULL;
                    long d = strtol(h, &end, 16);
                    if (end && *end == '\0') { ch = (char)d; i += 2; }
                }
                out[w++] = ch;
            }
            out[w] = '\0';
            return 1;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return 0;
}

/* Legge il TESTO GREZZO del valore JSON di `key` al PRIMO livello dell'oggetto.
 * Ritorna 1 se trovato. Gestisce stringhe (con \\ \" \n \t \r \b \f \uXXXX → UTF-8),
 * numeri, true/false/null. Non Naviga dentro oggetti/array annidati (le route webapp
 * usano solo campi scalari di primo livello). */
static int json_field_raw(const char* body, const char* key, char* out, size_t cap)
{
    if (!body || !key) return 0;
    const char* p = strchr(body, '{');
    if (!p) return 0;
    p++;
    size_t kl = strlen(key);
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p == '}') return 0;
        if (*p != '"') return 0;
        const char* ks = ++p;
        const char* ke = ks;
        while (*ke && *ke != '"') { if (*ke == '\\' && ke[1]) ke++; ke++; }
        if (!*ke) return 0;
        size_t klen = (size_t)(ke - ks);
        p = ke + 1;
        while (*p == ' ' || *p == ':' || *p == '\t') p++;
        if (klen == kl && strncmp(ks, key, kl) == 0) {
            /* valore */
            if (*p == '"') {
                p++;
                size_t w = 0;
                while (*p && *p != '"' && w + 1 < cap) {
                    if (*p == '\\' && p[1]) {
                        p++;
                        char c = *p++;
                        switch (c) {
                            case 'n': out[w++] = '\n'; break;
                            case 't': out[w++] = '\t'; break;
                            case 'r': out[w++] = '\r'; break;
                            case 'b': out[w++] = '\b'; break;
                            case 'f': out[w++] = '\f'; break;
                            case 'u': {
                                char hx[5] = { p[0], p[1], p[2], p[3], 0 };
                                unsigned cp = (unsigned)strtoul(hx, NULL, 16);
                                p += 4;
                                if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                                    char hx2[5] = { p[2], p[3], p[4], p[5], 0 };
                                    unsigned lo = (unsigned)strtoul(hx2, NULL, 16);
                                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                        p += 6;
                                    }
                                }
                                if (cp < 0x80) out[w++] = (char)cp;
                                else if (cp < 0x800) {
                                    out[w++] = (char)(0xC0 | (cp >> 6));
                                    out[w++] = (char)(0x80 | (cp & 0x3F));
                                } else if (cp < 0x10000) {
                                    out[w++] = (char)(0xE0 | (cp >> 12));
                                    out[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                                    out[w++] = (char)(0x80 | (cp & 0x3F));
                                } else {
                                    out[w++] = (char)(0xF0 | (cp >> 18));
                                    out[w++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                                    out[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                                    out[w++] = (char)(0x80 | (cp & 0x3F));
                                }
                                break;
                            }
                            default: out[w++] = c;
                        }
                    } else out[w++] = *p++;
                }
                out[w] = '\0';
                return 1;
            }
            size_t w = 0;
            while (*p && *p != ',' && *p != '}' && *p != ' ' && w + 1 < cap) out[w++] = *p++;
            out[w] = '\0';
            return 1;
        }
        if (*p == '"') {                 /* salta la stringa-valore */
            p++;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
            if (*p) p++;
        } else if (*p == '{' || *p == '[') {
            int depth = 0;
            while (*p) {
                if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; } if (*p) p++; continue; }
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') { depth--; if (!depth) { p++; break; } }
                p++;
            }
        } else {
            while (*p && *p != ',' && *p != '}') p++;
        }
    }
    return 0;
}

/* Ordine C#: JSON prima, poi query-string (POST /api/memory, /api/brain/…). */
static int field_or_query(const http_req_t* q, const char* name, char* out, size_t cap)
{
    if (json_field_raw(q->body, name, out, cap)) return 1;
    return query_get(q, name, out, cap);
}

/* Ordine C# invertito in GET/DELETE /api/memory: query-string prima, poi JSON. */
static int query_or_field(const http_req_t* q, const char* name, char* out, size_t cap)
{
    if (query_get(q, name, out, cap)) return 1;
    return json_field_raw(q->body, name, out, cap);
}

static int is_post(const http_req_t* q) { return strcmp(q->method, "POST") == 0; }
static int is_get(const http_req_t* q)  { return strcmp(q->method, "GET") == 0; }
static int is_delete(const http_req_t* q) { return strcmp(q->method, "DELETE") == 0; }

static int blank(const char* s) { return !s || !*s; }

/* ============================================================================
 * Route legacy agent (/chat /execute /memory /workflow)
 * ========================================================================== */
static void h_chat(const http_req_t* q, http_res_t* r)
{
    char agent[256] = "Demo", prompt[2048] = "Hello";
    field_or_query(q, "agent", agent, sizeof(agent));
    field_or_query(q, "prompt", prompt, sizeof(prompt));
    char* ans = gmb_agent_ask(agent, prompt);
    rbuf_t b = {0};
    rb_open_kv(&b, "answer", ans ? ans : "");
    res_set(r, 200, rb_take(&b));
    if (ans) free(ans);
}

static void h_memory_agent(const http_req_t* q, http_res_t* r)
{
    char agent[256] = "Demo";
    field_or_query(q, "agent", agent, sizeof(agent));
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "agent", agent);
    rb_adds(&b, ",");
    const char* st = gmb_agent_status(agent);
    rb_open_kv(&b, "status", st ? st : "");
    rb_adds(&b, ",\"history\":[]}");
    res_set(r, 200, rb_take(&b));
}

static void h_workflow_legacy(const http_req_t* q, http_res_t* r)
{
    char name[256] = "";
    if (!field_or_query(q, "workflow", name, sizeof(name)) ||
        (blank(name) && !field_or_query(q, "name", name, sizeof(name)))) {
        if (!query_get(q, "name", name, sizeof(name))) {
            res_err(r, 400, "Missing 'workflow' or 'name'");
            return;
        }
    }
    if (blank(name)) { res_err(r, 400, "Missing 'workflow' or 'name'"); return; }
    if (!gmb_workflow_exists(name)) {
        char m[512];
        snprintf(m, sizeof(m), "Workflow '%s' not found", name);
        res_err(r, 404, m);
        return;
    }
    /* il C# esegue se lo stato è ready/failed/completed, poi rilegge */
    int executed = 0;
    gmb_string_t st0 = gmb_workflow_status(name);
    const char* s0 = st0.data ? st0.data : "";
    if (strcmp(s0, "ready") == 0 || strcmp(s0, "failed") == 0 || strcmp(s0, "completed") == 0) {
        executed = gmb_workflow_execute_ok(name);
        gmb_string_free(&st0);
    }
    gmb_string_t st = gmb_workflow_status(name);
    gmb_string_t steps = gmb_workflow_steps(name);
    char* log = gmb_wf_log_json(name);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "workflow", name);
    rb_adds(&b, ",");
    rb_open_kv(&b, "status", st.data ? st.data : "");
    rb_adds(&b, ",");
    rb_open_kv(&b, "steps", steps.data ? steps.data : "");
    rb_adds(&b, ",\"log\":");
    rb_adds(&b, log ? log : "[]");
    rb_addf(&b, ",\"executed\":%s}", executed ? "true" : "false");
    res_set(r, 200, rb_take(&b));
    gmb_string_free(&st);
    gmb_string_free(&steps);
    if (log) free(log);
}

/* ============================================================================
 * Route webapp — /api/memory*
 * ========================================================================== */
static void h_api_memory(const http_req_t* q, http_res_t* r)
{
    if (is_post(q)) {
        char key[512] = "";
        if (!field_or_query(q, "key", key, sizeof(key)) || blank(key)) {
            res_err(r, 400, "Missing 'key'");
            return;
        }
        char val[4096] = "";
        int have_val = json_field_raw(q->body, "value", val, sizeof(val));
        if (!have_val && !query_get(q, "value", val, sizeof(val))) val[0] = '\0';
        int lt = 0;
        char ltbuf[32] = "";
        if (json_field_raw(q->body, "longTerm", ltbuf, sizeof(ltbuf))) lt = atoi(ltbuf);
        else if (json_field_raw(q->body, "isLongTerm", ltbuf, sizeof(ltbuf)))
            lt = (strcmp(ltbuf, "true") == 0 || strcmp(ltbuf, "True") == 0) ? 1 : 0;
        int ok = gmb_mem_store(key, val, lt);
        rbuf_t b = {0};
        rb_adds(&b, "{");
        rb_open_kv(&b, "key", key);
        rb_addf(&b, ",\"ok\":%s,", ok ? "true" : "false");
        rb_open_kv(&b, "value", val);
        rb_adds(&b, "}");
        res_set(r, 200, rb_take(&b));
        return;
    }
    if (is_delete(q)) {
        char key[512] = "";
        if (!query_or_field(q, "key", key, sizeof(key)) || blank(key)) {
            res_err(r, 400, "Missing 'key'");
            return;
        }
        int ok = gmb_mem_delete(key);
        rbuf_t b = {0};
        rb_adds(&b, "{");
        rb_open_kv(&b, "key", key);
        rb_addf(&b, ",\"deleted\":%s}", ok ? "true" : "false");
        res_set(r, ok ? 200 : 404, rb_take(&b));
        return;
    }
    /* GET */
    char key[512] = "";
    if (query_or_field(q, "key", key, sizeof(key)) && !blank(key)) {
        char* v = gmb_mem_load(key);
        rbuf_t b = {0};
        rb_adds(&b, "{");
        rb_open_kv(&b, "key", key);
        rb_adds(&b, ",");
        rb_open_kv(&b, "value", v ? v : "");
        rb_adds(&b, "}");
        res_set(r, 200, rb_take(&b));
        if (v) free(v);
        return;
    }
    char* list = gmb_mem_list();
    rbuf_t b = {0};
    rb_adds(&b, "{\"keys\":");
    rb_add_lines_array(&b, list);
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (list) free(list);
}

static void h_api_memory_search(const http_req_t* q, http_res_t* r)
{
    char pat[512] = "";
    if (!query_get(q, "pattern", pat, sizeof(pat)) && !query_get(q, "q", pat, sizeof(pat)))
        field_or_query(q, "pattern", pat, sizeof(pat));
    char* res = gmb_mem_search(pat);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "pattern", pat);
    rb_adds(&b, ",\"keys\":");
    rb_add_lines_array(&b, res);
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (res) free(res);
}

/* ============================================================================
 * Route webapp — /api/brain/<route>
 * ========================================================================== */
static void h_api_brain_note(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    char txt[4096] = "";
    if (!field_or_query(q, "text", txt, sizeof(txt)) || blank(txt))
        field_or_query(q, "note", txt, sizeof(txt));
    if (blank(txt)) { res_err(r, 400, "Missing 'text'"); return; }
    long id = gmb_brain_add(txt, "Note", NULL, 0);
    rbuf_t b = {0};
    rb_addf(&b, "{\"id\":%ld,", id);
    rb_open_kv(&b, "text", txt);
    rb_adds(&b, ",\"kind\":\"Note\"}");
    res_set(r, 200, rb_take(&b));
}

static void h_api_brain_save(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    char fact[4096] = "";
    if (!field_or_query(q, "fact", fact, sizeof(fact)) || blank(fact))
        field_or_query(q, "text", fact, sizeof(fact));
    if (blank(fact)) { res_err(r, 400, "Missing 'fact'"); return; }
    long id = gmb_brain_add(fact, "Fact", NULL, 0);
    rbuf_t b = {0};
    rb_addf(&b, "{\"id\":%ld,", id);
    rb_open_kv(&b, "fact", fact);
    rb_adds(&b, ",\"kind\":\"Fact\"}");
    res_set(r, 200, rb_take(&b));
}

static void h_api_brain_forget(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q) && !is_delete(q)) { res_err(r, 405, "POST or DELETE required"); return; }
    char ids[64] = "";
    if (!field_or_query(q, "id", ids, sizeof(ids)) || blank(ids)) {
        res_err(r, 400, "Missing numeric 'id'");
        return;
    }
    long id = strtol(ids, NULL, 10);
    int ok = gmb_brain_forget(id);
    rbuf_t b = {0};
    rb_addf(&b, "{\"id\":%ld,\"deleted\":%s}", id, ok ? "true" : "false");
    res_set(r, ok ? 200 : 404, rb_take(&b));
}

static void h_api_brain_import(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    char path[1024] = "";
    if (!field_or_query(q, "path", path, sizeof(path)) || blank(path)) {
        res_err(r, 400, "Missing 'path'");
        return;
    }
    long id = gmb_brain_import_file(path, 0);
    if (id < 0) { res_err(r, 500, "ImportFile failed"); return; }
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "path", path);
    rb_addf(&b, ",\"id\":%ld}", id);
    res_set(r, 200, rb_take(&b));
}

static void h_api_brain_watch(const http_req_t* q, http_res_t* r)
{
    if (is_get(q)) {
        char* csv = gmb_brain_watched_csv();
        rbuf_t b = {0};
        rb_adds(&b, "{\"watched\":");
        rb_add_lines_array(&b, csv);
        rb_adds(&b, "}");
        res_set(r, 200, rb_take(&b));
        if (csv) free(csv);
        return;
    }
    if (!is_post(q)) { res_err(r, 405, "GET or POST required"); return; }
    char path[1024] = "";
    if (!field_or_query(q, "path", path, sizeof(path)) || blank(path)) {
        res_err(r, 400, "Missing 'path'");
        return;
    }
    if (gmb_brain_add_watch(path) <= 0) { res_err(r, 500, "AddWatch failed"); return; }
    char* csv = gmb_brain_watched_csv();
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "path", path);
    rb_adds(&b, ",\"watched\":");
    rb_add_lines_array(&b, csv);
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (csv) free(csv);
}

static void h_api_brain_refresh(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    int n = gmb_brain_refresh(0);
    rbuf_t b = {0};
    rb_addf(&b, "{\"refreshed\":%d}", n);
    res_set(r, 200, rb_take(&b));
}

static void h_api_brain_consolidate(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    gmb_memory_consolidate();
    res_set(r, 200, strdup("{\"consolidated\":true}"));
}

static void h_api_brain_list(const http_req_t* q, http_res_t* r)
{
    int lim = 20;
    char lb[32] = "";
    if (query_get(q, "limit", lb, sizeof(lb))) lim = atoi(lb);
    else if (json_field_raw(q->body, "limit", lb, sizeof(lb))) lim = atoi(lb);
    if (lim < 0) lim = 0;
    int topk = lim > 50 ? lim : 50;              /* Math.Max(lim, 50) */
    char* all = gmb_brain_search("", topk, 0);
    /* split('\n') + Take(min(lim, n)) */
    int total = 0;
    for (const char* p = all; p && *p; p++) if (*p == '\n') total++;
    total = (all && *all) ? total + 1 : 0;
    int take = lim < total ? lim : total;
    rbuf_t b = {0};
    rb_addf(&b, "{\"count\":%d,\"items\":", take);
    rb_adds(&b, "[");
    int i = 0;
    const char* p = all;
    while (p && *p && i < take) {
        const char* e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char line[8192];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        rb_adds(&b, i ? ",\"" : "\"");
        rb_add_escaped(&b, line);
        rb_adds(&b, "\"");
        i++;
        if (!e) break;
        p = e + 1;
    }
    rb_adds(&b, "]}");
    res_set(r, 200, rb_take(&b));
    if (all) free(all);
}

static void h_api_brain_search(const http_req_t* q, http_res_t* r)
{
    char query[1024] = "";
    if (!query_get(q, "query", query, sizeof(query)) && !query_get(q, "q", query, sizeof(query)))
        field_or_query(q, "query", query, sizeof(query));
    double topk = 5.0;
    char tb[32] = "";
    if (json_field_raw(q->body, "topK", tb, sizeof(tb))) topk = atof(tb);
    else if (query_get(q, "topK", tb, sizeof(tb))) topk = atof(tb);
    char* res = gmb_brain_search(query, (int)topk, 0);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "query", query);
    rb_addf(&b, ",\"topK\":%d,", (int)topk);
    rb_open_kv(&b, "result", res ? res : "");
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (res) free(res);
}

/* ============================================================================
 * Route webapp — /api/enrich
 * ========================================================================== */
static void h_api_enrich(const http_req_t* q, http_res_t* r)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    char text[8192] = "";
    if (!field_or_query(q, "text", text, sizeof(text)) || blank(text)) {
        res_err(r, 400, "Missing 'text'");
        return;
    }
    char mode[64] = "SUMMARIZATION";
    if (!field_or_query(q, "mode", mode, sizeof(mode)) || blank(mode))
        snprintf(mode, sizeof(mode), "SUMMARIZATION");
    char* summary = gmb_enrich(text, mode);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "text", text);
    rb_adds(&b, ",");
    rb_open_kv(&b, "mode", mode);
    rb_adds(&b, ",");
    rb_open_kv(&b, "summary", summary ? summary : "");
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (summary) free(summary);
}

/* ============================================================================
 * Route webapp — /api/workflows*
 * ========================================================================== */
static void h_api_workflows_coll(const http_req_t* q, http_res_t* r)
{
    if (is_post(q)) {
        char name[256] = "";
        if (!field_or_query(q, "name", name, sizeof(name)) || blank(name)) {
            res_err(r, 400, "Missing 'name'");
            return;
        }
        if (!gmb_workflow_begin_checked(name)) {
            char m[512];
            snprintf(m, sizeof(m), "Workflow '%s' gia' in definizione o esistente", name);
            res_err(r, 409, m);
            return;
        }
        gmb_workflow_end();
        char* js = gmb_workflow_summary_json(name);
        res_set(r, 201, js ? js : strdup("{}"));
        return;
    }
    char* js = gmb_workflow_list_json();
    res_set(r, 200, js ? js : strdup("[]"));
}

/* /api/workflows/{name} — GET dettaglio | DELETE */
static void h_api_workflow_one(const http_req_t* q, http_res_t* r, const char* name)
{
    if (is_delete(q)) {
        if (!gmb_workflow_delete(name)) {
            char m[512];
            snprintf(m, sizeof(m), "Workflow '%s' not found o in costruzione", name);
            res_err(r, 404, m);
            return;
        }
        rbuf_t b = {0};
        rb_adds(&b, "{");
        rb_open_kv(&b, "deleted", name);
        rb_adds(&b, "}");
        res_set(r, 200, rb_take(&b));
        return;
    }
    char* log = gmb_wf_log_json(name);
    if (!log) {
        char m[512];
        snprintf(m, sizeof(m), "Workflow '%s' not found", name);
        res_err(r, 404, m);
        return;
    }
    gmb_string_t st = gmb_workflow_status(name);
    gmb_string_t steps = gmb_workflow_steps(name);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "workflow", name);
    rb_adds(&b, ",");
    rb_open_kv(&b, "status", st.data ? st.data : "");
    rb_adds(&b, ",");
    rb_open_kv(&b, "steps", steps.data ? steps.data : "");
    rb_adds(&b, ",\"log\":");
    rb_adds(&b, log);
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    gmb_string_free(&st);
    gmb_string_free(&steps);
    free(log);
}

/* POST /api/workflows/{name}/steps */
static void h_api_workflow_steps(const http_req_t* q, http_res_t* r, const char* name)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    char step[256] = "", action[2048] = "";
    int retry = 0, timeout = 0;
    if (!field_or_query(q, "name", step, sizeof(step)) || blank(step))
        field_or_query(q, "step", step, sizeof(step));
    if (blank(step)) field_or_query(q, "name", step, sizeof(step));
    field_or_query(q, "action", action, sizeof(action));
    char nb[32] = "";
    if (json_field_raw(q->body, "retryCount", nb, sizeof(nb))) retry = atoi(nb);
    if (json_field_raw(q->body, "timeoutMs", nb, sizeof(nb))) timeout = atoi(nb);
    if (blank(step)) { res_err(r, 400, "Missing 'name' for step"); return; }
    if (!gmb_workflow_add_step_by_name(name, step, action, retry, timeout)) {
        char m[512];
        snprintf(m, sizeof(m), "Workflow '%s' not found o in costruzione", name);
        res_err(r, 404, m);
        return;
    }
    gmb_string_t st = gmb_workflow_status(name);
    char* steps = gmb_workflow_steps_detail_json(name);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "name", name);
    rb_adds(&b, ",");
    rb_open_kv(&b, "status", (st.data && *st.data) ? st.data : "ready");
    rb_adds(&b, ",\"steps\":");
    rb_adds(&b, steps ? steps : "[]");
    rb_adds(&b, "}");
    res_set(r, 201, rb_take(&b));
    gmb_string_free(&st);
    if (steps) free(steps);
}

/* POST /api/workflows/{name}/execute */
static void h_api_workflow_execute(const http_req_t* q, http_res_t* r, const char* name)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    if (!gmb_workflow_exists(name)) {
        char m[512];
        snprintf(m, sizeof(m), "Workflow '%s' not found", name);
        res_err(r, 404, m);
        return;
    }
    int ok = gmb_workflow_execute_ok(name);
    char* log = gmb_wf_log_json(name);
    gmb_string_t st = gmb_workflow_status(name);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "workflow", name);
    rb_addf(&b, ",\"ok\":%s,", ok ? "true" : "false");
    rb_open_kv(&b, "status", st.data ? st.data : "");
    rb_adds(&b, ",\"log\":");
    rb_adds(&b, log ? log : "[]");
    rb_adds(&b, "}");
    res_set(r, ok ? 200 : 500, rb_take(&b));
    gmb_string_free(&st);
    if (log) free(log);
}

/* ============================================================================
 * Route webapp — /api/agent/{n}/prompt, /api/team/{n}/broadcast
 * ========================================================================== */
static void h_api_agent_prompt(const http_req_t* q, http_res_t* r, const char* name)
{
    if (blank(name)) { res_err(r, 400, "Missing agent name"); return; }
    char prompt[4096] = "";
    if (!json_field_raw(q->body, "prompt", prompt, sizeof(prompt)) || blank(prompt)) {
        res_err(r, 400, "Missing 'prompt'");
        return;
    }
    char* ans = gmb_agent_ask(name, prompt);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "agent", name);
    rb_adds(&b, ",");
    rb_open_kv(&b, "answer", ans ? ans : "");
    rb_adds(&b, "}");
    res_set(r, 200, rb_take(&b));
    if (ans) free(ans);
}

static void h_api_team_broadcast(const http_req_t* q, http_res_t* r, const char* team)
{
    if (!is_post(q)) { res_err(r, 405, "POST required"); return; }
    if (blank(team)) { res_err(r, 400, "Missing team name"); return; }
    char msg[4096] = "";
    if (!field_or_query(q, "message", msg, sizeof(msg)) || blank(msg)) {
        res_err(r, 400, "Missing 'message'");
        return;
    }
    int ok = gmb_team_broadcast(team, msg);
    rbuf_t b = {0};
    rb_adds(&b, "{");
    rb_open_kv(&b, "team", team);
    rb_adds(&b, ",");
    rb_open_kv(&b, "message", msg);
    rb_addf(&b, ",\"ok\":%s}", ok > 0 ? "true" : "false");
    res_set(r, ok > 0 ? 200 : 404, rb_take(&b));
}

/* Route non portate: errore ESPLICITO, mai 404 silenzioso (regola no-stub). */
static void h_not_in_c_msg(http_res_t* r)
{
    res_err(r, 501, "route non implementata nel runtime C (manca l'aggregatore JSON: vedi Todo Webapp-HTTP-Surface-C-Port §2.1)");
}
static void h_not_in_c(const http_req_t* q, http_res_t* r)
{
    (void)q;
    h_not_in_c_msg(r);
}

/* ============================================================================
 * Tabella di route + dispatch
 * ========================================================================== */
typedef void (*route_fn_t)(const http_req_t*, http_res_t*);
typedef void (*route_arg_fn_t)(const http_req_t*, http_res_t*, const char*);

typedef struct { const char* method; const char* path; route_fn_t fn; } route_t;

static const route_t ROUTES[] = {
    { "POST", "chat",                    h_chat },
    { "POST", "execute",                 h_chat },
    { "*",    "memory",                  h_memory_agent },
    { "*",    "workflow",                h_workflow_legacy },
    { "*",    "api/memory",              h_api_memory },
    { "*",    "api/memory/search",       h_api_memory_search },
    { "*",    "api/brain/note",          h_api_brain_note },
    { "*",    "api/brain/save",          h_api_brain_save },
    { "*",    "api/brain/forget",        h_api_brain_forget },
    { "*",    "api/brain/import",        h_api_brain_import },
    { "*",    "api/brain/watch",         h_api_brain_watch },
    { "*",    "api/brain/refresh",       h_api_brain_refresh },
    { "*",    "api/brain/consolidate",   h_api_brain_consolidate },
    { "*",    "api/brain/list",          h_api_brain_list },
    { "*",    "api/brain/search",        h_api_brain_search },
    { "*",    "api/enrich",              h_api_enrich },
    { "*",    "api/workflows",           h_api_workflows_coll },
    { "GET",  "api/agents",              h_not_in_c },
    { "GET",  "api/teams",               h_not_in_c },
    { "GET",  "api/events",              h_not_in_c },
};

#define ROUTE_COUNT ((int)(sizeof(ROUTES) / sizeof(ROUTES[0])))

static int method_matches(const char* pat, const char* m)
{
    return strcmp(pat, "*") == 0 || strcmp(pat, m) == 0;
}

static void dispatch(const http_req_t* q, http_res_t* r)
{
    /* match esatto sulla tabella */
    for (int i = 0; i < ROUTE_COUNT; i++) {
        if (strcmp(ROUTES[i].path, q->path) == 0 && method_matches(ROUTES[i].method, q->method)) {
            ROUTES[i].fn(q, r);
            return;
        }
    }
    /* match prefissato: api/workflows/<name>[/steps|/execute] */
    static const char* WFP = "api/workflows/";
    size_t wl = strlen(WFP);
    if (strncmp(q->path, WFP, wl) == 0) {
        char rest[192] = "";
        snprintf(rest, sizeof(rest), "%s", q->path + wl);
        if (strcmp(rest, "agents") == 0 || strcmp(rest, "teams") == 0 || strcmp(rest, "events") == 0) {
            h_not_in_c_msg(r);
            return;
        }
        size_t rl = strlen(rest);
        if (rl > 6 && strcmp(rest + rl - 6, "/steps") == 0) {
            char nm[160] = "";
            snprintf(nm, sizeof(nm), "%.*s", (int)(rl - 6), rest);
            if (blank(nm)) { res_err(r, 400, "Missing workflow name"); return; }
            h_api_workflow_steps(q, r, nm);
            return;
        }
        if (rl > 8 && strcmp(rest + rl - 8, "/execute") == 0) {
            char nm[160] = "";
            snprintf(nm, sizeof(nm), "%.*s", (int)(rl - 8), rest);
            if (blank(nm)) { res_err(r, 400, "Missing workflow name"); return; }
            h_api_workflow_execute(q, r, nm);
            return;
        }
        if (!blank(rest)) { h_api_workflow_one(q, r, rest); return; }
        res_err(r, 400, "Missing workflow name");
        return;
    }
    /* api/agent/<name>/prompt */
    static const char* AGP = "api/agent/";
    if (strncmp(q->path, AGP, strlen(AGP)) == 0) {
        const char* rest = q->path + strlen(AGP);
        size_t rl = strlen(rest);
        if (rl > 7 && strcmp(rest + rl - 7, "/prompt") == 0) {
            char nm[160] = "";
            snprintf(nm, sizeof(nm), "%.*s", (int)(rl - 7), rest);
            h_api_agent_prompt(q, r, nm);
            return;
        }
        res_err(r, 404, "route not found");
        return;
    }
    /* api/team/<name>/broadcast */
    static const char* TBP = "api/team/";
    if (strncmp(q->path, TBP, strlen(TBP)) == 0) {
        const char* rest = q->path + strlen(TBP);
        size_t rl = strlen(rest);
        if (rl > 10 && strcmp(rest + rl - 10, "/broadcast") == 0) {
            char nm[160] = "";
            snprintf(nm, sizeof(nm), "%.*s", (int)(rl - 10), rest);
            h_api_team_broadcast(q, r, nm);
            return;
        }
        res_err(r, 404, "route not found");
        return;
    }
    /* api/teams/<name> → non portata */
    static const char* TEAMS = "api/teams/";
    if (strncmp(q->path, TEAMS, strlen(TEAMS)) == 0) { h_not_in_c_msg(r); return; }

    res_err(r, 404, "route not found");
}

/* ============================================================================
 * Server
 * ========================================================================== */
static void req_init(http_req_t* q, const char* raw)
{
    memset(q, 0, sizeof(*q));
    const char* sp = raw;
    int k = sscanf(raw, "%15s %255s", q->method, q->path);
    (void)sp;
    if (k < 1) { snprintf(q->method, sizeof(q->method), "GET"); }
    if (k < 2) { snprintf(q->path, sizeof(q->path), "/"); }
    char* qm = strchr(q->path, '?');
    if (qm) {
        snprintf(q->query, sizeof(q->query), "%s", qm + 1);
        *qm = '\0';
    }
    if (q->path[0] == '/') memmove(q->path, q->path + 1, strlen(q->path));
    const char* b = strstr(raw, "\r\n\r\n");
    if (b) {
        b += 4;
        size_t bl = strnlen(b, 65535);
        q->body = (char*)malloc(bl + 1);
        if (q->body) { memcpy(q->body, b, bl); q->body[bl] = '\0'; }
    }
}

/* serve una singola richiesta: ritorna una stringa heap (risposta completa) o NULL. */
static char* serve_request(const char* req)
{
    http_req_t q;
    req_init(&q, req);
    http_res_t r = { 200, NULL };
    dispatch(&q, &r);
    free(q.body);
    const char* body = r.json ? r.json : "{\"error\":\"no response\"}";
    size_t bl = strlen(body);
    const char* reason = (r.status == 200) ? "OK" :
                         (r.status == 400) ? "Bad Request" :
                         (r.status == 404) ? "Not Found" :
                         (r.status == 405) ? "Method Not Allowed" :
                         (r.status == 409) ? "Conflict" :
                         (r.status == 500) ? "Internal Server Error" :
                         (r.status == 501) ? "Not Implemented" : "OK";
    char* out = (char*)malloc(bl + 256);
    if (!out) { free(r.json); return NULL; }
    snprintf(out, bl + 256,
             "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
             r.status, reason, bl, body);
    free(r.json);                 /* DOPO l'uso: `body` punta in r.json */
    return out;
}

static void* http_thread_fn(void* arg)
{
    (void)arg;
    while (g_http_running) {
        int64_t cli = gmb_net_tcp_accept(g_http_listener);
        if (cli < 0) {
            if (!g_http_running) break;
            struct timespec ts = { 0, 20000000 }; nanosleep(&ts, NULL);
            continue;
        }
        char buf[16384];
        int got = 0, waited = 0;
        while (g_http_running && waited < 50) {
            got = gmb_net_tcp_recv(cli, (uint8_t*)buf, sizeof(buf) - 1);
            if (got > 0) break;
            struct timespec ts = { 0, 40000000 }; nanosleep(&ts, NULL);
            waited++;
        }
        if (got > 0) {
            buf[got] = '\0';
            char* resp = serve_request(buf);
            if (resp) {
                gmb_net_tcp_send(cli, (const uint8_t*)resp, strlen(resp));
                free(resp);
            }
        }
        gmb_net_tcp_close(cli);
    }
    return NULL;
}

int gmb_agent_http_start(int port)
{
    if (g_http_running) return 1;
    gmb_net_init();
    int64_t l = gmb_net_tcp_listen("127.0.0.1", port);
    if (l < 0) l = gmb_net_tcp_listen("", port);
    if (l < 0) return 0;
    g_http_listener = l;
    g_http_running = 1;
    if (pthread_create(&g_http_thread, NULL, http_thread_fn, NULL) != 0) {
        g_http_running = 0;
        gmb_net_tcp_close(l);
        g_http_listener = -1;
        return 0;
    }
    return 1;
}

int gmb_agent_http_stop(void)
{
    if (!g_http_running) return 1;
    g_http_running = 0;
    if (g_http_listener >= 0) { gmb_net_tcp_close(g_http_listener); g_http_listener = -1; }
    pthread_join(g_http_thread, NULL);
    return 1;
}
