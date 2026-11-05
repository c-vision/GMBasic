' EXPECT: c= 1  w= 6  h= 3  bg= 1073741994  px= 1073785344  oob=-1  bw=-1  save= 0  load= 2  px= 1073785344  blit= 1073785344  freed=-1
' CANVAS (canvas offscreen): NEW/WIDTH/HEIGHT/CLEAR/SET/GET/SAVE/LOAD/BLIT/FREE.
' Deterministico e headless: opera solo su buffer RGBA8, nessuna finestra.
' Colori: indice palette (CLEAR 1 → 0x400000AA, SET 2 → 0x4000AA00); GET ritorna
' il colore impacchettato con RGB flag, riutilizzabile come argomento colore.
IMPORT GRAPHICS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
P$ = WK$ + "/canvas_core.png"

SUB Main()
  DIM c AS LONG, w AS LONG, h AS LONG, a AS LONG, b AS LONG, c2 AS LONG

  CANVAS NEW 6, 3 INTO c
  PRINT "c="; c;
  CANVAS WIDTH c INTO w
  CANVAS HEIGHT c INTO h
  PRINT " w="; w; " h="; h;

  CANVAS CLEAR c, 1
  CANVAS GET c, 0, 0 INTO a
  CANVAS SET c, 5, 2, 2
  CANVAS GET c, 5, 2 INTO b
  PRINT " bg="; a; " px="; b;

  ' fuori range: -1 (nessuna scrittura/lettura fuori buffer)
  CANVAS GET c, 99, 0 INTO a
  CANVAS WIDTH 99 INTO w
  PRINT " oob="; a; " bw="; w;

  ' save + load
  CANVAS SAVE c, P$
  DIM ok AS LONG
  CANVAS SAVE c, P$ INTO ok
  PRINT " save="; ok;
  CANVAS LOAD P$ INTO c2
  CANVAS GET c2, 5, 2 INTO b
  PRINT " load="; c2; " px="; b;

  ' blit del canvas 1 sul canvas 2 (target attivo)
  CANVAS USE c2
  CANVAS BLIT c, 0, 0
  CANVAS GET c2, 5, 2 INTO b
  CANVAS END
  PRINT " blit="; b;

  CANVAS FREE c
  CANVAS FREE c2
  CANVAS WIDTH c INTO w
  PRINT " freed="; w
END SUB
