/* gmb_fetch_curl.c — trasporto HTTP del motore gmbfetch (libcurl di sistema).
 *
 * Questo file è SOLO trasporto: prende un fd già posizionato su `from`, chiede `Range` se serve,
 * scrive i byte, e dichiara cosa ha visto (206/200, Content-Range, byte scritti). La politica
 * (lock, resume fra processi, verifica, rename) sta in gmb_fetch.c — così si prova offline.
 *
 * TOKEN. Per un asset di release privato GitHub risponde al primo GET con un 302 verso un URL
 * CDN firmato e a scadenza. Il token va SOLO all'API: libcurl (dal 7.58) NON ripropaga
 * `Authorization` verso un host diverso, e l'URL firmato è già l'autorizzazione. Inoltre l'URL
 * nei messaggi d'errore viene stampato SENZA query string: una URL firmata è una credenziale.
 */

#include "gmb_fetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#  include <windows.h>
#  define fetch_write_fd(fd, buf, n) _write((fd), (buf), (unsigned)(n))
#else
#  include <unistd.h>
#  include <errno.h>
#  define fetch_write_fd(fd, buf, n) write((fd), (buf), (n))
#endif

#if __has_include(<curl/curl.h>)
#  include <curl/curl.h>
#  define GMB_FETCH_HAVE_CURL 1
#endif

#ifdef GMB_FETCH_HAVE_CURL

typedef struct {
    int fd;
    long long from;          /* offset richiesto (ripartenza)   */
    long long expected;      /* dimensione attesa, 0 = ignota   */
    long long written;       /* byte scritti in questo tentativo */
    long long cr_start;      /* Content-Range: inizio, -1 = assente */
    long long cr_total;      /* Content-Range: totale, -1 = assente */
    int http;                /* ultimo status visto negli header */
    int ranged;              /* 1 = 206 */
    int ignored_range;       /* 1 = 200 mentre chiedevamo un Range */
    int too_much;            /* 1 = il server ha mandato più del dichiarato */
    int cancelled;
    int io_error;
    long long t0_ms;
    long long last_progress_ms;
    gmb_fetch_transfer* t;
} curl_state;

static long long now_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L;
#endif
}

/* URL per il log: senza query string né frammento (le URL firmate sono credenziali). */
static void url_for_log(const char* url, char* out, size_t cap)
{
    size_t i = 0;
    if (!out || !cap) return;
    if (url) {
        while (url[i] && url[i] != '?' && url[i] != '#' && i + 1 < cap) { out[i] = url[i]; i++; }
    }
    out[i] = '\0';
}

