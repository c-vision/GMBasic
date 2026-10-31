' MiniTest TIMESERIES.TSAUTOCORR — copertura con input VALIDO (serie di 10 punti).
' Gli stub gen_* passavano 0 al posto della serie: nessun backend poteva calcolare.
' EXPECT: 0.7
IMPORT TIMESERIES AS TS
IMPORT MATH

SUB Main()
    DIM s(9) AS DOUBLE = {1,2,3,4,5,6,7,8,9,10}
    PRINT MATH.ROUND(TS.TSAUTOCORR(s, 1), 6)
END SUB
