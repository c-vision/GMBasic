' EXPECT: eth=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|cks=0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf
' MiniTest BLOCKCHAIN (pacchetto FFI): ETHADDRFROMPUB su chiave G non compressa (65 byte,
' inizio 0x04) + ETHADDRCHECKSUM (EIP-55). L'indirizzo si ottiene hashande i byte ricevuti.
REQUIRE "blockchain"
REQUIRE "crypto"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM pub(64) AS BYTE
  DIM _n_pub AS LONG
  _n_pub = FROMHEX("0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", pub)
  DIM a$ AS STRING
  a$ = ETHADDRFROMPUB(pub)
  PRINT "eth="; a$
  PRINT "cks="; ETHADDRCHECKSUM(a$)
END SUB
