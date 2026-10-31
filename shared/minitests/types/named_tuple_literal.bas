' EXPECT: 10|20|30|10|20|30|10|20|30
' MiniTest: named tuple literal fields — (nome: valore, ...)
' Verifica che i campi nominati funzionino in:
'   1. Tuple interamente nominati
'   2. Tuple interamente anonimi (backward compat)
'   3. Tuple misti (posizionali + nominati)
IMPORT CONSOLE
SUB Main()
    ' --- 1. Tuple interamente nominati ---
    DIM t1 = (x: 10, y: 20, z: 30)
    PRINT t1(0)
    PRINT t1(1)
    PRINT t1(2)

    ' --- 2. Tuple anonimi (backward compat) ---
    DIM t2 = (10, 20, 30)
    PRINT t2(0)
    PRINT t2(1)
    PRINT t2(2)

    ' --- 3. Tuple misti ---
    DIM t3 = (10, y: 20, 30)
    PRINT t3(0)
    PRINT t3(1)
    PRINT t3(2)
END SUB
