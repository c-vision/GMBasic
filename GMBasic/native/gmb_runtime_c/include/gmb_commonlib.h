// FILE: gmb_commonlib.h — CommonLib C (porting di CommonLib/ per il runtime C99)
//
// Struttura speculare a CommonLib/:
//   - crypto: SHA256, HMAC-SHA512, PBKDF2 (BIP39), Keccak-256, RIPEMD-160, secp256k1
//   - encoding: Base58, Hex (TOHEX), Base32, RLP
//
// Convenzioni: gli array di byte sono (uint8_t* data, size_t len). Le funzioni che
// restituiscono array allocano su heap (malloc) — il chiamante libera con free().

#ifndef GMB_COMMONLIB_H
#define GMB_COMMONLIB_H

#include <stddef.h>
#include <stdint.h>

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------- crypto ------------------------- */

// SHA-256: digests len byte in out (out >= 32).
void gmb_sha256(const uint8_t* data, size_t len, uint8_t out[32]);

// HMAC-SHA256 (RFC 2104): key + msg → 32 byte out.
void gmb_hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]);

// Provenance (Second Brain §2): firma HMAC-SHA256 del contenuto con chiave condivisa.
// sign → hex string (allocata, 65 byte); verify confronta in modo costante (0/1).
// Parità output con C# System.Security.Cryptography.HMACSHA256 (hex lowercase).
char* gmb_provenance_sign(const uint8_t* content, size_t content_len, const char* key, size_t key_len);
int   gmb_provenance_verify(const uint8_t* content, size_t content_len, const char* key, size_t key_len,
                            const char* signature_hex);


// HMAC-SHA512: key + msg → 64 byte out.
void gmb_hmac_sha512(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[64]);

// HMAC-SHA384: key + msg → 48 byte out.
void gmb_hmac_sha384(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[48]);
// HMAC hex (per C99/emitter): return hex string allocata.
char* gmb_hmac_sha256_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len);
char* gmb_hmac_sha384_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len);
char* gmb_hmac_sha512_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len);

// PBKDF2-HMAC-SHA512 (RFC 2898, usata da BIP39): produce dk_len byte in out.
void gmb_pbkdf2_sha512(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len);

// PBKDF2-HMAC-SHA256 (RFC 2898): produce dk_len byte in out.
// KDF canonico di PASSWORDHASH/PASSWORDVERIFY — parità con C# Rfc2898DeriveBytes.
void gmb_pbkdf2_sha256(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len);

// Keccak-256 (Ethereum): digests len byte in out (out >= 32).
void gmb_keccak256(const uint8_t* data, size_t len, uint8_t out[32]);

// RIPEMD-160: digests len byte in out (out >= 20).
void gmb_ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);

// MD5: digests len byte in out (out >= 16).
void gmb_md5(const uint8_t* data, size_t len, uint8_t out[16]);

// SHA-1: digests len byte in out (out >= 20).
void gmb_sha1(const uint8_t* data, size_t len, uint8_t out[20]);

// SHA-512: digests len byte in out (out >= 64).
void gmb_sha512(const uint8_t* data, size_t len, uint8_t out[64]);

// Helper transpiler C99: digest mallocato + lunghezza (MD5/SHA1/SHA256/SHA512).
uint8_t* gmb_md5_alloc(const uint8_t* data, size_t len, size_t* len_out);
uint8_t* gmb_sha1_alloc(const uint8_t* data, size_t len, size_t* len_out);
uint8_t* gmb_sha256_alloc(const uint8_t* data, size_t len, size_t* len_out);
uint8_t* gmb_sha512_alloc(const uint8_t* data, size_t len, size_t* len_out);
// Digest hex in stringa allocata (STRMD5/STRSHA*/TOHEX(MD5(...))).
char* gmb_md5_hex(const uint8_t* data, size_t len);
gmb_string_t gmb_digest_raw(gmb_int_t algo, const uint8_t* data, size_t len);
// Costruisce un gmb_string_t BINARIO da un buffer mallocato, prendendone ownership
// (usato dal C99Native per RLPENCODE/VARINTENCODE: uint8_t* + out-len → byte[]).
gmb_string_t gmb_string_take_bytes(uint8_t* data, size_t len);
char* gmb_sha1_hex(const uint8_t* data, size_t len);
char* gmb_sha256_hex(const uint8_t* data, size_t len);
char* gmb_keccak256_hex(const uint8_t* data, size_t len);
char* gmb_ripemd160_hex(const uint8_t* data, size_t len);
char* gmb_blake2b256_hex(const uint8_t* data, size_t len);
char* gmb_sha512_hex(const uint8_t* data, size_t len);

