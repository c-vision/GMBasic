' FILE: Clipper/WindowTurboVisionApp.bas
' Applicazione dimostrativa completa in stile Borland Turbo Vision TUI.
' Mostra finestre sovrapposte con ombre 3D reali, barra di stato,
' editor di testo con scrollbar e finestre di dialogo informative.

IMPORT WINDOWS

IMPORT CONTROLS
SUB Main()
    PRINT "=== Turbo Vision Application ==="

    ' Imposta il tema autentico Turbo Vision
    WINDOW THEME "Turbo"

    ' 1. Finestra Editor di Codice (attiva, bordo doppio, ombra 3D)
    WINDOW DEFINE Editor FROM 2, 3 TO 17, 45 TITLE "MAIN.BAS" DOUBLE SHADOW
    WINDOW Editor PRINT "PROGRAM TurboVisionDemo"
    WINDOW Editor PRINT "  ' Esempio di codice GMBasic"
    WINDOW Editor PRINT "  IMPORT WINDOWS"
    WINDOW Editor PRINT "  WINDOW THEME ""Turbo"""
    WINDOW Editor PRINT "  WINDOW DEFINE Editor FROM 2, 3 TO 17, 45"
    WINDOW Editor PRINT "  PRINT ""Hello, Turbo Vision!"""
    WINDOW Editor PRINT "  DIM counter AS INTEGER"
    WINDOW Editor PRINT "  FOR i = 1 TO 100"
    WINDOW Editor PRINT "    counter = counter + i"
    WINDOW Editor PRINT "  NEXT i"
    WINDOW Editor PRINT "  PRINT ""Totale: ""; counter"
    WINDOW Editor PRINT "END PROGRAM"

    ' 2. Finestra Ispezione & Output (sovrapposta a destra)
    WINDOW DEFINE OutputWin FROM 4, 40 TO 15, 77 TITLE "Output Console" DOUBLE SHADOW
    WINDOW OutputWin PRINT "Compilazione completata con successo."
    WINDOW OutputWin PRINT "Memoria allocata: 64 KB"
    WINDOW OutputWin PRINT "Desktop attivo: D1"
    WINDOW OutputWin PRINT "Ombra 3D: Attiva (2 col x 1 row)"
    WINDOW OutputWin PRINT "Stato: IN ESECUZIONE"

    ' 3. Finestra Info & Aiuto (in basso a sinistra)
    WINDOW DEFINE HelpWin FROM 12, 10 TO 21, 65 TITLE "Guida Turbo Vision" DOUBLE SHADOW
    WINDOW HelpWin PRINT "Comandi disponibili:"
    WINDOW HelpWin PRINT " [■] Chiudi finestra corrente"
    WINDOW HelpWin PRINT " [▲] Ingrandisci / Zoom finestra"
    WINDOW HelpWin PRINT " [▼] Riduci a icona sulla barra"
    WINDOW HelpWin PRINT " F6  Passa alla finestra successiva"

    ' Seleziona l'editor come finestra principale focalizzata
    WINDOW Editor ACTIVATE
    WINDOW SELECT Editor

    ' Barra di stato con shortcut classici Borland
    WINDOW SELECT 0
    STATUSBAR "F1 Aiuto  F2 Salva  F3 Carica  F5 Zoom  F6 Succ  F10 Menu  Alt+X Esci"

    PRINT "=== Turbo Vision Desktop Pronto (Premi un tasto per chiudere) ==="
    WAITKEY
END SUB
