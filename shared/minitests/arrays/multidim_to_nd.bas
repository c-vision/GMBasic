' EXPECT: dims= 1 , 2 , 1 , 3 , 1 , 2 , 1 , 2|corners= 1111 , 2322 , 1312 , 2121|sum= 41196|ub/lb= 2 , 1|d5= 1 , 2 , 1 , 9|lit3= 1 , 2 , 3 , 4 , 5 , 8
SUB Main()
  DIM a AS LONG = 2
  DIM b AS LONG = 3
  DIM c AS LONG = 2
  DIM d AS LONG = 2
  DIM m(1 TO a, 1 TO b, 1 TO c, 1 TO d) AS DOUBLE
  DIM i AS LONG, j AS LONG, k AS LONG, l AS LONG
  DIM s AS DOUBLE
  FOR i = 1 TO a
   FOR j = 1 TO b
    FOR k = 1 TO c
     FOR l = 1 TO d
       m(i,j,k,l) = i*1000 + j*100 + k*10 + l
       s = s + m(i,j,k,l)
     NEXT l
    NEXT k
   NEXT j
  NEXT i
  PRINT "dims="; LBOUND(m,1); ","; UBOUND(m,1); ","; LBOUND(m,2); ","; UBOUND(m,2); ","; LBOUND(m,3); ","; UBOUND(m,3); ","; LBOUND(m,4); ","; UBOUND(m,4)
  PRINT "corners="; m(1,1,1,1); ","; m(2,3,2,2); ","; m(1,3,1,2); ","; m(2,1,2,1)
  PRINT "sum="; s
  PRINT "ub/lb="; UBOUND(m); ","; LBOUND(m)
  DIM q(1 TO 2, 1 TO 2, 1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE
  q(1,1,1,1,1)=1 : q(2,2,2,2,2)=9
  PRINT "d5="; LBOUND(q,5); ","; UBOUND(q,5); ","; q(1,1,1,1,1); ","; q(2,2,2,2,2)
  DIM M3(1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE = {{{1,2},{3,4}},{{5,6},{7,8}}}
  PRINT "lit3="; M3(1,1,1); ","; M3(1,1,2); ","; M3(1,2,1); ","; M3(1,2,2); ","; M3(2,1,1); ","; M3(2,2,2)
END SUB
