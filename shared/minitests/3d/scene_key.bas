' EXPECT: t0= 4|t05= 2|t1= 1|t2= 1|yaw0= 1|yaw45= 1|yaw90= 0|pitch90= 0|roll= 1|rst= 1
' scene_key — keyframe TRS degli oggetti (`SCENE KEY o, t, x, y, z, yaw, pitch, roll`
' e `SCENE TIME t`), specifica M-KEY.
'
' Le chiavi sono una dichiarazione: RENDER le campiona al tempo di scena e scrive
' la trasformazione nell'oggetto. Il test guarda il RISULTATO nel frame, non lo
' stato interno: il buffer di PICK dice quale pixel è coperto dall'oggetto, quindi
' ogni maschera qui sotto è la posizione vera dell'oggetto sullo schermo.
'
' Target 32×32 con fov verticale 60 e camera nell'origine: a z=60 il mezzo lato
' visibile è tan(30°)*60 ≈ 34,6, quindi il pixel 16 è il centro e i pixel 6 e 25
' stanno a circa ±22 unità. Quad 20×20: a x=0 copre ~11..20; con x=-20 copre
' ~21..29 e con x=+20 ~3..11 — il renderer guarda verso +z con l'asse y in alto,
' quindi le x positive cadono a SINISTRA sullo schermo (stessa convenzione di
' tutte le golden): la maschera legge i pixel, non il segno delle x.
IMPORT SCENE

SUB Main()
  ' ── A. interpolazione lineare della posizione e clamp fuori intervallo ────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE KEY 1, 0, -20, 0, 60, 0, 0, 0
  SCENE KEY 1, 1,  20, 0, 60, 0, 0, 0

  SCENE TIME 0
  SCENE RENDER
  m = 0
  IF SCENE.PICK(6, 16) = 1 THEN m = m + 1
  IF SCENE.PICK(16, 16) = 1 THEN m = m + 2
  IF SCENE.PICK(25, 16) = 1 THEN m = m + 4
  PRINT "t0="; m

  SCENE TIME 0.5
  SCENE RENDER
  m = 0
  IF SCENE.PICK(6, 16) = 1 THEN m = m + 1
  IF SCENE.PICK(16, 16) = 1 THEN m = m + 2
  IF SCENE.PICK(25, 16) = 1 THEN m = m + 4
  PRINT "t05="; m

  SCENE TIME 1
  SCENE RENDER
  m = 0
  IF SCENE.PICK(6, 16) = 1 THEN m = m + 1
  IF SCENE.PICK(16, 16) = 1 THEN m = m + 2
  IF SCENE.PICK(25, 16) = 1 THEN m = m + 4
  PRINT "t1="; m

  ' Dopo l'ultima chiave vale l'ultima: nessuna estrapolazione.
  SCENE TIME 2
  SCENE RENDER
  m = 0
  IF SCENE.PICK(6, 16) = 1 THEN m = m + 1
  IF SCENE.PICK(16, 16) = 1 THEN m = m + 2
  IF SCENE.PICK(25, 16) = 1 THEN m = m + 4
  PRINT "t2="; m

  ' ── B. yaw interpolato: a 90° il quad è di taglio e non si disegna ────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE KEY 1, 0, 0, 0, 60, 0, 0, 0
  SCENE KEY 1, 1, 0, 0, 60, 90, 0, 0

  v = 0
  SCENE TIME 0
  SCENE RENDER
  IF SCENE.STATS("draw") > 0 THEN v = 1
  PRINT "yaw0="; v
  v = 0
  SCENE TIME 0.5
  SCENE RENDER
  IF SCENE.STATS("draw") > 0 THEN v = 1
  PRINT "yaw45="; v
  v = 0
  SCENE TIME 1
  SCENE RENDER
  IF SCENE.STATS("draw") > 0 THEN v = 1
  PRINT "yaw90="; v

  ' ── C. pitch interpolato: 90° mette il quad di taglio (asse x) ────────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE KEY 1, 0, 0, 0, 60, 0, 0, 0
  SCENE KEY 1, 1, 0, 0, 60, 0, 90, 0
  SCENE TIME 1
  SCENE RENDER
  PRINT "pitch90="; SCENE.STATS("draw")

  ' ── D. roll interpolato: il quad ruota nel piano, il frame cambia ─────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -4, 0, 0, 1
  SCENE VERTEX  10, -4, 0, 1, 1
  SCENE VERTEX  10,  4, 0, 1, 0
  SCENE VERTEX -10,  4, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE KEY 1, 0, 0, 0, 60, 0, 0, 0
  SCENE KEY 1, 1, 0, 0, 60, 0, 0, 90
  SCENE TIME 0
  SCENE RENDER
  h0 = SCENE.HASH()
  SCENE TIME 1
  SCENE RENDER
  r = 0
  IF SCENE.HASH() <> h0 THEN r = 1
  PRINT "roll="; r

  ' ── E. RESET azzera le piste: l'oggetto torna dove lo dichiara OBJPOS ─────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 60, 0
  SCENE RENDER
  PRINT "rst="; SCENE.PICK(16, 16)
END SUB
