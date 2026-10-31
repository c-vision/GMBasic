' FILE: Clipper/WindowThemesShowcase.bas
' Panoramica di tutti i temi visivi supportati e degli stili di cornice (Double, Single, Rounded).

IMPORT WINDOWS
IMPORT CONTROLS

SUB Main()
    PRINT "=== WindowThemesShowcase ==="

    ' 1. Finestra in stile Double (default Turbo Vision)
    WINDOW DEFINE WinDouble FROM 2, 2 TO 12, 38 TITLE "Cornice Doppia" DOUBLE SHADOW
    WINDOW WinDouble PRINT "Stile cornice predefinito Turbo Vision."
    WINDOW WinDouble PRINT "Bordi doppi con angoli ╔ ╗ ╚ ╝."
    WINDOW WinDouble PRINT "Pulsante chiudi: [■] in alto a sinistra."
    WINDOW WinDouble PRINT "Pulsante zoom:   [▲] in alto a destra."

    ' 2. Finestra in stile Rounded (Unicode)
    WINDOW DEFINE WinRounded FROM 5, 42 TO 16, 78 TITLE "Cornice Arrotondata" ROUNDED SHADOW
    WINDOW WinRounded PRINT "Stile cornice moderno arrotondato."
    WINDOW WinRounded PRINT "Angoli dolci con caratteri ╭ ╮ ╰ ╯."
    WINDOW WinRounded PRINT "Ombreggiatura 3D attiva a destra e sotto."

    ' 3. Finestra in stile Single (classica linea singola)
    WINDOW DEFINE WinSingle FROM 14, 10 TO 22, 55 TITLE "Cornice Singola" SINGLE SHADOW
    WINDOW WinSingle PRINT "Stile compatto a linea singola ┌ ┐ └ ┘."

    ' Ciclo dimostrativo attraverso le palette disponibili
    DIM themes(7) AS STRING
    themes(0) = "Turbo"
    themes(1) = "QBasic"
    themes(2) = "Nord"
    themes(3) = "Dracula"
    themes(4) = "Monokai"
    themes(5) = "SolarizedDark"
    themes(6) = "RetroGreen"
    themes(7) = "Amber"

    FOR i = 0 TO 7
        t$ = themes(i)
        WINDOW THEME t$
        PRINT "Applicato tema: "; WINDOWS.THEME()
    NEXT i

    ' Ripristina Turbo Vision
    WINDOW THEME "Turbo"
    WINDOW SELECT 0
    STATUSBAR "F1 Aiuto  Alt+X Esci"
    PRINT "=== Showcase Temi completato ==="
END SUB
