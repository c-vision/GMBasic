' MiniTest BITS.BITTOGGLE — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: bittoggle0= 254|bittoggle1= 8|bittoggle2= 5
IMPORT BITS

SUB Main()
  PRINT "bittoggle0="; BITS.BITTOGGLE(255, 0)
  PRINT "bittoggle1="; BITS.BITTOGGLE(0, 3)
  PRINT "bittoggle2="; BITS.BITTOGGLE(5, 64)
END SUB
