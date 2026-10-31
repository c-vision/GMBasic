' FILE: Arrays/ArraysPassingLocal.bas
' =================================================================================================
'                                GUIDA TEORICA: ARRAY DI STRINGHE LOCALI
' =================================================================================================
'
' Questo test verifica il passaggio di array locali di tipo STRING a sottoprogrammi.
'
' 1. ARRAY DI STRINGHE
'    Gli array di stringhe in GM-BASIC sono array di oggetti (poiché le stringhe sono tipi riferimento).
'    Possono essere passati BYREF esattamente come gli array numerici.
'
' 2. SINTASSI MODERNA DI PASSAGGIO
'    In questo esempio utilizziamo la sintassi moderna di passaggio per nome: `CreaStringheDiTest(Stringhe)`.
'    A differenza della sintassi `Stringhe()`, questa non è ambigua per il parser ed è preferibile
'    per il nuovo codice.
'
' =================================================================================================

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ArraysPassingLocal - Verifica Regressione Passaggio Array Locale            ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"

    ' Dichiarazione di un array locale di stringhe con 1 elemento (indice 0).
    DIM Stringhe(0) AS STRING
    
    ' Passaggio dell'array alla SUB.
    ' Qui usiamo la sintassi moderna (solo nome), che è pulita e non ambigua.
    ' L'array viene passato per riferimento, quindi la SUB può ridimensionarlo o modificarlo.
    CreaStringheDiTest(Stringhe)

    PRINT "Array dopo la chiamata (dovrebbe essere ridimensionato e popolato):"
    FOR i = 0 TO UBOUND(Stringhe)
        PRINT "  Stringhe("; i; ") = "; Stringhe(i)
    NEXT i
    
    PRINT "Test completato."
END SUB

' Questa SUB accetta un array di stringhe.
' Il parametro Dati() non specifica la dimensione, accettando array di qualsiasi lunghezza.
SUB CreaStringheDiTest(Dati() AS STRING)
    PRINT "  -> Dentro CreaStringheDiTest. Ridimensiono e popolo..."
    
    ' Ridimensiona l'array originale (passato per riferimento).
    REDIM Dati(2)
    
    Dati(0) = "Prima Stringa"
    Dati(1) = "Seconda Stringa"
    Dati(2) = "Terza Stringa"
END SUB