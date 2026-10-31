' AMAP/AFILTER/AREDUCE/ACOUNT/ASCAN/ASORT su array di UDT con FUNZIONE NOMINATA (non
' lambda): verifica la parita' 4 backend dell'ABI callback ctx-first (ClosureData) e
' dell'accumulatore AREDUCE double corretto il 2026-09-24.
' EXPECT: sort=ABC|scan= 0|map= 2|filter= 3|reduce= 6.75|count= 3
IMPORT ARRAYS
IMPORT STRINGS
TYPE Prodotto
    Nome AS STRING * 20
    Prezzo AS DOUBLE
END TYPE

FUNCTION ConfrontaProdotti(P1 AS Prodotto, P2 AS Prodotto) AS INTEGER
    IF P1.Prezzo < P2.Prezzo THEN RETURN -1
    IF P1.Prezzo > P2.Prezzo THEN RETURN 1
    RETURN 0
END FUNCTION
FUNCTION FiltraScorta(P AS Prodotto) AS BIT
    RETURN TRIM(P.Nome) <> ""
END FUNCTION
FUNCTION ApplicaSconto(P AS Prodotto) AS Prodotto
    RETURN P
END FUNCTION
FUNCTION SommaPrezzi(acc AS DOUBLE, P AS Prodotto) AS DOUBLE
    RETURN acc + P.Prezzo
END FUNCTION

SUB Main()
    DIM C(2) AS Prodotto
    C(0).Nome = "C" : C(0).Prezzo = 3.5
    C(1).Nome = "A" : C(1).Prezzo = 1.25
    C(2).Nome = "B" : C(2).Prezzo = 2.0
    ASORT C, ConfrontaProdotti
    PRINT "sort="; TRIM(C(0).Nome); TRIM(C(1).Nome); TRIM(C(2).Nome)
    PRINT "scan="; ASCAN(C, FiltraScorta)
    DIM M() = AMAP(C, ApplicaSconto)
    PRINT "map="; M(1).Prezzo
    DIM F() = AFILTER(C, FiltraScorta)
    PRINT "filter="; UBOUND(F) + 1
    PRINT "reduce="; AREDUCE(F, SommaPrezzi, 0.0)
    PRINT "count="; ACOUNT(F, FiltraScorta)
END SUB
