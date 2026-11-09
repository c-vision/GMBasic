' MiniTest: rank statico nel TIPO (matrici rettangolari rank-2/rank-3).
' Il rank fa parte del tipo: attraversa parametri e valori di ritorno senza
' deduzione dal valore. Sintassi: `AS DOUBLE(,)` (rank 2), `AS DOUBLE(,,)` (rank 3);
' parametri `a(,) AS DOUBLE`. `DIM a(x,y)` / `DIM a(x,y,z)`.
' EXPECT: m= 9|s= 5|n3= 5
IMPORT SYSTEM

FUNCTION F2() AS DOUBLE(,)
  DIM r(1,1) AS DOUBLE
  r(0,0) = 1 : r(1,1) = 9
  RETURN r
END FUNCTION

FUNCTION F3() AS DOUBLE(,,)
  DIM r(1,1,1) AS DOUBLE
  r(1,1,1) = 9 : r(1,0,1) = 5
  RETURN r
END FUNCTION

SUB S2(a(,) AS DOUBLE)
  PRINT "s="; a(1,0)
END SUB

SUB Main()
  DIM m = F2()
  m(1,0) = 5
  PRINT "m="; m(1,1)
  CALL S2(m)

  DIM t = F3()
  PRINT "n3="; t(1,0,1)
END SUB
