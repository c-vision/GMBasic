' Store con sintassi bracket `a[i] = v` (bug C99-IndexedTarget-Bracket-Store-Scartati,
' 2026-09-15): l'emitter C# li scartava in silenzio, ora parità coi backend managed.
' EXPECT: 15|x|b|z|3
IMPORT STATS
IMPORT STRINGS
SUB Main()
    DIM n(3) AS DOUBLE
    n[0] = 10
    n[1] = 20
    n[2] = 30
    PRINT STR$(STATS.MEAN(n))
    DIM parts() AS STRING
    parts = STRINGS.SPLIT("a,b,c", ",")
    parts[0] = "x"
    parts[2] = "z"
    PRINT parts[0]
    PRINT parts[1]
    PRINT parts[2]
    PRINT STR$(UBOUND(parts) + 1)
END SUB
