' ASCAN(array, valore) vs ASCAN(array, predicato): il valore numerico NON deve essere
' scambiato per un indirizzo di codice (regressione PCode stack-overflow, 2026-09-24).
' EXPECT: val160= 2|val99=-1|val25= 1
IMPORT ARRAYS
SUB Main()
    DIM v() = {10, 25, 160, 80}
    PRINT "val160="; ASCAN(v, 160)
    PRINT "val99="; ASCAN(v, 99)
    PRINT "val25="; ASCAN(v, 25)
END SUB
