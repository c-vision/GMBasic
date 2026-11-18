' EXPECT: k=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8|b=324dcf027dd4a30a932c441f365a25e86b173defa4b8e58948253471b81b72cf|r=108f07b8382412612c048d07d13f814118445acd|kn=1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8
' MiniTest BLOCKCHAIN (pacchetto FFI): KECCAK256 / BLAKE2B256 / RIPEMD160 con input E output
' passati come PARAMETRI `x() AS BYTE` di SUB — non DIM locali del chiamante. Il wrapper
' (KECCAK256 -> BCKECCAK256 con n = LEN(data) e cap = LEN(out)) riceve quindi lunghezze lette
' da array di parametro e scrive nel buffer del chiamante (by-reference). `kn` e' il percorso
' parametro -> parametro: DoK riceve gli array che le sono stati inoltrati da HashKNested.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB DoK(a() AS BYTE, o() AS BYTE)
  KECCAK256(a, o)
END SUB

SUB HashK(a() AS BYTE, o() AS BYTE)
  DoK(a, o)
  PRINT "k="; BYTESHEX(o)
END SUB

SUB HashKNested(a() AS BYTE, o() AS BYTE)
  DoK(a, o)
  PRINT "kn="; BYTESHEX(o)
END SUB

SUB HashB(a() AS BYTE, o() AS BYTE)
  BLAKE2B256(a, o)
  PRINT "b="; BYTESHEX(o)
END SUB

SUB HashR(a() AS BYTE, o() AS BYTE)
  RIPEMD160(a, o)
  PRINT "r="; BYTESHEX(o)
END SUB

SUB Main()
  DIM d(4) AS BYTE
  DIM _n_d AS LONG
  _n_d = FROMHEX("68656c6c6f", d)
  DIM k(31) AS BYTE
  DIM b(31) AS BYTE
  DIM r(19) AS BYTE
  DIM kn(31) AS BYTE
  HashK(d, k)
  HashB(d, b)
  HashR(d, r)
  HashKNested(d, kn)
END SUB
