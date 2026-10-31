' MiniTest: array-campo di CLASSE 2-D numerico (PUBLIC DIM m(2,2) AS INTEGER),
' lettura/scrittura dentro (SELF) e fuori dalla classe, con e senza costruttore.
' EXPECT: sum= 36|e= 5
CLASS Matrix
  PUBLIC DIM m(2, 2) AS INTEGER
  FUNCTION Sum() AS INTEGER
    DIM i AS INTEGER, j AS INTEGER, s AS INTEGER
    FOR i = 0 TO 2
      FOR j = 0 TO 2
        m(i, j) = i * 3 + j
      NEXT j
    NEXT i
    s = 0
    FOR i = 0 TO 2
      FOR j = 0 TO 2
        s = s + m(i, j)
      NEXT j
    NEXT i
    Sum = s
  END FUNCTION
END CLASS
SUB Main()
  DIM mx AS Matrix
  mx = NEW Matrix()
  PRINT "sum="; mx.Sum()
  PRINT "e="; mx.m(1, 2)
END SUB
