' =================================================================================================
'  MVC BASIC DEMO — CRUD (Fase A del Mvc-Implementation-Todo)
' =================================================================================================
' Self-contained: mostra la CONVENZIONE controller (SUB verbo-first + helper di risposta
' sulla base HttpController). Storage = JSON globale (pattern PackageServer); lista di classi
' come campo di classe è un limite del linguaggio (piano Fase Z). La discovery automatica nel
' linker (Fase A3) sostituirà gli HTTP ROUTE espliciti.
' =================================================================================================
IMPORT HTTP
IMPORT JSON

' --- Base HttpController (helper di risposta) ---
PUBLIC CLASS HttpController
    PUBLIC DIM Request AS ANY
    PUBLIC DIM Response AS ANY
    PUBLIC SUB Json(res AS HTTPRESPONSE, data AS ANY)
        res.SendJSON(data)
    END SUB
    PUBLIC SUB NotFound(res AS HTTPRESPONSE)
        res.StatusCode = 404
        res.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Not found" + CHR$(34) + "}")
    END SUB
    PUBLIC SUB Status(res AS HTTPRESPONSE, code AS LONG)
        res.SetStatus(code)
    END SUB
END CLASS

' --- Storage globale (JSON: id -> UserModel) ---
DIM users AS JSON
users = NEW JSON
DIM seed AS JSON
seed = NEW JSON
seed["name"] = "Alice"
seed["email"] = "alice@example.com"
seed["age"] = 30
users["1"] = seed

' --- Controller (SUB verbo-first) ---
PUBLIC CLASS UsersController INHERITS HttpController
    ' GET /api/users/{id} → JSON utente (o 404)
    PUBLIC SUB Get(req AS HTTPREQUEST, res AS HTTPRESPONSE, id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF users[key].name <> "" THEN
            SELF.Json(res, users[key])
        ELSE
            SELF.NotFound(res)
        END IF
    END SUB

    ' GET /api/users → JSON di tutti
    PUBLIC SUB GetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        SELF.Json(res, users)
    END SUB

    ' POST /api/users → 201 con il body
    PUBLIC SUB Post(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        res.StatusCode = 201
        SELF.Json(res, req.Body)
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8095
HTTP CONFIG cfg

DIM ctl AS UsersController = NEW UsersController

' Route esplicite → azioni controller (la discovery automatica li sostituirà, Fase A3)
HTTP ROUTE "GET", "/api/users", CtrlGetAll()
HTTP ROUTE "GET", "/api/users/{id}", CtrlGet()
HTTP ROUTE "POST", "/api/users", CtrlPost()

SUB CtrlGetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.GetAll(req, res)
END SUB
SUB CtrlGet(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM id AS LONG = VAL(req.Params["id"])
  ctl.Get(req, res, id)
END SUB
SUB CtrlPost(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.Post(req, res)
END SUB

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
