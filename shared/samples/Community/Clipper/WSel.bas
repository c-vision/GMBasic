IMPORT WINDOWS
SUB Main()
    WINDOW DEFINE Edit FROM 2, 2 TO 10, 40 TITLE "Editor"
    WINDOW SELECT Edit
    PRINT "dentro finestra"
    WINDOW SELECT 0
    PRINT "fuori"
END SUB
