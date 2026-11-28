' SoundOrbit.bas — audio posizionale della scena (P6, M-SND): due sorgenti e il giocatore
' che gli gira intorno.
'
' Cosa mostra:
'   * due sorgenti dichiarate una volta con `SCENE SOUND n, freq, dur, x, y, z, dist, vol`:
'     un rintocco grave (220 Hz) e uno acuto (880 Hz);
'   * il giocatore che percorre un quadrato attorno a loro, girato nella direzione di marcia;
'   * a ogni lato, guadagno e pan LETTI dalle sorgenti (`SCENE.SOUNDGAIN(n)`,
'     `SCENE.SOUNDPAN(n)`), che `RENDER` ricalcola una volta per frame dalla camera.
'
' Il listener è la CAMERA, non un punto a parte: occhio e yaw vengono da `SCENE CAMERA`
' (qui spostata da `SCENE MOVE`, che è il corpo). Le sorgenti stanno alla quota 40, vicina
' all'occhio 46: così il pan dipende dalla direzione e non dalla differenza di quota, e
' l'attenuazione racconta la distanza in pianta.
'
' Le due sorgenti sono messe di proposito a CAVALLO del primo lato — una appena dentro al
' quadrato, una appena fuori. Con up +y l'asse destro della camera vale (−cos yaw, 0, sin yaw),
' quindi a yaw 90 il +z suona a destra: la sorgente interna esce con pan positivo, quella
' esterna con pan negativo. Con tutte e due dentro, il giro le terrebbe sempre dallo stesso
' lato e il segno del pan non direbbe niente.
'
' Il tono parte con il gain/pan del momento della dichiarazione: le letture cambiano a ogni
' `RENDER`, ma un tono già avviato non si risuona (M-SND-SNAP) — è un one-shot. Headless
' (CI, nessun device) non esce audio, ma gain e pan si calcolano lo stesso: è esattamente
' ciò che rende la superficie verificabile, e questo programma stampa i valori.
' Per ascoltarlo: `SCREEN 12, 320, 180, 32`, poi `SCENE PRESENT` dopo `RENDER`.
'
' Tutto su quattro gambe: `tools/sh/gmb_c99native_run.sh Samples3D/SoundOrbit.bas`
' (nativa), `test_ilrun --engine=il|pcode|vmc` per le altre.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 320, 180, 60
  SCENE PLAYER 14, 56, 46, 24      ' raggio 14, altezza 56, occhio 46, gradino 24
  SCENE CAMERA -24, 46, -24, 90, 0 ' angolo di partenza, sguardo verso +x

  ' ── Il mondo ────────────────────────────────────────────────────────────────────
  ' Solo il pavimento: il suono non ha geometria, e un pilastro sotto alla sorgente
  ' interna (a 6 unità dal percorso, meno del raggio 14 del corpo) fermerebbe il giro.
  SCENE OBJECT 70, 80, 60
  SCENE BOX -30, -2, -30, 30, 0, 30

  ' ── Le due sorgenti (slot 1 e 2 di 16) ──────────────────────────────────────────
  '   n, freq, dur, x, y, z, dist, vol
  ' Sorgente interna, a 6 unità dal lato z = −24: al passaggio suona a destra.
  SCENE SOUND 1, 220, 4.0, -6, 40, -18, 100, 100
  ' Sorgente esterna, a 10 unità oltre lo stesso lato: al passaggio suona a sinistra,
  ' e con 80 unità di portata si sente meno lontana sul lato opposto del quadrato.
  SCENE SOUND 2, 880, 2.0, 6, 40, -34, 80, 100

  ' ── Costanti del percorso ───────────────────────────────────────────────────────
  STEP = 6         ' unità per frame
  NF = 8           ' frame per lato: 8 × 6 = 48 = un lato del quadrato
  HALF = 24        ' semilato: si passa per (±24, ±24)

  ' I quattro lati: `MOVE` è in coordinate LOCALI (avanti, strafe), quindi basta
  ' puntare lo yaw e camminare dritto. Conferma la convenzione del dominio — yaw 90
  ' guarda +x, 0 guarda +z, −90 guarda −x, −180 guarda −z — ed è il motivo per cui
  ' ogni lato si compie con una sola `MOVE STEP, 0`.
  FOR leg = 0 TO 3
    want = 90
    IF leg = 1 THEN want = 0
    IF leg = 2 THEN want = -90
    IF leg = 3 THEN want = -180

    ' TURN è relativo: si prende il delta più corto, senza far fare giri interi.
    d = want - SCENE.YAW()
    IF d > 180 THEN d = d - 360
    IF d < -180 THEN d = d + 360
    SCENE TURN d, 0

    FOR f = 0 TO NF - 1
      SCENE MOVE STEP, 0
      SCENE RENDER                 ' ricalcola gain e pan delle sorgenti dalla camera
    NEXT f

    PRINT "lato "; leg; " x="; SCENE.X(); " z="; SCENE.Z(); " yaw="; SCENE.YAW()
    PRINT "  1 (220 Hz, dentro): gain="; ROUND(SCENE.SOUNDGAIN(1), 3); " pan="; ROUND(SCENE.SOUNDPAN(1), 3)
    PRINT "  2 (880 Hz, fuori):  gain="; ROUND(SCENE.SOUNDGAIN(2), 3); " pan="; ROUND(SCENE.SOUNDPAN(2), 3)
  NEXT leg

  ' Slot mai dichiarato: le sentinelle documentate, −1 per il gain e −2 per il pan
  ' (il pan valido include −1, quindi non può fare da sentinella).
  PRINT "slot 3: gain="; SCENE.SOUNDGAIN(3); " pan="; SCENE.SOUNDPAN(3)
END SUB