/* ------------------------- encoding ------------------------- */

// Hex encode: scrive 2*len char in out (il chiamante alloca). out non NUL-terminato
// se non si aggiunge spazio; gmb_hex_encode_string aggiunge il terminatore.
void gmb_hex_encode(const uint8_t* data, size_t len, char* out);
char* gmb_hex_encode_string(const uint8_t* data, size_t len);

// HTML encode (OWASP set: & < > " ') → stringa allocata (parità .NET WebUtility.HtmlEncode).
char* gmb_html_encode(const char* input);

// URL percent-encode RFC 3986 (unreserved conservati, %HH maiuscolo) → stringa allocata
// (parità .NET Uri.EscapeDataString).
char* gmb_url_encode(const char* input);

// JS string-encode (literal a doppi apici): escape \\ " ' e controlli C0 + U+2028/2029.
char* gmb_js_encode(const char* input);

// JWT HS256 (Security P2, parità CommonLib.Jwt): create/verify/payload.
char* gmb_jwt_create(const char* claims, const char* secret);
int gmb_jwt_verify(const char* token, const char* secret);
char* gmb_jwt_payload(const char* token);

// Upload security (parità CommonLib.Http.FileUpload): nome file sicuro + magic-number.
char* gmb_upload_safe_name(const char* input);
const char* gmb_upload_detect_type(const uint8_t* data, size_t len);

// Base58 encode (alfabeto Bitcoin): restituisce stringa allocata.
char* gmb_base58_encode(const uint8_t* data, size_t len);

// Base58 decode: restituisce array allocato, len_out = numero di byte.
uint8_t* gmb_base58_decode(const char* input, size_t* len_out);

// Base64 encode: restituisce stringa allocata.
char* gmb_base64_encode(const uint8_t* data, size_t len);

// Base64 decode: restituisce array allocato, len_out = numero di byte.
uint8_t* gmb_base64_decode(const char* input, size_t* len_out);

// Base64URL (RFC 4648 §5, senza padding): encode → stringa allocata; decode → array allocato.
// Parità con C# CoreToBase64Url/CoreFromBase64Url.
char* gmb_base64url_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base64url_decode(const char* input, size_t* len_out);

// Hex decode: restituisce array allocato, len_out = numero di byte (NULL se input non valido).
uint8_t* gmb_hex_decode(const char* input, size_t* len_out);

// ChaCha20 (RFC 8439, nonce 12-byte): xor in-place su `out` (può coincidere con `in`).
void gmb_chacha20_xor(const uint8_t* key, const uint8_t* nonce, const uint8_t* in, size_t inlen, uint8_t* out);

// Poly1305 (RFC 8439): MAC di 16 byte su `msg` con chiave di 32 byte.
void gmb_poly1305_mac(const uint8_t* key, const uint8_t* msg, size_t msglen, uint8_t out[16]);

// ChaCha20 block (contatore esplicito) e xor a partire da un contatore (per AEAD).
void gmb_chacha20_block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]);
void gmb_chacha20_xor_counter(const uint8_t key[32], const uint8_t nonce[12], uint32_t start_counter,
                              const uint8_t* in, size_t inlen, uint8_t* out);

// AES-GCM (RFC 5288): key 32B, nonce 12B, aad opzionale. tag 16B.
int gmb_aes_gcm_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                        const uint8_t* aad, size_t aadlen,
                        const uint8_t* pt, size_t ptlen, uint8_t* ct, uint8_t tag[16]);
int gmb_aes_gcm_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                        const uint8_t* aad, size_t aadlen,
                        const uint8_t* ct, size_t ctlen, const uint8_t tag[16], uint8_t* pt);

