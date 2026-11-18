' ============================================================================
' CRYPTOGRAPHY — crittografia asimmetrica: KEYGEN, SIGN, VERIFY, KEYEXPORT.
' Esempio per il namespace CRYPTOGRAPHY (pacchetto `crypto`, chiavi ECDSA via OpenSSL).
' NB (2026-11-13): API del pacchetto FFI — `KEYGEN(algo, bits, h())` riempie l'array di
' handle (`h(0)`=privata, `h(1)`=pubblica) e ritorna un esito; `SIGN` scrive la firma in
' `out()` e ritorna la LUNGHEZZA, che `VERIFY` richiede insieme alla firma.
' ============================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' Genera una coppia di chiavi: keys(0) = handle privato, keys(1) = handle pubblico.
  DIM keys(2) AS LONG
  DIM nK AS LONG = CR.KEYGEN("ECDSA", 256, keys)
  IF nK < 0 THEN PRINT "keygen: fail" : EXIT SUB

  ' Firma i dati con la chiave privata: `out()` riceve la firma DER, il ritorno è n.
  DIM sig(256) AS BYTE
  DIM nSig AS LONG = CR.SIGN("hello", keys(0), sig)

  ' Verifica con la chiave pubblica: ok sui dati originali, fail su quelli alterati.
  IF CR.VERIFY(sig, nSig, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
  IF CR.VERIFY(sig, nSig, "tampered", keys(1)) THEN PRINT "tamper: ok" ELSE PRINT "tamper: fail"

  ' Esporta la chiave privata in PEM.
  pem$ = CR.KEYEXPORT(keys(0))
  IF LEN(pem$) > 0 THEN PRINT "export: ok" ELSE PRINT "export: fail"

  ' Chiude gli handle (le risorse OpenSSL non sono garbage-collected).
  DIM nC1 AS LONG = CR.KEYCLOSE(keys(0))
  DIM nC2 AS LONG = CR.KEYCLOSE(keys(1))
END SUB
