' FILE: Clipper/WindowDesktops.bas
' Dimostrazione dei Desktop Virtuali (Virtual Desktops) con GMBasic Window Manager.

IMPORT WINDOWS

SUB Main()
    PRINT "=== WindowDesktops Demo ==="

    ' 1. Verifica desktop iniziale (1)
    d = WINDOWS.DESKTOP()
    PRINT "Desktop corrente: "; d

    ' 2. Crea finestre sul Desktop 1
    WINDOW DEFINE Doc1 FROM 2, 2 TO 12, 38 TITLE "Documento 1" DOUBLE SHADOW
    WINDOW Doc1 PRINT "Finestra attiva sul Desktop 1"

    WINDOW DEFINE Tools FROM 4, 42 TO 18, 76 TITLE "Strumenti D1" DOUBLE SHADOW
    WINDOW Tools PRINT "Pannello strumenti su Desktop 1"

    ' 3. Passa al Desktop 2 e crea finestre
    WINDOW DESKTOP 2
    PRINT "Passato a Desktop: "; WINDOWS.DESKTOP()

    WINDOW DEFINE DatabaseView FROM 3, 5 TO 16, 50 TITLE "Archivio DB" DOUBLE SHADOW
    WINDOW DatabaseView PRINT "Finestra isolata sul Desktop 2"

    ' 4. Sposta la finestra Doc1 da Desktop 1 a Desktop 2
    WINDOW Doc1 DESKTOP 2
    PRINT "Spostata finestra Doc1 su Desktop 2"

    ' 5. Ritorna al Desktop 1
    WINDOW DESKTOP 1
    PRINT "Ritorno a Desktop: "; WINDOWS.DESKTOP()

    ' 6. Torna al Desktop 2
    WINDOW DESKTOP 2
    PRINT "Ritorno a Desktop: "; WINDOWS.DESKTOP()

    ' Ripristina desktop 1 e schermo base
    WINDOW DESKTOP 1
    WINDOW SELECT 0
    PRINT "=== Test Desktop Virtuali completato con successo ==="
END SUB
