' TypesForeachAdd.bas — test FOR EACH + Add pattern

PRINT "=== FOR EACH + Add ==="

DIM nums = {5, 12, 3, 21, 8, 15, 1, 30}

PRINT "--- Filtraggio manuale ---"
DIM filtered = []
FOR EACH x IN nums
    IF x > 10 THEN filtered.Add(x)
NEXT
PRINT "Count: "; filtered.Count()
PRINT "Join: "; filtered.Join(", ")

PRINT "--- Secondo filtro ---"
DIM result = []
FOR EACH x IN nums
    IF x > 5 AND x MOD 2 = 0 THEN result.Add(x)
NEXT
PRINT "Pari > 5: "; result.Join(", ")

PRINT "--- FOR EACH annidato ---"
DIM products = []
FOR EACH x IN filtered
    FOR EACH y IN filtered
        products.Add(x * y)
    NEXT
NEXT
PRINT "Prodotti: "; products.Count()

PRINT "=== Fine ==="
