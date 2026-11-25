' EXPECT: plain= 6579300|mapped= 6447714
' scene_colormap — `SCENE COLORMAP` (C-SAME-COLORMAP): luce quantizzata a 32 livelli.
'
' Piano orizzontale (normale +Y) con luce dall'alto a livello 100: la luce per
' faccia vale esattamente 100 (0x646464 = 6579300). Accendendo COLORMAP la stessa
' luce passa per q = (100*31 + 127)/255 = 12 → 12*255/31 = 98 (0x626262 = 6447714).
'
' Il default resta SPENTO: le scene P3 che non chiamano COLORMAP non cambiano byte
' (qui è il primo numero a dimostrarlo).
IMPORT SCENE

SUB Main()
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -1, 0, -1, 0, 0
  SCENE VERTEX -1, 0,  1, 0, 1
  SCENE VERTEX  1, 0,  1, 1, 1
  SCENE VERTEX  1, 0, -1, 1, 0
  SCENE QUAD 0, 1, 2, 3
  SCENE LIGHT 0, 1, 0, 100
  SCENE CAMERA 0, 3, -0.01, 0, -88
  SCENE RENDER
  PRINT "plain="; SCENE.PIXEL(8, 8)

  SCENE COLORMAP
  SCENE RENDER
  PRINT "mapped="; SCENE.PIXEL(8, 8)
END SUB
