' EXPECT: k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|b=324dcf027dd4a30a932c441f365a25e86b173defa4b8e58948253471b81b72cf|r=108f07b8382412612c048d07d13f814118445acd
' MiniTest BLOCKCHAIN (pacchetto FFI): versioni a BYTE di KECCAK256 / BLAKE2B256 / RIPEMD160
' su "hello" — l'output lo scrive la lib nel buffer passato dal chiamante (copy-back).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM d() AS BYTE = FROMHEX("68656c6c6f")
  DIM k(31) AS BYTE
  KECCAK256(d, k)
  PRINT "k="; TOHEX(k)
  DIM b(31) AS BYTE
  BLAKE2B256(d, b)
  PRINT "b="; TOHEX(b)
  DIM r(19) AS BYTE
  RIPEMD160(d, r)
  PRINT "r="; TOHEX(r)
END SUB
