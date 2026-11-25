' EXPECT: point_center= 13948116|point_edge= 2631720|spot_axis= 13092807|spot_out= 2631720
' scene_lamp_point_spot — `SCENE LAMP` (M-LIGHT): 6 argomenti = puntiforme, 10 = spot.
'
' La luce si somma alla luce base dell'oggetto (qui 40, il fondo ambiente storico)
' e si valuta nel PUNTO del mondo interpolato per pixel: il centro del quad è a
' ~0.15 dal centro della lampada, l'angolo a ~2.3. La formula è una sola
' (`gmb_3d_light_eval`), la stessa usata dall'oracolo.
'
'   1. point_center: lampada puntiforme raggio 1 sulla faccia, quantità piena.
'      Il centro del pixel (8,8) dista 0.326 dal centro della lampada: falloff
'      lineare f = 1 - 0.326/1 = 0.674 → 40 + 255*0.674 = 212 → 0xD4D4D4
'      = 13948116;
'   2. point_edge: l'angolo del quad è fuori dal raggio → resta l'ambiente 40
'      → 0x282828 = 2631720;
'   3. spot_axis: spot davanti alla faccia, direzione +z, semiangolo 30°: il
'      pixel centrale è dentro il cono (f = 0.99, s = 0.63 sul cono) →
'      40 + 255*0.624 = 199 → 0xC7C7C7 = 13092807;
'   4. spot_out: l'angolo è fuori dal cono (coseno < cos 30°) → solo ambiente
'      → 2631720. È il check che distingue lo spot dalla puntiforme: la
'      puntiforme, con lo stesso raggio, illuminerebbe anche l'angolo.
IMPORT SCENE

SUB Main()
  ' ── puntiforme ───────────────────────────────────────────────────────────
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, 0, 1, 255
  SCENE RENDER
  PRINT "point_center="; SCENE.PIXEL(8, 8)
  PRINT "point_edge="; SCENE.PIXEL(0, 0)

  ' ── spot ─────────────────────────────────────────────────────────────────
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, -1, 100, 255, 0, 0, 1, 30
  SCENE RENDER
  PRINT "spot_axis="; SCENE.PIXEL(8, 8)
  PRINT "spot_out="; SCENE.PIXEL(0, 0)
END SUB
