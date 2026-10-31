' MiniTest BITS.BITDEINTERLEAVE — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitdeinterleave0= 3|bitdeinterleave1= 0
IMPORT BITS

SUB Main()
  PRINT "bitdeinterleave0="; BITS.BITDEINTERLEAVE(5, 0)
  PRINT "bitdeinterleave1="; BITS.BITDEINTERLEAVE(5, 1)
END SUB
