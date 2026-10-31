' FILE: Databases/DbfIndexNtxNdx.bas
' INDEX ON → Clipper NTX (0x0006) e dBase NDX; SEEK, SET ORDER, REINDEX.

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 DIM cWork AS STRING
20 DIM cKey AS STRING
30 DIM nFound AS LONG
40 DIM nBefore AS LONG

50 cWork = g_work + "/dbf_index_ntx_ndx_work"

100 ON ERROR RESUME NEXT
110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE
120 MKDIR cWork
130 DBF SET DEFAULT TO cWork

140 TYPE Item
150    ID AS INTEGER
160    NAME AS STRING * 20
170 END TYPE

180 DBF CREATE g_work + "/items.dbf" FROM TYPE Item
190 DBF APPEND RECORD {ID: 1, NAME: "Alpha"}
200 DBF APPEND RECORD {ID: 2, NAME: "Beta"}
210 DBF APPEND RECORD {ID: 3, NAME: "Gamma"}
220 DBF APPEND RECORD {ID: 4, NAME: "Delta"}
230 DBF CLOSE

240 DBF USE g_work + "/items.dbf" EXCLUSIVE UNSAFE
250 DBF INDEX ON STR(DBF.RECNO()) TO g_work + "/by_recno.ntx"
260 PRINT "NTX Clipper creato: by_recno.ntx"
270 DBF INDEX ON NAME TO g_work + "/by_name.ndx"
280 PRINT "NDX creato: by_name.ndx"
290 DBF CLOSE

300 DBF USE g_work + "/items.dbf" EXCLUSIVE INDEX g_work + "/by_recno.ntx", g_work + "/by_name.ndx" ALIAS Items
310 DBF SET ORDER TO "by_recno"
320 DBF SEEK 3
330 nFound = DBF.RECNO()
340 PRINT "SEEK by_recno recno=3 ->"; nFound; " found="; DBF.FOUND()

360 DBF SET ORDER TO "by_name"
370 DBF SEEK "Gamma"
380 PRINT "SEEK by_name Gamma recno="; DBF.RECNO()

390 nBefore = DBF.LASTREC()
400 DBF APPEND BLANK
410 DBF REPLACE ID WITH 5, NAME WITH "Epsilon"
420 DBF REINDEX
430 PRINT "REINDEX dopo append: lastrec="; DBF.LASTREC()

440 DBF SET ORDER TO "by_recno"
450 DBF SEEK 5
460 PRINT "SEEK nuovo record recno="; DBF.RECNO(); " found="; DBF.FOUND()

470 DBF CLOSE
480 END
