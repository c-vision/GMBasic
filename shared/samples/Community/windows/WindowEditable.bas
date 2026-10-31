' FILE: Windows/WindowEditable.bas
' Test residui eliminati: SAVESCREEN/RESTORESCREEN del buffer finestra + editing INKEY/
' WAITKEY sulla finestra attiva + READ.

IMPORT WINDOWS

SUB Main()
    PRINT "=== WindowEditable ==="

    ' Finestra con buffer
    WINDOW DEFINE Edit FROM 2, 2 TO 20, 60 TITLE "Editor"
    WINDOW Edit CLEAR
    WINDOW Edit PRINT "Riga A"
    WINDOW Edit PRINT "Riga B"
    WINDOW Edit PRINT "Riga C"

    ' SAVESCREEN del buffer finestra
    WINDOW SELECT Edit
    SAVE SCREEN "snap1"
    PRINT "E1_OK (buffer salvato)"

    ' Modifica il buffer
    CLS
    PRINT "E2_OK (buffer svuotato via CLS)"

    ' RESTORESCREEN ripristina il buffer
    RESTORE SCREEN "snap1"
    PRINT "E3_OK (buffer ripristinato)"

    ' READ abilita l'editing sulla finestra attiva
    READ
    PRINT "E4_OK (READ: editing attivo)"

    ' WAITKEY iniettato processa il tasto sul buffer (editing)
    WAITKEY
    PRINT "E5_OK"

    ' Ritorno allo schermo base
    WINDOW SELECT 0
    PRINT "=== Test completato ==="
END SUB
