' MiniTest BITS.BITSCANREV — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bitscanrev0= 3|bitscanrev1=-1|bitscanrev2= 2
IMPORT BITS

SUB Main()
  PRINT "bitscanrev0="; BITS.BITSCANREV(8)
  PRINT "bitscanrev1="; BITS.BITSCANREV(0)
  PRINT "bitscanrev2="; BITS.BITSCANREV(5)
END SUB
