' EXPECT: fog_half= 8421504|fog_zero= 0
' scene_fog — `SCENE FOG d` (M-FOG): buio alla distanza d, 0 = spento.
'
' Il quad è a z = 0, la camera a z = -3: la distanza del pixel centrale è
' 3.0039 (la piccola componente laterale del centro del pixel). La legge è
' lineare `1 - dist/fog` sulla luce già calcolata (qui base 255, nessuna lampada).
'
'   1. fog_half: FOG 6 → k = 1 - 3.0039/6 = 0.4994 → 255*0.4994 + 0.5 = 128
'      → 0x808080 = 8421504;
'   2. fog_zero: FOG 3 → il piano è oltre la distanza di buio → k = 0 → 0.
'      (Senza fog la stessa scena dà 16777215: il buio non è il fondo del target.)
IMPORT SCENE

SUB Main()
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0

  SCENE FOG 6
  SCENE RENDER
  PRINT "fog_half="; SCENE.PIXEL(8, 8)

  SCENE FOG 3
  SCENE RENDER
  PRINT "fog_zero="; SCENE.PIXEL(8, 8)
END SUB
