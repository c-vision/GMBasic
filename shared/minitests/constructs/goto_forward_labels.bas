' GOTO / GOSUB verso etichette in AVANTI: gli opcode a indirizzo ASSOLUTO non devono ricevere un offset relativo (regressione del patch
' immediato delle etichette: ogni GOTO in avanti ripartiva dall'inizio su PCode e VM/C → loop infinito).
' EXPECT: a|b|sub-ok|c|fine
SUB Main()
    PRINT "a"
    GOTO Salto
    PRINT "MAI"
Salto:
    PRINT "b"
    GOSUB Rout
    PRINT "c"
    GOTO Fine
Rout:
    PRINT "sub-ok"
    RETURN
Fine:
    PRINT "fine"
END SUB
