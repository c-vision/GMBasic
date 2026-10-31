' MiniTest: array di UDT a livello MODULO (`DIM arr(N) AS Tipo`, DIM PRIMA del TYPE)
' con accesso al campo dell'elemento (`arr(i).campo`). Copre il bug IL
' "accesso a un array non DIMensionato — DIM deve precedere l'uso" (il fix scrive/legge
' l'array globale nel bag tra istanze del compilatore).
' EXPECT: t= 123|p= 1.5
DIM sb(3) AS Rec
TYPE Rec
  t AS LONG
  p AS DOUBLE
END TYPE
SUB Main()
  sb(0).t = 123
  sb(0).p = 1.5
  PRINT "t="; sb(0).t
  PRINT "p="; sb(0).p
END SUB
