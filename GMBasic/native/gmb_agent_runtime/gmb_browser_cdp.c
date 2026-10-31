/* gmb_browser_cdp.c — CDP client nativo C per la VM/C (BROWSER.* 1375-1379).
 * Usa il WS client del runtime condiviso (gmb_ws_connect/send/recv) + HTTP GET
 * (gmb_net_http_get) per /json/list e /json/version. Nessuna logica duplicata nei
 * backend (§0bis): il client vive qui nel runtime condiviso gmb_agent_runtime.
 *
 * Modelli supportati:
 *  - Chromium: comandi direttamente sul ws del page target (fast-path).
 *  - Obscura: non accetta comandi sul ws del page target ("No page"); upgrade lazy
 *    al flat-session sul browser ws (Target.createTarget + attachToTarget flatten).
 * Un file/dominio, <1200 righe. */
#include "gmb_browser_cdp.h"
#include "gmb_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

struct gmb_cdp_s {
    int64_t ws;               /* handle WS attivo (page ws o browser ws flat) */
    int flat;                 /* 1 = flat-session attiva su browser ws */
    long next_id;
    char session_id[128];     /* sessionId della flat-session (se flat) */
    char page_ws_path[256];   /* path del page ws (/devtools/page/...) */
    char browser_ws_path[256];/* path del browser ws (/devtools/browser/...) */
    char host[64];            /* host usato (127.0.0.1 o ::1) */
    int port;
    char last_err[512];
};

/* ---- minimo scanner JSON (abbastanza per gli envelope CDP) ---- */
static int json_isspace_c(char c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }

static const char* json_skip_str(const char* p) {
    /* p punta al '"' iniziale */
    p++;
    while (*p) {
        if (*p=='\\') { p += 2; continue; }
        if (*p=='"') return p+1;
        p++;
    }
    return p;
}

/* ritorna il puntatore dopo il valore JSON che inizia a v (gestisce stringhe/oggetti/array/primitivi) */
static const char* json_value_end(const char* v) {
    if (!v) return v;
    if (*v=='"') return json_skip_str(v);
    if (*v=='{' || *v=='[') {
        int depth = 0; int in_str = 0; const char* p = v;
        for (; *p; p++) {
            char c = *p;
            if (in_str) { if (c=='\\') { p++; continue; } if (c=='"') in_str=0; continue; }
            if (c=='"') { in_str=1; continue; }
            if (c=='{' || c=='[') depth++;
            else if (c=='}' || c==']') { depth--; if (depth==0) return p+1; }
        }
        return p;
    }
    while (*v && !json_isspace_c(*v) && *v != ',' && *v != '}' && *v != ']') v++;
    return v;
}

/* trova la prima occorrenza di "key" (non dentro stringhe) seguita da ':'.
   setta *start e *end ai bordi del valore (bilanciato). Ritorna 1 se trovato. */
static int json_find_value(const char* s, const char* key, const char** start, const char** end) {
    const char* p = s;
    size_t klen = strlen(key);
    int in_str = 0;
    if (!s || !*s) return 0;
    for (; *p; ) {
        char c = *p;
        if (in_str) { if (c=='\\') { p += 2; continue; } if (c=='"') in_str = 0; p++; continue; }
        if (c=='"') {
            const char* tok = p + 1;
            const char* q = tok;
            while (*q && *q!='"') { if (*q=='\\') q++; q++; }
            size_t tlen = (size_t)(q - tok);
            if (tlen == klen && strncmp(tok, key, klen) == 0) {
                const char* r = q + 1;
                while (json_isspace_c(*r)) r++;
                if (*r == ':') {
                    const char* v = r + 1;
                    while (json_isspace_c(*v)) v++;
                    if (start) *start = v;
                    if (end) *end = json_value_end(v);
                    return 1;
                }
            }
            p = q + 1;
            continue;
        }
        p++;
    }
    return 0;
}

/* estrae il valore JSON grezzo (heap) della prima occorrenza di key; NULL se assente */
static char* json_raw_field(const char* s, const char* key) {
    const char *start, *end;
    if (!json_find_value(s, key, &start, &end)) return NULL;
    size_t len = (size_t)(end - start);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}

