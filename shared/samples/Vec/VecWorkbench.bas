' FILE: Vec/VecWorkbench.bas
' SCENARIO DIDATTICO — una palla che rimbalza su un piano inclinato, con
' l'illuminazione della superficie: le due cose che una scena 3D fa di continuo.
'
'   A) rimbalzo: la velocità si scompone in componente lungo la normale
'      (PROJECT) e tangenziale (REJECT); con il coefficiente di restituzione si
'      ricompone la velocità dopo l'urto
'   B) limiti: velocità massima (VCLAMPMAG), colore in [0,1] (VCLAMP01),
'      norma per componente (VCLAMP)
'   C) illuminazione: riflessione di Fresnel (SCHLICK) e bias del raggio
'      (BIAS) per non intersecare la superficie che lo ha generato
'   D) utilità: valore assoluto, radice (deviazione standard da una varianza)
'      e gamma (VPOW) su un colore
'   E) strada 2D: perpendicolare alla direzione di marcia (PERP2D)
'
' Vettori = array di 3 double (2 per PERP2D). Domini: VEC. Output deterministico.
IMPORT VEC

SUB Main()
  DIM v(2) AS DOUBLE
  DIM n(2) AS DOUBLE
  DIM pr(2) AS DOUBLE
  DIM tg(2) AS DOUBLE
  DIM o(2) AS DOUBLE
  DIM f0(2) AS DOUBLE
  DIM p2(1) AS DOUBLE
  DIM e AS DOUBLE

  ' ══════════════════════════════════════════════════════════════════════════
  ' A) RIMBALZO su un piano orizzontale (normale +Y)
  ' ══════════════════════════════════════════════════════════════════════════
  v(0) = 3 : v(1) = -2 : v(2) = 1        ' velocità in arrivo
  n(0) = 0 : n(1) = 1 : n(2) = 0         ' normale del piano
  e = 0.6                                 ' coefficiente di restituzione
  PRINT "palla: v = (3, -2, 1), piano con normale +Y, e = "; e

  PRINT " componente normale = "; VEC.PROJECT(v, n, pr)
  PRINT " -> ("; FORMAT(pr(0), "#.0"); ", "; FORMAT(pr(1), "#.0"); ", "; FORMAT(pr(2), "#.0"); ")"
  PRINT " componente tangente = "; VEC.REJECT(v, n, tg)
  PRINT " -> ("; FORMAT(tg(0), "#.0"); ", "; FORMAT(tg(1), "#.0"); ", "; FORMAT(tg(2), "#.0"); ")"
  ' dopo l'urto: la tangente resta, la normale si inverte e perde (1-e)
  pr(1) = -pr(1) * e
  o(0) = tg(0) + pr(0)
  o(1) = tg(1) + pr(1)
  o(2) = tg(2) + pr(2)
  PRINT " velocita' dopo l'urto = ("; FORMAT(o(0), "#.0"); ", "; FORMAT(o(1), "#.0"); ", "; FORMAT(o(2), "#.0"); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' B) LIMITI
  ' ══════════════════════════════════════════════════════════════════════════
  v(0) = 30 : v(1) = 40 : v(2) = 0
  PRINT "|velocita' (30,40,0) limitata a 10 = "; VEC.VCLAMPMAG(v, 10, o)
  PRINT " -> ("; FORMAT(o(0), "#.000000"); ", "; FORMAT(o(1), "#.000000"); ", "; FORMAT(o(2), "#.0"); ")"
  v(0) = 1.4 : v(1) = 0.5 : v(2) = -0.2
  PRINT "|colore HDR (1.4, 0.5, -0.2) in [0,1] = "; VEC.VCLAMP01(v, o)
  PRINT " -> ("; FORMAT(o(0), "#.000000"); ", "; FORMAT(o(1), "#.000000"); ", "; FORMAT(o(2), "#.000000"); ")"
  v(0) = 1 : v(1) = 2 : v(2) = 3
  PRINT "|clamp per componente in [0, 1.5] = "; VEC.VCLAMP(v, 0, 1.5, o)
  PRINT " -> ("; FORMAT(o(0), "#.000000"); ", "; FORMAT(o(1), "#.000000"); ", "; FORMAT(o(2), "#.000000"); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' C) ILLUMINAZIONE: Fresnel e bias del raggio
  ' ══════════════════════════════════════════════════════════════════════════
  f0(0) = 0.02 : f0(1) = 0.02 : f0(2) = 0.02     ' acqua: riflettanza normale
  PRINT "acqua: Fresnel a incidenza normale (u=1) = "; VEC.SCHLICK(f0, 1, o)
  PRINT " -> "; FORMAT(o(0), "#.000000")
  PRINT "       a 60 gradi dalla normale (u=0.5)  = "; VEC.SCHLICK(f0, 0.5, o)
  PRINT " -> "; FORMAT(o(0), "#.000000")
  PRINT "       radente (u=0.1)                   = "; VEC.SCHLICK(f0, 0.1, o)
  PRINT " -> "; FORMAT(o(0), "#.000000")
  ' bias: punto sul piano + normale, eps = |max componente| * 1e-7
  v(0) = 1 : v(1) = 2 : v(2) = 3
  n(0) = 0 : n(1) = 0 : n(2) = 1
  PRINT "bias del punto (1,2,3) lungo +Z = "; VEC.BIAS(v, n, o)
  PRINT " -> scostamento z = "; FORMAT(o(2) - 3, "#.0000000")

  ' ══════════════════════════════════════════════════════════════════════════
  ' D) VALORE ASSOLUTO, RADICE, GAMMA
  ' ══════════════════════════════════════════════════════════════════════════
  v(0) = -1 : v(1) = 2 : v(2) = -3
  PRINT "abs(-1, 2, -3) = "; VEC.VABS(v, o)
  PRINT " -> ("; FORMAT(o(0), "#.0"); ", "; FORMAT(o(1), "#.0"); ", "; FORMAT(o(2), "#.0"); ")"
  ' deviazione standard per componente da una varianza
  v(0) = 4 : v(1) = 9 : v(2) = 16
  PRINT "radice di (4, 9, 16) = "; VEC.VSQRT(v, o)
  PRINT " -> ("; FORMAT(o(0), "#.0"); ", "; FORMAT(o(1), "#.0"); ", "; FORMAT(o(2), "#.0"); ")"
  ' gamma 2.2: colore lineare (0.25, 0.5, 1) -> sRGB
  v(0) = 0.25 : v(1) = 0.5 : v(2) = 1
  PRINT "gamma 1/2.2 di (0.25, 0.5, 1) = "; VEC.VPOW(v, 1 / 2.2, o)
  PRINT " -> ("; FORMAT(o(0), "#.000000"); ", "; FORMAT(o(1), "#.000000"); ", "; FORMAT(o(2), "#.000000"); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' E) STRADA 2D: perpendicolare alla direzione di marcia
  ' ══════════════════════════════════════════════════════════════════════════
  PRINT "direzione (3, 4) -> perpendicolare = "; VEC.PERP2D(3, 4, p2)
  PRINT " ("; FORMAT(p2(0), "#.0"); ", "; FORMAT(p2(1), "#.0"); ")"

  ' ══════════════════════════════════════════════════════════════════════════
  ' F) guardie: un array da 2 non basta per un vettore 3D
  ' ══════════════════════════════════════════════════════════════════════════
  DIM corto(1) AS DOUBLE
  corto(0) = 1 : corto(1) = 2
  PRINT "guardia (tre componenti in due celle) = "; VEC.PROJECT(corto, corto, corto)
  PRINT "fine"
END SUB
