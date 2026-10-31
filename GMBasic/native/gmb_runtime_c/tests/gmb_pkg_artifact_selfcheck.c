/* gmb_pkg_artifact_selfcheck.c — selezione artefatto + classificazione contentType. */
#include "gmb_pkg_artifact.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)
static void wr(const char* p, const char* s) { FILE* f = fopen(p, "w"); if (f) { fputs(s, f); fclose(f); } }

int main(void)
{
    system("rm -rf /tmp/gmb_art; mkdir -p /tmp/gmb_art/pkg/src /tmp/gmb_art/pkg/bin/il /tmp/gmb_art/pkg/lib/osx-arm64");

    /* source */
    wr("/tmp/gmb_art/pkg/gmb-package.json", "{\"name\":\"a\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"exports\":[\"src/\"]}");
    wr("/tmp/gmb_art/pkg/src/a.bas", "PRINT 1\n");
    gmb_pkg_artifact_t* a = gmb_pkg_artifact_select("/tmp/gmb_art/pkg", "il", "osx-arm64");
    CHECK(a && gmb_pkg_artifact_ct(a) == 0);
    CHECK(gmb_pkg_artifact_source_dir(a) && strstr(gmb_pkg_artifact_source_dir(a), "src"));
    CHECK(strcmp(gmb_pkg_artifact_backend(a), "any") == 0);
    gmb_pkg_artifact_free(a);

    /* dotnet con .dll */
    wr("/tmp/gmb_art/pkg/gmb-package.json", "{\"name\":\"b\",\"version\":\"1.0.0\",\"contentType\":\"dotnet\",\"exports\":[\"src/\"],\"binaries\":{\"il\":{\"osx-arm64\":\"bin/il/B.dll\"}}}");
    wr("/tmp/gmb_art/pkg/bin/il/B.dll", "MZ");
    a = gmb_pkg_artifact_select("/tmp/gmb_art/pkg", "il", "osx-arm64");
    CHECK(a && gmb_pkg_artifact_ct(a) == 1);
    CHECK(gmb_pkg_artifact_binary(a) && strstr(gmb_pkg_artifact_binary(a), "B.dll"));
    CHECK(strcmp(gmb_pkg_artifact_backend(a), "il") == 0);
    gmb_pkg_artifact_free(a);

    /* c-ffi: lib + header */
    wr("/tmp/gmb_art/pkg/gmb-package.json", "{\"name\":\"c\",\"version\":\"1.0.0\",\"contentType\":\"c-ffi\",\"ffi\":{\"api\":\"c-abi\",\"headers\":[\"include/c.h\"],\"libs\":{\"osx-arm64\":\"lib/osx-arm64/libc.dylib\"}}}");
    wr("/tmp/gmb_art/pkg/lib/osx-arm64/libc.dylib", "MZ");
    a = gmb_pkg_artifact_select("/tmp/gmb_art/pkg", "c99", "osx-arm64");
    CHECK(a && gmb_pkg_artifact_ct(a) == 3);
    CHECK(gmb_pkg_artifact_ffi_lib_count(a) == 1);
    CHECK(gmb_pkg_artifact_ffi_lib(a, 0) && strstr(gmb_pkg_artifact_ffi_lib(a, 0), "libc.dylib"));
    CHECK(gmb_pkg_artifact_ffi_header_count(a) == 1);
    gmb_pkg_artifact_free(a);

    /* contentType "vm" senza chunk → fallback a Source (ha exports) */
    wr("/tmp/gmb_art/pkg/gmb-package.json", "{\"name\":\"d\",\"version\":\"1.0.0\",\"contentType\":\"vm\",\"exports\":[\"src/\"]}");
    a = gmb_pkg_artifact_select("/tmp/gmb_art/pkg", "il", "osx-arm64");
    CHECK(a && gmb_pkg_artifact_ct(a) == 0);
    gmb_pkg_artifact_free(a);

    system("rm -rf /tmp/gmb_art");
    printf("gmb_pkg_artifact_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
