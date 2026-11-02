/* gmb_pkg_pack_signed_selfcheck.c — firma Ed25519 detached del .gmbpkg.
 *
 * Complementare a `gmb_pkg_pack_selfcheck` (che copre elenco file/ignore/zip/estrazione):
 * qui si verifica SOLO la firma — entry `.gmbpkg.sig`/`.gmbpkg.pub`, validità con la chiave
 * trasportata, rilevamento della manomissione, assenza di firma senza chiave, variante HMAC.
 */
#include "gmb_runtime.h"
#include "gmb_pkg_pack.h"
#include "gmb_pkg_ed25519.h"
#include "gmb_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); g_fail = 1; } } while (0)

static void write_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static unsigned char* read_all(const char* p, size_t* len)
{
    FILE* f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* b = (unsigned char*)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

typedef struct { const char* name; char* data; size_t len; int found; } entry_t;

static int zip_cb(const char* name, const unsigned char* content, size_t clen, void* ctx)
{
    entry_t* e = (entry_t*)ctx;
    if (strcmp(name, e->name) != 0) return 0;
    e->data = (char*)malloc(clen + 1);
    if (e->data) {
        memcpy(e->data, content, clen);
        e->data[clen] = 0;
        e->len = clen;
        e->found = 1;
    }
    return 0;
}

static int zip_has(const char* zip, const char* entry)
{
    size_t n = 0;
    unsigned char* d = read_all(zip, &n);
    if (!d) return 0;
    entry_t e = { entry, NULL, 0, 0 };
    gmb_zip_foreach_mem(d, n, zip_cb, &e);
    free(e.data);
    free(d);
    return e.found;
}

int main(void)
{
    system("rm -rf /tmp/gmb_pack_signed_sc && mkdir -p /tmp/gmb_pack_signed_sc/src");
    const char* dir = "/tmp/gmb_pack_signed_sc";
    write_file("/tmp/gmb_pack_signed_sc/gmb-package.json",
               "{\n  \"name\": \"demo\",\n  \"version\": \"1.0.0\",\n  \"type\": \"source\",\n"
               "  \"exports\": { \"main\": \"src\" },\n  \"dependencies\": {}\n}\n");
    write_file("/tmp/gmb_pack_signed_sc/src/a.bas", "PRINT \"ciao\"\n");

    const char* zsign = "/tmp/gmb_pack_signed_sc/signed.gmbpkg";
    const char* zplain = "/tmp/gmb_pack_signed_sc/plain.gmbpkg";
    const char* zhmac = "/tmp/gmb_pack_signed_sc/hmac.gmbpkg";

    char priv[256] = "", pub[256] = "";
    CHECK(gmb_pkg_ed25519_keygen(priv, sizeof priv, pub, sizeof pub) == 1, "keygen ed25519");
    CHECK(priv[0] != 0 && pub[0] != 0, "keygen: chiavi non vuote");

    CHECK(gmb_pkg_pack_signed(dir, zsign, priv) == 1, "pack_signed");
    CHECK(zip_has(zsign, ".gmbpkg.sig"), "pack_signed: .gmbpkg.sig");
    CHECK(zip_has(zsign, ".gmbpkg.pub"), "pack_signed: .gmbpkg.pub");
    CHECK(zip_has(zsign, "gmb-package.json"), "pack_signed: manifest");
    CHECK(zip_has(zsign, "src/a.bas"), "pack_signed: sorgente");

    /* la firma è valida con la chiave trasportata nel pacchetto */
    size_t zlen = 0;
    unsigned char* zd = read_all(zsign, &zlen);
    CHECK(zd != NULL, "zip firmato leggibile");
    if (zd) {
        entry_t man = { "gmb-package.json", NULL, 0, 0 };
        entry_t sig = { ".gmbpkg.sig", NULL, 0, 0 };
        entry_t pb = { ".gmbpkg.pub", NULL, 0, 0 };
        gmb_zip_foreach_mem(zd, zlen, zip_cb, &man);
        gmb_zip_foreach_mem(zd, zlen, zip_cb, &sig);
        gmb_zip_foreach_mem(zd, zlen, zip_cb, &pb);
        if (man.found && sig.found && pb.found) {
            CHECK(gmb_pkg_ed25519_verify(pb.data, (const unsigned char*)man.data, strlen(man.data), sig.data) == 1,
                  "firma: valida con la chiave del pacchetto");
            /* la pub trasportata è quella della chiave privata usata */
            char pub2[256] = "";
            CHECK(gmb_pkg_ed25519_public(priv, pub2, sizeof pub2) == 1, "ed25519_public");
            CHECK(!strcmp(pub2, pb.data), "firma: .gmbpkg.pub == public(priv)");
            /* manomissione del manifest → firma NON valida */
            char* tampered = strdup(man.data);
            CHECK(tampered != NULL, "copia manifest");
            if (tampered) {
                tampered[1] = (tampered[1] == 'x') ? 'y' : 'x';
                CHECK(gmb_pkg_ed25519_verify(pb.data, (const unsigned char*)tampered, strlen(tampered), sig.data) == 0,
                      "firma: manomissione rilevata");
                free(tampered);
            }
        } else {
            CHECK(0, "zip firmato: entry mancanti");
        }
        free(man.data); free(sig.data); free(pb.data);
        free(zd);
    }

    /* senza chiave: nessuna entry di firma */
    CHECK(gmb_pkg_pack(dir, zplain, NULL) == 1, "pack senza firma");
    CHECK(!zip_has(zplain, ".gmbpkg.sig"), "pack senza firma: nessun .gmbpkg.sig");
    CHECK(!zip_has(zplain, ".gmbpkg.pub"), "pack senza firma: nessun .gmbpkg.pub");

    /* variante HMAC: .gmbpkg.sig (hex) presente, nessuna .pub */
    CHECK(gmb_pkg_pack(dir, zhmac, "segreto") == 1, "pack con hmac");
    CHECK(zip_has(zhmac, ".gmbpkg.sig"), "pack hmac: .gmbpkg.sig");
    CHECK(!zip_has(zhmac, ".gmbpkg.pub"), "pack hmac: nessuna .gmbpkg.pub");

    system("rm -rf /tmp/gmb_pack_signed_sc");
    if (g_fail) { fprintf(stderr, "pkg_pack_signed_selfcheck: FALLITO\n"); return 1; }
    printf("pkg_pack_signed_selfcheck: OK\n");
    return 0;
}
