' FILE: Security/HttpSessionsDemo.bas
' =================================================================================================
'       SECURITY — sessioni server-side su route HTTP (HTTP CONFIG .Sessions)
' =================================================================================================
' HTTP CONFIG .Sessions = TRUE abilita le sessioni sulle route/middleware HTTP:
'   - la prima richiesta riceve il cookie GMBSESSION (HttpOnly; SameSite=Lax);
'   - REQ("Session") e' il dict JSON VIVO della sessione (le modifiche persistono tra richieste
'     con lo stesso cookie — store server-side, sliding 30 min);
'   - default: in-memory; in futuro backend RDD/ISAM file (piano).
' Questo demo verifica lo schema/flag; l'e2e a 2 richieste col cookie vive nei test.
' Eseguibile su IL, PCodeVM e VM/C.

IMPORT HTTP

PRINT "=== Sessioni route HTTP ==="

DIM config AS HTTPCONFIG = NEW HTTPCONFIG
PRINT "Sessions default: "; config.Sessions
config.Sessions = TRUE
IF config.Sessions THEN
    PRINT "SESSIONS OK"
ELSE
    PRINT "SESSIONS FAIL"
END IF

' Applica (server non attivo: no-op, il flag agisce al primo HTTP SERVER)
HTTP CONFIG config

PRINT "HttpSessions demo done"
