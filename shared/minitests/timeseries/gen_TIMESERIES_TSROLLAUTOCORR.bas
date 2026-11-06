' MiniTest TIMESERIESX.TSROLLAUTOCORR — copertura con input VALIDO (serie di 10 punti).
' Gli stub gen_* passavano 0 al posto della serie: nessun backend poteva calcolare.
' EXPECT: 8 | 0
IMPORT TIMESERIESX AS TS

SUB Main()
    DIM s(9) AS DOUBLE = {1,2,3,4,5,6,7,8,9,10}
    DIM r() = TS.TSROLLAUTOCORR(s, 3, 1)
    PRINT LEN(r()); "|"; MATH.ROUND(r(UBOUND(r)), 6)
END SUB
