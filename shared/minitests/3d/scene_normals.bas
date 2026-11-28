' EXPECT: flat_center= 15987699|flat_edge= 14342874|front_center= 15856113|front_edge= 10329501|away_center= 2631720|away_edge= 2631720|rot_center= 2631720|rot_edge= 2631720
' scene_normals — `SCENE VERTEX` a 8 argomenti (M-LIGHT-N): normali per vertice.
'
' Lo stesso quad, la stessa lampada (slot 0, a z=-2, raggio 10, quantità 255) e la
' stessa luce base 40, con tre dichiarazioni diverse dei vertici:
'
'   1. flat  = VERTEX a 5 argomenti, nessuna normale. È il percorso di P4: la
'      lampada schiarisce solo in base alla distanza. Centro 15987699 (0xF3F3F3,
'      luce 243), angolo in alto a sinistra 14342874 (0xDADADA, luce 218: più
'      lontano dalla lampada). Il percorso senza normali è quello di P4, byte per
'      byte: le 18 golden restano a diff=0.
'   2. front = VERTEX a 8 argomenti con normale (0,0,-1), verso la lampada.
'      Centro 15856113 (luce 241: N·L quasi 1, la lampada è davanti), angolo
'      10329501 (luce 157: N·L ≈ 0.66, la faccia si scurisce verso il bordo).
'      È il check che distingue N·L dal solo falloff.
'   3. away  = normale (0,0,1), girata rispetto alla lampada. N·L = 0: resta
'      il solo ambiente 40 → 0x282828 = 2631720 su TUTTI i pixel. È il check
'      che dimostra che la normale conta davvero.
'   4. rot   = stessa geometria e stessa normale (0,0,-1) di `front`, ma con
'      `SCENE OBJPOS 0, 0, 0, 180`: la normale GIRA con l'oggetto e finisce a
'      (0,0,1), quindi la lampada non la vede più → solo ambiente 2631720. Senza
'      la rotazione della normale il quad sarebbe rimasto chiaro (241/157): è
'      il check che lega le normali all'orientamento dell'oggetto. TWOSIDED on
'      serve perché il quad ruotato mostra il retro.
'
' Le normali sono nello spazio dell'OGGETTO e per OGGETTO: in un oggetto ci
' sono per tutti i vertici o per nessuno (mescolarle è un errore esplicito).
IMPORT SCENE

SUB Main()
  ' ── 1. nessuna normale: comportamento di P4 ─────────────────────────────
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, -2, 10, 255
  SCENE RENDER
  PRINT "flat_center="; SCENE.PIXEL(8, 8)
  PRINT "flat_edge="; SCENE.PIXEL(0, 0)

  ' ── 2. normali verso la lampada: N·L pieno al centro, minore al bordo ────
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1, 0, 0, -1
  SCENE VERTEX  2, -2, 0, 1, 1, 0, 0, -1
  SCENE VERTEX  2,  2, 0, 1, 0, 0, 0, -1
  SCENE VERTEX -2,  2, 0, 0, 0, 0, 0, -1
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, -2, 10, 255
  SCENE RENDER
  PRINT "front_center="; SCENE.PIXEL(8, 8)
  PRINT "front_edge="; SCENE.PIXEL(0, 0)

  ' ── 3. normali girate: la lampada non conta, resta l'ambiente ────────────
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1, 0, 0, 1
  SCENE VERTEX  2,  2, 0, 1, 0, 0, 0, 1
  SCENE VERTEX -2,  2, 0, 0, 0, 0, 0, 1
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, -2, 10, 255
  SCENE RENDER
  PRINT "away_center="; SCENE.PIXEL(8, 8)
  PRINT "away_edge="; SCENE.PIXEL(0, 0)

  ' ── 4. normali ruotate con l'oggetto: OBJPOS 180 le gira dalla lampada ───
  SCENE RESET
  SCENE VIEW 16, 16, 60
  SCENE TWOSIDED on
  SCENE OBJECT 255, 255, 255, 0, 40
  SCENE VERTEX -2, -2, 0, 0, 1, 0, 0, -1
  SCENE VERTEX  2, -2, 0, 1, 1, 0, 0, -1
  SCENE VERTEX  2,  2, 0, 1, 0, 0, 0, -1
  SCENE VERTEX -2,  2, 0, 0, 0, 0, 0, -1
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 0, 180
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE LAMP 0, 0, 0, -2, 10, 255
  SCENE RENDER
  PRINT "rot_center="; SCENE.PIXEL(8, 8)
  PRINT "rot_edge="; SCENE.PIXEL(0, 0)
END SUB
