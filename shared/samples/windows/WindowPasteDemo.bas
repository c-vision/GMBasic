' FILE: Windows/WindowPasteDemo.bas
' Demo: finestra vuota → dopo ~3 secondi viene incollato testo lungo (scrollbar visibili).
' Mostra il contenuto che supera l'altezza della finestra → compaiono le scrollbar.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    SCREEN 12
    FONT "font.ttf", 12

    ' ── Finestra vuota ──
    WINDOW DEFINE Main FROM 3, 3 TO 20, 76 TITLE "Editor"
    WINDOW Main CLEAR
    PRINT "Finestra vuota... attendo 3s"

    ' ── Attesa (finestra resta vuota) ──
    SLEEP 3000

    ' ── "Incolla" testo lungo (40 righe → scrollbar verticale) ──
    WINDOW Main CLEAR
    WINDOW Main PRINT "=== Documento incollato ==="
    WINDOW Main PRINT "Inizio del contenuto"
    DIM i
    FOR i = 1 TO 40
        WINDOW Main PRINT "Riga " + STR(i) + ": Lorem ipsum dolor sit amet, consectetur"
    NEXT i
    WINDOW Main PRINT "=== Fine documento ==="
    PRINT "Testo incollato (40 righe) — scrollbar attiva"

    ' ── Scroll di prova ──
    SLEEP 1500
    WINDOW Main SCROLL 10
    PRINT "Scrolled di 10 righe"

    ' ── Attesa finale (ESC chiude) ──
    WAITKEY
END SUB
