' MiniTest: KEYCLOSE handle (statement senza namespace) — chiude l'handle di chiave crypto.
' Verifica i valori: solo completamento + stampa (il registro chiavi e' interno).
' EXPECT: closed
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM keys = CRYPTOGRAPHY.KEYGEN("ECDSA", 256)
  pem$ = CRYPTOGRAPHY.KEYEXPORT(keys(0))
  imported = CRYPTOGRAPHY.KEYIMPORT(pem$, 0)
  KEYCLOSE imported
  PRINT "closed"
END SUB
