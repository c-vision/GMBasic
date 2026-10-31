' FILE: Databases/DbfIndexCompound.bas
' Apre MDX/CDX compound (tag catalog); SET ORDER per nome tag; SEEK.
' Fixture: ../Fixtures/Dbf/working/index-formats/ (tags.mdx, tags.cdx, dbase_03.dbf)

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 DIM cWork AS STRING
20 DIM cFixture AS STRING
30 DIM cKey AS STRING
40 DIM nRec AS LONG

50 cWork = g_work + "/dbf_index_compound_work"
60 cFixture = "../Fixtures/Dbf/working/index-formats"

100 ON ERROR RESUME NEXT
110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE
120 MKDIR cWork
130 DBF SET DEFAULT TO cWork

140 COPY cFixture + "/dbase_03.dbf" TO g_work + "/dbase_03.dbf"
150 COPY cFixture + "/tags.mdx" TO "tags.mdx"
160 COPY cFixture + "/tags.cdx" TO "tags.cdx"

170 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE INDEX "tags.cdx" ALIAS Dbf03
180 DBF SET ORDER TO "PRIMARY"
190 DBF SEEK 5
200 nRec = DBF.RECNO()
210 PRINT "CDX tag PRIMARY SEEK recno=5 ->"; nRec; " found="; DBF.FOUND()

230 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE INDEX "tags.mdx" ALIAS Dbf03Mdx
240 DBF SET ORDER TO "BY_RECNO"
250 DBF SEEK 10
260 PRINT "MDX tag BY_RECNO SEEK recno=10 ->"; DBF.RECNO(); " found="; DBF.FOUND()

270 DBF SET ORDER TO "BY_TYPE"
280 DBF GO TOP
290 PRINT "MDX tag BY_TYPE first recno="; DBF.RECNO()

300 DBF CLOSE
310 END
