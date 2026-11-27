' EXPECT: hit= 1|kind= 1|obj= 1|dist= 100|use= 2|thing= 7|tkind= 2|tdist= 15|sees= 1|sees0= 0|sees99=-1
' scene_rays — raggi della superficie SCENE (P5, specifica §3.3 M-RAYS).
'
' SHOOT(range) spara lungo la vista e colpisce oggetti (SHOTKIND 1) e thing
' (SHOTKIND 2); il flag 8 (see-through) lascia passare il colpo, il flag 4
' (passabile) no. USE(reach) prende l'oggetto davanti QUALUNQUE sia il flag.
' SEES(n) e' la linea di vista del thing: 1 vede, 0 non vede, -1 inesistente
' (C-DIVERGE-SEES).
'
' Geometria (y = altezza, piano x/z, fronte a yaw 0 = +z):
'   oggetto 1: muro solido a z = 100
'   oggetto 2: vetro see-through (flag 8) a z = 50 — il colpo ci passa
'   thing 7:   billboard a z = 20, alto 80 — fronte del cilindro a z = 15
'   oggetto 3: muro solido a z = 10 — spegne la linea di vista del thing
' Gli stessi valori su C99Native, IL, PCode e VM/C.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 32, 16, 60
  SCENE CAMERA 0, 46, 0, 0, 0

  SCENE OBJECT 200, 200, 200
  SCENE BOX -50, 0, 100, 50, 100, 110
  SCENE OBJECT 120, 200, 255, 8, 255
  SCENE BOX -50, 0, 50, 50, 100, 55

  ' ── 1. SHOOT attraversa il vetro (flag 8) e ferma sul muro ────────────────
  PRINT "hit="; SCENE.SHOOT(300)
  PRINT "kind="; SCENE.SHOTKIND()
  PRINT "obj="; SCENE.SHOTOBJ()
  PRINT "dist="; SCENE.SHOTDIST()

  ' ── 2. USE ignora i flag: prende il vetro, che e' il piu' vicino ──────────
  PRINT "use="; SCENE.USE(300)

  ' ── 3. Thing: cilindro di raggio w/2, colpito prima del muro ──────────────
  SCENE THING 7, 0, 0, 20, 10, 80
  PRINT "thing="; SCENE.SHOOT(300)
  PRINT "tkind="; SCENE.SHOTKIND()
  PRINT "tdist="; SCENE.SHOTDIST()

  ' ── 4. Linea di vista: libera, poi bloccata dal muro, poi thing assente ───
  PRINT "sees="; SCENE.SEES(7)
  SCENE OBJECT 200, 120, 120
  SCENE BOX -10, 0, 10, 10, 100, 12
  PRINT "sees0="; SCENE.SEES(7)
  PRINT "sees99="; SCENE.SEES(99)
END SUB
