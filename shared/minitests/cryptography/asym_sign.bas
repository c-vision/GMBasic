' MiniTest: CRYPTOGRAPHY.KEYGEN/SIGN/VERIFY/KEYEXPORT (asimmetrico).
' EXPECT: verify: ok|tamper: fail|export: ok
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM keys(1) AS LONG
  CALL CRYPTOGRAPHY.KEYGEN("ECDSA", 256, keys)
  DIM sig(511) AS BYTE
  n = CRYPTOGRAPHY.SIGN("hello", keys(0), sig)
  IF CRYPTOGRAPHY.VERIFY(sig, n, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
  IF CRYPTOGRAPHY.VERIFY(sig, n, "tampered", keys(1)) THEN PRINT "tamper: ok" ELSE PRINT "tamper: fail"
  pem$ = CRYPTOGRAPHY.KEYEXPORT(keys(0))
  IF LEN(pem$) > 0 THEN PRINT "export: ok" ELSE PRINT "export: fail"
END SUB
