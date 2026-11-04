/* gmb_fetch_selfcheck.c — motore gmbfetch, provato OFFLINE con un trasporto finto.
 *
 * Il trasporto è il confine del sistema: sostituendolo con uno che dichiara intestazioni e byte
 * prestabiliti si può dimostrare in modo deterministico ciò che con la rete sarebbe casuale —
 * che un download interrotto RIPRENDE, che un server che ignora il Range non duplica i byte,
 * che un lock impedisce a due processi di scrivere lo stesso file, che uno sha sbagliato non
 * produce MAI il file finale. Nessuna rete, nessun modello, nessun file grande.
 */
#include "gmb_fetch.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#  include <process.h>
#  include <direct.h>
#  include <fcntl.h>
#  define TEST_MKDIR(p) _mkdir(p)
#  define TEST_OPENW(p) _open((p), _O_WRONLY | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE)
#  define TEST_WRITE(fd, b, n) _write((fd), (b), (unsigned)(n))
#  define TEST_CLOSE(fd) _close(fd)
#  define TEST_GETPID() _getpid()
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/file.h>
#  define TEST_MKDIR(p) mkdir((p), 0755)
#  define TEST_OPENW(p) open((p), O_WRONLY | O_CREAT | O_TRUNC, 0644)
#  define TEST_WRITE(fd, b, n) write((fd), (b), (n))
#  define TEST_CLOSE(fd) close(fd)
#  define TEST_GETPID() getpid()
#endif

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

/* ── trasporto finto ─────────────────────────────────────────────────────────────────────── */
typedef struct {
    const unsigned char* data;
    long long len;
    int ignore_range;      /* risponde 200 col corpo intero anche se from > 0   */
    int fail_attempts;     /* N tentativi falliscono (rete) prima di riuscire    */
    long long fail_after;  /* scrive N byte poi "la rete cade" (ripartibile)     */
    int bad_content_range; /* Content-Range che non combacia con la richiesta    */
    int honour_cancel;     /* chiama la callback di annullamento                 */
    int calls;
    long long from_seen[8];
    int n_from;
} fake_t;

static int fake_write_all(int fd, const unsigned char* p, long long n)
{
    long long off = 0;
    while (off < n) {
        long long chunk = (n - off) > 65536 ? 65536 : (n - off);
#if defined(_WIN32) || defined(_WIN64)
        int w = TEST_WRITE(fd, p + off, chunk);
#else
        long long w = TEST_WRITE(fd, p + off, chunk);
#endif
        if (w <= 0) return 0;
        off += w;
    }
    return 1;
}

static int fake_get(void* ctx, const char* url, const char* token, long long from,
                    long long expected_total, int fd, gmb_fetch_transfer* t,
                    char* err, size_t errcap)
{
    fake_t* f = (fake_t*)ctx;
    (void)url; (void)token; (void)expected_total;
    f->calls++;
    if (f->n_from < 8) f->from_seen[f->n_from++] = from;

    t->http_status = from > 0 ? (f->ignore_range ? 200 : 206) : 200;
    t->ranged = (from > 0 && !f->ignore_range);
    t->received_from = f->bad_content_range ? from + 5 : from;
    t->received_total = f->len;

    if (f->fail_attempts > 0) {
        f->fail_attempts--;
        t->retryable = 1;
        t->written = 0;
        if (err && errcap) snprintf(err, errcap, "rete finta: connessione interrotta");
        return -1;
    }

    /* Un server che ignora il Range manda il corpo INTERO (dal byte 0). */
    long long start = f->ignore_range ? 0 : from;
    if (start < 0 || start > f->len) start = 0;
    long long budget = f->len - start;
    long long cut = f->fail_after;
    if (cut > 0 && cut < budget) budget = cut;

    long long done = 0;
    while (done < budget) {
        long long chunk = (budget - done) > 16384 ? 16384 : (budget - done);
        if (!fake_write_all(fd, f->data + start + done, chunk)) {
            if (err && errcap) snprintf(err, errcap, "scrittura fallita nel trasporto finto");
            return -1;
        }
        done += chunk;
        t->written = done;
        if (f->honour_cancel && t->cancel && t->cancel(t->cancel_user)) {
            t->cancelled = 1;
            return -1;
        }
        if (t->progress) t->progress(t->progress_user, from + done, f->len, 0.0, 0);
    }

    if (f->fail_after > 0 && f->fail_after < (f->len - start)) {
        f->fail_after = 0;      /* una sola volta: al tentativo dopo si completa */
        t->retryable = 1;
        if (err && errcap) snprintf(err, errcap, "rete finta: caduta a metà");
        return -1;
    }
    return 0;
}

