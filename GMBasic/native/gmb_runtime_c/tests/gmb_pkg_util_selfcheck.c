/* gmb_pkg_util_selfcheck.c — gate di compatibilità + safe_dir + platform_key. */
#include "gmb_pkg_util.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    char err[512], out[64];
    CHECK(gmb_pkg_compat_check("{\"name\":\"a\",\"version\":\"1.0.0\"}", NULL, err, sizeof(err)) == 1);
    CHECK(gmb_pkg_compat_check("{\"name\":\"a\",\"version\":\"1.0.0\",\"settings\":{\"minGmbVersion\":\"999.0.0\"}}", NULL, err, sizeof(err)) == 0);
    /* il vecchio settings.requiresCommercial non e' piu' un gate: il manifest e' compatibile */
    CHECK(gmb_pkg_compat_check("{\"name\":\"a\",\"version\":\"1.0.0\"}", NULL, err, sizeof(err)) == 1);
    CHECK(gmb_pkg_compat_check("{\"name\":\"a\",\"version\":\"1.0.0\"}", NULL, err, sizeof(err)) == 1);
    /* una chiave "edition" residua non blocca piu' nulla */
    CHECK(gmb_pkg_compat_check("{\"name\":\"a\",\"version\":\"1.0.0\",\"edition\":\"Commercial\"}", NULL, err, sizeof(err)) == 1);

    CHECK(gmb_pkg_safe_dir("@marco/geom", out, sizeof(out)) == 1 && strcmp(out, "@marco_geom") == 0);
    CHECK(gmb_pkg_platform_key(out, sizeof(out)) == 1 && strchr(out, '-') != NULL);

    printf("gmb_pkg_util_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
