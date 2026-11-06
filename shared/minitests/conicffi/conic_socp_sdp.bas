' FILE: conic_socp_sdp.bas
' OPTIMIZE (FFI conic) — SOCP (denso e sparso) e SDP con blocchi PSD, SOC ed
' esponenziale.
'   SOCP  min t  s.t. x=1, (t,x) in SOC     -> t=1, x=1, y=-1, lam=(1,-1)
'   SDP   min X11+2X22  s.t. tr(X)=2, X>=0  -> X=[[2,0],[0,0]], S=[[0,0],[0,1]], y=1
'   SDP+SOC  min z0  s.t. z0=1, z in SOC    -> z=(1,0), y=1
'   SDP+EXP  min e0  s.t. e1=1, e2=0, e in PEXP -> e=(1,1,0), y=(1,1), s=(1,-1,-1)
' EXPECT: socp= 0|:1.000000,1.000000|:-1.000000|:1.000000,-1.000000||socpsp= 0|:1.000000,1.000000|:1.000000,-1.000000||sdp= 0|:1.000000|:2.000000,0.000000,0.000000,0.000000|:0.000000,0.000000,0.000000,1.000000|: 0||sdpsoc= 0|:1.000000|:1.000000,0.000000|:0.000000,0.000000||sdpexp= 0|:1.000000,1.000000|:1.000000,1.000000,0.000000|:1.000000,-1.000000,-1.000000||bad=-1|:-1|:-1||f=0
IMPORT OPTIMIZE

SUB Main()
  ' ── SOCP ────────────────────────────────────────────────────────────────
  DIM E(1) AS DOUBLE, d(0) AS DOUBLE, cS(1) AS DOUBLE
  E(0)=0 : E(1)=1
  d(0)=1
  cS(0)=1 : cS(1)=0
  DIM ctype(0) AS LONG, cnm(0) AS LONG, cmem(1) AS LONG
  ctype(0)=1 : cnm(0)=2 : cmem(0)=0 : cmem(1)=1
  DIM G(3) AS DOUBLE, hh(1) AS DOUBLE
  G(0)=1 : G(1)=0 : G(2)=0 : G(3)=1
  hh(0)=0 : hh(1)=0
  DIM xs(1) AS DOUBLE, ys(0) AS DOUBLE, lam(1) AS DOUBLE
  PRINT "socp="; OPTIMIZE.SOCPSOLVE(2, 1, E, d, cS, 1, ctype, cnm, cmem, G, hh, 1E-8, 1E-8, 200, xs, ys, lam)
  PRINT ":"; FORMAT(xs(0), "#.000000"); ","; FORMAT(xs(1), "#.000000")
  PRINT ":"; FORMAT(ys(0), "#.000000")
  PRINT ":"; FORMAT(lam(0), "#.000000"); ","; FORMAT(lam(1), "#.000000")

  ' ── SOCP con fattorizzazione sparsa: stessi risultati ───────────────────
  DIM xs2(1) AS DOUBLE, ys2(0) AS DOUBLE, lam2(1) AS DOUBLE
  PRINT "|socpsp="; OPTIMIZE.SOCPSOLVESP(2, 1, E, d, cS, 1, ctype, cnm, cmem, G, hh, 1E-8, 1E-8, 200, xs2, ys2, lam2)
  PRINT ":"; FORMAT(xs2(0), "#.000000"); ","; FORMAT(xs2(1), "#.000000")
  PRINT ":"; FORMAT(lam2(0), "#.000000"); ","; FORMAT(lam2(1), "#.000000")

  ' ── SDP: blocco PSD 2x2 ─────────────────────────────────────────────────
  DIM zA(0) AS DOUBLE, zC(0) AS DOUBLE
  DIM bP(0) AS DOUBLE
  bP(0)=2
  DIM dims(0) AS LONG
  dims(0)=2
  DIM Cbar(3) AS DOUBLE, Abar(3) AS DOUBLE
  Cbar(0)=1 : Cbar(1)=0 : Cbar(2)=0 : Cbar(3)=2
  Abar(0)=1 : Abar(1)=0 : Abar(2)=0 : Abar(3)=1
  DIM Xb(3) AS DOUBLE, Sb(3) AS DOUBLE, yP(0) AS DOUBLE
  DIM dL(0) AS LONG, dD(0) AS DOUBLE, fb(0) AS LONG
  PRINT "|sdp="; OPTIMIZE.SDPSOLVE(1, 0, zA, bP, zC, 1, dims, Cbar, Abar, 0, dL, dD, dD, 0, dL, dD, dD, dD, 300, 1E-8, 1E-7, 1E-7, 1E-7, 1000, zA, Xb, yP, Sb, dD, dD, dD, dD, fb)
  PRINT ":"; FORMAT(yP(0), "#.000000")
  PRINT ":"; FORMAT(Xb(0), "#.000000"); ","; FORMAT(Xb(1), "#.000000"); ","; FORMAT(Xb(2), "#.000000"); ","; FORMAT(Xb(3), "#.000000")
  PRINT ":"; FORMAT(Sb(0), "#.000000"); ","; FORMAT(Sb(1), "#.000000"); ","; FORMAT(Sb(2), "#.000000"); ","; FORMAT(Sb(3), "#.000000")
  PRINT ":"; fb(0)

  ' ── SDP con blocco SOC: min z0 s.t. z0=1, z in SOC ──────────────────────
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

  ' ── SDP con blocco esponenziale: min e0 s.t. e1=1, e2=0, e in PEXP ─────
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

  ' ── errori: handle/argomenti non validi -> -1 ───────────────────────────
  PRINT "|bad="; OPTIMIZE.SOCPSOLVE(2, 1, E, d, cS, 1, dL, dL, dL, G, hh, 1E-8, 1E-8, 10, xs, ys, lam)
  PRINT ":"; OPTIMIZE.PRESOLVEFREE(9999)
  PRINT ":"; OPTIMIZE.STDFORMFREE(9999)
  PRINT "|f=0"
END SUB
