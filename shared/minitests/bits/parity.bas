' MiniTest BITS.PARITY — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: parity0= 1|parity1= 0|parity2= 0
IMPORT BITS

SUB Main()
  PRINT "parity0="; BITS.PARITY(7)
  PRINT "parity1="; BITS.PARITY(3)
  PRINT "parity2="; BITS.PARITY(0)
END SUB
