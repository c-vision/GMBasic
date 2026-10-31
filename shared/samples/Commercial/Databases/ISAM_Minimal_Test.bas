' ISAM_Minimal_Test - Crea file, APPEND, GET
IMPORT ISAM
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

CLS
PRINT "=== ISAM_Minimal_Test ==="

' Cancella file precedente
ON ERROR RESUME NEXT
KILL g_work + "/test_min.isdb"
ON ERROR GOTO 0

TYPE R
    ID AS STRING * 10
    V AS STRING * 20
END TYPE

DIM rec AS R
DIM chk AS R

' Crea nuovo file
ISAM OPEN g_work + "/test_min.isdb" OF R ALIAS X INDEXED BY ID
PRINT "OPEN OK"

' Append
rec.ID = "T1"
rec.V = "TestVal1"
ISAM APPEND X, rec
PRINT "APPEND OK"

' Append un altro
rec.ID = "T2"
rec.V = "TestVal2"
ISAM APPEND X, rec
PRINT "APPEND2 OK"

' Go TOP e leggi
ISAM GO X, TOP
ISAM GET X, chk
PRINT "GET TOP: ID="; chk.ID; " V="; chk.V

' Seek T2
ISAM SEEK X, "T2"
IF ISAM.FOUND(X) THEN
    ISAM GET X, chk
    PRINT "SEEK T2: ID="; chk.ID; " V="; chk.V
ELSE
    PRINT "SEEK T2: NOT FOUND"
END IF

ISAM CLOSE X
KILL g_work + "/test_min.isdb"
PRINT "DONE"
