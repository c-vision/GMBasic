IMPORT CONTROLS
SCREEN 12, 480, 240, 32
DIM sel AS INTEGER
DIM voci(2) AS STRING
voci(0) = "Alfa" : voci(1) = "Beta" : voci(2) = "Gamma"
LISTBOX 60, 50, 200, 120, voci INTO sel
DIM f AS INTEGER
FOR f = 1 TO 120 : WINDOW PUMP 5 : NEXT
SCREEN 0
PRINT "sel="; sel
