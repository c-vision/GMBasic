' FILE: Arrays/ArraysMiniTest.bas
' Dimostra ARRAYS functions coperte dai MiniTest: AADD, ACHUNK, ACOPY, ADEL,
' AFILL, AFLAT, AINS, ASHUFFLE, AZIP. Output in work/.
IMPORT ARRAYS
SUB Main()
  ' AADD
  DIM a() = {10, 20, 30}
  AADD a, 40
  PRINT "aadd="; a(0); ","; a(1); ","; a(2); ","; a(3)
  ' ACHUNK
  DIM b(5) = {1, 2, 3, 4, 5, 6}
  DIM c = ARRAYS.ACHUNK(b, 2)
  PRINT "achunk="; UBOUND(c) + 1
  ' ACOPY
  DIM src(2) = {10, 20, 30}
  DIM dst(2)
  ACOPY src, dst
  PRINT "acopy="; dst(0); dst(1); dst(2)
  ' ADEL
  DIM d(2) = {10, 20, 30}
  ADEL d, 1
  PRINT "adel="; d(0); d(1)
  ' AFILL
  DIM e(4)
  AFILL e, 7
  PRINT "afill="; e(0); e(1); e(2); e(3); e(4)
  ' AFLAT
  DIM nested = {{1, 2}, {3, 4}, {5, 6}}
  DIM f = ARRAYS.AFLAT(nested)
  PRINT "aflat="; f(0); f(1); f(2); f(3); f(4); f(5)
  ' AINS
  DIM g() = {10, 20, 30}
  AINS g, 1
  PRINT "ains="; g(0); g(1); g(2); g(3)
  ' ASHUFFLE
  DIM h(4) = {5, 3, 1, 4, 2}
  ASHUFFLE h
  ASORT h
  PRINT "ashuffle="; h(0); h(1); h(2); h(3); h(4)
  ' AZIP
  DIM a1(2) = {10, 20, 30}
  DIM a2(2) = {100, 200, 300}
  DIM z = ARRAYS.AZIP(a1, a2)
  PRINT "azip="; UBOUND(z) + 1
END SUB