static gmb_fetch_transport g_fake = { GMB_FETCH_TRANSPORT_ABI, fake_get, NULL, NULL };

/* ── utilità di test ─────────────────────────────────────────────────────────────────────── */
static char g_dir[1024];

static long long file_size(const char* p)
{
    struct stat st;
    if (stat(p, &st) != 0) return -1;
    return (long long)st.st_size;
}

static int file_has(const char* p, const unsigned char* data, long long len)
{
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    unsigned char buf[8192];
    long long off = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (off + (long long)n > len || memcmp(buf, data + off, n) != 0) { fclose(f); return 0; }
        off += (long long)n;
    }
    fclose(f);
    return off == len;
}

static void fill_pattern(unsigned char* p, long long n)
{
    unsigned int x = 0x12345678u;
    for (long long i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        p[i] = (unsigned char)(x >> 24);
    }
}

static int write_partial(const char* path, const unsigned char* data, long long n)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(data, 1, (size_t)n, f);
    fclose(f);
    return w == (size_t)n;
}

static void rm(const char* p) { remove(p); }

/* Crea <g_dir>/<name> (le sottocartelle dei casi che scrivono un .incomplete "già iniziato"). */
static void mkdir_sub(const char* name)
{
    char d[1100];
    snprintf(d, sizeof d, "%s/%s", g_dir, name);
    TEST_MKDIR(d);
}

/* Annullamento immediato: la prima callback dice "basta". */
static int cancel_now(void* user) { (void)user; return 1; }

/* ── casi ────────────────────────────────────────────────────────────────────────────────── */
static void test_sha_stream(void)
{
    /* Vettori noti: "" e "abc". Poi un buffer grande contro la versione one-shot. */
    uint8_t d[32];
    char hex[65];
    gmb_sha256_stream s;
    gmb_sha256_stream_init(&s);
    gmb_sha256_stream_final(&s, d);
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", d[i]);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    gmb_sha256_stream_init(&s);
    gmb_sha256_stream_update(&s, "abc", 3);
    gmb_sha256_stream_final(&s, d);
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", d[i]);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    long long n = 200000;
    unsigned char* big = (unsigned char*)malloc((size_t)n);
    fill_pattern(big, n);
    char one[65], two[65];
    gmb_sha256_stream_init(&s);
    /* a blocchi irregolari: il contesto non può assumere update allineati */
    for (long long off = 0; off < n; ) {
        long long step = 7000;
        if (off + step > n) step = n - off;
        gmb_sha256_stream_update(&s, big + off, (size_t)step);
        off += step;
    }
    gmb_sha256_stream_final(&s, d);
    for (int i = 0; i < 32; i++) snprintf(two + i * 2, 3, "%02x", d[i]);
    char* alloc = gmb_sha256_hex(big, (size_t)n);
    snprintf(one, sizeof one, "%s", alloc ? alloc : "");
    free(alloc);
    CHECK(strcmp(one, two) == 0);

    char fp[1024];
    snprintf(fp, sizeof fp, "%s/sha_stream.bin", g_dir);
    FILE* f = fopen(fp, "wb");
    if (f) { fwrite(big, 1, (size_t)n, f); fclose(f); }
    char fh[65];
    CHECK(gmb_sha256_file_hex(fp, fh, sizeof fh) == 1);
    CHECK(strcmp(fh, one) == 0);
    rm(fp);
    free(big);
}

