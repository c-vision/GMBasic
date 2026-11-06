' FILE: conic_ipm_presolve.bas
' OPTIMIZE (FFI conic) — IPM per LP e QP (denso, warm start, CSC) e presolve LP.
' Dati scelti perché la soluzione si verifichi a mano:
'   LP   min 2x1+3x2  s.t. x1+x2=1, x1-x2=0.5   -> x=(0.75,0.25), y=(2.5,-0.5)
'   QP   min x1^2+x2^2  s.t. x1+x2=1            -> x=(0.5,0.5), y=1
'   PRE  min x1+x2  s.t. x1+x2=2, x2=1          -> x=(1,1), y=(1,0)
' EXPECT: lp= 0|:0.750000,0.250000|:2.500000,-0.500000|:0.000000,0.000000||lpw= 0|:0.750000,0.250000||lpc= 0|:0.750000,0.250000|:2.500000,-0.500000||qp= 0|:0.500000,0.500000|:1.000000||qpw= 0|:0.500000,0.500000||qpc= 0|:0.500000,0.500000|:1.000000||pre=1|: 1|: 0 = 0 , 0 , 0|: 0|: 0|:1.000000,1.000000|:1.000000,0.000000||prd= 0|:0.000000,0.000000|:0.000000,0.000000|: 0||f=0
IMPORT OPTIMIZE

