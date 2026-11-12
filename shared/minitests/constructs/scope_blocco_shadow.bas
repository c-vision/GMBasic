' EXPECT: dim dentro= 10|dim dopo= 100|loc dentro= 20|loc dopo= 100|let dentro= 30|let dopo= 100
' SCOPE DI BLOCCO (spec Variables-Scope §2.1/§2.2/§3.1/§6): DIM, LOCAL e LET dentro un blocco
' creano una variabile DEL BLOCCO che ombreggia quella esterna; alla chiusura del blocco il nome
' torna a indicare la variabile esterna (che non è stata modificata).
' NB: un nome dichiarato nel blocco e letto FUORI non è garantito (default o valore del blocco
' a seconda della gamba): è codice errato per la spec — OPTION STRICT ON lo rende un errore di
' compilazione (constructs/scope_strict_fuori_blocco.bas).
SUB Blocchi()
    DIM A = 100
    IF TRUE THEN
        DIM A = 10
        PRINT "dim dentro="; A
    END IF
    PRINT "dim dopo="; A

    IF TRUE THEN
        LOCAL A = 20
        PRINT "loc dentro="; A
    END IF
    PRINT "loc dopo="; A

    IF TRUE THEN
        LET A = 30
        PRINT "let dentro="; A
    END IF
    PRINT "let dopo="; A
END SUB

SUB Main()
    Blocchi()
END SUB
