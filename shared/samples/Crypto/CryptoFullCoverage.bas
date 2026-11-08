' SURFACE: esercita la superficie del pacchetto (drill), non uno scenario didattico.
' FILE: Crypto/CryptoFullCoverage.bas
' Ogni funzione del dominio BLOCKCHAIN col pacchetto `blockchain` (ABI a array di BYTE).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Crypto - Copertura completa (pacchetto blockchain)"
    PRINT "============================================================"
    PRINT

    DIM d() AS BYTE = FROMHEX("68656c6c6f")

    PRINT "-- hashing --"
    DIM k(31) AS BYTE
    KECCAK256(d, k) : PRINT "KECCAK256   "; TOHEX(k)
    DIM r(19) AS BYTE
    RIPEMD160(d, r) : PRINT "RIPEMD160   "; TOHEX(r)
    DIM b(31) AS BYTE
    BLAKE2B256(d, b) : PRINT "BLAKE2B256  "; TOHEX(b)

    PRINT "-- encoding --"
    PRINT "BASE32ENC   "; BASE32ENCODE(d)
    PRINT "BASE58ENC   "; BASE58ENCODE(d)
    DIM o(63) AS BYTE
    DIM n AS LONG
    n = BASE32DECODE("NBSWY3DP", o) : PRINT "BASE32DEC   "; LEFT(TOHEX(o), n * 2)
    n = BASE58DECODE("Cn8eVZg", o) : PRINT "BASE58DEC   "; LEFT(TOHEX(o), n * 2)
    DIM dog() AS BYTE = FROMHEX("646f67")
    n = RLPENCODE(dog, o) : PRINT "RLPENCODE   "; LEFT(TOHEX(o), n * 2)
    n = VARINTENCODE(300, o) : PRINT "VARINTENC   "; LEFT(TOHEX(o), n * 2)
    DIM vi() AS BYTE = FROMHEX("ac02")
    PRINT "VARINTDEC   "; VARINTDECODE(vi)

    PRINT "-- indirizzi --"
    DIM pubF() AS BYTE = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8")
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(pubF) : PRINT "ETHADDR     "; eth$
    PRINT "ETHCHECKSUM "; ETHADDRCHECKSUM(eth$)
    DIM pubC() AS BYTE = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798")
    PRINT "BTC main    "; BTCADDRFROMPUB(pubC, FALSE)
    PRINT "BTC test    "; BTCADDRFROMPUB(pubC, TRUE)

    PRINT "-- difficolta / ABI --"
    DIM hdr() AS BYTE = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000")
    PRINT "DIFFICOLTA  "; BLOCKDIFFICULTY(hdr, 553648127)
    PRINT "ABISELECTOR "; ABISELECTOR("balanceOf(address)")
    DIM nums(0) AS DOUBLE
    nums(0) = 1
    PRINT "ABIENCODE   "; ABIENCODE("balanceOf(address)", nums)
    DIM out(9) AS DOUBLE
    DIM w$ AS STRING
    w$ = "0x0000000000000000000000000000000000000000000000000000000000000001"
    n = ABIDECODE(w$, out) : PRINT "ABIDECODE   "; out(0)

    PRINT "-- chiavi / firma --"
    DIM priv(31) AS BYTE
    priv(31) = 1
    DIM pub(63) AS BYTE
    n = GETPUBLICKEY(priv, pub) : PRINT "GETPUBKEY   "; LEFT(TOHEX(pub), n * 2)
    DIM msg() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
    DIM sig(79) AS BYTE
    n = SECP256K1SIGN(msg, priv, sig) : PRINT "SIGN DER B  "; n

    PRINT "-- HD wallet --"
    DIM m$ AS STRING
    m$ = GENERATEMNEMONIC(12) : PRINT "MNEMONIC    "; m$
    DIM seed(63) AS BYTE
    n = MNEMONICTOSEED(m$, "", seed) : PRINT "SEED        "; LEFT(TOHEX(seed), 32); "..."
    DIM dk(96) AS BYTE
    n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk) : PRINT "DERIVEKEY   "; n; " byte"
    PRINT
    PRINT "Test completato."
END SUB
