' EXPECT: 10  20  30  40
SUB Main()
  DIM a() = {10, 20, 30}
  AADD a, 40
  PRINT a(0); a(1); a(2); a(3)
END SUB