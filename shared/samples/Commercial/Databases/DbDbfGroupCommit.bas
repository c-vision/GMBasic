' FILE: Databases/DbDbfGroupCommit.bas
' Compile-check: DBF.BEGINGROUPCOMMIT / DBF.ENDGROUPCOMMIT batch fsync in Full mode.

IMPORT DBF
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

TYPE Row
  Name AS STRING * 20
END TYPE

KILL g_work + "/dbf_groupcommit.dbf" : ON ERROR RESUME NEXT
DBF CREATE g_work + "/dbf_groupcommit.dbf" FROM TYPE Row
DBF USE g_work + "/dbf_groupcommit.dbf" ALIAS "Grp" AS Row

PRINT DBF.SETDURABILITY("Full")
PRINT DBF.BEGINGROUPCOMMIT()
DBF APPEND RECORD {Name: "One"}
DBF APPEND RECORD {Name: "Two"}
DBF APPEND RECORD {Name: "Three"}
PRINT DBF.ENDGROUPCOMMIT()

PRINT "LASTREC="; DBF.LASTREC()
DBF CLOSE
