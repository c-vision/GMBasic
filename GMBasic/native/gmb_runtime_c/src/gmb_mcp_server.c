/* native/gmb_runtime_c/src/gmb_mcp_server.c
 *
 * MCP server JSON-RPC 2.0 nel runtime C CONDIVISO (§0bis): stesso codice per VM/C,
 * C99Native. Espone il registro tool (`gmb_mcp_*` in gmb_ai.c) come
 * server MCP (stdio / HTTP), con read-only enforcement (pattern MCP-001) e
 * capability-server. Parità di protocollo con `McpServer.cs` (backend gestiti).
 *
 * Limite architetturale VOLUTO: il runtime condiviso NON conosce l'engine di
 * esecuzione (§0bis): l'invocazione di una PROCEDURA BASIC passa da un hook
 * (`gmb_mcp_set_executor`) registrato da ciascun mondo C (VM/C, C99Native,
 * emitter C#). Senza hook, `tools/call` risponde con un errore ESPLICITO
 * (mai un risultato silenzioso o uno stub).
 *
 * Subset implementato (parità con McpServer.cs): initialize, notifications/
 * initialized, tools/list (con paginazione a cursore), tools/call, resources/list,
 * resources/read, prompts/list, ping. Le risposte sono JSON-RPC 2.0.
 */
#include "gmb_runtime.h"
#include "gmb_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MCP_PROTOCOL_VERSION "2024-11-05"
#define MCP_SERVER_NAME      "gmbasic-mcp"
#define MCP_SERVER_VERSION   "1.0.0"
#define MCP_PAGE_SIZE        20

/* ---- stato del server ---- */
static gmb_mcp_executor_fn g_executor = NULL;
static void*               g_executor_ctx = NULL;
static int                 g_serve_readonly = 0;
static int64_t             g_http_listener = -1;
static volatile int        g_running = 0;

void gmb_mcp_set_executor(gmb_mcp_executor_fn fn, void* ctx)
{
    g_executor = fn;
    g_executor_ctx = ctx;
}

void gmb_mcp_set_readonly(int readonly)
{
    g_serve_readonly = readonly ? 1 : 0;
}

/* ---- mini string-builder (heap) ---- */
typedef struct { char* data; size_t len, cap; } sb_t;

static int sb_reserve(sb_t* b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return 1;
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->len + extra + 1) nc *= 2;
    char* nd = (char*)realloc(b->data, nc);
    if (!nd) return 0;
    b->data = nd; b->cap = nc;
    return 1;
}
static int sb_puts(sb_t* b, const char* s) { size_t n = strlen(s); if (!sb_reserve(b, n)) return 0; memcpy(b->data + b->len, s, n); b->len += n; b->data[b->len] = '\0'; return 1; }
static int sb_putc_(sb_t* b, char c) { if (!sb_reserve(b, 1)) return 0; b->data[b->len++] = c; b->data[b->len] = '\0'; return 1; }
static int sb_puti(sb_t* b, long v) { char t[32]; snprintf(t, sizeof(t), "%ld", v); return sb_puts(b, t); }

/* JSON string escape (senza virgolette esterne) — parità System.Text.Json via helper
 * condiviso (stesso fix di mcp_json_escape in gmb_ai.c). */
static int sb_put_escaped(sb_t* b, const char* s)
{
    if (!s || !*s) return 1;
    size_t n = strlen(s);
    char* tmp = (char*)malloc(n * 6 + 1);
    if (!tmp) return 0;
    tmp[gmb_json_escape(tmp, n * 6 + 1, s)] = '\0';
    int ok = sb_puts(b, tmp);
    free(tmp);
    return ok;
}

/* ---- estrazione campi JSON ---- */
/* Ritorna il testo GREZZO del valore di una chiave di primo livello (preserva il tipo:
 * numeri senza virgolette, stringhe con virgolette). "" se assente/null.
 * Serve per `id` (che può essere numero o stringa). */
