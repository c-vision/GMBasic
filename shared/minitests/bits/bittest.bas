' MiniTest BITS.BITTEST — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bittest0=-1|bittest1= 0|bittest2= 0
IMPORT BITS

SUB Main()
  PRINT "bittest0="; BITS.BITTEST(8, 3)
  PRINT "bittest1="; BITS.BITTEST(8, 2)
  PRINT "bittest2="; BITS.BITTEST(8, 64)
END SUB
