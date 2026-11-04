/* gmb_brains_selfcheck.c — cervelli come dati: risoluzione, manifest, installazione.
 *
 * Offline e deterministico: HOME è reindirizzata a una cartella temporanea (quindi
 * `~/.gmbasic/brains` è sandboxata e i cervelli veri dell'utente NON vengono toccati) e il
 * trasporto HTTP è finto. Si prova ciò che conta:
 *   - il manifest dichiara i cervelli (`gmb_brains_available`);
 *   - l'installazione scarica, estrae e VERIFICA (sha sbagliato = niente cervello installato);
 *   - il cervello installato si trova con `gmb_brain_lookup` (cache) e ha le modalità giuste;
 *   - un asset PRIVATO senza token è RIFIUTATO (fail-closed), senza nemmeno provare a scaricare;
 *   - installazione idempotente (seconda chiamata = zero download) e rimozione.
 */
#include "gmb_brains.h"
#include "gmb_compress.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static char g_dir[1024];
static char g_home[1100];

static void mkdir_p(const char* p)
{
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char* q = tmp + 1; *q; q++) { if (*q != '/') continue; *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}

static long long fsize(const char* p)
{
    struct stat st;
    return stat(p, &st) == 0 ? (long long)st.st_size : -1;
}

static int write_file(const char* p, const void* data, size_t n)
{
    FILE* f = fopen(p, "wb");
    if (!f) return 0;
    size_t w = fwrite(data, 1, n, f);
    fclose(f);
    return w == n;
}

static int read_file(const char* p, char* out, size_t cap)
{
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = 0;
    return 1;
}

/* ── trasporto finto: serve il contenuto di un file locale ───────────────────────────────── */
typedef struct { const char* path; int calls; int fail_status; } fake_t;
static fake_t g_fake_state;

static int fake_get(void* ctx, const char* url, const char* token, long long from,
                    long long expected_total, int fd, gmb_fetch_transfer* t,
                    char* err, size_t errcap)
{
    (void)url; (void)token; (void)expected_total;
    fake_t* f = (fake_t*)ctx;
    f->calls++;
    if (f->fail_status >= 400) {
        t->http_status = f->fail_status;
        t->retryable = 0;
        if (err && errcap) snprintf(err, errcap, "HTTP %d", f->fail_status);
        return -1;
    }
    FILE* in = fopen(f->path, "rb");
    if (!in) { if (err && errcap) snprintf(err, errcap, "file finto assente"); return -1; }
    fseek(in, (long)from, SEEK_SET);
    char buf[8192];
    size_t n;
    long long total = 0;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        size_t off = 0;
        while (off < n) {
            ssize_t w = write(fd, buf + off, n - off);
            if (w <= 0) { fclose(in); return -1; }
            off += (size_t)w;
        }
        total += (long long)n;
    }
    fclose(in);
    t->http_status = from > 0 ? 206 : 200;
    t->ranged = from > 0;
    t->received_from = from;
    t->received_total = -1;
    t->written = total;
    return 0;
}

static gmb_fetch_transport g_fake = { GMB_FETCH_TRANSPORT_ABI, fake_get, NULL, NULL };

