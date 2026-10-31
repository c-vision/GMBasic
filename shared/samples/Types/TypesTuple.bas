' FILE: Types/TypesTuple.bas
' =================================================================================================
'       GUIDA TEORICA: TUPLE E DISTRUZIONE (DESTRUCTURING)
' =================================================================================================
' GM-BASIC supporta le tuple, che permettono di raggruppare valori correlati in una singola
' struttura senza definire un tipo esplicito.
'
' 1. SINTASSI
'    - `(val1, val2, ..., valN)`: Crea una tuple con gli valori specificati.
'    - `FUNCTION ... AS (T1, T2)`: Ritorna una tuple da una funzione.
'
' 2. UTILIZZO
'    - Scambio senza variabile temporanea: `(a, b) = (b, a)`
'    - Ritorno multiplo da funzione
'    - Raggruppo di valori correlati
'
' =================================================================================================

IMPORT CONSOLE

' Funzione che ritorna una tuple (min, max)
FUNCTION GetMinMax(arr() AS INTEGER) AS (INTEGER, INTEGER)
    DIM minVal = arr(0), maxVal = arr(0)
    FOR i = 1 TO UBOUND(arr)
        IF arr(i) < minVal THEN minVal = arr(i)
        IF arr(i) > maxVal THEN maxVal = arr(i)
    NEXT
    RETURN (minVal, maxVal)
END FUNCTION

' Funzione che ritorna una tuple (sum, avg)
FUNCTION GetSumAvg(arr() AS INTEGER) AS (LONG, DOUBLE)
    DIM s AS LONG = 0
    FOR i = 0 TO UBOUND(arr)
        s = s + arr(i)
    NEXT
    RETURN (s, s / (UBOUND(arr) + 1))
END FUNCTION

SUB Main()

    CLS
    PRINT "=========================================="
    PRINT "  TEST: Tuple e Destrutturazione"
    PRINT "=========================================="
    PRINT

    ' --- 1. Creazione di tuple ---
    PRINT "--- 1. Creazione tuple ---"
    DIM point3d = (10, 20, 30)
    DIM person = ("Alice", 30, "Engineer")
    PRINT "point3d = (10, 20, 30)"
    PRINT "person = (""Alice"", 30, ""Engineer"")"
    PRINT

    ' --- 2. Accesso ai campi della tuple ---
    PRINT "--- 2. Accesso campi ---"
    PRINT $"point3d[0]={point3d(0)}, point3d[1]={point3d(1)}, point3d[2]={point3d(2)}"
    PRINT $"person[0]={person(0)}, person[1]={person(1)}, person[2]={person(2)}"
    PRINT

    ' --- 3. Ritorno multiplo da funzione ---
    PRINT "--- 3. Ritorno multiplo ---"
    DIM data(5) AS INTEGER
    data(0) = 42 : data(1) = 17 : data(2) = 99 : data(3) = 3 : data(4) = 56 : data(5) = 28

    DIM result1 = GetMinMax(data())
    PRINT $"GetMinMax: min={result1(0)}, max={result1(1)}"

    DIM result2 = GetSumAvg(data())
    PRINT $"GetSumAvg: sum={result2(0)}, avg={result2(1)}"
    PRINT

    ' --- 4. Scambio senza temporanea ---
    PRINT "--- 4. Swap con tuple ---"
    DIM a = 100, b = 200
    PRINT $"Before: a={a}, b={b}"
    (a, b) = (b, a)
    PRINT $"After:  a={a}, b={b}"
    PRINT

    ' --- 5. Tuple annidate ---
    PRINT "--- 5. Tuple annidate ---"
    DIM nested = ((1, 2), (3, 4))
    PRINT $"nested[0]={nested(0)}, nested[1]={nested(1)}"
    PRINT

    PRINT "=========================================="
    PRINT "  TEST COMPLETATO"
    PRINT "=========================================="

END SUB
