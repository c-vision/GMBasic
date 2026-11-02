/* Notebook/notebook_server.c — server HTTP del notebook (C, 127.0.0.1).
 *
 * Protocollo (parità con il NotebookServer della vecchia REPL C#, eliminata in F6b):
 *   GET  /health    → {"ok":true}
 *   POST /execute   {"cellId":"...","source":"..."} → {"ok":true,"output":"...","value":…,
 *                                                      "compileMs":0,"executeMs":N}
 *   GET|POST /vars  → {"vars":[{"name":"x","value":5}]}
 *   GET|POST /reset → {"ok":true}
 *   POST /shutdown  → {"ok":true} (arresta il server)
 *
 * Bind su 127.0.0.1 (mai 0.0.0.0): è una sessione locale, come nel server C#.
 * Un kernel = una sessione: le richieste sono servite IN SERIE dal loop accept.
 */
#include "notebook_server.h"
#include "notebook_kernel.h"

#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NB_MAX_REQ   (1 << 20)   /* 1 MB: celle anche grosse */
#define NB_MAX_BODY  (768 * 1024)

static volatile sig_atomic_t g_stop = 0;
static const char* g_autosave = NULL;
static nb_kernel_t* g_kernel = NULL;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

/* ── risposta ─────────────────────────────────────────────────────────────── */

static const char* status_text(int code)
{
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        default:  return "Internal Server Error";
    }
}

static void send_response(int fd, int status, const char* json)
{
    char head[256];
    size_t blen = json ? strlen(json) : 0;
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n", status, status_text(status), blen);
    (void)!write(fd, head, (size_t)n);
    if (blen) (void)!write(fd, json, blen);
}

/* ── autosave ─────────────────────────────────────────────────────────────── */

static void autosave_snapshot(void)
{
    if (!g_autosave || !*g_autosave || !g_kernel) return;
    char* js = nb_kernel_vars_json(g_kernel);
    if (!js) return;
    FILE* f = fopen(g_autosave, "wb");
    if (f) { fwrite(js, 1, strlen(js), f); fclose(f); }
    free(js);
}

/* ── richiesta ────────────────────────────────────────────────────────────── */

typedef struct {
    char method[16];
    char path[512];
    char* body;     /* malloc, NUL-terminated (può essere "") */
} nb_request_t;

static int read_request(int fd, nb_request_t* r)
{
    memset(r, 0, sizeof(*r));
    char* buf = (char*)malloc(NB_MAX_REQ + 1);
    if (!buf) return -1;
    size_t used = 0;
    int header_end = -1;

    while (used < NB_MAX_REQ) {
        ssize_t n = read(fd, buf + used, NB_MAX_REQ - used);
        if (n <= 0) break;
        used += (size_t)n;
        buf[used] = 0;
        char* p = strstr(buf, "\r\n\r\n");
        if (p) { header_end = (int)(p - buf) + 4; break; }
    }
    if (header_end < 0) { free(buf); return -1; }
    buf[used] = 0;

    /* riga di richiesta */
    if (sscanf(buf, "%15s %511s", r->method, r->path) != 2) { free(buf); return -1; }
    char* q = strchr(r->path, '?');
    if (q) *q = 0;

    /* Content-Length */
    size_t clen = 0;
    int have_cl = 0;
    for (char* line = buf; line && *line; ) {
        char* eol = strstr(line, "\r\n");
        if (!eol) break;
        if (!strncasecmp(line, "Content-Length:", 15)) {
            clen = (size_t)strtoul(line + 15, NULL, 10);
            have_cl = 1;
        }
        line = eol + 2;
        if (line == buf + header_end) break;
        if ((size_t)(line - buf) >= (size_t)header_end) break;
    }

    size_t have = used - (size_t)header_end;
    if (have_cl) {
        if (clen > NB_MAX_BODY) { free(buf); return -2; }
        if (have < clen) {
            size_t need = clen - have;
            char* nb = (char*)realloc(buf, (size_t)header_end + clen + 1);
            if (!nb) { free(buf); return -1; }
            buf = nb;
            while (need > 0) {
                ssize_t n = read(fd, buf + header_end + have, need);
                if (n <= 0) break;
                have += (size_t)n;
                need -= (size_t)n;
            }
            used = (size_t)header_end + have;
        }
        clen = have < clen ? have : clen;
    } else {
        clen = have;
    }

    r->body = (char*)malloc(clen + 1);
    if (!r->body) { free(buf); return -1; }
    memcpy(r->body, buf + header_end, clen);
    r->body[clen] = 0;
    free(buf);
    return 0;
}

/* ── handler ──────────────────────────────────────────────────────────────── */

static char* json_error(const char* msg)
{
    size_t need = strlen(msg) * 2 + 32;
    char* out = (char*)malloc(need);
    if (!out) return NULL;
    char* p = out;
    p += sprintf(p, "{\"ok\":false,\"error\":\"");
    for (const unsigned char* s = (const unsigned char*)msg; *s; s++) {
        if (*s == '"' || *s == '\\') *p++ = '\\';
        if (*s == '\n' || *s == '\r') { *p++ = '\\'; *p++ = 'n'; }
        else *p++ = (char)*s;
    }
    sprintf(p, "\"}");
    return out;
}

