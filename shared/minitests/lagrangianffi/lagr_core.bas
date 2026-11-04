' OPTIMIZE (FFI lagrangian) — rilassamento lagrangiano + subgradiente.
' Due istanze con dimensioni diverse (m=2/nx=2 e m=1/nx=3) per coprire i parametri m, nx, A, b,
' maxit (incl. <=0 = default 100) e il layout di out() = [LB, lambda(m), best x(nx)].
' La callback usa la forma "ritorno ARRAY": r(0)=valore del subproblema, r(1..nx)=x.
' EXPECT: a=3.54:-1 -1 -1 :-1 -1 :-1 -1||b=4.00:-1 -1 -1 :-1 :-1 -1 -1
IMPORT OPTIMIZE

' (a) min x0 + 2*x1  s.t.  x0 + x1 >= 5,  2*x0 + x1 >= 6,  x in {0..10}^2  (ottimo 5 in (5,0))
FUNCTION Sub2(lam() AS DOUBLE) AS ARRAY OF DOUBLE
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

' (b) min x0 + 2*x1 + 3*x2  s.t.  x0 + x1 + x2 >= 4,  x in {0..5}^3  (ottimo 4 in (4,0,0))
FUNCTION Sub3(lam() AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(3) AS DOUBLE
  best = 1000000000.0 : bx0 = 0 : bx1 = 0 : bx2 = 0
  FOR a = 0 TO 5
    FOR c = 0 TO 5
      FOR e = 0 TO 5
        v = a + 2 * c + 3 * e + lam(0) * (4 - a - c - e)
        IF v < best THEN
          best = v : bx0 = a : bx1 = c : bx2 = e
        END IF
      NEXT e
    NEXT c
  NEXT a
  r(0) = best : r(1) = bx0 : r(2) = bx1 : r(3) = bx2
  RETURN r
END FUNCTION

SUB Main()
  ' ── (a) m=2, nx=2, 60 iterazioni ────────────────────────────────────────────────
  DIM A(3) AS DOUBLE
  A(0)=1 : A(1)=1 : A(2)=2 : A(3)=1
  DIM b(1) AS DOUBLE
  b(0)=5 : b(1)=6
  DIM out(4) AS DOUBLE
  lb = OPTIMIZE.LAGRANGIANSUBGRADIENT(Sub2, A, b, 2, 2, 60, out)
  PRINT "a="; FORMAT(lb, "#.00"); ":"; (lb <= 5); (lb > 3); (out(0) = lb); ":";
  PRINT (out(1) >= 0); (out(2) >= 0); ":"; (out(3) >= 0); (out(4) >= 0)

  ' ── (b) m=1, nx=3, maxit 0 -> default 100 ───────────────────────────────────────
  DIM A3(2) AS DOUBLE
  A3(0)=1 : A3(1)=1 : A3(2)=1
  DIM b3(0) AS DOUBLE
  b3(0)=4
  DIM out3(4) AS DOUBLE
  lb3 = OPTIMIZE.LAGRANGIANSUBGRADIENT(Sub3, A3, b3, 1, 3, 0, out3)
  PRINT "|b="; FORMAT(lb3, "#.00"); ":"; (lb3 <= 4); (lb3 > 2); (out3(0) = lb3); ":";
  PRINT (out3(1) >= 0); ":"; (out3(2) >= 0); (out3(3) >= 0); (out3(4) >= 0)
  PRINT ""
END SUB
