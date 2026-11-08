' MiniTest: CRYPTOGRAPHY.PASSWORDHASH/PASSWORDVERIFY (parita' 4 backend).
' PASSWORDHASH e' random (salt), quindi si verifica il FATTO del verify.
' EXPECT: verify: ok|wrong: fail
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  h$ = CRYPTOGRAPHY.PASSWORDHASH("secret")
  IF CRYPTOGRAPHY.PASSWORDVERIFY("secret", h$) THEN PRINT "verify: ok" ELSE PRINT "verify: fail"
  IF CRYPTOGRAPHY.PASSWORDVERIFY("wrong", h$) THEN PRINT "wrong: ok" ELSE PRINT "wrong: fail"
END SUB
