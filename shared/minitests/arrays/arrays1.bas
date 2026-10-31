' EXPECT: rev= 2 , 1 , 4 , 1 , 3|uniqN= 4|uniq= 3 , 1|sort= 1 , 1 , 2 , 3 , 4
SUB Main()
  DIM a(4)
  a(0) = 3
  a(1) = 1
  a(2) = 4
  a(3) = 1
  a(4) = 2
  DIM r = ARRAYS.AREVERSE(a)
  PRINT "rev="; r(0); ","; r(1); ","; r(2); ","; r(3); ","; r(4)
  DIM u = ARRAYS.AUNIQUE(a)
  PRINT "uniqN="; UBOUND(u) + 1
  PRINT "uniq="; u(0); ","; u(1)
  DIM s = ARRAYS.ASORT(a)
  PRINT "sort="; s(0); ","; s(1); ","; s(2); ","; s(3); ","; s(4)
END SUB
