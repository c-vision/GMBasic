' EXPECT: el= 2 , 3|rows= 2|cols= 2|gv= 4|tr= 2|copy= 3|field= 9|ub= 1 , 1|len= 4|in= 2 , 2
' Campo MATRICE di un TYPE (UDT): `DIM m(N,M) AS DOUBLE` dentro il tipo. Elemento per
' indice, metadati MATRIX.*, GETVALUE, TRANSPOSE (view), UBOUND/LEN, copia di valore
' CONDIVISA (`DIM x = t.m`: stessa semantica di riferimento di PCode/VM-C/C99) e
' passaggio a un parametro `MATRIX OF`. Il campo è il valore matrice del tipo (no handle).
IMPORT SYSTEM

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
  DIM x = t.m
  PRINT "copy="; x(1, 0)
  x(1, 0) = 9
  PRINT "field="; t.m(1, 0)
  PRINT "ub="; UBOUND(t.m, 1); ","; UBOUND(t.m, 2)
  PRINT "len="; LEN(t.m)
  CALL Show(t.m)
END SUB
