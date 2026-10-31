' EXPECT: rlpA=41|rlpDog=83646f67|rlpEmpty=80|sel=0x70a08231
' MiniTest BLOCKCHAIN: RLPENCODE (byte singolo <0x80 invariato, stringa corta, stringa vuota) + selettore ABIENCODE senza argomenti.
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
SUB Main()
  PRINT "rlpA="; TOHEX(RLPENCODE("A"))
  PRINT "rlpDog="; TOHEX(RLPENCODE("dog"))
  PRINT "rlpEmpty="; TOHEX(RLPENCODE(""))
  PRINT "sel="; ABIENCODE("balanceOf(address)")
END SUB
