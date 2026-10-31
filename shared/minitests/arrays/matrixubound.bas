' EXPECT: 2d= 1 , 1|rect= 2 , 3|1d= 5 , 0|init= 2 , 1
SUB Main()
  DIM M(1, 1)
  PRINT "2d="; UBOUND(M, 1); ","; UBOUND(M, 2)
  DIM R(2, 3)
  PRINT "rect="; UBOUND(R, 1); ","; UBOUND(R, 2)
  DIM V(5)
  PRINT "1d="; UBOUND(V); ","; LBOUND(V)
  DIM W(2, 1) = {{1, 2}, {3, 4}, {5, 6}}
  PRINT "init="; UBOUND(W, 1); ","; UBOUND(W, 2)
END SUB
