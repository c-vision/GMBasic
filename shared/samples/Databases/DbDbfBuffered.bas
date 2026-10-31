' FILE: Databases/DbDbfBuffered.bas
' Compile-check: DBF.SETDURABILITY / DBF.FLUSHDURABILITY con modalità Buffered.

IMPORT DBF
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

TYPE Row
  Name AS STRING * 20
END TYPE

KILL g_work + "/dbf_buffered.dbf" : ON ERROR RESUME NEXT
DBF CREATE g_work + "/dbf_buffered.dbf" FROM TYPE Row
DBF USE g_work + "/dbf_buffered.dbf" ALIAS "Buf" AS Row

PRINT DBF.SETDURABILITY("Buffered")
DBF APPEND RECORD {Name: "One"}
DBF APPEND RECORD {Name: "Two"}
PRINT DBF.FLUSHDURABILITY()
PRINT DBF.SETDURABILITY("Full")

DBF CLOSE
