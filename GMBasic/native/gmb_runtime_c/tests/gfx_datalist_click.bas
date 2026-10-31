IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 700, 400, 32
WINDOW THEME "soft"
FONT "font.ttf", 14
DIM n AS INTEGER
DIM voci(2) AS STRING
voci(0) = "Alfa" : voci(1) = "Beta" : voci(2) = "Gamma"
DATALIST 3, 3, 24, 8, voci INTO n
DIM f AS INTEGER
FOR f = 1 TO 40 : WINDOW PUMP 16 : NEXT
SAVE SCREEN "/tmp/dl_soft.png"
SCREEN 0
PRINT "sel="; n
