' FILE: Collections/CollectionsSafeAccess.bas
' Safe access operator: coll[key]? returns NIL instead of error on missing key/index

IMPORT SYSTEM
IMPORT COLLECTIONS
IMPORT JSON

CLS
PRINT "=== Safe Access Operator (arr[i]?) ==="
PRINT

' --- DICT safe access ---
PRINT "--- 1. dict[key]? ---"
DIM d AS DICT<STRING, INTEGER> = {"a": 1, "b": 2}
PRINT "d[a]:  "; d["a"]
PRINT "d[x]?: "; d["x"]?
PRINT "IS NIL? "; (d["x"]? IS NIL)
PRINT

' --- LIST safe access ---
PRINT "--- 2. list[i]? ---"
DIM nums AS LIST<INTEGER> = [10, 20, 30]
PRINT "nums[0]:  "; nums[0]
PRINT "nums[5]?: "; nums[5]?
PRINT "IS NIL? "; (nums[5]? IS NIL)
PRINT

' --- Array safe access ---
PRINT "--- 3. array(i)? ---"
DIM arr() = {100, 200}
PRINT "arr(0):   "; arr(0)
PRINT "arr(9)?:  "; arr(9)?
PRINT "IS NIL? "; (arr(9)? IS NIL)
PRINT

' --- Combined with ?? ---
PRINT "--- 4. Safe + coalescing ---"
DIM v = d["x"]? ?? -1
PRINT "d[x]? ?? -1:"; v
PRINT

PRINT "=== Fine ==="
END
