' EXPECT: far_off= 16711680|far_on= 988969|far_drawn= 0|far_back= 16711680
' scene_far — `SCENE FAR d` (M-FAR): oltre d il triangolo si salta, 0 = spento.
'
' Il quad rosso è a z = 4, la camera a z = -3: distanza 7. Con FAR 5 la geometria
' è oltre il limite e non entra nel frame; con FAR 10 torna a disegnarsi.
' Il fondo del target è 0x0F1729 = 988969 (lo stesso dell'oracolo).
'
'   1. far_off: senza limite il quad riempie il frame → 0xFF0000 = 16711680;
'   2. far_on: FAR 5 → il quad è saltato, resta il fondo → 988969;
'   3. far_drawn: SCENE.DRAWN() = 0, il triangolo non è stato rasterizzato;
'   4. far_back: FAR 10 → di nuovo 16711680 (il limite è reversibile).
IMPORT SCENE

SUB Main()
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -2, -2, 4, 0, 1
  SCENE VERTEX  2, -2, 4, 1, 1
  SCENE VERTEX  2,  2, 4, 1, 0
  SCENE VERTEX -2,  2, 4, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0

  SCENE RENDER
  PRINT "far_off="; SCENE.PIXEL(8, 8)

  SCENE FAR 5
  SCENE RENDER
  PRINT "far_on="; SCENE.PIXEL(8, 8)
  PRINT "far_drawn="; SCENE.DRAWN()

  SCENE FAR 10
  SCENE RENDER
  PRINT "far_back="; SCENE.PIXEL(8, 8)
END SUB
