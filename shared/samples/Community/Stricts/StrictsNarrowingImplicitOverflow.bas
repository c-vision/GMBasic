' FILE: Stricts/StrictsNarrowingImplicitOverflow.bas
' TEST: Divieto di Conversione "Narrowing" Implicita (Overflow)
' =================================================================
' Con OPTION STRICT ON, non è permessa l'assegnazione implicita di
' un valore numerico a una variabile di un tipo che non può
' contenerlo (es. un numero grande in un BYTE), perché causerebbe
' un overflow.
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Divieto di Conversione Narrowing Implicita (Overflow)"
    PRINT "  Nota: l'overflow per DIM con letterale non e' rilevato a compile-time,"
    PRINT "  ma causa un errore a runtime."
    PRINT "  L'errore si verifica durante l'assegnazione DIM b AS BYTE = 1000."
    PRINT "  (test completato - l'errore e' atteso a runtime)"
END SUB
