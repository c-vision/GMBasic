' EXPECT: COMPILE ERROR: In OPTION STRICT mode, la variabile 'VarBlocco' deve essere dichiarata esplicitamente.
' MiniTest NEGATIVO di compilazione: con OPTION STRICT ON un nome dichiarato dentro un blocco
' (qui un IF) NON è più in scope dopo la chiusura del blocco — leggerlo è un errore, con lo
' stesso messaggio sulle 4 gambe. Fuori da STRICT lo stesso programma compila e stampa il valore
' implicito (0): vedi constructs/scope_blocco_shadow.bas.
OPTION STRICT ON

SUB F()
    IF TRUE THEN
        DIM VarBlocco AS LONG
        VarBlocco = 7
        PRINT "dentro="; VarBlocco
    END IF
    PRINT "dopo="; VarBlocco
END SUB

SUB Main()
    F()
END SUB
