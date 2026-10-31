' EXPECT: half_even_ok
' CLNG/CINT: arrotondamento half-to-even (MidpointRounding.ToEven, come .NET Convert.ToInt64/ToInt16).
' Canone: 0.5->0, 1.5->2, 2.5->2 (parita' al pari), 3.99->4, -3.5->-4.
SUB Main()
  DIM ok AS INTEGER = 1
  IF CLNG(0.5) <> 0 THEN ok = 0
  IF CLNG(1.5) <> 2 THEN ok = 0
  IF CLNG(2.5) <> 2 THEN ok = 0
  IF CLNG(3.99) <> 4 THEN ok = 0
  IF CLNG(-3.5) <> -4 THEN ok = 0
  IF CLNG(123.4) <> 123 THEN ok = 0
  IF CINT(-0.5) <> 0 THEN ok = 0
  IF CINT(-3.5) <> -4 THEN ok = 0
  IF ok = 1 THEN
    PRINT "half_even_ok"
  ELSE
    PRINT "half_even_FAIL"
  END IF
END SUB
