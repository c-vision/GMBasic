' FILE: Databases/IsamReplicationTcp.bas
' ISAM - replicazione TCP: REPLICATIONSHIP / REPLICATIONRECV.
'
' PERCHÉ QUESTA FORMA E NON IL PERCORSO DI SUCCESSO
' Il percorso ship 1 / recv 1 richiede DUE processi: la GPM e cooperativa e non esiste un
' builtin che apra un listener NON bloccante (nessun SERVER/LISTEN), quindi
' REPLICATIONRECV dentro un task ASYNC blocca l'unico worker e il mittente non parte mai
' (misura: timeout a 40 s, vedi MiniTests/dbf/replication_tcp.bas).
' Questo sample esercita quindi la SUPERFICIE delle due funzioni nella forma DETERMINISTICA e
' veloce - argomenti non validi (porta 0 fuori range, host vuoto) -> 0 = errore, senza tentare
' connect/accept - che e il comportamento che i 4 backend devono condividere.

IMPORT ISAM
IMPORT IO

DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

TYPE Item
  ID AS STRING * 8
  Name AS STRING * 30
END TYPE

ON ERROR RESUME NEXT
KILL g_work + "/tcp_repl.isdb"
KILL g_work + "/tcp_repl.isdb.repl"
ON ERROR GOTO 0

ISAM OPEN g_work + "/tcp_repl.isdb" OF Item ALIAS X INDEXED BY ID
DIM R AS Item
R.ID = "S001" : R.Name = "ServerRecord" : ISAM APPEND X, R

' Il log di replicazione e cio che SHIP spedisce e RECV riceve.
ISAM.SETREPLICATION(X, g_work + "/tcp_repl.isdb.repl")

PRINT "ship porta 0 (fuori range): "; ISAM.REPLICATIONSHIP("127.0.0.1", 0)
PRINT "ship host vuoto: "; ISAM.REPLICATIONSHIP("", 45721)
PRINT "recv porta 0 (fuori range): "; ISAM.REPLICATIONRECV(0)

PRINT "ISAM TCP replication API verified"
ISAM CLOSE X

ON ERROR RESUME NEXT
KILL g_work + "/tcp_repl.isdb"
KILL g_work + "/tcp_repl.isdb.repl"
ON ERROR GOTO 0
