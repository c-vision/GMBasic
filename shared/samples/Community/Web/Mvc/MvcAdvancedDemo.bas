' =================================================================================================
'  MVC ADVANCED DEMO — View Engine BSP (layout/section/partial) + Form binding + Validation
' =================================================================================================
' Fase D: integra il view engine (Fase B) con il form binding + validazione (Fase C).
'   - Le pagine .bsp vivono in pages/ (layout _Layout.bsp, partial _Header.bsp, form.bsp) e sono
'     servite renderizzate da HTTP STATIC "/" (TryServeBspPage, come il RegistryBsp).
'   - Il form POSTa via AJAX (Fetch API) a /api/utenti: binding form-urlencoded + validazione
'     esplicita → 400 {errors} o 201 JSON.
' =================================================================================================
IMPORT HTTP
IMPORT JSON

' --- Base HttpController (binding + validazione, pattern MvcFormDemo) ---
PUBLIC CLASS HttpController
    ' NB: NON dichiarare una variabile di nome `form` (collide case-insensitive con la FUNCTION
    ' Form e con la keyword FORM): usare frm/formData.
    PUBLIC FUNCTION Form(req AS HTTPREQUEST) AS JSON
        RETURN req.ParseForm()
    END FUNCTION

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

    PUBLIC SUB Json(res AS HTTPRESPONSE, data AS ANY)
        res.SendJSON(data)
    END SUB
END CLASS

' --- Controller: API di registrazione validata ---
PUBLIC CLASS UtentiController INHERITS HttpController
    ' POST /api/utenti — form-urlencoded → binding + validazione → 201 o 400
    PUBLIC SUB Post(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM frm AS JSON = SELF.Form(req)
        DIM errors AS JSON = NEW JSON
        DIM nome AS STRING = frm.nome
        DIM email AS STRING = frm.email

        SELF.Required(errors, "nome", nome, "Il nome è obbligatorio")
        SELF.Required(errors, "email", email, "L'email è obbligatoria")
        SELF.Email(errors, "email", email, "Email non valida")
        SELF.StringLength(errors, "nome", nome, 2, 50, "Il nome deve avere 2-50 caratteri")

        IF SELF.HasErrors(errors) THEN
            SELF.SendValidationErrors(res, errors)
        ELSE
            DIM u AS JSON = NEW JSON
            u["nome"] = nome
            u["email"] = email
            res.StatusCode = 201
            SELF.Json(res, u)
        END IF
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8097
HTTP CONFIG cfg

' Pagine BSP (pages/) servite renderizzate alla radice
HTTP STATIC "/", "pages"

DIM ctl AS UtentiController = NEW UtentiController
HTTP ROUTE "POST", "/api/utenti", CtrlPost()

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
