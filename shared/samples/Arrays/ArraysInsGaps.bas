' FILE: Arrays/ArraysInsGaps.bas
' Copre ARRAYS.AINS(array, position): inserisce uno slot vuoto alla posizione indicata
' (indici 0-based). Forma comando: AINS <array>, <posizione>.
'
' ATTESO: dopo AINS a, 1  ->  a(0)=10  a(1)=0  a(2)=30
IMPORT ARRAYS

SUB Main()
    DIM a() = {10, 30}
    AINS a, 1
    PRINT "a(0)="; a(0); " a(1)="; a(1); " a(2)="; a(2)
    PRINT "ArraysInsGaps: completato."
END SUB
