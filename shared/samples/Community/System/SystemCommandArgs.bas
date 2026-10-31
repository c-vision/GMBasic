' FILE: Samples/System/SystemCommandArgs.bas
' Demo: COMMAND$ e COMMANDCOUNT

SUB Main()
    DIM i AS INTEGER
    PRINT "=== COMMAND$ / COMMANDCOUNT ==="
    PRINT "Numero argomenti: "; COMMANDCOUNT()
    
    FOR i = 1 TO COMMANDCOUNT()
        PRINT "Arg"; i; " = "; COMMAND$(i)
    NEXT i
    
    IF COMMANDCOUNT() = 0 THEN
        PRINT "Nessun argomento. Prova: gmbasic SystemCommandArgs.bas uno due tre"
    END IF
END SUB
