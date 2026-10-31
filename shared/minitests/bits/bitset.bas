' MiniTest BITS.BITSET — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitset0= 8|bitset1= 8|bitset2= 5
IMPORT BITS

SUB Main()
  PRINT "bitset0="; BITS.BITSET(0, 3)
  PRINT "bitset1="; BITS.BITSET(8, 3)
  PRINT "bitset2="; BITS.BITSET(5, 64)
END SUB
