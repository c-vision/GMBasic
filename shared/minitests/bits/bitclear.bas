' MiniTest BITS.BITCLEAR — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitclear0= 254|bitclear1= 0|bitclear2= 5
IMPORT BITS

SUB Main()
  PRINT "bitclear0="; BITS.BITCLEAR(255, 0)
  PRINT "bitclear1="; BITS.BITCLEAR(8, 3)
  PRINT "bitclear2="; BITS.BITCLEAR(5, 64)
END SUB
