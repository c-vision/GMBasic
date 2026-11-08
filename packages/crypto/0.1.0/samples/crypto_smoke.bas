' crypto_smoke.bas — pacchetto `crypto` (superficie 0.1.0), lib libgmbcrypto (OpenSSL 3).
' I digest/encoding ritornano STRING (esadecimale/Base64); decodifiche e cifrature
' scrivono in un ARRAY di BYTE del chiamante e ritornano il numero di byte.
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Main()
  PRINT "sha256(abc)  = "; SHA256HEX("abc")
  PRINT "sha512(abc)  = "; SHA512HEX("abc")
  PRINT "hmac256      = "; HMACSHA256("data", "key")
  PRINT "crc32hex     = "; CRC32HEX("data")
  PRINT "base64       = "; TOBASE64("hello")
  PRINT "base64url    = "; TOBASE64URL("hello?")
  DIM d(7) AS BYTE
  n = FROMBASE64("aGVsbG8=", d)
  PRINT "frombase64   = "; n; " byte"
  h$ = PASSWORDHASH("secret")
  IF PASSWORDVERIFY("secret", h$) THEN PRINT "password     = ok" ELSE PRINT "password     = FAIL"
  IF PASSWORDVERIFY("wrong", h$) THEN PRINT "wrong-pass   = FAIL" ELSE PRINT "wrong-pass   = ok"
  DIM keys(1) AS LONG
  CALL CRYPTOGRAPHY.KEYGEN("ECDSA", 256, keys)
  DIM sig(255) AS BYTE
  sn = CRYPTOGRAPHY.SIGN("hello", keys(0), sig)
  IF CRYPTOGRAPHY.VERIFY(sig, sn, "hello", keys(1)) THEN PRINT "sign/verify  = ok" ELSE PRINT "sign/verify  = FAIL"
  CALL CRYPTOGRAPHY.KEYCLOSE(keys(0))
  CALL CRYPTOGRAPHY.KEYCLOSE(keys(1))
END SUB
