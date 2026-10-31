/* gmb_fetch_http_selfcheck.c — END-TO-END del trasporto libcurl vero, contro un server HTTP
 * LOCALE (nessuna rete esterna, nessun download grande).
 *
 * Il selfcheck offline (gmb_fetch_selfcheck) prova la POLITICA con un trasporto finto. Qui si
 * prova il pezzo che il finto non può coprire: il trasporto reale deve davvero mandare
 * `Range: bytes=N-`, leggere `Content-Range`, fermarsi quando il server ignora il Range,
 * mandare `Authorization: Bearer` SOLO quando c'è un token, e mappare 404/401 su errori
 * definitivi (nessun retry inutile).
 */
#include "gmb_fetch.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
int main(void) { printf("gmb_fetch_http_selfcheck: SKIP (server locale non implementato su Windows)\n"); return 0; }
#else

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

/* ── server minimale ─────────────────────────────────────────────────────────────────────── */
static unsigned char g_body[200000];
static long long g_body_len = 150000;
static int g_listen_fd = -1;
static volatile int g_stop = 0;
static volatile int g_requests = 0;
static volatile long long g_last_range = -1;   /* -1 = nessun header Range */
static volatile int g_saw_auth = 0;

static int send_all(int fd, const void* buf, size_t n)
{
    const char* p = (const char*)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, p + off, n - off, 0);
        if (w <= 0) return 0;
        off += (size_t)w;
    }
    return 1;
}

static void serve_one(int fd)
{
    char req[8192];
    size_t used = 0;
    while (used + 1 < sizeof req) {
        ssize_t n = recv(fd, req + used, sizeof req - 1 - used, 0);
        if (n <= 0) return;
        used += (size_t)n;
        req[used] = '\0';
        if (strstr(req, "\r\n\r\n")) break;
    }
    g_requests++;

    /* path = seconda parola della prima riga */
    char path[256] = "";
    const char* sp = strchr(req, ' ');
    if (sp) { const char* sp2 = strchr(sp + 1, ' '); if (sp2) { size_t l = (size_t)(sp2 - sp - 1); if (l < sizeof path) { memcpy(path, sp + 1, l); path[l] = 0; } } }

    long long from = -1;
    const char* rg = strstr(req, "Range: bytes=");
    if (!rg) rg = strstr(req, "range: bytes=");
    if (rg) from = strtoll(rg + 13, NULL, 10);
    g_last_range = from;
    if (strstr(req, "Authorization: Bearer sekret")) g_saw_auth = 1;

    char hdr[512];
    long long start = 0, len = g_body_len;
    int status = 200;

    if (strcmp(path, "/missing") == 0) {
        const char* m = "not found";
        int n = snprintf(hdr, sizeof hdr,
                         "HTTP/1.1 404 Not Found\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
                         strlen(m), m);
        send_all(fd, hdr, (size_t)n);
        return;
    }
    if (strcmp(path, "/deny") == 0 && !g_saw_auth) {
        const char* m = "no";
        int n = snprintf(hdr, sizeof hdr,
                         "HTTP/1.1 401 Unauthorized\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
                         strlen(m), m);
        send_all(fd, hdr, (size_t)n);
        return;
    }
    /* /norange: finge di non supportare Range (200 + corpo intero). */
    int supports_range = (strcmp(path, "/norange") != 0);
    if (supports_range && from > 0 && from < g_body_len) {
        start = from;
        len = g_body_len - start;
        status = 206;
    }
    if (supports_range && from >= g_body_len && from > 0) {
        int n = snprintf(hdr, sizeof hdr,
                         "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */%lld\r\n"
                         "Content-Length: 0\r\nConnection: close\r\n\r\n", g_body_len);
        send_all(fd, hdr, (size_t)n);
        return;
    }
    if (status == 206) {
        int n = snprintf(hdr, sizeof hdr,
                         "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes %lld-%lld/%lld\r\n"
                         "Content-Length: %lld\r\nAccept-Ranges: bytes\r\nConnection: close\r\n\r\n",
                         start, g_body_len - 1, g_body_len, len);
        send_all(fd, hdr, (size_t)n);
    } else {
        int n = snprintf(hdr, sizeof hdr,
                         "HTTP/1.1 200 OK\r\nContent-Length: %lld\r\nAccept-Ranges: %s\r\n"
                         "Connection: close\r\n\r\n", len, supports_range ? "bytes" : "none");
        send_all(fd, hdr, (size_t)n);
    }
    send_all(fd, g_body + start, (size_t)len);
}

static void* server_main(void* arg)
{
    (void)arg;
    while (!g_stop) {
        struct sockaddr_in cli;
        socklen_t cl = sizeof cli;
        int c = accept(g_listen_fd, (struct sockaddr*)&cli, &cl);
        if (c < 0) {
            if (errno == EINTR) continue;
            if (g_stop) break;
            usleep(5000);
            continue;
        }
        serve_one(c);
        close(c);
    }
    return NULL;
}

static int start_server(int* out_port)
{
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) return 0;
    int one = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    if (bind(g_listen_fd, (struct sockaddr*)&a, sizeof a) != 0) return 0;
    if (listen(g_listen_fd, 8) != 0) return 0;
    socklen_t al = sizeof a;
    if (getsockname(g_listen_fd, (struct sockaddr*)&a, &al) != 0) return 0;
    *out_port = ntohs(a.sin_port);
    return 1;
}

