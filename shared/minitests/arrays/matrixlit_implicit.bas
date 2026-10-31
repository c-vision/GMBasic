' EXPECT: a= 10 , 20 , 30 , 40|mut= 99
SUB Main()
  A = {{10, 20}, {30, 40}}
  PRINT "a="; A(0,0); ","; A(0,1); ","; A(1,0); ","; A(1,1)
  A(1, 0) = 99
  PRINT "mut="; A(1,0)
END SUB
