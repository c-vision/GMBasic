' EXPECT: sigprefix=30|derlen>=68=-1
' MiniTest BLOCKCHAIN (pacchetto FFI): SECP256K1SIGN (DER, firma randomizzata: deterministico
' solo il prefisso 0x30 e la lunghezza >= 68). Contestualmente verifica il copy-back: il
' buffer BYTE di uscita e' scritto dalla lib e riletto qui.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB Main()
  DIM msg() AS BYTE = FROMHEX("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff")
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM sig(79) AS BYTE
  DIM n AS LONG
  n = SECP256K1SIGN(msg, priv, sig)
  PRINT "sigprefix="; LEFT(TOHEX(sig), 2)
  PRINT "derlen>=68="; (n >= 68)
END SUB
