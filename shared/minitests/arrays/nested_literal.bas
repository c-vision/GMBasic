' EXPECT: ub= 2|f= 1  2  3  4  5  6
SUB Main()
  DIM nested = {{1, 2}, {3, 4}, {5, 6}}
  PRINT "ub="; UBOUND(nested)
  DIM f = ARRAYS.AFLAT(nested)
  PRINT "f="; f(0); f(1); f(2); f(3); f(4); f(5)
END SUB
