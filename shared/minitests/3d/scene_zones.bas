' EXPECT: z0=-1|t0=|f0=-1000000|c0=-1000000|z1= 2|z2= 3|z3=-1|z4=-1|t1=outer|t2=inner|t3=|f1= 20|f2= 10|f3=-1000000|f4= 5|c1= 100|c2=-1000000
' scene_zones — zone e quote della superficie interattiva SCENE (P5).
'
' Specifica §3.3: M-ZONE, M-FLOOR, M-PASSABLE. Assi SCENE: y = altezza,
' piano (x, z). `ZONE id, x1, y1, x2, y2, z1, z2 [, tag]` ha x1/x2 e z1/z2 sul
' piano e y1/y2 in altezza; il punto vale da 2 unità sotto la base (M-ZONE).
'
' Casi: sentinelle senza zone/geometria (−1, "", −1000000); la PRIMA zona
' creata vince anche se una successiva si sovrappone; la tolleranza di 2 sotto
' la base; ZONETAG di un id inesistente; FLOORAT su piani sovrapposti (vince il
' più alto a o sotto il punto); il passabile (flag 4) visto da FLOORAT; CEILAT.
' Gli stessi valori su C99Native, IL, PCode e VM/C.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 32, 16, 60
  SCENE CAMERA 0, 46, 0, 0, 0

  ' ── 0. Nessuna zona, nessuna geometria: sentinelle ───────────────────────
  PRINT "z0="; SCENE.ZONE(0, 0, 0)
  PRINT "t0="; SCENE.ZONETAG(0)
  PRINT "f0="; SCENE.FLOORAT(0, 0, 0)
  PRINT "c0="; SCENE.CEILAT(0, 0, 0)

  ' ── 1. Zone: creazione in ordine, la prima vince ─────────────────────────
  SCENE ZONE 2, -20, -100, 20, 100, -20, 20, "outer"
  SCENE ZONE 1, -10, 0, 10, 50, -10, 10, "inner"
  SCENE ZONE 3, 30, 10, 40, 20, 30, 40, "shelf"

  PRINT "z1="; SCENE.ZONE(0, 0, 0)
  PRINT "z2="; SCENE.ZONE(35, 8.5, 35)
  PRINT "z3="; SCENE.ZONE(35, 7.5, 35)
  PRINT "z4="; SCENE.ZONE(0, 500, 0)
  PRINT "t1="; SCENE.ZONETAG(2)
  PRINT "t2="; SCENE.ZONETAG(1)
  PRINT "t3="; SCENE.ZONETAG(99)

  ' ── 2. FLOORAT su piani sovrapposti ──────────────────────────────────────
  SCENE OBJECT 200, 200, 200
  SCENE BOX 60, 0, 60, 70, 10, 70
  SCENE OBJECT 200, 200, 200
  SCENE BOX 60, 0, 60, 70, 20, 70

  PRINT "f1="; SCENE.FLOORAT(65, 25, 65)
  PRINT "f2="; SCENE.FLOORAT(65, 15, 65)
  PRINT "f3="; SCENE.FLOORAT(65, 5, 65)

  ' ── 3. Passabile (flag 4): FLOORAT continua a vederlo ────────────────────
  SCENE OBJECT 0, 255, 0, 4, 255
  SCENE BOX -20, 0, -10, -10, 5, 10
  PRINT "f4="; SCENE.FLOORAT(-15, 6, 0)

  ' ── 4. CEILAT: il soffitto più basso a o sopra il punto ─────────────────
  SCENE OBJECT 90, 90, 160
  SCENE BOX -45, 100, -5, -35, 120, 5
  PRINT "c1="; SCENE.CEILAT(-40, 0, 0)
  PRINT "c2="; SCENE.CEILAT(0, 0, 0)
END SUB
