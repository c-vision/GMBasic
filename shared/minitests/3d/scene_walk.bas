' EXPECT: mx= 1|x= 0|z= 3|yaw= 40|p=-20|wz= 6|wh= 1|cp= 88|jp= 20|jy= 0|jg= 1|sy= 0|sh= 4|ce= 20|cy= 0
' scene_walk — il primo passo del ciclo di gioco: input → corpo (P6, piano §6).
'
' Perché un MiniTest di input senza dispositivo. Il ciclo di un gioco 3D è una funzione
' di (mask del pad, delta del mouse, tempo): qui la mask e i delta sono un copione, quindi
' l'output è deterministico e identico sulle quattro gambe. Le STESSE righe, con
' `JOY.MASK(0)` e `MOUSE.X/Y` al posto del copione, sono il controllore di
' `Samples3D/Walk.bas` — headless quei lettori valgono 0 (verificato: nessun device,
' nessun errore), quindi il test non dipende dall'hardware.
'
' Cosa fissa (specifica §3.3 e M-INPUT-*):
'  - mask `JOY.MASK` 1 su · 2 giù · 4 sinistra · 8 destra (16 fuoco/salto, 32 accovacciato):
'    avanti = bit0 − bit1, destra = bit3 − bit2; 1 e 2 insieme si annullano;
'  - mouse-look = delta → `SCENE TURN`, sensibilità in gradi per pixel, pitch limitato a
'    ±88 (TURN rifiuta ±89: il clamp è del controllore, non un errore di superficie);
'  - salto: `vy` alla partenza, gravità per frame, `SCENE LIFT` e atterraggio con
'    `SCENE.FLOORAT(x, piedi, z)` (la quota si chiede, non si indovina);
'  - accovacciato: `SCENE PLAYER` abbassa l'OCCHIO e lascia i piedi dove sono.
'
' `SCENE.Y()` è l'occhio (camera), `SCENE.EYE()` l'altezza dell'occhio: i piedi sono
' `Y() − EYE()` (P5, M-PLAYER).
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 64, 32, 60
  SCENE PLAYER 14, 56, 46, 24
  SCENE CAMERA 0, 46, 0, 0, 0        ' occhio 46 con eye 46 → piedi a y = 0
  ' Pavimento 100x100 (tetto a y = 0) e muro frontale z 20..30: col raggio 14 il corpo
  ' si ferma a z = 6.
  SCENE OBJECT 80, 80, 80
  SCENE BOX -50, -2, -50, 50, 0, 50
  SCENE OBJECT 120, 120, 120
  SCENE BOX -30, 0, 20, 30, 100, 30

  sens = 2
  plim = 88

  ' Copione: 9 frame di input (mask, delta x, delta y).
  DIM bm(9)
  DIM mdx(9)
  DIM mdy(9)
  FOR i = 0 TO 8
    bm(i) = 0
    mdx(i) = 0
    mdy(i) = 0
  NEXT i
  FOR i = 0 TO 3
    bm(i) = 1                      ' avanti per 4 frame: z = 4
  NEXT i
  bm(4) = 8                        ' destra: x = 1
  bm(5) = 4                        ' sinistra: x = 0
  bm(6) = 2                        ' indietro: z = 3
  bm(7) = 3                        ' su + giù insieme: si annullano
  mdx(7) = 10
  mdy(7) = 5
  mdx(8) = 10
  mdy(8) = 5

  mx = 0                            ' massimo x raggiunto: prova che destra ha MOSSO
  FOR fr = 0 TO 8
    m = bm(fr)
    b0 = m MOD 2
    b1 = (m \ 2) MOD 2
    b2 = (m \ 4) MOD 2
    b3 = (m \ 8) MOD 2
    fwd = b0 - b1
    strafe = b3 - b2
    SCENE MOVE fwd, strafe
    IF SCENE.X() > mx THEN mx = SCENE.X()
    dp = SCENE.PITCH() - mdy(fr) * sens
    IF dp > plim THEN dp = plim
    IF dp < -plim THEN dp = -plim
    SCENE TURN mdx(fr) * sens, dp - SCENE.PITCH()
  NEXT fr

  ' Ogni valore è l'unica prova di un comportamento: mx e x la coppia di strafe (destra
  ' muove, sinistra riporta), z avanti + indietro + annullamento di 1 e 2, yaw e p il
  ' mouse (delta per frame, non posizione), wz e wh la collisione e il suo bit, cp il
  ' clamp del pitch, jp e jy l'arco del salto e l'atterraggio, sy e sh il riappoggio del
  ' MOVE da in aria, ce e cy l'occhio che scende e i piedi che restano.
  ' `jg` NON è ridondante con `jy`: l'arco finisce a 0 esatto per aritmetica (8+6+4+2+0
  ' −2−4−6−8), quindi senza il ramo di atterraggio `jy` darebbe 0 lo stesso — `jg` è
  ' l'unica prova che il ramo è scattato. `se` (la quota ripristinata) è invece stato
  ' tolto: rifaceva la stessa `PLAYER` già coperta da `ce`/`cy`.
  PRINT "mx="; mx
  PRINT "x="; SCENE.X()
  PRINT "z="; SCENE.Z()
  PRINT "yaw="; SCENE.YAW()
  PRINT "p="; SCENE.PITCH()

  ' ── Muro: la faccia è a z = 20, il raggio 14 → stop a 6, HIT bit 1 ─────────────
  ' CAMERA riporta l'occhio (e quindi il corpo) all'origine: da lì 10 unità avanti
  ' arrivano al muro invece di fermarsi prima (il muro è a 20 − 14 = 6).
  SCENE CAMERA 0, 46, 0, 0, 0
  SCENE MOVE 10, 0
  PRINT "wz="; SCENE.Z()
  PRINT "wh="; SCENE.HIT()                  ' bit 1 = urto, e nessun altro bit

  ' ── Mouse-look con clamp: dy = −100 px × 2 = +200 → si ferma a 88 ──────────────
  dp = SCENE.PITCH() - (-100) * sens
  IF dp > plim THEN dp = plim
  IF dp < -plim THEN dp = -plim
  SCENE TURN 0, dp - SCENE.PITCH()
  PRINT "cp="; SCENE.PITCH()

  ' ── Salto: vy = 10, gravità 2 per frame, atterraggio con FLOORAT ───────────────
  SCENE CAMERA 0, 46, 0, 0, 0
  vy = 0
  peak = 0
  grounded = 1
  FOR fr = 1 TO 9
    IF grounded = 1 THEN
      vy = 10
      grounded = 0
    END IF
    vy = vy - 2
    SCENE LIFT vy
    feet = SCENE.Y() - SCENE.EYE()
    fl = SCENE.FLOORAT(SCENE.X(), feet, SCENE.Z())
    IF fl > -1000000 THEN
      IF feet <= fl THEN
        SCENE LIFT fl - feet       ' i piedi sono fl: LIFT li ha già portati lì
        vy = 0
        grounded = 1
      END IF
    END IF
    IF feet > peak THEN peak = feet
  NEXT fr
  PRINT "jp="; peak
  PRINT "jy="; SCENE.Y() - SCENE.EYE()
  PRINT "jg="; grounded

  ' ── MOVE da in aria: il motore riappoggia a terra entro lo step ────────────────
  ' È il motivo per cui il controllore sospende MOVE mentre `grounded` è 0: chiamarlo
  ' durante il salto rimetterebbe i piedi giù (supporto a 0 entro step 24) e il salto
  ' non si vedrebbe mai. HIT bit 4 = quota cambiata.
  SCENE CAMERA 0, 46, 0, 0, 0
  SCENE LIFT 8
  SCENE MOVE 1, 0
  PRINT "sy="; SCENE.Y() - SCENE.EYE()
  PRINT "sh="; SCENE.HIT()

  ' ── Accovacciato: l'occhio scende, i piedi restano ─────────────────────────────
  SCENE PLAYER 14, 56, 20, 24
  PRINT "ce="; SCENE.EYE()
  PRINT "cy="; SCENE.Y() - SCENE.EYE()
END SUB
