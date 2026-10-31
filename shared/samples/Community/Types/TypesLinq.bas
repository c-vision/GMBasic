' FILE: Types/TypesLinq.bas
' LINQ-style queries on collections
' NOTA: FROM...SELECT syntax non ancora implementato.
'       Test con operazioni base su liste.

IMPORT COLLECTIONS

CLS
PRINT "=== LINQ-style Operations ==="
PRINT

DIM nums = {5, 12, 3, 21, 8, 15, 1, 30}

PRINT "--- Numeri > 10 (manuale) ---"
' WHERE non ancora implementato: test manuale
DIM big = []
FOR EACH n IN nums
    IF n > 10 THEN big.Add(n)
NEXT
PRINT "Count: "; big.Count()
PRINT "Valori: "; big.Join(", ")

PRINT "--- Ordinamento ---"
nums.Sort()
PRINT "Sorted: "; nums.Join(", ")

nums.Reverse()
PRINT "Reverse: "; nums.Join(", ")

PRINT "--- Max/Min/Sum ---"
PRINT "Max: "; nums.Max()
PRINT "Min: "; nums.Min()

PRINT "--- Filtraggio + Ordinamento ---"
DIM result = []
FOR EACH n IN nums
    IF n > 5 AND n MOD 2 = 0 THEN result.Add(n)
NEXT
result.Sort()
result.Reverse()
PRINT "Pari > 5 DESC: "; result.Join(", ")

PRINT
PRINT "=== FROM...SELECT Syntax ==="

PRINT "--- Numeri > 10 (FROM...SELECT) ---"
DIM fromSel = FROM n IN nums WHERE n > 10 SELECT n
PRINT "Count: "; fromSel.Count()

PRINT "--- Ordinati (ORDER BY) ---"
DIM ordered = FROM n IN nums ORDER BY n SELECT n
PRINT "Sorted: "; ordered.Join(", ")

PRINT "--- Raddoppiati (SELECT) ---"
DIM doubled = FROM n IN nums SELECT n * 2
PRINT "Doubled: "; doubled.Join(", ")

PRINT
PRINT "=== Fine ==="
