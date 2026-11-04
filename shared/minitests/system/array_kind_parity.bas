' EXPECT: long2dbl= 7 |dbl2long= 7 |writeback= 9
' Parità del KIND dell'elemento al confine di una chiamata (2026-11-04): su IL gli array sono
' tipizzati (double[]/long[]) mentre C99/PCode/VM-C hanno un'unica rappresentazione numerica.
' Un array LONG passato a un parametro `a() AS DOUBLE` (e viceversa) deve essere convertito,
' e le scritture del callee devono tornare al chiamante.
FUNCTION SommaD(a() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(a)
    s = s + a(i)
  NEXT i
  RETURN s
END FUNCTION

FUNCTION SommaL(a() AS LONG) AS LONG
  s = 0
  FOR i = 0 TO UBOUND(a)
    s = s + a(i)
  NEXT i
  RETURN s
END FUNCTION

SUB Scrivi(a() AS DOUBLE)
  a(0) = 9.0
END SUB

SUB Main()
  DIM xl(1) AS LONG
  xl(0)=3 : xl(1)=4
  PRINT "long2dbl="; SommaD(xl);

  DIM xd(1) AS DOUBLE
  xd(0)=3 : xd(1)=4
  PRINT "|dbl2long="; SommaL(xd);

  ' scrittura del callee su un array LONG passato a un parametro DOUBLE (copy-back)
  DIM xw(1) AS LONG
  xw(0)=1 : xw(1)=2
  CALL Scrivi(xw)
  PRINT "|writeback="; xw(0)
  PRINT ""
END SUB
