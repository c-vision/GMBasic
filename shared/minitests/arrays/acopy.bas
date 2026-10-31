' EXPECT: 10  20  30
SUB Main()
  DIM src(2) = {10, 20, 30}
  DIM dst(2)
  ACOPY src, dst
  PRINT dst(0); dst(1); dst(2)
END SUB