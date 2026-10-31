' FILE: Clipper/WindowTurboVisionGui.bas
' Finestre Turbo Vision renderizzate su finestra grafica Cocoa/SDL (SCREEN 12).
' Mostra finestre sovrapposte con ombre 3D reali, retino desktop, barre e controlli.

IMPORT WINDOWS

IMPORT CONTROLS
IMPORT GRAPHICS

SUB Main()
    ' 1. Inizializza schermo grafico SDL2 (640x480)
    SCREEN 12

    ' 2. Imposta il tema Turbo Vision autentico
    WINDOW THEME "Turbo"

    ' 3. Finestra principale Editor (attiva, bordo doppio, ombra 3D)
    WINDOW DEFINE Editor FROM 2, 3 TO 16, 44 TITLE "MAIN.BAS" DOUBLE SHADOW
    WINDOW Editor PRINT "PROGRAM TurboVisionDemo"
    WINDOW Editor PRINT "  ' Esempio di codice GMBasic"
    WINDOW Editor PRINT "  IMPORT WINDOWS"
    WINDOW Editor PRINT "  WINDOW THEME ""Turbo"""
    WINDOW Editor PRINT "  WINDOW DEFINE Editor FROM 2, 3 TO 16, 44"
    WINDOW Editor PRINT "  PRINT ""Hello, Turbo Vision!"""
    WINDOW Editor PRINT "  DIM counter AS INTEGER"
    WINDOW Editor PRINT "  FOR i = 1 TO 100"
    WINDOW Editor PRINT "    counter = counter + i"
    WINDOW Editor PRINT "  NEXT i"
    WINDOW Editor PRINT "  PRINT ""Totale: ""; counter"
    WINDOW Editor PRINT "END PROGRAM"

    ' 4. Finestra secondaria Output Console
    WINDOW DEFINE OutputWin FROM 4, 38 TO 14, 76 TITLE "Output Console" DOUBLE SHADOW
    WINDOW OutputWin PRINT "Compilazione completata con successo."
    WINDOW OutputWin PRINT "Memoria allocata: 64 KB"
    WINDOW OutputWin PRINT "Desktop attivo: D1"
    WINDOW OutputWin PRINT "Ombra 3D: Attiva (2 col x 1 row)"
    WINDOW OutputWin PRINT "Stato: IN ESECUZIONE"

    ' 5. Finestra Guida Turbo Vision
    WINDOW DEFINE HelpWin FROM 12, 10 TO 21, 65 TITLE "Guida Turbo Vision" DOUBLE SHADOW
    WINDOW HelpWin PRINT "Comandi disponibili:"
    WINDOW HelpWin PRINT " [■] Chiudi finestra corrente"
    WINDOW HelpWin PRINT " [▲] Ingrandisci / Zoom finestra"
    WINDOW HelpWin PRINT " [▼] Riduci a icona sulla barra"
    WINDOW HelpWin PRINT " F6  Passa alla finestra successiva"

    ' Attiva l'editor
    WINDOW Editor ACTIVATE
    WINDOW SELECT Editor

    ' Barra di stato
    WINDOW SELECT 0
    STATUSBAR "F1 Aiuto  F2 Salva  F3 Carica  F5 Zoom  F6 Succ  F10 Menu  Alt+X Esci"

    PRINT "=== Turbo Vision GUI Aperto sulla finestra Cocoa ==="

    ' Mantiene la finestra aperta e visibile a schermo per 20 secondi
    DIM i AS INTEGER
    FOR i = 1 TO 20
        SLEEP 1000
    NEXT i
END SUB
