' EXPECT: full= 4|edge1= 1|near= 4|lod= 2|id= 1|edge0= 0|zero= 2|chain= 2|ray= 60|reset= 4
' scene_lod — livello di dettaglio per oggetto (`SCENE LOD o, dist, mesh`), specifica M-LOD.
'
' L'oggetto 1 sono due quad 10×20 affiancati a z=60 (quattro triangoli): il pixel 16
' è il centro dello schermo, il 12 cade a ~+8,6 unità e sta nel quad di destra. Le
' geometrie sostitutive stanno a z=400, oltre il far plane della camera (300), quindi
' FUORI dal frustum: l'unico modo di vederle sullo schermo è che il LOD le disegni al
' posto dell'1. Conteggi distinti (4 la piena, 2 la mesh, 4 quella dell'oggetto 3):
' così un secondo livello applicato per sbaglio si vedrebbe.
'
' Il test guarda il frame, non lo stato interno: STATS("draw") dice quanti triangoli
' sono disegnati e PICK(x, y) dice quale oggetto copre il pixel (l'identità resta
' quella di 1 anche quando la geometria è quella della mesh).
IMPORT SCENE

SUB Main()
  ' ── A. senza LOD: geometria dichiarata, quattro triangoli, pixel 12 coperto ─
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 60, 0
  SCENE OBJECT 0, 255, 0
  SCENE VERTEX -6, -6, 0, 0, 1
  SCENE VERTEX  6, -6, 0, 1, 1
  SCENE VERTEX  6,  6, 0, 1, 0
  SCENE VERTEX -6,  6, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 400, 0
  SCENE OBJECT 0, 0, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX -6, -6, 0, 0, 1
  SCENE VERTEX  6, -6, 0, 1, 1
  SCENE VERTEX  6,  6, 0, 1, 0
  SCENE VERTEX -6,  6, 0, 0, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 400, 0

  SCENE RENDER
  PRINT "full="; SCENE.STATS("draw")
  e = 0
  IF SCENE.PICK(12, 16) = 1 THEN e = 1
  PRINT "edge1="; e

  ' ── B. sotto la soglia (distanza 60 < 100): resta la geometria piena ───────
  SCENE LOD 1, 100, 2
  SCENE RENDER
  PRINT "near="; SCENE.STATS("draw")

  ' ── C. oltre la soglia: si disegna la geometria della mesh, con l'identità ─
  '      di 1 (PICK torna 1) e la copertura che si stringe (pixel 12 scoperto).
  SCENE LOD 1, 30, 2
  SCENE RENDER
  PRINT "lod="; SCENE.STATS("draw")
  PRINT "id="; SCENE.PICK(16, 16)
  e = 0
  IF SCENE.PICK(12, 16) = 1 THEN e = 1
  PRINT "edge0="; e

  ' ── D. dist = 0: la mesh vale sempre (l'occhio non sta nell'oggetto) ───────
  SCENE LOD 1, 0, 2
  SCENE RENDER
  PRINT "zero="; SCENE.STATS("draw")

  ' ── E. un livello solo: la mesh 2 ha il suo LOD verso 3, ma quando fa da ───
  '      sostituta non lo applica (altrimenti si vedrebbero i 4 tri di 3).
  SCENE LOD 2, 0, 3
  SCENE RENDER
  PRINT "chain="; SCENE.STATS("draw")

  ' ── F. i raggi usano la geometria DICHIARATA di 1: il quad è un piano a z=60 ─
  hit = SCENE.SHOOT(300)
  PRINT "ray="; SCENE.SHOTDIST()

  ' ── G. RESET azzera anche il LOD: stessa scena, senza SCENE LOD, torna piena ─
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 60, 0
  SCENE OBJECT 0, 255, 0
  SCENE VERTEX -6, -6, 0, 0, 1
  SCENE VERTEX  6, -6, 0, 1, 1
  SCENE VERTEX  6,  6, 0, 1, 0
  SCENE VERTEX -6,  6, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 400, 0
  SCENE OBJECT 0, 0, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX -6, -6, 0, 0, 1
  SCENE VERTEX  6, -6, 0, 1, 1
  SCENE VERTEX  6,  6, 0, 1, 0
  SCENE VERTEX -6,  6, 0, 0, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 400, 0
  SCENE RENDER
  PRINT "reset="; SCENE.STATS("draw")
END SUB
