' FILE: Functions/GeneratorExpr.bas
' Generator expressions (lazy): (expr FOR var IN coll [IF cond])
' FOR EACH iteration over generators and collections

IMPORT SYSTEM
IMPORT COLLECTIONS

CLS
PRINT "=== Generator Expressions & FOR EACH ==="
PRINT

' --- 1. Generator expression base ---
PRINT "--- 1. (x*x FOR x) ---"
DIM nums() = {1, 2, 3, 4, 5}
DIM g1 = (x * x FOR x IN nums)
PRINT "g1 type:"; TYPENAME(g1)
PRINT "values:";
FOR EACH v IN g1
    PRINT v;
NEXT
PRINT
PRINT

' --- 2. Con filtro IF ---
PRINT "--- 2. (x FOR x IF cond) ---"
DIM g2 = (x FOR x IN nums IF x MOD 2 = 0)
PRINT "even:";
FOR EACH v IN g2
    PRINT v;
NEXT
PRINT
PRINT

' --- 3. Su lista ---
PRINT "--- 3. (UPPER(s) FOR s IN list) ---"
DIM words AS LIST<STRING> = ["ciao", "mondo"]
DIM g3 = (UPPER(s) FOR s IN words)
PRINT "upper:";
FOR EACH v IN g3
    PRINT v;
NEXT
PRINT
PRINT

' --- 4. FOR EACH base su array ---
PRINT "--- 4. FOR EACH su array ---"
FOR EACH n IN nums
    PRINT n;
NEXT
PRINT
PRINT

' --- 5. FOR EACH su lista ---
PRINT "--- 5. FOR EACH su list ---"
FOR EACH w IN words
    PRINT w;
NEXT
PRINT
PRINT

PRINT "=== Fine ==="
END
