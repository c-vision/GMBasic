' FILE: Databases/DbDbfAppendMany.bas
' Compile-check: DBF.APPENDMANY su LIST OF record UDT + DBF APPEND FROM.

IMPORT DBF
5 IMPORT DBF
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Row
20   Name AS STRING * 20
30 END TYPE

40 KILL g_work + "/dbf_appendmany.dbf" : ON ERROR RESUME NEXT
50 DBF CREATE g_work + "/dbf_appendmany.dbf" FROM TYPE Row
60 DBF USE g_work + "/dbf_appendmany.dbf" ALIAS "Bulk" AS Row

70 DIM batch AS LIST OF Row
80 batch = NEW LIST OF Row {}
90 DIM R AS Row
100 R.Name = "Alpha" : batch.Add(R)
110 R.Name = "Beta" : batch.Add(R)

120 DIM n AS LONG
130 n = DBF.APPENDMANY(Bulk, batch)
140 PRINT "APPENDMANY inserted "; n

150 DBF COPY STRUCTURE TO g_work + "/dbf_appendfrom_src.dbf"
160 DBF USE g_work + "/dbf_appendfrom_src.dbf" ALIAS "Src" AS Row
170 DBF APPEND RECORD {Name: "Gamma"}
180 DBF CLOSE
190 DBF SELECT Bulk
200 DBF APPEND FROM g_work + "/dbf_appendfrom_src.dbf"
210 PRINT "APPENDFROM total "; DBF.LASTREC()

220 DBF CLOSE
