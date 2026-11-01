/* ============================================================================
 * gmb_vm_bsp.c — pagine BSP sulla VM/C (§BSP-VM).
 * Flusso: gmb_bsp_try_serve (gmb_runtime_c) → gmb_vm_bsp_render:
 *   1. sidecar "<file>.bsp.chunk" (A1) presente e fresco (mtime >= .bsp),
 *   2. VM indipendente per richiesta (gmb_vm_run_mt: la pagina può FETCHare),
 *   3. globali REQ/QUERY/FORM/SESSION seminati per NOME (slot dal chunk),
 *   4. PRINT catturato → corpo HTML (semantica ASP-classic),
 *   5. hook del net-watcher ripristinati sulla VM principale.
 * Rendering serializzato (mutex): i hook di cattura sono statici.
 * ============================================================================ */
#include "gmb_vm_bsp.h"

#include "gmb_bsp.h"
#include "gmb_chunk.h"
#include "gmb_network.h"
#include "gmb_vm_core.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- Cattura PRINT: buffer PER-RENDER (contesto = job), non più statico ----
 * Prima la cattura era un buffer statico e l'unico modo per non corromperlo era serializzare
 * TUTTI i render con un mutex (un solo core utile). Ora ogni render ha il proprio buffer
 * (`print_ctx` = &job) e la sincronizzazione si riduce a:
 *   - rwlock: i render di pagine dichiarate "concorrenti" prendono il lato LETTURA (paralleli),
 *     tutti gli altri il lato SCRITTURA (serializzati come prima);
 *   - un piccolo mutex (g_net_mu) attorno al solo ripristino dei net-hook, che sono globali. */
typedef struct { char* buf; size_t n, cap; } bsp_cap_t;
static bsp_cap_t g_fallback_cap;          /* solo per chiamate senza ctx (non dovrebbero esistere) */
static pthread_mutex_t g_fallback_mu = PTHREAD_MUTEX_INITIALIZER;

static int cap_grow(bsp_cap_t* c, size_t extra)
{
    if (c->n + extra + 1 <= c->cap) return 1;
    size_t nc = c->cap ? c->cap : 1024;
    while (nc < c->n + extra + 1) nc *= 2;
    char* p = (char*)realloc(c->buf, nc);
    if (!p) return 0;
    c->buf = p;
    c->cap = nc;
    return 1;
}

static void cap_app(bsp_cap_t* c, const char* s, size_t len)
{
    if (!c || !s || !len) return;
    if (!cap_grow(c, len)) return;
    memcpy(c->buf + c->n, s, len);
    c->n += len;
    c->buf[c->n] = '\0';
}

/* Contesto di cattura: il job del render. Senza ctx si usa il fallback statico (protetto:
 * non deve accadere su questo percorso, ma un print fuori contesto non deve perdere dati). */
static bsp_cap_t* cap_of(void* ctx) { return ctx ? (bsp_cap_t*)ctx : &g_fallback_cap; }

static void bsp_print_str(void* ctx, const char* s, uint32_t len)
{
    if (ctx) { cap_app((bsp_cap_t*)ctx, s, len); return; }
    pthread_mutex_lock(&g_fallback_mu); cap_app(&g_fallback_cap, s, len); pthread_mutex_unlock(&g_fallback_mu);
}

static void bsp_print_num(void* ctx, double v)
{
    char b[64];
    int n = snprintf(b, sizeof(b), "%.17g", v);
    if (n <= 0) return;
    if (ctx) { cap_app((bsp_cap_t*)ctx, b, (size_t)n); return; }
    pthread_mutex_lock(&g_fallback_mu); cap_app(&g_fallback_cap, b, (size_t)n); pthread_mutex_unlock(&g_fallback_mu);
}

static void bsp_print_nl(void* ctx)
{
    if (ctx) { cap_app((bsp_cap_t*)ctx, "\n", 1); return; }
    pthread_mutex_lock(&g_fallback_mu); cap_app(&g_fallback_cap, "\n", 1); pthread_mutex_unlock(&g_fallback_mu);
}

