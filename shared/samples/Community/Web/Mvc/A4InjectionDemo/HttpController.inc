' FILE: common/Web/HttpController.inc
' =================================================================================================
'       MVC FRAMEWORK — HttpController (classe base dei controller)
' =================================================================================================
' Modulo con NAMESPACE MVC (in web/): importabile con `IMPORT MVC` dal programma nella cartella
' parent (il linker scansiona le sottocartelle). Un controller è una PUBLIC CLASS che
' INHERITS HttpController e definisce le azioni verbo-first come SUB che ricevono soltanto i
' parametri applicativi/path. Il linker crea un'istanza per richiesta e popola Request/Response:
'
'   PUBLIC CLASS UsersController INHERITS MVC.HttpController
'       PUBLIC SUB Get(id AS LONG)
'           ' ... lookup ...
'           SELF.Response.SendJSON(utente)  ' 200 application/json
'       END SUB
'   END CLASS
'
' Perché SUB che scrivono su res (e non FUNCTION che ritornano IActionResult): l'upcast
' polimorfico derivata->base in ritorno NON è supportato da GMBasic (vedi Mvc-Implementation-Todo
' Fase Z1) — le azioni seguono la semantica comando: scrivono la risposta, non la ritornano.
' =================================================================================================

NAMESPACE MVC

PUBLIC CLASS HttpController
    ' Contesto per-request: popolato dalla bridge GPM PRIMA di invocare l'azione.
    PUBLIC DIM Request AS HTTPREQUEST
    PUBLIC DIM Response AS HTTPRESPONSE

    ' Inietta il contesto HTTP nell'istanza (chiamato dalla bridge sintetica del linker MVC).
    ' NB: i campi ereditati NON sono assegnabili da un'istanza esterna (gap del linguaggio):
    ' l'assegnazione deve avvenire QUI, via SELF, dentro la classe che li dichiara.
    PUBLIC SUB SetContext(req AS HTTPREQUEST, res AS HTTPRESPONSE)
        SELF.Request = req
        SELF.Response = res
    END SUB

    ' --- Helper di risposta (scrivono su res) ---

    ' Invia data come JSON (200 application/json).
    PUBLIC SUB Json(res AS HTTPRESPONSE, data AS ANY)
        res.SendJSON(data)
    END SUB

    ' Invia contenuto arbitrario con il contentType specificato (200).
    PUBLIC SUB Content(res AS HTTPRESPONSE, content AS STRING, contentType AS STRING)
        res.SetHeader("Content-Type", contentType)
        res.Send(content)
    END SUB

    ' Imposta solo lo status code.
    PUBLIC SUB Status(res AS HTTPRESPONSE, code AS LONG)
        res.SetStatus(code)
    END SUB

    ' 404 con corpo JSON d'errore.
    PUBLIC SUB NotFound(res AS HTTPRESPONSE)
        res.SetStatus(404)
        res.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Not found" + CHR$(34) + "}")
    END SUB

    ' 204 senza corpo.
    PUBLIC SUB NoContent(res AS HTTPRESPONSE)
        res.SetStatus(204)
    END SUB

    ' 302 verso url (o status custom).
    PUBLIC SUB Redirect(res AS HTTPRESPONSE, url AS STRING)
        res.Redirect(url)
    END SUB

    ' --- Helper di binding (Fase C) ---
    '
    ' Binding per convenzione nome: il form (application/x-www-form-urlencoded) arriva come JSON
    ' con le STESSE chiavi dei nomi dei campi (req.ParseForm). Poi, per popolare un UDT tipizzato
    ' usa JSON.ToObject(Tipo, form) — materializza un'istanza UDT vera nel backend IL (nel PCodeVM
    ' ritorna un dict generico: vedi Mvc-Implementation-Todo Fase Z7).
    PUBLIC FUNCTION Form(req AS HTTPREQUEST) AS JSON
        RETURN req.ParseForm()
    END FUNCTION

    ' --- Helper di validazione (Fase C) ---
    '
    ' Gli ATTRIBUTI ([Required], [Range]...) sono metadata INERTI nel linguaggio (gap Z6): la
    ' validazione è quindi ESPLICITA — ogni helper accumula gli errori in `errors` (JSON passato
    ' per riferimento), poi SendValidationErrors produce il 400 con { "errors": {...} }.
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
        ' "{}" è l'unico errore valido = nessun errore
        RETURN LEN(JSON.STRINGIFY(errors)) > 2
    END FUNCTION

    ' 400 + { "errors": { campo: [messaggio], ... } }
    PUBLIC SUB SendValidationErrors(res AS HTTPRESPONSE, errors AS JSON)
        res.StatusCode = 400
        DIM payload AS JSON = NEW JSON
        payload["errors"] = errors
        res.SendJSON(payload)
    END SUB
END CLASS
