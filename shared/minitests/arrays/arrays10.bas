' EXPECT: f= 9 , 9 , 9 , 9 , 9|p= 9 , 5 , 5 , 5 , 9|c= 1 , 2 , 3
SUB Main()
  DIM b(4)
  AFILL b, 9
  PRINT "f="; b(0); ","; b(1); ","; b(2); ","; b(3); ","; b(4)
  AFILL b, 5, 1, 3
  PRINT "p="; b(0); ","; b(1); ","; b(2); ","; b(3); ","; b(4)
  DIM s(2)
  s(0) = 1: s(1) = 2: s(2) = 3
  DIM d(2)
  ACOPY s, d
  PRINT "c="; d(0); ","; d(1); ","; d(2)
END SUB
