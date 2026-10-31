' ASORT con ordine discendente passato come STRINGA ("D"), non solo come variabile.
' EXPECT: asc= 10 , 50|desc= 50 , 10
IMPORT ARRAYS
IMPORT STRINGS
SUB Main()
    DIM n() = {50, 10, 20, 40, 30}
    ASORT n
    PRINT "asc="; n(0); ","; n(4)
    ASORT n, , "D"
    PRINT "desc="; n(0); ","; n(4)
END SUB
