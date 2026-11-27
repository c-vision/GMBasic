' EXPECT: base= 2|cbase= 16777215|cout= 988969|bone= 2|cbone= 16777215|shift= 16777215|cs= 988969|blend= 16777215|cbl= 988969|single= 16777215|reset= 2
' scene_skin — skinning a 64 ossa (`SCENE BONES` / `POSEBONE` / `WEIGHT`), specifica M-SKIN.
'
' Un quad ±10 a z=60 in un 32×32 con fov 60: mezzo lato verticale = 60·tan30° = 34,64
' unità, quindi 1 unità = 0,462 px e il quad copre i pixel 11..21 (centro 16).
' Una posa +20 in x lo sposta di 9,2 px: la camera guarda +z con up +y, quindi +x va
' a SINISTRA sullo schermo e il quad finisce sui pixel 5..11.
'
' Il test guarda il frame, non lo stato interno: STATS("draw") conta i triangoli e
' PIXEL(x, y) dice dove è finita la geometria — cioè se la posa dell'osso e la fusione
' dei pesi hanno davvero mosso i vertici, e se peso 0 e posa identità non muovono nulla.
IMPORT SCENE

SUB Main()
  ' ── A. riferimento: geometria dichiarata ─────────────────────────────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 60, 0

  SCENE RENDER
  PRINT "base="; SCENE.STATS("draw")
  PRINT "cbase="; SCENE.PIXEL(16, 16)
  PRINT "cout="; SCENE.PIXEL(24, 16)

  ' ── B. osso con peso 1 e posa di default: nessun pixel cambia ────────────
  SCENE BONES 2
  FOR v = 0 TO 3
    SCENE WEIGHT 1, v, 1, 1
  NEXT v
  SCENE RENDER
  PRINT "bone="; SCENE.STATS("draw")
  PRINT "cbone="; SCENE.PIXEL(16, 16)

  ' ── C. posa +20 in x: il quad si sposta a sinistra (pixel 5..11) ─────────
  SCENE POSEBONE 1, 20, 0, 0, 0
  SCENE RENDER
  PRINT "shift="; SCENE.PIXEL(8, 16)
  PRINT "cs="; SCENE.PIXEL(16, 16)

  ' ── D. due influenze 1:1: identità e +20 danno il punto medio +10 ───────
  SCENE BONES 2
  FOR v = 0 TO 3
    SCENE WEIGHT 1, v, 1, 1
    SCENE WEIGHT 1, v, 2, 1
  NEXT v
  SCENE POSEBONE 1, 0, 0, 0, 0
  SCENE POSEBONE 2, 20, 0, 0, 0
  SCENE RENDER
  PRINT "blend="; SCENE.PIXEL(14, 16)
  PRINT "cbl="; SCENE.PIXEL(20, 16)

  ' ── E. una sola influenza con peso 0,25 vale la matrice intera (+20) ────
  SCENE BONES 2
  FOR v = 0 TO 3
    SCENE WEIGHT 1, v, 2, 0.25
  NEXT v
  SCENE POSEBONE 2, 20, 0, 0, 0
  SCENE RENDER
  PRINT "single="; SCENE.PIXEL(8, 16)

  ' ── F. RESET azzera ossa, pose e pesi: la scena ridichiarata torna piena ─
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 60, 0
  SCENE RENDER
  PRINT "reset="; SCENE.STATS("draw")
END SUB
