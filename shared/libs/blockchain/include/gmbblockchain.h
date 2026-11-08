/* gmbblockchain.h — lib C99 di dominio: primitivi hash/encoding + blockchain
 * (BIP39/BIP32, indirizzi ETH/BTC, ABI EVM, difficolta' di blocco, secp256k1).
 *
 * AUTONOMA: non include header di GMBasic ne' di altre lib (gmbsuite fondazione).
 * Unica dipendenza esterna: OpenSSL (libcrypto) per secp256k1 e i BIGNUM della
 * difficolta' — per questo il dominio e' distribuito anche come PACCHETTO.
 *
 * ABI FFI: gli array di BYTE sono ammessi come elementi-array (2026-11-06): il bridge
 * passa ogni elemento a 8 byte nello scratch (valori 0..255) e la lib lo riduce a
 * uint8_t. Le stringhe di ritorno sono `char*` malloc'd liberate dal runtime.
 */
#ifndef GMBLOCKCHAIN_H
#define GMBLOCKCHAIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- SHA-256 incrementale (stesso layout del runtime GMBasic) ------------ */
typedef struct gmb_sha256_stream {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t  block[64];
    size_t   block_len;
} gmb_sha256_stream;

void gmb_sha256(const uint8_t* data, size_t len, uint8_t out[32]);
void gmb_sha256_stream_init(gmb_sha256_stream* s);
void gmb_sha256_stream_update(gmb_sha256_stream* s, const void* data, size_t len);
void gmb_sha256_stream_final(gmb_sha256_stream* s, uint8_t out[32]);

/* ---- hashing ------------------------------------------------------------- */
void gmb_sha512(const uint8_t* data, size_t len, uint8_t out[64]);
void gmb_hmac_sha256(const uint8_t* key, size_t key_len,
                     const uint8_t* msg, size_t msg_len, uint8_t out[32]);
void gmb_hmac_sha512(const uint8_t* key, size_t key_len,
                     const uint8_t* msg, size_t msg_len, uint8_t out[64]);
void gmb_pbkdf2_sha512(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len);
void gmb_pbkdf2_sha256(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len);
void gmb_keccak256(const uint8_t* data, size_t len, uint8_t out[32]);
void gmb_ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);
void gmb_blake2b256(const uint8_t* data, size_t len, uint8_t out[32]);

char* gmb_keccak256_hex(const uint8_t* data, size_t len);
char* gmb_ripemd160_hex(const uint8_t* data, size_t len);
char* gmb_blake2b256_hex(const uint8_t* data, size_t len);
char* gmb_sha512_hex(const uint8_t* data, size_t len);

/* ---- encoding (risultati malloc'd: il chiamante libera) ------------------ */
void gmb_hex_encode(const uint8_t* data, size_t len, char* out);
char* gmb_hex_encode_string(const uint8_t* data, size_t len);
uint8_t* gmb_hex_decode(const char* input, size_t* len_out);
char* gmb_base58_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base58_decode(const char* input, size_t* len_out);
char* gmb_base32_encode(const uint8_t* data, size_t len);
uint8_t* gmb_base32_decode(const char* input, size_t* len_out);
uint8_t* gmb_varint_encode(int64_t value, size_t* len_out);
int64_t gmb_varint_decode(const uint8_t* data, size_t len, size_t* consumed);
uint8_t* gmb_rlp_encode(const uint8_t* data, size_t len, size_t* len_out);

/* ---- BIP39 / BIP32 / secp256k1 ------------------------------------------ */
char* gmb_bip39_generate_mnemonic(unsigned n_words);
void gmb_bip39_mnemonic_to_seed(const char* mnemonic, const char* passphrase, uint8_t seed[64]);
void gmb_secp256k1_pubkey(const uint8_t priv[32], uint8_t pub[33]);
/* 1 = scritta la chiave NON compressa (65 byte: 0x04 || X || Y), 0 = non calcolabile
 * (build senza OpenSSL). E' la forma richiesta dagli indirizzi Ethereum. */
