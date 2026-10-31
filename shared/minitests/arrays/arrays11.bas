' EXPECT: s= 1 , 1 , 2 , 3 , 4
SUB Main()
  DIM a(4)
  a(0) = 3: a(1) = 1: a(2) = 4: a(3) = 1: a(4) = 2
  ASORT a
  PRINT "s="; a(0); ","; a(1); ","; a(2); ","; a(3); ","; a(4)
END SUB
