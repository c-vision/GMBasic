/* gmb_pkg_trust_selfcheck.c — keyring + HMAC. */
#include "gmb_pkg_trust.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    const char* fox = "The quick brown fox jumps over the lazy dog";
    const char* expect = "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"; /* HMAC-SHA256("key", fox) */
    char out[256], fp[64];

    CHECK(gmb_pkg_hmac_hex("key", (const unsigned char*)fox, strlen(fox), out, sizeof(out)) == 1);
    CHECK(strcmp(out, expect) == 0);
    CHECK(gmb_pkg_key_fingerprint("some-key", fp, sizeof(fp)) == 1 && strlen(fp) == 16);

    const char* krp = "/tmp/gmb_trust_keyring.json";
    remove(krp);
    CHECK(gmb_pkg_keyring_set(krp, "a", "k1") == 1);
    CHECK(gmb_pkg_keyring_set(krp, "b", "k2") == 1);
    CHECK(gmb_pkg_keyring_count(krp) == 2);
    CHECK(gmb_pkg_keyring_get(krp, "a", out, sizeof(out)) == 1 && strcmp(out, "k1") == 0);
    CHECK(gmb_pkg_keyring_get(krp, "zz", out, sizeof(out)) == 0);
    CHECK(gmb_pkg_keyring_set(krp, "a", "k1b") == 1); /* update: non duplica */
    CHECK(gmb_pkg_keyring_count(krp) == 2);
    CHECK(gmb_pkg_keyring_get(krp, "a", out, sizeof(out)) == 1 && strcmp(out, "k1b") == 0);
    CHECK(gmb_pkg_keyring_remove(krp, "a") == 1);
    CHECK(gmb_pkg_keyring_get(krp, "a", out, sizeof(out)) == 0);
    CHECK(gmb_pkg_keyring_count(krp) == 1);

    const char* ff = "/tmp/gmb_trust_fox.txt";
    FILE* f = fopen(ff, "w");
    fputs(fox, f);
    fclose(f);
    CHECK(gmb_pkg_hmac_file_hex("key", ff, out, sizeof(out)) == 1 && strcmp(out, expect) == 0);

    remove(krp);
    remove(ff);
    printf("gmb_pkg_trust_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
