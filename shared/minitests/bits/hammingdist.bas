' MiniTest BITS.HAMMINGDIST — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: hammingdist0= 3|hammingdist1= 0|hammingdist2= 8
IMPORT BITS

SUB Main()
  PRINT "hammingdist0="; BITS.HAMMINGDIST(0, 7)
  PRINT "hammingdist1="; BITS.HAMMINGDIST(5, 5)
  PRINT "hammingdist2="; BITS.HAMMINGDIST(0, 255)
END SUB
