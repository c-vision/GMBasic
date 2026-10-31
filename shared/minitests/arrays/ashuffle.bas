' EXPECT: 1  2  3  4  5
SUB Main()
  DIM a(4) = {5, 3, 1, 4, 2}
  ASHUFFLE a
  ASORT a
  PRINT a(0); a(1); a(2); a(3); a(4)
END SUB