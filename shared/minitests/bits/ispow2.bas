' MiniTest BITS.ISPOW2 — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: ispow20=-1|ispow21= 0|ispow22=-1|ispow23= 0
IMPORT BITS

SUB Main()
  PRINT "ispow20="; BITS.ISPOW2(8)
  PRINT "ispow21="; BITS.ISPOW2(7)
  PRINT "ispow22="; BITS.ISPOW2(1)
  PRINT "ispow23="; BITS.ISPOW2(0)
END SUB
