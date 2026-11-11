' EXPECT: g= 1|g= 2|x= 6|x= 7|x= 8|a= 11  b=ciao!|a= 12  b=ciao!!|altro x= 101|altro x= 102|n= 0  chiamate= 3|n= 1  chiamate= 3|n= 2  chiamate= 3|solo= 1|no|solo= 2
' STATIC dentro una procedura (2026-11-11): la variabile conserva il valore fra una chiamata
' e l'altra e l'inizializzatore gira UNA volta sola — come una variabile `static` del C.
' Prima il backend IL era l'unico corretto: PCode/VM-C/C99Native la trattavano come un locale
' e ripartivano da zero a ogni chiamata (divergenza scoperta dalla verifica BYREF).
' Copre: inizializzatore esplicito, due STATIC nella stessa procedura (numerica + STRING che
' cresce), omonimo in un'ALTRA procedura (storage indipendente), ricorsione (il contatore è
' condiviso fra i livelli), STATIC dentro un blocco IF (inizializzata solo quando il ramo
' gira), e STATIC a livello di modulo (equivalente a un GLOBALE).
SUB Init()
    STATIC x AS LONG = 5
    x = x + 1
    PRINT "x="; x
END SUB

SUB Due()
    STATIC a AS LONG = 10
    STATIC b AS STRING = "ciao"
    a = a + 1
    b = b + "!"
    PRINT "a="; a; " b="; b
END SUB

SUB Altro()
    STATIC x AS LONG = 100
    x = x + 1
    PRINT "altro x="; x
END SUB

SUB Ricorsione(n AS LONG)
    STATIC chiamate AS LONG
    chiamate = chiamate + 1
    IF n > 0 THEN Ricorsione(n - 1)
    PRINT "n="; n; " chiamate="; chiamate
END SUB

SUB NelBlocco(flag AS BIT)
    IF flag THEN
        STATIC solo AS LONG
        solo = solo + 1
        PRINT "solo="; solo
    ELSE
        PRINT "no"
    END IF
END SUB

SUB Main()
    Init() : Init() : Init()
    Due() : Due()
    Altro() : Altro()
    Ricorsione(2)
    NelBlocco(TRUE) : NelBlocco(FALSE) : NelBlocco(TRUE)
END SUB

' STATIC a livello di modulo: è un globale (nessuna procedura in cui persistere).
STATIC g AS LONG
g = g + 1
PRINT "g="; g
g = g + 1
PRINT "g="; g
