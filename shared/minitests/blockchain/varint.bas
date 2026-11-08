' EXPECT: e0=00|e1=01|e127=7f|d127= 127|rt= 127
' MiniTest BLOCKCHAIN (pacchetto FFI): VARINT LEB128 su valori single-byte 0/1/127.
' Encode scrive nel buffer BYTE e ritorna la lunghezza; decode legge da un array BYTE.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY

SUB Main()
  DIM o(9) AS BYTE
  DIM n AS LONG
  n = VARINTENCODE(0, o)
  PRINT "e0="; LEFT(TOHEX(o), n * 2)
  n = VARINTENCODE(1, o)
  PRINT "e1="; LEFT(TOHEX(o), n * 2)
  n = VARINTENCODE(127, o)
  PRINT "e127="; LEFT(TOHEX(o), n * 2)
  DIM b() AS BYTE = FROMHEX("7f")
  PRINT "d127="; VARINTDECODE(b)
  PRINT "rt="; VARINTDECODE(o)
END SUB
