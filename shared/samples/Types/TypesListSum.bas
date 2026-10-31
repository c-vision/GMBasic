' TypesListSum.bas — test List.Max/Min/Sum

PRINT "=== List.Max/Min/Sum ==="

DIM list = []
list.Add(5)
list.Add(12)
list.Add(3)
list.Add(21)

PRINT "Join: "; list.Join(", ")
PRINT "Max: "; list.Max()
PRINT "Min: "; list.Min()
PRINT "Sum: "; list.Sum()

PRINT "=== Fine ==="
