/* gmb_ai_server.c — runtime C del comando `AI SERVER` (mondo C): gestisce il server LLM
 * OpenAI-compatible (gmb_llm_server) come processo esterno figlio. Nessun bridge in-process.
 * Piattaforme unix (macOS/Linux); altrove -> errore esplicito.
 */
#include "gmb_ai_server.h"

#if defined(__APPLE__) || defined(__linux__)

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define AISRV_TIMEOUT_MS 60000
#define AISRV_POLL_MS 200
#define AISRV_HTTP_BUF 16384

static void seterr(char* e, size_t cap, const char* m) {
    if (e && cap) { snprintf(e, cap, "%s", m ? m : "ai server error"); }
}

static int file_exec_ok(const char* p) {
    return p != NULL && p[0] != '\0' && access(p, X_OK) == 0;
}

/* Risale da `dir` cercando la build canonica del server (mutando dir). */
static const char* walk_up_find(char* dir) {
    static char cand[4096];
    for (int i = 0; i < 8 && dir[0] != '\0'; i++) {
        snprintf(cand, sizeof cand, "%s/native/gmb_llm_server/build/gmb_llm_server", dir);
        if (file_exec_ok(cand)) return cand;
        snprintf(cand, sizeof cand, "%s/native/gmb_llm_server/gmb_llm_server", dir);
        if (file_exec_ok(cand)) return cand;
        char* slash = strrchr(dir, '/');
        if (slash == NULL || slash == dir) break;
        *slash = '\0';
    }
    return NULL;
}

static const char* resolve_bin(void) {
    /* La CWD e' la cartella del PROGRAMMA BASIC con l'entry point (regola AGENTS §9.1: ogni
     * esecuzione imposta la CWD sulla cartella del programma PRIMA di eseguirlo). La risoluzione
     * parte da li':
     *   1) env esplicito GMBASIC_LLM_SERVER (parita' con AiServerManager C#);
     *   2) path compilato esplicito (se contiene '/');
     *   3) risalita dalla CWD (cartella programma) cercando la build canonica del repo;
     *   4) nome nudo (PATH).
     * Programmi fuori dal repo -> richiedono env o PATH (comportamento atteso, non un bug). */
    const char* env = getenv("GMBASIC_LLM_SERVER");
    if (env && env[0]) return env;
    if (strchr(GMB_AI_SERVER_BIN, '/') != NULL) return GMB_AI_SERVER_BIN;
    char dir[4096];
    if (getcwd(dir, sizeof dir) != NULL) {
        const char* f = walk_up_find(dir);
        if (f != NULL) return f;
    }
    return GMB_AI_SERVER_BIN;   /* assumibile in PATH */
}

/* HTTP elementare su 127.0.0.1:port, connection: close. body!=NULL => POST json.
 * Ritorna 0 se HTTP 200 (resp = corpo se resp non NULL), -1 altrimenti (resp = error msg). */
static int http_req(int port, const char* method, const char* path, const char* body,
                    char* resp, size_t respcap) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { if (resp && respcap) snprintf(resp, respcap, "socket"); return -1; }
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    if (connect(fd, (struct sockaddr*)&a, sizeof a) != 0) {
        snprintf(resp, respcap, "connect: %s", strerror(errno));
        close(fd);
        return -1;
    }
    char req[AISRV_HTTP_BUF];
    size_t used = 0;
    used += (size_t)snprintf(req + used, sizeof req - used, "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n", method, path);
    if (body) {
        used += (size_t)snprintf(req + used, sizeof req - used, "Content-Type: application/json\r\nContent-Length: %zu\r\n", strlen(body));
    }
    used += (size_t)snprintf(req + used, sizeof req - used, "\r\n");
    if (body) { memcpy(req + used, body, strlen(body)); used += strlen(body); }
    size_t sent = 0;
    while (sent < used) {
        ssize_t w = send(fd, req + sent, used - sent, 0);
        if (w <= 0) { close(fd); if (resp && respcap) snprintf(resp, respcap, "send: %s", strerror(errno)); return -1; }
        sent += (size_t)w;
    }
    /* risposta completa (connection: close) */
    char buf[AISRV_HTTP_BUF + 1];
    size_t n = 0;
    for (;;) {
        ssize_t r = recv(fd, buf + n, (sizeof buf) - 1 - n, 0);
        if (r <= 0) break;
        n += (size_t)r;
        if (n >= sizeof buf - 1) break;
    }
    close(fd);
    buf[n] = 0;
    int code = 0;
    if (sscanf(buf, "HTTP/1.%*d %d", &code) != 1 && sscanf(buf, "HTTP/1.1 %d", &code) != 1) {
        if (resp && respcap) snprintf(resp, respcap, "risposta http non valida");
        return -1;
    }
    char* h = strstr(buf, "\r\n\r\n");
    const char* bodyp = h ? h + 4 : buf;
    if (resp && respcap) { size_t bl = strlen(bodyp); if (bl >= respcap) bl = respcap - 1; memcpy(resp, bodyp, bl); resp[bl] = 0; }
    return code == 200 ? 0 : -1;
}

