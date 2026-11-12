' EXPECT: param dentro= 5|param dopo= 9|dentro k= 10|dentro k= 20|dopo k= 1|dentro g= 1|dopo g= 7
' LET e SCOPE DI BLOCCO (spec Variables-Scope §3.1/§6): `LET x = v` crea la variabile nello
' scope CORRENTE, quindi dentro un blocco (IF, FOR, ...) una `LET` su un nome che vive fuori
' crea una variabile di BLOCCO che lo ombreggia — alla chiusura del blocco il nome torna a
' indicare quella esterna e l'esterno non è stato modificato.
' Copre: LET su un PARAMETRO dentro un blocco, LET su una locale di procedura dentro un corpo
' FOR (il valore del blocco resta nel blocco: 10, 20, e fuori torna 1), LET su un GLOBAL dentro
' un blocco. L'assegnazione implicita (senza LET) resta quella che cerca verso l'alto.
GLOBAL g AS LONG
g = 7

SUB ParamBlocco(x AS LONG)
    IF TRUE THEN
        LET x = 5
        PRINT "param dentro="; x
    END IF
    PRINT "param dopo="; x
END SUB

SUB LocaleInLoop()
    LET k = 1
    DIM i AS LONG
    FOR i = 1 TO 2
        LET k = k + 10
        PRINT "dentro k="; k
    NEXT i
    PRINT "dopo k="; k
END SUB

SUB GlobaleInBlocco()
    IF TRUE THEN
        LET g = 1
        PRINT "dentro g="; g
    END IF
    PRINT "dopo g="; g
END SUB

SUB Main()
    ParamBlocco(9)
    LocaleInLoop()
    GlobaleInBlocco()
END SUB
