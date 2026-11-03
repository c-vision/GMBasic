' MiniTest NAMESPACE AUTH — tutte le 3 funzioni della superficie.
' EXPECT: verify=-1|bad= 0|payload=-1
IMPORT AUTH
SUB Main()
  DIM t AS STRING
  t = AUTH.JWTCREATE("{""sub"":""x""}", "secret")
  PRINT "verify="; AUTH.JWTVERIFY(t, "secret")
  PRINT "bad="; AUTH.JWTVERIFY(t, "wrong")
  PRINT "payload="; (INSTR(AUTH.JWTPAYLOAD(t), "sub") > 0)
END SUB
