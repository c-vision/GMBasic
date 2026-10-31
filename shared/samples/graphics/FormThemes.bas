' FormThemes.bas — bottoni pixel con default di stile PER TEMA (temi esistenti).
' Ogni BUTTON cattura lo stile dal tema corrente al momento della creazione.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM clicks = 0

SUB Main()
    WINDOW THEME "Turbo"      ' classico: flat + bordo
    BUTTON 40, 30, 220, 44, "Turbo"   ON CLICK OnClick
    WINDOW THEME "Nord"       ' moderno: rounded + ombra
    BUTTON 40, 90, 220, 44, "Nord"    ON CLICK OnClick
    WINDOW THEME "Dracula"    ' moderno scuro
    BUTTON 40, 150, 220, 44, "Dracula" ON CLICK OnClick
    WINDOW THEME "RetroGreen"
    BUTTON 40, 210, 220, 44, "RetroGreen" ON CLICK OnClick
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

SUB OnClick()
    clicks = clicks + 1
    WINDOW CLOSE
END SUB
