' RDD_Memory_CRUD.bas — ARRAY RDD in memoria con CRUD
' ===================================================================
' Database in memoria: zero I/O, massima velocità.
' Dimostra USE, GETFIELD, SETFIELD, BUFFER, TABLEUPDATE, eventi.
' ===================================================================
IMPORT IO
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== Memory RDD CRUD ==="
PRINT

' Apri database in memoria
PRINT "1. Apertura ARRAY RDD"
RDD USE g_work + "/mem_crud" VIA "ARRAY" ALIAS Mem
PRINT "   Area 1 aperta"
PRINT

' Lock + Unlock
PRINT "2. Lock"
ok = RDD.RLOCK(1)
PRINT "   RLOCK(1)="; ok
RDD UNLOCK 1
ok = RDD.FLOCK(1)
PRINT "   FLOCK(1)="; ok
RDD UNLOCK 1
RDD SET LOCK TIMEOUT 500
RDD SET LOCK TIMEOUT 0
PRINT "   SETLOCKTIMEOUT OK"
PRINT

' Buffer ottimistico
PRINT "3. BUFFER + TABLEUPDATE/TABLEREVERT"
ok = RDD.BUFFER(1, 3)
PRINT "   BUFFER(1,3) ottimistico="; ok
RDD SET FIELD 1, 1, 42
v = RDD.GETFIELD(1, 1)
PRINT "   SETFIELD(1,1,42) -> GETFIELD="; v
old = RDD.OLDVAL(1, 1)
PRINT "   OLDVAL(1,1)="; old
st = RDD.GETFLDSTATE(1, 1)
PRINT "   GETFLDSTATE(1,1)="; st
RDD.TABLEREVERT(1)
PRINT "   TABLEREVERT OK"
PRINT

' Eventi
PRINT "4. Eventi"
RDD ON EVENT "BeforeAppend"
RDD ON EVENT "AfterDelete"
RDD OFF EVENT "AfterDelete"
PRINT "   ONEVENT/OFFEVENT OK"
PRINT

' SETREPROCESS + Date/Currency
PRINT "5. SETREPROCESS + Tipi"
RDD SET REPROCESS 2000
RDD SET REPROCESS 0
d = RDD.TODAY()
PRINT "   TODAY="; d
c = RDD.CURVAL(49.99)
PRINT "   CURVAL="; c
PRINT

' SETDEFAULT
PRINT "6. SETDEFAULT + SETORDER"
RDD SET DEFAULT "ARRAY"
RDD SET DEFAULT "ISAM"
PRINT "   SETDEFAULT OK"
RDD SET ORDER 1, "Name"
PRINT "   SETORDER OK"
PRINT

PRINT "=== Memory CRUD OK ==="
