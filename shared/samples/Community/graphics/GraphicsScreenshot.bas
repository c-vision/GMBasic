' FILE: Graphics/GraphicsScreenshot.bas
' =================================================================================================
'       TEST COMPLETO PRIMITIVE GRAFICHE: FONT, PRINT, LINE (B/BF), CIRCLE, PSET, PRESET,
'       DRAW, SAVE/RESTORE SCREEN. Ogni primitiva usa un colore distinto così lo screenshot
'       (BMP) permette di verificare i pixel di ogni zona.
' =================================================================================================

SUB Main()
  ' Modalità VGA 640x480, 16 colori.
  SCREEN 12

  ' --- FONT + PRINT (testo bianco 15 in alto a sinistra) ---
  FONT "font.ttf", 12
  COLOR 15, 0
  CLS
  LOCATE 20, 20
  PRINT "FONT_PRINT_OK"

  ' --- LINE semplice (gialla 14): diagonale ---
  COLOR 14, 0
  LINE (50, 100)-(200, 130), 14

  ' --- LINE B (rettangolo bordi magenta 13) ---
  COLOR 13, 0
  LINE (50, 150)-(150, 200), 13, "B"

  ' --- LINE BF (rettangolo pieno ciano 11) ---
  COLOR 11, 0
  LINE (160, 150)-(260, 200), 11, "BF"

  ' --- CIRCLE (verde 10) ---
  COLOR 10, 0
  CIRCLE (320, 150), 50, 10

  ' --- PSET (rosso 12) poi PRESET (cancella: il rosso non deve restare) ---
  COLOR 12, 0
  PSET (300, 250), 12
  PRESET (300, 250)

  ' --- DRAW (blu 9): quadrato con comandi M/R/D/L/U ---
  COLOR 9, 0
  DRAW "M400,100 R50 D50 L50 U50"

  ' --- SAVE SCREEN / RESTORE SCREEN: CLS nero poi ripristino ---
  SAVE SCREEN "s1"
  CLS 0
  RESTORE SCREEN "s1"

  ' Attende un tasto: il runner resta aperto finché l'utente preme sulla finestra SDL.
  WAITKEY
END SUB
