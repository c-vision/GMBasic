' FILE: Windows/WindowColors.bas
' Demo COLOR polimorfico sulle finestre pixel: schermo grafico con sfondo blu,
' una finestra con sfondo ROSSO e scritte GIALLE nell'header (titlebar), nessun
' altro testo. Nessun comando nuovo: COLOR fg,bg con finestra attiva imposta
' testo/titolo (fg) e sfondo (bg) della finestra — come PRINT/LOCATE/CLS.
'
' COLOR accetta tutti i colori ARGB: indici palette (0-15) oppure 0xAARRGGBB.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    ' Schermo grafico 640x480 con sfondo blu (indice 1 = blu)
    SCREEN 12
    FONT "font.ttf", 16
    COLOR 15, 1
    CLS

    ' Finestra pixel: nessun contenuto, solo header con titolo.
    WINDOW DEFINE W1 FROM 40, 40 TO 440, 160 TITLE "CIAO" PIXEL

    ' COLOR con la finestra attiva: testo/titolo giallo (14), sfondo rosso (4).
    WINDOW SELECT W1
    COLOR 14, 4
    WINDOW SELECT 0

    WAITKEY
END SUB
