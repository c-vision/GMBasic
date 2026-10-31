' ============================================================================
' BracketStoreDemo.bas — store e letture con sintassi bracket `a[i]`
' Categoria: Community/Arrays
' Gli store bracket (AST IndexedTarget) erano scartati in silenzio dal backend
' C99 (bug C99-IndexedTarget-Bracket-Store-Scartati, 2026-09-15): ora parità 5/5.
' Combina store bracket su fisso + dinamico da SPLIT + MEAN + UBOUND + JOIN.
' ============================================================================
IMPORT STATS
IMPORT STRINGS

SUB Main()
    ' Array fisso di DOUBLE con store bracket
    DIM n(3) AS DOUBLE
    n[0] = 10
    n[1] = 20
    n[2] = 30
    PRINT "mean="; STR$(STATS.MEAN(n))
    PRINT "ubound="; STR$(UBOUND(n) + 1)

    ' Array dinamico da SPLIT con store bracket
    DIM parts() AS STRING
    parts = STRINGS.SPLIT("10,20,30", ",")
    parts[0] = "diec"
    PRINT "p0="; parts[0]
    PRINT "p2="; parts[2]
    PRINT "join="; JOIN(parts, "-")

    ' Riempimento da dinamico a fisso con bracket su entrambi i lati
    DIM m(3) AS DOUBLE
    m[0] = VAL(parts[1])
    m[1] = VAL(parts[2])
    m[2] = 5
    PRINT "sum="; STR$(STATS.MEAN(m) * 3)
END SUB
