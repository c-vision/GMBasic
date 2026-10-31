' =================================================================================================
'  MVC A4 DEMO — CRUD con azioni SENZA req/res (iniezione del contesto HTTP via linker)
' =================================================================================================
' A4 (Linker.Mvc.cs): il linker scopre il controller (INHERITS MVC.HttpController), genera la
' bridge per-request che crea un'istanza, inietta Request/Response nei campi ereditati e invoca
' l'azione con i SOLI parametri path. Le route automatiche (/api/users, /api/users/{id}) sono
' generate dal linker: nessun HTTP ROUTE esplicito, nessun wrapper.
' =================================================================================================
IMPORT HTTP
IMPORT JSON
IMPORT MVC

' --- Storage globale (JSON: id -> UserModel) ---
DIM users AS JSON
users = NEW JSON
DIM seed AS JSON
seed = NEW JSON
seed["name"] = "Alice"
seed["email"] = "alice@example.com"
seed["age"] = 30
users["1"] = seed

' --- Controller (azioni SUB verbo-first, SENZA req/res: contesto da SELF.Request/Response) ---
PUBLIC CLASS UsersController INHERITS MVC.HttpController
    ' GET /api/users/{id} → JSON utente (o 404)
    PUBLIC SUB Get(id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF users[key].name <> "" THEN
            SELF.Response.SendJSON(users[key])
        ELSE
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Not found" + CHR$(34) + "}")
        END IF
    END SUB

    ' GET /api/users → JSON di tutti
    PUBLIC SUB GetAll()
        SELF.Response.SendJSON(users)
    END SUB

    ' POST /api/users → 201 con il body
    PUBLIC SUB Post()
        SELF.Response.StatusCode = 201
        SELF.Response.SendJSON(SELF.Request.Body)
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8095
HTTP CONFIG cfg

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