static int ci_prefix(const char* s, size_t n, const char* prefix)
{
    size_t p = strlen(prefix);
    if (n < p) return 0;
    for (size_t i = 0; i < p; i++) {
        char a = s[i], b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

/* "Content-Range: bytes 123-456/789" → cr_start/cr_total. `*` (totale ignoto) → -1. */
static void parse_content_range(curl_state* s, const char* p, size_t n)
{
    char buf[256];
    size_t len = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    char* q = strchr(buf, ':');
    if (!q) return;
    q++;
    while (*q == ' ') q++;
    if (!ci_prefix(q, strlen(q), "bytes")) return;
    q += 5;
    while (*q == ' ') q++;
    char* end = NULL;
    long long first = strtoll(q, &end, 10);
    if (end == q) return;
    s->cr_start = first;
    s->cr_total = -1;
    if (*end == '-') {
        char* slash = strchr(end, '/');
        if (slash) {
            char* tail = slash + 1;
            if (*tail != '*') {
                char* e2 = NULL;
                long long total = strtoll(tail, &e2, 10);
                if (e2 != tail) s->cr_total = total;
            }
        }
    }
}

static size_t on_header(char* buf, size_t size, size_t nitems, void* ud)
{
    size_t n = size * nitems;
    curl_state* s = (curl_state*)ud;
    if (n >= 5 && ci_prefix(buf, n, "HTTP/")) {
        /* Nuova risposta (redirect, 100-continue, definitiva): azzera il blocco precedente. */
        const char* sp = memchr(buf, ' ', n);
        int code = sp ? atoi(sp + 1) : 0;
        s->http = code;
        s->ranged = (code == 206) ? 1 : 0;
        s->cr_start = -1;
        s->cr_total = -1;
        if (code == 200 && s->from > 0) {
            /* Il server ha ignorato il Range: fermiamoci SUBITO, prima che il corpo duplichi i
             * byte. Il motore troncherà e ricomincerà da zero. */
            s->ignored_range = 1;
            return 0;
        }
        return n;
    }
    if (ci_prefix(buf, n, "content-range:")) parse_content_range(s, buf, n);
    return n;
}

static size_t on_write(void* ptr, size_t size, size_t nmemb, void* ud)
{
    curl_state* s = (curl_state*)ud;
    size_t n = size * nmemb;
    if (n == 0) return 0;

    /* Limite = dimensione DICHIARATA: un server che promette 1 KB e ne manda 1 TB va fermato. */
    if (s->expected > 0 && s->from + s->written + (long long)n > s->expected) {
        s->too_much = 1;
        return 0;
    }

    size_t off = 0;
    while (off < n) {
#if defined(_WIN32) || defined(_WIN64)
        int w = fetch_write_fd(s->fd, (const char*)ptr + off, n - off);
#else
        ssize_t w = fetch_write_fd(s->fd, (const char*)ptr + off, n - off);
        if (w < 0 && errno == EINTR) continue;
#endif
        if (w <= 0) { s->io_error = 1; return 0; }
        off += (size_t)w;
    }
    s->written += (long long)n;

    if (s->t && s->t->cancel && s->t->cancel(s->t->cancel_user)) {
        s->cancelled = 1;
        return 0;
    }
    if (s->t && s->t->progress) {
        long long ms = now_ms();
        if (ms - s->last_progress_ms >= 100) {
            s->last_progress_ms = ms;
            double secs = (double)(ms - s->t0_ms) / 1000.0;
            double rate = secs > 0.0 ? (double)s->written / secs : 0.0;
            long long done = s->from + s->written;
            long long eta = (s->expected > 0 && rate > 1.0 && s->expected > done)
                          ? (long long)((double)(s->expected - done) / rate) : 0;
            s->t->progress(s->t->progress_user, done, s->expected, rate, eta);
        }
    }
    return n;
}

static int curl_retryable(CURLcode rc, long http, const curl_state* s)
{
    if (s->cancelled || s->too_much || s->io_error) return 0;
    if (rc == CURLE_HTTP_RETURNED_ERROR)   /* il server ha risposto 4xx/5xx */
        return (http == 408 || http == 429 || http >= 500) ? 1 : 0;
    if (rc == CURLE_OK) {
        if (http == 408 || http == 429 || http >= 500) return 1;
        return 0;
    }
    switch (rc) {
    case CURLE_COULDNT_CONNECT:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_PARTIAL_FILE:
    case CURLE_RECV_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_GOT_NOTHING:
    case CURLE_SSL_CONNECT_ERROR:
        return 1;
    default:
        return 0;
    }
}

static int curl_get(void* ctx, const char* url, const char* token,
                    long long from, long long expected_total, int fd,
                    gmb_fetch_transfer* t, char* err, size_t errcap)
{
    (void)ctx;
    if (err && errcap) err[0] = '\0';
    if (!url || !*url) { if (err && errcap) snprintf(err, errcap, "URL vuoto"); return -1; }

    CURL* c = curl_easy_init();
    if (!c) { if (err && errcap) snprintf(err, errcap, "libcurl non inizializzabile"); return -1; }

    curl_state s;
    memset(&s, 0, sizeof s);
    s.fd = fd;
    s.from = from;
    s.expected = expected_total;
    s.cr_start = -1;
    s.cr_total = -1;
    s.t0_ms = now_ms();
    s.last_progress_ms = s.t0_ms;
    s.t = t;

    struct curl_slist* hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Accept: application/octet-stream");
    hdrs = curl_slist_append(hdrs, "User-Agent: gmbasic-fetch/1");
    if (token && *token) {
        char auth[2048];
        /* Il token non compare MAI nei messaggi: solo nell'header della richiesta. */
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
        hdrs = curl_slist_append(hdrs, auth);
    }

    char range[64];
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
    /* Su 4xx/5xx NON si scrive il corpo: è testo d'errore, non byte dell'asset (un 401 di 2 byte
     * finirebbe nell'.incomplete e corromperebbe la ripresa). Lo status si legge dagli header. */
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &s);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, (long)((t->connect_timeout > 0) ? t->connect_timeout : 20));
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, (long)((t->low_speed_limit > 0) ? t->low_speed_limit : 1024));
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, (long)((t->low_speed_time > 0) ? t->low_speed_time : 60));
    if (from > 0) {
        snprintf(range, sizeof range, "%lld-", from);
        curl_easy_setopt(c, CURLOPT_RANGE, range);
    }

    CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);

    t->written = s.written;
    t->http_status = (s.http > 0) ? s.http : (long long)http;
    t->ranged = s.ranged;
    t->received_from = s.cr_start;
    t->received_total = s.cr_total;
    t->cancelled = s.cancelled;
    if (s.cancelled) { if (err && errcap) snprintf(err, errcap, "download annullato"); return -1; }
    if (s.too_much) {
        if (err && errcap) snprintf(err, errcap, "il server ha inviato più byte del dichiarato");
        return -1;
    }
    if (s.io_error) { if (err && errcap) snprintf(err, errcap, "scrittura su file fallita"); return -1; }

    /* Range ignorato: NON è un errore del trasporto — il motore tronca e ricomincia. */
    if (s.ignored_range) { t->ranged = 0; t->retryable = 0; return 0; }

    if (rc != CURLE_OK || t->http_status >= 400 || (t->http_status > 0 && t->http_status < 200)) {
        char safe[1024];
        url_for_log(url, safe, sizeof safe);
        t->retryable = curl_retryable(rc, (long)t->http_status, &s);
        if (err && errcap) {
            if (t->http_status == 401 || t->http_status == 403)
                snprintf(err, errcap, "HTTP %lld: accesso negato (token assente o non valido) — %s",
                         t->http_status, safe);
            else if (t->http_status == 404)
                snprintf(err, errcap, "HTTP 404: asset inesistente — %s", safe);
            else if (rc != CURLE_OK)
                snprintf(err, errcap, "%s (HTTP %lld) — %s", curl_easy_strerror(rc), t->http_status, safe);
            else
                snprintf(err, errcap, "HTTP %lld — %s", t->http_status, safe);
        }
        return -1;
    }
    return 0;
}

static const gmb_fetch_transport g_curl_transport = {
    GMB_FETCH_TRANSPORT_ABI,
    curl_get,
    NULL,
    NULL
};

const gmb_fetch_transport* gmb_fetch_transport_curl(void) { return &g_curl_transport; }

#else  /* !GMB_FETCH_HAVE_CURL */

const gmb_fetch_transport* gmb_fetch_transport_curl(void) { return NULL; }

#endif
