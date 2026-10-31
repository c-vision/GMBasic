' MiniTest BITS.BITINTERLEAVE — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitinterleave0= 1|bitinterleave1= 2|bitinterleave2= 5|bitinterleave3= 39
IMPORT BITS

SUB Main()
  PRINT "bitinterleave0="; BITS.BITINTERLEAVE(1, 0)
  PRINT "bitinterleave1="; BITS.BITINTERLEAVE(0, 1)
  PRINT "bitinterleave2="; BITS.BITINTERLEAVE(3, 0)
  PRINT "bitinterleave3="; BITS.BITINTERLEAVE(3, 5)
END SUB
