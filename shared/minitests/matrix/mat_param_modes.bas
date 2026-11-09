' EXPECT: cell= 2|sum= 10|ones= 1
' ADR-MATRIX §3 (F3): IN (default) = sola lettura; INOUT/OUT consentono la scrittura.
' Il passaggio resta per descrittore condiviso: le scritture dentro la procedura sono
' visibili al chiamante.
SUB SetCell(INOUT m AS MATRIX OF (INT, INT, DOUBLE), r AS LONG, c AS LONG, v AS DOUBLE)
  m(r, c) = v
END SUB

SUB FillOnes(OUT m AS MATRIX OF (INT, INT, DOUBLE))
  DIM i AS LONG
  DIM j AS LONG
  FOR i = 0 TO MATRIX.ROWCOUNT(m) - 1
    FOR j = 0 TO MATRIX.COLUMNCOUNT(m) - 1
      m(i, j) = 1
    NEXT j
  NEXT i
END SUB

FUNCTION SumAll(IN m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM i AS LONG
  DIM j AS LONG
  DIM s AS DOUBLE
  s = 0
  FOR i = 0 TO MATRIX.ROWCOUNT(m) - 1
    FOR j = 0 TO MATRIX.COLUMNCOUNT(m) - 1
      s = s + m(i, j)
    NEXT j
  NEXT i
  SumAll = s
END FUNCTION

SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  CALL SetCell(m, 0, 0, 1)
  CALL SetCell(m, 0, 1, 2)
  CALL SetCell(m, 1, 0, 3)
  CALL SetCell(m, 1, 1, 4)
  PRINT "cell="; MATRIX.GETVALUE(m, 0, 1)
  PRINT "sum="; SumAll(m)
  CALL FillOnes(m)
  PRINT "ones="; MATRIX.GETVALUE(m, 1, 1)
END SUB
