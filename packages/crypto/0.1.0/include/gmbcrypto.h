/* gmbcrypto.h — API pubblica di libgmbcrypto (dominio CRYPTOGRAPHY via FFI).
 *
 * Dipendenza esterna: OpenSSL 3 (libcrypto), per hashing, HMAC, KDF, AEAD,
 * RSA/EC e random. Il dominio e' quindi un PACCHETTO (packages/crypto), non
 * una lib self-contained di shared/libs.
 *
 * Convenzioni:
 *  - le funzioni che producono stringhe ritornano `char*` malloc'd: il runtime
 *    FFI le copia e le libera (stessa convenzione di gmbblockchain);
 *  - ogni funzione che scrive in un buffer del chiamante riceve `cap` e
 *    ritorna i byte scritti, oppure -1 (errore) / -2 (buffer troppo piccolo).
 */
#ifndef GMB_CRYPTO_H
#define GMB_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hashing (algo: "sha1","sha256","sha384","sha512","sha3-256","sha3-512","md5","ripemd160") ── */
int ofx_crypto_digest(const char* algo, const uint8_t* data, size_t len,
                      uint8_t* out, size_t cap);
char* ofx_crypto_digest_hex(const char* algo, const uint8_t* data, size_t len);
char* ofx_crypto_digest_hex_str(const char* algo, const char* text);
int ofx_crypto_digest_str(const char* algo, const char* text, uint8_t* out, size_t cap);
char* ofx_crypto_file_hex(const char* path, const char* algo);

/* ── HMAC ─────────────────────────────────────────────────────────────────── */
int ofx_crypto_hmac(const char* algo, const uint8_t* key, size_t key_len,
                    const uint8_t* data, size_t len, uint8_t* out, size_t cap);
char* ofx_crypto_hmac_hex(const char* algo, const char* key, const char* text);

/* ── KDF ──────────────────────────────────────────────────────────────────── */
int ofx_crypto_pbkdf2(const char* algo, const uint8_t* pw, size_t pw_len,
                      const uint8_t* salt, size_t salt_len, int iters,
                      uint8_t* out, size_t out_len);
int ofx_crypto_hkdf(const char* algo, const uint8_t* ikm, size_t ikm_len,
                    const uint8_t* salt, size_t salt_len,
                    const uint8_t* info, size_t info_len,
                    uint8_t* out, size_t out_len);

/* ── password (formato: Base64(salt).Base64(hash), PBKDF2-SHA256) ─────────── */
char* ofx_crypto_password_hash(const char* password);
int ofx_crypto_password_verify(const char* password, const char* stored);

/* ── AEAD (algo: "aes256gcm","aes128gcm","chacha20poly1305") ───────────────── */
/* out = ciphertext || tag(16). Ritorna la lunghezza totale, -2 se cap e' corto. */
int ofx_crypto_aead_encrypt(const char* algo,
                            const uint8_t* key, size_t key_len,
                            const uint8_t* iv, size_t iv_len,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* pt, size_t pt_len,
                            uint8_t* out, size_t cap);
/* pt_len = lunghezza del ciphertext (tag compreso). */
int ofx_crypto_aead_decrypt(const char* algo,
                            const uint8_t* key, size_t key_len,
                            const uint8_t* iv, size_t iv_len,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* ct, size_t ct_len,
                            uint8_t* out, size_t cap);

/* ── checksum ─────────────────────────────────────────────────────────────── */
uint32_t ofx_crypto_crc32(const uint8_t* data, size_t len);
uint32_t ofx_crypto_crc32c(const uint8_t* data, size_t len);
uint32_t ofx_crypto_adler32(const uint8_t* data, size_t len);
uint32_t ofx_crypto_fletcher32(const uint8_t* data, size_t len);
/* variant: 0=ARC/IBM  1=CCITT-FALSE  2=XMODEM  3=KERMIT */
uint32_t ofx_crypto_crc16(const uint8_t* data, size_t len, int variant);
/* variant: 0=ECMA-182  1=ISO/XZ (stesso polinomio riflesso, init diverso) */
uint64_t ofx_crypto_crc64(const uint8_t* data, size_t len, int variant);
/* forme testuali per l'export BASIC (LONG e' a 64 bit, ma l'hex e' piu' leggibile) */
char* ofx_crypto_crc32_hex_str(const char* text);
char* ofx_crypto_crc64_hex(const uint8_t* data, size_t len, int variant);

