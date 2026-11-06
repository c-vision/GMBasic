' EXPECT: d2= 1  ,  4  ,  1  ,  3|sum= 3024|corners= 101  ,  403  ,  202  ,  401|ub= 4  , lb= 1|d3= 1  ,  2  ,  1  ,  3  ,  1  ,  2|fe= 60
SUB Main()
  DIM r AS LONG = 4
  DIM c AS LONG = 3
  DIM m(1 TO r, 1 TO c) AS DOUBLE
  DIM i AS LONG, j AS LONG
  DIM s AS DOUBLE
  FOR i = 1 TO r
    FOR j = 1 TO c
      m(i, j) = i * 100 + j
      s = s + m(i, j)
    NEXT j
  NEXT i
  PRINT "d2="; LBOUND(m,1); " , "; UBOUND(m,1); " , "; LBOUND(m,2); " , "; UBOUND(m,2)
  PRINT "sum="; s
  PRINT "corners="; m(1,1); " , "; m(4,3); " , "; m(2,2); " , "; m(4,1)
  PRINT "ub="; UBOUND(m); " , lb="; LBOUND(m)
  DIM d3(1 TO 2, 1 TO 3, 1 TO 2) AS DOUBLE
  d3(1,1,1) = 1 : d3(2,3,2) = 9
  PRINT "d3="; LBOUND(d3,1); " , "; UBOUND(d3,1); " , "; LBOUND(d3,2); " , "; UBOUND(d3,2); " , "; LBOUND(d3,3); " , "; UBOUND(d3,3)
  DIM fe(1 TO 3) AS DOUBLE
  fe(1) = 10 : fe(2) = 20 : fe(3) = 30
  DIM t AS DOUBLE
  FOR EACH x IN fe
    t = t + x
  NEXT
  PRINT "fe="; t
END SUB
