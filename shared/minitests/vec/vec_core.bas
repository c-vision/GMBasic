' VEC (FFI) — 11 operazioni vettoriali che VECTOR non ha, su valori calcolabili a mano.
' Vettori = array di 3 double (2 per PERP2D).
' EXPECT: proj= 0|:1.0,0.0,0.0||rej= 0|:0.0,2.0,3.0||proj2= 0|:1.0,0.0,0.0||proj0= 0|:0.0,0.0,0.0||sch1= 0|:0.040000||sch0= 0|:1.000000||sch05= 0|:0.070000||bias= 0|:1.0,2.0,0.0000003||clamp= 0|:1.0,1.5,1.5||clamp01= 0|:0.0,0.5,1.0||mag= 0|:1.200000,1.600000,0.0||mag2= 0|:3.0,4.0||abs= 0|:1.0,2.0,3.0||sqrt= 0|:2.0,3.0,4.0||pow= 0|:1.0,4.0,9.0||perp= 0|:-4.0,3.0||guard=-1|:-1|:-1||f=0
IMPORT VEC

SUB Main()
  DIM v(2) AS DOUBLE
  DIM dirv(2) AS DOUBLE
  DIM o(2) AS DOUBLE
  DIM f0(2) AS DOUBLE
  DIM nrm(2) AS DOUBLE
  DIM p2(1) AS DOUBLE

  ' ── proiezione e componente perpendicolare ────────────────────────────────
  v(0) = 1 : v(1) = 2 : v(2) = 3
  dirv(0) = 1 : dirv(1) = 0 : dirv(2) = 0
  PRINT "proj="; VEC.PROJECT(v, dirv, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  PRINT "|rej="; VEC.REJECT(v, dirv, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  ' onto non normalizzato: la proiezione non cambia
  dirv(0) = 2
  PRINT "|proj2="; VEC.PROJECT(v, dirv, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  ' onto nullo: vettore zero (nessuna divisione per zero)
  dirv(0) = 0
  PRINT "|proj0="; VEC.PROJECT(v, dirv, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")

  ' ── Fresnel di Schlick ────────────────────────────────────────────────────
  f0(0) = 0.04 : f0(1) = 0.04 : f0(2) = 0.04
  PRINT "|sch1="; VEC.SCHLICK(f0, 1, o)
  PRINT ":"; FORMAT(o(0), "#.000000")
  PRINT "|sch0="; VEC.SCHLICK(f0, 0, o)
  PRINT ":"; FORMAT(o(0), "#.000000")
  PRINT "|sch05="; VEC.SCHLICK(f0, 0.5, o)
  PRINT ":"; FORMAT(o(0), "#.000000")

  ' ── bias lungo la normale ─────────────────────────────────────────────────
  ' |v| massima = 3 -> eps = 3e-7; la componente z diventa 3 + 3e-7
  nrm(0) = 0 : nrm(1) = 0 : nrm(2) = 1
  PRINT "|bias="; VEC.BIAS(v, nrm, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2) - 3, "#.0000000")

  ' ── clamp ─────────────────────────────────────────────────────────────────
  v(0) = 1 : v(1) = 2 : v(2) = 3
  PRINT "|clamp="; VEC.VCLAMP(v, 0, 1.5, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  v(0) = -1 : v(1) = 0.5 : v(2) = 2
  PRINT "|clamp01="; VEC.VCLAMP01(v, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  ' modulo 5 limitato a 2: scala 2/5 -> (1.2, 1.6, 0)
  v(0) = 3 : v(1) = 4 : v(2) = 0
  PRINT "|mag="; VEC.VCLAMPMAG(v, 2, o)
  PRINT ":"; FORMAT(o(0), "#.000000"); ","; FORMAT(o(1), "#.000000"); ","; FORMAT(o(2), "#.0")
  ' già sotto il limite: invariato
  PRINT "|mag2="; VEC.VCLAMPMAG(v, 10, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0")

  ' ── per componente ────────────────────────────────────────────────────────
  v(0) = -1 : v(1) = 2 : v(2) = -3
  PRINT "|abs="; VEC.VABS(v, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  v(0) = 4 : v(1) = 9 : v(2) = 16
  PRINT "|sqrt="; VEC.VSQRT(v, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")
  v(0) = 1 : v(1) = 2 : v(2) = 3
  PRINT "|pow="; VEC.VPOW(v, 2, o)
  PRINT ":"; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0")

  ' ── perpendicolare 2D ─────────────────────────────────────────────────────
  PRINT "|perp="; VEC.PERP2D(3, 4, p2)
  PRINT ":"; FORMAT(p2(0), "#.0"); ","; FORMAT(p2(1), "#.0")

  ' ── guardie: array troppo corto ───────────────────────────────────────────
  DIM corto(1) AS DOUBLE
  corto(0) = 1 : corto(1) = 2
  PRINT "|guard="; VEC.PROJECT(corto, corto, corto)
  PRINT ":"; VEC.VCLAMP01(corto, corto)
  PRINT ":"; VEC.VABS(corto, corto)
  PRINT "|f=0"
END SUB
