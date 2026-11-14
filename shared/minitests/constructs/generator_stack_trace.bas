' MiniTest — il frame del GENERATORE compare nello stack trace dell'errore che esce dal
' corpo (parità 4 backend). Il corpo gira in un contesto separato (thread su IL, task su
' PCode/VM-C, coroutine su C99Native): senza il trasporto dei frame, `e.StackTrace` mostrava
' solo il chiamante. Sulle gambe native la catena è condivisa tra corpo e chiamante, quindi
' il frame del generatore c'è "da sé"; IL/PCode/VM-C devono anteporre la catena del corpo.
' EXPECT: 1|err= 13|in G (riga 9)|in Main (riga 16)|fine
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
        PRINT e.StackTrace
    END TRY
    PRINT "fine"
END SUB
