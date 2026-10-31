/* gmb_pkg_resolver_selfcheck.c — resolver (builder ABI): transitivi, override, locked, local, errore. */
#include "gmb_pkg_resolver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static void wr(const char* p, const char* s) { FILE* f = fopen(p, "w"); if (f) { fputs(s, f); fclose(f); } }
static void mk(const char* cache, const char* name, const char* ver, const char* json)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/%s/%s'", cache, name, ver);
    system(cmd);
    char p[2400];
    snprintf(p, sizeof(p), "%s/%s/%s/gmb-package.json", cache, name, ver);
    wr(p, json);
}
static const char* ver_of(gmb_pkg_resolve_result_t* r, const char* name, char* nb, size_t ncap, char* vb, size_t vcap, char* ib, size_t icap, char* lb, size_t lcap)
{
    for (int i = 0; i < gmb_pkg_result_count(r); i++)
        if (gmb_pkg_result_item(r, i, nb, ncap, vb, vcap, ib, icap, lb, lcap) && strcmp(nb, name) == 0)
            return vb;
    return NULL;
}

int main(void)
{
    const char* cache = "/tmp/gmb_res/cache";
    system("rm -rf /tmp/gmb_res; mkdir -p /tmp/gmb_res/cache /tmp/gmb_res/local/liba");
    mk(cache, "p1", "1.0.0", "{\"name\":\"p1\",\"version\":\"1.0.0\",\"contentType\":\"source\",\"dependencies\":{\"p2\":\"^1.0.0\"}}");
    mk(cache, "p2", "1.0.0", "{\"name\":\"p2\",\"version\":\"1.0.0\",\"contentType\":\"source\"}");
    mk(cache, "p2", "1.1.0", "{\"name\":\"p2\",\"version\":\"1.1.0\",\"contentType\":\"source\"}");
    mk(cache, "p2", "2.0.0", "{\"name\":\"p2\",\"version\":\"2.0.0\",\"contentType\":\"source\"}");
    wr("/tmp/gmb_res/local/liba/gmb-package.json", "{\"name\":\"liba\",\"version\":\"2.5.0\",\"contentType\":\"source\"}");

    char nb[128], vb[64], ib[2048], lb[8192];

    /* transitivo ^1.0.0 → p2 1.1.0 */
    gmb_pkg_resolve_ctx_t* c = gmb_pkg_resolve_ctx_new();
    gmb_pkg_resolve_ctx_set(c, cache, "", "il", "osx-arm64", "Community", 0);
    gmb_pkg_resolve_ctx_add_req(c, "p1", "^1.0.0");
    gmb_pkg_resolve_result_t* r = gmb_pkg_resolve(c);
    CHECK(gmb_pkg_result_error(r) == NULL);
    CHECK(gmb_pkg_result_count(r) == 2);
    CHECK(ver_of(r, "p1", nb, sizeof(nb), vb, sizeof(vb), ib, sizeof(ib), lb, sizeof(lb)) && strcmp(nb, "p1") == 0);
    const char* v2 = ver_of(r, "p2", nb, sizeof(nb), vb, sizeof(vb), ib, sizeof(ib), lb, sizeof(lb));
    CHECK(v2 && strcmp(v2, "1.1.0") == 0);
    gmb_pkg_result_free(r);
    gmb_pkg_resolve_ctx_free(c);

    /* override p2 → 2.0.0 */
    c = gmb_pkg_resolve_ctx_new();
    gmb_pkg_resolve_ctx_set(c, cache, "", "il", "osx-arm64", "Community", 0);
    gmb_pkg_resolve_ctx_add_req(c, "p1", "^1.0.0");
    gmb_pkg_resolve_ctx_add_override(c, "p2", "2.0.0");
    r = gmb_pkg_resolve(c);
    const char* vo = ver_of(r, "p2", nb, sizeof(nb), vb, sizeof(vb), ib, sizeof(ib), lb, sizeof(lb));
    CHECK(vo && strcmp(vo, "2.0.0") == 0);
    gmb_pkg_result_free(r);
    gmb_pkg_resolve_ctx_free(c);

    /* locked p2 = 1.0.0 (vince sul vincolo) */
    c = gmb_pkg_resolve_ctx_new();
    gmb_pkg_resolve_ctx_set(c, cache, "", "il", "osx-arm64", "Community", 0);
    gmb_pkg_resolve_ctx_add_req(c, "p1", "^1.0.0");
    gmb_pkg_resolve_ctx_add_locked(c, "p2", "1.0.0");
    r = gmb_pkg_resolve(c);
    const char* vl = ver_of(r, "p2", nb, sizeof(nb), vb, sizeof(vb), ib, sizeof(ib), lb, sizeof(lb));
    CHECK(vl && strcmp(vl, "1.0.0") == 0);
    gmb_pkg_result_free(r);
    gmb_pkg_resolve_ctx_free(c);

    /* locale: liba dalla dir */
    c = gmb_pkg_resolve_ctx_new();
    gmb_pkg_resolve_ctx_set(c, cache, "", "il", "osx-arm64", "Community", 0);
    gmb_pkg_resolve_ctx_add_req(c, "liba", "^1.0.0");
    gmb_pkg_resolve_ctx_add_local(c, "liba", "/tmp/gmb_res/local/liba");
    r = gmb_pkg_resolve(c);
    CHECK(gmb_pkg_result_count(r) == 1);
    gmb_pkg_result_item(r, 0, nb, sizeof(nb), vb, sizeof(vb), ib, sizeof(ib), lb, sizeof(lb));
    CHECK(strcmp(vb, "2.5.0") == 0);
    CHECK(strcmp(ib, "/tmp/gmb_res/local/liba") == 0);
    gmb_pkg_result_free(r);
    gmb_pkg_resolve_ctx_free(c);

    /* vincolo impossibile → errore */
    c = gmb_pkg_resolve_ctx_new();
    gmb_pkg_resolve_ctx_set(c, cache, "", "il", "osx-arm64", "Community", 0);
    gmb_pkg_resolve_ctx_add_req(c, "p1", "^9.0.0");
    r = gmb_pkg_resolve(c);
    CHECK(gmb_pkg_result_error(r) != NULL);
    gmb_pkg_result_free(r);
    gmb_pkg_resolve_ctx_free(c);

    system("rm -rf /tmp/gmb_res");
    printf("gmb_pkg_resolver_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
