' EXPECT: m01= 5|m11= 1|v= 1|rows= 3|cols= 3|tr= 5
' MATRIX a VALORE (F1): CREATE restituisce MATRIX OF, SETVALUE restituisce il valore
' mutato, GETVALUE/ROWCOUNT/COLUMNCOUNT/TRANSPOSE accettano il valore.
SUB Main()
  DIM m = MATRIX.CREATE(3, 3)
  m = MATRIX.SETVALUE(m, 0, 1, 5)
  PRINT "m01="; MATRIX.GETVALUE(m, 0, 1)
  PRINT "m11="; MATRIX.GETVALUE(m, 1, 1)
  PRINT "v="; m(1, 1)
  PRINT "rows="; MATRIX.ROWCOUNT(m)
  PRINT "cols="; MATRIX.COLUMNCOUNT(m)
  DIM t = MATRIX.TRANSPOSE(m)
  PRINT "tr="; MATRIX.GETVALUE(t, 1, 0)
END SUB
