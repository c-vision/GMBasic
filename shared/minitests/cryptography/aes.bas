' MiniTest: CRYPTOGRAPHY.AESENCRYPT/AESDECRYPT — round-trip.
' EXPECT: [104, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  DIM enc(63) AS BYTE
  n = CRYPTOGRAPHY.AESENCRYPT("hello world", "0123456789abcdef", enc)
  DIM dec(31) AS BYTE
  k = CRYPTOGRAPHY.AESDECRYPT(enc, n, "0123456789abcdef", dec)
  DIM r(k - 1) AS BYTE
  FOR i = 0 TO k - 1
    r(i) = dec(i)
  NEXT i
  PRINT r
END SUB