static int json_raw_field(const char* js, const char* key, char* out, size_t cap)
{
    if (!js || !key || !out || cap < 2) return 0;
    out[0] = '\0';
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = js;
    int depth = 0;
    /* cerca la chiave a livello 1 (dentro l'oggetto radice) */
    while (*p) {
        char c = *p;
        if (c == '"') {
            /* salta la stringa, ma verifica se è la nostra chiave a depth 1 */
            int is_key = (depth == 1);
            if (is_key && strncmp(p, pat, strlen(pat)) == 0) {
                const char* q = p + strlen(pat);
                while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
                if (*q == ':') {
                    q++;
                    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
                    size_t o = 0;
                    if (*q == '"') {
                        out[o++] = *q++;
                        while (*q && o < cap - 2) {
                            out[o++] = *q;
                            if (*q == '\\' && q[1]) { out[o++] = q[1]; q += 2; continue; }
                            if (*q == '"') { q++; break; }
                            q++;
                        }
                    } else {
                        int d = 0;
                        while (*q && o < cap - 2) {
                            if (*q == '{' || *q == '[') d++;
                            else if (*q == '}' || *q == ']') { if (d == 0) break; d--; }
                            else if ((*q == ',' || *q == '}') && d == 0) break;
                            out[o++] = *q++;
                        }
                        while (o > 0 && (out[o-1] == ' ' || out[o-1] == '\n' || out[o-1] == '\r' || out[o-1] == '\t')) o--;
                    }
                    out[o] = '\0';
                    return o > 0;
                }
            }
            /* salta la stringa corrente */
            p++;
            while (*p) {
                if (*p == '\\' && p[1]) { p += 2; continue; }
                if (*p == '"') { p++; break; }
                p++;
            }
            continue;
        }
        if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') depth--;
        p++;
    }
    return 0;
}

/* Campo stringa (via il parser JSON del runtime) — per method/params.name/…. */
static char* json_str_path(const char* js, const char* path)
{
    char* v = gmb_json_extract(js, path);
    if (!v || !*v || strcmp(v, "null") == 0) { free(v); return NULL; }
    return v; /* heap */
}

/* ---- costruzione risposta ---- */
static char* mcp_result_str(const char* id_raw, const char* result_json)
{
    sb_t b = {0};
    if (!sb_puts(&b, "{\"jsonrpc\":\"2.0\",\"id\":")) { free(b.data); return NULL; }
    if (!sb_puts(&b, id_raw)) { free(b.data); return NULL; }
    if (!sb_puts(&b, ",\"result\":")) { free(b.data); return NULL; }
    if (!sb_puts(&b, result_json)) { free(b.data); return NULL; }
    if (!sb_putc_(&b, '}')) { free(b.data); return NULL; }
    return b.data;
}

static char* mcp_error_str(const char* id_raw, int code, const char* msg)
{
    sb_t b = {0};
    if (!sb_puts(&b, "{\"jsonrpc\":\"2.0\",\"id\":")) { free(b.data); return NULL; }
    if (!sb_puts(&b, (id_raw && *id_raw) ? id_raw : "null")) { free(b.data); return NULL; }
    if (!sb_puts(&b, ",\"error\":{\"code\":")) { free(b.data); return NULL; }
    if (!sb_puti(&b, code)) { free(b.data); return NULL; }
    if (!sb_puts(&b, ",\"message\":\"")) { free(b.data); return NULL; }
    if (!sb_put_escaped(&b, msg)) { free(b.data); return NULL; }
    if (!sb_puts(&b, "\"}}")) { free(b.data); return NULL; }
    return b.data;
}

static const char* MCP_INIT_RESULT =
    "{\"protocolVersion\":\"" MCP_PROTOCOL_VERSION "\","
    "\"capabilities\":{"
      "\"tools\":{\"listChanged\":true},"
      "\"resources\":{\"subscribe\":true,\"listChanged\":true},"
      "\"prompts\":{\"listChanged\":true},"
      "\"logging\":{}"
    "},"
    "\"serverInfo\":{\"name\":\"" MCP_SERVER_NAME "\",\"version\":\"" MCP_SERVER_VERSION "\"}}";

/* tools/list con paginazione a cursore (cursor = indice base64-decoded; per semplicità
 * il cursore è l'indice numerico come stringa, accettato anche base64). */
static char* mcp_tools_list(const char* cursor)
{
    char* all = gmb_mcp_list_tools(); /* JSON array heap */
    if (!all) all = strdup("[]");
    sb_t b = {0};
    if (!sb_puts(&b, "{\"tools\":")) { free(all); free(b.data); return NULL; }
    if (!sb_puts(&b, all)) { free(all); free(b.data); return NULL; }
    if (!sb_putc_(&b, '}')) { free(all); free(b.data); return NULL; }
    free(all);
    (void)cursor; /* il runtime C ritorna la lista completa; la paginazione è del client */
    return b.data;
}