/* legge il valore come intero della prima occorrenza di key (-1 se assente) */
static long json_int_field(const char* s, const char* key) {
    const char *start, *end;
    if (!json_find_value(s, key, &start, &end)) return -1;
    long v = 0; int neg = 0; const char* p = start;
    if (*p=='"') p++;
    if (*p=='-') { neg = 1; p++; }
    while (p < end && isdigit((unsigned char)*p)) { v = v*10 + (*p - '0'); p++; }
    return neg ? -v : v;
}

/* legge il valore come stringa (unescape minimo) della prima occorrenza di key; ritorna 0 ok */
static int json_str_field(const char* s, const char* key, char* out, size_t cap) {
    const char *start, *end;
    if (!json_find_value(s, key, &start, &end)) return -1;
    const char* p = start;
    if (p < end && *p == '"') p++;
    const char* e = end;
    if (e > start && e[-1] == '"') e--;
    size_t n = 0;
    while (p < e && n + 1 < cap) {
        if (*p=='\\' && p+1 < e) {
            p++;
            switch (*p) {
                case 'n': out[n++] = '\n'; break;
                case 't': out[n++] = '\t'; break;
                case 'r': out[n++] = '\r'; break;
                case '"': out[n++] = '"'; break;
                case '\\': out[n++] = '\\'; break;
                case 'u': { p += 4; /* skip utf-16 hex: gestione best-effort */ break; }
                default: out[n++] = *p; break;
            }
            p++;
        } else out[n++] = *p++;
    }
    out[n] = 0;
    return 0;
}

/* ---- helper connessione ---- */
/* Budget per UN comando CDP (ms). Prima il loop di ricezione poteva attendere 40×3 s = 120 s e la
 * connect 30 s: con un CDP in ascolto ma silenzioso il comando BLOCCAVA il run fino al kill
 * dell'harness (misurato in AI OBSERVE BROWSER, 2026-09-16). Ora è bounded e configurabile. */
static int64_t cdp_cmd_timeout_ms(void)
{
    const char* env = getenv("GMBASIC_CDP_TIMEOUT_MS");
    int64_t v = (env && *env) ? atoll(env) : 5000;
    if (v <= 0) v = 5000;
    if (v > 60000) v = 60000;
    return v;
}

static int ws_send_recv(int64_t ws, const char* json, long expect_id, char** out_frame)
{
    *out_frame = NULL;
    if (ws <= 0) return -1;
    int sr = gmb_ws_send_text(ws, json);
    if (sr < 0) return -1;
    /* legge frame finché non trova la risposta con id==expect_id, entro UN budget totale
     * (deadline assoluta): ogni recv riceve il tempo RESIDUO, mai una finestra fissa × N. */
    int64_t budget = cdp_cmd_timeout_ms();
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t used = (now.tv_sec - t0.tv_sec) * 1000 + (now.tv_nsec - t0.tv_nsec) / 1000000;
        int64_t remaining = budget - used;
        if (remaining <= 0) return -1;
        int opcode = 0;
        char* payload = NULL;
        size_t plen = 0;
        int rr = gmb_ws_recv_frame(ws, &opcode, &payload, &plen, remaining);
        if (rr != 0) {
            if (payload) free(payload);
            return -1;
        }
        if (opcode == GMB_WS_TEXT || opcode == GMB_WS_BINARY) {
            long got = json_int_field(payload ? payload : "", "id");
            if (got == expect_id) {
                *out_frame = payload ? payload : strdup("");
                return 0;
            }
        }
        if (opcode == GMB_WS_CLOSE) { if (payload) free(payload); return -1; }
        if (payload) free(payload);
    }
    return -1;
}

static char* http_get_cdp(const char* base, const char* path) {
    /* base es. "http://127.0.0.1:9222" */
    size_t need = strlen(base) + strlen(path) + 1;
    char* url = (char*)malloc(need);
    if (!url) return NULL;
    snprintf(url, need, "%s%s", base, path);
    char* body = gmb_net_http_get(url);
    free(url);
    return body;
}

static void cdp_set_err(gmb_cdp_t* c, const char* fmt, const char* arg) {
    if (!c) return;
    snprintf(c->last_err, sizeof(c->last_err), fmt, arg ? arg : "");
}

