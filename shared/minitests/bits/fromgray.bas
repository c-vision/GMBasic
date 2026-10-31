' MiniTest BITS.FROMGRAY — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: fromgray0= 5|fromgray1= 8|fromgray2= 0
IMPORT BITS

SUB Main()
  PRINT "fromgray0="; BITS.FROMGRAY(7)
  PRINT "fromgray1="; BITS.FROMGRAY(12)
  PRINT "fromgray2="; BITS.FROMGRAY(0)
END SUB
