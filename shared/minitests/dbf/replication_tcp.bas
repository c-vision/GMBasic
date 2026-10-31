' MiniTest DBF+ISAM — REPLICATIONSHIP / REPLICATIONRECV (varianti TCP: host, port / port).
'
' PERCHÉ QUESTE FORME E NON IL PERCORSO DI SUCCESSO
' Questi due simboli esistono SOLO in variante TCP: servono un mittente e un ricevitore.
' Il percorso di successo (ship 1 / recv 1) richiede DUE processi: la GPM è cooperativa e
' qui non esiste un builtin che apra un listener NON bloccante (nessun SERVER/LISTEN), quindi
' `REPLICATIONRECV` in un task ASYNC blocca l'unico worker e il mittente non parte mai
' (verificato: timeout a 40s). Per la stessa ragione i sample ufficiali
' (samples/Databases/IsamReplicationTcp.bas) sono solo compile-check.
'
' Qui si esercita quindi il percorso DETERMINISTICO e veloce: argomenti non validi
' (porta 0 = fuori range, host vuoto) → 0 = errore, senza tentare connect/accept.
' È il comportamento che i 4 backend devono condividere.
' EXPECT: dbf_ship_porta0:  0|dbf_ship_nohost:  0|dbf_recv_porta0:  0|isam_ship_porta0:  0|isam_ship_nohost:  0|isam_recv_porta0:  0
IMPORT DBF
IMPORT ISAM

TYPE TRec
    Name AS STRING * 20
END TYPE

SUB Main()
    DIM pd AS STRING
    DIM pi AS STRING
    pd = "/tmp/gmb_mt_repltcp.dbf"
    pi = "/tmp/gmb_mt_repltcp.isdb"

    ON ERROR RESUME NEXT
    KILL pd
    KILL pd + ".repl"
    KILL pi
    KILL pi + ".repl"
    ON ERROR GOTO 0

    ' ---------- DBF ----------
    DBF CREATE pd FROM TYPE TRec
    DBF USE pd ALIAS "Src" AS TRec
    DBF.SETREPLICATION(Src, "/tmp/gmb_mt_repltcp.dbf.repl")
    DBF APPEND RECORD {Name: "Alpha"}

    ' Porta fuori range (0): nessun connect → 0.
    PRINT "dbf_ship_porta0: "; DBF.REPLICATIONSHIP("127.0.0.1", 0)
    ' Host vuoto: nessun connect → 0.
    PRINT "dbf_ship_nohost: "; DBF.REPLICATIONSHIP("", 45721)
    ' Ascolto su porta fuori range: nessun accept → 0.
    PRINT "dbf_recv_porta0: "; DBF.REPLICATIONRECV(0)
    DBF CLOSE

    ' ---------- ISAM ----------
    ISAM OPEN pi OF TRec ALIAS SrcIsam INDEXED BY Name
    ISAM.SETREPLICATION(SrcIsam, "/tmp/gmb_mt_repltcp.isdb.repl")
    PRINT "isam_ship_porta0: "; ISAM.REPLICATIONSHIP("127.0.0.1", 0)
    PRINT "isam_ship_nohost: "; ISAM.REPLICATIONSHIP("", 45723)
    PRINT "isam_recv_porta0: "; ISAM.REPLICATIONRECV(0)
    ISAM CLOSE

    ON ERROR RESUME NEXT
    KILL pd
    KILL pd + ".repl"
    KILL pi
    KILL pi + ".repl"
    ON ERROR GOTO 0
END SUB
