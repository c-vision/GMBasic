' FILE: Types/TypesNullable.bas
' =================================================================================================
'       TEST: Nullable Types (INTEGER?, STRING?, DOUBLE?, ecc.)
' =================================================================================================
'
' Sintassi supportate:
'   - Dichiarazione:  DIM x AS INTEGER?
'   - Assegnazione:   x = 42 / x = NIL
'   - Controllo:      IF x HAS VALUE THEN ...
'   - Accesso:        DIM v AS INTEGER = x.VALUE
'   - IS NIL:         IF x IS NIL THEN ...
'
' =================================================================================================

IMPORT SYSTEM

10 CLS
20 PRINT "=== Nullable Types Test ==="
30 PRINT

GOSUB TestIntegerNullable
GOSUB TestStringNullable
GOSUB TestNullableValueAccess
GOSUB TestNullableCheck

90 PRINT
95 PRINT "=== Tutti i test nullable completati ==="
100 END


' -------------------------------------------------------------------------
' 1. INTEGER? — assegnazione valore e NIL
' -------------------------------------------------------------------------
1000 TestIntegerNullable:
1010 PRINT "--- 1. INTEGER? ---"
1020 DIM n AS INTEGER?
1030 IF n IS NIL THEN PRINT "  - n inizialmente NIL (OK)"

1040 n = 42
1050 IF n HAS VALUE THEN PRINT "  - n ha valore (OK):"; n
1060 IF n <> 42 THEN ERROR 9999, "n dovrebbe essere 42"

1070 n = NIL
1080 IF n IS NIL THEN PRINT "  - n di nuovo NIL dopo assegnazione (OK)"
1090 IF NOT (n HAS VALUE) THEN PRINT "  - n NON ha valore (OK)"
1100 RETURN


' -------------------------------------------------------------------------
' 2. STRING? — nullable su tipo stringa
' -------------------------------------------------------------------------
2000 TestStringNullable:
2010 PRINT "--- 2. STRING? ---"
2020 DIM s AS STRING?
2030 IF s IS NIL THEN PRINT "  - s inizialmente NIL (OK)"

2040 s = "Hello Nullable"
2050 IF s HAS VALUE THEN PRINT "  - s ha valore (OK):"; s

2060 s = NIL
2070 IF s IS NIL THEN PRINT "  - s di nuovo NIL (OK)"
2080 RETURN


' -------------------------------------------------------------------------
' 3. .VALUE — accesso al valore sottostante
' -------------------------------------------------------------------------
3000 TestNullableValueAccess:
3010 PRINT "--- 3. .VALUE access ---"
3020 DIM d AS DOUBLE? = 3.14
3030 IF d HAS VALUE THEN
3040     PRINT "  - d.VALUE ="; d.VALUE
3050     IF ABS(d.VALUE - 3.14) > 0.001 THEN ERROR 9999, "d.VALUE dovrebbe essere 3.14"
3060 END IF

3070 DIM empty AS INTEGER?
3080 IF NOT (empty HAS VALUE) THEN PRINT "  - empty non ha valore (OK)"
3090 RETURN


' -------------------------------------------------------------------------
' 4. HAS VALUE in condizioni composte
' -------------------------------------------------------------------------
4000 TestNullableCheck:
4010 PRINT "--- 4. HAS VALUE in condizioni ---"
4020 DIM x AS INTEGER? = 100
4030 DIM y AS INTEGER?

4040 IF x HAS VALUE AND x.VALUE = 100 THEN PRINT "  - x HAS VALUE e x.VALUE=100 (OK)"
4050 IF NOT (y HAS VALUE) THEN PRINT "  - y non ha valore (OK)"

4060 y = x.VALUE + 50
4070 IF y HAS VALUE AND y.VALUE = 150 THEN PRINT "  - y = x.VALUE + 50 = 150 (OK)"
4080 RETURN
