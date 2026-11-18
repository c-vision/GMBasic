' SURFACE: esercita la superficie del pacchetto (drill), non uno scenario didattico.
' FILE: Crypto/CryptoFullCoverage.bas
' Ogni funzione del dominio BLOCKCHAIN col pacchetto `blockchain` (ABI a array di BYTE).
' NB (2026-11-13): API del pacchetto — i byte si LEGGONO da `out()` e la lunghezza è il
' valore di ritorno; l'esadecimale di un array si rende con `BYTESHEX`. Le dimensioni dei
' buffer contano: `DIM d(4)` = 5 byte (LEN = UBOUND+1 è la lunghezza d'ingresso).
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
    PRINT "============================================================"
    PRINT "Crypto - Copertura completa (pacchetto blockchain)"
    PRINT "============================================================"
    PRINT

    DIM d(4) AS BYTE
    DIM nd AS LONG = FROMHEX("68656c6c6f", d)     ' "hello" = 5 byte

    PRINT "-- hashing --"
    DIM k(31) AS BYTE
    KECCAK256(d, k) : PRINT "KECCAK256   "; BYTESHEX(k)
    DIM r(19) AS BYTE
    RIPEMD160(d, r) : PRINT "RIPEMD160   "; BYTESHEX(r)
    DIM b(31) AS BYTE
    BLAKE2B256(d, b) : PRINT "BLAKE2B256  "; BYTESHEX(b)

    PRINT "-- encoding --"
    PRINT "BASE32ENC   "; BASE32ENCODE(d)
    PRINT "BASE58ENC   "; BASE58ENCODE(d)
    DIM o(63) AS BYTE
    DIM n AS LONG
    n = BASE32DECODE("NBSWY3DP", o) : PRINT "BASE32DEC   "; LEFT(BYTESHEX(o), n * 2)
    n = BASE58DECODE("Cn8eVZg", o) : PRINT "BASE58DEC   "; LEFT(BYTESHEX(o), n * 2)
    DIM dog(2) AS BYTE
    DIM nDog AS LONG = FROMHEX("646f67", dog)
    n = RLPENCODE(dog, o) : PRINT "RLPENCODE   "; LEFT(BYTESHEX(o), n * 2)
    n = VARINTENCODE(300, o) : PRINT "VARINTENC   "; LEFT(BYTESHEX(o), n * 2)
    DIM vi(1) AS BYTE
    DIM nVi AS LONG = FROMHEX("ac02", vi)
    PRINT "VARINTDEC   "; VARINTDECODE(vi)

    PRINT "-- indirizzi --"
    DIM pubF(64) AS BYTE
    DIM nPubF AS LONG = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", pubF)
    DIM eth$ AS STRING
    eth$ = ETHADDRFROMPUB(pubF) : PRINT "ETHADDR     "; eth$
    PRINT "ETHCHECKSUM "; ETHADDRCHECKSUM(eth$)
    DIM pubC(32) AS BYTE
    DIM nPubC AS LONG = FROMHEX("0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798", pubC)
    PRINT "BTC main    "; BTCADDRFROMPUB(pubC, FALSE)
    PRINT "BTC test    "; BTCADDRFROMPUB(pubC, TRUE)

    PRINT "-- difficolta / ABI --"
    DIM hdr(31) AS BYTE
    DIM nHdr AS LONG = FROMHEX("0000000000000000000000000000000000000000000000000000000000000000", hdr)
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
    n = GETPUBLICKEY(priv, pub) : PRINT "GETPUBKEY   "; LEFT(BYTESHEX(pub), n * 2)
    DIM msg(31) AS BYTE
    DIM nMsg AS LONG = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", msg)
    DIM sig(79) AS BYTE
    n = SECP256K1SIGN(msg, priv, sig) : PRINT "SIGN DER B  "; n

    PRINT "-- HD wallet --"
    ' Il mnemonico e' CASUALE: si verifica la PROPRIETA' (quante parole), non il testo.
    DIM m$ AS STRING
    m$ = GENERATEMNEMONIC(12)
    DIM nWords AS LONG = 0
    DIM i AS LONG
    FOR i = 1 TO LEN(m$)
        IF MID(m$, i, 1) = " " THEN nWords = nWords + 1
    NEXT i
    PRINT "MNEMONIC    "; (nWords + 1); " parole"
    DIM seed(63) AS BYTE
    n = MNEMONICTOSEED(m$, "", seed) : PRINT "SEED        "; LEFT(BYTESHEX(seed), 32); "..."
    DIM dk(96) AS BYTE
    n = DERIVEKEY(seed, "m/44'/60'/0'/0/0", dk) : PRINT "DERIVEKEY   "; n; " byte"
    PRINT
    PRINT "Test completato."
END SUB
