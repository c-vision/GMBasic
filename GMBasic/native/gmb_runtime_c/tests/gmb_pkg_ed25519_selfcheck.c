/* gmb_pkg_ed25519_selfcheck.c — vettore RFC 8032 (test 1) + roundtrip. */
#include "gmb_pkg_ed25519.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    const char* seed = "nWGxne/9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A=";
    const char* pub = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";
    const char* sig = "5VZDAMNgrHKQhuLMgG6CioSHfx645dl02HPgZSJJAVVfuIIVkKM7rMYeOXAc+bRr0lv18FlbviRlUUFDjnoQCw==";
    char out[128];

    CHECK(gmb_pkg_ed25519_public(seed, out, sizeof(out)) == 1 && strcmp(out, pub) == 0);
    CHECK(gmb_pkg_ed25519_sign(seed, (const unsigned char*)"", 0, out, sizeof(out)) == 1 && strcmp(out, sig) == 0);
    CHECK(gmb_pkg_ed25519_verify(pub, (const unsigned char*)"", 0, sig) == 1);
    CHECK(gmb_pkg_ed25519_verify(pub, (const unsigned char*)"x", 1, sig) == 0); /* tamper */

    char pk[64], pb[64];
    CHECK(gmb_pkg_ed25519_keygen(pk, sizeof(pk), pb, sizeof(pb)) == 1);
    CHECK(gmb_pkg_ed25519_sign(pk, (const unsigned char*)"hi", 2, out, sizeof(out)) == 1);
    CHECK(gmb_pkg_ed25519_verify(pb, (const unsigned char*)"hi", 2, out) == 1);

    printf("gmb_pkg_ed25519_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
