' Regressione bug C99-SPLIT-Array-Dinamico-Crash (2026-09-15):
' `DIM x() AS STRING : x = STRINGS.SPLIT(...)` andava in SIGSEGV (exit 139)
' su C99/C99Native; la forma non qualificata riempiva zero elementi.
' EXPECT: n= 3|10|20|30|mean= 15
IMPORT STATS
IMPORT STRINGS
SUB Main()
    DIM parts() AS STRING
    parts = STRINGS.SPLIT("10,20,30", ",")
    PRINT "n="; UBOUND(parts) + 1
    PRINT parts[0]
    PRINT parts[1]
    PRINT parts[2]
    DIM n(3) AS DOUBLE
    n(0) = VAL(parts[0])
    n(1) = VAL(parts[1])
    n(2) = VAL(parts[2])
    PRINT "mean="; STR$(STATS.MEAN(n))
END SUB
