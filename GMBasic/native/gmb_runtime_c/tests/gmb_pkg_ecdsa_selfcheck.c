/* gmb_pkg_ecdsa_selfcheck.c — ECDSA P-256 PEM: keygen + sign + verify (via Ed25519-verify PEM). */
#include "gmb_pkg_ecdsa.h"
#include "gmb_pkg_ed25519.h"
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

int main(void)
{
    char priv[4096], pub[4096], pub2[4096], sig[256];
    CHECK(gmb_pkg_ecdsa_keygen_pem(priv, sizeof(priv), pub, sizeof(pub)) == 1);
    CHECK(strstr(priv, "BEGIN") != NULL && strstr(pub, "BEGIN") != NULL);
    CHECK(gmb_pkg_ecdsa_public_pem(priv, pub2, sizeof(pub2)) == 1 && strcmp(pub2, pub) == 0);

    CHECK(gmb_pkg_ecdsa_sign_pem(priv, (const unsigned char*)"hello", 5, sig, sizeof(sig)) == 1);
    CHECK(gmb_pkg_ed25519_verify(pub, (const unsigned char*)"hello", 5, sig) == 1); /* percorso PEM ECDSA */
    CHECK(gmb_pkg_ed25519_verify(pub, (const unsigned char*)"x", 1, sig) == 0);     /* tamper */

    printf("gmb_pkg_ecdsa_selfcheck: %d check, %d fail\n", checks, fails);
    return fails ? 1 : 0;
}
