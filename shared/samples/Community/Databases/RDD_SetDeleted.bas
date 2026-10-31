' RDD_SetDeleted.bas — SET DELETED legacy + qualified
' ===================================================================
' Legacy:  RDD SET DELETED ON | OFF
' Qualif:  RDD.SETDELETED(area, 1|0)
' ===================================================================
IMPORT IO
IMPORT ISAM
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== SET DELETED ==="

TYPE DelT
    ID AS STRING * 8
    Val AS LONG
END TYPE

KILL g_work + "/setdel.isdb"
ON ERROR RESUME NEXT
DIM d AS DelT
ISAM OPEN g_work + "/setdel.isdb" OF DelT ALIAS DL INDEXED BY ID
d.ID = "A1" : d.Val = 100 : ISAM APPEND DL, d
d.ID = "A2" : d.Val = 200 : ISAM APPEND DL, d
d.ID = "A3" : d.Val = 300 : ISAM APPEND DL, d
PRINT "3 record inseriti"
PRINT

' Legacy
RDD SET DELETED ON
PRINT "Legacy: RDD SET DELETED ON"
RDD SET DELETED OFF
PRINT "Legacy: RDD SET DELETED OFF"

' Qualificata
RDD.SETDELETED(1, 1)
PRINT "Qualif: RDD.SETDELETED(1,1)"
RDD.SETDELETED(1, 0)
PRINT "Qualif: RDD.SETDELETED(1,0)"
PRINT

ISAM CLOSE DL : KILL g_work + "/setdel.isdb"
PRINT "=== OK ==="
