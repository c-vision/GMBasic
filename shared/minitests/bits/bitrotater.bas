' MiniTest BITS.BITROTATER — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitrotater0= 8|bitrotater1= 1|bitrotater2= 5
IMPORT BITS

SUB Main()
  PRINT "bitrotater0="; BITS.BITROTATER(1, 1, 4)
  PRINT "bitrotater1="; BITS.BITROTATER(2, 1, 4)
  PRINT "bitrotater2="; BITS.BITROTATER(5, 0, 4)
END SUB
