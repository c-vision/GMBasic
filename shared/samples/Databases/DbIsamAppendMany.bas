' FILE: Databases/DbIsamAppendMany.bas
' Compile-check: ISAM.APPENDMANY su LIST OF TYPE (PK-only, bulk session automatica).

IMPORT ISAM
5 IMPORT ISAM
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Row
20   ID AS LONG
30   Name AS STRING * 20
40 END TYPE

50 KILL g_work + "/appendmany.isdb" : ON ERROR RESUME NEXT
60 ISAM OPEN g_work + "/appendmany.isdb" OF Row ALIAS Bulk INDEXED BY ID

70 DIM batch AS LIST OF Row
80 batch = NEW LIST OF Row {}
90 DIM R1 AS Row
100 R1.ID = 1 : R1.Name = "Alpha" : batch.Add(R1)
110 DIM R2 AS Row
120 R2.ID = 2 : R2.Name = "Beta" : batch.Add(R2)

130 DIM n AS LONG
140 n = ISAM.APPENDMANY(Bulk, batch)
150 PRINT "APPENDMANY inserted "; n

160 ISAM CLOSE Bulk
