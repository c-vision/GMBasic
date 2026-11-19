' FILE: Security/HmacAndBase64Url.bas
' =================================================================================================
'       SECURITY — HMAC-SHA256/384/512 + Base64URL (JWT primitives)
' =================================================================================================
' Verifica end-to-end delle funzioni CRYPTOGRAPHY introdotte 2026-09-02:
'   CRYPTOGRAPHY.HMACSHA256/384/512(data, key) -> hex STRING (RFC 4231)
'   CRYPTOGRAPHY.TOBASE64URL(data) / FROMBASE64URL(s) (RFC 4648 sezione 5)
' Vettori attesi sul "The quick brown fox..." con chiave "key".
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato 2026-09-02).

REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

PRINT "=== HMAC (RFC 4231) ==="
DIM msg$ = "The quick brown fox jumps over the lazy dog"
PRINT "HMAC-SHA256: "; CRYPTOGRAPHY.HMACSHA256(msg$, "key")
PRINT "HMAC-SHA384: "; CRYPTOGRAPHY.HMACSHA384(msg$, "key")
PRINT "HMAC-SHA512: "; CRYPTOGRAPHY.HMACSHA512(msg$, "key")

IF CRYPTOGRAPHY.HMACSHA256(msg$, "key") = "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" THEN
    PRINT "HMAC256 VECTOR OK"
ELSE
    PRINT "HMAC256 VECTOR FAIL"
END IF

PRINT "=== Base64URL (RFC 4648 sez.5, no padding) ==="
PRINT "TOBASE64URL(hello): "; CRYPTOGRAPHY.TOBASE64URL("hello")
PRINT "TOBASE64URL(f):     "; CRYPTOGRAPHY.TOBASE64URL("f")

' API del PACCHETTO `crypto`: i decodificatori scrivono in un array BYTE out-param e
' ritornano la LUNGHEZZA; l'esadecimale di un array si rende con BYTESHEX().
DIM dec(63) AS BYTE
DIM nDec AS LONG = CRYPTOGRAPHY.FROMBASE64URL("aGVsbG8", dec)
DIM roundtrip$ = LEFT(CRYPTOGRAPHY.BYTESHEX(dec), nDec * 2)
PRINT "FROMBASE64URL roundtrip hex: "; roundtrip$
IF roundtrip$ = "68656c6c6f" THEN
    PRINT "B64URL ROUNDTRIP OK"
ELSE
    PRINT "B64URL ROUNDTRIP FAIL"
END IF