/* apre un ws verso ws_url_path sull'host/port del contesto */
static int64_t cdp_ws_open(gmb_cdp_t* c, const char* path);
/* upgrade alla flat-session su browser ws (Obscura) — definito dopo cdp_ws_open */
static int cdp_flat_upgrade(gmb_cdp_t* c);

static int64_t cdp_ws_open(gmb_cdp_t* c, const char* path) {
    if (!path || !*path) return -1;
    int64_t h = gmb_ws_connect(c->host, c->port, path, (int)cdp_cmd_timeout_ms());
    if (h > 0) return h;
    /* retry sull'altro loopback (es. Chromium solo [::1]) */
    const char* alt = (strcmp(c->host, "127.0.0.1") == 0) ? "::1" : "127.0.0.1";
    h = gmb_ws_connect(alt, c->port, path, (int)cdp_cmd_timeout_ms());
    if (h > 0) { snprintf(c->host, sizeof(c->host), "%s", alt); return h; }
    return -1;
}

static int cdp_extract_ws_path(const char* ws_url, char* out, size_t cap) {
    /* ws://host:port/path... → /path... */
    if (!ws_url) return -1;
    const char* p = strstr(ws_url, "://");
    if (!p) return -1;
    p += 3;
    const char* slash = strchr(p, '/');
    if (!slash) return -1;
    snprintf(out, cap, "%s", slash);
    return 0;
}

static int cdp_page_setup(gmb_cdp_t* c) {
    char base[96];
    snprintf(base, sizeof(base), "http://%s:%d", c->host, c->port);
    char* list = http_get_cdp(base, "/json/list");
    if (list) {
        char ws[512] = "";
        if (json_str_field(list, "webSocketDebuggerUrl", ws, sizeof(ws)) == 0)
            cdp_extract_ws_path(ws, c->page_ws_path, sizeof(c->page_ws_path));
        free(list);
    }
    char* ver = http_get_cdp(base, "/json/version");
    if (ver) {
        char ws[512] = "";
        if (json_str_field(ver, "webSocketDebuggerUrl", ws, sizeof(ws)) == 0)
            cdp_extract_ws_path(ws, c->browser_ws_path, sizeof(c->browser_ws_path));
        free(ver);
    }
    if (!c->page_ws_path[0] && !c->browser_ws_path[0]) return -1;
    if (c->page_ws_path[0]) {
        int64_t h = cdp_ws_open(c, c->page_ws_path);
        if (h > 0) { c->ws = h; c->flat = 0; return 0; }
    }
    /* niente page ws raggiungibile → prova flat diretto sul browser ws (Obscura) */
    if (c->browser_ws_path[0] && cdp_flat_upgrade(c) == 0) return 0;
    return -1;
}

/* upgrade alla flat-session su browser ws (Obscura): attach del page target + flatten.
   Il targetId si deriva dal path del page ws (ultimo segmento: /devtools/page/<id>),
   identico per Obscura ("page-1") e Chromium (UUID). */
static int cdp_flat_upgrade(gmb_cdp_t* c) {
    if (!c->browser_ws_path[0]) return -1;
    int64_t bws = cdp_ws_open(c, c->browser_ws_path);
    if (bws <= 0) return -1;

    /* targetId = ultimo segmento del page ws path */
    char target_id[128] = "";
    const char* last = strrchr(c->page_ws_path, '/');
    if (last && last[1]) snprintf(target_id, sizeof(target_id), "%s", last + 1);

    char sid[128] = "";
    if (target_id[0]) {
        long id = ++c->next_id;
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "{\"id\":%ld,\"method\":\"Target.attachToTarget\",\"params\":{\"targetId\":\"%s\",\"flatten\":true}}",
            id, target_id);
        char* frame = NULL;
        if (ws_send_recv(bws, cmd, id, &frame) == 0) {
            json_str_field(frame ? frame : "", "sessionId", sid, sizeof(sid));
            if (frame) free(frame);
        }
    }
    if (!sid[0]) {
        /* fallback: crea un nuovo target page e attacha */
        long id = ++c->next_id;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "{\"id\":%ld,\"method\":\"Target.createTarget\",\"params\":{\"url\":\"about:blank\"}}", id);
        char* frame = NULL;
        if (ws_send_recv(bws, cmd, id, &frame) == 0) {
            char* raw_tid = json_raw_field(frame ? frame : "", "targetId"); /* include le virgolette */
            if (frame) free(frame);
            if (raw_tid) {
                id = ++c->next_id;
                snprintf(cmd, sizeof(cmd),
                    "{\"id\":%ld,\"method\":\"Target.attachToTarget\",\"params\":{\"targetId\":%s,\"flatten\":true}}",
                    id, raw_tid);
                free(raw_tid);
                frame = NULL;
                if (ws_send_recv(bws, cmd, id, &frame) == 0) {
                    json_str_field(frame ? frame : "", "sessionId", sid, sizeof(sid));
                    if (frame) free(frame);
                }
            }
        }
    }
    if (!sid[0]) { gmb_ws_close(bws); return -1; }

    /* sostituisci il socket attuale col browser ws flat */
    if (c->ws > 0) gmb_ws_close(c->ws);
    c->ws = bws;
    c->flat = 1;
    snprintf(c->session_id, sizeof(c->session_id), "%s", sid);
    return 0;
}

