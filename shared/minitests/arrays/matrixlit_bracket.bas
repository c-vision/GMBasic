' EXPECT: diag= 1 , 5 , 9|row1= 4 , 5 , 6
SUB Main()
  DIM M(3, 3)
  M = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
  PRINT "diag="; M(0,0); ","; M(1,1); ","; M(2,2)
  PRINT "row1="; M(1,0); ","; M(1,1); ","; M(1,2)
END SUB
