' EXPECT: then A= 10|dopo A= 100|else A= 20|dopo A= 100|B= 200
' THEN e ELSE hanno scope INDIPENDENTI (spec Variables-Scope §6.2): una variabile dichiarata nel
' ramo (LOCAL/LET/DIM) vive solo in quel ramo e non tocca quella esterna; alla fine dell'IF il nome
' torna a indicare la variabile esterna. L'assegnazione IMPLICITA (senza LET) invece cerca risalendo
' e MODIFICA quella esterna.
SUB Ramo(flag)
    DIM A AS LONG
    A = 100
    IF flag THEN
        LOCAL A = 10
        PRINT "then A="; A
    ELSE
        LOCAL A = 20
        PRINT "else A="; A
    END IF
    PRINT "dopo A="; A
END SUB

SUB ImplicitaNelRamo()
    DIM B AS LONG
    B = 100
    IF TRUE THEN
        B = 200                 ' implicita: modifica la B esterna
    END IF
    PRINT "B="; B
END SUB

SUB Main()
    Ramo(TRUE)
    Ramo(FALSE)
    ImplicitaNelRamo()
END SUB
