' OPTIMIZE (FFI lagrangian) — rilassamento lagrangiano + subgradiente su un piccolo ILP.
' min x0 + 2*x1  s.t.  x0 + x1 >= 5,  2*x0 + x1 >= 6,  x in {0..10}^2
' L'ottimo vale 5 in (5,0); il rilassamento deve avvicinarsi dall'esterno (LB <= 5).
' EXPECT: lb=3.54:-1 -1 -1 :-1 -1
IMPORT OPTIMIZE

' Subproblema: min_x f(x) + lam(0)*(5 - A0 x) + lam(1)*(6 - A1 x) su griglia 11x11.
' Ritorno "vettore": r(0) = valore ottimo del subproblema, r(1..2) = x.
FUNCTION Sub(lam() AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  best = 1000000000.0 : bx0 = 0 : bx1 = 0
  FOR a = 0 TO 10
    FOR c = 0 TO 10
      v = a + 2 * c + lam(0) * (5 - a - c) + lam(1) * (6 - 2 * a - c)
      IF v < best THEN
        best = v : bx0 = a : bx1 = c
      END IF
    NEXT c
  NEXT a
  r(0) = best : r(1) = bx0 : r(2) = bx1
  RETURN r
END FUNCTION

SUB Main()
  DIM A(3) AS DOUBLE
  A(0)=1 : A(1)=1 : A(2)=2 : A(3)=1
  DIM b(1) AS DOUBLE
  b(0)=5 : b(1)=6
  DIM out(4) AS DOUBLE          ' 1 + m(2) + nx(2)
  lb = OPTIMIZE.LAGRANGIANSUBGRADIENT(Sub, A, b, 2, 2, 60, out)
  PRINT "lb="; FORMAT(lb, "#.00"); ":";
  ' bound valido (<= ottimo 5) e non banale; out(0) replica il valore di ritorno
  PRINT (lb <= 5); (lb > 3); (out(0) = lb); ":";
  ' moltiplicatori non negativi
  PRINT (out(1) >= 0); (out(2) >= 0)
  PRINT ""
END SUB