static int pid_alive(int pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0 || errno == EPERM;
}

int gmb_ai_server_start(const char* model_dir, int port, int max_active,
                        int* pid_out, char* err, size_t errcap) {
    *pid_out = 0;
    const char* bin = resolve_bin();
    if (!model_dir || !model_dir[0]) { seterr(err, errcap, "model_dir vuoto"); return -1; }
    /* Fail-loud su dir inesistente PRIMA del fork: parità col gestore C#
     * (AiServerManager.Start controlla Directory.Exists) e niente processo orfano. */
    struct stat dirst;
    if (stat(model_dir, &dirst) != 0 || !S_ISDIR(dirst.st_mode)) {
        seterr(err, errcap, "model dir inesistente");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) { seterr(err, errcap, "fork fallita"); return -1; }
    if (pid == 0) {
        /* figlio: silenzioso e sostituito dal server */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); if (devnull > 2) close(devnull); }
        if (max_active > 0) { char envb[16]; snprintf(envb, sizeof envb, "%d", max_active); setenv("GMB_MAX_ACTIVE", envb, 1); }
        char portstr[16]; snprintf(portstr, sizeof portstr, "%d", port);
        execl(bin, bin, model_dir, portstr, (char*)NULL);
        _exit(127);
    }
    /* parent: readiness */
    int elapsed = 0;
    for (; elapsed < AISRV_TIMEOUT_MS; elapsed += AISRV_POLL_MS) {
        if (!pid_alive((int)pid)) {
            int st = 0; waitpid(pid, &st, WNOHANG);
            seterr(err, errcap, "il server e' uscito durante l'avvio");
            return -1;
        }
        gmb_ai_server_status_t st;
        memset(&st, 0, sizeof st);
        if (gmb_ai_server_status((int)pid, port, &st) == 0 && st.loaded) {
            *pid_out = (int)pid;
            return 0;
        }
        usleep((useconds_t)(AISRV_POLL_MS * 1000));
    }
    kill(pid, SIGTERM);
    seterr(err, errcap, "timeout readiness (server non pronto in 60s)");
    return -1;
}

int gmb_ai_server_stop(int pid, char* err, size_t errcap) {
    if (!pid_alive(pid)) return 0;
    if (kill(pid, SIGTERM) != 0) { seterr(err, errcap, "kill fallita"); return -1; }
    for (int i = 0; i < 50 && pid_alive(pid); i++) usleep(50000);
    if (pid_alive(pid)) kill(pid, SIGKILL);
    /* reap: evita zombie (un figlio terminato resta "alive" per kill(pid,0) finche' non e'
     * raccolto da waitpid) */
    for (int i = 0; i < 100 && pid_alive(pid); i++) {
        int stt = 0;
        if (waitpid(pid, &stt, WNOHANG) == pid) break;
        usleep(10000);
    }
    (void)err; (void)errcap;
    return 0;
}

int gmb_ai_server_status(int pid, int port, gmb_ai_server_status_t* out) {
    memset(out, 0, sizeof *out);
    out->pid = pid;
    out->port = port;
    out->running = pid_alive(pid);
    if (!out->running) return -1;
    char resp[AISRV_HTTP_BUF];
    resp[0] = 0;
    if (http_req(port, "GET", "/v1/status", NULL, resp, sizeof resp) != 0) {
        snprintf(out->err, sizeof out->err, "%s", resp[0] ? resp : "GET /v1/status non riuscito");
        return -1;
    }
    out->loaded = strstr(resp, "\"loaded\":true") != NULL;
    out->busy = strstr(resp, "\"busy\":true") != NULL;
    const char* m = strstr(resp, "\"model\":\"");
    if (m) {
        m += 9;
        size_t i = 0;
        while (m[i] && m[i] != '"' && i + 1 < sizeof out->model) { out->model[i] = m[i]; i++; }
        out->model[i] = 0;
    }
    return 0;
}