int gmb_secp256k1_pubkey_uncompressed(const uint8_t priv[32], uint8_t pub[65]);
void gmb_bip32_master_from_seed(const uint8_t seed[64], uint8_t priv[32], uint8_t pub[33]);
uint8_t* gmb_get_public_key(const uint8_t* priv, size_t len, size_t* len_out);

/* Firma ECDSA (DER) di un hash SHA-256 gia' calcolato: lunghezza della firma,
 * 0 se non firmabile (es. build senza OpenSSL). */
size_t gmb_secp256k1_sign(const uint8_t hash[32], const uint8_t priv[32],
                          uint8_t* der, size_t cap);

/* ---- indirizzi ----------------------------------------------------------- */
void gmb_eth_address_from_pub(const uint8_t pub[33], uint8_t addr[20]);
char* gmb_eth_address_from_pub_hex(const uint8_t* pub, size_t len);
char* gmb_eth_address_checksum(const char* address);
char* gmb_btc_address_from_pub(const uint8_t* pub, size_t len, int testnet);

/* ---- difficolta' di blocco / ABI EVM ------------------------------------- */
double gmb_block_difficulty(const uint8_t* header, size_t len, int32_t bits);
double* gmb_abi_decode_doubles(const char* hex_in, int* count_out);
char* gmb_abi_encode(const char* signature, const double* args, int argc,
                     const char** strargs, const int* strargs_idx);

/* ---- ABI FFI piatta usata dall'export BASIC (blockchain.inc) -------------
 * Array di BYTE: ogni elemento viaggia come intero a 8 byte (valori 0..255) nello
 * scratch del bridge; qui viene ridotto a uint8_t e viceversa in uscita. `char*`
 * ritornati = malloc'd (il runtime FFI li copia e li libera subito dopo). */
int64_t ofx_bc_keccak256(const int64_t* data, int64_t len, int64_t* out, int64_t cap);
int64_t ofx_bc_ripemd160(const int64_t* data, int64_t len, int64_t* out, int64_t cap);
int64_t ofx_bc_blake2b256(const int64_t* data, int64_t len, int64_t* out, int64_t cap);

char* ofx_bc_base58encode(const int64_t* data, int64_t len);
int64_t ofx_bc_base58decode(const char* text, int64_t* out, int64_t cap);
char* ofx_bc_base32encode(const int64_t* data, int64_t len);
int64_t ofx_bc_base32decode(const char* text, int64_t* out, int64_t cap);
int64_t ofx_bc_rlpencode(const int64_t* data, int64_t len, int64_t* out, int64_t cap);
int64_t ofx_bc_varintencode(int64_t value, int64_t* out, int64_t cap);
int64_t ofx_bc_varintdecode(const int64_t* data, int64_t len);

char* ofx_bc_ethaddrfrompub(const int64_t* pub, int64_t len);
char* ofx_bc_ethaddrchecksum(const char* addr);
char* ofx_bc_btcaddrfrompub(const int64_t* pub, int64_t len, int64_t testnet);
double ofx_bc_blockdifficulty(const int64_t* header, int64_t len, int64_t bits);

int64_t ofx_bc_secp256k1sign(const int64_t* hash, int64_t hlen,
                             const int64_t* priv, int64_t plen,
                             int64_t* out, int64_t cap);
int64_t ofx_bc_getpublickey(const int64_t* priv, int64_t len, int64_t* out, int64_t cap);
/* chiave NON compressa (65 byte): 65 = scritti, 0 = non calcolabile, -1 = cap/argomenti */
int64_t ofx_bc_getpublickeyfull(const int64_t* priv, int64_t len, int64_t* out, int64_t cap);

char* ofx_bc_generatemnemonic(int64_t words);
int64_t ofx_bc_mnemonicstoseed(const char* mnemonic, const char* pass, int64_t* out, int64_t cap);
int64_t ofx_bc_derivekey(const int64_t* seed, int64_t len, const char* path,
                         int64_t* out, int64_t cap);

char* ofx_bc_abiencode(const char* sig, const double* nums, int64_t n);
int64_t ofx_bc_abidecode(const char* hex_in, double* out, int64_t cap);

#ifdef __cplusplus
}
#endif
#endif /* GMBLOCKCHAIN_H */