/* ---- Parsing query string (k=v&k2=v2, url-encoded) → dict ---- */
static int bsp_hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void bsp_urldec(char* s)
{
    char* r = s;
    char* w = s;
    while (*r)
    {
        if (*r == '+') { *w++ = ' '; r++; }
        else if (*r == '%' && bsp_hexval((unsigned char)r[1]) >= 0 && bsp_hexval((unsigned char)r[2]) >= 0)
        {
            *w++ = (char)((bsp_hexval((unsigned char)r[1]) << 4) | bsp_hexval((unsigned char)r[2]));
            r += 3;
        }
        else *w++ = *r++;
    }
    *w = '\0';
}

static gmb_dict_t bsp_parse_query(const char* query)
{
    gmb_dict_t d = gmb_dict_new();
    if (!query || !*query) return d;
    char* buf = strdup(query);
    if (!buf) return d;
    char* p = buf;
    while (*p)
    {
        char* amp = strchr(p, '&');
        if (amp) *amp = '\0';
        char* eq = strchr(p, '=');
        char* val = (char*)"";
        if (eq) { *eq = '\0'; val = eq + 1; }
        bsp_urldec(p);
        if (eq) bsp_urldec(val);
        if (*p) gmb_dict_set(&d, p, gmb_make_val_string(val));
        if (!amp) break;
        p = amp + 1;
    }
    free(buf);
    return d;
}

/* ---- Rendering: sincronizzazione "leggera" ----
 * Il lock del render non è più un mutex unico: è un rwlock.
 *   - pagine dichiarate CONCORRENTI (`gmb_bsp_shared_set_concurrent_pages`): lato LETTURA →
 *     più render in parallelo (buffer di cattura per-render, quindi nessuna interferenza);
 *   - tutte le altre: lato SCRITTURA → serializzate come prima (stato C condiviso non protetto:
 *     vedi Knowledge/BSP-Engine-Audit-Concorrenza).
 * Il ripristino dei net-hook resta serializzato da un mutex dedicato (stato globale del VM/C).
 * Le pagine "concorrenti" sono quelle il cui stato passa INTERAMENTE dal client managed (hook):
 * l'elenco è esplicito e verificabile, non dedotto. */
static pthread_rwlock_t g_render_lock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t g_net_mu = PTHREAD_MUTEX_INITIALIZER;
static char g_concurrent_pages[1024];
static int g_concurrent_any = 0;

void gmb_bsp_set_concurrent_pages(const char* csv)
{
    snprintf(g_concurrent_pages, sizeof g_concurrent_pages, "%s", csv ? csv : "");
    g_concurrent_any = g_concurrent_pages[0] != '\0';
}

/* True se il path (basename) è nell'elenco delle pagine concorrenti. */
static int bsp_page_is_concurrent(const char* url_path)
{
    if (!g_concurrent_any || !url_path) return 0;
    const char* base = strrchr(url_path, '/');
    base = base ? base + 1 : url_path;
    size_t bl = strlen(base);
    const char* p = g_concurrent_pages;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char* e = p;
        while (*e && *e != ',') e++;
        size_t l = (size_t)(e - p);
        while (l > 0 && (p[l - 1] == ' ' || p[l - 1] == '\t')) l--;
        if (l == bl && strncasecmp(p, base, l) == 0) return 1;
        p = e;
    }
    return 0;
}

/* Il server HTTP nativo serve le connessioni in SERIE nel loop di accept:
 * eseguire la pagina nel thread chiamante bloccherebbe il loop, e una pagina
 * che FETCHa lo stesso server andrebbe in self-deadlock. Il render gira quindi
 * su un worker dedicato; il chiamante (thread del server) aspetta qui — il loop
 * di accept resta libero di servire il FETCH della pagina. */
typedef struct {
    gmb_chunk_t* chunk;
    const char* url_path;
    const char* query;
    const char* host;
    char* html;
    int status;
    bsp_cap_t cap;            /* cattura PRINT di QUESTO render */
    int concurrent;           /* 1 = lato lettura del rwlock */
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int done;
} bsp_render_job_t;

