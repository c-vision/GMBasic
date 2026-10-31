' RDD_Mixed_Demo.bas — Legacy + RDD mix
' ===================================================================
' Verifica che comandi legacy (ISAM USE) attivino automaticamente
' il driver RDD, permettendo di usare comandi RDD sulla stessa area.
' ===================================================================
IMPORT IO
IMPORT ISAM
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== Mixed Legacy + RDD ==="

TYPE MixT
    ID AS STRING * 8
    Val AS LONG
END TYPE

KILL g_work + "/mixed_demo.isdb"
ON ERROR RESUME NEXT

' 1. Legacy ISAM USE → attiva IsamDriver in DataSession
PRINT "1. Legacy ISAM USE"
DIM m AS MixT
ISAM OPEN g_work + "/mixed_demo.isdb" OF MixT ALIAS MX INDEXED BY ID
m.ID = "A1" : m.Val = 100 : ISAM APPEND MX, m
m.ID = "A2" : m.Val = 200 : ISAM APPEND MX, m
PRINT "   2 record appesi (legacy)"
PRINT

' 2. RDD lock sulla stessa area
PRINT "2. RDD lock su area ISAM"
ok = RDD.RLOCK(1)
PRINT "   RLOCK(1): "; ok
RDD UNLOCK 1
PRINT "   UNLOCK OK"

ok = RDD.FLOCK(1)
PRINT "   FLOCK(1): "; ok
RDD UNLOCK 1
PRINT

' 3. RDD buffer su area ISAM
PRINT "3. RDD buffer su area legacy"
ISAM GO MX, TOP
ok = RDD.BUFFER(1, 2)
PRINT "   BUFFER(1,2): "; ok
RDD SET FIELD 1, 1, 999
v = RDD.GETFIELD(1, 1)
PRINT "   GETFIELD dopo modifica: "; v
RDD.TABLEREVERT(1)
PRINT "   TABLEREVERT OK"
PRINT

' 4. SETREPROCESS
PRINT "4. SETREPROCESS"
RDD SET REPROCESS 1000
RDD SET REPROCESS 0
PRINT "   OK"

ISAM CLOSE MX
KILL g_work + "/mixed_demo.isdb"
PRINT
PRINT "=== Mixed Demo OK ==="
