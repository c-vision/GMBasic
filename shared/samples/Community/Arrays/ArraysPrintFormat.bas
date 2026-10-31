' FILE: Arrays/ArraysPrintFormat.bas
' PRINT format arrays/collections as [elem1, elem2, elem3]

IMPORT COLLECTIONS

CLS
PRINT "Array PRINT formatting (nuovo):"

PRINT "--- Array 1D ---"
DIM a() = {10, 20, 30, 40}
PRINT "a = "; a

PRINT "--- Array 2D ---"
DIM m(1, 1) AS INTEGER = {{1, 2}, {3, 4}}
PRINT "m = "; m

PRINT "--- Array strings ---"
DIM s$(2)
s$(0) = "foo" : s$(1) = "bar" : s$(2) = "baz"
PRINT "s$ = "; s$

PRINT "--- LIST OF INTEGER ---"
DIM nums AS LIST OF INTEGER = [5, 10, 15, 20]
PRINT "nums = "; nums

PRINT "--- LIST OF STRING ---"
DIM words AS LIST<STRING> = ["hello", "world"]
PRINT "words = "; words

PRINT "--- DICT ---"
DIM d AS DICT<STRING, INTEGER> = {"x": 1, "y": 2}
PRINT "d = "; d

PRINT "--- HASHSET ---"
DIM hs AS HASHSET<INTEGER> = [3, 1, 4, 1, 5]
PRINT "hs = "; hs

PRINT "--- QUEUE ---"
DIM q AS QUEUE<STRING> = ["a", "b", "c"]
PRINT "q = "; q

PRINT "--- STACK ---"
DIM st AS STACK<STRING> = ["top", "mid", "bot"]
PRINT "st = "; st

PRINT "--- Empty array ---"
DIM empty() AS INTEGER
PRINT "empty = "; empty

PRINT "Tutti i formati funzionano."
END
