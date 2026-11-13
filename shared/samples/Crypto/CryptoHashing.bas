' ============================================================================
' CRYPTOGRAPHY — hash, HMAC, encoding, password e random.
' Esempio per il namespace CRYPTOGRAPHY (sottoinsieme deterministico).
' NB (2026-11-13): API del PACCHETTO `crypto` (non più builtin): i decodificatori
' (`FROMBASE64`/`FROMHEX`) scrivono in un array BYTE out-param e ritornano la LUNGHEZZA;
' l'esadecimale di un array si rende con `BYTESHEX`. Vedi CRYPTOGRAPHY in packages/crypto.
' ============================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' --- Hash (varianti STR*: stringa esadecimale)
  PRINT "md5:     "; CR.STRMD5("hello")
  PRINT "sha1:    "; CR.STRSHA1("hello")
  PRINT "sha256:  "; CR.STRSHA256("hello")
  PRINT "sha512:  "; CR.STRSHA512("hello")

  ' --- HMAC (hex)
  PRINT "hmac256: "; CR.HMACSHA256("data", "key")

  ' --- Encoding (round-trip). I decodificatori scrivono i BYTE in out() e ritornano n.
  DIM dec8(64) AS BYTE
  b64$ = CR.TOBASE64("hello")
  PRINT "base64:  "; b64$
  DIM n64 AS LONG = CR.FROMBASE64(b64$, dec8)
  PRINT "decoded: "; LEFT(CR.BYTESHEX(dec8), n64 * 2); " ("; n64; " byte)"

  h$ = CR.TOHEX("AB")
  PRINT "hex:     "; h$
  DIM decH(64) AS BYTE
  DIM nH AS LONG = CR.FROMHEX(h$, decH)
  PRINT "unhex:   "; LEFT(CR.BYTESHEX(decH), nH * 2); " ("; nH; " byte)"

  ' --- Password (hash con salt; verify deterministico)
  hsh$ = CR.PASSWORDHASH("secret")
  IF CR.PASSWORDVERIFY("secret", hsh$) THEN PRINT "password: ok" ELSE PRINT "password: fail"

  ' --- Random (solo fatti: i valori sono casuali)
  ' Il valore casuale NON si stampa: cambia a ogni esecuzione -> la parita' fra le gambe
  ' sarebbe impossibile. Si verifica la PROPRIETA' (intervallo richiesto).
  rn = CR.RANDOMINT(1, 100)
  IF rn >= 1 AND rn <= 100 THEN PRINT "random:  ok" ELSE PRINT "random:  fail"
  DIM rb(16) AS BYTE
  DIM nrb AS LONG = CR.RANDOMCRYPTOBYTES(8, rb)
  PRINT "bytes:   "; LEN(LEFT(CR.BYTESHEX(rb), nrb * 2))
END SUB
