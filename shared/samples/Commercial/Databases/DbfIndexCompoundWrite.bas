' FILE: Databases/DbfIndexCompoundWrite.bas
' INDEX ON con TAG ... OF per creare tag MDX/CDX da BASIC.

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 DIM cWork AS STRING
20 DIM cFixture AS STRING

30 cWork = g_work + "/dbf_index_compound_write_work"
40 cFixture = "../Fixtures/Dbf/working/dbase3/dbase_03.dbf"

100 ON ERROR RESUME NEXT
110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE
120 MKDIR cWork
130 DBF SET DEFAULT TO cWork

140 COPY cFixture TO g_work + "/dbase_03.dbf"

150 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE
160 DBF INDEX ON SHAPE TAG "BY_SHAPE" OF "tags.mdx"
170 PRINT "MDX tag BY_SHAPE creato"
180 DBF INDEX ON CONDITION TAG "BY_COND" OF "tags.cdx"
190 PRINT "CDX tag BY_COND creato"
200 DBF CLOSE

210 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE INDEX "tags.mdx", "tags.cdx" ALIAS Dbf03
220 DBF SET ORDER TO "BY_SHAPE"
230 DBF GO TOP
240 PRINT "MDX BY_SHAPE first recno="; DBF.RECNO()

250 DBF SET ORDER TO "BY_COND"
260 DBF GO TOP
270 PRINT "CDX BY_COND first recno="; DBF.RECNO()

280 DBF CLOSE
290 END
