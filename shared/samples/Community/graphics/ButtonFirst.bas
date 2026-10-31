' ButtonFirst.bas — il PRIMO controllo, end-to-end: render + click reale.
' Il click incrementa il contatore e lo mostra a schermo.
IMPORT WINDOWS
IMPORT CONTROLS

SCREEN 12, 640, 320, 32
DIM clicks AS INTEGER

SUB OnClick()
    clicks = clicks + 1
    DRAW TEXT "click: " + STR$(clicks), 60, 130
END SUB

SUB Main()
    WINDOW THEME "light"
    WINDOW SHOW
    BUTTON 60, 50, 200, 44, "Cliccami" ON CLICK OnClick STYLE "rounded radius=8"
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

