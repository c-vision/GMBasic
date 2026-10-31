' ClipperReadGfx.bas — @SAY/@GET + READ su schermo GRAFICO.
' I campi sono controlli CONTROLS (text-input) e si modificano con la tastiera;
' in grafica le coordinate (riga, colonna) sono convertite in pixel.
' Verifica: con GMB_FORM_AUTOKEY="8,Mario;16,TAB;24,42;32,ENTER" stampa
' nome=Mario / eta=42 (il valore iniziale della variabile è quello mostrato).
SCREEN 12, 760, 460, 32
WINDOW THEME "soft"
FONT "font.ttf", 14
DIM nome AS STRING
DIM eta AS INTEGER
nome = "Mario"
eta = 42
@ 3, 6 SAY "Nome:"
@ 3, 20 GET nome SIZE 20
@ 6, 6 SAY "Eta:"
@ 6, 20 GET eta TYPE "spinner" MIN 0 MAX 120 STEP 1 SIZE 12
@ 9, 6 SAY "ENTER conferma, ESC annulla"
READ
SAVE SCREEN "work/clipper_read_gfx.png"
SCREEN 0
PRINT "nome="; nome
PRINT "eta="; eta
