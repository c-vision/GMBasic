' RDD_Legacy.bas — Sintassi legacy + qualificata RDD a confronto
' ===================================================================
IMPORT IO
IMPORT ISAM
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== RDD Legacy + Qualified ==="
PRINT

' --- 1. SET DEFAULT TO (legacy) vs RDD.SETDEFAULT (qualified) ---
PRINT "1. SET DEFAULT"
RDD SET DEFAULT TO "DBF"
PRINT "   Legacy: RDD SET DEFAULT TO DBF"
RDD SET DEFAULT "ISAM"
PRINT "   Qualif: RDD SET DEFAULT 'ISAM'"
PRINT

' --- 2. USE legacy vs RDD.USE ---
PRINT "2. USE"
PRINT "   Legacy: RDD USE 'file' VIA 'driver' ALIAS x"
PRINT "   Qualif: RDD.USE('file', 'driver', 'alias')"
PRINT "   Auto:   RDD USE 'file.isdb' ALIAS x"
PRINT

' --- 3. RLOCK/FLOCK/UNLOCK legacy vs qualificati ---
TYPE LockT
    ID AS STRING * 8
    Val AS LONG
END TYPE
KILL g_work + "/legacy_lock.isdb"
ON ERROR RESUME NEXT
DIM lt AS LockT
ISAM OPEN g_work + "/legacy_lock.isdb" OF LockT ALIAS LK INDEXED BY ID
lt.ID = "K1" : lt.Val = 100 : ISAM APPEND LK, lt
ISAM GO LK, TOP

PRINT "3. RLOCK/FLOCK/UNLOCK"
ok = RDD.RLOCK(1)
PRINT "   Legacy: RDD RLOCK 1"
RDD UNLOCK 1
PRINT "   Legacy: RDD UNLOCK 1"
ok = RDD.FLOCK(1)
PRINT "   Legacy: RDD FLOCK 1"
RDD UNLOCK 1
PRINT "   Legacy: RDD UNLOCK 1"

ok = RDD.RLOCK(1)
PRINT "   Qualif: RDD.RLOCK(1)="; ok
RDD UNLOCK 1
PRINT "   Qualif: RDD.UNLOCK 1"
ISAM CLOSE LK : KILL g_work + "/legacy_lock.isdb"
PRINT

' --- 4. SETREPROCESS ---
PRINT "4. SETREPROCESS"
RDD SET REPROCESS 2000
PRINT "   Qualif: RDD SET REPROCESS 2000"
RDD SET REPROCESS 0
PRINT

' --- 5. Tipi Date/Currency ---
PRINT "5. Tipi nativi"
d = RDD.TODAY()
PRINT "   TODAY="; d
c = RDD.CURVAL(199.99)
PRINT "   CURVAL="; c
PRINT

' --- 6. Eventi ---
PRINT "6. Eventi"
RDD ON EVENT "BeforeNavigate"
PRINT "   ONEVENT OK"
RDD OFF EVENT "BeforeNavigate"
PRINT "   OFFEVENT OK"

PRINT
PRINT "=== Legacy + Qualified OK ==="