/* ── casi ────────────────────────────────────────────────────────────────────────────────── */
static char g_dir[1024];

static long long file_size(const char* p)
{
    struct stat st;
    if (stat(p, &st) != 0) return -1;
    return (long long)st.st_size;
}

static int file_equals(const char* p, const unsigned char* d, long long n)
{
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    unsigned char buf[8192];
    long long off = 0;
    size_t r;
    while ((r = fread(buf, 1, sizeof buf, f)) > 0) {
        if (off + (long long)r > n || memcmp(buf, d + off, r) != 0) { fclose(f); return 0; }
        off += (long long)r;
    }
    fclose(f);
    return off == n;
}

static int write_partial(const char* path, const unsigned char* d, long long n)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(d, 1, (size_t)n, f);
    fclose(f);
    return w == (size_t)n;
}

static int g_progress_calls = 0;
static void on_progress(void* u, long long done, long long total, double rate, long long eta)
{
    (void)u; (void)done; (void)total; (void)rate; (void)eta;
    g_progress_calls++;
}

int main(void)
{
    const gmb_fetch_transport* tr = gmb_fetch_transport_curl();
    if (!tr) {
        printf("gmb_fetch_http_selfcheck: SKIP (runtime compilato senza libcurl)\n");
        return 0;
    }

    /* corpo deterministico */
    unsigned int x = 12345u;
    for (long long i = 0; i < g_body_len; i++) { x = x * 1664525u + 1013904223u; g_body[i] = (unsigned char)(x >> 24); }
    char* sha = gmb_sha256_hex(g_body, (size_t)g_body_len);

    const char* tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s/gmb_fetch_http_%d", tmp, (int)getpid());
    mkdir(g_dir, 0755);

    int port = 0;
    CHECK(start_server(&port) == 1);
    pthread_t th;
    CHECK(pthread_create(&th, NULL, server_main, NULL) == 0);

    char url[512], dest[1200], incom[1300], dir2[1100];
    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;
    op.progress = on_progress;
    char err[512];

    /* 1) resume vero: il server risponde 206 e la richiesta parte dal byte giusto */
    snprintf(dest, sizeof dest, "%s/http_resume/brain.db", g_dir);
    snprintf(dir2, sizeof dir2, "%s/http_resume", g_dir);
    mkdir(dir2, 0755);
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(write_partial(incom, g_body, 30000));
    snprintf(url, sizeof url, "http://127.0.0.1:%d/full", port);
    g_last_range = -1;
    g_progress_calls = 0;
    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = g_body_len;
    CHECK(gmb_fetch_to_file(&rq, &op, tr, err, sizeof err) == GMB_FETCH_OK);
    CHECK(g_last_range == 30000);                    /* RANGE davvero inviato */
    CHECK(file_size(dest) == g_body_len);
    CHECK(file_equals(dest, g_body, g_body_len));
    CHECK(g_progress_calls > 0);                     /* la barra di avanzamento è viva */

    /* 2) server che IGNORA il Range: il trasporto si ferma, il motore tronca e rifà */
    snprintf(dest, sizeof dest, "%s/http_norange/brain.db", g_dir);
    snprintf(dir2, sizeof dir2, "%s/http_norange", g_dir);
    mkdir(dir2, 0755);
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(write_partial(incom, g_body, 20000));
    snprintf(url, sizeof url, "http://127.0.0.1:%d/norange", port);
    g_requests = 0;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = g_body_len;
    int rc_nr = gmb_fetch_to_file(&rq, &op, tr, err, sizeof err);
    if (rc_nr != GMB_FETCH_OK) printf("  [norange] rc=%d err=%s\n", rc_nr, err);
    CHECK(rc_nr == GMB_FETCH_OK);
    CHECK(g_requests == 2);                          /* primo tentativo + riavvio */
    CHECK(file_equals(dest, g_body, g_body_len));     /* nessun byte duplicato */

    /* 3) token: senza → 401 (definitivo, nessun retry); con → 200 */
    snprintf(dest, sizeof dest, "%s/http_deny/brain.db", g_dir);
    snprintf(dir2, sizeof dir2, "%s/http_deny", g_dir);
    mkdir(dir2, 0755);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/deny", port);
    int before = g_requests;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = g_body_len;
    int rc = gmb_fetch_to_file(&rq, &op, tr, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_NET);
    CHECK(g_requests - before == 1);                 /* 401 NON si riprova */
    CHECK(file_size(dest) == -1);

    g_saw_auth = 0;
    rq.token = "sekret";
    CHECK(gmb_fetch_to_file(&rq, &op, tr, err, sizeof err) == GMB_FETCH_OK);
    CHECK(g_saw_auth == 1);                          /* Authorization arrivato */
    CHECK(file_equals(dest, g_body, g_body_len));

    /* 4) 404: errore definitivo, una sola richiesta */
    snprintf(dest, sizeof dest, "%s/http_404/brain.db", g_dir);
    snprintf(dir2, sizeof dir2, "%s/http_404", g_dir);
    mkdir(dir2, 0755);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/missing", port);
    before = g_requests;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = dest;
    rq.size = 10;
    rc = gmb_fetch_to_file(&rq, &op, tr, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_NET);
    CHECK(g_requests - before == 1);
    CHECK(file_size(dest) == -1);

    g_stop = 1;
    shutdown(g_listen_fd, SHUT_RDWR);
    close(g_listen_fd);
    pthread_join(th, NULL);

    free(sha);
    printf("gmb_fetch_http_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}

#endif
