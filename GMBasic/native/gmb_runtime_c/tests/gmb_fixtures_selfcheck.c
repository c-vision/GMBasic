/* gmb_fixtures_selfcheck.c — fixture on demand, provate OFFLINE con un trasporto finto.
 *
 * Si prova ciò che conta: l'archivio viene scaricato ed ESTRATTO nella cartella della fixture,
 * una fixture già presente NON viene riscaricata, uno sha sbagliato non lascia nulla a metà, e
 * una fixture non dichiarata è un errore esplicito (non un "fatto").
 */
#include "gmb_fixtures.h"
#include "gmb_compress.h"
#include "gmb_commonlib.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static char g_dir[1024];

static long long fsize(const char* p)
{
    struct stat st;
    return stat(p, &st) == 0 ? (long long)st.st_size : -1;
}

static int write_file(const char* p, const void* d, size_t n)
{
    FILE* f = fopen(p, "wb");
    if (!f) return 0;
    size_t w = fwrite(d, 1, n, f);
    fclose(f);
    return w == n;
}

/* trasporto finto: serve il contenuto di un file locale */
typedef struct { const char* path; int calls; } fake_t;
static fake_t g_fake_state;

static int fake_get(void* ctx, const char* url, const char* token, long long from,
                    long long expected_total, int fd, gmb_fetch_transfer* t,
                    char* err, size_t errcap)
{
    (void)url; (void)token; (void)expected_total;
    fake_t* f = (fake_t*)ctx;
    f->calls++;
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
    snprintf(g_dir, sizeof g_dir, "%s/gmb_fixtures_selfcheck_%d", tmp, (int)getpid());
    gmb_fs_mkdir_p(g_dir);

    /* archivio "fixture": una cartella con due file, zippata (entry = contenuto della cartella) */
    char src[1200], zip[1200], one[1300], two[1300];
    snprintf(src, sizeof src, "%s/src", g_dir);
    gmb_fs_mkdir_p(src);
    snprintf(one, sizeof one, "%s/a.csv", src);
    snprintf(two, sizeof two, "%s/b.csv", src);
    CHECK(write_file(one, "alfa,1\nbeta,2\n", 13));
    CHECK(write_file(two, "gamma,3\n", 8));
    snprintf(zip, sizeof zip, "%s/fixture.zip", g_dir);
    char* sources[1]; sources[0] = src;
    CHECK(gmb_zip_create(zip, sources, 1) == 0);
    char sha[65], bad[65];
    CHECK(gmb_sha256_file_hex(zip, sha, sizeof sha) == 1);
    snprintf(bad, sizeof bad, "%064d", 0);
    long long zsize = fsize(zip);
    CHECK(zsize > 0);

    /* manifest con due voci: una valida, una con sha sbagliato, una "sconosciuta" non elencata */
    char mpath[1200], mjs[2048];
    snprintf(mpath, sizeof mpath, "%s/fixtures-manifest.json", g_dir);
    int n = snprintf(mjs, sizeof mjs,
        "{ \"version\": \"1.0.0\", \"fixtures\": ["
        "  { \"name\": \"demo\", \"latest\": \"2.0.0\", \"versions\": ["
        "     { \"version\": \"2.0.0\", \"source\": \"url\", \"url\": \"https://example.invalid/demo.zip\","
        "       \"sha256\": \"%s\", \"size\": %lld } ] },"
        "  { \"name\": \"rotto\", \"latest\": \"1.0.0\", \"versions\": ["
        "     { \"version\": \"1.0.0\", \"source\": \"url\", \"url\": \"https://example.invalid/r.zip\","
        "       \"sha256\": \"%s\", \"size\": %lld } ] }"
        "] }", sha, zsize, bad, zsize);
    CHECK(n > 0 && write_file(mpath, mjs, (size_t)n));

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = 1;
    char destroot[1200], out[1200], err[512];
    snprintf(destroot, sizeof destroot, "%s/Fixtures", g_dir);
    gmb_fs_mkdir_p(destroot);

    memset(&g_fake_state, 0, sizeof g_fake_state);
    g_fake_state.path = zip;
    g_fake.ctx = &g_fake_state;

    /* 1) pull: scarica + estrae in <dest>/<nome>/ */
    int rc = gmb_fixtures_pull_from(mpath, "demo", NULL, destroot, 0, &g_fake, &op, out, sizeof out, err, sizeof err);
    if (rc != GMB_FIX_OK) printf("  [pull] rc=%d err=%s\n", rc, err);
    CHECK(rc == GMB_FIX_OK);
    CHECK(g_fake_state.calls == 1);
    char f1[1300], f2[1300];
    snprintf(f1, sizeof f1, "%s/demo/a.csv", destroot);
    snprintf(f2, sizeof f2, "%s/demo/b.csv", destroot);
    CHECK(fsize(f1) == 13);
    CHECK(fsize(f2) == 8);

    /* 2) già presente: nessun download */
    int before = g_fake_state.calls;
    rc = gmb_fixtures_pull_from(mpath, "demo", NULL, destroot, 0, &g_fake, &op, out, sizeof out, err, sizeof err);
    CHECK(rc == GMB_FIX_OK);
    CHECK(g_fake_state.calls == before);

    /* 3) --force: riscarica (e sostituisce) */
    rc = gmb_fixtures_pull_from(mpath, "demo", NULL, destroot, 1, &g_fake, &op, out, sizeof out, err, sizeof err);
    CHECK(rc == GMB_FIX_OK);
    CHECK(g_fake_state.calls == before + 1);
    CHECK(fsize(f1) == 13);

    /* 4) sha sbagliato: errore di download, NIENTE cartella installata */
    char broken[1300];
    snprintf(broken, sizeof broken, "%s/rotto", destroot);
    rc = gmb_fixtures_pull_from(mpath, "rotto", NULL, destroot, 0, &g_fake, &op, out, sizeof out, err, sizeof err);
    CHECK(rc == GMB_FIX_E_FETCH);
    CHECK(!gmb_fs_isdir(broken));

    /* 5) fixture non dichiarata: errore esplicito */
    rc = gmb_fixtures_pull_from(mpath, "inesistente", NULL, destroot, 0, &g_fake, &op, out, sizeof out, err, sizeof err);
    CHECK(rc == GMB_FIX_E_NOT_FOUND);

    /* 6) versione non dichiarata */
    rc = gmb_fixtures_pull_from(mpath, "demo", "9.9.9", destroot, 0, &g_fake, &op, out, sizeof out, err, sizeof err);
    CHECK(rc == GMB_FIX_E_NOT_FOUND);

    /* 7) `ensure` (il compilatore/REPL la usa prima di eseguire): scarica SOLO ciò che manca,
     *    dalla fixture dichiarata dal programma (`"fixtures": [...]` in app.settings.json). */
    {
        char app[1200], appdir[1200], fdir[1300];
        snprintf(appdir, sizeof appdir, "%s/Fixtures2", g_dir);
        gmb_fs_mkdir_p(appdir);
        snprintf(app, sizeof app, "%s/app.settings.json", g_dir);
        char ajs[512];
        int al = snprintf(ajs, sizeof ajs, "{\"fixtures\": [\"demo\"]}");
        CHECK(write_file(app, ajs, (size_t)al));

        /* destinazione: una radice nuova (gmb_fixtures_root non e' sovrascrivibile → si usa la
         * pull diretta con dest_root; l'ensure e' coperto dal caso "già presente" qui sotto) */
        char out2[1200], e2[512];
        /* manifest e destinazione ESPLICITI: il test non tocca shared/samples/Fixtures reale */
        char dest2[1200];
        snprintf(dest2, sizeof dest2, "%s/FxEnsure", g_dir);
        gmb_fs_mkdir_p(dest2);
        int n1 = gmb_fixtures_ensure_declared_from(mpath, dest2, app, &g_fake, &op, e2, sizeof e2);
        CHECK(n1 == 1);                                     /* "demo" mancava → scaricata */
        char got[1400];
        snprintf(got, sizeof got, "%s/demo/a.csv", dest2);
        CHECK(fsize(got) == 13);
        /* seconda chiamata: già presente → 0 */
        CHECK(gmb_fixtures_ensure_declared_from(mpath, dest2, app, &g_fake, &op, e2, sizeof e2) == 0);

        /* "ensure" su un app.settings.json SENZA fixtures: nessun download */
        char ajs2[128];
        int al2 = snprintf(ajs2, sizeof ajs2, "{\"paths\": {\"x\": 1}}");
        CHECK(write_file(app, ajs2, (size_t)al2));
        int before3 = g_fake_state.calls;
        CHECK(gmb_fixtures_ensure_declared_from(mpath, dest2, app, &g_fake, &op, e2, sizeof e2) == 0);
        CHECK(g_fake_state.calls == before3);
        gmb_fs_rmtree(appdir);
        gmb_fs_rmtree(dest2);
    }

    printf("gmb_fixtures_selfcheck: %d check, %d fail\n", checks, fails);
    gmb_fs_rmtree(g_dir);
    return fails ? 1 : 0;
}
