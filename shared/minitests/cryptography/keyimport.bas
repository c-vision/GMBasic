' MiniTest: CRYPTOGRAPHY.KEYIMPORT (parita' 4 backend).
' EXPECT: verify: ok
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM keys = CRYPTOGRAPHY.KEYGEN("ECDSA", 256)
  pem$ = CRYPTOGRAPHY.KEYEXPORT(keys(0))
  imported = CRYPTOGRAPHY.KEYIMPORT(pem$, 0)
  DIM sig = CRYPTOGRAPHY.SIGN("hello", imported)
  IF CRYPTOGRAPHY.VERIFY(sig, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
END SUB
