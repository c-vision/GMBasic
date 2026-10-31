' =================================================================================================
'  MVC API DEMO — REST con content negotiation, errori strutturati e HATEOAS (Fase D)
' =================================================================================================
' Endpoint (products in memoria):
'   GET    /api/products            → lista con link HATEOAS
'   GET    /api/products/{id}       → singolo + links, o 404
'   POST   /api/products            → crea (body JSON), validazione → 201 + Location, o 400
'   PUT    /api/products/{id}       → aggiorna, o 404
'   DELETE /api/products/{id}       → 204, o 404
' Content negotiation: Accept "text/plain" → testo piatto; altrimenti application/json.
' =================================================================================================
IMPORT HTTP
IMPORT JSON

' --- Base HttpController (helper risposta + validazione) ---
PUBLIC CLASS HttpController
    PUBLIC FUNCTION Form(req AS HTTPREQUEST) AS JSON
        RETURN req.ParseForm()
    END FUNCTION

    PUBLIC SUB Json(res AS HTTPRESPONSE, data AS ANY)
        res.SendJSON(data)
    END SUB

    PUBLIC SUB NotFound(res AS HTTPRESPONSE)
        res.StatusCode = 404
        res.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Not found" + CHR$(34) + "}")
    END SUB

    PUBLIC SUB NoContent(res AS HTTPRESPONSE)
        ' Nel PCodeVM la risposta si considera inviata solo con Send/OnSent:
        ' un 204 senza corpo NON completa la richiesta (il client riceve reset).
        res.StatusCode = 204
        res.Send("")
    END SUB

    PUBLIC SUB Required(errors AS JSON, field AS STRING, value AS STRING, message AS STRING)
        IF value = "" THEN errors[field] = message
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

    ' Content negotiation: Accept "text/plain" → testo piatto, altrimenti JSON.
    PUBLIC SUB Negotiate(req AS HTTPREQUEST, res AS HTTPRESPONSE, data AS JSON)
        DIM accept AS STRING = req.Headers.Get("Accept")
        IF accept = "" THEN accept = "application/json"
        IF INSTR(accept, "text/plain") > 0 THEN
            DIM nomeP AS STRING = data.nome
            DIM prezzoP AS STRING = LTRIM$(STR(data.prezzo))
            res.SetHeader("Content-Type", "text/plain")
            res.Send("Prodotto: " + nomeP + " | Prezzo: " + prezzoP)
        ELSE
            SELF.Json(res, data)
        END IF
    END SUB
END CLASS

' --- Storage in memoria (id -> product) ---
DIM prods AS JSON
prods = NEW JSON
DIM nextId AS LONG = 1

' --- Controller REST ---
PUBLIC CLASS ProductsController INHERITS HttpController
    ' GET /api/products — lista + links HATEOAS
    PUBLIC SUB GetAll(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM list AS JSON = NEW JSON
        list["items"] = prods
        DIM links AS JSON = NEW JSON
        links["self"] = "/api/products"
        links["create"] = "/api/products"
        list["links"] = links
        SELF.Json(res, list)
    END SUB

    ' GET /api/products/{id} — singolo + links self/update/delete, o 404
    PUBLIC SUB Get(req AS HTTPREQUEST, res AS HTTPRESPONSE, id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF prods[key].nome = "" THEN
            SELF.NotFound(res)
        ELSE
            DIM links AS JSON = NEW JSON
            links["self"] = "/api/products/" + key
            links["update"] = "/api/products/" + key
            links["delete"] = "/api/products/" + key
            prods[key]["links"] = links
            SELF.Negotiate(req, res, prods[key])
        END IF
    END SUB

    ' POST /api/products — body JSON + validazione → 201 + Location, o 400
    PUBLIC SUB Post(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        DIM body AS JSON = req.BodyJson
        DIM errors AS JSON = NEW JSON
        DIM nome AS STRING = body.nome
        DIM prezzo AS LONG = VAL(body.prezzo)

        SELF.Required(errors, "nome", nome, "Il nome è obbligatorio")
        IF prezzo <= 0 THEN errors["prezzo"] = "Il prezzo deve essere > 0"

        IF SELF.HasErrors(errors) THEN
            SELF.SendValidationErrors(res, errors)
        ELSE
            DIM key AS STRING = LTRIM$(STR(nextId))
            DIM p AS JSON = NEW JSON
            p["nome"] = nome
            p["prezzo"] = prezzo
            prods[key] = p
            nextId = nextId + 1
            res.StatusCode = 201
            res.SetHeader("Location", "/api/products/" + key)
            SELF.Json(res, p)
        END IF
    END SUB

    ' PUT /api/products/{id} — aggiorna, o 404
    PUBLIC SUB Put(req AS HTTPREQUEST, res AS HTTPRESPONSE, id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF prods[key].nome = "" THEN
            SELF.NotFound(res)
        ELSE
            DIM body AS JSON = req.BodyJson
            IF body.nome <> "" THEN prods[key]["nome"] = body.nome
            IF VAL(body.prezzo) > 0 THEN prods[key]["prezzo"] = VAL(body.prezzo)
            SELF.Json(res, prods[key])
        END IF
    END SUB

    ' DELETE /api/products/{id} — 204, o 404 (il distruttore è SUB DESTRUCTOR, non Delete)
    PUBLIC SUB Delete(req AS HTTPREQUEST, res AS HTTPRESPONSE, id AS LONG)
        DIM key AS STRING = LTRIM$(STR(id))
        IF prods[key].nome = "" THEN
            SELF.NotFound(res)
        ELSE
            ' Rimozione chiave JSON (prods.Remove(key)) non supportata nel PCodeVM:
            ' soft-delete con oggetto vuoto (il check prods[key].nome = "" dà comunque 404).
            prods[key] = NEW JSON
            SELF.NoContent(res)
        END IF
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8098
HTTP CONFIG cfg

DIM ctl AS ProductsController = NEW ProductsController

HTTP ROUTE "GET", "/api/products", CtrlGetAll()
HTTP ROUTE "GET", "/api/products/{id}", CtrlGet()
HTTP ROUTE "POST", "/api/products", CtrlPost()
HTTP ROUTE "PUT", "/api/products/{id}", CtrlPut()
HTTP ROUTE "DELETE", "/api/products/{id}", CtrlDelete()

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
SUB CtrlPut(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM id AS LONG = VAL(req.Params["id"])
  ctl.Put(req, res, id)
END SUB
SUB CtrlDelete(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  DIM id AS LONG = VAL(req.Params["id"])
  ctl.Delete(req, res, id)
END SUB

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
