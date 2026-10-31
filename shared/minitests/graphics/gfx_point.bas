' EXPECT: 14|12|0|0|13|0
' Readback POINT (pagina software C) dopo PSET/LINE B/BF/CIRCLE/PRESET — parità 4 backend.
' I valori si leggono a schermo attivo in variabili, poi SCREEN 0 e PRINT su stdout
' (su stdout perché EXPECT/gate/parity confrontano l'output, non la pagina grafica).
IMPORT GRAPHICS
SCREEN 12
CLS
PSET (10, 10), 14
LINE (20, 20)-(30, 30), 12, "BF"
LINE (40, 40)-(60, 60), 10, "B"
CIRCLE (100, 100), 20, 13
DIM p1, p2, p3, p4, p5, p6
p1 = GRAPHICS.POINT(10, 10)
p2 = GRAPHICS.POINT(25, 25)
p3 = GRAPHICS.POINT(50, 41)
p4 = GRAPHICS.POINT(50, 50)
p5 = GRAPHICS.POINT(120, 100)
PRESET (10, 10)
p6 = GRAPHICS.POINT(10, 10)
SCREEN 0
PRINT p1
PRINT p2
PRINT p3
PRINT p4
PRINT p5
PRINT p6
