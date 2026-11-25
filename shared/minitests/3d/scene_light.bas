' EXPECT: up= 16777215|side_lit= 16777215|side_amb= 2631720
' scene_light — `SCENE LIGHT dx, dy, dz, livello`: luce direzionale per faccia.
'
' La normale si calcola sulla geometria in coordinate MONDO, quindi conta come è messa
' la faccia, non come è stata dichiarata. Aritmetica: coseno quantizzato 0..255 e miscela
' intera ambiente(40)→livello; tinta bianca e nessuna texture, così il pixel È la luce.
'
'   1. up=: piano orizzontale (normale +Y) con luce dall'alto (0,1,0) → luce piena 255
'      → 0xFFFFFF = 16777215;
'   2. side_lit=: piano verticale (normale -Z) con luce (0,0,-1) → luce piena → 16777215;
'   3. side_amb=: lo stesso piano verticale con luce dall'alto (0,1,0) → coseno 0
'      → solo ambiente 40 → 0x282828 = 2631720.
IMPORT SCENE

SUB Main()
  ' 1) piano orizzontale, luce dall'alto
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -1, 0, -1, 0, 0
  SCENE VERTEX -1, 0,  1, 0, 1
  SCENE VERTEX  1, 0,  1, 1, 1
  SCENE VERTEX  1, 0, -1, 1, 0
  SCENE QUAD 0, 1, 2, 3
  SCENE LIGHT 0, 1, 0, 255
  SCENE CAMERA 0, 3, -0.01, 0, -88
  SCENE RENDER
  PRINT "up="; SCENE.PIXEL(8, 8)

  ' 2) piano verticale rivolto alla camera, luce frontale
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -1, -1, 0, 0, 1
  SCENE VERTEX  1, -1, 0, 1, 1
  SCENE VERTEX  1,  1, 0, 1, 0
  SCENE VERTEX -1,  1, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE LIGHT 0, 0, -1, 255
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE RENDER
  PRINT "side_lit="; SCENE.PIXEL(8, 8)

  ' 3) lo stesso piano, ma luce dall'alto: la faccia non la vede → solo ambiente
  SCENE LIGHT 0, 1, 0, 255
  SCENE RENDER
  PRINT "side_amb="; SCENE.PIXEL(8, 8)
END SUB
