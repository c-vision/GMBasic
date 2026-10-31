' MiniTest: CRYPTOGRAPHY.KEYGEN/SIGN/VERIFY/KEYEXPORT (asimmetrico, parita' 4 backend).
' EXPECT: verify: ok|tamper: fail|export: ok
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM keys = CRYPTOGRAPHY.KEYGEN("ECDSA", 256)
  DIM sig = CRYPTOGRAPHY.SIGN("hello", keys(0))
  IF CRYPTOGRAPHY.VERIFY(sig, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
  IF CRYPTOGRAPHY.VERIFY(sig, "tampered", keys(1)) THEN PRINT "tamper: ok" ELSE PRINT "tamper: fail"
  pem$ = CRYPTOGRAPHY.KEYEXPORT(keys(0))
  IF LEN(pem$) > 0 THEN PRINT "export: ok" ELSE PRINT "export: fail"
END SUB
