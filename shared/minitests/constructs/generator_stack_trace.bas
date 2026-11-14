' MiniTest — errore che esce dal corpo di un GENERATOR: frame, RIGA e TESTO della riga.
' Il corpo gira in un contesto separato (thread su IL, task su PCode/VM-C, coroutine su
' C99Native): senza trasporto esplicito, `e.StackTrace` mostrava solo il chiamante e
' `e.LineNumber`/`e.SourceLineText` riportavano la riga della NEXT (dove sta il chiamante)
' invece di quella del corpo che ha fallito. Sulle gambe native la catena è condivisa,
' quindi il frame del generatore c'è "da sé"; IL/PCode/VM-C devono anteporre catena+riga.
' EXPECT: 1|err= 13|line= 10|text=[ERROR 13, "corpo"]|in G (riga 10)|in Main (riga 17)|fine
GENERATOR G() AS LONG
    YIELD 1
    ERROR 13, "corpo"
END GENERATOR

SUB Main()
    DIM it = G()
    TRY
        PRINT NEXT(it)
        PRINT NEXT(it)
    CATCH e AS ERR
        PRINT "err="; e.Number
        PRINT "line="; e.LineNumber
        PRINT "text=["; e.SourceLineText; "]"
        PRINT e.StackTrace
    END TRY
    PRINT "fine"
END SUB
