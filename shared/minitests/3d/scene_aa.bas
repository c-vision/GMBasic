' EXPECT: flat_off= 16777215|edge_off= 16777215|flat_on= 16777215|edge_on= 8883092
' scene_aa — `SCENE AA on|off`: antialiasing SSAA 2x per pixel (spento di default).
'
' Un quad bianco con il bordo sinistro quasi verticale vicino al centro; il fondo è
' 0x0F1729. Con AA spento un solo campione al centro del pixel: il pixel 6 è dentro
' (bianco pieno) e il 7 è fuori (fondo). Con AA acceso quattro campioni a 1/4 e 3/4:
' il pixel 6 è coperto solo a metà e diventa la fusione bianco/fondo, mentre l'area
' piatta (pixel 3) resta ESATTAMENTE bianca: è il check che l'AA non sporca il
' riempimento, cambia solo i bordi.
'
'   1. flat_off / flat_on = 16777215 (0xFFFFFF): area piatta identica.
'   2. edge_off = 16777215 (0xFFFFFF): bordo netto, il pixel 6 è dentro col solo
'      campione centrale, il 7 è già fuori.
'   3. edge_on  = 8883092 (0x878B94): lo stesso pixel 6, coi quattro campioni, è
'      coperto a metà → 50% bianco + 50% fondo.
'
' Lo stato AA sopravvive a RESET, come gli altri flag di scena; il default è spento,
' quindi le scene che non lo chiamano restano byte per byte quelle di P4.
IMPORT SCENE

SUB Main()
  ' ── 1. AA spento (default): un campione al centro ─────────────────────────
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX 0.2, -2, 0, 0, 1
  SCENE VERTEX 4,   -2, 0, 1, 1
  SCENE VERTEX 4,    2, 0, 1, 0
  SCENE VERTEX 0.2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE RENDER
  PRINT "flat_off="; SCENE.PIXEL(3, 8)
  PRINT "edge_off="; SCENE.PIXEL(6, 8)

  ' ── 2. AA acceso: quattro campioni per pixel ─────────────────────────────
  SCENE AA on
  SCENE RENDER
  PRINT "flat_on="; SCENE.PIXEL(3, 8)
  PRINT "edge_on="; SCENE.PIXEL(6, 8)
END SUB
