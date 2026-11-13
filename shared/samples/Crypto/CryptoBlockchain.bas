' FILE: Crypto/CryptoBlockchain.bas
' =================================================================================================
'       GUIDA TEORICA: PRIMITIVE CRITTOGRAFICHE PER BLOCKCHAIN (pacchetto `blockchain`)
' =================================================================================================
'
' Il dominio `BLOCKCHAIN` non e' un builtin: e' una LIBRERIA C (OpenSSL per secp256k1 e i
' BIGNUM della difficolta') distribuita come pacchetto.
'
'   REQUIRE "blockchain"        ' risolve il pacchetto e include blockchain.inc
'   IMPORT BLOCKCHAIN           ' porta il dominio in scope
'   REQUIRE "crypto" / IMPORT CRYPTOGRAPHY   ' FROMHEX / TOHEX / BYTESHEX
'
' ABI (2026-11-13): i dati binari sono ARRAY DI BYTE e le funzioni che PRODUCONO byte scrivono
' in un buffer out() dimensionato dal CHIAMANTE e ritornano quanti byte hanno scritto.
' ATTENZIONE alle dimensioni: i wrapper usano LEN(buffer) = UBOUND+1 come lunghezza
' dell'input, quindi un buffer piu' grande del dato fa hasare anche i byte a zero in coda
' (`DIM d(19)` = 20 byte; `DIM d(31)` = 32). L'esadecimale di un array si rende con BYTESHEX.
'
' 1. HASHING SPECIALIZZATO (KECCAK256, RIPEMD160, BLAKE2B256)
' 2. ENCODING (BASE58 / BASE32)
' 3. INDIRIZZI ETH (EIP-55) E BTC (P2PKH mainnet/testnet)
' 4. SERIALIZZAZIONE: RLP, VARINT, difficolta', firma ECDSA secp256k1
' =================================================================================================

REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    PRINT "=================================================================="
    PRINT " CryptoBlockchain - primitive blockchain via pacchetto FFI"
    PRINT "=================================================================="
    PRINT

    ' "gm-basic blockchain" = 19 byte (g m - b a s i c _ b l o c k c h a i n)
    DIM data(18) AS BYTE
    DIM nData AS LONG = FROMHEX("676d2d626173696320626c6f636b636861696e", data)

    PRINT "--- 1. Hashing ---"
    DIM k(31) AS BYTE
    KECCAK256(data, k)
    PRINT "Keccak-256:  "; BYTESHEX(k)
    DIM r(19) AS BYTE
    RIPEMD160(data, r)
    PRINT "RIPEMD-160:  "; BYTESHEX(r)
    DIM b(31) AS BYTE
    BLAKE2B256(data, b)
    PRINT "BLAKE2b-256: "; BYTESHEX(b)
    PRINT

    PRINT "--- 2. Encoding ---"
    PRINT "Base32: "; BASE32ENCODE(data)
    PRINT "Base58: "; BASE58ENCODE(data)
    DIM back(63) AS BYTE
    DIM n AS LONG
    n = BASE32DECODE(BASE32ENCODE(data), back)
    PRINT "Base32 round-trip: "; LEFT(BYTESHEX(back), n * 2)
    n = BASE58DECODE(BASE58ENCODE(data), back)
    PRINT "Base58 round-trip: "; LEFT(BYTESHEX(back), n * 2)
    PRINT

    PRINT "--- 3. Indirizzi ---"
    ' chiave pubblica NON compressa (65 byte, inizio 0x04): e' quella che hasha ETHADDRFROMPUB
    DIM pubFull(64) AS BYTE
    DIM nPubFull AS LONG = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", pubFull)
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(pubFull)
    PRINT "Ethereum:        "; eth$
    PRINT "Ethereum (EIP55): "; ETHADDRCHECKSUM(eth$)
    ' chiave pubblica COMPRESSA (33 byte) per l'indirizzo Bitcoin
    DIM pubCmp(32) AS BYTE
    DIM nPubCmp AS LONG = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798", pubCmp)
    PRINT "Bitcoin mainnet: "; BTCADDRFROMPUB(pubCmp, FALSE)
    PRINT "Bitcoin testnet: "; BTCADDRFROMPUB(pubCmp, TRUE)
    PRINT

    PRINT "--- 4. Serializzazione e utilita' ---"
    DIM o(63) AS BYTE
    DIM dog(2) AS BYTE
    DIM nDog AS LONG = FROMHEX("646f67", dog)
    n = RLPENCODE(dog, o)
    PRINT "RLP(dog):        "; LEFT(BYTESHEX(o), n * 2)
    n = VARINTENCODE(300, o)
    PRINT "VarInt(300):     "; LEFT(BYTESHEX(o), n * 2)
    DIM vi(1) AS BYTE
    DIM nVi AS LONG = FROMHEX("ac02", vi)
    PRINT "VarInt decode:   "; VARINTDECODE(vi)
    DIM hdr(31) AS BYTE
    DIM nHdr AS LONG = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000", hdr)
    PRINT "Difficolta':     "; BLOCKDIFFICULTY(hdr, 553648127)
    PRINT

    PRINT "--- 5. Chiavi e firma (secp256k1) ---"
    DIM priv(31) AS BYTE
    priv(31) = 1
    DIM pub(63) AS BYTE
    n = GETPUBLICKEY(priv, pub)
    PRINT "Pubkey(1):       "; LEFT(BYTESHEX(pub), n * 2)
    DIM msg(31) AS BYTE
    DIM nMsg AS LONG = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", msg)
    DIM sig(79) AS BYTE
    n = SECP256K1SIGN(msg, priv, sig)
    PRINT "Firma DER (byte):"; n
    PRINT "Firma DER (hex): "; LEFT(BYTESHEX(sig), n * 2)
    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB
