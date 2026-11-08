' blockchain.inc — namespace BLOCKCHAIN via FFI (pacchetto `blockchain`, lib gmbblockchain).
' I byte sono ARRAY DI BYTE: il bridge passa ogni elemento come intero a 8 byte (valori
' 0..255) e la lib li riduce a uint8_t. I buffer di uscita li dimensiona il chiamante
' (`DIM out(31) AS BYTE`); i wrapper deducono le lunghezze con UBOUND.
' Uso: REQUIRE "blockchain"  →  IMPORT BLOCKCHAIN

NAMESPACE BLOCKCHAIN

' ── primitive (ABI: array di BYTE + lunghezze esplicite) ────────────────────
DECLARE NATIVE SUB BCKECCAK256 LIB "gmbblockchain" ALIAS "ofx_bc_keccak256" (data() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE SUB BCRIPEMD160 LIB "gmbblockchain" ALIAS "ofx_bc_ripemd160" (data() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE SUB BCBLAKE2B256 LIB "gmbblockchain" ALIAS "ofx_bc_blake2b256" (data() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCBASE58ENCODE LIB "gmbblockchain" ALIAS "ofx_bc_base58encode" (data() AS BYTE, n AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCBASE58DECODE LIB "gmbblockchain" ALIAS "ofx_bc_base58decode" (text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCBASE32ENCODE LIB "gmbblockchain" ALIAS "ofx_bc_base32encode" (data() AS BYTE, n AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCBASE32DECODE LIB "gmbblockchain" ALIAS "ofx_bc_base32decode" (text AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCRLPENCODE LIB "gmbblockchain" ALIAS "ofx_bc_rlpencode" (data() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCVARINTENCODE LIB "gmbblockchain" ALIAS "ofx_bc_varintencode" (value AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCVARINTDECODE LIB "gmbblockchain" ALIAS "ofx_bc_varintdecode" (data() AS BYTE, n AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCETHADDRFROMPUB LIB "gmbblockchain" ALIAS "ofx_bc_ethaddrfrompub" (pub() AS BYTE, n AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCETHADDRCHECKSUM LIB "gmbblockchain" ALIAS "ofx_bc_ethaddrchecksum" (address AS STRING) AS STRING
DECLARE NATIVE FUNCTION BCBTCADDRFROMPUB LIB "gmbblockchain" ALIAS "ofx_bc_btcaddrfrompub" (pub() AS BYTE, n AS LONG, testnet AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCBLOCKDIFFICULTY LIB "gmbblockchain" ALIAS "ofx_bc_blockdifficulty" (header() AS BYTE, n AS LONG, bits AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION BCSECP256K1SIGN LIB "gmbblockchain" ALIAS "ofx_bc_secp256k1sign" (hash() AS BYTE, hn AS LONG, priv() AS BYTE, pn AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCGETPUBLICKEY LIB "gmbblockchain" ALIAS "ofx_bc_getpublickey" (priv() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCGETPUBLICKEYFULL LIB "gmbblockchain" ALIAS "ofx_bc_getpublickeyfull" (priv() AS BYTE, n AS LONG, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCGENERATEMNEMONIC LIB "gmbblockchain" ALIAS "ofx_bc_generatemnemonic" (words AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCMNEMONICSTOSEED LIB "gmbblockchain" ALIAS "ofx_bc_mnemonicstoseed" (mnemonic AS STRING, pass AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCDERIVEKEY LIB "gmbblockchain" ALIAS "ofx_bc_derivekey" (seed() AS BYTE, n AS LONG, path AS STRING, out() AS BYTE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION BCABIENCODE LIB "gmbblockchain" ALIAS "ofx_bc_abiencode" (signature AS STRING, nums() AS DOUBLE, n AS LONG) AS STRING
DECLARE NATIVE FUNCTION BCABIDECODE LIB "gmbblockchain" ALIAS "ofx_bc_abidecode" (hex AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG

' ── hashing ─────────────────────────────────────────────────────────────────
' out() dimensionato dal chiamante: 31 per Keccak-256/BLAKE2b-256, 19 per RIPEMD-160.
PUBLIC SUB KECCAK256(data() AS BYTE, out() AS BYTE)
    CALL BCKECCAK256(data, LEN(data), out, LEN(out))
END SUB
PUBLIC SUB RIPEMD160(data() AS BYTE, out() AS BYTE)
    CALL BCRIPEMD160(data, LEN(data), out, LEN(out))
END SUB
PUBLIC SUB BLAKE2B256(data() AS BYTE, out() AS BYTE)
    CALL BCBLAKE2B256(data, LEN(data), out, LEN(out))
END SUB

' ── encoding / decoding ─────────────────────────────────────────────────────
PUBLIC FUNCTION BASE58ENCODE(data() AS BYTE) AS STRING
    RETURN BCBASE58ENCODE(data, LEN(data))
END FUNCTION
' Ritorna quanti byte sono stati scritti in out().
PUBLIC FUNCTION BASE58DECODE(text AS STRING, out() AS BYTE) AS LONG
    RETURN BCBASE58DECODE(text, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION BASE32ENCODE(data() AS BYTE) AS STRING
    RETURN BCBASE32ENCODE(data, LEN(data))
END FUNCTION
PUBLIC FUNCTION BASE32DECODE(text AS STRING, out() AS BYTE) AS LONG
    RETURN BCBASE32DECODE(text, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION RLPENCODE(data() AS BYTE, out() AS BYTE) AS LONG
    RETURN BCRLPENCODE(data, LEN(data), out, LEN(out))
END FUNCTION
PUBLIC FUNCTION VARINTENCODE(value AS LONG, out() AS BYTE) AS LONG
    RETURN BCVARINTENCODE(value, out, LEN(out))
END FUNCTION
PUBLIC FUNCTION VARINTDECODE(data() AS BYTE) AS LONG
    RETURN BCVARINTDECODE(data, LEN(data))
END FUNCTION

' ── indirizzi ───────────────────────────────────────────────────────────────
' ATTENZIONE: vuole la chiave NON compressa (65 byte: 0x04 || X || Y) e NON
' decomprime un punto compresso. Con i 33 byte di GETPUBLICKEY restituisce un indirizzo
' DIVERSO e sbagliato (priv=1: 0x7034adcc... invece di 0x7e5f4552...).
' La forma si deduce dal PRIMO byte (0x04 / 0x02-0x03), non da LEN: un buffer piu' grande
' del necessario non cambia l'hash. Per ETH: ricavare la chiave con GETFULLPUBLICKEY(priv, out).
' Per BTC: BTCADDRFROMPUB, che vuole proprio la compressa. Dettagli nel README del pacchetto.
PUBLIC FUNCTION ETHADDRFROMPUB(pub() AS BYTE) AS STRING
    RETURN BCETHADDRFROMPUB(pub, LEN(pub))
END FUNCTION
PUBLIC FUNCTION ETHADDRCHECKSUM(address AS STRING) AS STRING
    RETURN BCETHADDRCHECKSUM(address)
END FUNCTION
' Vuole la chiave COMPRESSA (33 byte: 0x02/0x03 || X): e' quella scritta da
' DERIVEKEY e restituita da GETPUBLICKEY. Non passarla a ETHADDRFROMPUB (vedi sopra).
PUBLIC FUNCTION BTCADDRFROMPUB(pub() AS BYTE, testnet AS LONG) AS STRING
    RETURN BCBTCADDRFROMPUB(pub, LEN(pub), testnet)
END FUNCTION
PUBLIC FUNCTION BLOCKDIFFICULTY(header() AS BYTE, bits AS LONG) AS DOUBLE
    RETURN BCBLOCKDIFFICULTY(header, LEN(header), bits)
END FUNCTION

' ── firme e chiavi ──────────────────────────────────────────────────────────
' Firma ECDSA (DER) di un hash SHA-256 gia' calcolato; ritorna la lunghezza DER.
PUBLIC FUNCTION SECP256K1SIGN(hash() AS BYTE, priv() AS BYTE, out() AS BYTE) AS LONG
    RETURN BCSECP256K1SIGN(hash, LEN(hash), priv, LEN(priv), out, LEN(out))
END FUNCTION
' 33 byte compressi in out() (out dimensionato 32).
PUBLIC FUNCTION GETPUBLICKEY(priv() AS BYTE, out() AS BYTE) AS LONG
    RETURN BCGETPUBLICKEY(priv, LEN(priv), out, LEN(out))
END FUNCTION
' Chiave pubblica NON compressa: 65 byte (0x04 || X || Y) in DIM out(64) AS BYTE.
' E' la forma che vuole ETHADDRFROMPUB: con la compressa di GETPUBLICKEY l'indirizzo ETH
' risulta DIVERSO e sbagliato (la lib non decomprime il punto).
' Ritorna 65 = byte scritti, 0 = non calcolabile (lib senza OpenSSL), -1 = buffer piccolo.
PUBLIC FUNCTION GETFULLPUBLICKEY(priv() AS BYTE, out() AS BYTE) AS LONG
    RETURN BCGETPUBLICKEYFULL(priv, LEN(priv), out, LEN(out))
END FUNCTION

' ── HD wallet (BIP39/BIP32) ─────────────────────────────────────────────────
PUBLIC FUNCTION GENERATEMNEMONIC(words AS LONG) AS STRING
    RETURN BCGENERATEMNEMONIC(words)
END FUNCTION
' 64 byte di seed in out() (out dimensionato 63).
PUBLIC FUNCTION MNEMONICTOSEED(mnemonic AS STRING, pass AS STRING, out() AS BYTE) AS LONG
    RETURN BCMNEMONICSTOSEED(mnemonic, pass, out, LEN(out))
END FUNCTION
' out() = priv(32) || pub(33) || chainCode(32) = 97 byte (out dimensionato 96).
' Il path non e' applicato: si ottiene la chiave master (parita' con il builtin dismesso).
PUBLIC FUNCTION DERIVEKEY(seed() AS BYTE, path AS STRING, out() AS BYTE) AS LONG
    RETURN BCDERIVEKEY(seed, LEN(seed), path, out, LEN(out))
END FUNCTION

' ── ABI EVM ─────────────────────────────────────────────────────────────────
PUBLIC FUNCTION ABIENCODE(signature AS STRING, nums() AS DOUBLE) AS STRING
    RETURN BCABIENCODE(signature, nums, LEN(nums))
END FUNCTION
' Solo selettore (primi 4 byte del Keccak della firma), senza argomenti: "0x70a08231".
PUBLIC FUNCTION ABISELECTOR(signature AS STRING) AS STRING
    DIM z(0) AS DOUBLE
    z(0) = 0
    RETURN BCABIENCODE(signature, z, 0)
END FUNCTION
' Numero di parole ABI (32 byte) decodificate in out().
PUBLIC FUNCTION ABIDECODE(hex AS STRING, out() AS DOUBLE) AS LONG
    RETURN BCABIDECODE(hex, out, LEN(out))
END FUNCTION

