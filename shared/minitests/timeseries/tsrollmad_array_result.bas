' TSROLLMAD / TSROLLAUTOCORR con risultato array su DIM rm() = ...:
' - VM/C: il builtin liberava due volte il buffer del risultato (abort "pointer being freed was not allocated")
'   e poi azzerava comunque il risultato; ora il risultato e' una lista valida.
' - MAD mobile = MEDIANA delle deviazioni assolute dalla mediana (Numerics.RollingMad), non la loro media.
' EXPECT: n=8|mad=1,1,1,0,1,1,1,0|ac=8
IMPORT TIMESERIESX AS TS

SUB Main()
    DIM s(9) AS DOUBLE
    s(0)=1.0 : s(1)=2.0 : s(2)=3.0 : s(3)=4.0 : s(4)=5.0
    s(5)=4.0 : s(6)=3.0 : s(7)=2.0 : s(8)=1.0 : s(9)=2.0

    DIM rm() = TS.TSROLLMAD(s, 3)
    DIM i AS INTEGER
    DIM t AS STRING = ""
    FOR i = 0 TO UBOUND(rm)
        IF i > 0 THEN t = t + ","
        t = t + TRIM(STR(rm(i)))
    NEXT i
    PRINT "n="; TRIM(STR(LEN(rm())))
    PRINT "mad="; t

    DIM rac() = TS.TSROLLAUTOCORR(s, 3, 1)
    PRINT "ac="; TRIM(STR(LEN(rac())))
END SUB
