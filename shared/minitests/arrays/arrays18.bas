' EXPECT: tot1= 60|cnt= 3
SUB Main()
  DIM Tot = 0
  DIM Numeri = {10, 20, 30}
  AEVAL Numeri, { |n| Tot := Tot + n }
  PRINT "tot1="; Tot
  DIM Cnt = 0
  DIM S$ = {"a", "b", "c"}
  AEVAL S$, { |s| Cnt := Cnt + 1 }
  PRINT "cnt="; Cnt
END SUB
