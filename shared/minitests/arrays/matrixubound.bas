' EXPECT: 2d= 1 , 1|rect= 2 , 3|1d= 5 , 0|init= 2 , 1|lb= 0 , 0|to1d= 1 , 4 , 1|3d= 0 , 3 , 0 , 2 , 0 , 1
SUB Main()
  DIM M(1, 1)
  PRINT "2d="; UBOUND(M, 1); ","; UBOUND(M, 2)
  DIM R(2, 3)
  PRINT "rect="; UBOUND(R, 1); ","; UBOUND(R, 2)
  DIM V(5)
  PRINT "1d="; UBOUND(V); ","; LBOUND(V)
  DIM W(2, 1) = {{1, 2}, {3, 4}, {5, 6}}
  PRINT "init="; UBOUND(W, 1); ","; UBOUND(W, 2)
  ' LBOUND per dimensione (canone 0-based senza TO).
  DIM L(2, 1)
  PRINT "lb="; LBOUND(L, 1); ","; LBOUND(L, 2)
  ' 1-D con limite inferiore esplicito: LBOUND/UBOUND espliciti devono rispettarlo
  ' (IL: l'argomento dimensione arriva come CONVERSIONS.CINT e cadeva nel path runtime
  ' GetLowerBound/GetUpperBound CLR, perdendo l'offset 1-based).
  DIM T(1 TO 4)
  PRINT "to1d="; LBOUND(T, 1); ","; UBOUND(T, 1); ","; LBOUND(T)
  ' Tre dimensioni: DIM C(3,2,1) → 4×3×2.
  DIM C(3, 2, 1)
  PRINT "3d="; LBOUND(C, 1); ","; UBOUND(C, 1); ","; LBOUND(C, 2); ","; UBOUND(C, 2); ","; LBOUND(C, 3); ","; UBOUND(C, 3)
END SUB
