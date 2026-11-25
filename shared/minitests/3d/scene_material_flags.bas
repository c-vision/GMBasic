' EXPECT: alpha_on= 16711680  ,  988969|alpha_off= 65280|cull= 0  ,  2|two= 2  ,  0|smooth= 3529079684  ,  3786841198
' scene_material_flags — `SCENE SMOOTH|ALPHA|TWOSIDED on|off` (idea #5).
'
' I tre flag esistono già nel kernel (`gmb_3d_material_t`: bilinear, masked, two_sided) e
' questi verbi li espongono al BASIC senza toccare il rasterizzatore. I default sono quelli
' storici — bilineare ON, alpha-test ON, back-face cull ON — quindi una scena che non li
' chiama dà gli stessi byte di sempre.
'
' Asset `scene_alpha.png` (8x8, RGBA8): bordo spesso 2px TRASPARENTE colore VERDE (alpha 0),
' centro 4x4 ROSSO opaco. Separa i due comportamenti dell'alpha-test:
'   ALPHA ON  → il bordo alpha 0 è scartato, resta il fondo (0x0F1729 = 988969);
'   ALPHA OFF → il bordo è disegnato col suo rgb (verde 0x00FF00 = 65280).
' Il quad (±2, camera a z = -3 con fov 60 che copre ±1.73) riempie il target: il pixel
' d'angolo cade nel bordo trasparente, il centro nel rosso opaco.
IMPORT SCENE

SUB Main()
  ' ── ALPHA ────────────────────────────────────────────────────────────────
  SCENE VIEW 16, 16, 60
  SCENE TEXTURE "scene_alpha.png"
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE SMOOTH OFF                              ' nearest: il pixel d'angolo è un texel puro

  SCENE ALPHA ON
  SCENE RENDER
  PRINT "alpha_on="; SCENE.PIXEL(8, 8); " , "; SCENE.PIXEL(0, 0)

  SCENE ALPHA OFF
  SCENE RENDER
  PRINT "alpha_off="; SCENE.PIXEL(0, 0)

  ' ── TWOSIDED: una faccia girata al contrario ─────────────────────────────
  ' Con il cull attivo (default) la faccia posteriore finisce in CULLED e non si vede;
  ' accendendo TWOSIDED viene rasterizzata. RESET non spegne i flag (sono materiale).
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 1, 2, 3                          ' winding opposto: posteriore alla camera
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE TWOSIDED OFF
  SCENE RENDER
  PRINT "cull="; SCENE.DRAWN(); " , "; SCENE.CULLED()

  SCENE TWOSIDED ON
  SCENE RENDER
  PRINT "two="; SCENE.DRAWN(); " , "; SCENE.CULLED()

  ' ── SMOOTH: bilineare contro nearest sullo stesso frame ──────────────────
  ' Due hash diversi = il flag arriva davvero al campionamento della texture.
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE TEXTURE 4
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -2, 0, 0

  SCENE SMOOTH ON
  SCENE RENDER
  hOn = SCENE.HASH()
  SCENE SMOOTH OFF
  SCENE RENDER
  hOff = SCENE.HASH()
  PRINT "smooth="; hOn; " , "; hOff
END SUB
