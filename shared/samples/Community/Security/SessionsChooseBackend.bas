' FILE: Security/SessionsChooseBackend.bas
' =================================================================================================
'       SECURITY — come scegliere il backend delle sessioni (memory vs file SQLite)
' =================================================================================================
' Le sessioni HTTP si configurano con l'oggetto HTTPCONFIG + HTTP CONFIG:
'   config.Sessions    = TRUE              ' abilita sessioni su route/middleware HTTP
'   config.SessionFile = ""                ' "" = in-memory (default, veloce, perse al riavvio)
'   config.SessionFile = "work/s.db"       ' file = persistenza SQLite (RDD-SQL, sempre presente)
' Nelle pagine BSP: SESSION("user") oppure REQ("Session")("user") (stesso dict).
' NB: REQ("Session") NON è disponibile negli handler di route TIPIZZATI
' (SUB x(req AS HTTPREQUEST, ...) — req non è indicizzabile lì): per lo stato
' di login lato server usare HTTP.SESSIONCREATE (pattern ServerAuthDemo.bas).
' Eseguibile su IL, PCodeVM e VM/C.

IMPORT HTTP

PRINT "=== Scelta backend sessioni ==="

' --- scegli il backend (di solito da una opzione di config dell'app) ---
DIM backend$ = "file"        ' "memory" oppure "file"

DIM config AS HTTPCONFIG = NEW HTTPCONFIG
config.Sessions = TRUE
config.SessionFile = ""
IF backend$ = "file" THEN
    config.SessionFile = "work/sessions.db"
END IF

IF config.Sessions = FALSE THEN
    PRINT "SESSIONS DISABLED"
ELSEIF config.SessionFile = "" THEN
    PRINT "BACKEND MEMORY"
ELSE
    PRINT "BACKEND FILE: "; config.SessionFile
END IF

IF backend$ = "file" AND config.SessionFile = "work/sessions.db" THEN
    PRINT "BACKEND FILE OK"
ELSEIF backend$ = "memory" AND config.SessionFile = "" THEN
    PRINT "BACKEND MEMORY OK"
END IF

' Applica la scelta (il backend viene creato dal server al primo HTTP SERVER/CONFIG reale)
HTTP CONFIG config

PRINT "SessionsChooseBackend demo done"
