' EXPECT: n= 6|b0= 0|b1= 255|b5= 128|md5=900150983cd24fb0d6963f7d28e17f72
' MiniTest ENCODING/CRYPTO: FROMHEX del pacchetto `crypto` su un buffer BYTE esplicito
' (`n = FROMHEX(testo, out())`), lettura dei byte (inclusi 0x00 e >127) e digest MD5.
' NB: la vecchia forma builtin `TOHEX(array)`/`FROMHEX(testo)` a 1 argomento che ritornava
' un array è stata ritirata con la migrazione del namespace CRYPTOGRAPHY a pacchetto FFI;
' vedi `Bugs/(open) - C99-TOHEX-TBASE64-Su-Array-Dopo-Migrazione-Crypto.md`.
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM b(5) AS BYTE
  DIM n AS LONG
  n = FROMHEX("00ff10aa7f80", b)
  PRINT "n="; n
  PRINT "b0="; b(0)
  PRINT "b1="; b(1)
  PRINT "b5="; b(5)
  PRINT "md5="; MD5("abc")
END SUB
