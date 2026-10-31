' FkeybarSingle.bas — UN controllo solo: FUNCTIONKEYBAR. Premi F1..F10: il tasto si illumina.
' ESC per chiudere.
IMPORT WINDOWS
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12
WINDOW THEME "light"
FONT "/System/Library/Fonts/SFNS.ttf", 20
CLS 1
COLOR 15, 1
DRAW TEXT "Premi F1..F10: il tasto si illumina.  ESC per chiudere.", 24, 40
FUNCTIONKEYBAR 4, 2, 70, "Apri|Salva|Cerca|Esegui|Aiuto|Esci"
BUTTON 520, 420, 90, 28, "OK" ON CLICK OnOk

DIM k AS STRING
DIM quit AS INTEGER
DIM i AS INTEGER
quit = 0
FOR i = 1 TO 10
    WINDOW PUMP 16
NEXT i
SAVE SCREEN "work/fkb_single.png"
TICK 60
    INKEY k
    IF k = CHR$(27) THEN EXIT TICK
    IF quit <> 0 THEN EXIT TICK
    WINDOW PUMP 16
END TICK
PRINT "chiuso"
END
SUB OnOk
    quit = 1
END SUB
