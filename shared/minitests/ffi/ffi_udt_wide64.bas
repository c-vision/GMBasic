' UDT con 64 campi (cap dichiarato, F5 2026-11-07): il test più largo era 16 (Wide16).
' 64 LONG = 256 byte; somma 1..64 = 2080; inc porta F0 a 2 e F63 a 64.
' EXPECT: sz= 256|sum= 2080|inc0= 2|inc63= 65
TYPE Wide64
    F0 AS LONG
    F1 AS LONG
    F2 AS LONG
    F3 AS LONG
    F4 AS LONG
    F5 AS LONG
    F6 AS LONG
    F7 AS LONG
    F8 AS LONG
    F9 AS LONG
    F10 AS LONG
    F11 AS LONG
    F12 AS LONG
    F13 AS LONG
    F14 AS LONG
    F15 AS LONG
    F16 AS LONG
    F17 AS LONG
    F18 AS LONG
    F19 AS LONG
    F20 AS LONG
    F21 AS LONG
    F22 AS LONG
    F23 AS LONG
    F24 AS LONG
    F25 AS LONG
    F26 AS LONG
    F27 AS LONG
    F28 AS LONG
    F29 AS LONG
    F30 AS LONG
    F31 AS LONG
    F32 AS LONG
    F33 AS LONG
    F34 AS LONG
    F35 AS LONG
    F36 AS LONG
    F37 AS LONG
    F38 AS LONG
    F39 AS LONG
    F40 AS LONG
    F41 AS LONG
    F42 AS LONG
    F43 AS LONG
    F44 AS LONG
    F45 AS LONG
    F46 AS LONG
    F47 AS LONG
    F48 AS LONG
    F49 AS LONG
    F50 AS LONG
    F51 AS LONG
    F52 AS LONG
    F53 AS LONG
    F54 AS LONG
    F55 AS LONG
    F56 AS LONG
    F57 AS LONG
    F58 AS LONG
    F59 AS LONG
    F60 AS LONG
    F61 AS LONG
    F62 AS LONG
    F63 AS LONG
END TYPE

DECLARE FUNCTION Wide64Size LIB "liblayout_ext" ALIAS "wide64_size" () AS LONG
DECLARE FUNCTION Wide64Sum LIB "liblayout_ext" ALIAS "wide64_sum" (w AS Wide64) AS DOUBLE
DECLARE FUNCTION Wide64Inc LIB "liblayout_ext" ALIAS "wide64_inc" (w AS Wide64) AS Wide64

SUB Main()
    PRINT "sz="; Wide64Size()

    DIM w AS Wide64
    w.F0 = 1 : w.F1 = 2 : w.F2 = 3 : w.F3 = 4 : w.F4 = 5 : w.F5 = 6 : w.F6 = 7 : w.F7 = 8 : w.F8 = 9 : w.F9 = 10 : w.F10 = 11 : w.F11 = 12 : w.F12 = 13 : w.F13 = 14 : w.F14 = 15 : w.F15 = 16
    w.F16 = 17 : w.F17 = 18 : w.F18 = 19 : w.F19 = 20 : w.F20 = 21 : w.F21 = 22 : w.F22 = 23 : w.F23 = 24 : w.F24 = 25 : w.F25 = 26 : w.F26 = 27 : w.F27 = 28 : w.F28 = 29 : w.F29 = 30 : w.F30 = 31 : w.F31 = 32
    w.F32 = 33 : w.F33 = 34 : w.F34 = 35 : w.F35 = 36 : w.F36 = 37 : w.F37 = 38 : w.F38 = 39 : w.F39 = 40 : w.F40 = 41 : w.F41 = 42 : w.F42 = 43 : w.F43 = 44 : w.F44 = 45 : w.F45 = 46 : w.F46 = 47 : w.F47 = 48
    w.F48 = 49 : w.F49 = 50 : w.F50 = 51 : w.F51 = 52 : w.F52 = 53 : w.F53 = 54 : w.F54 = 55 : w.F55 = 56 : w.F56 = 57 : w.F57 = 58 : w.F58 = 59 : w.F59 = 60 : w.F60 = 61 : w.F61 = 62 : w.F62 = 63 : w.F63 = 64
    PRINT "sum="; Wide64Sum(w)

    DIM r AS Wide64
    r = Wide64Inc(w)
    PRINT "inc0="; r.F0
    PRINT "inc63="; r.F63
END SUB
