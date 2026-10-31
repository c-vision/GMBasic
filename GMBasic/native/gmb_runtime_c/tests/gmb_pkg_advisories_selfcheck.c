/* gmb_pkg_advisories_selfcheck.c — audit (range SemVer) + SBOM CycloneDX. */
#include "gmb_pkg_advisories.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    const char* names[2] = {"dup", "z"};
    const char* vers[2] = {"1.5.0", "2.0.0"};
    const char* adv =
        "[{\"id\":\"A1\",\"package\":\"dup\",\"range\":\"<1.6.0\",\"severity\":\"high\",\"summary\":\"x\",\"fixed\":\"1.6.0\"},"
        "{\"id\":\"A2\",\"package\":\"z\",\"range\":\"<1.0.0\",\"severity\":\"low\",\"summary\":\"y\"}]";

    gmb_pkg_audit_t* a = gmb_pkg_audit(2, names, vers, adv);
    CHECK(gmb_pkg_audit_count(a) == 1); /* solo 'dup' (z 2.0.0 non <1.0.0) */
    char n[64], v[64], id[32], sev[32], sum[128], fix[32];
    CHECK(gmb_pkg_audit_item(a, 0, n, sizeof(n), v, sizeof(v), id, sizeof(id), sev, sizeof(sev), sum, sizeof(sum), fix, sizeof(fix)) == 1);
    CHECK(strcmp(n, "dup") == 0 && strcmp(id, "A1") == 0 && strcmp(sev, "high") == 0 && strcmp(fix, "1.6.0") == 0);
    gmb_pkg_audit_free(a);

    system("rm -rf /tmp/gmb_adv; mkdir -p /tmp/gmb_adv/p");
    FILE* m = fopen("/tmp/gmb_adv/p/gmb-package.json", "w");
    fputs("{\"name\":\"dup\",\"version\":\"1.5.0\"}", m);
    fclose(m);
    const char* idirs[1] = {"/tmp/gmb_adv/p"};
    char out[2048];
    CHECK(gmb_pkg_sbom(1, names, vers, idirs, out, sizeof(out)) == 1);
    CHECK(strstr(out, "CycloneDX") != NULL && strstr(out, "\"dup\"") != NULL);
    system("rm -rf /tmp/gmb_adv");

    printf("gmb_pkg_advisories_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
