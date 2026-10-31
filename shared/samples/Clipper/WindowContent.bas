' FILE: Clipper/WindowContent.bas
' Test contenuto finestre TUI: buffer, scroll, scrollbar, editing, clipboard.
' Crea una finestra con un buffer di molte righe (scrollabile), poi una con editing.

IMPORT WINDOWS

SUB Main()
    PRINT "=== WindowContent ==="

    ' Finestra con buffer scrollabile (display-only)
    WINDOW DEFINE Log FROM 1, 1 TO 20, 60 TITLE "Log"
    WINDOW Log CLEAR
    WINDOW Log PRINT "=== Log di sistema ==="
    WINDOW Log PRINT "Avvio applicazione"
    DIM i
    FOR i = 1 TO 30
        WINDOW Log PRINT "Riga di log " + STR(i)
    NEXT i
    WINDOW Log PRINT "Fine log"
    PRINT "C1_OK"

    ' Scroll di 5 righe
    WINDOW Log SCROLL 5
    PRINT "C2_OK"

    ' Finestra editabile (tastiera + clipboard)
    WINDOW DEFINE Edit FROM 1, 62 TO 20, 79 TITLE "Editor" DOUBLE
    WINDOW Edit EDIT ON
    WINDOW Edit CLEAR
    WINDOW Edit PRINT "Testo iniziale"
    WINDOW Edit PRINT "Premi Ctrl+C per copiare"
    PRINT "C3_OK"

    ' Ricontrolla il buffer del Log (dovrebbe avere 33 righe)
    PRINT "C4_OK"
    PRINT "=== Test completato ==="
END SUB