/* ── encoding ─────────────────────────────────────────────────────────────── */
char* ofx_crypto_hex_encode(const uint8_t* data, size_t len);
char* ofx_crypto_hex_encode_str(const char* text);
char* ofx_crypto_base64_encode_str(const char* text);
char* ofx_crypto_base64url_encode_str(const char* text);
int ofx_crypto_hex_decode(const char* text, uint8_t* out, size_t cap);
char* ofx_crypto_base64_encode(const uint8_t* data, size_t len);
int ofx_crypto_base64_decode(const char* text, uint8_t* out, size_t cap);
char* ofx_crypto_base64url_encode(const uint8_t* data, size_t len);
int ofx_crypto_base64url_decode(const char* text, uint8_t* out, size_t cap);

/* ── random / utility ─────────────────────────────────────────────────────── */
int ofx_crypto_random_bytes(uint8_t* out, size_t len);
int ofx_crypto_timing_safe_equal(const uint8_t* a, const uint8_t* b, size_t len);
int64_t ofx_crypto_random_int(int64_t lo, int64_t hi);
double ofx_crypto_random_double(double lo, double hi);
double ofx_crypto_random_normal(double mean, double sd);

/* ── AES-GCM con password (salt16|nonce12|tag16|ct) e ChaCha20 grezzo ─────── */
int ofx_crypto_aes_password_encrypt(const uint8_t* data, size_t len, const char* password,
                                    uint8_t* out, size_t cap);
int ofx_crypto_aes_password_decrypt(const uint8_t* enc, size_t len, const char* password,
                                    uint8_t* out, size_t cap);
int ofx_crypto_chacha20_xor(const uint8_t* data, size_t len,
                            const uint8_t* key, size_t klen,
                            const uint8_t* nonce, size_t nlen,
                            uint8_t* out, size_t cap);

/* ── coppie di chiavi (handle interi, registro interno alla lib) ───────────── */
int ofx_crypto_keys_generate(const char* algo, int bits, int* pvt_out, int* pub_out);
void ofx_crypto_keys_close(int handle);
int ofx_crypto_keys_is_private(int handle);
/* PEM allocato (privata o pubblica a seconda dell'handle) */
char* ofx_crypto_keys_export_pem(int handle);
int ofx_crypto_keys_import_pem(const char* pem);
/* firma SHA-256 (buffer allocato) e verifica */
uint8_t* ofx_crypto_keys_sign(int handle, const uint8_t* data, size_t len, size_t* len_out);
int ofx_crypto_keys_verify(int handle, const uint8_t* data, size_t len,
                           const uint8_t* sig, size_t sig_len);
/* cifratura asimmetrica RSA-OAEP-SHA256 (buffer allocato) */
uint8_t* ofx_crypto_keys_encrypt(int handle, const uint8_t* data, size_t len, size_t* len_out);
uint8_t* ofx_crypto_keys_decrypt(int handle, const uint8_t* data, size_t len, size_t* len_out);
/* verifica Ed25519 (chiave pubblica Base64 32B) o PEM */
int ofx_crypto_keys_verify_raw(const uint8_t* data, size_t len,
                               const uint8_t* sig, size_t sig_len, const char* pub_b64);

/* ── adattatori per la superficie BASIC (handle a 64 bit, buffer) ─────────── */
int ofx_crypto_keys_generate_pair(const char* algo, int64_t bits, int64_t* out);
int64_t ofx_crypto_keys_import64(const char* pem);
int ofx_crypto_keys_close64(int64_t handle);
int ofx_crypto_keys_sign_buf(int64_t handle, const char* data, size_t len,
                             uint8_t* out, size_t cap);
int ofx_crypto_keys_verify_buf(int64_t handle, const char* data, size_t len,
                               const uint8_t* sig, size_t siglen);
int ofx_crypto_keys_encrypt_buf(int64_t handle, const char* data, size_t len,
                                uint8_t* out, size_t cap);
int ofx_crypto_keys_decrypt_buf(int64_t handle, const uint8_t* enc, size_t len,
                                uint8_t* out, size_t cap);
int ofx_crypto_chacha20_xor_text(const char* text, size_t len,
                                 const uint8_t* key, size_t klen,
                                 const uint8_t* nonce, size_t nlen,
                                 uint8_t* out, size_t cap);
int ofx_crypto_file_digest(const char* path, const char* algo, uint8_t* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_CRYPTO_H */
