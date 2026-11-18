' MiniTest — ERR.SourceLineText / e.SourceLineText: TESTO della riga che ha sollevato
' l'errore (parità 4 gambe). Il testo viaggia DENTRO il binario/chunk (C99: letterale
' incorporato; IL: blob registrato nell'assembly; PCode/VM-C: coda del chunk), quindi
' funziona anche su un eseguibile senza sorgenti accanto.
'
' Canone: testo della riga TRIMMATO (niente indentazione), "" se la riga non è disponibile.
' EXPECT: n 1 = 11|t 1 =[IF n = 1 THEN ERROR 11, "primo"]|n 2 = 13|t 2 =[ERROR 13, "secondo"]|stack=[  in Interna (riga 10)|in Main (riga 17)|]|fine
SUB Interna(n AS LONG)
    IF n = 1 THEN ERROR 11, "primo"
    ERROR 13, "secondo"
END SUB

SUB Main()
    DIM k AS LONG
    FOR k = 1 TO 2
        TRY
            Interna(k)
        CATCH e AS ERR
            PRINT "n"; k; "="; e.Number
            PRINT "t"; k; "=["; e.SourceLineText; "]"
        END TRY
    NEXT k
    PRINT "stack=["; ERR.StackTrace; "]"
    PRINT "fine"
END SUB
