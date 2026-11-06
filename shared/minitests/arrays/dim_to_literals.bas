' Inizializzatori letterali su array con lower bound esplicito: 1-D, 2-D, 3-D annidati,
' riempimento parziale e FOR EACH sull'intervallo logico.
' EXPECT: a= 11 , 22 , 33 , 44|b= 1 , 2 , 0|m= 1 , 2 , 5 , 7|c= 1 , 4 , 6 , 8|fe= 60
SUB Main()
  DIM a(1 TO 4) AS DOUBLE = {11,22,33,44}
  PRINT "a="; a(1); ","; a(2); ","; a(3); ","; a(4)
  DIM b(1 TO 3) AS DOUBLE = {1,2}
  PRINT "b="; b(1); ","; b(2); ","; b(3)
  DIM m(1 TO 2, 1 TO 2) AS DOUBLE = {{1,2},{5,7}}
  PRINT "m="; m(1,1); ","; m(1,2); ","; m(2,1); ","; m(2,2)
  DIM c(1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE = {{{1,2},{3,4}},{{5,6},{7,8}}}
  PRINT "c="; c(1,1,1); ","; c(1,2,2); ","; c(2,1,2); ","; c(2,2,2)
  DIM f(1 TO 3) AS DOUBLE
  f(1) = 10 : f(2) = 20 : f(3) = 30
  DIM t AS DOUBLE
  FOR EACH x IN f
    t = t + x
  NEXT
  PRINT "fe="; t
END SUB
