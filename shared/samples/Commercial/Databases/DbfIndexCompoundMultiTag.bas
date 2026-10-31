' FILE: Databases/DbfIndexCompoundMultiTag.bas
' Append di un secondo tag sullo stesso compound MDX/CDX (BY_COND + BY_SHAPE).
' Verifica SET ORDER su entrambi i tag senza perdere il primo.

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 DIM cWork AS STRING
20 DIM cFixture AS STRING
30 DIM nRec AS LONG

40 cWork = g_work + "/dbf_index_compound_multitag_work"
50 cFixture = "../Fixtures/Dbf/working/dbase3/dbase_03.dbf"

100 ON ERROR RESUME NEXT
110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE
120 MKDIR cWork
130 DBF SET DEFAULT TO cWork

140 COPY cFixture TO g_work + "/dbase_03.dbf"

150 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE
160 DBF INDEX ON CONDITION TAG "BY_COND" OF "tags.mdx"
170 PRINT "MDX tag 1 BY_COND creato"
180 DBF INDEX ON SHAPE TAG "BY_SHAPE" OF "tags.mdx"
190 PRINT "MDX tag 2 BY_SHAPE append su tags.mdx"
200 DBF INDEX ON CONDITION TAG "BY_COND" OF "tags.cdx"
210 PRINT "CDX tag 1 BY_COND creato"
220 DBF INDEX ON SHAPE TAG "BY_SHAPE" OF "tags.cdx"
230 PRINT "CDX tag 2 BY_SHAPE append su tags.cdx"
240 DBF CLOSE

250 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE INDEX "tags.mdx" ALIAS DbfMdx
260 DBF SET ORDER TO "BY_COND"
270 DBF GO TOP
280 nRec = DBF.RECNO()
290 PRINT "MDX BY_COND first recno="; nRec
300 DBF SET ORDER TO "BY_SHAPE"
310 DBF GO TOP
320 PRINT "MDX BY_SHAPE first recno="; DBF.RECNO()
330 DBF CLOSE

340 DBF USE g_work + "/dbase_03.dbf" EXCLUSIVE INDEX "tags.cdx" ALIAS DbfCdx
350 DBF SET ORDER TO "BY_COND"
360 DBF GO TOP
370 PRINT "CDX BY_COND first recno="; DBF.RECNO()
380 DBF SET ORDER TO "BY_SHAPE"
390 DBF GO TOP
400 PRINT "CDX BY_SHAPE first recno="; DBF.RECNO()
410 DBF CLOSE
420 END
