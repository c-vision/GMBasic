' MiniTest BITS — casi limite DOCUMENTATI (n fuori [0,63], width clampato, lo/hi
' invertiti, argomenti a zero). Verifica che la semantica sia DEFINITA e identica
' sui 4 backend (nessun risultato silenzioso).
' EXPECT: bg64= 0|bs64= 0|bclneg= 255|bt64= 0|bfswap= 7|bfclamp= 7|brev0= 0|brl0= 1|brlwrap= 2|par0= 0|swap0= 0|np0= 1|np1= 1|ip0= 0|ip1=-1|ham0= 0|dil0= 0
IMPORT BITS

SUB Main()
  PRINT "bg64="; BITS.BITGET(255, 64)
  PRINT "bs64="; BITS.BITSET(0, 64)
  PRINT "bclneg="; BITS.BITCLEAR(255, -1)
  PRINT "bt64="; BITS.BITTEST(255, 64)
  PRINT "bfswap="; BITS.BITFIELD(255, 3, 1)
  PRINT "bfclamp="; BITS.BITFIELD(255, -5, 2)
  PRINT "brev0="; BITS.BITREVERSE(3, 0)
  PRINT "brl0="; BITS.BITROTATEL(1, 0, 4)
  PRINT "brlwrap="; BITS.BITROTATEL(1, 5, 4)
  PRINT "par0="; BITS.PARITY(0)
  PRINT "swap0="; BITS.BYTESWAP(0)
  PRINT "np0="; BITS.NEXTPOW2(0)
  PRINT "np1="; BITS.NEXTPOW2(1)
  PRINT "ip0="; BITS.ISPOW2(0)
  PRINT "ip1="; BITS.ISPOW2(1)
  PRINT "ham0="; BITS.HAMMINGDIST(255, 255)
  PRINT "dil0="; BITS.BITDEINTERLEAVE(0, 0)
END SUB
