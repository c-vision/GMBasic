' Walk.bas — camminata in prima persona: pad, mouse-look, salto, accovacciato (P6).
'
' Il ciclo di gioco che il piano (§6, P6) chiede come accettazione: un corpo che cammina
' con i comandi che esistono già — `SCENE PLAYER/MOVE/TURN/LIFT`, `SCENE.FLOORAT` — e i
' lettori di input del linguaggio: `JOY.MASK(0)` (bitmask 1 su, 2 giù, 4 sinistra,
' 8 destra, 16 fuoco, 32 secondo tasto) e `MOUSE.X/Y` (posizione, da cui si ricava il
' delta). Headless quei lettori valgono 0 e non aprono niente: lo stesso programma gira
' in CI senza device.
'
' Perché un copione. Con `LIVE = 0` l'input viene da una tabella di frame (deterministico:
' stesso percorso su tutte e quattro le gambe). Con `LIVE = 1` le stesse righe leggono il
' pad e il mouse veri — è l'unica differenza fra il test e il gioco.
'
' Il controllore, in ordine, a ogni frame (tutto dentro il task GPM: niente handler su
' thread esterni, §0ter / C-GPM):
'   1. mask → avanti/indietro e destra/sinistra (bit che si annullano se opposti);
'   2. delta del mouse → `SCENE TURN`, con il pitch limitato a ±88 (TURN rifiuta ±89:
'      il clamp è del controllore);
'   3. fuoco + piedi a terra → salto; gravità per frame, atterraggio chiesto a
'      `SCENE.FLOORAT(x, piedi, z)` — la quota non si indovina;
'   4. passo con `MOVE`, **solo a piedi a terra**: `MOVE` riappoggia il corpo alla
'      superficie entro lo step (è così che si scendono le scale) e da in aria
'      annullerebbe il salto;
'   5. secondo tasto → `SCENE PLAYER` con l'occhio a 20 invece di 46: accovacciato.
'      `PLAYER` conserva i piedi, quindi ci si accovaccia senza sprofondare.
'
' Per giocarci davvero: `SCREEN 12, 320, 180, 32` prima del ciclo, `LIVE = 1`, e
' `SCENE PRESENT` (o `SCENE SAVE`) dopo `RENDER`; il frame rate lo limita il `TICK` del
' programma, mai un'attesa dentro il motore.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 320, 180, 60
  SCENE PLAYER 14, 56, 46, 24      ' raggio 14, altezza 56, occhio 46, gradino 24
  SCENE CAMERA 0, 46, 0, 0, 0      ' occhio 46 con eye 46 → piedi a y = 0

  ' ── Il mondo: pavimento, muro di fondo, colonne e un gradino ────────────────────
  SCENE OBJECT 80, 80, 80
  SCENE BOX -40, -2, -40, 40, 0, 40
  ' Il corpo è un cilindro di raggio 14: niente scenografia entro 15 unità dal percorso,
  ' altrimenti il primo passo nasce già addosso a un ostacolo e la spinta lo sposta.
  ' UN oggetto = UNA scatola: l'AABB di un oggetto è l'unione dei suoi box, quindi due
  ' pareti lontane nello stesso oggetto darebbero una scatola che contiene il giocatore
  ' (la collisione lo espellerebbe). Per ogni box remoto: un `SCENE OBJECT` suo.
  SCENE OBJECT 150, 120, 90
  SCENE BOX -40, 0, 30, 40, 60, 32      ' muro alle spalle, a z = 30
  SCENE OBJECT 160, 130, 100
  SCENE BOX 40, 0, -40, 42, 60, 40      ' muro di fondo: si cammina verso +x
  FOR i = 0 TO 3
    SCENE OBJECT 90 + i * 30, 150, 200 - i * 20
    SCENE BOX 6 + i * 5, 0, 24, 8 + i * 5, 12, 26    ' colonne di sfondo
  NEXT i
  SCENE OBJECT 200, 160, 60
  SCENE BOX 24, 0, -12, 30, 4, -6        ' gradino basso (tetto 4 ≤ step 24)

  ' ── Costanti del controllore ───────────────────────────────────────────────────
  SPD = 1          ' unità per frame
  SENS = 2         ' gradi di rotazione per pixel di mouse
  PLIM = 88        ' limite del pitch (TURN rifiuta ±89)
  GRAV = 2         ' unità per frame al quadrato
  JUMPV = 10       ' velocità verticale di partenza
  NF = 40          ' frame del copione
  LIVE = 0         ' 1 = pad e mouse veri (finestra), 0 = copione

  vy = 0
  peak = 0
  grounded = 1
  lastmx = 0
  lastmy = 0

  FOR fr = 0 TO NF - 1
    ' ── 1. Input: copione o dispositivi, stessa forma ────────────────────────────
    IF LIVE = 1 THEN
      m = JOY.MASK(0)
      dx = MOUSE.X - lastmx
      dy = MOUSE.Y - lastmy
      lastmx = MOUSE.X
      lastmy = MOUSE.Y
    ELSE
      m = 0
      dx = 0
      dy = 0
      IF fr <= 7 THEN
        m = 1                      ' avanti: z = 8
      END IF
      IF fr >= 8 AND fr <= 11 THEN
        m = 8                      ' destra: x = 4
      END IF
      IF fr = 12 THEN
        dx = 45                    ' mezzo giro a destra: 45 px × 2 = 90°
      END IF
      IF fr >= 16 AND fr <= 23 THEN
        m = 1                      ' avanti a yaw 90: x = 12
      END IF
      IF fr >= 24 AND fr <= 27 THEN
        m = 17                     ' avanti + fuoco: salto in corsa
      END IF
      IF fr >= 30 AND fr <= 33 THEN
        m = 32                     ' accovacciato
      END IF
    END IF

    ' ── 2. Mask → vettore (bit 0 su, bit 1 giù, bit 2 sinistra, bit 3 destra) ─────
    b0 = m MOD 2
    b1 = (m \ 2) MOD 2
    b2 = (m \ 4) MOD 2
    b3 = (m \ 8) MOD 2
    b4 = (m \ 16) MOD 2
    b5 = (m \ 32) MOD 2
    fwd = b0 - b1
    strafe = b3 - b2

    ' ── 3. Mouse-look (il clamp è qui: TURN rifiuta ±89) ───────────────────────────
    dp = SCENE.PITCH() - dy * SENS
    IF dp > PLIM THEN dp = PLIM
    IF dp < -PLIM THEN dp = -PLIM
    SCENE TURN dx * SENS, dp - SCENE.PITCH()

    ' ── 4. Salto e gravità: si atterra sulla quota chiesta a FLOORAT ──────────────
    IF b4 = 1 AND grounded = 1 THEN
      vy = JUMPV
      grounded = 0
    END IF
    IF grounded = 0 THEN
      vy = vy - GRAV
      SCENE LIFT vy
      feet = SCENE.Y() - SCENE.EYE()
      IF feet > peak THEN peak = feet
      fl = SCENE.FLOORAT(SCENE.X(), feet, SCENE.Z())
      IF fl > -1000000 THEN
        IF feet <= fl THEN
          SCENE LIFT fl - feet
          vy = 0
          grounded = 1
        END IF
      END IF
    END IF

    ' ── 5. Passo, SOLO a piedi a terra ────────────────────────────────────────────
    ' `MOVE` riappoggia il corpo alla superficie entro lo step (è così che si scendono
    ' le scale): chiamarlo da in aria annullerebbe il salto riportandolo a terra. In un
    ' gioco è la riga che distingue un salto vero da uno che "non si stacca mai".
    IF grounded = 1 THEN
      SCENE MOVE fwd * SPD, strafe * SPD
    END IF

    ' ── 6. Accovacciato: cambia l'occhio, non i piedi ─────────────────────────────
    IF b5 = 1 THEN
      SCENE PLAYER 14, 56, 20, 24
    ELSE
      SCENE PLAYER 14, 56, 46, 24
    END IF

    ' ── 7. Disegno del frame (in una finestra qui va anche `SCENE PRESENT`) ────────
    SCENE RENDER
  NEXT fr

  PRINT "frame="; NF
  PRINT "x="; SCENE.X()
  PRINT "z="; SCENE.Z()
  PRINT "y="; SCENE.Y() - SCENE.EYE()
  PRINT "yaw="; SCENE.YAW()
  PRINT "pitch="; SCENE.PITCH()
  PRINT "eye="; SCENE.EYE()
  PRINT "jump="; peak
  PRINT "drawn="; SCENE.DRAWN()
  PRINT "hash="; SCENE.HASH()
END SUB