static int post_json(int pid, int port, const char* path, const char* body, char* err, size_t errcap) {
    if (!pid_alive(pid)) { seterr(err, errcap, "server non attivo"); return -1; }
    char resp[AISRV_HTTP_BUF];
    resp[0] = 0;
    if (http_req(port, "POST", path, body, resp, sizeof resp) != 0) {
        seterr(err, errcap, resp[0] ? resp : "POST non riuscito");
        return -1;
    }
    return 0;
}

int gmb_ai_server_load(int pid, int port, const char* model_dir, char* err, size_t errcap) {
    char body[AISRV_HTTP_BUF];
    snprintf(body, sizeof body, "{\"model_dir\":\"%s\"}", model_dir);
    return post_json(pid, port, "/v1/models/load", body, err, errcap);
}

int gmb_ai_server_unload(int pid, int port, char* err, size_t errcap) {
    return post_json(pid, port, "/v1/models/unload", "{}", err, errcap);
}

/* ---- strato "verbi": server corrente + JSON (specchio IlRuntime.AiServer*) ---- */
static int g_cur_pid = 0;
static int g_cur_port = 0;
int gmb_ai_server_current_pid(void) { return g_cur_pid; }
int gmb_ai_server_current_port(void) { return g_cur_port; }

static void json_append_esc(char* out, size_t cap, const char* s) {
    size_t n = strlen(out);
    for (const char* p = s; *p && n + 2 < cap; p++) {
        if (*p == '"' || *p == '\\') { if (n + 2 < cap) { out[n++] = '\\'; out[n++] = *p; } }
        else if (*p == '\n') { if (n + 3 < cap) { out[n++] = '\\'; out[n++] = 'n'; } }
        else out[n++] = *p;
    }
    out[n] = 0;
}

int gmb_ai_server_json_start(const char* model_dir, int port, int max_active, char* out, size_t cap) {
    char err[256] = {0};
    int pid = 0;
    out[0] = 0;
    if (gmb_ai_server_start(model_dir, port, max_active, &pid, err, sizeof err) != 0) {
        snprintf(out, cap, "{\"ok\":false,\"error\":\"");
        json_append_esc(out, cap, err);
        size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
        return -1;
    }
    g_cur_pid = pid;
    g_cur_port = port;
    snprintf(out, cap, "{\"ok\":true,\"running\":true,\"pid\":%d,\"port\":%d,\"model\":\"", pid, port);
    json_append_esc(out, cap, model_dir);
    size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
    return 0;
}

int gmb_ai_server_json_stop(char* out, size_t cap) {
    char err[256] = {0};
    int port = g_cur_port;
    int rc = gmb_ai_server_stop(g_cur_pid, err, sizeof err);
    if (rc != 0) {
        snprintf(out, cap, "{\"ok\":false,\"error\":\"");
        json_append_esc(out, cap, err);
        size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
        g_cur_pid = 0;
        return -1;
    }
    g_cur_pid = 0;
    g_cur_port = 0;
    snprintf(out, cap, "{\"ok\":true,\"running\":false,\"port\":%d}", port);
    return 0;
}

int gmb_ai_server_json_status(char* out, size_t cap) {
    gmb_ai_server_status_t st;
    memset(&st, 0, sizeof st);
    out[0] = 0;
    if (g_cur_pid <= 0) {
        snprintf(out, cap, "{\"ok\":false,\"running\":false,\"error\":\"nessun server avviato\"}");
        return -1;
    }
    if (gmb_ai_server_status(g_cur_pid, g_cur_port, &st) != 0) {
        snprintf(out, cap, "{\"ok\":false,\"running\":%s,\"error\":\"", st.running ? "true" : "false");
        json_append_esc(out, cap, st.err[0] ? st.err : "status non riuscito");
        size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
        return -1;
    }
    snprintf(out, cap, "{\"ok\":true,\"running\":true,\"pid\":%d,\"port\":%d,\"model\":\"",
             st.pid, st.port);
    json_append_esc(out, cap, st.model);
    size_t n = strlen(out);
    snprintf(out + n, cap - n, "\",\"loaded\":%s,\"busy\":%s}",
             st.loaded ? "true" : "false", st.busy ? "true" : "false");
    return 0;
}

int gmb_ai_server_json_load(const char* model_dir, char* out, size_t cap) {
    char err[256] = {0};
    out[0] = 0;
    if (g_cur_pid <= 0) { snprintf(out, cap, "{\"ok\":false,\"error\":\"server non avviato\"}"); return -1; }
    if (gmb_ai_server_load(g_cur_pid, g_cur_port, model_dir, err, sizeof err) != 0) {
        snprintf(out, cap, "{\"ok\":false,\"error\":\"");
        json_append_esc(out, cap, err);
        size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
        return -1;
    }
    snprintf(out, cap, "{\"ok\":true,\"model\":\"");
    json_append_esc(out, cap, model_dir);
    size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
    return 0;
}