SUB Main()
  ' ── LP denso ────────────────────────────────────────────────────────────
  DIM A(3) AS DOUBLE
  A(0)=1 : A(1)=1 : A(2)=1 : A(3)=-1
  DIM b(1) AS DOUBLE
  b(0)=1 : b(1)=0.5
  DIM c(1) AS DOUBLE
  c(0)=2 : c(1)=3
  DIM x(1) AS DOUBLE, y(1) AS DOUBLE, z(1) AS DOUBLE
  PRINT "lp="; OPTIMIZE.IPMLP(A, 2, 2, b, c, 1E-8, 1E-8, 1E-8, 200, x, y, z)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")
  PRINT ":"; FORMAT(y(0), "#.000000"); ","; FORMAT(y(1), "#.000000")
  PRINT ":"; FORMAT(z(0), "#.000000"); ","; FORMAT(z(1), "#.000000")

  ' ── LP con warm start ───────────────────────────────────────────────────
  DIM x0(1) AS DOUBLE, y0(1) AS DOUBLE
  x0(0)=0.7 : x0(1)=0.3
  y0(0)=2.4 : y0(1)=-0.6
  PRINT "|lpw="; OPTIMIZE.IPMLPWARM(A, 2, 2, b, c, 1E-8, 1E-8, 1E-8, 200, x, y, z, x0, y0)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")

  ' ── LP sparso (CSC: Aptr, Arow, Aval) ───────────────────────────────────
  DIM ap(2) AS LONG, ar(3) AS LONG, av(3) AS DOUBLE
  ap(0)=0 : ap(1)=2 : ap(2)=4
  ar(0)=0 : ar(1)=1 : ar(2)=0 : ar(3)=1
  av(0)=1 : av(1)=1 : av(2)=1 : av(3)=-1
  PRINT "|lpc="; OPTIMIZE.IPMLPCSC(ap, ar, av, 2, 2, b, c, 1E-8, 1E-8, 1E-8, 200, x, y, z)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")
  PRINT ":"; FORMAT(y(0), "#.000000"); ","; FORMAT(y(1), "#.000000")

  ' ── QP denso: Q = 2I, un solo vincolo x1+x2=1 ───────────────────────────
  DIM A1(1) AS DOUBLE
  A1(0)=1 : A1(1)=1
  DIM Q(3) AS DOUBLE
  Q(0)=2 : Q(1)=0 : Q(2)=0 : Q(3)=2
  DIM b1(0) AS DOUBLE
  b1(0)=1
  DIM c0(1) AS DOUBLE
  c0(0)=0 : c0(1)=0
  DIM y1(0) AS DOUBLE
  PRINT "|qp="; OPTIMIZE.IPMQP(A1, Q, 1, 2, b1, c0, 1E-8, 1E-8, 1E-8, 200, x, y1, z)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")
  PRINT ":"; FORMAT(y1(0), "#.000000")

  ' ── QP con warm start ───────────────────────────────────────────────────
  DIM xq(1) AS DOUBLE, yq(0) AS DOUBLE
  xq(0)=0.4 : xq(1)=0.6
  yq(0)=0.5
  PRINT "|qpw="; OPTIMIZE.IPMQPWARM(A1, Q, 1, 2, b1, c0, 1E-8, 1E-8, 1E-8, 200, x, y1, z, xq, yq)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")

  ' ── QP sparso: A in CSC, Q nel triangolo inferiore in CSC ───────────────
  DIM aq(2) AS LONG, arq(1) AS LONG, avq(1) AS DOUBLE
  aq(0)=0 : aq(1)=1 : aq(2)=2
  arq(0)=0 : arq(1)=0
  avq(0)=1 : avq(1)=1
  DIM qp(2) AS LONG, qrow(1) AS LONG, qv(1) AS DOUBLE
  qp(0)=0 : qp(1)=1 : qp(2)=2
  qrow(0)=0 : qrow(1)=1
  qv(0)=2 : qv(1)=2
  PRINT "|qpc="; OPTIMIZE.IPMQPCSC(aq, arq, avq, qp, qrow, qv, 1, 2, b1, c0, 1E-8, 1E-8, 1E-8, 200, x, y1, z)
  PRINT ":"; FORMAT(x(0), "#.000000"); ","; FORMAT(x(1), "#.000000")
  PRINT ":"; FORMAT(y1(0), "#.000000")

  ' ── presolve LP: min x1+x2 s.t. x1+x2=2, x2=1 ──────────────────────────
  DIM pp(2) AS LONG, pr(2) AS LONG, pv(2) AS DOUBLE
  pp(0)=0 : pp(1)=1 : pp(2)=3
  pr(0)=0 : pr(1)=0 : pr(2)=1
  pv(0)=1 : pv(1)=1 : pv(2)=1
  DIM pb(1) AS DOUBLE, pc(1) AS DOUBLE
  pb(0)=2 : pb(1)=1
  pc(0)=1 : pc(1)=1
  h = OPTIMIZE.PRESOLVENEW(pp, pr, pv, pb, pc, 2, 2, 1E-9)
  IF h > 0 THEN
    PRINT "|pre=1"
  ELSE
    PRINT "|pre=0"
  END IF
  PRINT ":"; OPTIMIZE.PRESOLVECHANGED(h)
  DIM pinfo(2) AS LONG
  w = OPTIMIZE.PRESOLVEDIMS(h, pinfo)
  PRINT ":"; w; "="; pinfo(0); ","; pinfo(1); ","; pinfo(2)
  DIM rp(0) AS LONG, rr(0) AS LONG, rv(0) AS DOUBLE
  DIM rb(0) AS DOUBLE, rc(0) AS DOUBLE
  PRINT ":"; OPTIMIZE.PRESOLVEREDUCED(h, rp, rr, rv, rb, rc)
  DIM xr(0) AS DOUBLE, yr(0) AS DOUBLE
  DIM xf(1) AS DOUBLE, yf(1) AS DOUBLE
  PRINT ":"; OPTIMIZE.PRESOLVEPOSTSOLVE(h, xr, yr, xf, yf)
  PRINT ":"; FORMAT(xf(0), "#.000000"); ","; FORMAT(xf(1), "#.000000")
  PRINT ":"; FORMAT(yf(0), "#.000000"); ","; FORMAT(yf(1), "#.000000")
  DIM rd(1) AS DOUBLE, yd(1) AS DOUBLE, rf(1) AS DOUBLE, ydf(1) AS DOUBLE
  PRINT "|prd="; OPTIMIZE.PRESOLVEPOSTSOLVEDIR(h, rd, yd, rf, ydf)
  PRINT ":"; FORMAT(rf(0), "#.000000"); ","; FORMAT(rf(1), "#.000000")
  PRINT ":"; FORMAT(ydf(0), "#.000000"); ","; FORMAT(ydf(1), "#.000000")
  PRINT ":"; OPTIMIZE.PRESOLVEFREE(h)
  PRINT "|f=0"
END SUB
