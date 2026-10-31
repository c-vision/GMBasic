' FILE: Windows/ClipperWindow.bas
' Test Window Management TUI — sintassi legacy

IMPORT WINDOWS

SUB Main()
    PRINT "=== Window Management Test ==="
    
    ' DEFINE finestre
    WINDOW DEFINE Main FROM 1, 1 TO 60, 20 TITLE "Finestra Principale"
    PRINT "Main defined"
    
    WINDOW DEFINE Help FROM 1, 22 TO 30, 40 TITLE "Help" DOUBLE
    PRINT "Help defined"
    
    ' ACTIVATE
    WINDOW Main ACTIVATE
    PRINT "Main activated"
    
    ' DEACTIVATE  
    WINDOW Main DEACTIVATE
    PRINT "Main deactivated"
    
    ' CLOSE
    WINDOW Help CLOSE
    PRINT "Help closed"
    
    PRINT "=== Test completato ==="
END SUB
