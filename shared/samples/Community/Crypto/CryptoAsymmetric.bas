' ============================================================================
' CRYPTOGRAPHY — crittografia asimmetrica: KEYGEN, SIGN, VERIFY, KEYEXPORT.
' Esempio per il namespace CRYPTOGRAPHY (chiavi ECDSA via OpenSSL).
' ============================================================================
IMPORT CRYPTOGRAPHY AS CR

SUB Main()
  ' Genera una coppia di chiavi: keys(0) = handle privato, keys(1) = handle pubblico.
  DIM keys = CR.KEYGEN("ECDSA", 256)

  ' Firma i dati con la chiave privata.
  DIM sig = CR.SIGN("hello", keys(0))

  ' Verifica con la chiave pubblica: ok sui dati originali, fail su quelli alterati.
  IF CR.VERIFY(sig, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
  IF CR.VERIFY(sig, "tampered", keys(1)) THEN PRINT "tamper: ok" ELSE PRINT "tamper: fail"

  ' Esporta la chiave privata in PEM.
  pem$ = CR.KEYEXPORT(keys(0))
  IF LEN(pem$) > 0 THEN PRINT "export: ok" ELSE PRINT "export: fail"
END SUB
