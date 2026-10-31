' ISAM_InMem_Test - Test CRUD con file su disco
IMPORT ISAM
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

CLS
PRINT "=== ISAM_Test ==="

ON ERROR RESUME NEXT
KILL g_work + "/test_crud.isdb"
ON ERROR GOTO 0

TYPE R
    ID AS STRING * 10
    V AS STRING * 30
END TYPE

DIM rec AS R
DIM chk AS R

ISAM OPEN g_work + "/test_crud.isdb" OF R ALIAS T INDEXED BY ID
PRINT "OPEN OK"

rec.ID = "A1"
rec.V = "Valore1"
ISAM APPEND T, rec
PRINT "APPEND OK"

rec.ID = "A2"
rec.V = "Valore2"
ISAM APPEND T, rec
PRINT "APPEND2 OK"

ISAM GO T, TOP
ISAM GET T, chk
PRINT "TOP: ID="; chk.ID; " V="; chk.V

ISAM SEEK T, "A2"
IF ISAM.FOUND(T) THEN
    ISAM GET T, chk
    PRINT "SEEK: ID="; chk.ID; " V="; chk.V
ELSE
    PRINT "SEEK: NOT FOUND"
END IF

ISAM CLOSE T
KILL g_work + "/test_crud.isdb"
PRINT "DONE"
