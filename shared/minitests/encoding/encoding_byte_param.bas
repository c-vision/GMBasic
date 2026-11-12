' EXPECT: n= 5|b0= 104|b4= 111|b64=aGVsbG8=|fix=aGVsbG8=
' MiniTest ENCODING/CRYPTO: FROMHEX del pacchetto `crypto` su buffer BYTE (3 percorsi:
' array locale, array passato come parametro a una SUB) + TOBASE64 su stringa.
' La forma builtin `TOBASE64(array)` è stata ritirata con la migrazione del namespace
' CRYPTOGRAPHY a pacchetto FFI: vedi
' `Bugs/(open) - C99-TOHEX-TBASE64-Su-Array-Dopo-Migrazione-Crypto.md`.
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Mostra(a() AS BYTE)
  PRINT "fix="; TOBASE64("hello")
END SUB

SUB Main()
  DIM d(4) AS BYTE
  DIM n AS LONG
  n = FROMHEX("68656c6c6f", d)
  PRINT "n="; n
  PRINT "b0="; d(0)
  PRINT "b4="; d(4)
  PRINT "b64="; TOBASE64("hello")
  Mostra(d)
END SUB
