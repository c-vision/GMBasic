' crypto.inc — namespace CRYPTOGRAPHY via FFI (pacchetto `crypto`, lib gmbcrypto).
' Uso: REQUIRE "crypto"  →  IMPORT CRYPTOGRAPHY
'
' FORME (vincolo dell'emettitore nativo: una PROCEDURA non puo' ritornare un array):
'   - i digest e gli encoding ritornano STRING (esadecimale / Base64);
'   - le decodifiche e le cifrature scrivono in un ARRAY di BYTE del chiamante e
'     ritornano il numero di byte scritti (0 o <0 = errore);
'   - le chiavi sono handle LONG in un registro interno alla lib; KEYGEN scrive la
'     coppia in un ARRAY di LONG di 2 elementi (0 = privata, 1 = pubblica).
' Le primitive FFI passano i byte come interi a 8 byte; la lib li riduce a uint8_t.

NAMESPACE CRYPTOGRAPHY

' ── primitive (ABI: DECLARE NATIVE) ──────────────────────────────────────────
DECLARE NATIVE FUNCTION DIGEST LIB "gmbcrypto" ALIAS "ofx_ffi_digest_str" (algo AS STRING, text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION DIGESTHEX LIB "gmbcrypto" ALIAS "ofx_crypto_digest_hex_str" (algo AS STRING, text AS STRING) AS STRING
DECLARE NATIVE FUNCTION FILEDIGEST LIB "gmbcrypto" ALIAS "ofx_ffi_file_digest" (path AS STRING, algo AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION FILEHEX LIB "gmbcrypto" ALIAS "ofx_crypto_file_hex" (path AS STRING, algo AS STRING) AS STRING
DECLARE NATIVE FUNCTION HMACHEX LIB "gmbcrypto" ALIAS "ofx_crypto_hmac_hex" (algo AS STRING, key AS STRING, text AS STRING) AS STRING
DECLARE NATIVE FUNCTION HEXENC LIB "gmbcrypto" ALIAS "ofx_crypto_hex_encode_str" (text AS STRING) AS STRING
DECLARE NATIVE FUNCTION HEXDEC LIB "gmbcrypto" ALIAS "ofx_ffi_hex_decode" (text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION B64ENC LIB "gmbcrypto" ALIAS "ofx_crypto_base64_encode_str" (text AS STRING) AS STRING
DECLARE NATIVE FUNCTION B64DEC LIB "gmbcrypto" ALIAS "ofx_ffi_b64_decode" (text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION B64URLENC LIB "gmbcrypto" ALIAS "ofx_crypto_base64url_encode_str" (text AS STRING) AS STRING
DECLARE NATIVE FUNCTION B64URLDEC LIB "gmbcrypto" ALIAS "ofx_ffi_b64url_decode" (text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION PWHASH LIB "gmbcrypto" ALIAS "ofx_crypto_password_hash" (pw AS STRING) AS STRING
DECLARE NATIVE FUNCTION PWVERIFY LIB "gmbcrypto" ALIAS "ofx_crypto_password_verify" (pw AS STRING, stored AS STRING) AS LONG
DECLARE NATIVE FUNCTION CRC32RAW LIB "gmbcrypto" ALIAS "ofx_ffi_crc32" (data() AS BYTE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION CRC32CRAW LIB "gmbcrypto" ALIAS "ofx_ffi_crc32c" (data() AS BYTE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION CRC16RAW LIB "gmbcrypto" ALIAS "ofx_ffi_crc16" (data() AS BYTE, n AS LONG, variant AS LONG) AS LONG
DECLARE NATIVE FUNCTION CRC64RAW LIB "gmbcrypto" ALIAS "ofx_ffi_crc64" (data() AS BYTE, n AS LONG, variant AS LONG) AS LONG
DECLARE NATIVE FUNCTION ADLER32RAW LIB "gmbcrypto" ALIAS "ofx_ffi_adler32" (data() AS BYTE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION FLETCHER32RAW LIB "gmbcrypto" ALIAS "ofx_ffi_fletcher32" (data() AS BYTE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION CRC32HEXSTR LIB "gmbcrypto" ALIAS "ofx_crypto_crc32_hex_str" (text AS STRING) AS STRING
DECLARE NATIVE FUNCTION RANDBYTES LIB "gmbcrypto" ALIAS "ofx_ffi_random_bytes" (out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION CTEQ LIB "gmbcrypto" ALIAS "ofx_crypto_timing_safe_equal" (a AS STRING, b AS STRING) AS LONG
DECLARE NATIVE FUNCTION RANDINT LIB "gmbcrypto" ALIAS "ofx_crypto_random_int" (lo AS LONG, hi AS LONG) AS LONG
DECLARE NATIVE FUNCTION RANDDBL LIB "gmbcrypto" ALIAS "ofx_crypto_random_double" (lo AS DOUBLE, hi AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION RANDNORM LIB "gmbcrypto" ALIAS "ofx_crypto_random_normal" (mean AS DOUBLE, sd AS DOUBLE) AS DOUBLE

' simmetrico / asimmetrico
DECLARE NATIVE FUNCTION AESENCRAW LIB "gmbcrypto" ALIAS "ofx_ffi_aes_encrypt" (data AS STRING, password AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION AESDECRAW LIB "gmbcrypto" ALIAS "ofx_ffi_aes_decrypt" (enc() AS BYTE, n AS LONG, password AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION CHACHATEXT LIB "gmbcrypto" ALIAS "ofx_ffi_chacha_text" (text AS STRING, n AS LONG, key() AS BYTE, klen AS LONG, nonce() AS BYTE, nlen AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION CHACHAARR LIB "gmbcrypto" ALIAS "ofx_ffi_chacha_arr" (data() AS BYTE, n AS LONG, key() AS BYTE, klen AS LONG, nonce() AS BYTE, nlen AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION GENPAIR LIB "gmbcrypto" ALIAS "ofx_crypto_keys_generate_pair" (algo AS STRING, bits AS LONG, out() AS LONG) AS LONG
DECLARE NATIVE FUNCTION KEYEXPORTPEM LIB "gmbcrypto" ALIAS "ofx_crypto_keys_export_pem" (handle AS LONG) AS STRING
DECLARE NATIVE FUNCTION KEYIMPORTPEM LIB "gmbcrypto" ALIAS "ofx_crypto_keys_import64" (pem AS STRING) AS LONG
DECLARE NATIVE FUNCTION KEYCLOSEH LIB "gmbcrypto" ALIAS "ofx_crypto_keys_close64" (handle AS LONG) AS LONG
DECLARE NATIVE FUNCTION KEYISPRIVATEH LIB "gmbcrypto" ALIAS "ofx_crypto_keys_is_private" (handle AS LONG) AS LONG
DECLARE NATIVE FUNCTION SIGNRAW LIB "gmbcrypto" ALIAS "ofx_ffi_keys_sign" (handle AS LONG, data AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION VERIFYSIG LIB "gmbcrypto" ALIAS "ofx_ffi_keys_verify" (handle AS LONG, data AS STRING, sig() AS BYTE, slen AS LONG) AS LONG
DECLARE NATIVE FUNCTION ENCASYMRAW LIB "gmbcrypto" ALIAS "ofx_ffi_keys_encrypt" (handle AS LONG, data AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION DECASYMRAW LIB "gmbcrypto" ALIAS "ofx_ffi_keys_decrypt" (handle AS LONG, enc() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION VERIFYRAWR LIB "gmbcrypto" ALIAS "ofx_ffi_keys_verify_raw" (data() AS BYTE, n AS LONG, sig() AS BYTE, slen AS LONG, pub AS STRING) AS LONG
DECLARE NATIVE FUNCTION AEADENC LIB "gmbcrypto" ALIAS "ofx_ffi_aead_encrypt" (algo AS STRING, key() AS BYTE, klen AS LONG, iv() AS BYTE, ilen AS LONG, aad AS STRING, aadlen AS LONG, pt AS STRING, ptlen AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION AEADDEC LIB "gmbcrypto" ALIAS "ofx_ffi_aead_decrypt" (algo AS STRING, key() AS BYTE, klen AS LONG, iv() AS BYTE, ilen AS LONG, aad AS STRING, aadlen AS LONG, ct() AS BYTE, ctlen AS LONG, out() AS BYTE, cap AS LONG) AS LONG

' ── hash (esadecimale) ──────────────────────────────────────────────────────
PUBLIC FUNCTION MD5(text AS STRING) AS STRING
    RETURN DIGESTHEX("md5", text)
END FUNCTION
PUBLIC FUNCTION SHA1(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha1", text)
END FUNCTION
PUBLIC FUNCTION SHA256(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha256", text)
END FUNCTION
PUBLIC FUNCTION SHA384(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha384", text)
END FUNCTION
PUBLIC FUNCTION SHA512(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha512", text)
END FUNCTION
PUBLIC FUNCTION SHA3256(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha3-256", text)
END FUNCTION
PUBLIC FUNCTION SHA3512(text AS STRING) AS STRING
    RETURN DIGESTHEX("sha3-512", text)
END FUNCTION
PUBLIC FUNCTION RIPEMD160(text AS STRING) AS STRING
    RETURN DIGESTHEX("ripemd160", text)
END FUNCTION

PUBLIC FUNCTION STRMD5(text AS STRING) AS STRING
    RETURN MD5(text)
END FUNCTION
PUBLIC FUNCTION STRSHA1(text AS STRING) AS STRING
    RETURN SHA1(text)
END FUNCTION
PUBLIC FUNCTION STRSHA256(text AS STRING) AS STRING
    RETURN SHA256(text)
END FUNCTION
PUBLIC FUNCTION STRSHA512(text AS STRING) AS STRING
    RETURN SHA512(text)
END FUNCTION
PUBLIC FUNCTION MD5HEX(text AS STRING) AS STRING
    RETURN MD5(text)
END FUNCTION
PUBLIC FUNCTION SHA1HEX(text AS STRING) AS STRING
    RETURN SHA1(text)
END FUNCTION
PUBLIC FUNCTION SHA256HEX(text AS STRING) AS STRING
    RETURN SHA256(text)
END FUNCTION
PUBLIC FUNCTION SHA512HEX(text AS STRING) AS STRING
    RETURN SHA512(text)
END FUNCTION

' digest di un testo in un buffer di BYTE (per chi vuole i byte grezzi)
PUBLIC FUNCTION HASH(text AS STRING, algo AS STRING, out() AS BYTE) AS LONG
    RETURN DIGEST(algo, text, out, LEN(out))
END FUNCTION

' ── HMAC: (data, key) come il builtin, risultato esadecimale ──────────────────
PUBLIC FUNCTION HMACSHA256(data AS STRING, key AS STRING) AS STRING
    RETURN HMACHEX("sha256", key, data)
END FUNCTION
PUBLIC FUNCTION HMACSHA384(data AS STRING, key AS STRING) AS STRING
    RETURN HMACHEX("sha384", key, data)
END FUNCTION
PUBLIC FUNCTION HMACSHA512(data AS STRING, key AS STRING) AS STRING
    RETURN HMACHEX("sha512", key, data)
END FUNCTION

' ── hash di file ────────────────────────────────────────────────────────────
PUBLIC FUNCTION HASHFILE(path AS STRING, algo AS STRING) AS STRING
    RETURN FILEHEX(path, algo)
END FUNCTION
PUBLIC FUNCTION STRHASHFILE(path AS STRING, algo AS STRING) AS STRING
    RETURN FILEHEX(path, algo)
END FUNCTION

' ── password (formato compatibile col builtin: Base64(salt).Base64(hash)) ────
PUBLIC FUNCTION PASSWORDHASH(pw AS STRING) AS STRING
    RETURN PWHASH(pw)
END FUNCTION
PUBLIC FUNCTION PASSWORDVERIFY(pw AS STRING, stored AS STRING) AS LONG
    RETURN PWVERIFY(pw, stored)
END FUNCTION

' ── encoding ────────────────────────────────────────────────────────────────
PUBLIC FUNCTION TOHEX(text AS STRING) AS STRING
    RETURN HEXENC(text)
END FUNCTION
PUBLIC FUNCTION FROMHEX(text AS STRING, out() AS BYTE) AS LONG
    RETURN HEXDEC(text, out, LEN(out))
END FUNCTION

' Hex (minuscolo, 2 cifre per byte) di un array BYTE.
' Sostituisce il vecchio builtin `TOHEX(array)`, ritirato insieme alle altre forme array del
' namespace quando CRYPTOGRAPHY è diventato un pacchetto FFI (la FUNCTION omonima TOHEX
' accetta una STRINGA). Usata dai MiniTest/sample del dominio blockchain.
PUBLIC FUNCTION BYTESHEX(a() AS BYTE) AS STRING
    DIM H AS STRING = "0123456789abcdef"
    DIM s AS STRING = ""
    DIM i AS LONG
    FOR i = 0 TO UBOUND(a)
        s = s + MID(H, CLNG(a(i)) \ 16 + 1, 1) + MID(H, CLNG(a(i)) MOD 16 + 1, 1)
    NEXT i
    RETURN s
END FUNCTION
PUBLIC FUNCTION TOBASE64(text AS STRING) AS STRING
    RETURN B64ENC(text)
END FUNCTION
PUBLIC FUNCTION FROMBASE64(text AS STRING, out() AS BYTE) AS LONG
    RETURN B64DEC(text, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION TOBASE64URL(text AS STRING) AS STRING
    RETURN B64URLENC(text)
END FUNCTION
PUBLIC FUNCTION FROMBASE64URL(text AS STRING, out() AS BYTE) AS LONG
    RETURN B64URLDEC(text, out, LEN(out))
END FUNCTION

' ── checksum ────────────────────────────────────────────────────────────────
PUBLIC FUNCTION CRC32(data() AS BYTE) AS LONG
    RETURN CRC32RAW(data, LEN(data))
END FUNCTION
PUBLIC FUNCTION CRC32C(data() AS BYTE) AS LONG
    RETURN CRC32CRAW(data, LEN(data))
END FUNCTION
' variant: 0=ARC/IBM  1=CCITT-FALSE  2=XMODEM  3=KERMIT
PUBLIC FUNCTION CRC16(data() AS BYTE, variant AS LONG) AS LONG
    RETURN CRC16RAW(data, LEN(data), variant)
END FUNCTION
' variant: 0=ECMA (init 0)  1=CRC-64/XZ
PUBLIC FUNCTION CRC64(data() AS BYTE, variant AS LONG) AS LONG
    RETURN CRC64RAW(data, LEN(data), variant)
END FUNCTION
PUBLIC FUNCTION ADLER32(data() AS BYTE) AS LONG
    RETURN ADLER32RAW(data, LEN(data))
END FUNCTION
PUBLIC FUNCTION FLETCHER32(data() AS BYTE) AS LONG
    RETURN FLETCHER32RAW(data, LEN(data))
END FUNCTION
PUBLIC FUNCTION CRC32HEX(text AS STRING) AS STRING
    RETURN CRC32HEXSTR(text)
END FUNCTION

' ── random ──────────────────────────────────────────────────────────────────
PUBLIC FUNCTION RANDOMCRYPTOBYTES(n AS LONG, out() AS BYTE) AS LONG
    RETURN RANDBYTES(out, n)
END FUNCTION
PUBLIC FUNCTION RANDOMINT(lo AS LONG, hi AS LONG) AS LONG
    RETURN RANDINT(lo, hi)
END FUNCTION
PUBLIC FUNCTION RANDOMDOUBLE(lo AS DOUBLE, hi AS DOUBLE) AS DOUBLE
    RETURN RANDDBL(lo, hi)
END FUNCTION
PUBLIC FUNCTION RANDOMNORMAL(mean AS DOUBLE, sd AS DOUBLE) AS DOUBLE
    RETURN RANDNORM(mean, sd)
END FUNCTION

' ── simmetrico ──────────────────────────────────────────────────────────────
' AES-GCM con password: cifrato = salt16 | nonce12 | tag16 | ciphertext (44 + n byte)
PUBLIC FUNCTION AESENCRYPT(text AS STRING, password AS STRING, out() AS BYTE) AS LONG
    RETURN AESENCRAW(text, password, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION AESDECRYPT(enc() AS BYTE, n AS LONG, password AS STRING, out() AS BYTE) AS LONG
    RETURN AESDECRAW(enc, n, password, out, LEN(out))
END FUNCTION

' ChaCha20 grezzo (RFC 8439): key 32 byte, nonce 12 byte
PUBLIC FUNCTION CHACHA20ENCRYPT(text AS STRING, key() AS BYTE, nonce() AS BYTE, out() AS BYTE) AS LONG
    RETURN CHACHATEXT(text, LEN(text), key, LEN(key), nonce, LEN(nonce), out, LEN(out))
END FUNCTION
PUBLIC FUNCTION CHACHA20DECRYPT(enc() AS BYTE, n AS LONG, key() AS BYTE, nonce() AS BYTE, out() AS BYTE) AS LONG
    RETURN CHACHAARR(enc, n, key, LEN(key), nonce, LEN(nonce), out, LEN(out))
END FUNCTION

' ── asimmetrico (handle LONG opachi) ────────────────────────────────────────
' KEYGEN("ECDSA", 256, h): h(0) = handle privata, h(1) = handle pubblica
PUBLIC FUNCTION KEYGEN(algo AS STRING, bits AS LONG, h() AS LONG) AS LONG
    RETURN GENPAIR(algo, bits, h)
END FUNCTION
PUBLIC FUNCTION KEYEXPORT(handle AS LONG) AS STRING
    RETURN KEYEXPORTPEM(handle)
END FUNCTION
PUBLIC FUNCTION KEYIMPORT(pem AS STRING) AS LONG
    RETURN KEYIMPORTPEM(pem)
END FUNCTION
PUBLIC FUNCTION KEYCLOSE(handle AS LONG) AS LONG
    RETURN KEYCLOSEH(handle)
END FUNCTION
PUBLIC FUNCTION KEYISPRIVATE(handle AS LONG) AS LONG
    RETURN KEYISPRIVATEH(handle)
END FUNCTION
PUBLIC FUNCTION SIGN(text AS STRING, handle AS LONG, out() AS BYTE) AS LONG
    RETURN SIGNRAW(handle, text, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION VERIFY(sig() AS BYTE, slen AS LONG, text AS STRING, handle AS LONG) AS LONG
    RETURN VERIFYSIG(handle, text, sig, slen)
END FUNCTION
PUBLIC FUNCTION ENCRYPTASYM(text AS STRING, handle AS LONG, out() AS BYTE) AS LONG
    RETURN ENCASYMRAW(handle, text, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION DECRYPTASYM(enc() AS BYTE, n AS LONG, handle AS LONG, out() AS BYTE) AS LONG
    RETURN DECASYMRAW(handle, enc, n, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION VERIFYRAW(sig() AS BYTE, slen AS LONG, data() AS BYTE, dn AS LONG, pub AS STRING) AS LONG
    RETURN VERIFYRAWR(data, dn, sig, slen, pub)
END FUNCTION

' ── AEAD (out = ciphertext || tag a 16 byte) ────────────────────────────────
PUBLIC FUNCTION AEADENCRYPT(algo AS STRING, text AS STRING, key() AS BYTE, nonce() AS BYTE, aad AS STRING, out() AS BYTE) AS LONG
    RETURN AEADENC(algo, key, LEN(key), nonce, LEN(nonce), aad, LEN(aad), text, LEN(text), out, LEN(out))
END FUNCTION
PUBLIC FUNCTION AEADDECRYPT(algo AS STRING, ct() AS BYTE, n AS LONG, key() AS BYTE, nonce() AS BYTE, aad AS STRING, out() AS BYTE) AS LONG
    RETURN AEADDEC(algo, key, LEN(key), nonce, LEN(nonce), aad, LEN(aad), ct, n, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION CHACHA20POLY1305ENCRYPT(text AS STRING, key() AS BYTE, nonce() AS BYTE, aad AS STRING, out() AS BYTE) AS LONG
    RETURN AEADENCRYPT("chacha20poly1305", text, key, nonce, aad, out)
END FUNCTION
PUBLIC FUNCTION CHACHA20POLY1305DECRYPT(ct() AS BYTE, n AS LONG, key() AS BYTE, nonce() AS BYTE, aad AS STRING, out() AS BYTE) AS LONG
    RETURN AEADDECRYPT("chacha20poly1305", ct, n, key, nonce, aad, out)
END FUNCTION

' ── confronto a tempo costante ──────────────────────────────────────────────
PUBLIC FUNCTION TIMINGSAFEEQUAL(a AS STRING, b AS STRING) AS LONG
    RETURN CTEQ(a, b)
END FUNCTION
