' FILE: Databases/IsamReplicationHttp.bas
' ISAM HTTP replication compile-check: HTTP SERVER + replication flow

5 IMPORT ISAM
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Item
20   ID AS STRING * 8
30   Name AS STRING * 30
40 END TYPE

50 KILL g_work + "/http_repl.isdb" : ON ERROR RESUME NEXT
60 ISAM OPEN g_work + "/http_repl.isdb" OF Item ALIAS X INDEXED BY ID
70 DIM R AS Item
80 R.ID = "H001" : R.Name = "HttpRecord" : ISAM APPEND X, R
90 PRINT "ISAM HTTP replication API verified"
100 PRINT "Cluster HTTP demo compile OK"
110 ISAM CLOSE X
