' EXPECT: full= 4|edge= 1|inst= 6|id= 3|tint= 8388608|blue= 255|self= 4|lodwin= 6|lodtint= 16711680|again= 8388608|chain= 6|ray= 60|reset= 4
' scene_instance — instancing per materiale (`SCENE INSTANCE o, src`), specifica M-INST.
'
' L'oggetto 1 sono due quad 15×20 affiancati a z=60 (quattro triangoli, blu). L'oggetto
' 2 è un quad ±6 a z=400 — OLTRE il far plane (300), quindi invisibile da solo — verde e
' con la luce base 128: è la SORGENTE. Gli oggetti 3 e 4 sono ospiti SENZA geometria, a
' z=50 e z=40: l'unico modo di vederli è che disegnino la geometria di un altro.
'
' Il test guarda il frame, non lo stato interno: STATS("draw") conta i triangoli
' disegnati, PICK(x, y) dice quale oggetto copre il pixel (l'identità resta quella
' dell'ospite) e PIXEL(x, y) dice con quale materiale: 0x800000 = rosso 255 con la luce
' 128 della sorgente, 0xFF0000 = luce 255 dell'ospite (quella che usa il LOD).
IMPORT SCENE

SUB Main()
  ' ── A. la scena: sorgente lontana, ospiti vuoti ──────────────────────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 0, 0, 255
  SCENE VERTEX -15, -10, 0, 0, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE VERTEX -15,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX  15, -10, 0, 1, 1
  SCENE VERTEX  15,  10, 0, 1, 0
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 60, 0
  SCENE OBJECT 0, 255, 0, 0, 128
  SCENE VERTEX -6, -6, 0, 0, 1
  SCENE VERTEX  6, -6, 0, 1, 1
  SCENE VERTEX  6,  6, 0, 1, 0
  SCENE VERTEX -6,  6, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 400, 0
  SCENE OBJECT 255, 0, 0
  SCENE OBJPOS 0, 0, 50, 0
  SCENE OBJECT 255, 255, 255
  SCENE OBJPOS 0, 0, 40, 0

  SCENE RENDER
  PRINT "full="; SCENE.STATS("draw")
  e = 0
  IF SCENE.PICK(10, 16) = 1 THEN e = 1
  PRINT "edge="; e

  ' ── B. istanza: geometria della sorgente, trasformazione e tinta dell'ospite ─
  '      Tre: z=50 (davanti all'1), rosso, luce 255. Il materiale (luce 128) è
  '      quello della sorgente 2; il pixel 10 resta all'oggetto 1.
  SCENE INSTANCE 3, 2
  SCENE RENDER
  PRINT "inst="; SCENE.STATS("draw")
  PRINT "id="; SCENE.PICK(16, 16)
  PRINT "tint="; SCENE.PIXEL(16, 16)
  PRINT "blue="; SCENE.PIXEL(10, 16)

  ' ── C. INSTANCE 3, 3 riporta l'ospite alla propria geometria (vuota) ───────
  SCENE INSTANCE 3, 3
  SCENE RENDER
  PRINT "self="; SCENE.STATS("draw")

  ' ── D. INSTANCE e LOD si contendono la stessa scelta: dopo LOD 3, 0, 2 ─────
  '      l'oggetto 3 disegna i triangoli di 2 ma con il PROPRIO materiale.
  SCENE LOD 3, 0, 2
  SCENE RENDER
  PRINT "lodwin="; SCENE.STATS("draw")
  PRINT "lodtint="; SCENE.PIXEL(16, 16)
  SCENE INSTANCE 3, 2
  SCENE RENDER
  PRINT "again="; SCENE.PIXEL(16, 16)

  ' ── E. un livello solo: l'istanza 4 di 3 (a sua volta istanza di 2) disegna ─
  '      la geometria DICHIARATA di 3, che è vuota — non segue la catena.
  SCENE INSTANCE 4, 3
  SCENE RENDER
  PRINT "chain="; SCENE.STATS("draw")

  ' ── F. raggi e quote usano la geometria DICHIARATA: l'ospite non è ostacolo ─
  hit = SCENE.SHOOT(300)
  PRINT "ray="; SCENE.SHOTDIST()

  ' ── G. RESET azzera anche le istanze: stessa scena, torna piena ────────────
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE PICK ON
  SCENE OBJECT 0, 0, 255
  SCENE VERTEX -15, -10, 0, 0, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE VERTEX -15,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE VERTEX   0, -10, 0, 0.5, 1
  SCENE VERTEX  15, -10, 0, 1, 1
  SCENE VERTEX  15,  10, 0, 1, 0
  SCENE VERTEX   0,  10, 0, 0.5, 0
  SCENE QUAD 4, 7, 6, 5
  SCENE OBJPOS 0, 0, 60, 0
  SCENE RENDER
  PRINT "reset="; SCENE.STATS("draw")
END SUB
