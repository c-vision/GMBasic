' FILE: Types/TypesLinqDebug.bas
' Debug version - con output di tracciamento

IMPORT COLLECTIONS

CLS
PRINT "=== LINQ-style Operations DEBUG ==="
PRINT

DIM nums = {5, 12, 3, 21, 8, 15, 1, 30}
PRINT "Array nums creato: "; nums.Count(); " elementi"

PRINT "--- Test 1: Creazione lista vuota ---"
DIM big = []
PRINT "Lista big creata"

PRINT "--- Test 2: FOR EACH nell'array nums ---"
FOR EACH n IN nums
    PRINT "Elemento: "; n
    IF n > 10 THEN big.Add(n)
NEXT
PRINT "FOR EACH completato"

PRINT "Count: "; big.Count()
PRINT "Valori: "; big.Join(", ")

PRINT "=== FINE TEST ==="
END