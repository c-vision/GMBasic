' TestSuite.bas — Comprehensive FOR EACH / FOR / List / Block tests
' Deve funzionare identicamente in GMBasic e GMBasicAI
' NOTA GMBasic: Test 5 e 18 usano Max() su lista popolata via FOR EACH.
'   In GMBasic questi test sono commentati (bug HandleEndOfCodeStream).

PRINT "========================================"
PRINT "=== COMPREHENSIVE LOOP & LIST TESTS ==="
PRINT "========================================"

' --- Test 1: FOR EACH semplice ---
PRINT "Test 1: FOR EACH semplice"
DIM t1 = 0
FOR EACH x IN {1, 2, 3}
    t1 = t1 + x
NEXT
PRINT "  Sum 1..3 = "; t1

' --- Test 2: FOR EACH + IF ---
PRINT "Test 2: FOR EACH + IF"
DIM t2 = 0
FOR EACH x IN {1, 2, 3, 4, 5}
    IF x > 3 THEN t2 = t2 + x
NEXT
PRINT "  Sum >3 = "; t2

' --- Test 3: FOR EACH + Add + Join ---
PRINT "Test 3: FOR EACH + Add + Join"
DIM t3 = []
FOR EACH x IN {10, 20, 30}
    t3.Add(x * 2)
NEXT
PRINT "  Doubled: "; t3.Join(",")

' --- Test 4: FOR EACH + Add + Count ---
PRINT "Test 4: FOR EACH + Add + Count"
DIM t4 = []
FOR EACH x IN {5, 15, 25, 35}
    IF x > 10 THEN t4.Add(x)
NEXT
PRINT "  Count: "; t4.Count()

' --- Test 5: FOR EACH + Add + Max (lista inizialmente vuota) ---
PRINT "Test 5: FOR EACH + Add + Max"
DIM t5 = []
FOR EACH x IN {5, 12, 3, 21, 8}
    IF x > 10 THEN t5.Add(x)
NEXT
PRINT "  Count: "; t5.Count()
PRINT "  Join: "; t5.Join(",")
PRINT "  Max: "; t5.Max()
PRINT "  Min: "; t5.Min()
PRINT "  Sum: "; t5.Sum()

' --- Test 6: List pre-popolata + FOR EACH + Max (funziona in entrambi) ---
PRINT "Test 6: List pre-popolata + FOR EACH + Max"
DIM t6 = []
t6.Add(100)
FOR EACH x IN {5, 12, 3}
    IF x > 10 THEN t6.Add(x)
NEXT
PRINT "  Count: "; t6.Count()
PRINT "  Max: "; t6.Max()

' --- Test 7: FOR EACH annidato ---
PRINT "Test 7: FOR EACH annidato"
DIM t7a = []
DIM t7b = []
FOR EACH x IN {1, 2, 3}
    t7a.Add(x)
    FOR EACH y IN {10, 20}
        t7b.Add(x * y)
    NEXT
NEXT
PRINT "  Outer count: "; t7a.Count()
PRINT "  Inner count: "; t7b.Count()
PRINT "  Inner join: "; t7b.Join(",")

' --- Test 8: FOR EACH vuoto ---
PRINT "Test 8: FOR EACH su lista vuota"
DIM t8 = []
FOR EACH x IN t8
    PRINT "  Should NOT print"
NEXT
PRINT "  OK (empty)"

' --- Test 9: FOR regolare ---
PRINT "Test 9: FOR regolare"
DIM i AS INTEGER
DIM t9 = 0
FOR i = 1 TO 5
    t9 = t9 + i
NEXT i
PRINT "  Sum 1..5 = "; t9

' --- Test 10: FOR con STEP ---
PRINT "Test 10: FOR con STEP"
DIM t10 = 0
FOR i = 0 TO 10 STEP 2
    t10 = t10 + i
NEXT i
PRINT "  Sum even 0..10 = "; t10

' --- Test 11: FOR annidati ---
PRINT "Test 11: FOR annidati"
DIM j AS INTEGER
DIM t11 = 0
FOR i = 1 TO 3
    FOR j = 1 TO 2
        t11 = t11 + i * j
    NEXT j
NEXT i
PRINT "  Sum i*j = "; t11

' --- Test 12: List.Max diretta (popolata con Add) ---
PRINT "Test 12: List.Max diretta"
DIM t12 = []
t12.Add(5)
t12.Add(12)
t12.Add(3)
t12.Add(21)
PRINT "  Max: "; t12.Max()
PRINT "  Min: "; t12.Min()
PRINT "  Sum: "; t12.Sum()
PRINT "  Join: "; t12.Join(", ")

' --- Test 13: FROM...SELECT ---
PRINT "Test 13: FROM...SELECT"
DIM nums = {5, 12, 3, 21, 8, 15}
DIM t13 = FROM n IN nums WHERE n > 10 SELECT n
PRINT "  Count: "; t13.Count()

' --- Test 14: FROM...SELECT + ORDER BY ---
PRINT "Test 14: FROM...SELECT ORDER BY"
DIM t14 = FROM n IN nums ORDER BY n SELECT n
PRINT "  Sorted: "; t14.Join(",")

' --- Test 14b: FROM...SELECT + ORDER BY DESC ---
PRINT "Test 14b: FROM...SELECT ORDER BY DESC"
DIM t14b = FROM n IN nums ORDER BY n DESC SELECT n
PRINT "  Sorted desc: "; t14b.Join(",")

' --- Test 15: FOR EACH + Sort + Max ---
PRINT "Test 15: FOR EACH + Sort + Max"
DIM t15 = []
FOR EACH x IN {30, 10, 20}
    t15.Add(x)
NEXT
t15.Sort()
PRINT "  Sorted: "; t15.Join(",")
PRINT "  Min: "; t15.Min()
PRINT "  Max: "; t15.Max()

' --- Test 16: FOR EACH + IF/ELSE + 2 liste ---
PRINT "Test 16: FOR EACH + IF/ELSE + 2 liste"
DIM t16_hi = []
DIM t16_lo = []
FOR EACH x IN {5, 15, 25, 3, 18}
    IF x > 10 THEN
        t16_hi.Add(x)
    ELSE
        t16_lo.Add(x)
    END IF
NEXT
PRINT "  >10: "; t16_hi.Join(",")
PRINT "  <=10: "; t16_lo.Join(",")

' --- Test 17: FOR EACH + un solo match + Max (con pre-add) ---
PRINT "Test 17: FOR EACH + un solo match + Max"
DIM t17 = []
t17.Add(1)
FOR EACH x IN {5, 12, 3}
    IF x > 10 THEN t17.Add(x)
NEXT
PRINT "  Count: "; t17.Count()
PRINT "  Max: "; t17.Max()

' --- Test 18: FOR EACH + Clear + ri-add + Max ---
PRINT "Test 18: FOR EACH + Clear + ri-add + Max"
DIM t18 = []
t18.Add(99)
t18.Clear()
FOR EACH x IN {5, 12, 3}
    IF x > 10 THEN t18.Add(x)
NEXT
PRINT "  Count: "; t18.Count()
PRINT "  Join: "; t18.Join(",")
PRINT "  Max: "; t18.Max()

' --- Test 19: Array.Max/Min/Sum ---
PRINT "Test 19: Array.Max/Min/Sum"
DIM t19 = {5, 12, 3, 21}
PRINT "  Max: "; t19.Max()
PRINT "  Min: "; t19.Min()

PRINT "========================================"
PRINT "=== ALL TESTS COMPLETE ==="
PRINT "========================================"
