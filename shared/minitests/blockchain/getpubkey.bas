' EXPECT: publen= 33|pub=0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
' MiniTest BLOCKCHAIN (pacchetto FFI): GETPUBLICKEY(priv = 1) = G compresso (33 byte).
' La chiave privata e' un array BYTE: priv(31)=1 -> big-endian 0x...01.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM pub(63) AS BYTE
  DIM n AS LONG
  n = GETPUBLICKEY(priv, pub)
  PRINT "publen="; n
  PRINT "pub="; LEFT(BYTESHEX(pub), n * 2)
END SUB
