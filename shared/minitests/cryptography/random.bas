' MiniTest: CRYPTOGRAPHY.RANDOM* — solo FATTI (i valori sono casuali).
' EXPECT: int: ok|double: ok|normal: ok|bytes: ok|crypto: ok
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  i = CRYPTOGRAPHY.RANDOMINT(1, 10)
  IF i >= 1 AND i <= 10 THEN PRINT "int: ok" ELSE PRINT "int: fail"
  d = CRYPTOGRAPHY.RANDOMDOUBLE(0, 1)
  IF d >= 0 AND d <= 1 THEN PRINT "double: ok" ELSE PRINT "double: fail"
  n = CRYPTOGRAPHY.RANDOMNORMAL(0, 1)
  IF ABS(n) < 100 THEN PRINT "normal: ok" ELSE PRINT "normal: fail"
  DIM b(7) AS BYTE
  k = CRYPTOGRAPHY.RANDOMCRYPTOBYTES(8, b)
  IF k = 8 THEN PRINT "bytes: ok" ELSE PRINT "bytes: fail"
  IF LEN(CRYPTOGRAPHY.TOHEX("abcdefgh")) = 16 THEN PRINT "crypto: ok" ELSE PRINT "crypto: fail"
END SUB
