' OPTIMIZE (FFI conic) — cono esponenziale: barriera, gradiente, Hessiana, duale, scaling.
' EXPECT: bar= 0 :0.000|grad= 0 :-1.000|hess= 0 :1.000|din= 1 |dpt= 0 :0.635|met= 0 :3.000|scl= 1 :3.000
IMPORT OPTIMIZE

SUB Main()
  ' PEXP: t=2, u=1, v=0 (interno: 2 > 1*e^0)
  DIM x(2) AS DOUBLE
  x(0)=2 : x(1)=1 : x(2)=0
  DIM f(0) AS DOUBLE
  PRINT "bar="; OPTIMIZE.EXPCONEBARRIER(0, 0.5, x, f); ":"; FORMAT(f(0), "#.000");

  DIM g(2) AS DOUBLE
  PRINT "|grad="; OPTIMIZE.EXPCONEGRAD(0, 0.5, x, g); ":"; FORMAT(g(0), "#.000");

  DIM H(8) AS DOUBLE
  PRINT "|hess="; OPTIMIZE.EXPCONEHESS(0, 0.5, x, H); ":"; FORMAT(H(0), "#.000");

  ' duale: s=(2,-1,0) interno a K* (s2<0, s1-s2+s2*log(-s2/s0)>0)
  DIM s(2) AS DOUBLE
  s(0)=2 : s(1)=2 : s(2)=-1
  PRINT "|din="; OPTIMIZE.EXPCONEDUALIN(0, 0.5, s);

  DIM W(2) AS DOUBLE
  PRINT "|dpt="; OPTIMIZE.EXPCONEDUALPOINT(0, 0.5, s, W); ":"; FORMAT(W(0), "#.000");

  DIM m(6) AS DOUBLE
  PRINT "|met="; OPTIMIZE.EXPCONENTMETRICS(0, 0.5, x, s, m); ":"; FORMAT(m(0), "#.000");

  DIM out(27) AS DOUBLE
  PRINT "|scl="; OPTIMIZE.EXPCONESCALING(0, 0.5, x, s, 1.0, out); ":"; FORMAT(out(27), "#.000")
  PRINT ""
END SUB
