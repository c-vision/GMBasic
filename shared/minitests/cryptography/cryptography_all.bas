' MiniTest: CRYPTOGRAPHY end-to-end su tutta la superficie del pacchetto.
' EXPECT: hash: ok|hmac: ok|enc: ok|pw: ok|aes: ok|chacha: ok|asym: ok|rand: ok
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
SUB Main()
  IF LEN(CRYPTOGRAPHY.SHA256("hello")) = 64 THEN PRINT "hash: ok" ELSE PRINT "hash: fail"
  IF LEN(CRYPTOGRAPHY.HMACSHA256("data", "key")) = 64 THEN PRINT "hmac: ok" ELSE PRINT "hmac: fail"
  DIM b(4) AS BYTE
  IF CRYPTOGRAPHY.FROMHEX("4142434445", b) = 5 THEN PRINT "enc: ok" ELSE PRINT "enc: fail"
  h$ = CRYPTOGRAPHY.PASSWORDHASH("secret")
  IF CRYPTOGRAPHY.PASSWORDVERIFY("secret", h$) THEN PRINT "pw: ok" ELSE PRINT "pw: fail"
  DIM a(63) AS BYTE
  na = CRYPTOGRAPHY.AESENCRYPT("hello", "key", a)
  DIM pa(63) AS BYTE
  IF CRYPTOGRAPHY.AESDECRYPT(a, na, "key", pa) = 5 THEN PRINT "aes: ok" ELSE PRINT "aes: fail"
  DIM k(31) AS BYTE
  CRYPTOGRAPHY.FROMHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", k)
  DIM nn(11) AS BYTE
  CRYPTOGRAPHY.FROMHEX("000000000000004a00000000", nn)
  DIM c(63) AS BYTE
  IF CRYPTOGRAPHY.CHACHA20ENCRYPT("hello", k, nn, c) = 5 THEN PRINT "chacha: ok" ELSE PRINT "chacha: fail"
  DIM keys(1) AS LONG
  CALL CRYPTOGRAPHY.KEYGEN("ECDSA", 256, keys)
  DIM sig(511) AS BYTE
  ns = CRYPTOGRAPHY.SIGN("hello", keys(0), sig)
  IF CRYPTOGRAPHY.VERIFY(sig, ns, "hello", keys(1)) THEN PRINT "asym: ok" ELSE PRINT "asym: fail"
  DIM rb(7) AS BYTE
  IF CRYPTOGRAPHY.RANDOMCRYPTOBYTES(8, rb) = 8 THEN PRINT "rand: ok" ELSE PRINT "rand: fail"
END SUB
