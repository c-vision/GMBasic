' FILE: Quat/QuatOrientation.bas
' SCENARIO DIDATTICO — assetto di un drone: il pilota comanda imbardata, beccheggio
' e rollio; il programma compone la rotazione, la applica agli assi del corpo,
' interpola fra due assetti (animazione di virata) e converte nella matrice 3x3 che
' serve al motore grafico. Tutte le 19 funzioni di QUAT sono usate su numeri
' verificabili a mano.
'
' Convenzione: quaternione (x, y, z, w) con (x,y,z) parte vettoriale e w scalare;
' le rotazioni assumono un quaternione NORMALIZZATO. Matrici 3x3 row-major.
'
' Imbardata (yaw)    = 90° attorno a +Z   (muso da +X verso +Y)
' Beccheggio (pitch) = 30° attorno a +Y
' Rollio (roll)      = -20° attorno a +X
' Se il pilota comanda in quest'ordine, la rotazione complessiva è
' yaw ∘ pitch ∘ roll: R = Rz(90°) · Ry(30°) · Rx(-20°).
'
' Domini: QUAT. Output deterministico.
IMPORT QUAT

CONST D2R = 0.017453292519943295

SUB Main()
  DIM q AS Quat, q2 AS Quat, qt AS Quat, qq AS Quat
  DIM qy AS Quat, qp AS Quat, qroll AS Quat

  ' ── 1. l'assetto di partenza ──────────────────────────────────────────────
  q = QUAT.IDENTITY()
  PRINT "identita="; FORMAT(q.X, "#.0"); ","; FORMAT(q.Y, "#.0"); ","; FORMAT(q.Z, "#.0"); ","; FORMAT(q.W, "#.0")
  PRINT " norma="; FORMAT(QUAT.LENGTH(q), "#.000")

  ' ── 2. i tre comandi del pilota ───────────────────────────────────────────
  qy = QUAT.FROMAXISANGLE(0, 0, 1, 90 * D2R)
  PRINT "yaw= "; FORMAT(qy.X, "#.000"); ","; FORMAT(qy.Y, "#.000"); ","; FORMAT(qy.Z, "#.000"); ","; FORMAT(qy.W, "#.000")
  qp = QUAT.FROMAXISANGLE(0, 1, 0, 30 * D2R)
  PRINT "pitch="; FORMAT(qp.X, "#.000"); ","; FORMAT(qp.Y, "#.000"); ","; FORMAT(qp.Z, "#.000"); ","; FORMAT(qp.W, "#.000")
  qroll = QUAT.FROMAXISANGLE(1, 0, 0, -20 * D2R)
  PRINT "roll= "; FORMAT(qroll.X, "#.000"); ","; FORMAT(qroll.Y, "#.000"); ","; FORMAT(qroll.Z, "#.000"); ","; FORMAT(qroll.W, "#.000")

  ' ── 3. composizione: prima rollio, poi beccheggio, poi imbardata ──────────
  q2 = QUAT.MUL(qp, qroll)          ' pitch ∘ roll
  q = QUAT.MUL(qy, q2)           ' yaw ∘ (pitch ∘ roll)
  PRINT "assetto="; FORMAT(q.X, "#.000"); ","; FORMAT(q.Y, "#.000"); ","; FORMAT(q.Z, "#.000"); ","; FORMAT(q.W, "#.000")
  PRINT "norma-assetto="; FORMAT(QUAT.LENGTH(q), "#.000000")

  ' ── 4. dove punta il muso (+X del corpo) nel mondo ────────────────────────
  DIM nose(2) AS DOUBLE
  DIM up(2) AS DOUBLE
  DIM m(8) AS DOUBLE
  DIM st(0) AS LONG
  QUAT.ROTATEVEC3(q, 1, 0, 0, nose)
  QUAT.ROTATEVEC3(q, 0, 1, 0, up)
  PRINT "muso="; FORMAT(nose(0), "#.000"); ","; FORMAT(nose(1), "#.000"); " "; FORMAT(nose(2), "#.000")
  PRINT "ala="; FORMAT(up(0), "#.000"); ","; FORMAT(up(1), "#.000"); " "; FORMAT(up(2), "#.000")

  ' ── 5. matrice 3x3 per il motore grafico, e ritorno ───────────────────────
  PRINT "matrice="; QUAT.TOMAT3(q, m)
  PRINT " riga0="; FORMAT(m(0), "#.000"); " "; FORMAT(m(1), "#.000"); " "; FORMAT(m(2), "#.000")
  PRINT " riga1="; FORMAT(m(3), "#.000"); " "; FORMAT(m(4), "#.000"); " "; FORMAT(m(5), "#.000")
  PRINT " riga2="; FORMAT(m(6), "#.000"); " "; FORMAT(m(7), "#.000"); " "; FORMAT(m(8), "#.000")
  qq = QUAT.FROMMAT3(m)
  PRINT "da-matrice="; FORMAT(qq.X, "#.000"); ","; FORMAT(qq.Y, "#.000"); ","; FORMAT(qq.Z, "#.000"); ","; FORMAT(qq.W, "#.000")

  ' ── 6. inversa: dal vettore del mondo al riferimento del corpo ────────────
  DIM back(2) AS DOUBLE
  qt = QUAT.INVERSE(q, st)
  PRINT "inversa="; st(0); " "; FORMAT(qt.X, "#.000"); ","; FORMAT(qt.Y, "#.000"); ","; FORMAT(qt.Z, "#.000"); ","; FORMAT(qt.W, "#.000")
  QUAT.ROTATEVEC3(qt, nose(0), nose(1), nose(2), back)
  PRINT "muso-in-corpo="; FORMAT(back(0), "#.000"); ","; FORMAT(back(1), "#.000"); " "; FORMAT(back(2), "#.000")

  ' ── 7. controllo di coerenza: coniugato di un versore = inversa ───────────
  q2 = QUAT.CONJUGATE(q)
  PRINT "conj-vs-inv="; FORMAT(QUAT.DOT(q2, qt), "#.000000")

  ' ── 8. due assetti e l'animazione di virata ───────────────────────────────
  ' assetto B: imbardata 180° (muso verso -X), stesso beccheggio/rollio
  qy = QUAT.FROMAXISANGLE(0, 0, 1, 180 * D2R)
  q2 = QUAT.MUL(qp, qroll)
  qq = QUAT.MUL(qy, q2)           ' assetto B completo
  PRINT "assetto-B="; FORMAT(qq.X, "#.000"); ","; FORMAT(qq.Y, "#.000"); ","; FORMAT(qq.Z, "#.000"); ","; FORMAT(qq.W, "#.000")

  ' angolo fra i due assetti: 2·acos(|dot|); dot = cos(metà angolo)
  PRINT "cos-meta="; FORMAT(QUAT.DOT(q, qq), "#.000000")

  ' slerp al 25%, 50%, 75%: il muso descrive l'arco di virata
  qt = QUAT.SLERP(q, qq, 0.25)
  QUAT.ROTATEVEC3(qt, 1, 0, 0, nose)
  PRINT "slerp25="; FORMAT(nose(0), "#.000"); ","; FORMAT(nose(1), "#.000")
  qt = QUAT.SLERP(q, qq, 0.50)
  QUAT.ROTATEVEC3(qt, 1, 0, 0, nose)
  PRINT "slerp50="; FORMAT(nose(0), "#.000"); ","; FORMAT(nose(1), "#.000")
  qt = QUAT.SLERP(q, qq, 0.75)
  QUAT.ROTATEVEC3(qt, 1, 0, 0, nose)
  PRINT "slerp75="; FORMAT(nose(0), "#.000"); ","; FORMAT(nose(1), "#.000")

  ' lerp (lineare normalizzato) sugli stessi due assetti, a metà strada
  qt = QUAT.LERP(q, qq, 0.5)
  QUAT.ROTATEVEC3(qt, 1, 0, 0, nose)
  PRINT "lerp50="; FORMAT(nose(0), "#.000"); ","; FORMAT(nose(1), "#.000")

  ' ── 9. media pesata "ingenua": add + scale + normalize ───────────────────
  ' media 50/50 dei due assetti = somma scalata, poi normalizzata
  q2 = QUAT.ADD(q, qq)
  qt = QUAT.SCALE(q2, 0.5)
  PRINT "media-grezza-norma="; FORMAT(QUAT.LENGTH(qt), "#.000000")
  q2 = QUAT.NORMALIZE(qt)
  PRINT "media="; FORMAT(q2.X, "#.000"); ","; FORMAT(q2.Y, "#.000"); ","; FORMAT(q2.Z, "#.000"); ","; FORMAT(q2.W, "#.000")

  ' l'opposto dell'assetto (stessa rotazione, emisfero opposto)
  qt = QUAT.NEG(q2)
  PRINT "negata="; FORMAT(qt.X, "#.000"); ","; FORMAT(qt.Y, "#.000"); ","; FORMAT(qt.Z, "#.000"); ","; FORMAT(qt.W, "#.000")

  ' ── 10. puntamento: camera verso un bersaglio ────────────────────────────
  ' la camera guarda +Z; il bersaglio è in direzione (1,1,1)
  qt = QUAT.FROMDIR(1, 1, 1)
  PRINT "look-at="; FORMAT(qt.X, "#.000"); ","; FORMAT(qt.Y, "#.000"); ","; FORMAT(qt.Z, "#.000"); ","; FORMAT(qt.W, "#.000")
  ' rotazione che porta l'asse +X su +Y (quarto di giro attorno a Z)
  qt = QUAT.FROMTWOVECTORS(1, 0, 0, 0, 1, 0)
  PRINT "x-verso-y="; FORMAT(qt.X, "#.000"); ","; FORMAT(qt.Y, "#.000"); ","; FORMAT(qt.Z, "#.000"); ","; FORMAT(qt.W, "#.000")

  ' ── 11. quaternione esplicito e casi limite ──────────────────────────────
  qt = QUAT.MAKE(0.5, -0.5, 0.5, 0.5)
  PRINT "esplicito="; FORMAT(qt.X, "#.000"); ","; FORMAT(qt.Y, "#.000"); ","; FORMAT(qt.Z, "#.000"); ","; FORMAT(qt.W, "#.000")
  PRINT "esplicito-norma="; FORMAT(QUAT.LENGTH(qt), "#.000000")
  qt = QUAT.MAKE(0, 0, 0, 0)
  q2 = QUAT.INVERSE(qt, st)
  PRINT "inversa-di-nullo="; st(0); " norma="; FORMAT(QUAT.LENGTH(q2), "#.000")
  ' NORMALIZE del nullo restituisce l'identità (documentato)
  q2 = QUAT.NORMALIZE(qt)
  PRINT "normalize-di-nullo="; FORMAT(q2.W, "#.000")
  PRINT "esiti-guardia="; QUAT.TOMAT3(q, m); " "; QUAT.ROTATEVEC3(q, 1, 0, 0, nose)
  PRINT "fine"
END SUB
