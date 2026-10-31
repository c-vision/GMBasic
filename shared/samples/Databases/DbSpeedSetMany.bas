' FILE: Databases/DbSpeedSetMany.bas
' Compile-check: SPEED.SETMANY su LIST OF record UDT.

IMPORT SPEED
5 IMPORT SPEED
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Row
20   ID AS LONG
30   Name AS STRING * 32
40 END TYPE

50 RMDIR g_work + "/speed_setmany_db" : ON ERROR RESUME NEXT
60 SPEED OPEN g_work + "/speed_setmany_db" ALIAS BulkDb
70 SPEED USE "Rows" FROM BulkDb OF Row ALIAS Bulk INDEXED BY ID, Name

80 DIM batch AS LIST OF Row
90 batch = NEW LIST OF Row {}
100 DIM R1 AS Row
110 R1.ID = 1 : R1.Name = "Alpha" : batch.Add(R1)
120 DIM R2 AS Row
130 R2.ID = 2 : R2.Name = "Beta" : batch.Add(R2)

140 DIM n AS LONG
150 n = SPEED.SETMANY(Bulk, batch)
160 PRINT "SETMANY inserted "; n

' NOTA: SPEED non ha (ancora) un comando/funzione CLOSE — l'engine chiude i database
' al termine del programma. La riga SPEED.CLOSE è stata rimossa: non registrata.
' SPEED.CLOSE
