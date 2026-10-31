/* gmb_pkg_manifest_selfcheck.c — parser del manifest (path-based). */
#include "gmb_pkg_manifest.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    const char* js =
        "{"
        "\"name\":\"duckdb\",\"version\":\"1.5.6\",\"contentType\":\"c-ffi\",\"edition\":\"any\","
        "\"backends\":[\"il\",\"c99\"],"
        "\"ffi\":{\"api\":\"c-abi\",\"headers\":[\"include/duckdb.h\"],\"libs\":{\"osx-arm64\":\"lib/osx-arm64/libduckdb.dylib\"}},"
        "\"binaries\":{\"c99\":{\"osx-arm64\":\"bin/c99/libduckdb.dylib\"}},"
        "\"dependencies\":{\"gmb-sqlite3\":\"^1.0.0\",\"gmb-z\":\"*\"},"
        "\"overrides\":{\"libz\":\"1.2.13\"}"
        "}";
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
    CHECK(m != NULL);
    char b[256], k[128], v[256];

    CHECK(gmb_pkg_manifest_get(m, "name", b, sizeof(b)) == 1 && strcmp(b, "duckdb") == 0);
    CHECK(gmb_pkg_manifest_get(m, "version", b, sizeof(b)) == 1 && strcmp(b, "1.5.6") == 0);
    CHECK(gmb_pkg_manifest_get(m, "contentType", b, sizeof(b)) == 1 && strcmp(b, "c-ffi") == 0);
    CHECK(gmb_pkg_manifest_get(m, "fi.absent", b, sizeof(b)) == 0);
    CHECK(gmb_pkg_manifest_has(m, "edition") == 1 && gmb_pkg_manifest_has(m, "zzz") == 0);

    CHECK(gmb_pkg_manifest_get(m, "ffi.api", b, sizeof(b)) == 1 && strcmp(b, "c-abi") == 0);
    CHECK(gmb_pkg_manifest_get(m, "ffi.libs.osx-arm64", b, sizeof(b)) == 1
          && strcmp(b, "lib/osx-arm64/libduckdb.dylib") == 0);
    CHECK(gmb_pkg_manifest_get(m, "binaries.c99.osx-arm64", b, sizeof(b)) == 1
          && strcmp(b, "bin/c99/libduckdb.dylib") == 0);

    CHECK(gmb_pkg_manifest_count(m, "backends") == 2);
    CHECK(gmb_pkg_manifest_item(m, "backends", 1, k, sizeof(k), v, sizeof(v)) == 1 && strcmp(v, "c99") == 0);

    CHECK(gmb_pkg_manifest_count(m, "dependencies") == 2);
    int found_sqlite = 0, found_z = 0;
    for (int i = 0; i < 2; i++) {
        gmb_pkg_manifest_item(m, "dependencies", i, k, sizeof(k), v, sizeof(v));
        if (strcmp(k, "gmb-sqlite3") == 0 && strcmp(v, "^1.0.0") == 0) found_sqlite = 1;
        if (strcmp(k, "gmb-z") == 0 && strcmp(v, "*") == 0) found_z = 1;
    }
    CHECK(found_sqlite && found_z);

    CHECK(gmb_pkg_manifest_count(m, "ffi.headers") == 1);
    CHECK(gmb_pkg_manifest_item(m, "ffi.headers", 0, k, sizeof(k), v, sizeof(v)) == 1 && strcmp(v, "include/duckdb.h") == 0);
    CHECK(gmb_pkg_manifest_get(m, "overrides.libz", b, sizeof(b)) == 1 && strcmp(b, "1.2.13") == 0);
    CHECK(gmb_pkg_manifest_parse("[]") == NULL); /* non oggetto → NULL */

    gmb_pkg_manifest_free(m);

    /* edit manifest: sezione MANCANTE (creazione), sezione esistente, remove */
    system("rm -rf /tmp/gmb_man; mkdir -p /tmp/gmb_man");
    const char* mp = "/tmp/gmb_man/gmb-package.json";
    char rbuf[4096];
    { FILE* wf = fopen(mp, "w"); if (wf) { fputs("{\"name\":\"p\",\"version\":\"1.0.0\"}", wf); fclose(wf); } }
    CHECK(gmb_pkg_manifest_set_dep(mp, "dependencies", "b", "^2.0.0") == 1);      /* sezione assente → creata */
    { FILE* rf = fopen(mp, "r"); size_t n = rf ? fread(rbuf, 1, sizeof(rbuf) - 1, rf) : 0; if (rf) fclose(rf); rbuf[n] = 0;
      CHECK(strstr(rbuf, "\"b\"") && strstr(rbuf, "^2.0.0")); }
    CHECK(gmb_pkg_manifest_set_dep(mp, "devDependencies", "t", "*") == 1);         /* seconda sezione */
    CHECK(gmb_pkg_manifest_set_dep(mp, "dependencies", "c", "^3.0.0") == 1);       /* sezione già presente */
    CHECK(gmb_pkg_manifest_remove_dep(mp, "b") == 1);
    { FILE* rf = fopen(mp, "r"); size_t n = rf ? fread(rbuf, 1, sizeof(rbuf) - 1, rf) : 0; if (rf) fclose(rf); rbuf[n] = 0;
      CHECK(strstr(rbuf, "\"b\"") == NULL && strstr(rbuf, "\"c\"") != NULL && strstr(rbuf, "\"t\"") != NULL); }
    system("rm -rf /tmp/gmb_man");

    printf("gmb_pkg_manifest_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
