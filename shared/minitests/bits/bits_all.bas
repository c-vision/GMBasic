' MiniTest BITS — namespace manipolazione bit a 64 bit (tutti i 21 simboli).
' Oracolo calcolato A MANO (indici 0-based, predicati → -1/0, casi limite definiti).
' Verificato sui 4 backend: IL, PCodeVM, VM/C, C99Native.
' EXPECT: bc= 4|bc8= 8|bg1= 1|bg0= 0|bs= 8|bcl= 254|btg= 254|bt1=-1|bt0= 0|bf= 6|brev= 8|brl= 2|brr= 8|gray= 7|fromgray= 5|par1= 1|par0= 0|fwd3= 3|rev3= 3|fwdnone=-1|swap= 281474976710656|ham= 3|il_10= 1|il_01= 2|il_30= 5|dil0= 3|dil1= 0|np5= 8|np8= 8|ip8=-1|ip7= 0
IMPORT BITS

SUB Main()
  ' ── Tier 1 ────────────────────────────────────────────────────────────────
  PRINT "bc="; BITS.BITCOUNT(240)
  PRINT "bc8="; BITS.BITCOUNT(255)
  PRINT "bg1="; BITS.BITGET(240, 4)
  PRINT "bg0="; BITS.BITGET(240, 0)
  PRINT "bs="; BITS.BITSET(0, 3)
  PRINT "bcl="; BITS.BITCLEAR(255, 0)
  PRINT "btg="; BITS.BITTOGGLE(255, 0)
  PRINT "bt1="; BITS.BITTEST(8, 3)
  PRINT "bt0="; BITS.BITTEST(8, 2)

  ' ── Tier 2 ────────────────────────────────────────────────────────────────
  PRINT "bf="; BITS.BITFIELD(45, 1, 3)
  PRINT "brev="; BITS.BITREVERSE(1, 4)
  PRINT "brl="; BITS.BITROTATEL(1, 1, 4)
  PRINT "brr="; BITS.BITROTATER(1, 1, 4)
  PRINT "gray="; BITS.GRAYCODE(5)
  PRINT "fromgray="; BITS.FROMGRAY(7)
  PRINT "par1="; BITS.PARITY(7)
  PRINT "par0="; BITS.PARITY(3)
  PRINT "fwd3="; BITS.BITSCANFWD(8)
  PRINT "rev3="; BITS.BITSCANREV(8)
  PRINT "fwdnone="; BITS.BITSCANFWD(0)
  PRINT "swap="; BITS.BYTESWAP(256)

  ' ── Tier 3 ────────────────────────────────────────────────────────────────
  PRINT "ham="; BITS.HAMMINGDIST(0, 7)
  PRINT "il_10="; BITS.BITINTERLEAVE(1, 0)
  PRINT "il_01="; BITS.BITINTERLEAVE(0, 1)
  PRINT "il_30="; BITS.BITINTERLEAVE(3, 0)
  PRINT "dil0="; BITS.BITDEINTERLEAVE(5, 0)
  PRINT "dil1="; BITS.BITDEINTERLEAVE(5, 1)
  PRINT "np5="; BITS.NEXTPOW2(5)
  PRINT "np8="; BITS.NEXTPOW2(8)
  PRINT "ip8="; BITS.ISPOW2(8)
  PRINT "ip7="; BITS.ISPOW2(7)
END SUB
