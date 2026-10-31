' MiniTest BITS.BITROTATEL — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitrotatel0= 2|bitrotatel1= 1|bitrotatel2= 5
IMPORT BITS

SUB Main()
  PRINT "bitrotatel0="; BITS.BITROTATEL(1, 1, 4)
  PRINT "bitrotatel1="; BITS.BITROTATEL(8, 1, 4)
  PRINT "bitrotatel2="; BITS.BITROTATEL(5, 0, 4)
END SUB
