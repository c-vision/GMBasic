' FILE: Databases/DbfReplication.bas
' Compile-check: SETREPLICATION, APPLYREPLICATION, FOLLOWREPLICATION, PACK replication.
' Cluster HTTP + TCP demo: vedi DbfReplicationHttp.bas (porte 8081–8083, TCP 9902).

IMPORT DBF
5 IMPORT DBF
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Row
20   Name AS STRING * 20
30 END TYPE
35 MKDIR g_work

40 KILL g_work + "/dbf_repl.dbf" : ON ERROR RESUME NEXT
50 KILL g_work + "/dbf_repl.dbf.repl" : ON ERROR RESUME NEXT
60 KILL g_work + "/dbf_repl.dbf.repl.ckpt" : ON ERROR RESUME NEXT
70 KILL g_work + "/dbf_repl_secondary.dbf" : ON ERROR RESUME NEXT

80 DBF CREATE g_work + "/dbf_repl.dbf" FROM TYPE Row
90 DBF USE g_work + "/dbf_repl.dbf" ALIAS "Primary" AS Row
100 DIM logPath AS STRING
110 logPath = DBF.SETREPLICATION(Primary, ".")
120 DBF APPEND RECORD {Name: "Alpha"}
130 DBF APPEND RECORD {Name: "Beta"}
140 DBF APPEND RECORD {Name: "Gamma"}
150 DBF GO TOP
160 DBF SKIP
170 DBF DELETE
180 DBF PACK
190 PRINT "Replication log: "; logPath

200 DBF CREATE g_work + "/dbf_repl_secondary.dbf" FROM TYPE Row
210 DBF USE g_work + "/dbf_repl_secondary.dbf" ALIAS "Secondary" AS Row
220 DIM applied AS LONG
230 applied = DBF.APPLYREPLICATION(Secondary, g_work + "/dbf_repl.dbf.repl")
240 PRINT "Applied ops: "; applied

250 DBF SELECT Primary
260 DBF APPEND RECORD {Name: "Delta"}
270 DBF SELECT Secondary
280 DIM followed AS LONG
290 followed = DBF.FOLLOWREPLICATION(g_work + "/dbf_repl.dbf.repl")
300 PRINT "Followed ops: "; followed

' --- REPLICATIONSHIP / REPLICATIONRECV (varianti TCP) ---
' Il percorso di SUCCESSO (ship 1 / recv 1) richiede DUE processi: la GPM e cooperativa e non
' esiste un builtin che apra un listener NON bloccante, quindi REPLICATIONRECV in un task ASYNC
' blocca l'unico worker e il mittente non parte mai (misura: timeout 40 s). Si esercita quindi la
' SUPERFICIE delle due funzioni nella forma DETERMINISTICA e veloce: argomenti non validi
' (porta 0 fuori range, host vuoto) -> 0 = errore, senza tentare connect/accept. E' il
' comportamento condiviso dai 4 backend (vedi MiniTests/dbf/replication_tcp.bas).
310 PRINT "ship porta 0 (fuori range): "; DBF.REPLICATIONSHIP("127.0.0.1", 0)
320 PRINT "ship host vuoto: "; DBF.REPLICATIONSHIP("", 45721)
330 PRINT "recv porta 0 (fuori range): "; DBF.REPLICATIONRECV(0)

340 DBF CLOSE
350 END
