' MiniTest BITS.BITCOUNT — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitcount0= 4|bitcount1= 8|bitcount2= 0|bitcount3= 64
IMPORT BITS

SUB Main()
  PRINT "bitcount0="; BITS.BITCOUNT(240)
  PRINT "bitcount1="; BITS.BITCOUNT(255)
  PRINT "bitcount2="; BITS.BITCOUNT(0)
  PRINT "bitcount3="; BITS.BITCOUNT(-1)
END SUB
