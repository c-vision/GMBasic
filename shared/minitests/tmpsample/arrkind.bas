' EXPECT: s= 7 |s2= 7
FUNCTION Somma(a() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(a)
    s = s + a(i)
  NEXT i
  RETURN s
END FUNCTION
SUB Main()
  DIM x(1) AS LONG
  x(0)=3 : x(1)=4
  PRINT "s="; Somma(x);
  DIM y() = {3, 4}
  PRINT "|s2="; Somma(y)
  PRINT ""
END SUB