static void* bsp_render_worker(void* arg)
{
    bsp_render_job_t* j = (bsp_render_job_t*)arg;

    /* Pagine concorrenti → lato lettura (parallelo); le altre → lato scrittura. */
    if (j->concurrent) pthread_rwlock_rdlock(&g_render_lock);
    else pthread_rwlock_wrlock(&g_render_lock);

    gmb_vm_t vm;
    gmb_vm_init(&vm, j->chunk);
    vm.print_ctx = &j->cap;          /* cattura PER-RENDER */
    vm.print_str = bsp_print_str;
    vm.print_num = bsp_print_num;
    vm.print_newline = bsp_print_nl;

    /* Globali seminati per NOME (parità BspDispatch managed). REQ/SESSION sono DIM
     * normali della pagina (hanno un vero slot in chunk->global_names) → gmb_vm_seed_global.
     * FORM/QUERY sono keyword riservate (TokenKind.Form/SqlQuery, MAI dichiarabili con
     * DIM): il compilatore .NET non assegna loro un vero slot globale, risolvendole
     * invece con OP_LOAD_DYNAMIC_SCOPE — gmb_vm_seed_global qui sarebbe SEMPRE un no-op
     * silenzioso (nessuno slot da trovare per nome in vm_apply_bsp_seeds), esattamente
     * il bug fissato lato PCodeVM/BspDispatch (C#): usa gmb_vm_set_dynamic_scope, letto
     * da OP_LOAD_DYNAMIC_SCOPE indipendentemente da chunk->global_names. */
    gmb_dict_t qd = bsp_parse_query(j->query);
    gmb_vm_set_dynamic_scope(&vm, "QUERY", gmb_make_val_dict(qd));
    gmb_dict_t rd = gmb_dict_new();
    gmb_dict_set(&rd, "Method", gmb_make_val_string("GET"));
    gmb_dict_set(&rd, "Path", gmb_make_val_string(j->url_path ? j->url_path : ""));
    gmb_dict_set(&rd, "Body", gmb_make_val_string(""));
    gmb_dict_set(&rd, "Query", gmb_make_val_dict(qd));
    /* Headers (parità BuildRequestDict C#): essenziale per le pagine BSP che si
     * auto-localizzano il registry via Host (es. RegistryBsp/index.bsp →
     * "http://" + REQ.Headers.Get("Host")). Senza, REQ.Headers è un dict vuoto e
     * la pagina BSP del registry non può fare FETCH verso lo stesso server. */
    {
        gmb_dict_t hd = gmb_dict_new();
        if (j->host && *j->host)
            gmb_dict_set(&hd, "Host", gmb_make_val_string(j->host));
        gmb_dict_set(&rd, "Headers", gmb_make_val_dict_take(&hd));
        gmb_dict_free(&hd);
    }
    gmb_vm_seed_global(&vm, "REQ", gmb_make_val_dict_take(&rd));
    gmb_dict_t fd = gmb_dict_new();
    gmb_vm_set_dynamic_scope(&vm, "FORM", gmb_make_val_dict_take(&fd));
    gmb_dict_t sd = gmb_dict_new();
    gmb_vm_seed_global(&vm, "SESSION", gmb_make_val_dict_take(&sd));
    gmb_dict_free(&qd);
    gmb_dict_free(&rd);
    gmb_dict_free(&fd);
    gmb_dict_free(&sd);

    j->cap.n = 0;
    if (j->cap.buf) j->cap.buf[0] = '\0';

    int rc = gmb_vm_run_mt(&vm);

    /* La run secondaria ha sovrascritto i block hook globali del net-watcher:
     * ripristina quelli della VM principale (handler HTTP parcheggiati). Sono stato GLOBALE
     * del VM/C → piccolo mutex dedicato (non serve serializzare tutto il render). */
    pthread_mutex_lock(&g_net_mu);
    gmb_vm_restore_net_hooks(gmb_vm_get_http_vm());
    pthread_mutex_unlock(&g_net_mu);

    if (rc != 0)
    {
        j->html = strdup("500 BSP Error: page execution failed");
        j->status = 500;
    }
    else
    {
        j->html = j->cap.buf ? strdup(j->cap.buf) : strdup("");
        j->status = 200;
    }

    free(j->cap.buf);
    j->cap.buf = NULL;
    j->cap.n = j->cap.cap = 0;
    pthread_rwlock_unlock(&g_render_lock);

    gmb_vm_free(&vm);
    gmb_chunk_free(j->chunk);

    pthread_mutex_lock(&j->mu);
    j->done = 1;
    pthread_cond_signal(&j->cv);
    pthread_mutex_unlock(&j->mu);
    return NULL;
}

