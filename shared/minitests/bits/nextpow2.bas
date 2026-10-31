' MiniTest BITS.NEXTPOW2 — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: nextpow20= 8|nextpow21= 8|nextpow22= 1|nextpow23= 1024
IMPORT BITS

SUB Main()
  PRINT "nextpow20="; BITS.NEXTPOW2(5)
  PRINT "nextpow21="; BITS.NEXTPOW2(8)
  PRINT "nextpow22="; BITS.NEXTPOW2(0)
  PRINT "nextpow23="; BITS.NEXTPOW2(1000)
END SUB
