/* gmb_pkg_lock_selfcheck.c — lock + integrità (sha256 per-file). */
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "gmb_pkg_lock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static void wr(const char* path, const char* s) { FILE* f = fopen(path, "w"); if (f) { fputs(s, f); fclose(f); } }

int main(void)
{
    const char* root = "/tmp/gmb_lock_selfcheck";
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'; mkdir -p '%s'", root, root);
    system(cmd);

    char cache[1024], pkgdir[1024], sp[1024], mf[1024], sf[1024], lock[1024];
    snprintf(cache, sizeof(cache), "%s/cache", root);
    snprintf(pkgdir, sizeof(pkgdir), "%s/pk/1.0.0", cache);
    snprintf(sp, sizeof(sp), "%s/src", pkgdir);
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", sp);
    system(cmd);

    const char* json = "{\"name\":\"pk\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"exports\":[\"src/\"]}";
    snprintf(mf, sizeof(mf), "%s/gmb-package.json", pkgdir);
    wr(mf, json);
    snprintf(sf, sizeof(sf), "%s/pk.bas", sp);
    wr(sf, "PRINT \"pk\"\n");

    char fp[8192];
    CHECK(gmb_pkg_pkg_hash_files(pkgdir, json, fp, sizeof(fp)) == 1);
    CHECK(strstr(fp, "gmb-package.json:") != NULL);
    CHECK(strstr(fp, "src/pk.bas:") != NULL);

    snprintf(lock, sizeof(lock), "%s/gmb-package.lock", root);
    char body[16384];
    snprintf(body, sizeof(body), "pk@1.0.0|Source|any|osx-arm64|%s", fp);
    CHECK(gmb_pkg_lock_write(lock, body) == 1);

    gmb_pkg_lock_t* l = gmb_pkg_lock_read(lock);
    CHECK(l != NULL);
    CHECK(gmb_pkg_lock_count(l) == 1);
    char name[128], ver[64], ct[64], be[64], pl[64];
    CHECK(gmb_pkg_lock_entry(l, 0, name, sizeof(name), ver, sizeof(ver), ct, sizeof(ct), be, sizeof(be), pl, sizeof(pl)) == 1);
    CHECK(strcmp(name, "pk") == 0 && strcmp(ver, "1.0.0") == 0 && strcmp(ct, "Source") == 0);
    CHECK(gmb_pkg_lock_hash_count(l, 0) == 2);

    char rep[8192];
    CHECK(gmb_pkg_lock_verify(l, cache, rep, sizeof(rep)) == 0); /* integro */
    wr(sf, "PRINT \"TAMPERED\"\n");
    CHECK(gmb_pkg_lock_verify(l, cache, rep, sizeof(rep)) == 1); /* manomesso */
    CHECK(strstr(rep, "hash diverso") != NULL);

    gmb_pkg_lock_free(l);
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
    system(cmd);

    printf("gmb_pkg_lock_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
