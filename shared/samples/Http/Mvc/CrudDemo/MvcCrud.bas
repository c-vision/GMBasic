' TAG: interactive
' FILE: samples/Http/Mvc/CrudDemo/MvcCrud.bas
' =================================================================================================
'   MVC DEMO — CRUD persistente di articoli (storage JSON su file in work/)
' =================================================================================================
' Usa il framework MVC (HttpController.inc accanto, IMPORT MVC) e la discovery A4:
'   GET    /api/articoli       → ArticoliController.GetAll()  lista
'   GET    /api/articoli/{id}  → ArticoliController.Get(id)   singolo o 404
'   POST   /api/articoli       → ArticoliController.Post()    crea (body JSON) → 201
'   PUT    /api/articoli/{id}  → ArticoliController.Put(id)   aggiorna → 200
'   DELETE /api/articoli/{id}  → ArticoliController.Delete(id) rimuove → 204
' I dati sono salvati su work/articoli.json (JSON OPEN ... FOR WRITE/READ, vedi
' samples/Json/JsonFiles.bas) e ricaricati a OGNI richiesta: nel PCodeVM le
' variabili globali del modulo vengono re-inizializzate a ogni richiesta HTTP, quindi lo
' stato persistente DEVE vivere nel file (vedi Bug report, LIMITE globale per-request).
'
' NB: un solo controller per programma (bug multi-controller del linker MVC, BUG-10).
' NB: `Delete` è un metodo normale (il distruttore è `SUB DESTRUCTOR`).
' =================================================================================================
IMPORT HTTP
IMPORT JSON
IMPORT MVC

' --- Storage persistente (in memoria = snapshot della richiesta corrente) ---
DIM articoli AS JSON = NEW JSON      ' id → { nome, prezzo }
DIM DB$ = "work/articoli.json"

' --- Carica i dati dal file (a ogni richiesta: le globali non persistono) ---
SUB Carica()
    IF IO.FileExists(DB$) THEN
        JSON OPEN DB$ FOR "READ" AS #f
        INPUT #f, articoli
        CLOSE #f
    END IF
END SUB

' --- Salva su file (creando work/ se serve) ---
SUB Salva()
    IF IO.DirExists("work") = 0 THEN MKDIR "work"
    JSON OPEN DB$ FOR "WRITE" AS #f
    PRINT #f, articoli
    CLOSE #f
END SUB

' --- Prossimo id: max(id esistenti) + 1 ---
FUNCTION ProssimoId() AS LONG
    DIM m AS LONG = 0
    FOR EACH k IN JSON.KEYS(articoli)
        IF VAL(k) > m THEN m = VAL(k)
    NEXT
    ProssimoId = m + 1
END FUNCTION

' --- Controller ---
PUBLIC CLASS ArticoliController INHERITS MVC.HttpController
    ' GET /api/articoli — lista
    PUBLIC SUB GetAll()
        Carica()
        SELF.Response.SendJSON(articoli)
    END SUB

    ' GET /api/articoli/{id} — singolo o 404
    PUBLIC SUB Get(id AS LONG)
        Carica()
        DIM key AS STRING = LTRIM$(STR(id))
        IF articoli[key].nome = "" THEN
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Articolo non trovato" + CHR$(34) + "}")
        ELSE
            SELF.Response.SendJSON(articoli[key])
        END IF
    END SUB

    ' POST /api/articoli — crea (body JSON {nome, prezzo}) → 201 + persistenza
    PUBLIC SUB Post()
        Carica()
        DIM body AS JSON = SELF.Request.BodyJson
        DIM nome AS STRING = body.nome
        IF nome = "" THEN
            SELF.Response.StatusCode = 400
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Nome obbligatorio" + CHR$(34) + "}")
        ELSE
            DIM key AS STRING = LTRIM$(STR(ProssimoId()))
            DIM a AS JSON = NEW JSON
            a["nome"] = nome
            a["prezzo"] = VAL(body.prezzo)
            articoli[key] = a
            Salva()
            SELF.Response.StatusCode = 201
            SELF.Response.SendJSON(articoli[key])
        END IF
    END SUB

    ' PUT /api/articoli/{id} — aggiorna → 200 + persistenza (o 404)
    PUBLIC SUB Put(id AS LONG)
        Carica()
        DIM key AS STRING = LTRIM$(STR(id))
        IF articoli[key].nome = "" THEN
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Articolo non trovato" + CHR$(34) + "}")
        ELSE
            DIM body AS JSON = SELF.Request.BodyJson
            DIM rec AS JSON
            rec = articoli[key]
            IF body.nome <> "" THEN rec["nome"] = body.nome
            IF VAL(body.prezzo) > 0 THEN rec["prezzo"] = VAL(body.prezzo)
            Salva()
            SELF.Response.SendJSON(articoli[key])
        END IF
    END SUB

    ' DELETE /api/articoli/{id} — rimuove (soft-delete) → 204 + persistenza (o 404)
    PUBLIC SUB Delete(id AS LONG)
        Carica()
        DIM key AS STRING = LTRIM$(STR(id))
        IF articoli[key].nome = "" THEN
            SELF.Response.StatusCode = 404
            SELF.Response.Send("{" + CHR$(34) + "error" + CHR$(34) + ":" + CHR$(34) + "Articolo non trovato" + CHR$(34) + "}")
        ELSE
            articoli[key] = NEW JSON
            Salva()
            SELF.Response.StatusCode = 204
            SELF.Response.Send("")
        END IF
    END SUB
END CLASS

' --- Entry point ---
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Port = 8098
HTTP CONFIG cfg

HTTP SERVER cfg.Port

' Keep server running
DIM wg AS WAITGROUP
GO SUB KeepAlive(wg) IN wg
WAIT wg

SUB KeepAlive(wg AS WAITGROUP)
  SLEEP 3600000
END SUB
