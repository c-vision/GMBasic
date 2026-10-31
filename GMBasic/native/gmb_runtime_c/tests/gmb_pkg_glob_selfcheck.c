/* gmb_pkg_glob_selfcheck.c — match glob (parità con PackageBuilder.GlobMatch). */
#include "gmb_pkg_glob.h"
#include <stdio.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    CHECK(gmb_pkg_glob_match("secret/", "secret/k.key") == 1);
    CHECK(gmb_pkg_glob_match("*.key", "dir/k.key") == 1);
    CHECK(gmb_pkg_glob_match("src/**", "src/a/b.bas") == 1);
    CHECK(gmb_pkg_glob_match("src/**", "gmb-package.json") == 0);
    CHECK(gmb_pkg_glob_match("src/", "src/a.bas") == 1);
    CHECK(gmb_pkg_glob_match("*.bas", "a.bas") == 1);
    CHECK(gmb_pkg_glob_match("*.bas", "dir/a.bas") == 1); /* basename */
    CHECK(gmb_pkg_glob_match("foo", "foo") == 1);
    CHECK(gmb_pkg_glob_match("foo", "bar") == 0);
    CHECK(gmb_pkg_glob_match("a/*.bas", "a/b.bas") == 1);
    CHECK(gmb_pkg_glob_match("a/*.bas", "a/x/b.bas") == 0); /* '*' non attraversa '/' */
    CHECK(gmb_pkg_glob_match("Secret/", "secret/K.KEY") == 1); /* case-insensitive */

    printf("gmb_pkg_glob_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
