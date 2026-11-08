' MiniTest NAMESPACE AUTH — tutte le 3 funzioni della superficie.
' JWTCREATE deterministico (claims/secret fissi) e JWTVERIFY/JWTPAYLOAD sulle sue uscite.
' EXPECT: verify=-1|bad= 0|payload=-1|header=-1
IMPORT AUTH
SUB Main()
  DIM q AS STRING
  q = CHR$(34)
  DIM claims AS STRING
  claims = "{" + q + "sub" + q + ":" + q + "x" + q + "}"
  DIM t AS STRING
  t = AUTH.JWTCREATE(claims, "secret")
  PRINT "verify="; AUTH.JWTVERIFY(t, "secret")
  PRINT "bad="; AUTH.JWTVERIFY(t, "wrong")
  PRINT "payload="; (AUTH.JWTPAYLOAD(t) = claims)
  PRINT "header="; (LEFT$(t, 10) = "eyJhbGciOi")
END SUB
