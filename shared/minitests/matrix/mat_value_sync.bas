' EXPECT: g1= 7|g2= 9|e= 4|v= 4
' MATRIX a valore: scritture e letture restano coerenti tra SETVALUE, accesso per
' indice e accesso elemento (descrittore <-> lista flat).
SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  m = MATRIX.SETVALUE(m, 0, 1, 7)
  PRINT "g1="; MATRIX.GETVALUE(m, 0, 1)
  m(1, 0) = 9
  PRINT "g2="; MATRIX.GETVALUE(m, 1, 0)
  DIM e = 4
  m(1, 1) = e
  PRINT "e="; m(1, 1)
  DIM v = MATRIX.GETVALUE(m, 1, 1)
  PRINT "v="; v
END SUB