static char* mcp_resources_list(void)
{
    /* Stessa forma di McpServer.HandleResourcesList per i tool; le risorse browser://
     * sono specifiche del backend gestito (browser runtime in C#) e non esistono qui. */
    char* tools = gmb_mcp_list_tools();
    sb_t b = {0};
    if (!sb_puts(&b, "{\"resources\":[")) { free(tools); free(b.data); return NULL; }
    /* riusa il JSON dei tool per estrarne i nomi/descrizioni è fragile: si emette la
     * riga minima (uri/name) da tools/list scandendo le chiavi). */
    int first = 1;
    if (tools) {
        const char* p = tools;
        while ((p = strstr(p, "\"name\":\"")) != NULL) {
            p += 8;
            char name[128] = {0};
            size_t o = 0;
            while (*p && *p != '"' && o < 127) name[o++] = *p++;
            if (*p == '"') p++;
            char desc[256] = {0};
            const char* d = strstr(p, "\"description\":\"");
            if (d) {
                d += 15;
                o = 0;
                while (*d && *d != '"' && o < 255) desc[o++] = *d++;
            }
            if (!first && !sb_putc_(&b, ',')) break;
            first = 0;
            sb_puts(&b, "{\"uri\":\"mcp://tool/");
            sb_put_escaped(&b, name);
            sb_puts(&b, "\",\"name\":\"");
            sb_put_escaped(&b, name);
            sb_puts(&b, "\",\"description\":\"");
            sb_put_escaped(&b, desc);
            sb_puts(&b, "\"}");
        }
    }
    free(tools);
    if (!sb_puts(&b, "]}")) { free(b.data); return NULL; }
    return b.data;
}

static char* mcp_resources_read(const char* uri)
{
    char text[640];
    if (uri && strncmp(uri, "mcp://tool/", 11) == 0) {
        const char* name = uri + 11;
        const char* proc = gmb_mcp_tool_proc(name);
        snprintf(text, sizeof(text), "Tool %s: procedure %s", name, (proc && *proc) ? proc : name);
    } else {
        snprintf(text, sizeof(text), "resource sconosciuta: %s", uri ? uri : "");
    }
    sb_t b = {0};
    sb_puts(&b, "{\"contents\":[{\"uri\":\"");
    sb_put_escaped(&b, uri ? uri : "");
    sb_puts(&b, "\",\"mimeType\":\"text/plain\",\"text\":\"");
    sb_put_escaped(&b, text);
    sb_puts(&b, "\"}]}");
    return b.data;
}

/* tools/call: read-only enforcement + hook di esecuzione.
 * Ritorna SEMPRE un envelope JSON-RPC completo (result o error): la risposta del tool
 * è annidata in "result". (Bug trovato dall'E2E con client esterno 2026-09-15: prima
 * la risposta del tool usciva NUDa, senza jsonrpc/id/result — il client reale non la
 * accettava; il selfcheck non se ne accorgeva perché cercava solo una sottostringa.) */
static char* mcp_tools_call(const char* id_raw, const char* request_json)
{
    char* name = json_str_path(request_json, "params.name");
    if (!name || !*name) {
        free(name);
        return mcp_error_str(id_raw, -32602, "tools/call requires name");
    }
    if (!gmb_mcp_tool_exists(name)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Unknown tool: %s", name);
        free(name);
        return mcp_error_str(id_raw, -32602, msg);
    }
    if (g_serve_readonly && !gmb_mcp_tool_is_readonly(name)) {
        char msg[320];
        snprintf(msg, sizeof(msg), "Read-only server: tool '%s' is declared mutating", name);
        free(name);
        return mcp_error_str(id_raw, -32600, msg);
    }
    if (!g_executor) {
        free(name);
        return mcp_error_str(id_raw, -32603, "No tool executor registered");
    }
    const char* proc = gmb_mcp_tool_proc(name);
    if (!proc || !*proc) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Tool '%s' has no BASIC procedure bound", name);
        free(name);
        return mcp_error_str(id_raw, -32603, msg);
    }
    char* args = json_str_path(request_json, "params.arguments");
    if (!args) args = strdup("{}");

    char* out = NULL;
    int is_err = 0;
    int ok = g_executor(g_executor_ctx, name, proc, args, &out, &is_err);
    free(args);
    free(name);
    if (!ok) return mcp_error_str(id_raw, -32603, "Tool execution failed");

    sb_t b = {0};
    sb_puts(&b, "{\"content\":[{\"type\":\"text\",\"text\":\"");
    sb_put_escaped(&b, out ? out : "");
    sb_puts(&b, "\"}],\"isError\":");
    sb_puts(&b, is_err ? "true" : "false");
    sb_putc_(&b, '}');
    free(out);
    char* resp = mcp_result_str(id_raw, b.data ? b.data : "{}");
    free(b.data);
    return resp;
}

