/* ============================================================================
 * gmb_pkg_ed25519.h — firme Ed25519 (riferimento C del Package Manager, via OpenSSL).
 * Chiavi: private = seed 32B base64, public = 32B base64 (o PEM ECDSA per il fallback).
 * Il C# lo consuma via P/Invoke.
 * ============================================================================ */
#ifndef GMB_PKG_ED25519_H
#define GMB_PKG_ED25519_H

#include "gmb_runtime.h"

/* Firma i dati (seed Ed25519 base64 32B) → firma base64. 1/0. */
int gmb_pkg_ed25519_sign(const char* private_b64, const unsigned char* data, size_t len, char* sig_b64, size_t cap);

/* Deriva la chiave pubblica (base64 32B) dalla privata (seed base64 32B). 1/0. */
int gmb_pkg_ed25519_public(const char* private_b64, char* pub_b64, size_t cap);

/* Genera una coppia di chiavi Ed25519: seed (priv) 32B base64 + pubblica 32B base64. 1/0. */
int gmb_pkg_ed25519_keygen(char* priv_b64, size_t pcap, char* pub_b64, size_t ucap);

/* Verifica una firma base64 con la pubblica (base64 32B Ed25519 o PEM ECDSA). 1/0. */
int gmb_pkg_ed25519_verify(const char* public_key, const unsigned char* data, size_t len, const char* sig_b64);

#endif /* GMB_PKG_ED25519_H */
