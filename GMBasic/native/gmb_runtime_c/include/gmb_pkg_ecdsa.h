/* ============================================================================
 * gmb_pkg_ecdsa.h — firme ECDSA P-256 (riferimento C del Package Manager, OpenSSL).
 * Percorso LEGACY (chiavi PEM). Firma base64 (sig DER, come il C# Rfc3279DerSequence).
 * ============================================================================ */
#ifndef GMB_PKG_ECDSA_H
#define GMB_PKG_ECDSA_H

#include "gmb_runtime.h"

/* Firma i dati (privata PEM ECDSA P-256) → firma base64 (DER). 1/0. */
int gmb_pkg_ecdsa_sign_pem(const char* priv_pem, const unsigned char* data, size_t len, char* sig_b64, size_t cap);

/* Esporta la pubblica (PEM SubjectPublicKeyInfo) dalla privata PEM. 1/0. */
int gmb_pkg_ecdsa_public_pem(const char* priv_pem, char* pub_pem, size_t cap);

/* Genera una coppia ECDSA P-256 (PEM priv/pubb) — per test/tooling. 1/0. */
int gmb_pkg_ecdsa_keygen_pem(char* priv_pem, size_t pcap, char* pub_pem, size_t ucap);

#endif /* GMB_PKG_ECDSA_H */
