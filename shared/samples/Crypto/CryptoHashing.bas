' ============================================================================
' CRYPTOGRAPHY — hash, HMAC, encoding, password e random.
' Esempio per il namespace CRYPTOGRAPHY (sottoinsieme deterministico).
' ============================================================================
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' --- Hash (byte[]; STRSHA* ritorna la stringa esadecimale)
  PRINT "md5:     "; CR.STRMD5("hello")
  PRINT "sha1:    "; CR.STRSHA1("hello")
  PRINT "sha256:  "; CR.STRSHA256("hello")
  PRINT "sha512:  "; CR.STRSHA512("hello")

  ' --- HMAC (hex)
  PRINT "hmac256: "; CR.HMACSHA256("data", "key")

  ' --- Encoding (round-trip)
  b64$ = CR.TOBASE64("hello")
  PRINT "base64:  "; b64$
  PRINT "decoded: "; CR.FROMBASE64(b64$)
  h$ = CR.TOHEX("AB")
  PRINT "hex:     "; h$
  PRINT "unhex:   "; CR.FROMHEX(h$)

  ' --- Password (hash con salt; verify deterministico)
  hsh$ = CR.PASSWORDHASH("secret")
  IF CR.PASSWORDVERIFY("secret", hsh$) THEN PRINT "password: ok" ELSE PRINT "password: fail"

  ' --- Random (solo fatti: i valori sono casuali)
  ' Il valore casuale NON si stampa: cambia a ogni esecuzione -> la parita' fra le gambe
  ' sarebbe impossibile. Si verifica la PROPRIETA' (intervallo richiesto).
  rn = CR.RANDOMINT(1, 100)
  IF rn >= 1 AND rn <= 100 THEN PRINT "random:  ok" ELSE PRINT "random:  fail"
  PRINT "bytes:   "; LEN(CR.TOHEX(CR.RANDOMBYTES(8)))
END SUB
