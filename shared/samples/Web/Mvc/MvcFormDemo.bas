' =================================================================================================
'  MVC BASIC DEMO — Form Binding + Validation (Fase C del Mvc-Implementation-Todo)
' =================================================================================================
' Self-contained (come MvcBasicDemo, Fase A): la base HttpController è inline per comodità di
' sample singolo. Con il framework condiviso (IMPORT MVC + HttpController.inc + INHERITS
' MVC.HttpController, Z3 risolto) vedere la demo A4 in A4InjectionDemo/. Nota: non dichiarare
' una variabile di nome `form` (collide con la FUNCTION Form e la keyword FORM) → usare frm.
'
' Demo:
'   POST /api/utenti   (application/x-www-form-urlencoded: nome, email, eta)
'     → binding per convenzione nome (req.ParseForm() → JSON con le chiavi dei campi)
'     → validazione ESPLICITA (gli attributi [Required]/[Range] sono metadata inerti, gap Z6)
'     → 201 + JSON dell'utente, oppure 400 + { "errors": { campo: messaggio } }
'   GET  /api/utenti   → JSON di tutti gli utenti
' =================================================================================================
IMPORT HTTP
IMPORT JSON

' --- Base HttpController (helper di risposta + binding + validazione) ---
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

    ' --- Binding per convenzione nome (Fase C) ---
    ' req.ParseForm() trasforma il body form-urlencoded in un JSON con le STESSE chiavi dei
    ' nomi dei campi. Per un UDT tipizzato usare JSON.ToObject(Tipo, formJson) nel backend IL.
    ' NB: NON dichiarare una variabile di nome `form` (collide case-insensitive con la FUNCTION
    ' Form e con la keyword FORM): usare es. frm/formData.
    PUBLIC FUNCTION Form(req AS HTTPREQUEST) AS JSON
        RETURN req.ParseForm()
    END FUNCTION

    ' --- Validazione esplicita (gap Z6: gli attributi non validano) ---
    ' Ogni helper accumula in `errors` (JSON passato per riferimento); SendValidationErrors
    ' produce il 400 con { "errors": { campo: messaggio } }.
    PUBLIC SUB Required(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
        IF value = "" THEN errors[field] = message
    END SUB

    PUBLIC SUB StringLength(errors AS JSON, field AS STRING, value AS STRING, minLen AS LONG, maxLen AS LONG, message AS STRING)
        IF LEN(value) < minLen OR LEN(value) > maxLen THEN errors[field] = message
    END SUB

    PUBLIC SUB Email(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
        DIM at AS LONG = INSTR(value, "@")
        IF at <= 0 THEN
            errors[field] = message
        ELSE
            IF INSTR(value, ".", at + 1) <= 0 THEN errors[field] = message
        END IF
    END SUB

    PUBLIC FUNCTION HasErrors(errors AS JSON) AS BIT
        RETURN LEN(JSON.STRINGIFY(errors)) > 2
    END FUNCTION

    PUBLIC SUB SendValidationErrors(res AS HTTPRESPONSE, errors AS JSON)
        res.StatusCode = 400
        DIM payload AS JSON = NEW JSON
        payload["errors"] = errors
        res.SendJSON(payload)
    END SUB
END CLASS

' --- Storage globale (JSON: id -> utente) ---
DIM utenti AS JSON
utenti = NEW JSON

' --- Controller (SUB verbo-first) ---
PUBLIC CLASS UtentiController INHERITS HttpController
    ' Validazione condivisa (form e JSON body usano le stesse regole)
    PRIVATE SUB ValidateInput(errors AS JSON, nome AS STRING, email AS STRING)
        SELF.Required(errors, "nome", nome, "Il nome è obbligatorio")
        SELF.Required(errors, "email", email, "L'email è obbligatoria")
        SELF.Email(errors, "email", email, "Email non valida")
        SELF.StringLength(errors, "nome", nome, 2, 50, "Il nome deve avere 2-50 caratteri")
    END SUB

    ' POST /api/utenti — form-urlencoded → binding + validazione → 201 o 400
    PUBLIC SUB Post(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM frm AS JSON = SELF.Form(req)
        DIM errors AS JSON = NEW JSON
        DIM nome AS STRING = frm.nome
        DIM email AS STRING = frm.email
        DIM eta AS LONG = VAL(frm.eta)

        SELF.ValidateInput(errors, nome, email)

        IF SELF.HasErrors(errors) THEN
            SELF.SendValidationErrors(res, errors)
        ELSE
            DIM u AS JSON = NEW JSON
            u["nome"] = nome
            u["email"] = email
            u["eta"] = eta
            utenti["1"] = u
            res.StatusCode = 201
            SELF.Json(res, u)
        END IF
    END SUB

    ' POST /api/utenti/json — body JSON → binding per convenzione nome → 201 o 400
    PUBLIC SUB PostJson(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM body AS JSON = req.BodyJson
        DIM errors AS JSON = NEW JSON
        DIM nome AS STRING = body.nome
        DIM email AS STRING = body.email
        DIM eta AS LONG = VAL(body.eta)

        SELF.ValidateInput(errors, nome, email)

        IF SELF.HasErrors(errors) THEN
            SELF.SendValidationErrors(res, errors)
        ELSE
            DIM u AS JSON = NEW JSON
            u["nome"] = nome
            u["email"] = email
            u["eta"] = eta
            utenti["2"] = u
            res.StatusCode = 201
            SELF.Json(res, u)
        END IF
    END SUB

    ' GET /api/utenti/{id} — binding dal path (req.Params) → 200 o 404
    PUBLIC SUB GetById(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM id AS STRING = req.Params["id"]
        IF utenti[id].nome <> "" THEN
            SELF.Json(res, utenti[id])
        ELSE
            SELF.NotFound(res)
        END IF
    END SUB

    ' GET /api/utenti — JSON di tutti
    PUBLIC SUB GetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        SELF.Json(res, utenti)
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8096
HTTP CONFIG cfg

DIM ctl AS UtentiController = NEW UtentiController

' Route esplicite → azioni controller (la discovery automatica li sostituirà, Fase A3)
HTTP ROUTE "POST", "/api/utenti", CtrlPost()
HTTP ROUTE "POST", "/api/utenti/json", CtrlPostJson()
HTTP ROUTE "GET", "/api/utenti", CtrlGetAll()
HTTP ROUTE "GET", "/api/utenti/{id}", CtrlGetById()

SUB CtrlPost(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.Post(req, res)
END SUB
SUB CtrlPostJson(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.PostJson(req, res)
END SUB
SUB CtrlGetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.GetAll(req, res)
END SUB
SUB CtrlGetById(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  ctl.GetById(req, res)
END SUB

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
