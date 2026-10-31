' EXPECT: 10  30
SUB Main()
  DIM a(2) = {10, 20, 30}
  ADEL a, 1
  PRINT a(0); a(1)
END SUB