static void test_happy(void)
{
    const long long n = 120000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/happy/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(file_size(dest) == n);
    CHECK(file_has(dest, data, n));
    CHECK(fk.calls == 1);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(file_size(incom) == -1);              /* nessun temporaneo lasciato in giro */
    CHECK(gmb_fetch_verify_file(dest, sha, n) == 1);

    /* idempotenza: già presente e valido → nessuna nuova chiamata */
    rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(fk.calls == 1);

    free(sha);
    free(data);
}

static void test_resume(void)
{
    const long long n = 90000, have = 40000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    mkdir_sub("resume");
    snprintf(dest, sizeof dest, "%s/resume/brain.db", g_dir);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(write_partial(incom, data, have));

    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(fk.calls == 1);
    CHECK(fk.from_seen[0] == have);             /* RIPARTE dal byte già scaricato */
    CHECK(file_size(dest) == n);
    CHECK(file_has(dest, data, n));

    free(sha);
    free(data);
}

static void test_range_ignored(void)
{
    const long long n = 70000, have = 25000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    mkdir_sub("range");
    snprintf(dest, sizeof dest, "%s/range/brain.db", g_dir);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(write_partial(incom, data, have));

    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;
    fk.ignore_range = 1;    /* server che risponde 200 col corpo intero */

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(fk.calls == 2);                       /* primo tentativo + riavvio da zero */
    CHECK(fk.n_from >= 2 && fk.from_seen[0] == have && fk.from_seen[1] == 0);
    CHECK(file_size(dest) == n);
    CHECK(file_has(dest, data, n));              /* NESSUN byte duplicato */

    free(sha);
    free(data);
}

static void test_sha_mismatch(void)
{
    const long long n = 5000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/sha/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
    rq.size = n;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_SHA);
    CHECK(file_size(dest) == -1);               /* MAI un file finale non verificato */
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    CHECK(file_size(incom) == n);               /* conservato per diagnosi */
    rm(incom);
    free(data);
}

static void test_size_mismatch(void)
{
    const long long n = 4000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/size/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.size = n + 1000;      /* il manifest dichiara più byte di quanti ne arrivano */

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_SIZE);
    CHECK(file_size(dest) == -1);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    rm(incom);
    free(data);
}

static void test_retry_resumes_after_stall(void)
{
    const long long n = 60000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/retry/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;
    fk.fail_after = 30000;    /* la rete cade a metà: il retry deve riprendere da lì */

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(fk.calls == 2);
    CHECK(fk.n_from >= 2 && fk.from_seen[1] == 30000);
    CHECK(file_has(dest, data, n));

    free(sha);
    free(data);
}

static void test_no_retry_on_definitive_error(void)
{
    const long long n = 1000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/noretry/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;
    fk.bad_content_range = 1;   /* Content-Range incoerente: errore definitivo, niente retry */

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.size = n;

    /* Un errore NON riprovabile (es. Content-Range sbagliato): una sola chiamata. */
    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_RANGE);
    CHECK(fk.calls == 1);

    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    rm(incom);
    free(data);
}

/* Il re-resolver dell'URL (per gli asset GitHub l'URL firmato scade) deve essere INVOCATO
 * PRIMA DI OGNI tentativo, non una volta sola: un URL memorizzato al primo tentativo sarebbe
 * già scaduto al secondo. */
static int g_resolve_calls = 0;
static int test_resolve_cb(void* user, char* url, size_t cap, char* err, size_t errcap)
{
    (void)user; (void)err; (void)errcap;
    g_resolve_calls++;
    snprintf(url, cap, "https://example.invalid/asset?t=%d", g_resolve_calls);
    return 0;
}

static void test_resolve_each_attempt(void)
{
    const long long n = 20000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/resolve/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;
    fk.fail_attempts = 1;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;
    rq.resolve = test_resolve_cb;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;
    g_resolve_calls = 0;
    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(g_resolve_calls == 2);       /* uno per tentativo, non uno in tutto */
    CHECK(fk.calls == 2);

    free(sha);
    free(data);
}

