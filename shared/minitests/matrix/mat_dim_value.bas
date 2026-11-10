' EXPECT: v= 12|ub= 1 , 2|lb= 0 , 0|len= 6|rows= 2 , 3|contig=-1|sum= 36
' Stage D (F4): un DIM rank-2 DOUBLE è il VALORE matrice (descrittore MatrixValue su IL):
' lettura/scrittura per indice, UBOUND/LBOUND, LEN, metadati MATRIX.* e passaggio a un
' parametro `MATRIX OF` (descrittore condiviso).
FUNCTION SumAll(m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM i AS LONG
  DIM j AS LONG
  DIM s AS DOUBLE
  FOR i = 0 TO MATRIX.ROWCOUNT(m) - 1
    FOR j = 0 TO MATRIX.COLUMNCOUNT(m) - 1
      s = s + m(i, j)
    NEXT j
  NEXT i
  SumAll = s
END FUNCTION

SUB Main()
  DIM m(1, 2) AS DOUBLE
  DIM i AS LONG
  DIM j AS LONG
  FOR i = 0 TO 1
    FOR j = 0 TO 2
      m(i, j) = i * 10 + j
    NEXT j
  NEXT i
  PRINT "v="; m(1, 2)
  PRINT "ub="; UBOUND(m, 1); ","; UBOUND(m, 2)
  PRINT "lb="; LBOUND(m, 1); ","; LBOUND(m, 2)
  PRINT "len="; LEN(m)
  PRINT "rows="; MATRIX.ROWCOUNT(m); ","; MATRIX.COLUMNCOUNT(m)
  PRINT "contig="; MATRIX.ISCONTIGUOUS(m)
  PRINT "sum="; SumAll(m)
END SUB
