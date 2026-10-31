' MiniTest BITS.BYTESWAP — oracolo calcolato a mano (indici 0-based, predicati -1/0).
' EXPECT: byteswap0= 281474976710656|byteswap1= 0|byteswap2= 1099511627776
IMPORT BITS

SUB Main()
  PRINT "byteswap0="; BITS.BYTESWAP(256)
  PRINT "byteswap1="; BITS.BYTESWAP(0)
  PRINT "byteswap2="; BITS.BYTESWAP(65536)
END SUB
