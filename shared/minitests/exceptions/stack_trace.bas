' MiniTest EXCEPTIONS — ERR.StackTrace: catena delle procedure dell'errore.
'
' Copre:
'   1. errore in profondità (Main → Media → Interna): la catena riporta TUTTE le
'      procedure, dalla più interna alla più esterna, con la riga corrente di ciascuna;
'   2. `e.StackTrace` (variabile del CATCH) e `ERR.StackTrace` (pseudo-oggetto) danno
'      lo stesso testo;
'   3. dopo un errore catturato NON restano frame fantasma: un secondo errore sollevato
'      in Main riporta solo Main (prima del fix la catena teneva i frame morti).
'
' EXPECT: num= 11|catch:|in Interna (riga 13)|in Media (riga 17)|in Main (riga 22)|ERR:|in Interna (riga 13)|in Media (riga 17)|in Main (riga 22)|secondo:|in Main (riga 32)|fine
SUB Interna()
    ERROR 11, "boom"
END SUB

SUB Media()
    Interna()
END SUB

SUB Main()
    TRY
        Media()
    CATCH e AS ERR
        PRINT "num="; e.Number
        PRINT "catch:"
        PRINT e.StackTrace
        PRINT "ERR:"
        PRINT ERR.StackTrace
    END TRY
    PRINT "secondo:"
    TRY
        ERROR 12, "secondo"
    CATCH e AS ERR
        PRINT e.StackTrace
    END TRY
    PRINT "fine"
END SUB
