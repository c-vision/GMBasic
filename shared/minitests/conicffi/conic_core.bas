' OPTIMIZE (FFI conic) — le 7 funzioni su tutti e 3 i coni: PEXP (kind 0), PPOW (1), RPOW (2).
' Copre i parametri `kind`, `alpha`, i domini (punto interno e punto NON interno) e `smu`.
' EXPECT: pexp= 0 :0.000: 0 :-1.000: 0 :1.000:2.000|bad=-1 -1 -1|ppow= 0 :-1.792: 0 :0.667: 0 :1.111|rpow= 0 :-2.639: 0 :0.286|badalpha=-1 -1|dual= 1 : 0 :0.824: 0|met= 0 :2.333:-1 :-1 :-1 |scl= 1 :2.333:-1 -1 -1
IMPORT OPTIMIZE

SUB Main()
  ' ── PEXP: barriera, gradiente, Hessiana in (2,1,0) [2 > 1*e^0] ──────────────────
  DIM x(2) AS DOUBLE
  x(0)=2 : x(1)=1 : x(2)=0
  DIM f(0) AS DOUBLE
  PRINT "pexp="; OPTIMIZE.EXPCONEBARRIER(0, 0.5, x, f); ":"; FORMAT(f(0), "#.000"); ":";
  DIM g(2) AS DOUBLE
  PRINT OPTIMIZE.EXPCONEGRAD(0, 0.5, x, g); ":"; FORMAT(g(0), "#.000"); ":";
  DIM H(8) AS DOUBLE
  PRINT OPTIMIZE.EXPCONEHESS(0, 0.5, x, H); ":"; FORMAT(H(0), "#.000"); ":"; FORMAT(H(4), "#.000")

  ' ── fuori dominio (u = 0) -> -1 ─────────────────────────────────────────────────
  DIM x0(2) AS DOUBLE
  x0(0)=2 : x0(1)=0 : x0(2)=0
  PRINT "bad="; OPTIMIZE.EXPCONEBARRIER(0, 0.5, x0, f); OPTIMIZE.EXPCONEGRAD(0, 0.5, x0, g); OPTIMIZE.EXPCONEHESS(0, 0.5, x0, H)

  ' ── PPOW (kind 1) e RPOW (kind 2) con alpha = 0.5, punto (2,2,1) ─────────────────
  DIM p(2) AS DOUBLE
  p(0)=2 : p(1)=2 : p(2)=1
  PRINT "ppow="; OPTIMIZE.EXPCONEBARRIER(1, 0.5, p, f); ":"; FORMAT(f(0), "#.000"); ":";
  PRINT OPTIMIZE.EXPCONEGRAD(1, 0.5, p, g); ":"; FORMAT(g(2), "#.000"); ":";
  PRINT OPTIMIZE.EXPCONEHESS(1, 0.5, p, H); ":"; FORMAT(H(8), "#.000")
  PRINT "rpow="; OPTIMIZE.EXPCONEBARRIER(2, 0.5, p, f); ":"; FORMAT(f(0), "#.000"); ":";
  PRINT OPTIMIZE.EXPCONEGRAD(2, 0.5, p, g); ":"; FORMAT(g(2), "#.000")

  ' ── alpha fuori (0,1) -> -1 ─────────────────────────────────────────────────────
  PRINT "badalpha="; OPTIMIZE.EXPCONEBARRIER(1, 1.5, p, f); OPTIMIZE.EXPCONEGRAD(1, 0.0, p, g)

  ' ── duale: s=(2,2,1) interno a K* (PPOW/RPOW), s=(2,1,0) NON interno a PEXP ─────
  DIM s(2) AS DOUBLE
  s(0)=2 : s(1)=2 : s(2)=-1
  DIM w(2) AS DOUBLE
  PRINT "dual="; OPTIMIZE.EXPCONEDUALIN(1, 0.5, s); ":";
  PRINT OPTIMIZE.EXPCONEDUALPOINT(1, 0.5, s, w); ":"; FORMAT(w(0), "#.000"); ":";
  DIM s2(2) AS DOUBLE
  s2(0)=2 : s2(1)=-1 : s2(2)=0
  PRINT OPTIMIZE.EXPCONEDUALIN(0, 0.5, s2)

  ' ── metriche NT (7) e scaling (Theta|Thin|rt|scale = 28) ────────────────────────
  DIM m(6) AS DOUBLE
  PRINT "met="; OPTIMIZE.EXPCONENTMETRICS(1, 0.5, p, s, m); ":"; FORMAT(m(0), "#.000");
  FOR i = 1 TO 3
    PRINT ":"; (m(i) > 0);
  NEXT i
  DIM sc(27) AS DOUBLE
  PRINT "|scl="; OPTIMIZE.EXPCONESCALING(1, 0.5, p, s, 1.5, sc); ":"; FORMAT(sc(27), "#.000"); ":";
  PRINT (sc(0) <> 0); (sc(9) <> 0); (sc(18) <> 0)
  PRINT ""
END SUB