static void test_no_space(void)
{
    const long long n = 1000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/space/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.size = n;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.disk_margin = 1LL << 50;   /* margine irragionevole: si ferma PRIMA di scaricare */
    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_SPACE);
    CHECK(fk.calls == 0);
    CHECK(file_size(dest) == -1);
    free(data);
}

static void test_lock(void)
{
    const long long n = 1000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/lock/brain.db", g_dir);
    char dir[1024], locks[1100], lockpath[1300];
    snprintf(dir, sizeof dir, "%s/lock", g_dir);
    snprintf(locks, sizeof locks, "%s/.locks", dir);
    TEST_MKDIR(dir);
    TEST_MKDIR(locks);
    snprintf(lockpath, sizeof lockpath, "%s/brain.db.lock", locks);

    /* Un altro "processo" tiene il lock. */
    int fd = TEST_OPENW(lockpath);
    CHECK(fd >= 0);
#if !defined(_WIN32) && !defined(_WIN64)
    CHECK(flock(fd, LOCK_EX | LOCK_NB) == 0);
#endif

    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.size = n;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.lock_wait_ms = 0;
    op.quiet = 1;
    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, &g_fake, err, sizeof err);
#if !defined(_WIN32) && !defined(_WIN64)
    CHECK(rc == GMB_FETCH_E_LOCKED);
    CHECK(fk.calls == 0);
#else
    (void)rc;   /* su Windows il lock è un handle esclusivo: coperto dal test end-to-end */
#endif
    TEST_CLOSE(fd);
    free(data);
}

static void test_cancel(void)
{
    const long long n = 80000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/cancel/brain.db", g_dir);
    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;
    fk.honour_cancel = 1;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.size = n;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;
    op.cancel = cancel_now;
    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_E_CANCEL);
    CHECK(file_size(dest) == -1);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    long long kept = file_size(incom);
    CHECK(kept > 0 && kept < n);      /* il lavoro fatto resta: il prossimo run riprende */
    rm(incom);
    free(data);
}

static void test_already_complete(void)
{
    const long long n = 30000;
    unsigned char* data = (unsigned char*)malloc((size_t)n);
    fill_pattern(data, n);
    char* sha = gmb_sha256_hex(data, (size_t)n);

    char dest[1200];
    snprintf(dest, sizeof dest, "%s/done/brain.db", g_dir);
    char incom[1300];
    gmb_fetch_incomplete_path(dest, incom, sizeof incom);
    char donedir[1100];
    snprintf(donedir, sizeof donedir, "%s/done", g_dir);
    TEST_MKDIR(donedir);
    CHECK(write_partial(incom, data, n));   /* .incomplete già completo */

    fake_t fk;
    memset(&fk, 0, sizeof fk);
    g_fake.ctx = &fk;
    fk.data = data;
    fk.len = n;

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = "https://example.invalid/brain.db";
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = n;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, NULL, &g_fake, err, sizeof err);
    CHECK(rc == GMB_FETCH_OK);
    CHECK(fk.calls == 0);             /* NESSUN GET: avrebbe risposto 416 */
    CHECK(file_has(dest, data, n));

    free(sha);
    free(data);
}

int main(void)
{
    const char* tmp = getenv("TMPDIR");
#if defined(_WIN32) || defined(_WIN64)
    if (!tmp || !*tmp) tmp = getenv("TEMP");
#endif
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s/gmb_fetch_selfcheck_%d", tmp, (int)TEST_GETPID());
    TEST_MKDIR(g_dir);

    test_sha_stream();
    test_happy();
    test_resume();
    test_range_ignored();
    test_sha_mismatch();
    test_size_mismatch();
    test_retry_resumes_after_stall();
    test_no_retry_on_definitive_error();
    test_resolve_each_attempt();
    test_no_space();
    test_lock();
    test_cancel();
    test_already_complete();

    printf("gmb_fetch_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
