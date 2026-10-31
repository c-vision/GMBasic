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
@ 6, 20 GET eta SIZE 12
READ
SAVE SCREEN "work/hl.png"
SCREEN 0
PRINT "nome="; nome
PRINT "eta="; eta
