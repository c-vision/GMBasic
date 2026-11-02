/* brain_registry_selfcheck.c — registry `brains.json`, manifest e risoluzione artefatti.
 *
 * Isolato: nessuna scrittura in config/rag/grafi/cervelli dell'utente (hook
 * gmb_paths_test_override, niente variabili d'ambiente).
 */
#include "gmb_runtime.h"
#include "gmb_brain_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

static void mkdirs(const char* p)
{
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char* q = tmp + 1; *q; q++)
        if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}

static void write_file(const char* path, const char* text)
{
    char dir[4096];
    snprintf(dir, sizeof dir, "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; mkdirs(dir); }
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static char* read_file(const char* p)
{
    FILE* f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)n, f);
    b[r] = 0;
    fclose(f);
    return b;
}

int main(void)
{
    char tmpl[] = "/tmp/gmb_brain_reg_sc.XXXXXX";
    char* root = mkdtemp(tmpl);
    CHECK(root != NULL, "mkdtemp");
    if (!root) return 1;

    char cfg[4200], rag[4200], graphs[4200], brains[4200], bcache[4200], bexp[4200];
    snprintf(cfg, sizeof cfg, "%s/config", root);
    snprintf(rag, sizeof rag, "%s/rag", root);
    snprintf(graphs, sizeof graphs, "%s/graphs", root);
    snprintf(brains, sizeof brains, "%s/brains", root);
    snprintf(bcache, sizeof bcache, "%s/bcache", root);
    snprintf(bexp, sizeof bexp, "%s/bexp", root);
    gmb_paths_test_override("configDir", cfg);
    gmb_paths_test_override("ragDir", rag);
    gmb_paths_test_override("graphsDir", graphs);
    gmb_paths_test_override("brainsDir", brains);
    gmb_paths_test_override("brainsCacheDir", bcache);
    gmb_paths_test_override("brainsExportsDir", bexp);

    /* cervello "vault" installato: brains/vault/graphs/vault.json */
    char gpath[4300];
    snprintf(gpath, sizeof gpath, "%s/vault/graphs/vault.json", brains);
    write_file(gpath, "{\"nodes\":[{\"id\":\"a.md\",\"title\":\"a\"}],\"edges\":[],\"dangling\":[],\"folderCounts\":{}}");

    /* ── disponibilità e risoluzione ── */
    char avail[64] = "";
    CHECK(gmb_brain_reg_available("vault", avail, sizeof avail) == 1, "available: vault scaricato");
    CHECK(!strcmp(avail, "graph"), "available: solo graph");
    char found[4300] = "";
    CHECK(gmb_brain_reg_graph_json("vault", found, sizeof found) == 1, "graph_json risolto");
    CHECK(strstr(found, "/vault/graphs/vault.json") != NULL, "graph_json path");
    CHECK(gmb_brain_reg_rag_db("vault", found, sizeof found) == 0, "rag_db assente (nessun .db)");
    CHECK(gmb_brain_reg_available("ghost", avail, sizeof avail) == 0, "available: sconosciuto = nessuno");

    /* ── register / info / attached ── */
    CHECK(gmb_brain_reg_register("vault", "src-test", brains, "graph", NULL, "1.2.3", 7, NULL) == 1, "register");
    gmb_brain_rec_t rec;
    memset(&rec, 0, sizeof rec);
    CHECK(gmb_brain_reg_info("vault", &rec) == 1, "info dopo register");
    CHECK(!strcmp(rec.mode, "graph"), "info: mode");
    CHECK(!strcmp(rec.version, "1.2.3"), "info: version");
    CHECK(rec.docs == 7, "info: docs");
    CHECK(rec.attached == 0, "info: non collegato");
    CHECK(gmb_brain_reg_info("ghost", &rec) == 0, "info: sconosciuto = 0");

    CHECK(gmb_brain_use("vault") == 1, "use: collegamento");
    CHECK(gmb_brain_reg_attached("vault") == 1, "attached dopo use");
    CHECK(gmb_brain_reg_info("vault", &rec) == 1 && rec.attached == 1, "info: attached dopo use");
    CHECK(gmb_brain_detach("vault") == 1, "detach");
    CHECK(gmb_brain_reg_attached("vault") == 0, "non attached dopo detach");

    /* il file contiene la voce (parità col C#: brains.json leggibile) */
    char json_path[4300];
    snprintf(json_path, sizeof json_path, "%s/brains.json", cfg);
    char* js = read_file(json_path);
    CHECK(js != NULL, "brains.json scritto");
    if (js) {
        CHECK(strstr(js, "\"name\": \"vault\"") != NULL, "brains.json: nome");
        CHECK(strstr(js, "\"version\": \"1.2.3\"") != NULL, "brains.json: versione");
        free(js);
    }

    /* ── manifest: hash, verifica, corruzione ── */
    char* h = gmb_brain_manifest_hash_file(gpath);
    CHECK(h != NULL, "hash_file");
    if (h) {
        CHECK(strncmp(h, "sha256:", 7) == 0, "hash: prefisso sha256:");
        CHECK(strlen(h) == 7 + 64, "hash: 64 hex");
        int upper = 1;
        for (const char* q = h + 7; *q; q++)
            if (!((*q >= '0' && *q <= '9') || (*q >= 'A' && *q <= 'F'))) upper = 0;
        CHECK(upper, "hash: MAIUSCOLO (parità Convert.ToHexString)");

        char mpath[4300];
        snprintf(mpath, sizeof mpath, "%s/vault/brain.manifest.json", brains);
        char mjs[2048];
        snprintf(mjs, sizeof mjs,
                 "{\n  \"format\": \"gmbrain\",\n  \"name\": \"vault\",\n"
                 "  \"files\": {\n    \"graphs/vault.json\": \"%s\"\n  }\n}\n", h);
        write_file(mpath, mjs);
        char err[512] = "";
        char vroot[4300];
        snprintf(vroot, sizeof vroot, "%s/vault", brains);
        CHECK(gmb_brain_manifest_verify(vroot, err, sizeof err) == 0, "manifest: verificato");
        CHECK(gmb_brain_manifest_verify("/tmp/nonexistent-root-xyz", err, sizeof err) == 1, "manifest assente = 1");

        /* corrompe il file → checksum diverso */
        write_file(gpath, "{\"nodes\":[],\"edges\":[],\"dangling\":[],\"folderCounts\":{}}");
        CHECK(gmb_brain_manifest_verify(vroot, err, sizeof err) == -1, "manifest: corruzione rilevata");
        write_file(gpath, "{\"nodes\":[{\"id\":\"a.md\"}],\"edges\":[],\"dangling\":[],\"folderCounts\":{}}");
        free(h);
    }

    /* ── unregister ── */
    CHECK(gmb_brain_reg_unregister("vault") == 1, "unregister");
    {
        /* l'ARTEFATTO resta su disco (unregister non cancella file): il cervello è ancora
         * disponibile, ma NON è più una voce del registry. */
        char* j2 = read_file(json_path);
        CHECK(j2 != NULL, "brains.json dopo unregister");
        if (j2) {
            CHECK(strstr(j2, "\"name\": \"vault\"") == NULL, "brains.json: voce rimossa");
            free(j2);
        }
        memset(&rec, 0, sizeof rec);
        CHECK(gmb_brain_reg_info("vault", &rec) == 1, "info: ancora disponibile dal disco");
        CHECK(rec.mode[0] != 0, "info: mode derivata dal disco");
    }
    CHECK(gmb_brain_reg_unregister("vault") == 0, "unregister doppio = 0");

    gmb_paths_test_override(NULL, NULL);
    char clean[4200];
    snprintf(clean, sizeof clean, "rm -rf '%s'", root);
    if (system(clean) != 0) { /* best-effort */ }

    if (g_fail) { fprintf(stderr, "brain_registry_selfcheck: FALLITO\n"); return 1; }
    printf("brain_registry_selfcheck: OK\n");
    return 0;
}
