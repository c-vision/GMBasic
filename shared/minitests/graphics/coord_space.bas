' EXPECT: a= 1|b= 2|c= 0|c2= 2|ok=1
' Spazio coordinate DICHIARATO dei controlli (SCREEN ... USE CELLS|PIXELS + clausola USE sul controllo).
' Deterministico e headless: SCREEN 0 riporta subito in modalità testo, nessuna finestra.
'
' Semantica:
'   nessuna clausola      → AUTO (default): pixel in grafica, CELLE in TUI (nessuna conversione)
'   USE CELLS  / metric 1 → CellsToPixel : su schermo grafico x,y,w,h vengono moltiplicati per (cw,ch)
'   USE PIXELS / metric 2 → PixelToCells : su TUI a celle x,y,w,h vengono divisi per (8,16)
IMPORT CONTROLS
IMPORT GRAPHICS

DIM m AS INTEGER

SUB Main()
  ' Forma ESTESA (token, senza virgole): WIDTH/HEIGHT/USE.
  SCREEN 0 WIDTH 640 HEIGHT 480 USE CELLS
  SCREEN 0
  ' Forma COMPATTA (virgole) con sugar: SCREEN mode, w, h, bpp, CELLS|PIXELS.
  SCREEN 0, 640, 480, 32, PIXELS
  SCREEN 0

  ' Clausole USE sui controlli: metric 1, metric 2, default 0.
  BUTTON 2, 2, 12, 2, "A" NAME "ba" USE CELLS
  BUTTON 2, 6, 12, 2, "B" NAME "bb" USE PIXELS
  BUTTON 2, 10, 12, 2, "C" NAME "bc"

  CONTROL GET "ba", "metric" INTO m
  PRINT "a="; m
  CONTROL GET "bb", "metric" INTO m
  PRINT "b="; m
  CONTROL GET "bc", "metric" INTO m
  PRINT "c="; m
  CONTROL SET "bc", "metric", 2
  CONTROL GET "bc", "metric" INTO m
  PRINT "c2="; m
  PRINT "ok=1"
END SUB