#define GMB_BSP_MAX_CHUNK (32 * 1024 * 1024)

static uint8_t* bsp_read_file(const char* path, size_t* out_len, long max_len)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    rewind(f);
    if (n <= 0 || n > max_len) { fclose(f); return NULL; }
    uint8_t* b = (uint8_t*)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f);
    fclose(f);
    if (r != (size_t)n) { free(b); return NULL; }
    if (out_len) *out_len = r;
    return b;
}

int gmb_vm_bsp_render(const char* bsp_path, const char* url_path,
                      const char* query, const char* host,
                      char** out_html, int* out_status)
{
    if (out_html) *out_html = NULL;
    if (out_status) *out_status = 0;
    if (!bsp_path || !*bsp_path) return 0;

    char chunk_path[1088];
    snprintf(chunk_path, sizeof(chunk_path), "%s.chunk", bsp_path);

    /* Freschezza: sidecar più vecchio della pagina → ricompilare (fallback). */
    struct stat st_bsp, st_chunk;
    if (stat(chunk_path, &st_chunk) != 0) return 0;
    if (stat(bsp_path, &st_bsp) == 0 && st_chunk.st_mtime < st_bsp.st_mtime) return 0;

    size_t chunk_len = 0;
    uint8_t* data = bsp_read_file(chunk_path, &chunk_len, GMB_BSP_MAX_CHUNK);
    if (!data) return 0;
    gmb_chunk_t* chunk = gmb_chunk_load(data, chunk_len, true);
    free(data);
    if (!chunk)
    {
        if (out_html) *out_html = strdup("500 BSP chunk invalid");
        if (out_status) *out_status = 500;
        return 1;
    }

    bsp_render_job_t job;
    job.chunk = chunk; /* ownership passa al worker */
    job.url_path = url_path;
    job.query = query;
    job.host = host;
    job.html = NULL;
    job.status = 0;
    job.done = 0;
    job.cap.buf = NULL; job.cap.n = 0; job.cap.cap = 0;
    /* Il lato del rwlock si decide UNA volta, dal path della pagina: nell'elenco delle pagine
     * concorrenti (stato tutto nel client managed) → lettura; altrimenti scrittura. */
    job.concurrent = bsp_page_is_concurrent(url_path);
    pthread_mutex_init(&job.mu, NULL);
    pthread_cond_init(&job.cv, NULL);

    pthread_t th;
    if (pthread_create(&th, NULL, bsp_render_worker, &job) != 0)
    {
        /* Degrado sicuro: nessun thread disponibile. */
        gmb_chunk_free(chunk);
        pthread_mutex_destroy(&job.mu);
        pthread_cond_destroy(&job.cv);
        if (out_html) *out_html = strdup("500 BSP Error: no thread");
        if (out_status) *out_status = 500;
        return 1;
    }

    pthread_mutex_lock(&job.mu);
    while (!job.done) pthread_cond_wait(&job.cv, &job.mu);
    pthread_mutex_unlock(&job.mu);
    pthread_join(th, NULL);
    pthread_mutex_destroy(&job.mu);
    pthread_cond_destroy(&job.cv);

    if (out_html) *out_html = job.html;
    else free(job.html);
    if (out_status) *out_status = job.status;
    return 1;
}

void gmb_vm_bsp_register(void)
{
    gmb_bsp_set_vm_renderer(gmb_vm_bsp_render);
}
