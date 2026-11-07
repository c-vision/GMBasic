' FILE: Xform/XformWorkbench.bas
' SCENARIO DIDATTICO — un pannello solare montato su una torre: si costruisce la
' trasformazione che porta il pannello dal suo spazio locale al mondo (rotazione di
' azimuth, inclinazione, traslazione), si trasformano i quattro angoli del pannello,
' si porta un punto del mondo indietro nello spazio locale con l'inversa affine e si
' mostra perché le normali vanno trasformate con la matrice normale.
'
'   pannello locale: rettangolo 4 x 2 nel piano XY (z = 0), normale +Z
'   azimuth        : 30° attorno a Z
'   inclinazione   : 20° attorno a X
'   posizione      : (5, 0, 2) sulla torre
'   scala          : non uniforme (2, 1, 1) per mostrare l'effetto sulle normali
'
' Convenzioni: matrici piatte ROW-MAJOR, 9 double per la 3x3 e 16 per la 4x4
' (m(riga*4 + colonna)); la traslazione sta in m(3), m(7), m(11).
' Domini: XFORM. Output deterministico.
IMPORT XFORM

CONST D2R = 0.017453292519943295

SUB Main()
  DIM ident3(8) AS DOUBLE
  DIM rotA(8) AS DOUBLE          ' azimuth 3x3
  DIM rotB(8) AS DOUBLE          ' inclinazione 3x3
  DIM comp3(8) AS DOUBLE         ' composizione 3x3
  DIM norm3(8) AS DOUBLE         ' matrice normale
  DIM back3(8) AS DOUBLE
  DIM id4(15) AS DOUBLE
  DIM rz(15) AS DOUBLE
  DIM rx(15) AS DOUBLE
  DIM tr(15) AS DOUBLE
  DIM sc(15) AS DOUBLE
  DIM m(15) AS DOUBLE            ' trasformazione locale -> mondo
  DIM tmp(15) AS DOUBLE
  DIM inv(15) AS DOUBLE
  DIM p(2) AS DOUBLE
  DIM f4(15) AS DOUBLE
  DIM i AS LONG

  ' ── 1. rotazioni 3x3: azimuth e inclinazione ───────────────────────────────
  PRINT "azimuth 3x3 = "; XFORM.MAT3ROTZ(30 * D2R, rotA)
  PRINT " inclinazione 3x3 = "; XFORM.MAT3ROTX(20 * D2R, rotB)
  ' composizione (prima inclinazione, poi azimuth)
  PRINT " composizione = "; XFORM.MAT3MUL(rotA, rotB, comp3)
  PRINT " r00 = "; FORMAT(comp3(0), "#.000000"); "  r01 = "; FORMAT(comp3(1), "#.000000")
  PRINT " r10 = "; FORMAT(comp3(3), "#.000000"); "  r11 = "; FORMAT(comp3(4), "#.000000")
  PRINT " r22 = "; FORMAT(comp3(8), "#.000000")
  ' la composizione di rotazioni è una rotazione: determinante 1 (traccia = 1 + 2cos)
  PRINT " identità 3x3 = "; XFORM.MAT3IDENTITY(ident3)
  PRINT " traccia = "; FORMAT(comp3(0) + comp3(4) + comp3(8), "#.000000")

  ' ── 2. la trasformazione completa locale -> mondo ──────────────────────────
  ' M = T(5,0,2) * Rz(30°) * Rx(20°) * S(2,1,1): prima la scala, poi i due assi,
  ' infine la traslazione (ogni MUL applica il secondo argomento per primo).
  PRINT "identità 4x4 = "; XFORM.MAT4IDENTITY(id4)
  PRINT " scala   = "; XFORM.MAT4SCALE(2, 1, 1, sc)
  PRINT " azimuth = "; XFORM.MAT4ROTZ(30 * D2R, rz)
  PRINT " tilt    = "; XFORM.MAT4ROTX(20 * D2R, rx)
  PRINT " torre   = "; XFORM.MAT4TRANSLATE(5, 0, 2, tr)
  PRINT " Rz*Rx   = "; XFORM.MAT4MUL(rz, rx, tmp)
  PRINT " *S      = "; XFORM.MAT4MUL(tmp, sc, m)
  PRINT " *T      = "; XFORM.MAT4MUL(tr, m, tmp)
  PRINT " M       = "; FORMAT(tmp(0), "#.000"); " "; FORMAT(tmp(1), "#.000"); " "; FORMAT(tmp(2), "#.000"); " trasl = (";
  PRINT FORMAT(tmp(3), "#.000"); ", "; FORMAT(tmp(7), "#.000"); ", "; FORMAT(tmp(11), "#.000"); ")"

  ' ── 3. i quattro angoli del pannello nel mondo ─────────────────────────────
  ' locale: (-2,-1), (2,-1), (2,1), (-2,1) nel piano z = 0
  PRINT "angolo 0 = "; XFORM.MAT4POINT(tmp, -2, -1, 0, p)
  PRINT " -> "; FORMAT(p(0), "#.000"); ", "; FORMAT(p(1), "#.000"); ", "; FORMAT(p(2), "#.000")
  PRINT "angolo 1 = "; XFORM.MAT4POINT(tmp, 2, -1, 0, p)
  PRINT " -> "; FORMAT(p(0), "#.000"); ", "; FORMAT(p(1), "#.000"); ", "; FORMAT(p(2), "#.000")
  PRINT "angolo 2 = "; XFORM.MAT4POINT(tmp, 2, 1, 0, p)
  PRINT " -> "; FORMAT(p(0), "#.000"); ", "; FORMAT(p(1), "#.000"); ", "; FORMAT(p(2), "#.000")
  PRINT "angolo 3 = "; XFORM.MAT4POINT(tmp, -2, 1, 0, p)
  PRINT " -> "; FORMAT(p(0), "#.000"); ", "; FORMAT(p(1), "#.000"); ", "; FORMAT(p(2), "#.000")

  ' ── 4. punto contro direzione: la traslazione conta solo per i punti ───────
  ' la normale locale +Z: come direzione non subisce la traslazione
  PRINT "normale (dir) = "; XFORM.MAT4DIR(tmp, 0, 0, 1, p)
  PRINT " -> "; FORMAT(p(0), "#.000"); ", "; FORMAT(p(1), "#.000"); ", "; FORMAT(p(2), "#.000")

  ' ── 5. perché serve la matrice normale ─────────────────────────────────────
  ' con scala non uniforme la rotazione 3x3 non conserva gli angoli: la normale
  ' corretta è (M^-1)^T, non la sottomatrice 3x3 di M.
  PRINT "rot 3x3 = "; XFORM.MAT3FROM4(tmp, back3)
  PRINT " normale = "; XFORM.MAT3NORMAL(back3, norm3)
  PRINT " n da rot  = "; FORMAT(back3(0), "#.000000"); ", "; FORMAT(back3(3), "#.000000"); ", "; FORMAT(back3(6), "#.000000")
  PRINT " n corretta= "; FORMAT(norm3(0), "#.000000"); ", "; FORMAT(norm3(3), "#.000000"); ", "; FORMAT(norm3(6), "#.000000"); " (la normale usa S^-1: 0.5 al posto di 2)"

  ' ── 6. inversa affine: dal mondo allo spazio locale ────────────────────────
  PRINT "inversa = "; XFORM.MAT4INVERSEAFFINE(tmp, inv)
  ' il centro del pannello è (0,0,0) locale: nel mondo è la traslazione
  PRINT "centro nel mondo = "; FORMAT(tmp(3), "#.000"); ", "; FORMAT(tmp(7), "#.000"); ", "; FORMAT(tmp(11), "#.000")
  PRINT "centro in locale = "; XFORM.MAT4POINT(inv, tmp(3), tmp(7), tmp(11), p)
  PRINT " -> "; FORMAT(p(0), "#.000000"); ", "; FORMAT(p(1), "#.000000"); ", "; FORMAT(p(2), "#.000000")

  ' ── 7. conversioni 3x3 <-> 4x4 ─────────────────────────────────────────────
  PRINT "4x4 da 3x3 = "; XFORM.MAT4FROM3(comp3, f4)
  PRINT " trasl nulla = "; FORMAT(f4(3), "#.0"); ", "; FORMAT(f4(7), "#.0"); ", "; FORMAT(f4(11), "#.0")
  PRINT "3x3 da 4x4 = "; XFORM.MAT3FROM4(f4, rotA)
  PRINT " r00/r11 = "; FORMAT(rotA(0), "#.000000"); " / "; FORMAT(rotA(4), "#.000000")

  ' ── 8. guardie ────────────────────────────────────────────────────────────
  DIM corto(2) AS DOUBLE
  PRINT "array corto = "; XFORM.MAT3ROTZ(1, corto); ", "; XFORM.MAT4IDENTITY(corto)
  ' matrice singolare: prima colonna nulla
  PRINT "singolare 4x4 = "; XFORM.MAT4IDENTITY(sc)
  sc(0) = 0
  PRINT ": "; XFORM.MAT4INVERSEAFFINE(sc, inv)
  ' matrice 3x3 nulla -> matrice normale impossibile
  PRINT "singolare 3x3 = ";
  FOR i = 0 TO 8
    rotA(i) = 0
  NEXT i
  PRINT XFORM.MAT3NORMAL(rotA, norm3)
  PRINT "fine"
END SUB
