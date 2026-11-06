REM FILE: 16-ConicSolvers.bas — pacchetto `conic` (FFI): IPM (LP/QP denso e sparso,
REM warm start), presolve+postsolve, SOCP (denso e sparso), SDP con blocchi PSD/SOC/EXP
REM e trasformazione in forma standard. 26 funzioni, tutte su dati verificabili a mano.
REM
REM Scenario: una piccola desk di dispacciamento elettrico e di allocazione.
REM
REM   LP   min 2g1 + 3g2 + 4g3   s.t. g1+g2+g3 = 100, g1+g2 = 60, g >= 0
REM        -> g = (60, 0, 40), costo 280, duali y = (4, -2)
REM   QP   min g1^2 + 2 g2^2      s.t. g1+g2 = 1
REM        -> g = (2/3, 1/3), y = 4/3
REM   SOCP min t                   s.t. g = 30, (t, g) in SOC
REM        -> t = 30, g = 30, lam = (1, -1), y = -1
REM   SDP  min X11 + 2 X22        s.t. tr(X) = 2, X >= 0
REM        -> X = [[2,0],[0,0]], S = [[0,0],[0,1]], y = 1
IMPORT OPTIMIZE

SUB Main()
  REM ── 1) dispacciamento LP: forma densa ────────────────────────────────────────
  DIM A(5) AS DOUBLE
  A(0)=1 : A(1)=1 : A(2)=1        ' riga 0: g1+g2+g3
  A(3)=1 : A(4)=1 : A(5)=0        ' riga 1: g1+g2
  DIM b(1) AS DOUBLE
  b(0)=100 : b(1)=60
  DIM c(2) AS DOUBLE
  c(0)=2 : c(1)=3 : c(2)=4
  DIM g(2) AS DOUBLE, y(1) AS DOUBLE, z(2) AS DOUBLE
  PRINT "lp="; OPTIMIZE.IPMLP(A, 2, 3, b, c, 1E-8, 1E-8, 1E-8, 200, g, y, z)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000"); ","; FORMAT(g(2), "#.000000")
  PRINT ":"; FORMAT(y(0), "#.000000"); ","; FORMAT(y(1), "#.000000")

  REM ── 2) stesso LP con warm start (la sera prima: 55/5/40) ────────────────────
  DIM g0(2) AS DOUBLE, y0(1) AS DOUBLE
  g0(0)=55 : g0(1)=5 : g0(2)=40
  y0(0)=3.5 : y0(1)=-1.5
  PRINT "|lpw="; OPTIMIZE.IPMLPWARM(A, 2, 3, b, c, 1E-8, 1E-8, 1E-8, 200, g, y, z, g0, y0)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000"); ","; FORMAT(g(2), "#.000000")

  REM ── 3) lo stesso LP in forma sparsa (CSC per colonne) ───────────────────────
  DIM ap(3) AS LONG, ar(4) AS LONG, av(4) AS DOUBLE
  ap(0)=0 : ap(1)=2 : ap(2)=4 : ap(3)=5
  ar(0)=0 : ar(1)=1 : ar(2)=0 : ar(3)=1 : ar(4)=0
  av(0)=1 : av(1)=1 : av(2)=1 : av(3)=1 : av(4)=1
  PRINT "|lpc="; OPTIMIZE.IPMLPCSC(ap, ar, av, 2, 3, b, c, 1E-8, 1E-8, 1E-8, 200, g, y, z)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000"); ","; FORMAT(g(2), "#.000000")
  PRINT ":"; FORMAT(y(0), "#.000000"); ","; FORMAT(y(1), "#.000000")

  REM ── 4) QP: due generatori con costi quadratici ─────────────────────────────
  DIM aq(1) AS DOUBLE
  aq(0)=1 : aq(1)=1
  DIM Q(3) AS DOUBLE
  Q(0)=2 : Q(1)=0 : Q(2)=0 : Q(3)=4        ' Q = diag(2, 4)
  DIM bq(0) AS DOUBLE
  bq(0)=1
  DIM cq(1) AS DOUBLE
  cq(0)=0 : cq(1)=0
  DIM yq(0) AS DOUBLE
  PRINT "|qp="; OPTIMIZE.IPMQP(aq, Q, 1, 2, bq, cq, 1E-8, 1E-8, 1E-8, 200, g, yq, z)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000")
  PRINT ":"; FORMAT(yq(0), "#.000000")

  REM warm start del QP da (0.5, 0.5)
  DIM gq(1) AS DOUBLE, yq0(0) AS DOUBLE
  gq(0)=0.5 : gq(1)=0.5
  yq0(0)=1
  PRINT "|qpw="; OPTIMIZE.IPMQPWARM(aq, Q, 1, 2, bq, cq, 1E-8, 1E-8, 1E-8, 200, g, yq, z, gq, yq0)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000")

  REM QP sparso: A in CSC, Q (triangolo inferiore) in CSC
  DIM aqc(2) AS LONG, arq(1) AS LONG, avq(1) AS DOUBLE
  aqc(0)=0 : aqc(1)=1 : aqc(2)=2
  arq(0)=0 : arq(1)=0
  avq(0)=1 : avq(1)=1
  DIM qpc(2) AS LONG, qrc(1) AS LONG, qvc(1) AS DOUBLE
  qpc(0)=0 : qpc(1)=1 : qpc(2)=2
  qrc(0)=0 : qrc(1)=1
  qvc(0)=2 : qvc(1)=4
  PRINT "|qpc="; OPTIMIZE.IPMQPCSC(aqc, arq, avq, qpc, qrc, qvc, 1, 2, bq, cq, 1E-8, 1E-8, 1E-8, 200, g, yq, z)
  PRINT ":"; FORMAT(g(0), "#.000000"); ","; FORMAT(g(1), "#.000000")
  PRINT ":"; FORMAT(yq(0), "#.000000")

  REM ── 5) presolve del LP: riduzione, soluzione del ridotto, postsolve ─────────
  h = OPTIMIZE.PRESOLVENEW(ap, ar, av, b, c, 2, 3, 1E-9)
  IF h > 0 THEN
    PRINT "|pre=1"
  ELSE
    PRINT "|pre=0"
  END IF
  PRINT ":changed="; OPTIMIZE.PRESOLVECHANGED(h)
  DIM pinfo(2) AS LONG
  w = OPTIMIZE.PRESOLVEDIMS(h, pinfo)
  PRINT ":mr="; pinfo(0); ",nr="; pinfo(1); ",nnz="; pinfo(2)

  REM il ridotto si legge in CSC e si risolve con qualsiasi solver
  DIM rp(8) AS LONG, rr(32) AS LONG, rv(32) AS DOUBLE
  DIM rb(8) AS DOUBLE, rc(8) AS DOUBLE
  nr = OPTIMIZE.PRESOLVEREDUCED(h, rp, rr, rv, rb, rc)
  DIM xr(8) AS DOUBLE, yr(8) AS DOUBLE, xf(2) AS DOUBLE, yf(1) AS DOUBLE
  IF nr > 0 THEN
    REM ridotto non vuoto: densifica e risolve (caso generale)
    DIM Ad(63) AS DOUBLE
    FOR j = 0 TO nr - 1
      FOR k = rp(j) TO rp(j + 1) - 1
        Ad(rr(k) * nr + j) = rv(k)
      NEXT k
    NEXT j
    rc2 = OPTIMIZE.IPMLP(Ad, pinfo(0), nr, rb, rc, 1E-8, 1E-8, 1E-8, 200, xr, yr, xr)
    PRINT ":rsolve="; rc2
  ELSE
    REM ridotto vuoto: il presolve ha già determinato tutto
    PRINT ":rsolve=0"
  END IF
  PRINT ":post="; OPTIMIZE.PRESOLVEPOSTSOLVE(h, xr, yr, xf, yf)
  PRINT ":"; FORMAT(xf(0), "#.000000"); ","; FORMAT(xf(1), "#.000000"); ","; FORMAT(xf(2), "#.000000")
  PRINT ":"; FORMAT(yf(0), "#.000000"); ","; FORMAT(yf(1), "#.000000")

  REM direzione (raggio di Farkas): lifting omogeneo
  DIM rd(2) AS DOUBLE, yd(1) AS DOUBLE, rf(2) AS DOUBLE, ydf(1) AS DOUBLE
  PRINT ":pdir="; OPTIMIZE.PRESOLVEPOSTSOLVEDIR(h, rd, yd, rf, ydf)
  PRINT ":"; FORMAT(rf(0), "#.000000"); ","; FORMAT(rf(1), "#.000000"); ","; FORMAT(rf(2), "#.000000")
  PRINT ":"; OPTIMIZE.PRESOLVEFREE(h)

  REM ── 6) SOCP: margine robusto su una previsione di 30 MW ─────────────────────
  REM min t  s.t. g = 30, (t, g) in SOC  ->  t = 30
  DIM E(1) AS DOUBLE, d(0) AS DOUBLE, cs(1) AS DOUBLE
  E(0)=0 : E(1)=1
  d(0)=30
  cs(0)=1 : cs(1)=0
  DIM ctype(0) AS LONG, cnm(0) AS LONG, cmem(1) AS LONG
  ctype(0)=1 : cnm(0)=2 : cmem(0)=0 : cmem(1)=1
  DIM Gs(3) AS DOUBLE, hSoc(1) AS DOUBLE
  Gs(0)=1 : Gs(1)=0 : Gs(2)=0 : Gs(3)=1
  hSoc(0)=0 : hSoc(1)=0
  DIM xs(1) AS DOUBLE, ys(0) AS DOUBLE, lam(1) AS DOUBLE
  PRINT "|socp="; OPTIMIZE.SOCPSOLVE(2, 1, E, d, cs, 1, ctype, cnm, cmem, Gs, hSoc, 1E-8, 1E-8, 200, xs, ys, lam)
  PRINT ":"; FORMAT(xs(0), "#.000000"); ","; FORMAT(xs(1), "#.000000")
  PRINT ":"; FORMAT(ys(0), "#.000000")
  PRINT ":"; FORMAT(lam(0), "#.000000"); ","; FORMAT(lam(1), "#.000000")

  REM stessa problema con fattorizzazione sparsa
  DIM xs2(1) AS DOUBLE, ys2(0) AS DOUBLE, lam2(1) AS DOUBLE
  PRINT "|socpsp="; OPTIMIZE.SOCPSOLVESP(2, 1, E, d, cs, 1, ctype, cnm, cmem, Gs, hSoc, 1E-8, 1E-8, 200, xs2, ys2, lam2)
  PRINT ":"; FORMAT(xs2(0), "#.000000"); ","; FORMAT(xs2(1), "#.000000")
  PRINT ":"; FORMAT(lam2(0), "#.000000"); ","; FORMAT(lam2(1), "#.000000")

  REM ── 7) SDP: allocazione su due asset, PSD 2x2 ─────────────────────────────
  DIM zA(0) AS DOUBLE, zC(0) AS DOUBLE, dL(0) AS LONG, dD(0) AS DOUBLE
  DIM bP(0) AS DOUBLE
  bP(0)=2
  DIM dims(0) AS LONG
  dims(0)=2
  DIM Cbar(3) AS DOUBLE, Abar(3) AS DOUBLE
  Cbar(0)=1 : Cbar(1)=0 : Cbar(2)=0 : Cbar(3)=2
  Abar(0)=1 : Abar(1)=0 : Abar(2)=0 : Abar(3)=1
  DIM Xb(3) AS DOUBLE, Sb(3) AS DOUBLE, yP(0) AS DOUBLE, fb(0) AS LONG
  PRINT "|sdp="; OPTIMIZE.SDPSOLVE(1, 0, zA, bP, zC, 1, dims, Cbar, Abar, 0, dL, dD, dD, 0, dL, dD, dD, dD, 300, 1E-8, 1E-7, 1E-7, 1E-7, 1000, zA, Xb, yP, Sb, dD, dD, dD, dD, fb)
  PRINT ":"; FORMAT(yP(0), "#.000000")
  PRINT ":"; FORMAT(Xb(0), "#.000000"); ","; FORMAT(Xb(1), "#.000000"); ","; FORMAT(Xb(2), "#.000000"); ","; FORMAT(Xb(3), "#.000000")
  PRINT ":"; FORMAT(Sb(0), "#.000000"); ","; FORMAT(Sb(1), "#.000000"); ","; FORMAT(Sb(2), "#.000000"); ","; FORMAT(Sb(3), "#.000000")
  PRINT ":"; fb(0)

  REM blocco SOC nello stesso modello: z0 = 1, z in SOC -> z = (1, 0)
  DIM bS(0) AS DOUBLE
  bS(0)=1
  DIM sd(0) AS LONG
  sd(0)=2
  DIM Csoc(1) AS DOUBLE, Asoc(1) AS DOUBLE
  Csoc(0)=1 : Csoc(1)=0
  Asoc(0)=1 : Asoc(1)=0
  DIM Zs(1) AS DOUBLE, Ss(1) AS DOUBLE
  PRINT "|sdpsoc="; OPTIMIZE.SDPSOLVE(1, 0, zA, bS, zC, 0, dL, dD, dD, 1, sd, Csoc, Asoc, 0, dL, dD, dD, dD, 300, 1E-8, 1E-7, 1E-7, 1E-7, 1000, zA, dD, yP, dD, Zs, Ss, dD, dD, fb)
  PRINT ":"; FORMAT(yP(0), "#.000000")
  PRINT ":"; FORMAT(Zs(0), "#.000000"); ","; FORMAT(Zs(1), "#.000000")
  PRINT ":"; FORMAT(Ss(0), "#.000000"); ","; FORMAT(Ss(1), "#.000000")

  REM blocco esponenziale: min e0 s.t. e1=1, e2=0, e in PEXP -> e=(1,1,0)
  DIM bE(1) AS DOUBLE
  bE(0)=1 : bE(1)=0
  DIM ek(0) AS LONG, ea(0) AS DOUBLE
  ek(0)=0 : ea(0)=0
  DIM Cexp(2) AS DOUBLE, Aexp(5) AS DOUBLE
  Cexp(0)=1 : Cexp(1)=0 : Cexp(2)=0
  Aexp(0)=0 : Aexp(1)=1 : Aexp(2)=0
  Aexp(3)=0 : Aexp(4)=0 : Aexp(5)=1
  DIM Ze(2) AS DOUBLE, Se(2) AS DOUBLE, yE(1) AS DOUBLE
  PRINT "|sdpexp="; OPTIMIZE.SDPSOLVE(2, 0, zA, bE, zC, 0, dL, dD, dD, 0, dL, dD, dD, 1, ek, ea, Cexp, Aexp, 300, 1E-8, 1E-7, 1E-7, 1E-7, 1000, zA, dD, yE, dD, dD, dD, Ze, Se, fb)
  PRINT ":"; FORMAT(yE(0), "#.000000"); ","; FORMAT(yE(1), "#.000000")
  PRINT ":"; FORMAT(Ze(0), "#.000000"); ","; FORMAT(Ze(1), "#.000000"); ","; FORMAT(Ze(2), "#.000000")
  PRINT ":"; FORMAT(Se(0), "#.000000"); ","; FORMAT(Se(1), "#.000000"); ","; FORMAT(Se(2), "#.000000")

  REM ── 8) forma standard: LP con limiti superiori, risolto e riportato ────────
  DIM ci(2) AS DOUBLE
  ci(0)=2 : ci(1)=3 : ci(2)=4
  DIM lx(2) AS DOUBLE, ux(2) AS DOUBLE
  lx(0)=0 : lx(1)=0 : lx(2)=0
  ux(0)=70 : ux(1)=70 : ux(2)=70
  DIM lc(1) AS DOUBLE, uc(1) AS DOUBLE
  lc(0)=100 : uc(0)=100      REM g1+g2+g3 = 100
  lc(1)=60  : uc(1)=60       REM g1+g2 = 60
  DIM dqs(0) AS LONG, dqd(0) AS DOUBLE
  hSf = OPTIMIZE.STDFORMBUILD(3, 2, ci, dqs, dqs, dqd, 0, lx, ux, lc, uc, ap, ar, av)
  IF hSf > 0 THEN
    PRINT "|sf=1"
  ELSE
    PRINT "|sf=0"
  END IF
  DIM sinfo(4) AS DOUBLE
  PRINT ":info="; OPTIMIZE.STDFORMINFO(hSf, sinfo)
  PRINT ":"; FORMAT(sinfo(0), "#.0"); ","; FORMAT(sinfo(1), "#.0"); ","; FORMAT(sinfo(2), "#.0"); ","; FORMAT(sinfo(3), "#.0"); ","; FORMAT(sinfo(4), "#.0")
  REM A/b/c del problema standard (m=5 righe, n=8 colonne) e soluzione
  DIM sfA(39) AS DOUBLE, sfb(4) AS DOUBLE, sfc(7) AS DOUBLE
  PRINT ":a="; OPTIMIZE.STDFORMGETA(hSf, sfA)
  PRINT ":b="; OPTIMIZE.STDFORMGETB(hSf, sfb)
  PRINT ":c="; OPTIMIZE.STDFORMGETC(hSf, sfc)
  DIM sxt(7) AS DOUBLE, syt(4) AS DOUBLE, szt(7) AS DOUBLE
  rc3 = OPTIMIZE.IPMLP(sfA, 5, 8, sfb, sfc, 1E-8, 1E-8, 1E-8, 200, sxt, syt, szt)
  PRINT ":solve="; rc3
  DIM sxo(2) AS DOUBLE, syo(1) AS DOUBLE
  PRINT ":mapx="; OPTIMIZE.STDFORMMAPX(hSf, sxt, sxo)
  PRINT ":"; FORMAT(sxo(0), "#.000000"); ","; FORMAT(sxo(1), "#.000000"); ","; FORMAT(sxo(2), "#.000000")
  PRINT ":mapy="; OPTIMIZE.STDFORMMAPY(hSf, syt, syo)
  PRINT ":"; FORMAT(syo(0), "#.000000"); ","; FORMAT(syo(1), "#.000000")
  DIM sxd(7) AS DOUBLE, sxdo(2) AS DOUBLE
  sxd(0)=1 : sxd(1)=0 : sxd(2)=0 : sxd(3)=0 : sxd(4)=0 : sxd(5)=0 : sxd(6)=0 : sxd(7)=0
  PRINT ":mapd="; OPTIMIZE.STDFORMMAPDIR(hSf, sxd, sxdo)
  PRINT ":"; FORMAT(sxdo(0), "#.000000"); ","; FORMAT(sxdo(1), "#.000000"); ","; FORMAT(sxdo(2), "#.000000")
  REM Q assente su un LP -> -1
  DIM Qd(35) AS DOUBLE
  PRINT ":q="; OPTIMIZE.STDFORMGETQ(hSf, Qd)
  PRINT ":free="; OPTIMIZE.STDFORMFREE(hSf)
  PRINT "|f=0"
END SUB
