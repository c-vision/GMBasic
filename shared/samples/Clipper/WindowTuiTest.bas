' FILE: Clipper/WindowTuiTest.bas
' Test backend IL: gestione finestre TUI polimorfica (W0+W1+viewport).
' Copre: DEFINE con TITLE/DOUBLE/SHADOW, tutte le azioni (ACTIVATE/DEACTIVATE/
' CLOSE/MINIMIZE/RESTORE/RELEASE), WINDOW SET TITLE, WINDOW SELECT (finestra attiva).
' Ogni step stampa un marker verificabile (i marker vanno sullo schermo base: si torna
' con WINDOW SELECT 0 dopo le operazioni che attivano una finestra).

IMPORT WINDOWS

SUB Main()
    PRINT "=== WindowTuiTest ==="

    ' DEFINE base
    WINDOW DEFINE Main FROM 1, 1 TO 20, 40 TITLE "Principale"
    PRINT "T1_OK"

    ' DEFINE con DOUBLE + SHADOW
    WINDOW DEFINE Help FROM 3, 42 TO 18, 78 TITLE "Aiuto" DOUBLE
    PRINT "T2_OK"

    ' DEFINE senza TITLE (usa il nome)
    WINDOW DEFINE BarState FROM 21, 1 TO 23, 78
    PRINT "T3_OK"

    ' WINDOW SET TITLE sulla finestra attiva (l'ultima definita è in focus)
    WINDOW SET TITLE "Barra di Stato"
    WINDOW SELECT 0
    PRINT "T4_OK"

    ' ATTIVAZIONI (ATTIVATE attiva anche la viewport della finestra)
    WINDOW Main ACTIVATE
    WINDOW SELECT 0
    PRINT "T5_OK"
    WINDOW Help ACTIVATE
    WINDOW SELECT 0
    PRINT "T6_OK"

    ' DEACTIVATE
    WINDOW Main DEACTIVATE
    PRINT "T7_OK"

    ' MINIMIZE / RESTORE
    WINDOW Help MINIMIZE
    PRINT "T8_OK"
    WINDOW Help RESTORE
    PRINT "T9_OK"

    ' CLOSE
    WINDOW BarState CLOSE
    PRINT "T10_OK"

    ' RELEASE
    WINDOW Help RELEASE
    PRINT "T11_OK"

    PRINT "=== Test completato ==="
END SUB
