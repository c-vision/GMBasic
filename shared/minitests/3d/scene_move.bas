' EXPECT: x0= 0|y0= 46|z0= 0|eye= 46|yaw0= 0|p0= 0|hit0= 0|z1= 4|h1= 0|f1= 0|z2= 6|h2= 1|z3= 6|h3= 3|sx= 7|sz= 6|gy= 56|gh= 4|ty= 90|tp=-10|ly= 51|le= 46|pe= 40|py= 45
' scene_move — corpo e movimento della superficie interattiva SCENE (P5).
'
' Specifica §3.3: M-PLAYER, M-COLLIDE. La superficie SCENE usa y come altezza e
' il piano (x, z); CAMERA posiziona l'OCCHIO (P3), quindi i piedi stanno `EYE`
' sotto e le letture X/Y/Z restano quelle di sempre.
'
' Geometria (y = altezza):
'   pavimento   y -2..0, x/z -50..50
'   muro        z 20..30, alto 100
'   gradino     x 10..20, z -10..10, alto 10 (≤ step 24)
'
' Casi: posizione iniziale; MOVE libero; stop sul muro con HIT bit 1; fermo
' addosso con bit 2; scivolamento obliquo (la componente lungo il muro
' sopravvive); salita del gradino con bit 4; TURN in gradi; LIFT; PLAYER.
' Gli stessi valori su C99Native, IL, PCode e VM/C.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 64, 32, 60
  SCENE CAMERA 0, 46, 0, 0, 0     ' piedi a y = 0

  ' Pavimento.
  SCENE OBJECT 80, 80, 80
  SCENE BOX -50, -2, -50, 50, 0, 50
  ' Muro frontale (z 20..30), alto 100: non è un gradino.
  SCENE OBJECT 120, 120, 120
  SCENE BOX -30, 0, 20, 30, 100, 30
  ' Gradino alto 10 (≤ step 24) a destra.
  SCENE OBJECT 200, 180, 60
  SCENE BOX 10, 0, -10, 20, 10, 10

  PRINT "x0="; SCENE.X()
  PRINT "y0="; SCENE.Y()
  PRINT "z0="; SCENE.Z()
  PRINT "eye="; SCENE.EYE()
  PRINT "yaw0="; SCENE.YAW()
  PRINT "p0="; SCENE.PITCH()
  PRINT "hit0="; SCENE.HIT()

  ' ── 1. MOVE libero: avanti di 4, nessun ostacolo, nessun gradino ─────────
  SCENE MOVE 4, 0
  PRINT "z1="; SCENE.Z()
  PRINT "h1="; SCENE.HIT()
  PRINT "f1="; SCENE.FLOORAT(0, 5, 4)

  ' ── 2. Muro: si ferma a un raggio dalla faccia (20 − 14 = 6) ─────────────
  SCENE CAMERA 0, 46, 0, 0, 0
  SCENE MOVE 100, 0
  PRINT "z2="; SCENE.Z()
  PRINT "h2="; SCENE.HIT()
  ' Addosso al muro: la richiesta non sposta nulla (bit 2).
  SCENE MOVE 10, 0
  PRINT "z3="; SCENE.Z()
  PRINT "h3="; SCENE.HIT()

  ' ── 3. Scivolamento: spinta obliqua, la componente lungo il muro passa ───
  SCENE CAMERA 0, 46, 6, 0, 0
  SCENE MOVE 7, 7
  PRINT "sx="; SCENE.X()
  PRINT "sz="; SCENE.Z()

  ' ── 4. Gradino ≤ step: si sale (HIT bit 4) ───────────────────────────────
  SCENE CAMERA 15, 46, -20, 0, 0
  SCENE MOVE 15, 0
  PRINT "gy="; SCENE.Y()
  PRINT "gh="; SCENE.HIT()

  ' ── 5. TURN in gradi, poi LIFT (piedi e occhio insieme) ──────────────────
  SCENE CAMERA 0, 46, 0, 0, 0
  SCENE TURN 90, -10
  PRINT "ty="; SCENE.YAW()
  PRINT "tp="; SCENE.PITCH()
  SCENE LIFT 5
  PRINT "ly="; SCENE.Y()
  PRINT "le="; SCENE.EYE()

  ' ── 6. PLAYER: cambia le misure; l'occhio resta sopra i piedi ────────────
  SCENE PLAYER 10, 50, 40, 20
  PRINT "pe="; SCENE.EYE()
  PRINT "py="; SCENE.Y()
END SUB
