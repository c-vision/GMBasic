' FILE: Databases/IsamReplication.bas
' Compile-check: SETREPLICATION, APPLYREPLICATION, PACK replication.

IMPORT ISAM
5 IMPORT ISAM
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Item
20   ID AS STRING * 8
30   Name AS STRING * 30
40 END TYPE

50 KILL g_work + "/isam_repl.isdb" : ON ERROR RESUME NEXT
60 KILL "isam_repl.isdb.repl" : ON ERROR RESUME NEXT
70 KILL "isam_repl.isdb.repl.ckpt" : ON ERROR RESUME NEXT
80 KILL g_work + "/isam_repl_secondary.isdb" : ON ERROR RESUME NEXT

90 ISAM OPEN g_work + "/isam_repl.isdb" OF Item ALIAS Primary INDEXED BY ID
100 DIM logPath AS STRING
110 logPath = ISAM.SETREPLICATION(Primary, ".")
120 ISAM BEGIN TRANSACTION
130 DIM R AS Item
140 R.ID = "R001"
150 R.Name = "Alpha"
160 ISAM APPEND Primary, R
170 ISAM COMMIT

180 R.ID = "R002" : R.Name = "Beta" : ISAM APPEND Primary, R
190 R.ID = "R003" : R.Name = "Gamma" : ISAM APPEND Primary, R
200 ISAM GO Primary, TOP
210 ISAM SKIP Primary
220 ISAM DELETE Primary
230 ISAM PACK Primary
240 PRINT "Replication log: "; logPath

250 ISAM OPEN g_work + "/isam_repl_secondary.isdb" OF Item ALIAS Secondary INDEXED BY ID
260 DIM applied AS LONG
270 applied = ISAM.APPLYREPLICATION(Secondary, "isam_repl.isdb.repl")
280 PRINT "Applied ops: "; applied

290 ISAM CLOSE Primary
300 ISAM CLOSE Secondary
310 END