// AES-GCM con password (formato C#: salt16|nonce12|tag16|ct). Ritorna allocato.
uint8_t* gmb_aes_encrypt_password(const uint8_t* data, size_t len, const char* password, size_t* out_len);
uint8_t* gmb_aes_decrypt_password(const uint8_t* enc, size_t len, const char* password, size_t* out_len);
/* Wrapper val-returning (byte[] come gmb_string_t) per i backend C. */
gmb_string_t gmb_aes_encrypt_val(gmb_string_t data, gmb_string_t key);
gmb_string_t gmb_aes_decrypt_val(gmb_string_t data, gmb_string_t key);
gmb_string_t gmb_chacha20_xor_val(gmb_string_t data, gmb_string_t key, gmb_string_t nonce);
/* Variante uint8_t* + out-len (emitter C# DIM AS BYTE). */
uint8_t* gmb_chacha20_xor_bytes(const uint8_t* data, size_t dlen,
                                const uint8_t* key, size_t klen,
                                const uint8_t* nonce, size_t nlen, size_t* out_len);
/* ChaCha20-Poly1305 val (dict con ciphertext/tag) + decrypt (byte[]). */
gmb_val_t gmb_chacha20poly1305_encrypt_sval(gmb_string_t pt, gmb_string_t key, gmb_string_t nonce, gmb_string_t aad);
gmb_string_t gmb_chacha20poly1305_decrypt_sval(gmb_val_t ct, gmb_val_t tag, gmb_string_t key, gmb_string_t nonce, gmb_string_t aad);
/* Varianti uint8_t* + len (emitter C#). */
gmb_val_t gmb_chacha20poly1305_encrypt_bytes(const uint8_t* pt, size_t plen,
    const uint8_t* key, size_t klen, const uint8_t* nonce, size_t nlen,
    const uint8_t* aad, size_t alen);
gmb_string_t gmb_chacha20poly1305_decrypt_bytes(gmb_val_t ct, gmb_val_t tag,
    const uint8_t* key, size_t klen, const uint8_t* nonce, size_t nlen,
    const uint8_t* aad, size_t alen);

// ChaCha20-Poly1305 AEAD (RFC 8439).
int gmb_chacha20poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                 const uint8_t* aad, size_t aadlen,
                                 const uint8_t* pt, size_t ptlen, uint8_t* ct, uint8_t tag[16]);
int gmb_chacha20poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                 const uint8_t* aad, size_t aadlen,
                                 const uint8_t* ct, size_t ctlen, const uint8_t tag[16], uint8_t* pt);
int gmb_poly1305_tag(const uint8_t key[32], const uint8_t* aad, size_t aadlen,
                     const uint8_t* ct, size_t ctlen, uint8_t out[16]);

/* --- Registro di chiavi asimmetriche (ECDSA secp256k1 / RSA) — backend OpenSSL --- */
int gmb_keys_generate(const char* algo, int bits, int* pvt_out, int* pub_out);
void gmb_keys_close(int handle);
int gmb_keys_is_private(int handle);
uint8_t* gmb_keys_sign(int handle, const uint8_t* data, size_t len, size_t* len_out);
int gmb_keys_verify(int handle, const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen);
int gmb_keys_verify_raw(const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen, const char* pub_b64);
uint8_t* gmb_keys_encrypt(int handle, const uint8_t* data, size_t len, size_t* len_out);
uint8_t* gmb_keys_decrypt(int handle, const uint8_t* data, size_t len, size_t* len_out);
char* gmb_keys_export_pem(int handle);
int gmb_keys_import_pem(const char* pem);
/* Wrapper val per i backend C. */
gmb_val_t gmb_keys_generate_val(const char* algo, gmb_int_t bits);
gmb_string_t gmb_keys_sign_val(gmb_int_t handle, const uint8_t* data, size_t len);
gmb_string_t gmb_keys_encrypt_val(gmb_int_t handle, const uint8_t* data, size_t len);
gmb_string_t gmb_keys_decrypt_val(gmb_int_t handle, const uint8_t* data, size_t len);
gmb_string_t gmb_keys_export_val(gmb_int_t handle);
int gmb_keys_verify_val(gmb_int_t handle, const uint8_t* data, size_t len, gmb_string_t sig);
int gmb_keys_verify_raw_val(const uint8_t* data, size_t len, gmb_string_t sig, const char* pub_b64);
int gmb_keys_verify_bytes(gmb_int_t handle, const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen);
int gmb_keys_verify_raw_bytes(const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen, const char* pub_b64);

/* ------------------------- blockchain (alto livello) ------------------------- */

// BIP39: genera un mnemonic di n_words parole (dalla wordlist BIP39 italiana).
// Restituisce stringa allocata "parola1 parola2 ...".
char* gmb_bip39_generate_mnemonic(unsigned n_words);

