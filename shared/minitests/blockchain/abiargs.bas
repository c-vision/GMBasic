' EXPECT: enc=0x70a082310000000000000000000000000000000000000000000000000000000000000001|dec= 1
' MiniTest BLOCKCHAIN: ABIENCODE con argomento uint256 + ABIDECODE della parola singola — oracolo IL/PCode (la VM/C codifica parole da 16 byte e salta il selettore in decode: gap noto).
IMPORT BLOCKCHAIN
SUB Main()
  PRINT "enc="; ABIENCODE("balanceOf(address)", 1)
  DIM w$ = "0x0000000000000000000000000000000000000000000000000000000000000001"
  DIM r = ABIDECODE(w$, {"uint256"})
  PRINT "dec="; r(0)
END SUB
