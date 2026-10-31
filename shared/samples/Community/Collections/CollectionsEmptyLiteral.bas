' FILE: Collections/CollectionsEmptyLiteral.bas
' Empty collection literal target typing
' DIM x AS LIST OF INTEGER = []  — inferisce List<INTEGER> da contesto

IMPORT COLLECTIONS
IMPORT JSON

CLS
PRINT "=== Empty Literal Target Typing ==="
PRINT

' --- LIST OF INTEGER ---
PRINT "--- 1. [] con tipo target ---"
DIM nums AS LIST OF INTEGER = []
PRINT "nums type:"; TYPENAME(nums)
PRINT "nums count:"; nums.Count
nums.Add(42)
PRINT "nums[0]:"; nums[0]
PRINT

' --- LIST OF STRING ---
PRINT "--- 2. [] con STRING target ---"
DIM names AS LIST<STRING> = []
PRINT "names type:"; TYPENAME(names)
names.Add("hello")
PRINT "names[0]:"; names[0]
PRINT

' --- DICT OF STRING, INTEGER (vuoto) ---
PRINT "--- 3. {} dict con tipo target ---"
DIM dict AS DICT OF STRING, INTEGER = {"x": 10}
PRINT "dict type:"; TYPENAME(dict)
dict("y") = 20
PRINT "dict keys:"; JSON.Stringify(dict.Keys())
PRINT

PRINT "=== Fine ==="
END