gmb_cdp_t* gmb_cdp_open(int port) {
    gmb_cdp_t* c = (gmb_cdp_t*)calloc(1, sizeof(gmb_cdp_t));
    if (!c) return NULL;
    c->port = port;
    c->next_id = 100;
    snprintf(c->host, sizeof(c->host), "127.0.0.1");
    if (cdp_page_setup(c) != 0 || c->ws <= 0) {
        gmb_cdp_close(c);
        return NULL;
    }
    /* stealth / anti-detection (funzioni avanzate Camoufox via CDP puro): lo script gira prima di
       ogni script di pagina sulle navigazioni successive. Parità con il client managed che lo applica
       alla connessione. Best-effort: se il target non lo accetta non blocca. */
    {
        const char* js =
            "(() => { try { const spoof=(o,p,v)=>{try{Object.defineProperty(o,p,{get:()=>v,configurable:true});}catch(e){}};"
            "try{Object.defineProperty(Navigator.prototype,'webdriver',{get:()=>undefined,configurable:true});}catch(e){}"
            "spoof(navigator,'webdriver',undefined);spoof(navigator,'hardwareConcurrency',8);"
            "spoof(navigator,'deviceMemory',8);spoof(navigator,'maxTouchPoints',0);"
            "spoof(navigator,'language','en-US');spoof(navigator,'languages',['en-US','en']);"
            "}catch(e){} })();";
        char esc[8192];
        gmb_cdp_json_escape(js, esc, sizeof(esc));
        char sparams[8300];
        snprintf(sparams, sizeof(sparams), "{\"source\":\"%s\"}", esc);
        char* sout = NULL;
        gmb_cdp_command(c, "Page.addScriptToEvaluateOnNewDocument", sparams, &sout);
        if (sout) free(sout);
    }
    return c;
}

int gmb_cdp_command(gmb_cdp_t* c, const char* method, const char* params_json, char** out) {
    if (out) *out = NULL;
    if (!c || !method) return -1;
    const char* params = (params_json && *params_json) ? params_json : "{}";
    long id = ++c->next_id;
    char* cmd = NULL;
    if (c->flat) {
        size_t need = strlen(method) + strlen(params) + strlen(c->session_id) + 96;
        cmd = (char*)malloc(need);
        if (!cmd) return -1;
        snprintf(cmd, need, "{\"id\":%ld,\"method\":\"%s\",\"params\":%s,\"sessionId\":\"%s\"}", id, method, params, c->session_id);
    } else {
        size_t need = strlen(method) + strlen(params) + 64;
        cmd = (char*)malloc(need);
        if (!cmd) return -1;
        snprintf(cmd, need, "{\"id\":%ld,\"method\":\"%s\",\"params\":%s}", id, method, params);
    }
    char* frame = NULL;
    int rc = ws_send_recv(c->ws, cmd, id, &frame);
    free(cmd);
    if (rc != 0) {
        snprintf(c->last_err, sizeof(c->last_err), "CDP trasporto: nessuna risposta a %s", method);
        if (frame) free(frame);
        return -1;
    }
    /* errore CDP esplicito */
    if (json_find_value(frame ? frame : "", "error", NULL, NULL)) {
        char* emsg = json_raw_field(frame ? frame : "", "message");
        if (!c->flat && c->browser_ws_path[0]) {
            /* probabile Obscura: "No page" sul page ws → upgrade flat e retry una volta */
            if (frame) free(frame);
            if (cdp_flat_upgrade(c) == 0) return gmb_cdp_command(c, method, params_json, out);
            snprintf(c->last_err, sizeof(c->last_err), "CDP errore: %s", emsg ? emsg : "?");
            if (emsg) free(emsg);
            return -1;
        }
        snprintf(c->last_err, sizeof(c->last_err), "CDP errore: %s", emsg ? emsg : "?");
        if (emsg) free(emsg);
        if (frame) free(frame);
        return -1;
    }
    if (out) {
        char* res = frame ? json_raw_field(frame, "result") : NULL;
        if (res) { *out = res; }
        else { *out = strdup("{}"); }
    }
    if (frame) free(frame);
    return 0;
}

