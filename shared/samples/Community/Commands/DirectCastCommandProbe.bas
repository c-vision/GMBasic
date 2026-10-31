' DIRECTCAST + SYSTEM.COMMAND/COMMANDCOUNT — sonda di conversione e argomenti.
' Output deterministico su 4 backend: si usa un indice fuori range (stringa vuota)
' invece degli argomenti dell'harness (diversi fra le gambe).
IMPORT SYSTEM

SUB Main()
    DIM intero AS LONG
    DIM reale AS DOUBLE
    DIM testo AS STRING

    intero = DIRECTCAST("42", LONG)
    reale = DIRECTCAST("3.5", DOUBLE)
    testo = DIRECTCAST("abc", STRING)

    IF intero = 42 THEN PRINT "long ok"
    IF ABS(reale - 3.5) < 0.0001 THEN PRINT "double ok"
    IF testo = "abc" THEN PRINT "string ok"
    IF ABS(-2.75) = 2.75 THEN PRINT "abs ok"
    IF INT("-3.9") = -4 THEN PRINT "int ok"
    IF FIX("-3.9") = -3 THEN PRINT "fix ok"
    IF FLOOR("3.9") = 3 THEN PRINT "floor ok"
    IF TRUNC("-3.9") = -3 THEN PRINT "trunc ok"
    IF CEILING("3.1") = 4 THEN PRINT "ceiling ok"

    IF LEN(COMMAND(999)) = 0 THEN PRINT "command ok"
    IF LEN(SYSTEM.COMMAND(999)) = 0 THEN PRINT "system.command ok"
    IF COMMANDCOUNT() >= 0 THEN PRINT "count ok"
    IF SYSTEM.COMMANDCOUNT() >= 0 THEN PRINT "system.count ok"

    PRINT "done"
END SUB
