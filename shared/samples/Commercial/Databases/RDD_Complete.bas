' RDD_Complete.bas — Test suite comandi RDD
' ===================================================================
IMPORT IO
IMPORT ISAM
IMPORT DBF
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== RDD Complete Test ==="
PRINT

' --- 1. SETDEFAULT ---
PRINT "1. RDD.SETDEFAULT"
RDD SET DEFAULT "ISAM"
RDD SET DEFAULT "DBF"
RDD SET DEFAULT "ISAM"
PRINT "   OK"
PRINT

' --- 2. SchemaBuffer + macro filter ---
PRINT "2. Filtro &(expr)"

TYPE Product
    Code AS STRING * 8
    Name AS STRING * 32
    Price AS DOUBLE
    Qty AS LONG
END TYPE

KILL g_work + "/rdd_prod.isdb" : ON ERROR RESUME NEXT
DIM p AS Product
ISAM OPEN g_work + "/rdd_prod.isdb" OF Product ALIAS PR INDEXED BY Code

p.Code = "P001" : p.Name = "Widget" : p.Price = 19.99 : p.Qty = 100
ISAM APPEND PR, p
p.Code = "P002" : p.Name = "Gadget" : p.Price = 49.99 : p.Qty = 50
ISAM APPEND PR, p
p.Code = "P003" : p.Name = "Thing" : p.Price = 9.99 : p.Qty = 0
ISAM APPEND PR, p

DIM f AS STRING
f = "Price > 10 AND Qty > 0"
ISAM SET FILTER TO &(f) ON -1
ISAM GO PR, TOP
WHILE NOT ISAM.EOF(PR)
    ISAM GET PR, p
    PRINT "   "; p.Code; " "; p.Name; " $"; p.Price
    ISAM SKIP PR
WEND
ISAM CLOSE PR
KILL g_work + "/rdd_prod.isdb"
PRINT

' --- 3. Lock ---
PRINT "3. Lock RLOCK/FLOCK"

TYPE LockRec
    ID AS STRING * 8
    Val AS LONG
END TYPE

KILL g_work + "/rdd_lock.isdb" : ON ERROR RESUME NEXT
DIM lr AS LockRec
ISAM OPEN g_work + "/rdd_lock.isdb" OF LockRec ALIAS LK INDEXED BY ID
lr.ID = "K1" : lr.Val = 100
ISAM APPEND LK, lr
ISAM GO LK, TOP

ok = RDD.RLOCK(1)
PRINT "   RLOCK(1): "; ok
RDD UNLOCK 1

ok = RDD.FLOCK(1)
PRINT "   FLOCK(1): "; ok
RDD UNLOCK 1

RDD SET LOCK TIMEOUT 1500
RDD SET LOCK TIMEOUT 0
ISAM CLOSE LK
KILL g_work + "/rdd_lock.isdb"
PRINT

' --- 4. SchemaBuffer ---
PRINT "4. GETFIELD / SETFIELD"

TYPE FastRec
    Code AS STRING * 8
    Score AS LONG
END TYPE

KILL g_work + "/rdd_fast.isdb" : ON ERROR RESUME NEXT
DIM fr AS FastRec
ISAM OPEN g_work + "/rdd_fast.isdb" OF FastRec ALIAS FT INDEXED BY Code
fr.Code = "X1" : fr.Score = 42
ISAM APPEND FT, fr
ISAM GO FT, TOP
ISAM GET FT, fr

' Campo 2 = Score (LONG): il set/get numerico e' coerente col tipo. Prima 1,1 puntava a Code (STRING*8): il valore
' tornava come testo "99" sul gestito e come NUMERO sui backend C (cell-store senza tipo) e `v` non tipizzata
' (DOUBLE di default) azzerava anche il testo "X1" in IL. Si stampa il risultato direttamente.
PRINT "   GETFIELD(1,2): "; RDD.GETFIELD(1, 2)
RDD SET FIELD 1, 2, 99
PRINT "   SETFIELD(1,2,99): "; RDD.GETFIELD(1, 2)
ISAM CLOSE FT
KILL g_work + "/rdd_fast.isdb"
PRINT

' --- 5. Eventi ---
PRINT "5. Eventi ONEVENT/OFFEVENT"
RDD ON EVENT "BeforeNavigate"
RDD ON EVENT "AfterAppend"
RDD OFF EVENT "BeforeNavigate"
PRINT "   OK"
PRINT

' --- 6. Date/Currency ---
PRINT "6. Tipi Date/Currency"
d = RDD.TODAY()
PRINT "   TODAY: "; d
d2 = RDD.CTOD("2026-12-25")
PRINT "   DTOC: "; RDD.DTOC(d2)
c = RDD.CURVAL(123.4567)
PRINT "   CURVAL: "; c
PRINT

PRINT "=== 6/6 test RDD completati ==="
