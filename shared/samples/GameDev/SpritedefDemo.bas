' TAG: interactive   (apre una finestra grafica e attende WAITKEY)
' SPRITEDEF Demo
' Definisce sprite con pixel art inline

SCREEN 13
COLOR 0, 15
CLS 15
FONT "font.ttf", 12

' Definisci sprite con pixel art
SPRITEDEF START 0
"..1111.."
".111111."
"11111111"
"11111111"
".111111."
"..1111.."
SPRITEDEF END

' Definisci secondo sprite
SPRITEDEF START 1
"..33.."
".3333."
"333333"
"..33.."
"..33.."
".3333."
SPRITEDEF END

' Mostra cosa è stato creato
DIM msg$ = "SPRITEDEF: create 2 sprite (0, 1)"
LOCATE 10, 10
PRINT msg$

LOCATE 30, 10
PRINT "Usa SPRITE LOAD per caricare in un handle"
LOCATE 50, 10
PRINT "Usa SPRITE SET POSITION per muovere"
LOCATE 70, 10
PRINT "Usa SPRITE ON per mostrare"

SPRITE SET POSITION 0, 160, 130
SPRITE ON 0
SPRITE SET POSITION 1, 110, 130
SPRITE ON 1
SPRITE SET SCALE 0, 8, 8
SPRITE SET SCALE 1, 8, 8
SYNC
SLEEP 2500
WAITKEY
SCREEN 0
