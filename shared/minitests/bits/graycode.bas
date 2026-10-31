' MiniTest BITS.GRAYCODE — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: graycode0= 7|graycode1= 0|graycode2= 12
IMPORT BITS

SUB Main()
  PRINT "graycode0="; BITS.GRAYCODE(5)
  PRINT "graycode1="; BITS.GRAYCODE(0)
  PRINT "graycode2="; BITS.GRAYCODE(8)
END SUB
