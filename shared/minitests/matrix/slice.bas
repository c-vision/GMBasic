' EXPECT: slicenorm= 1.414214|slicerows= 2|slicecols= 2
IMPORT MATRIX
IMPORT MATH
SUB Main()
  DIM m = MATRIX.CREATE(3, 3)
  m = MATRIX.SETVALUE(m, 0, 0, 1) : m = MATRIX.SETVALUE(m, 1, 1, 1) : m = MATRIX.SETVALUE(m, 2, 2, 1)
  DIM s = MATRIX.SLICE(m, 0, 2, 0, 2)
  PRINT "slicenorm="; MATH.ROUND(MATRIX.FROBENIUSNORM(s), 6)
  PRINT "slicerows="; MATRIX.ROWCOUNT(s)
  PRINT "slicecols="; MATRIX.COLUMNCOUNT(s)
END SUB
