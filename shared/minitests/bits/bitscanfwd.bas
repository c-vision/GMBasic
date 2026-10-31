' MiniTest BITS.BITSCANFWD — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitscanfwd0= 3|bitscanfwd1=-1|bitscanfwd2= 0
IMPORT BITS

SUB Main()
  PRINT "bitscanfwd0="; BITS.BITSCANFWD(8)
  PRINT "bitscanfwd1="; BITS.BITSCANFWD(0)
  PRINT "bitscanfwd2="; BITS.BITSCANFWD(1)
END SUB