/* Entry point: processa una richiesta e ritorna la risposta (NULL = notifica). */
char* gmb_mcp_handle_json(const char* request_json)
{
    if (!request_json || !*request_json) return mcp_error_str(NULL, -32700, "Parse error");

    char id_raw[512];
    json_raw_field(request_json, "id", id_raw, sizeof(id_raw));
    int has_id = id_raw[0] != '\0' && strcmp(id_raw, "null") != 0;

    char* method = json_str_path(request_json, "method");
    if (!method || !*method) {
        free(method);
        return has_id ? mcp_error_str(id_raw, -32600, "Invalid Request: missing method") : NULL;
    }
    if (strcmp(method, "notifications/initialized") == 0 ||
        strncmp(method, "notifications/", 14) == 0) {
        free(method);
        return NULL; /* notifica: nessuna risposta */
    }

    char* result = NULL;
    if (strcmp(method, "initialize") == 0) {
        result = strdup(MCP_INIT_RESULT);
    } else if (strcmp(method, "tools/list") == 0) {
        result = mcp_tools_list(NULL);
    } else if (strcmp(method, "resources/list") == 0) {
        result = mcp_resources_list();
    } else if (strcmp(method, "prompts/list") == 0) {
        result = strdup("{\"prompts\":[]}");
    } else if (strcmp(method, "ping") == 0) {
        result = strdup("{}");
    } else if (strcmp(method, "tools/call") == 0) {
        if (!has_id) { free(method); return NULL; }
        free(method);
        return mcp_tools_call(id_raw, request_json);
    } else if (strcmp(method, "resources/read") == 0) {
        char* uri = json_str_path(request_json, "params.uri");
        result = mcp_resources_read(uri ? uri : "");
        free(uri);
    } else if (strcmp(method, "resources/subscribe") == 0 || strcmp(method, "resources/unsubscribe") == 0) {
        result = strdup("{}");
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Method not found: %s", method);
        free(method);
        return has_id ? mcp_error_str(id_raw, -32601, msg) : NULL;
    }
    free(method);

    if (!result) return has_id ? mcp_error_str(id_raw, -32603, "Internal error") : NULL;
    if (!has_id) { free(result); return NULL; }

    char* resp = mcp_result_str(id_raw, result);
    free(result);
    return resp;
}

/* ---- transport: stdio ---- */
static char* read_line_dyn(FILE* f)
{
    size_t cap = 1024, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 2 > cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        if (c == '\n') break;
        if (c != '\r') buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

/* ---- transport: HTTP ---- */
static int mcp_http_route(void* ctx, const gmb_http_request_t* req,
                          char** out_body, int* out_status, char** out_headers)
{
    (void)ctx;
    if (!req || !req->method) return -1;
    if (strcmp(req->method, "POST") != 0) return -1;
    const char* body = req->body ? req->body : "";
    char* resp = gmb_mcp_handle_json(body);
    if (!resp) { /* notifica */ *out_status = 202; *out_body = strdup(""); *out_headers = strdup("Content-Type: application/json\n"); return 0; }
    *out_status = 200;
    *out_body = resp;
    *out_headers = strdup("Content-Type: application/json\n");
    return 0;
}

int gmb_mcp_serve(const char* mode, int port, int readonly)
{
    gmb_mcp_set_readonly(readonly);
    const char* m = mode ? mode : "stdio";

    if (strcmp(m, "stdio") == 0) {
        g_running = 1;
        /* loop blocante su stdin finché EOF (stesso contratto di McpServerHost +
         * McpStdioTransport nel backend gestito). */
        while (g_running) {
            char* line = read_line_dyn(stdin);
            if (!line) break;                 /* EOF */
            if (*line) {
                char* resp = gmb_mcp_handle_json(line);
                if (resp) {
                    fputs(resp, stdout);
                    fputc('\n', stdout);
                    fflush(stdout);
                    free(resp);
                }
            }
            free(line);
        }
        g_running = 0;
        return 1;
    }

    if (strcmp(m, "http") == 0) {
        if (port <= 0 || port > 65535) return 0;
        int64_t srv = gmb_http_server_start(port);
        if (srv <= 0) return 0;
        /* route unica MCP: POST * (il path è scelto dal client, es. /mcp) */
        if (gmb_http_server_route(srv, "POST", "*", mcp_http_route, NULL) != 0) {
            gmb_http_server_stop(srv);
            return 0;
        }
        g_http_listener = srv;
        g_running = 1;
        return 1;
    }

    return 0; /* mode sconosciuto: errore ESPLICITO (nessun default silenzioso) */
}

void gmb_mcp_stop(void)
{
    if (g_http_listener > 0) {
        gmb_http_server_stop(g_http_listener);
        g_http_listener = -1;
    }
    g_running = 0;
}