static char* handle_execute(const char* body)
{
    gmb_val_t root = gmb_json_parse(body && *body ? body : "{}");
    gmb_val_t src = gmb_val_dict_get(root, "source");
    if (src.kind != GMB_TYPE_STRING) {
        return json_error("campo 'source' (stringa) mancante");
    }
    const char* code = src.v_string.data ? src.v_string.data : "";

    nb_result_t r;
    int rc = nb_kernel_exec(g_kernel, code, &r);
    if (rc != 0) {
        char* out = json_error(r.error ? r.error : "errore di esecuzione");
        nb_result_free(&r);
        return out;
    }

    size_t need = strlen(r.output) * 2 + (r.value_json ? strlen(r.value_json) : 4) + 256;
    char* out = (char*)malloc(need);
    if (!out) { nb_result_free(&r); return NULL; }

    char* p = out;
    p += sprintf(p, "{\"ok\":true,\"output\":\"");
    for (const unsigned char* s = (const unsigned char*)r.output; *s; s++) {
        switch (*s) {
            case '"':  *p++ = '\\'; *p++ = '"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default:
                if (*s < 0x20) p += sprintf(p, "\\u%04x", (unsigned)*s);
                else *p++ = (char)*s;
        }
    }
    p += sprintf(p, "\",\"value\":%s,\"compileMs\":0,\"executeMs\":%.3f,\"fromCache\":false}",
                 r.value_json ? r.value_json : "null", r.exec_ms);
    nb_result_free(&r);
    autosave_snapshot();
    return out;
}

/* Ritorna 1 se la richiesta è stata gestita (stato HTTP in *status, corpo in *out). */
static int dispatch(const nb_request_t* r, int* status, char** out)
{
    const int is_post = strcasecmp(r->method, "POST") == 0;
    const int is_get = strcasecmp(r->method, "GET") == 0;

    if (strcmp(r->path, "/health") == 0) {
        *status = 200; *out = strdup("{\"ok\":true}");
        return 1;
    }
    if (strcmp(r->path, "/execute") == 0) {
        if (!is_post) { *status = 405; *out = json_error("usa POST /execute"); return 1; }
        *status = 200;
        *out = handle_execute(r->body);
        return 1;
    }
    if (strcmp(r->path, "/vars") == 0) {
        if (!is_post && !is_get) { *status = 405; *out = json_error("usa GET|POST /vars"); return 1; }
        *status = 200; *out = nb_kernel_vars_json(g_kernel);
        return 1;
    }
    if (strcmp(r->path, "/reset") == 0) {
        if (!is_post && !is_get) { *status = 405; *out = json_error("usa GET|POST /reset"); return 1; }
        nb_kernel_reset(g_kernel);
        autosave_snapshot();
        *status = 200; *out = strdup("{\"ok\":true}");
        return 1;
    }
    if (strcmp(r->path, "/shutdown") == 0) {
        g_stop = 1;
        *status = 200; *out = strdup("{\"ok\":true}");
        return 1;
    }
    *status = 404;
    *out = json_error("endpoint sconosciuto");
    return 1;
}

/* ── ciclo ────────────────────────────────────────────────────────────────── */

int nb_server_run(int port, const char* autosave)
{
    g_autosave = autosave;
    g_kernel = nb_kernel_new();
    if (!g_kernel) { fprintf(stderr, "?notebook: kernel non allocato\n"); return 1; }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("?notebook: socket"); nb_kernel_free(g_kernel); return 1; }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* SOLO 127.0.0.1 */

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("?notebook: bind 127.0.0.1");
        close(fd);
        nb_kernel_free(g_kernel);
        return 1;
    }
    if (listen(fd, 16) < 0) {
        perror("?notebook: listen");
        close(fd);
        nb_kernel_free(g_kernel);
        return 1;
    }

    printf("NOTEBOOK: server attivo su http://127.0.0.1:%d/  (POST /execute, /vars, /reset; GET /health)\n", port);
    fflush(stdout);

    while (!g_stop) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof(cli);
        int cfd = accept(fd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        nb_request_t req;
        int rr = read_request(cfd, &req);
        if (rr == -2) {
            const char* too = "{\"ok\":false,\"error\":\"corpo troppo grande\"}";
            send_response(cfd, 413, too);
        } else if (rr != 0) {
            const char* bad = "{\"ok\":false,\"error\":\"richiesta non valida\"}";
            send_response(cfd, 400, bad);
        } else {
            int status = 200;
            char* out = NULL;
            dispatch(&req, &status, &out);
            send_response(cfd, status, out ? out : "{\"ok\":false}");
            free(out);
            free(req.body);
        }
        close(cfd);
    }

    close(fd);
    nb_kernel_free(g_kernel);
    g_kernel = NULL;
    printf("NOTEBOOK: server fermato\n");
    fflush(stdout);
    return 0;
}
