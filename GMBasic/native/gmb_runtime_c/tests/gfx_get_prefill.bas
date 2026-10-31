SCREEN 12, 760, 460, 32
WINDOW THEME "soft"
FONT "font.ttf", 14
DIM nome AS STRING
DIM eta AS INTEGER
nome = "Mario"
eta = 42
@ 3, 6 SAY "Nome:"
@ 3, 20 GET nome PICTURE "@!" SIZE 20
@ 6, 6 SAY "Eta:"
@ 6, 20 GET eta TYPE "spinner" MIN 0 MAX 120 STEP 1 SIZE 12
READ
SCREEN 0
PRINT "nome="; nome
PRINT "eta="; eta
