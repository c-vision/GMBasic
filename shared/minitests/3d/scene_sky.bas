' EXPECT: sky_center= 65280|sky_red= 16711680|sky_shift= 16711680
' scene_sky — `SCENE SKY repeat, shift` (M-SKY): cielo per angolo con la texture corrente.
'
' Nessuna geometria: ogni pixel prende il texel della direzione che gli corrisponde.
' La picture è `scene_alpha.png` (8x8): bordo spesso 2px VERDE (0x00FF00) e centro
' 4x4 ROSSO (0xFF0000). Il cielo usa solo l'RGB, non l'alpha.
'
' Con repeat 6 il campo visivo (60°) copre un giro intero della picture:
'   - il centro dello schermo (8,8) cade nella colonna 7 e nella riga 4 → VERDE;
'   - il pixel (12,8) cade nella colonna 5, riga 4 (centro) → ROSSO.
' Con shift 0.5 la picture scorre di mezzo giro: il centro passa alla colonna 3,
' sempre riga 4 → ROSSO. È il check che prova che `shift` agisce davvero.
IMPORT SCENE

SUB Main()
  SCENE VIEW 16, 16, 60
  SCENE TEXTURE "scene_alpha.png"
  SCENE SKY 6, 0
  SCENE RENDER
  PRINT "sky_center="; SCENE.PIXEL(8, 8)
  PRINT "sky_red="; SCENE.PIXEL(12, 8)

  SCENE SKY 6, 0.5
  SCENE RENDER
  PRINT "sky_shift="; SCENE.PIXEL(8, 8)
END SUB
