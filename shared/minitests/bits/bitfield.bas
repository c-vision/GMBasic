' MiniTest BITS.BITFIELD — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitfield0= 6|bitfield1= 6|bitfield2= 18|bitfield3= 5
IMPORT BITS

SUB Main()
  PRINT "bitfield0="; BITS.BITFIELD(45, 1, 3)
  PRINT "bitfield1="; BITS.BITFIELD(45, 3, 1)
  PRINT "bitfield2="; BITS.BITFIELD(4660, 8, 15)
  PRINT "bitfield3="; BITS.BITFIELD(45, 8, 3)
END SUB
