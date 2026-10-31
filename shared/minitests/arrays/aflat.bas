' EXPECT: 1  2  3  4  5  6
SUB Main()
  DIM nested = {{1, 2}, {3, 4}, {5, 6}}
  DIM f = ARRAYS.AFLAT(nested)
  PRINT f(0); f(1); f(2); f(3); f(4); f(5)
END SUB