const char* gmb_cdp_last_error(gmb_cdp_t* c) { return c ? c->last_err : ""; }

int gmb_cdp_json_get_str(const char* js, const char* key, char* out, size_t cap) {
    return json_str_field(js ? js : "", key, out, cap);
}
int gmb_cdp_json_has(const char* js, const char* key) {
    return json_find_value(js ? js : "", key, NULL, NULL) ? 1 : 0;
}

gmb_cdp_t* gmb_cdp_singleton(void) {
    static gmb_cdp_t* cache = NULL;
    static int port = 0;
    const char* env = getenv("GMBASIC_CDP_PORT");
    int want = (env && *env) ? atoi(env) : 9222;
    if (want <= 0) want = 9222;
    if (cache && port == want) return cache;
    if (cache) { gmb_cdp_close(cache); cache = NULL; }
    cache = gmb_cdp_open(want);
    port = cache ? want : 0;
    return cache;
}

void gmb_cdp_close(gmb_cdp_t* c) {
    if (!c) return;
    if (c->ws > 0) gmb_ws_close(c->ws);
    free(c);
}

size_t gmb_cdp_json_escape(const char* in, char* out, size_t cap) {
    if (!in) in = "";
    size_t n = 0;
    for (const char* p = in; *p && n + 6 < cap; p++) {
        unsigned char ch = (unsigned char)*p;
        switch (ch) {
            case '"':  out[n++] = '\\'; out[n++] = '"'; break;
            case '\\': out[n++] = '\\'; out[n++] = '\\'; break;
            case '\n': out[n++] = '\\'; out[n++] = 'n'; break;
            case '\r': out[n++] = '\\'; out[n++] = 'r'; break;
            case '\t': out[n++] = '\\'; out[n++] = 't'; break;
            default:
                if (ch < 0x20) {
                    if (n + 6 >= cap) break;
                    n += (size_t)snprintf(out + n, cap - n, "\\u%04x", ch);
                } else out[n++] = (char)ch;
                break;
        }
    }
    if (cap) out[n < cap ? n : (cap - 1)] = 0;
    return n;
}

char* gmb_cdp_evaluate_value(const char* evaluate_result_json) {
    /* evaluate_result_json è il "result" del comando: {"result":{"type":...,"value":...}}.
       Parità C#: ritorna il valore interno come testo (stringa unescaped), altrimenti "null". */
    if (!evaluate_result_json) return strdup("null");
    const char* vstart, *vend;
    if (json_find_value(evaluate_result_json, "value", &vstart, &vend)) {
        const char* s = vstart; const char* e = vend;
        int is_str = 0;
        if (e > s && *s == '"' && e[-1] == '"') { s++; e--; is_str = 1; }
        size_t len = (size_t)(e - s);
        char* out = (char*)malloc(len + 1);
        if (!out) return strdup("null");
        if (is_str) {
            size_t n = 0;
            const char* p = s;
            while (p < e && n < len) {
                if (*p == '\\' && p + 1 < e) {
                    p++;
                    switch (*p) {
                        case 'n': out[n++] = '\n'; break;
                        case 't': out[n++] = '\t'; break;
                        case 'r': out[n++] = '\r'; break;
                        case '"': out[n++] = '"'; break;
                        case '\\': out[n++] = '\\'; break;
                        default: out[n++] = *p; break;
                    }
                    p++;
                } else out[n++] = *p++;
            }
            out[n] = 0;
        } else {
            memcpy(out, s, len);
            out[len] = 0;
        }
        return out;
    }
    return strdup("null");
}
