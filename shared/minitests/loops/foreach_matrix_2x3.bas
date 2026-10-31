' MiniTest: matrice 2x3 row-major con FOR EACH + accesso indicizzato (canone
' stride 2026-09-30: prima VM/C usava isqrt(count) → leggeva 3,4,5 invece di 4,5,6).
' EXPECT: mat-2x3-ok
SUB Main()
  DIM m(1, 2) AS DOUBLE = { {1,2,3}, {4,5,6} }
  DIM sum AS DOUBLE = 0
  FOR R = 0 TO 1
    FOR C = 0 TO 2
      sum = sum + m(R, C)
    NEXT
  NEXT
  IF sum = 21 AND m(1,0) = 4 AND m(1,2) = 6 THEN PRINT "mat-2x3-ok"
END SUB
