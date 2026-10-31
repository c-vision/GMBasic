' MiniTest BITS.BITGET — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitget0= 1|bitget1= 0|bitget2= 0|bitget3= 0
IMPORT BITS

SUB Main()
  PRINT "bitget0="; BITS.BITGET(240, 4)
  PRINT "bitget1="; BITS.BITGET(240, 0)
  PRINT "bitget2="; BITS.BITGET(8, 64)
  PRINT "bitget3="; BITS.BITGET(8, -1)
END SUB
