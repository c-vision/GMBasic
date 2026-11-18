' EXPECT: n= 65|full=0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8|eth=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|comp=0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798|prefix04=SI|xmatch=SI
' MiniTest BLOCKCHAIN (pacchetto FFI): GETFULLPUBLICKEY = chiave pubblica NON compressa
' (65 byte: 0x04 || X || Y) della privata, nel buffer del chiamante. E' la forma che vuole
' ETHADDRFROMPUB, che con la compressa di GETPUBLICKEY produce un ALTRO indirizzo.
' Vettore noto: priv = 1 (generatore di secp256k1).
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

SUB MostraFull(priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = GETFULLPUBLICKEY(priv, o)
  PRINT "n="; n
  PRINT "full="; BYTESHEX(o)
  PRINT "eth="; ETHADDRFROMPUB(o)
END SUB

SUB Compressa(priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = GETPUBLICKEY(priv, o)
  PRINT "comp="; LEFT(BYTESHEX(o), n * 2)
END SUB

SUB Main()
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM full(64) AS BYTE
  MostraFull(priv, full)
  DIM comp(32) AS BYTE
  Compressa(priv, comp)
  IF LEFT(BYTESHEX(full), 2) = "04" THEN
    PRINT "prefix04=SI"
  ELSE
    PRINT "prefix04=NO"
  END IF
  IF MID(BYTESHEX(full), 3, 64) = MID(BYTESHEX(comp), 3, 64) THEN
    PRINT "xmatch=SI"
  ELSE
    PRINT "xmatch=NO"
  END IF
END SUB
