' Propagazione dei lower bound ai parametri array delle procedure: byval, BYREF,
' scrittura through, array 0-based, function value e lambda.
' EXPECT: fill= 1 , 3|after= 20 , 40 , 60|ref= 1 , 3 , 40|refafter= 99|zero= 0 , 2 , 7|fv= 80|lam= 80
FUNCTION Somma(a() AS DOUBLE) AS DOUBLE
  RETURN a(1) + a(UBOUND(a,1))
END FUNCTION

SUB Fill(a() AS DOUBLE)
  DIM i AS LONG
  FOR i = LBOUND(a,1) TO UBOUND(a,1)
    a(i) = a(i) * 2
  NEXT i
  PRINT "fill="; LBOUND(a,1); ","; UBOUND(a,1)
END SUB

SUB ShowRef(BYREF a() AS DOUBLE)
  PRINT "ref="; LBOUND(a,1); ","; UBOUND(a,1); ","; a(2)
  a(2) = 99
END SUB

SUB Zero(a() AS DOUBLE)
  PRINT "zero="; LBOUND(a,1); ","; UBOUND(a,1); ","; a(0)
END SUB

SUB Main()
  DIM a(1 TO 3) AS DOUBLE
  a(1) = 10 : a(2) = 20 : a(3) = 30
  CALL Fill(a)
  PRINT "after="; a(1); ","; a(2); ","; a(3)
  CALL ShowRef(a)
  PRINT "refafter="; a(2)
  DIM z(0 TO 2) AS DOUBLE
  z(0) = 7 : z(1) = 8 : z(2) = 9
  CALL Zero(z)
  DIM f = Somma
  PRINT "fv="; f(a)
  DIM l = FN(x() AS DOUBLE) AS DOUBLE => x(1) + x(UBOUND(x,1))
  PRINT "lam="; l(a)
END SUB
