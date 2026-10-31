' FILE: Databases/DbfMemoCrud.bas
' Golden dBase III/IV memo fixtures: read existing memo, append/put, round-trip.
' Fixtures: Interpreter/Samples/Fixtures/Dbf/readonly/dbase3-memo, dbase4-memo.

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 DIM cWork AS STRING
20 DIM cGolden83 AS STRING
30 DIM cGolden8b AS STRING
40 DIM sMarker AS STRING
50 DIM nBefore AS LONG

60 cWork = g_work + "/dbf_memo_crud_work"
70 cGolden83 = "../Fixtures/Dbf/readonly/dbase3-memo/dbase_83.dbf"
80 cGolden8b = "../Fixtures/Dbf/readonly/dbase4-memo/dbase_8b.dbf"
90 sMarker = "GM-BASIC DbfMemoCrud marker"

100 ON ERROR RESUME NEXT
110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE
120 ON ERROR RESUME NEXT
130 MKDIR cWork
140 DBF SET DEFAULT TO cWork

150 COPY cGolden83 TO g_work + "/dbase_83.dbf"
160 COPY "../Fixtures/Dbf/readonly/dbase3-memo/dbase_83.dbt" TO "dbase_83.dbt"

170 DBF USE g_work + "/dbase_83.dbf" EXCLUSIVE ALIAS D83
180 nBefore = DBF.LASTREC()
190 DBF GO TOP
200 DBF GET
210 PRINT "dBase III memo read len="; LEN(DESC)

220 DBF APPEND BLANK
230 DBF REPLACE ID WITH 999999, DESC WITH sMarker
240 DBF GO BOTTOM
250 DBF GET
260 PRINT "dBase III memo write="; DESC
270 DBF CLOSE

280 COPY cGolden8b TO g_work + "/dbase_8b.dbf"
290 COPY "../Fixtures/Dbf/readonly/dbase4-memo/dbase_8b.dbt" TO "dbase_8b.dbt"

300 DBF USE g_work + "/dbase_8b.dbf" EXCLUSIVE ALIAS D84
310 DBF GO TOP
320 DBF GET
330 PRINT "dBase IV memo read len="; LEN(MEMO)

340 DBF REPLACE MEMO WITH sMarker
350 DBF GET
360 PRINT "dBase IV memo put="; MEMO
370 DBF CLOSE

380 END
