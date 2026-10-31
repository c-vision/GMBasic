' FILE: Security/HttpSessionsFileDemo.bas
' =================================================================================================
'       SECURITY — persistenza sessioni su file (HTTP CONFIG .SessionFile, SQLite/RDD-SQL)
' =================================================================================================
' HTTP CONFIG .SessionFile = "work/sessions.db" attiva il backend durevole SQLite per le
' sessioni HTTP (route REQ("Session") e pagine BSP): le sessioni sopravvivono al riavvio del
' processo. Default senza .SessionFile: in-memory. Tabella sessions(session_id, data,
' last_access), accesso per chiave.
' Eseguibile su IL, PCodeVM e VM/C (config schema; il file viene aperto dal server all'avvio).

IMPORT HTTP

PRINT "=== Persistenza sessioni ==="

DIM config AS HTTPCONFIG = NEW HTTPCONFIG
config.Sessions = TRUE
config.SessionFile = "work/sessions.db"

IF config.Sessions AND config.SessionFile = "work/sessions.db" THEN
    PRINT "SESSIONFILE OK"
ELSE
    PRINT "SESSIONFILE FAIL"
END IF

' Applica (server non attivo: il backend viene configurato al primo HTTP SERVER/CONFIG reale)
HTTP CONFIG config

PRINT "HttpSessionsFile demo done"
