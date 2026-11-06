' MiniTest: un PARAMETRO il cui nome coincide con una keyword-funzione a 0 argomenti (`message`, builtin
' MESSAGE()) e' una variabile, non una chiamata. Prima (bug 2026-11-06, regressione della risoluzione
' "keyword a 0 argomenti senza parentesi") il parametro valeva "" su IL/PCode/VM-C: SUB, FUNCTION e
' ASYNC SUB (es. LogAsync(message) di ConcurrencyAsync). C99Native ha un parser proprio e non era affetto.
' EXPECT: S:a|F:b|A:c|L:d|Z:z
ASYNC SUB LogAsync(message AS STRING)
    AWAIT TaskYield()
    PRINT "L:" + message
END SUB
SUB S(message AS STRING)
    PRINT "S:" + message
END SUB
FUNCTION F(message AS STRING) AS STRING
    RETURN "F:" + message
END FUNCTION
ASYNC FUNCTION A(message AS STRING) AS STRING
    AWAIT TaskYield()
    RETURN "A:" + message
END FUNCTION
SUB Main()
    S("a")
    PRINT F("b")
    DIM r = AWAIT A("c")
    PRINT r
    LogAsync("d")
    AWAIT TaskYield()
    PRINT "Z:z"
END SUB
