' ============================================
' TypesNamedTuple.bas — Named Tuple Fields
' ============================================
' Campi nominati opzionali nelle tuple:
'   DIM t = (x: 10, y: 20)       ← tuple nominato
'   DIM t = (10, 20)             ← tuple anonimo (backward compat)
'   DIM t = (10, y: 20)          ← mix
'
' Accesso per nome: t.x risolve a t(0) via Linker
' Accesso per indice: t(0) resta valido
' Destrutturazione: DIM (a, b) = t
' ============================================
IMPORT CONSOLE

SUB Main()
    PRINT "=== Named Tuple Fields ==="
    PRINT

    ' --- 1. Tuple interamente nominati ---
    PRINT "--- 1. Tuple nominati ---"
    DIM point = (x: 100, y: 200, z: 300)
    PRINT "point = ("; point.x; ", "; point.y; ", "; point.z; ")"
    PRINT "Indice: point(0)="; point(0); " point(1)="; point(1); " point(2)="; point(2)
    PRINT

    ' --- 2. Tuple anonimi (backward compat) ---
    PRINT "--- 2. Tuple anonimi ---"
    DIM coords = (10, 20, 30)
    PRINT "coords(0)="; coords(0); " coords(1)="; coords(1); " coords(2)="; coords(2)
    PRINT

    ' --- 3. Tuple misti ---
    PRINT "--- 3. Tuple misti ---"
    DIM mixed = (10, y: 20, 30)
    PRINT "mixed(0)="; mixed(0); " mixed.y="; mixed.y; " mixed(2)="; mixed(2)
    PRINT

    ' --- 4. Destrutturazione ---
    PRINT "--- 4. Destrutturazione ---"
    DIM t = (x: 10, y: 20, z: 30)
    DIM (a, b, c) = t
    PRINT "a="; a; " b="; b; " c="; c
    PRINT

    ' --- 5. Swap con tuple nominate ---
    PRINT "--- 5. Swap ---"
    DIM p = (x: 1, y: 2)
    DIM (px, py) = p
    PRINT "prima: px="; px; " py="; py
    (px, py) = (py, px)
    PRINT "dopo:  px="; px; " py="; py
    PRINT

    ' --- 6. Ritorno multiplo con nomi ---
    PRINT "--- 6. Ritorno multiplo ---"
    DIM result = GetBounds()
    PRINT "per nome:   lo="; result.lo; " hi="; result.hi
    PRINT "per indice: lo="; result(0); " hi="; result(1)
    PRINT

    ' --- 7. Elementi stringa ---
    PRINT "--- 7. Elementi stringa ---"
    DIM person = (name: "Alice", age: 30)
    PRINT "person.name="; person.name; " person.age="; person.age
    PRINT "person(0)="; person(0); " person(1)="; person(1)
    DIM (pn, pa) = person
    PRINT "destr: name="; pn; " age="; pa
    PRINT

    ' --- 8. Tuple annidate ---
    PRINT "--- 8. Tuple annidate ---"
    DIM nested = ((1, 2), (3, 4))
    PRINT $"nested(0)={nested(0)} nested(1)={nested(1)}"
    DIM pair = (10, 20)
    PRINT $"pair={pair}"
    PRINT

    PRINT "=== TEST COMPLETATO ==="
END SUB

FUNCTION GetBounds() AS (lo AS DOUBLE, hi AS DOUBLE)
    RETURN (lo: 3, hi: 99)
END FUNCTION
