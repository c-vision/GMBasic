' EXPECT: x= 7|y= 10|dentro blocco= 5|dopo k= 100
' OPTION STRICT ON con codice dichiarato: la modalità chiede che ogni NOME sia dichiarato (nessuna
' variabile implicita), NON impone una tipizzazione statica. Quindi `DIM x AS LONG : x = 7`
' (letterale DOUBLE → narrowing) resta valido, `DIM x = 7` (tipo inferito) è una dichiarazione
' valida, e una variabile di blocco può essere dichiarata e usata al suo interno.
OPTION STRICT ON

SUB Main()
    DIM x AS LONG
    x = 7
    PRINT "x="; x

    DIM y = 10
    PRINT "y="; y

    DIM k = 100
    IF TRUE THEN
        DIM k = 5
        PRINT "dentro blocco="; k
    END IF
    PRINT "dopo k="; k
END SUB