int gmb_ai_server_json_unload(char* out, size_t cap) {
    char err[256] = {0};
    out[0] = 0;
    if (g_cur_pid <= 0) { snprintf(out, cap, "{\"ok\":false,\"error\":\"server non avviato\"}"); return -1; }
    if (gmb_ai_server_unload(g_cur_pid, g_cur_port, err, sizeof err) != 0) {
        snprintf(out, cap, "{\"ok\":false,\"error\":\"");
        json_append_esc(out, cap, err);
        size_t n = strlen(out); if (n + 2 < cap) { out[n++] = '"'; out[n++] = '}'; out[n] = 0; }
        return -1;
    }
    snprintf(out, cap, "{\"ok\":true,\"loaded\":false}");
    return 0;
}


#else

#include <stdio.h>   /* snprintf dei messaggi di errore del ramo "non supportato" (era implicito) */

int gmb_ai_server_start(const char* model_dir, int port, int max_active,
                        int* pid_out, char* err, size_t errcap) {
    (void)model_dir; (void)port; (void)max_active; *pid_out = 0;
    if (err && errcap) snprintf(err, errcap, "gmb_ai_server non supportato su questa piattaforma");
    return -1;
}
int gmb_ai_server_stop(int pid, char* err, size_t errcap) {
    (void)pid; if (err && errcap) snprintf(err, errcap, "non supportato"); return -1;
}
int gmb_ai_server_status(int pid, int port, gmb_ai_server_status_t* out) {
    (void)pid; (void)port; memset(out, 0, sizeof *out); return -1;
}
int gmb_ai_server_load(int pid, int port, const char* model_dir, char* err, size_t errcap) {
    (void)pid; (void)port; (void)model_dir; if (err && errcap) snprintf(err, errcap, "non supportato"); return -1;
}
int gmb_ai_server_unload(int pid, int port, char* err, size_t errcap) {
    (void)pid; (void)port; if (err && errcap) snprintf(err, errcap, "non supportato"); return -1;
}
int gmb_ai_server_json_start(const char* model_dir, int port, int max_active, char* out, size_t cap) {
    (void)model_dir; (void)port; (void)max_active; snprintf(out, cap, "{\"ok\":false,\"error\":\"non supportato\"}"); return -1;
}
int gmb_ai_server_json_stop(char* out, size_t cap) { snprintf(out, cap, "{\"ok\":false,\"error\":\"non supportato\"}"); return -1; }
int gmb_ai_server_json_status(char* out, size_t cap) { snprintf(out, cap, "{\"ok\":false,\"running\":false,\"error\":\"non supportato\"}"); return -1; }
int gmb_ai_server_json_load(const char* model_dir, char* out, size_t cap) {
    (void)model_dir; snprintf(out, cap, "{\"ok\":false,\"error\":\"non supportato\"}"); return -1;
}
int gmb_ai_server_json_unload(char* out, size_t cap) { snprintf(out, cap, "{\"ok\":false,\"error\":\"non supportato\"}"); return -1; }
int gmb_ai_server_current_pid(void) { return 0; }
int gmb_ai_server_current_port(void) { return 0; }

#endif


/* wrapper statement (default porta 8020; risultato scartato dal chiamante) */
int gmb_ai_server_stmt_start(const char* model_dir) {
    char out[1024];
    return gmb_ai_server_json_start(model_dir, 8020, 0, out, sizeof out) == 0 ? 0 : -1;
}
int gmb_ai_server_stmt_start_port(const char* model_dir, int port) {
    char out[1024];
    if (port < 1 || port > 65535) port = 8020;
    return gmb_ai_server_json_start(model_dir, port, 0, out, sizeof out) == 0 ? 0 : -1;
}
int gmb_ai_server_stmt_stop(void) { char out[1024]; return gmb_ai_server_json_stop(out, sizeof out) == 0 ? 0 : -1; }
int gmb_ai_server_stmt_status(void) { char out[1024]; return gmb_ai_server_json_status(out, sizeof out) == 0 ? 0 : -1; }
int gmb_ai_server_stmt_model(const char* model_dir) { char out[1024]; return gmb_ai_server_json_load(model_dir, out, sizeof out) == 0 ? 0 : -1; }
int gmb_ai_server_stmt_unload(void) { char out[1024]; return gmb_ai_server_json_unload(out, sizeof out) == 0 ? 0 : -1; }
