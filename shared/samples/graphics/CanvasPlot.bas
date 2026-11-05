REM FILE: CanvasPlot.bas — pacchetto stdlib: canvas offscreen (comando CANVAS).
REM Scenario: disegnare un grafico sinusoidale in un buffer RGBA8 96x48, salvarlo in PNG,
REM riaprirlo, copiarlo su un secondo canvas e verificarne i pixel. Usa TUTTI i sottocomandi
REM del comando: NEW, WIDTH, HEIGHT, CLEAR, USE, SET, GET, BLIT, SAVE, LOAD, FREE, END.
REM
REM Il disegno avviene con CANVAS SET (pixel espliciti): è l'unico modo che produce lo
REM stesso identico bitmap su tutti e 4 i backend anche in headless. Con un motore grafico
REM attivo (finestra) dentro `CANVAS USE c` funzionano anche PSET/LINE/CIRCLE/PAINT/DRAW,
REM che disegnano sul canvas invece che sullo schermo.
IMPORT GRAPHICS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
P$ = WK$ + "/canvas_plot.png"

SUB Main()
  DIM c AS LONG, c2 AS LONG
  DIM w AS LONG, h AS LONG
  DIM x AS LONG, y AS LONG, col AS LONG, p0 AS LONG, q0 AS LONG
  DIM ok AS LONG

  ' ── 1. canvas 96x48, pulito a nero ──────────────────────────────────────────────
  CANVAS NEW 96, 48 INTO c
  CANVAS WIDTH c INTO w
  CANVAS HEIGHT c INTO h
  PRINT "canvas="; c; " "; w; "x"; h
  CANVAS CLEAR c, 0

  ' ── 2. assi (pixel espliciti: nessuna dipendenza dal servizio grafico) ──────────
  FOR x = 0 TO w - 1
    CANVAS SET c, x, h \ 2, 1
  NEXT x
  FOR y = 0 TO h - 1
    CANVAS SET c, 0, y, 1
  NEXT y

  ' ── 3. curva: y = centro - 18*sin(2πx/(w-1)) ───────────────────────────────────
  FOR x = 0 TO w - 1
    y = h \ 2 - INT(18 * SIN(6.283185307179586 * x / (w - 1)))
    IF y < 0 THEN y = 0
    IF y > h - 1 THEN y = h - 1
    CANVAS SET c, x, y, 14
  NEXT x

  ' ── 4. lettura di un pixel noto (x=0 → sin=0 → centro, colore 14) ───────────────
  CANVAS GET c, 0, h \ 2 INTO p0
  PRINT "p0="; p0

  ' ── 5. salvataggio PNG e riapertura ─────────────────────────────────────────────
  CANVAS SAVE c, P$ INTO ok
  PRINT "save="; ok
  CANVAS LOAD P$ INTO c2
  CANVAS GET c2, 0, h \ 2 INTO q0
  PRINT "load="; c2; " p0="; q0

  ' ── 6. blit del primo canvas sul secondo (target = canvas attivo) ───────────────
  CANVAS USE c2
  CANVAS BLIT c, 0, 0
  CANVAS GET c2, 0, h \ 2 INTO q0
  CANVAS END
  PRINT "blit="; q0

  ' ── 7. pulizia ──────────────────────────────────────────────────────────────────
  CANVAS FREE c
  CANVAS FREE c2
  CANVAS WIDTH c INTO w
  PRINT "freed="; w
END SUB
