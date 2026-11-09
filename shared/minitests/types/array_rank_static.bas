' MiniTest: tipi composti ARRAY/MATRIX (2026-11-09).
' `ARRAY OF T` = rank 1; `MATRIX OF (assi…, T)` = rank-n. Lo zucchero
' `DIM v(n) AS T` → ARRAY OF T e `DIM m(n,m) AS T` → MATRIX rank-2.
' `ARRAY OF ARRAY` è rimosso.
' EXPECT: m= 9|s= 5|n3= 7|str= 4
IMPORT SYSTEM

FUNCTION F2() AS MATRIX OF (INT, INT, DOUBLE)
  DIM r(1, 1) AS DOUBLE
  r(0,0) = 1 : r(1,1) = 9 : r(1,0) = 5
  RETURN r
END FUNCTION

FUNCTION F3() AS MATRIX OF (INT, INT, INT, DOUBLE)
  DIM r(1, 1, 1) AS DOUBLE
  r(1,1,1) = 9 : r(1,0,1) = 7
  RETURN r
END FUNCTION

SUB S2(a AS MATRIX OF (INT, INT, DOUBLE))
  PRINT "s="; a(1,0)
END SUB

SUB Main()
  DIM m = F2()
  PRINT "m="; m(1,1)
  CALL S2(m)

  DIM t = F3()
  PRINT "n3="; t(1,0,1)

  ' zucchero: DIM v(n) AS STRING → ARRAY OF STRING
  DIM strs(2) AS STRING
  strs(0) = "ab" : strs(1) = "abc" : strs(2) = "abcd"
  PRINT "str="; LEN(strs(2))
END SUB
