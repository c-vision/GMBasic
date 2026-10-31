/* gmb_semver_selfcheck.c — verifica la semantica SemVer C (parità col resolver C#). */
#include "gmb_semver.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; printf("FAIL %d: %s\n", __LINE__, #cond); } } while (0)

static int sat(const char* ver, const char* range)
{
    gmb_semver_t v;
    if (!gmb_semver_parse(ver, &v)) return -1;
    return gmb_semver_satisfies(&v, range);
}

int main(void)
{
    gmb_semver_t a, b;

    CHECK(gmb_semver_parse("1.2.3", &a) == 1 && a.major == 1 && a.minor == 2 && a.patch == 3 && !a.has_pre);
    CHECK(gmb_semver_parse("v1.2.3-rc.1+b5", &a) == 1 && a.has_pre
          && strcmp(a.prerelease, "rc.1") == 0 && strcmp(a.build, "b5") == 0);
    CHECK(gmb_semver_parse("1.2", &a) == 0);
    CHECK(gmb_semver_parse("1.2.3-", &a) == 0);   /* prerelease vuota: invalida */
    CHECK(gmb_semver_parse("1.2.3-01", &a) == 0); /* leading zero: invalida */
    CHECK(gmb_semver_parse("x.y.z", &a) == 0);

    gmb_semver_parse("1.2.3", &a); gmb_semver_parse("1.2.4", &b);
    CHECK(gmb_semver_cmp(&a, &b) < 0);
    gmb_semver_parse("1.2.3-rc.1", &a); gmb_semver_parse("1.2.3", &b);
    CHECK(gmb_semver_cmp(&a, &b) < 0); /* prerelease < release */

    CHECK(sat("1.5.0", "^1.0.0") == 1);
    CHECK(sat("2.0.0", "^1.0.0") == 0);
    CHECK(sat("1.2.9", "~1.2.3") == 1);
    CHECK(sat("1.3.0", "~1.2.3") == 0);
    CHECK(sat("1.2.3", "1.2.*") == 1);
    CHECK(sat("1.3.0", "1.2.*") == 0);
    CHECK(sat("2.0.0", ">=1.0.0") == 1);
    CHECK(sat("0.9.0", ">=1.0.0") == 0);
    CHECK(sat("1.0.0", "<2.0.0") == 1);
    CHECK(sat("1.2.3", "1.2.3") == 1);
    CHECK(sat("1.2.4", "1.2.3") == 0);
    CHECK(sat("1.5.0", ">=1.0.0 || <0.5.0") == 1);
    CHECK(sat("0.4.0", ">=1.0.0 || <0.5.0") == 1);
    CHECK(sat("0.6.0", ">=1.0.0 || <0.5.0") == 0);
    CHECK(sat("1.0.0", "*") == 1);
    CHECK(sat("1.0.0-rc.1", "^1.0.0") == 0); /* ^ release-only non matcha prerelease */

    /* BUILD METADATA ignorato nel matching (SemVer 2.0 §10, comportamento npm).
     * Prima il resolver confrontava anche il build con gmb_semver_eq: "1.2.3+b5" NON
     * soddisfaceva "1.2.3" e la versione pubblicata 1.2.3+b5 restava invisibile al
     * REQUIRE, mentre il compilatore C (frontend) la sceglieva: due esiti diversi. */
    CHECK(sat("1.2.3+b5", "1.2.3") == 1);
    CHECK(sat("1.2.3", "1.2.3+b5") == 1);
    CHECK(sat("1.2.3+b5", "1.2.3 || 2.0.0") == 1);
    CHECK(sat("1.2.3+b5", "^1.2.3") == 1);
    CHECK(sat("1.2.3-rc.1+b5", "1.2.3-rc.1") == 1);
    CHECK(sat("1.2.3+b5", "1.2.4") == 0);   /* build non "aggiusta" la versione */

    const char* cons[] = { "^1.0.0" };
    const char* vers[] = { "1.0.0", "1.2.0", "2.0.0", "1.1.5" };
    char out[64];
    CHECK(gmb_semver_find_best(1, cons, 4, vers, out, sizeof(out)) == 1 && strcmp(out, "1.2.0") == 0);

    printf("gmb_semver_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
