' FILE: Arrays/ArraysUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: ARRAYS - FUNZIONI NON ANCORA COPERte DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni ARRAYS che non comparivano in alcun esempio .bas:
'   ACHUNK, APAD, AZIP, ATOKENS, AMAX, AMIN, ASUM.
'
' COPERTURA:
' - ARRAYS.ACHUNK(arr, size) : divide in blocchi.
' - ARRAYS.APAD(arr, len, pad): riempie a lunghezza.
' - ARRAYS.AZIP(arr1, arr2)  : combina due array.
' - ARRAYS.ATOKENS(s, del)   : tokenizza una stringa.
' - ARRAYS.AMAX(array) / AMIN(array) / ASUM(array): statistiche.
' =================================================================================================
IMPORT ARRAYS AS AR

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "ARRAYS - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    DIM data(7) AS DOUBLE
    FOR i = 0 TO 7
        data(i) = i + 1
    NEXT

    ' 1. AMAX / AMIN / ASUM
    PRINT "-- AMAX / AMIN / ASUM --"
    PRINT "AMAX(data) = "; AR.AMAX(data)
    PRINT "AMIN(data) = "; AR.AMIN(data)
    PRINT "ASUM(data) = "; AR.ASUM(data)
    PRINT

    ' 2. ACHUNK
    PRINT "-- ACHUNK --"
    DIM chunks = AR.ACHUNK(data, 2)
    PRINT "ACHUNK(data, 2) = "; UBOUND(chunks) + 1; " blocchi."
    PRINT

    ' 3. APAD
    PRINT "-- APAD --"
    DIM padded = AR.APAD(data, 12, 0.0)
    PRINT "APAD(data, 12, 0) = "; UBOUND(padded) + 1; " elementi."
    PRINT

    ' 4. AZIP
    PRINT "-- AZIP --"
    DIM a1(2) AS DOUBLE = {1, 2, 3}
    DIM a2(2) AS STRING = {"uno", "due", "tre"}
    DIM zipped = AR.AZIP(a1, a2)
    PRINT "AZIP(a1, a2) = "; UBOUND(zipped) + 1; " elementi."
    PRINT

    ' 5. ATOKENS
    PRINT "-- ATOKENS --"
    DIM toks = AR.ATOKENS("a,b,c,d", ",")
    PRINT "ATOKENS('a,b,c,d', ',') = "; UBOUND(toks) + 1; " token."
    PRINT

    PRINT "Test Completato."
END SUB