int main(void)
{
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s/gmb_brains_selfcheck_%d", tmp, (int)getpid());
    mkdir_p(g_dir);
    snprintf(g_home, sizeof g_home, "%s/home", g_dir);
    mkdir_p(g_home);

    /* HOME sandboxata: `~/.gmbasic/brains` è della prova, non dell'utente. */
    setenv("HOME", g_home, 1);

    char brain[64];
    snprintf(brain, sizeof brain, "demo%d", (int)getpid());

    /* 1) sorgente del cervello + zip (gmb_zip_create usa la radice comune: entry relative) */
    char src[1200], srcrag[1200], srcman[1200], zip[1200];
    snprintf(src, sizeof src, "%s/src", g_dir);
    snprintf(srcrag, sizeof srcrag, "%s/rag", src);
    mkdir_p(srcrag);
    unsigned char blob[4096];
    unsigned int x = 99u;
    for (size_t i = 0; i < sizeof blob; i++) { x = x * 1664525u + 1013904223u; blob[i] = (unsigned char)(x >> 24); }
    char db[1300];
    snprintf(db, sizeof db, "%s/rag/%s.db", src, brain);
    CHECK(write_file(db, blob, sizeof blob));
    /* un cervello può essere disponibile come RAG **e** come GRAPH: il pacchetto porta entrambi */
    char gdir[1300], gfile[1350];
    snprintf(gdir, sizeof gdir, "%s/graphs", src);
    mkdir_p(gdir);
    snprintf(gfile, sizeof gfile, "%s/graphs/%s.json", src, brain);
    CHECK(write_file(gfile, "{\"nodes\":[{\"id\":\"a\"}],\"edges\":[]}", 34));
    snprintf(srcman, sizeof srcman, "%s/brain.manifest.json", src);
    char man[512];
    int mn = snprintf(man, sizeof man, "{\"format\":\"gmbrain/1\",\"name\":\"%s\",\"version\":\"1.2.0\"}", brain);
    CHECK(write_file(srcman, man, (size_t)mn));
    snprintf(zip, sizeof zip, "%s/brain.gmbrain.zip", g_dir);
    char* sources[1]; sources[0] = src;
    CHECK(gmb_zip_create(zip, sources, 1) == 0);
    long long zsize = fsize(zip);
    CHECK(zsize > 0);
    char zsha[65];
    CHECK(gmb_sha256_file_hex(zip, zsha, sizeof zsha) == 1);
    char bad_sha[65];
    snprintf(bad_sha, sizeof bad_sha, "%064d", 0);

    /* 2) manifest con un cervello pubblico e uno PRIVATO (senza token: deve fallire chiuso) */
    char mpath[1200];
    snprintf(mpath, sizeof mpath, "%s/brains-manifest.json", g_dir);
    char mjs[4096];
    int n = snprintf(mjs, sizeof mjs,
        "{ \"version\": \"1.0.0\", \"brains\": ["
        "  { \"name\": \"%s\", \"description\": \"prova\", \"latest\": \"1.2.0\", \"versions\": ["
        "     { \"version\": \"1.2.0\", \"source\": \"url\", \"url\": \"https://example.invalid/%s.zip\","
        "       \"sha256\": \"%s\", \"size\": %lld } ] },"
        "  { \"name\": \"%s-priv\", \"latest\": \"1.0.0\", \"versions\": ["
        "     { \"version\": \"1.0.0\", \"source\": \"github-asset\","
        "       \"url\": \"https://api.github.com/repos/x/y/releases/assets/1\", \"sha256\": \"%s\" } ] }"
        "] }", brain, brain, zsha, zsize, brain, bad_sha);
    CHECK(n > 0 && write_file(mpath, mjs, (size_t)n));

    /* 3) manifest dichiarato */
    char avail[2048];
    int na = gmb_brains_available_from(mpath, avail, sizeof avail);
    CHECK(na > 0);
    CHECK(strstr(avail, brain) != NULL);
    {
        char priv[80];
        snprintf(priv, sizeof priv, "%s-priv", brain);
        CHECK(strstr(avail, priv) != NULL);
    }

    /* 4) sha sbagliato → NESSUN cervello installato */
    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;
    char out_dir[1200], err[512];
    memset(&g_fake_state, 0, sizeof g_fake_state);
    g_fake_state.path = zip;
    g_fake.ctx = &g_fake_state;

    char mjs_bad[4096];
    snprintf(mjs_bad, sizeof mjs_bad, "%s", mjs);
    {
        /* stesso manifest ma con sha del cervello sbagliato */
        char* p = strstr(mjs_bad, zsha);
        if (p) memcpy(p, bad_sha, 64);
    }
    char mpath_bad[1200];
    snprintf(mpath_bad, sizeof mpath_bad, "%s/brains-bad.json", g_dir);
    CHECK(write_file(mpath_bad, mjs_bad, strlen(mjs_bad)));
    int rc = gmb_brains_install_from_manifest(mpath_bad, brain, NULL, NULL, &g_fake, &op,
                                              out_dir, sizeof out_dir, err, sizeof err);
    CHECK(rc == GMB_BRAIN_E_FETCH);
    CHECK(g_fake_state.calls == 1);
    CHECK(out_dir[0] == 0);
    char look[1200];
    CHECK(gmb_brain_lookup(brain, NULL, look, sizeof look) == 0);   /* non installato */

    /* 5) fail-closed: asset privato senza token, senza nemmeno una richiesta */
    memset(&g_fake_state, 0, sizeof g_fake_state);
    g_fake_state.path = zip;
    char priv[80];
    snprintf(priv, sizeof priv, "%s-priv", brain);
    rc = gmb_brains_install_from_manifest(mpath, priv, NULL, NULL, &g_fake, &op,
                                          out_dir, sizeof out_dir, err, sizeof err);
    CHECK(rc == GMB_BRAIN_E_UNAUTHORIZED);
    CHECK(g_fake_state.calls == 0);                 /* fail-closed: nessun download tentato */

    /* 6) installazione vera dal manifest.
     * Prima si butta l'`.incomplete` lasciato dal tentativo con sha sbagliato: è lavoro
     * ("verificato male") che il motore riuserebbe — qui vogliamo provare il download. */
    {
        char inc[1300];
        snprintf(inc, sizeof inc, "%s/.gmbasic/brains/.downloads/%s-1.2.0.gmbrain.zip.incomplete", g_home, brain);
        remove(inc);
    }
    memset(&g_fake_state, 0, sizeof g_fake_state);
    g_fake_state.path = zip;
    rc = gmb_brains_install_from_manifest(mpath, brain, NULL, NULL, &g_fake, &op,
                                          out_dir, sizeof out_dir, err, sizeof err);
    if (rc != GMB_BRAIN_OK) printf("  [install] rc=%d err=%s\n", rc, err);
    CHECK(rc == GMB_BRAIN_OK);
    CHECK(g_fake_state.calls == 1);
    CHECK(strstr(out_dir, ".gmbasic/brains") != NULL);   /* è la CACHE, non vendor */
    char dbpath[1400];
    snprintf(dbpath, sizeof dbpath, "%s/rag/%s.db", out_dir, brain);
    CHECK(fsize(dbpath) == (long long)sizeof blob);      /* il file c'è VERO (estratto) */

    /* 6bis) il DB RAG si risolve dalla CACHE (è dove `AI BRAIN INSTALL` mette il cervello):
     * è ciò che usano il RAG C e `AI.QAANSWER`, non più solo <ragDir>/<nome>.db. */
    {
        char ragp[1400];
        CHECK(gmb_brain_rag_db(brain, ragp, sizeof ragp) == 1);
        CHECK(strstr(ragp, ".gmbasic/brains/") != NULL);
        CHECK(strstr(ragp, "rag/") != NULL);
    }
    /* 6ter) e il GRAFO si risolve allo stesso modo (stesso cervello, cartella graphs/) */
    {
        char grp[1400];
        CHECK(gmb_brain_graph_json(brain, grp, sizeof grp) == 1);
        CHECK(strstr(grp, ".gmbasic/brains/") != NULL);
        CHECK(strstr(grp, "graphs/") != NULL);
        /* un cervello inesistente non ha né RAG né grafo */
        char none[1400];
        CHECK(gmb_brain_graph_json("cervello_mai_installato", none, sizeof none) == 0);
    }

    /* 7) risoluzione: lookup, versioni, modalità, lista */
    char dir[1200], vers[512], modes[64], list[2048];
    int cnt = 0;
    CHECK(gmb_brain_lookup(brain, NULL, dir, sizeof dir) == 1);
    CHECK(strcmp(dir, out_dir) == 0);
    CHECK(gmb_brain_lookup(brain, "1.2.0", dir, sizeof dir) == 1);
    CHECK(gmb_brain_lookup(brain, "9.9.9", dir, sizeof dir) == 0);
    CHECK(gmb_brain_versions(brain, vers, sizeof vers) == 1 && strcmp(vers, "1.2.0") == 0);
    CHECK(gmb_brain_modes(out_dir, brain, modes, sizeof modes) == 1 && strcmp(modes, "rag,graph") == 0);
    CHECK(gmb_brain_list(list, sizeof list, &cnt) == 1 && cnt >= 1);
    CHECK(strstr(list, brain) != NULL);
    char sp[2048];
    CHECK(gmb_brains_search_path(sp, sizeof sp) == 1 && strstr(sp, ".gmbasic/brains") != NULL);

    /* 8) idempotenza: già installato → nessun download */
    int before = g_fake_state.calls;
    rc = gmb_brains_install_from_manifest(mpath, brain, NULL, NULL, &g_fake, &op,
                                          out_dir, sizeof out_dir, err, sizeof err);
    CHECK(rc == GMB_BRAIN_OK);
    CHECK(g_fake_state.calls == before);

    /* 9) installazione da SORGENTE esplicita (zip locale): stesso cervello, versione dal manifest */
    rc = gmb_brains_install_source(brain, zip, &g_fake, &op, out_dir, sizeof out_dir, err, sizeof err);
    CHECK(rc == GMB_BRAIN_OK);
    CHECK(strstr(out_dir, "1.2.0") != NULL);

    /* 10) uno zip che NON è quel cervello viene rifiutato (nome nel brain.manifest.json) */
    rc = gmb_brains_install_source("altro-nome", zip, &g_fake, &op, out_dir, sizeof out_dir, err, sizeof err);
    CHECK(rc == GMB_BRAIN_E_INVALID);

    /* 11) rimozione dalla cache (e anche della cartella <nome>/ se resta vuota) */
    CHECK(gmb_brain_remove(brain, "1.2.0") == 1);
    CHECK(gmb_brain_lookup(brain, NULL, dir, sizeof dir) == 0);
    {
        char parent[1300];
        snprintf(parent, sizeof parent, "%s/.gmbasic/brains/%s", g_home, brain);
        CHECK(fsize(parent) == -1);   /* non è un file... */
        struct stat st;
        CHECK(stat(parent, &st) != 0);   /* ...e non esiste nemmeno come cartella */
    }

    /* 11ter) NESSUN fallback su shared/rag: un cervello che non è in nessuna radice NON ha un
     * DB, e il chiamante deve poter dire che manca (prima si "risolveva" un path inesistente). */
    {
        char nope[1400];
        CHECK(gmb_brain_rag_db("cervello_mai_installato", nope, sizeof nope) == 0);
        CHECK(nope[0] == '\0');
    }

    /* 11bis) le cartelle CONDIVISE del monorepo (rag/, graphs/) si risolvono in shared/ ANCHE se
     * la sottocartella non esiste ancora: dopo lo spostamento dei cervelli nella cache,
     * `shared/rag/` e' vuota/assente e la vecchia regola ricadeva su <root>/rag (sporcizia in
     * GMBasic/). La condizione ora e' sull'esistenza di <root>/../shared. */
    {
        char rag[4096], graphs[4096];
        gmb_paths_rag_dir(rag, sizeof rag);
        gmb_paths_graphs_dir(graphs, sizeof graphs);
        /* default = CACHE utente (dati PRODOTTI dal compilatore), mai il checkout */
        CHECK(strstr(rag, ".gmbasic/rag") != NULL);
        CHECK(strstr(rag, "/GMBasic/rag") == NULL);
        CHECK(strstr(rag, "/shared/rag") == NULL);
        CHECK(strstr(graphs, ".gmbasic/graphs") != NULL);
    }

    /* 12) token GitHub: 0600 si legge, permessi larghi si RIFIUTANO (fail-closed) */
    {
        char tdir[1200], tpath[1300], tok[256];
        snprintf(tdir, sizeof tdir, "%s/.gmbasic", g_home);
        mkdir_p(tdir);
        snprintf(tpath, sizeof tpath, "%s/github.token", tdir);
        CHECK(write_file(tpath, "ghp_selftest\n", 13));
        if (chmod(tpath, 0600) == 0) {
            CHECK(gmb_brains_github_token(tok, sizeof tok) == 1);
            CHECK(strcmp(tok, "ghp_selftest") == 0);
        }
        if (chmod(tpath, 0644) == 0) {
            CHECK(gmb_brains_github_token(tok, sizeof tok) == 0);   /* token leggibile da altri: rifiutato */
        }
        remove(tpath);
    }

    /* 13) INDICE di un programma distribuito: `<exe>/brains.txt` + snapshot del manifest in
     *     `<exe>/settings/` → installazione in `<exe>/brains/<nome>/<versione>` (prima della
     *     cache nell'ordine di ricerca). È ciò che fa `gmbbrains`. */
    {
        char exedir[1200], settdir[1300], mpath2[1400], index[1400], want[1500], outb[1200], errb[512];
        snprintf(exedir, sizeof exedir, "%s/exe", g_dir);
        snprintf(settdir, sizeof settdir, "%s/settings", exedir);
        mkdir_p(exedir);
        mkdir_p(settdir);
        snprintf(mpath2, sizeof mpath2, "%s/brains-manifest.json", settdir);
        CHECK(write_file(mpath2, mjs, strlen(mjs)));
        snprintf(index, sizeof index, "%s/brains.txt", exedir);
        char idx[512];
        int il = snprintf(idx, sizeof idx, "# cervelli del programma\n%s\t1.2.0\n", brain);
        CHECK(write_file(index, idx, (size_t)il));

        memset(&g_fake_state, 0, sizeof g_fake_state);
        g_fake_state.path = zip;
        g_fake.ctx = &g_fake_state;
        int n = gmb_brains_fetch_index(index, exedir, NULL, NULL, &g_fake, &op, errb, sizeof errb);
        if (n != 1) printf("  [index] n=%d err=%s\n", n, errb);
        CHECK(n == 1);
        snprintf(want, sizeof want, "%s/brains/%s/1.2.0/rag/%s.db", exedir, brain, brain);
        CHECK(fsize(want) == (long long)sizeof blob);
        snprintf(want, sizeof want, "%s/brains/%s/1.2.0/graphs/%s.json", exedir, brain, brain);
        CHECK(fsize(want) == 34);
        /* seconda passata: niente da scaricare */
        int before2 = g_fake_state.calls;
        int n2 = gmb_brains_fetch_index(index, exedir, NULL, NULL, &g_fake, &op, errb, sizeof errb);
        CHECK(n2 == 0);
        CHECK(g_fake_state.calls == before2);
        /* `--only` con un altro nome: non installa nulla */
        int n3 = gmb_brains_fetch_index(index, exedir, "altro", NULL, &g_fake, &op, errb, sizeof errb);
        CHECK(n3 == 0);
        (void)outb;
    }

    printf("gmb_brains_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
