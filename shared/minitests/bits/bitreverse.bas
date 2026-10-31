' MiniTest BITS.BITREVERSE — oracolo calcolato a mano (width <= 0 → 0; width > 64 → clamp 64).
' NB: valori >= 1e16 hanno formati di stampa divergenti fra IL e i backend C (reperto PRINT,
' non BITS) → si usano width <= 48.
' EXPECT: bitreverse0= 8|bitreverse1= 13|bitreverse2= 0|bitreverse3= 140737488355328|bitreverse4= 0
IMPORT BITS

SUB Main()
  PRINT "bitreverse0="; BITS.BITREVERSE(1, 4)
  PRINT "bitreverse1="; BITS.BITREVERSE(11, 4)
  PRINT "bitreverse2="; BITS.BITREVERSE(1, 0)
  PRINT "bitreverse3="; BITS.BITREVERSE(1, 48)
  PRINT "bitreverse4="; BITS.BITREVERSE(0, 64)
END SUB
