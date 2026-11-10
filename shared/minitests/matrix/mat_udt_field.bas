' EXPECT: el= 2 , 3|rows= 2|cols= 2|gv= 4|tr= 2|in= 2 , 2
' Campo MATRICE di un TYPE (UDT): `DIM m(N,M) AS DOUBLE` dentro il tipo. Elemento per
' indice, metadati MATRIX.*, GETVALUE, TRANSPOSE (view) e passaggio a un parametro
' `MATRIX OF`. Il campo è un buffer flat del tipo (niente handle): il descrittore lo
' espone come view non proprietaria (C99) / MatrixValue (IL, PCode, VM/C).
TYPE Grid
  DIM m(1, 1) AS DOUBLE
END TYPE

SUB Show(a AS MATRIX OF (INT, INT, DOUBLE))
  PRINT "in="; MATRIX.ROWCOUNT(a); ","; MATRIX.COLUMNCOUNT(a)
END SUB

SUB Main()
  DIM t AS Grid
  t.m(0, 0) = 1 : t.m(0, 1) = 2
  t.m(1, 0) = 3 : t.m(1, 1) = 4
  PRINT "el="; t.m(0, 1); ","; t.m(1, 0)
  PRINT "rows="; MATRIX.ROWCOUNT(t.m)
  PRINT "cols="; MATRIX.COLUMNCOUNT(t.m)
  PRINT "gv="; MATRIX.GETVALUE(t.m, 1, 1)
  DIM s = MATRIX.TRANSPOSE(t.m)
  PRINT "tr="; s(1, 0)
  CALL Show(t.m)
END SUB
