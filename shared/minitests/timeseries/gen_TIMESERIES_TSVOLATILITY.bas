' MiniTest TIMESERIES.TSVOLATILITY — copertura con input VALIDO (serie di 10 punti).
' Gli stub gen_* passavano 0 al posto della serie: nessun backend poteva calcolare.
' EXPECT: 0.268853
IMPORT TIMESERIESX AS TS

SUB Main()
    DIM s(9) AS DOUBLE = {1,2,3,4,5,6,7,8,9,10}
    PRINT MATH.ROUND(TS.TSVOLATILITY(s), 6)
END SUB
