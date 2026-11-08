' MiniTest: CRYPTOGRAPHY.KEYIMPORT.
' EXPECT: verify: ok
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM keys(1) AS LONG
  CALL CRYPTOGRAPHY.KEYGEN("ECDSA", 256, keys)
  pem$ = CRYPTOGRAPHY.KEYEXPORT(keys(0))
  imported = CRYPTOGRAPHY.KEYIMPORT(pem$)
  DIM sig(511) AS BYTE
  n = CRYPTOGRAPHY.SIGN("hello", imported, sig)
  IF CRYPTOGRAPHY.VERIFY(sig, n, "hello", keys(1)) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
END SUB