// BIP39: mnemonic + passphrase → seed 64 byte (PBKDF2-HMAC-SHA512, 2048 iter).
void gmb_bip39_mnemonic_to_seed(const char* mnemonic, const char* passphrase, uint8_t seed[64]);

// Variante che alloca il seed su heap e restituisce la lunghezza (per DIM x() = MNEMONICTOSEED).
uint8_t* gmb_bip39_mnemonic_to_seed_alloc(const char* mnemonic, const char* passphrase, size_t* len_out);

// BIP32: derivazione di una chiave privata (32 byte) dal seed (BIP32 master).
// La chiave pubblica secp256k1 associata è in pub_out (33 byte, compressed).
void gmb_bip32_master_from_seed(const uint8_t seed[64], uint8_t priv[32], uint8_t pub[33]);

// secp256k1: calcola la chiave pubblica (33 byte compressed) da una privata (32 byte).
void gmb_secp256k1_pubkey(const uint8_t priv[32], uint8_t pub[33]);

// Ethereum: indirizzo da chiave pubblica (Keccak256 degli ultimi 64 byte) → 20 byte.
void gmb_eth_address_from_pub(const uint8_t pub[33], uint8_t addr[20]);

// BIP32 DERIVEKEY: popola il dict con {privateKey, publicKey, chainCode} come array di byte
// boxed (gmb_make_val_bytes) — per `DIM d AS DICT = BLOCKCHAIN.DERIVEKEY(seed, path)`.
void gmb_bip32_derive_key_dict(const uint8_t* seed, size_t seed_len, const char* path, gmb_dict_t* out);

// ETHADDRFROMPUB(pub, len): indirizzo Ethereum come stringa hex "0x..." (allocata).
char* gmb_eth_address_from_pub_hex(const uint8_t* pub, size_t len);

// GETPUBLICKEY(priv, len): chiave pubblica (33 byte compressed) allocata.
uint8_t* gmb_get_public_key(const uint8_t* priv, size_t len, size_t* len_out);

#ifdef __cplusplus
}
#endif

#endif // GMB_COMMONLIB_H

/* --- Encoding & blockchain (gmb_encoding.c) --- */
char* gmb_base32_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base32_decode(const char* input, size_t* len_out);
void gmb_blake2b256(const uint8_t* data, size_t len, uint8_t out[32]);
uint8_t* gmb_varint_encode(int64_t value, size_t* len_out);
int64_t gmb_varint_decode(const uint8_t* data, size_t len, size_t* consumed);
uint8_t* gmb_rlp_encode(const uint8_t* data, size_t len, size_t* len_out);
double gmb_block_difficulty(const uint8_t* header, size_t len, int32_t bits);
char* gmb_password_hash(const char* password);
int gmb_password_verify(const char* password, const char* stored);
char* gmb_abi_encode(const char* signature, const double* args, int argc, const char** strargs, const int* strargs_idx);
char* gmb_abi_encode_vals(const char* signature, const gmb_val_t* args, int argc);
/* CHACHA20POLY1305ENCRYPT → dict {ciphertext, tag} (val boxed, parità VM case 354). */
gmb_val_t gmb_chacha20poly1305_encrypt_val(const uint8_t* key, size_t klen, const uint8_t* nonce, size_t nlen,
                                           const uint8_t* pt, size_t plen, const uint8_t* aad, size_t alen);
/* CONTRACT(addr, iface, rpc) → dict {address, interface, rpcUrl} (parità VM case 392). */
gmb_val_t gmb_contract_proxy_val(const char* addr, const char* iface, const char* rpc);
/* DERIVEKEY(seed, path) → dict {privateKey, publicKey, chainCode} boxed. */
gmb_val_t gmb_bip32_derive_key_dict_val(const uint8_t* seed, size_t len, const char* path);

char* gmb_eth_address_checksum(const char* address);
char* gmb_btc_address_from_pub(const uint8_t* pub, size_t len, int testnet);
char** gmb_abi_decode_words(const char* hex_in, int* count_out);
/* ABIDECODE numerico (parità IlRuntime.AbiDecode): decodifica parole da 32 byte big-endian
 * a partire dall'offset 0 (niente skip del selettore) → double*, count_out = n parole. */
double* gmb_abi_decode_doubles(const char* hex_in, int* count_out);

uint8_t* gmb_ecdsa_sign_raw(const uint8_t* msg, size_t mlen, const uint8_t* priv, size_t privlen, size_t* out_len);
