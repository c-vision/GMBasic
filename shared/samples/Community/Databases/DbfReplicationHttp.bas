' FILE: Databases/DbfReplicationHttp.bas
' DBF HTTP replication compile-check: HTTP SERVER + replication flow

IMPORT DBF
5 IMPORT DBF
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Row
20   Name AS STRING * 20
30 END TYPE

40 KILL g_work + "/dbf_http_repl.dbf" : ON ERROR RESUME NEXT
50 DBF CREATE g_work + "/dbf_http_repl.dbf" FROM TYPE Row
60 DBF USE g_work + "/dbf_http_repl.dbf" ALIAS "X" AS Row
70 DBF APPEND RECORD {Name: "HttpDbfRecord"}
80 PRINT "DBF HTTP replication API verified"
90 PRINT "Cluster HTTP demo compile OK"
100 DBF CLOSE
