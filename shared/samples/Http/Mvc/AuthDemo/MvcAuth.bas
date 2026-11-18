' TAG: interactive
' FILE: samples/Http/Mvc/AuthDemo/MvcAuth.bas
' =================================================================================================
'   MVC DEMO — Autenticazione con sessione (login / me / logout)
' =================================================================================================
' Usa il framework MVC (HttpController.inc accanto, IMPORT MVC) e la discovery A4: le azioni
' sono SUB verbo-first SENZA req/res, il linker inietta il contesto (SELF.Request/Response) e
' genera le route automatiche di UN solo controller:
'   POST   /api/auth   → AuthController.Post()    login (verifica credenziali, imposta cookie)
'   GET    /api/auth   → AuthController.Get()     "chi sono" + dati per-utente (protetto)
'   DELETE /api/auth   → AuthController.Delete()  logout (invalida la sessione)
' La sessione server-side è un JSON sessionId → username; il cookie è GM_BASIC_SESSION.
' Get() fa da ROTTA PROTETTA: senza cookie valido risponde 401.
'
' NB: il linker MVC supporta attualmente UN SOLO controller per programma (bug multi-controller,
' vedi Bugs/Classi-Distruttore-Ereditarieta-CrossFile-Bugs.md BUG-10).
'
' Test con curl:
'   curl -c jar -X POST -H "Content-Type: application/json" -d '{"username":"mario","password":"segreta"}' http://localhost:8097/api/auth
'   curl -b jar http://localhost:8097/api/auth
'   curl -b jar -X DELETE http://localhost:8097/api/auth
' =================================================================================================
IMPORT HTTP
IMPORT JSON
IMPORT MVC

' --- Utenti (username → password). In produzione: password hash + persistenza. ---
DIM utenti AS JSON = NEW JSON
utenti["mario"] = "segreta"
utenti["admin"] = "root"

' --- Sessioni server-side (sessionId → username) ---
DIM sessioni AS JSON = NEW JSON

' --- Dati per-utente (protetti: visibili solo dopo il login) ---
DIM note AS JSON = NEW JSON
note["mario"] = "comprare il latte"
note["admin"] = "revisione settimanale"

' --- Controller di autenticazione (UN solo controller per programma, vedi nota sopra) ---
PUBLIC CLASS AuthController INHERITS MVC.HttpController
    ' POST /api/auth — login: body JSON {username, password} → cookie + token
    PUBLIC SUB Post()
        DIM body AS JSON = SELF.Request.BodyJson
        DIM username AS STRING = body.username
        DIM password AS STRING = body.password

        IF utenti[username] = "" OR utenti[username] <> password THEN
            SELF.Response.StatusCode = 401
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Credenziali non valide" + CHR$(34) + "}")
        ELSE
            DIM sid AS STRING = LTRIM$(STR$(INT(RND() * 1000000)))
            sessioni[sid] = username
            SELF.Response.SetCookie("GM_BASIC_SESSION", sid)
            DIM ok AS JSON = NEW JSON
            ok["token"] = sid
            ok["username"] = username
            SELF.Response.SendJSON(ok)
        END IF
    END SUB

    ' GET /api/auth — ROTTA PROTETTA: chi sono + dati per-utente (401 senza cookie)
    PUBLIC SUB Get()
        DIM sid AS STRING = SELF.Request.SessionId
        IF sessioni[sid] = "" THEN
            SELF.Response.StatusCode = 401
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Non autenticato" + CHR$(34) + "}")
        ELSE
            DIM meJson AS JSON = NEW JSON
            meJson["username"] = sessioni[sid]
            meJson["note"] = note[sessioni[sid]]
            SELF.Response.SendJSON(meJson)
        END IF
    END SUB

    ' DELETE /api/auth — logout: invalida la sessione
    PUBLIC SUB Delete()
        DIM sid AS STRING = SELF.Request.SessionId
        sessioni[sid] = ""
        SELF.Response.StatusCode = 204
        SELF.Response.Send("")
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8097
HTTP CONFIG cfg

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
