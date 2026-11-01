' FILE: samples/Collections/DictLiteralInit.bas
' ======================================================================================================================
' Dizionari con letterale `{ "chiave": valore, ... }`
' ======================================================================================================================
' Copre:
'   - dict TIPIZZATO:  DIM d AS DICT OF STRING, INTEGER = { "uno": 1, "due": 2 }
'   - dict INFERITO:   DIM d = { "a": 10, "b": 20 }        (tipo dedotto dal letterale)
'   - valori STRING:   DIM s AS DICT OF STRING, STRING = { "x": "hello" }
'   - lettura / scrittura per chiave `d(chiave)`, assegnazione `d(chiave) = valore`
' Verificato in parita' sui 4 backend (IL, P-Code, VM/C, C99Native).
SUB Main()
    PRINT "=== DictLiteralInit: { chiave: valore } ==="

    DIM typed AS DICT OF STRING, INTEGER = {"uno": 1, "due": 2}
    PRINT "typed uno="; typed("uno"); " due="; typed("due")

    DIM inferred = {"a": 10, "b": 20}
    PRINT "inferred a="; inferred("a"); " b="; inferred("b")

    DIM s AS DICT OF STRING, STRING = {"x": "hello", "y": "world"}
    PRINT "s x="; s("x"); " y="; s("y")

    ' Scrittura per chiave (nuova chiave + aggiornamento).
    inferred("c") = 30
    inferred("a") = 11
    PRINT "inferred a="; inferred("a"); " c="; inferred("c")

    PRINT "DictLiteralInit completato."
END SUB
