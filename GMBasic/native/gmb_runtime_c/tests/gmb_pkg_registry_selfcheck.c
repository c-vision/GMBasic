/* gmb_pkg_registry_selfcheck.c — estrazione versioni dal dettaglio del registry (offline). */
#include "gmb_pkg_registry.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    char v[64];
    /* forma `versions` come oggetto (chiavi = versioni) */
    gmb_pkg_registry_detail_t* d = gmb_pkg_registry_detail_from_json(
        "{\"versions\":{\"1.0.0\":{\"version\":\"1.0.0\"},\"1.2.0\":{\"version\":\"1.2.0\"}}}");
    CHECK(d && gmb_pkg_registry_detail_count(d) == 2);
    int has1 = 0, has2 = 0;
    for (int i = 0; i < gmb_pkg_registry_detail_count(d); i++) {
        gmb_pkg_registry_detail_version(d, i, v, sizeof(v));
        if (strcmp(v, "1.0.0") == 0) has1 = 1;
        if (strcmp(v, "1.2.0") == 0) has2 = 1;
    }
    CHECK(has1 && has2);
    gmb_pkg_registry_detail_free(d);

    /* forma `versionList` come array */
    d = gmb_pkg_registry_detail_from_json("{\"versionList\":[\"1.0.0\",\"2.0.0\"]}");
    CHECK(d && gmb_pkg_registry_detail_count(d) == 2);
    gmb_pkg_registry_detail_free(d);

    /* non-oggetto → NULL */
    CHECK(gmb_pkg_registry_detail_from_json("[]") == NULL);

    printf("gmb_pkg_registry_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
